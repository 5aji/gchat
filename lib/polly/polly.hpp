// polly - C++ wrapper for the Linux epoll system.
// (c) Saji Champlin 2022.

// I'm not going to comment much on the semantics of epoll syscalls.
// But I will say that this is deeply flawed code, and it's not *all*
// my fault.

#include <sys/epoll.h>
#include <memory>
#include <vector>
#include <map>
#include <system_error>
#include <unistd.h>
#include "filedes.hpp"
namespace polly {

// One of the challenges is reconstructing a list of "things" that represent underlying file
// descriptors. In our case, since we use the Netty::Socket abstraction layer, we can't just store
// the fd in the epoll_event structure, since it might be closed when we try to reconstruct it if
// the Socket leaves scope. But then, we *also* can't store a void* to the Socket itself, since if
// it leaves scope, that pointer will be broken.

// So what's the solution? The best way I can think of is to use shared pointers and a map of
// weak_ptr stored in the Epoll class. Then, when it comes to adding a thing (Socket), we take a
// weak pointer to it, add the weak pointer to our map, *and then store the key with the
// epoll_event*. This way, when it comes time to reconstruct, we can middleman the returned
// epoll_event and get the index out, and look up the key to get the weak_ptr, which we can easily
// runtime-check using lock().

// another issue is handling the return of epoll_wait, which returns events + data. we could do this
// by making a struct inside the class. Then, the wait() call will return a vector of these structs,
// which contains a weak_ptr (leave handling lock to caller) as well as a uint32_t for events. This
// way makes it easier to handle the wait call.

// The brilliant part of templates is that this code will work for any file descriptor wrapper that
// has a function `get_fd()`. So if I make an inotify wrapper, I can use the same Epoll library if I
// add a  get_fd method. Likewise for all the other file descriptors that epoll can support (NOT a
// real file on the filesystem, however. That's not supported by epoll or this class).

// A simplified Epoll wrapper. It uses a file descriptor wrapper class that has a get_fd() method.
// It doesn't support modification of the existing fds stored. 
class Epoll : FileDes {
	int epoll_fd = -1;
	// the lut contains a bunch of weak_ptrs to the wrappers.
	std::map<int, std::weak_ptr<FileDes>> lut;
	// A response for wait(). It just has a pointer to the
	// original instance as well as the event that occured.
	typedef struct {
		std::weak_ptr<FileDes> object;
		uint32_t events;
	} event_result;
	public:

	// Create a new Epoll file descriptor. It uses
	// the EPOLL_CLOEXEC flag to prevent issues when forking, making
	// it safer to use in multiprocess systems.
	Epoll() {
		epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	};
	// I'm not sure why you'd want this, but okay.
	Epoll(int fd) : epoll_fd(fd) {};

	~Epoll(){ close(); };

	// returns the epoll file descriptor.
	int get_fd() { return epoll_fd; };

	// Closes the epoll file descriptor. Also clears the internal look up table.
	void close() {
		lut.clear();
		::close(epoll_fd);
	};

	void add_item(std::shared_ptr<FileDes> item, uint32_t events) {
		int item_fd = item->get_fd(); // this is where the next addition will go.
		epoll_event ev;
		ev.events = events;
		ev.data.fd = item_fd;
		int result = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, item_fd, &ev);
		if (result == -1) {
			throw std::system_error(errno, std::generic_category(), "epoll_ctl() failed");
		}
		// it didn't fail, so we add our new guy to the lut.
		lut.emplace(item_fd, item);
	};

	// same as add_item, but for a vector of items. uses the same events list for all
	// items, so if you want more fine-grained control, you'll have to split it up.
	void add_items(std::vector<std::shared_ptr<FileDes>> items, uint32_t events) {
		for (auto item : items) {
			add_item(item, events);
		}
	}

	// Wait for any events to occur. If timeout is -1, will block forever.
	// Will return a list of structs containing a pointer to the original object,
	// as well as the event that occured. See Epoll::event_result for more details.
	std::vector<event_result> wait(int timeout) {
		std::array<struct epoll_event, 100> events; // We can change this value later, but I doubt it will be needed.
		int nevents = epoll_wait(epoll_fd, events.data(), 100, timeout);

		if (nevents == -1) {
			throw std::system_error(errno, std::generic_category(), "epoll_wait() failed");
		}
		// we have events! let's turn them into a list of event_results.
		std::vector<event_result> results{};
		for(int i = 0; i < nevents; i++) {
			auto evnt = events.at(i);
			event_result my_result {lut.at(evnt.data.fd), evnt.events};
			results.push_back(my_result);
		}

		return results; // this can be empty, it doesn't matter.

	}
	// TODO: Modify item? not sure how that'd work.
	
	
	// Removes an item from the epoll interest list. If it's already gone, it won't throw
	// an exeception.
	void delete_item(std::shared_ptr<FileDes> item) {
		// clear it from lut
		lut.erase(item->get_fd());

		int result = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, item->get_fd(), nullptr);

		if (result == -1 && errno != ENOENT) { // if there's no entry, we don't care.
			throw std::system_error(errno, std::generic_category(), "epoll_ctl() failed");
		}
	};
};

}

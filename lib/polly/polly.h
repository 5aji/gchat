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

template<class T>
class Epoll {
	int epoll_fd = -1;
	// the lut contains a bunch of weak_ptrs to 
	std::map<int, std::weak_ptr<T>> lut;
	using event_result = struct {
		std::weak_ptr<T> object;
		uint32_t events;
	};
	public:

	Epoll() {
		epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	};
	// I'm not sure why you'd want this, but okay.
	Epoll(int fd) : epoll_fd(fd) {};
	~Epoll(){ close(); };

	// Closes the epoll file descriptor. Will be called automatically.
	void close() {
		close(epoll_fd);
	};

	void add_item(std::shared_ptr<T> item, uint32_t events) {
		int index = item->get_fd(); // this is where the next addition will go.
		auto ev = std::make_unique<epoll_event>(events, index);
		int result = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, index, ev.get());
		if (result == -1) {
			throw std::system_error(errno, std::generic_category(), "epoll_ctl() failed");
		}
		// it didn't fail, so we add our new guy to the lut.
		lut.emplace(index, item);
	};

	// same as add_item, but for a vector of items. uses the same events list for all
	void add_items(std::vector<std::shared_ptr<T>> items, uint32_t events) {
		for (auto item : items) {
			add_item(item, events);
		}
	}

	// blocks until events occur.
	std::vector<event_result> wait(int timeout) {
		std::array<epoll_event, 100> events; // We can change this value later, but I doubt it will be needed.

		int nevents = epoll_wait(epoll_fd, events.data(), 100, timeout);

		if (nevents == -1) {
			throw std::system_error(errno, std::generic_category(), "epoll_wait() failed");
		}
		// we have events! let's turn them into a list of event_results.
		std::vector<event_result> results;
		for(int i = 0; i < nevents; i++) {
			auto evnt = events.at(i);
			results.emplace(lut.at(evnt.data.fd), evnt.events);
		}


		return results;


	}
	// TODO: Modify item?
	
	// Removes an item from the epoll watch list.
	void delete_item(std::shared_ptr<T> item) {
		// clear it from lut
		lut.erase(item->get_fd());

		int result = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, item->get_fd(), nullptr);

		if (result == -1 && errno != ENOENT) { // if there's no entry, we don't care.
			throw std::system_error(errno, std::generic_category(), "epoll_ctl() failed");
		}
	};
};

}

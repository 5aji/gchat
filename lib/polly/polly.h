// polly - C++ wrapper for the Linux epoll system.
// (c) Saji Champlin 2022.

// I'm not going to comment much on the semantics of epoll syscalls.
// But I will say that this is deeply flawed code, and it's not *all*
// my fault.

#include <sys/epoll.h>
#include <memory>
#include <vector>
#include <system_error>
namespace polly {

// One of the challenges is reconstructing a list of "things" that represent underlying file
// descriptors. In our case, since we use the Netty::Socket abstraction layer, we can't just store
// the fd in the epoll_event structure, since it might be closed when we try to reconstruct it if
// the Socket leaves scope. But then, we *also* can't store a void* to the Socket itself, since if
// it leaves scope, that pointer will be broken.

// So what's the solution? The best way I can think of is to use shared pointers and a map of
// weak_ptr stored in the Epoll class. Then, when it comes to adding a thing (Socket), we take a
// weak pointer to it, add the weak pointer to our vector, *and then store the index with the
// epoll_event*. This way, when it comes time to reconstruct, we can middleman the returned
// epoll_event and get the index out, and look up the index to get the weak_ptr, which we can easily
// runtime-check using lock().

// another issue is handling the return of epoll_wait, which returns events + data. we could do this
// by making a struct inside the class. Then, the wait() call will return a vector of these structs,
// which contains a weak_ptr (leave handling lock to caller) as well as a uint32_t for events.


// Create a helper function to make epoll events.
using epoll_event_p = std::unique_ptr<epoll_event>;

// should not be needed, since the Epoll class will automatically construct
// an epoll event. 
epoll_event_p create_event(uint32_t events, epoll_data_t data) {
	return std::make_unique<epoll_event>(events,data);
}


template<class T>
class Epoll {

	int epoll_fd = -1;
	std::vector<std::weak_ptr<T>> lut;
	using event_result = struct {
		std::weak_ptr<T> object;
		uint32_t events;
	};
	public:
	Epoll();
	Epoll(int fd) : epoll_fd(fd) {};

	~Epoll();

	// Closes the epoll file descriptor. Will be called automatically.
	void close();

	void add_item(std::shared_ptr<T> item, uint32_t events) {
		int index = lut.size(); // this is where the next addition will go.
		auto ev = std::make_unique<epoll_event>(events, index);
		int result = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, item->get_fd(), ev);
		if (result == -1) {
			throw std::system_error(errno, std::generic_category(), "epoll_ctl() failed");
		}
		// it didn't fail, so we add our new guy to the lut.
		lut.emplace_back(item);
	};

	// trims the fat of the LUT (removes dead ptrs), and clears the epoll
	void rebuild();

	void add_items(std::vector<T> items, epoll_event_p ev);

	// blocks until events occur.
	std::vector<event_result> wait() {

	}
};

}

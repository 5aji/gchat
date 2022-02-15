// polly - C++ wrapper for epoll() and friends.
// (c) Saji Champlin 2022.

// I'm not going to comment much on the semantics of epoll syscalls.
// But I will say that this is deeply flawed code, and it's not all
// my fault.

#include <sys/epoll.h>
#include <memory>
#include <vector>
namespace polly {

// One of the challenges is reconstructing a list of
//

// Create a helper function to make epoll events.
using epoll_event_p = std::unique_ptr<epoll_event>;

epoll_event_p create_event(uint32_t events, epoll_data_t data) {
	return std::make_unique<epoll_event>(events,data);
}


template<class T>
class Epoll {

	int epoll_fd = -1;
	public:
	Epoll();
	Epoll(int fd) : epoll_fd(fd) {};

	~Epoll();

	// Closes the epoll file descriptor. Will be called automatically.
	void close();

	void add_item(T item, epoll_event_p ev) {	
		epoll_ctl(epoll_fd, EPOLL_CTL_ADD, item.get_fd(), ev);
	};

	void add_items(std::vector<T> items, epoll_event_p ev);

	// blocks until events occur. Returns a list
	std::vector<T> wait();
};

}

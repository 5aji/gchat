// polly - C++ wrapper for epoll() and friends.
// (c) Saji Champlin 2022.
#include <sys/epoll.h>

namespace polly {

enum EPOLL_OP {
	ADD = 1,
	DEL,
	MOD,
};

class Epoll {

	int epoll_fd = -1;
	public:
	Epoll();
	Epoll(int fd) : epoll_fd(fd) {};

	~Epoll();

	// Closes the epoll file descriptor. Will be called automatically.
	void close();




};

}

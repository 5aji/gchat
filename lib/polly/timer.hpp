// timer.hpp - example Epoll compatible wrapper for timerfd
// (c) Saji Champlin 2022

#include <sys/timerfd.h>
#include <memory>
#include "filedes.hpp"
namespace polly {


// The timer class uses the linux timerfd system to expose an epoll-compatible
// timer API for multiplexed I/O applications. It can be used for either
class Timer : FileDes {
	int timer_fd = -1;

	public:
	Timer() : Timer(CLOCK_REALTIME, 0) {};
	Timer(int clockid, int flags);

	~Timer();

	// return the underlying fd, fulfilling the Epoll requirement.
	int get_fd();

	// Set nonblocking to `mode`
	void setnonblocking(bool mode);

	// Disable the timer, but keep the descriptor open. Useful
	// if you want to keep a timer allocated but don't want it to activate.
	void disarm();

	// Set the timer to activate in seconds + nanos time. If oneshot is false,
	// it will repeat at the same interval. If it's true, it will only activate once.
	void settime(long int seconds, long int nanos, bool oneshot);

	// Set the timer spec.
	void settime(struct itimerspec& spec);
	// returns the current specifications of the timer, including time remaining before
	// it activates.
	std::unique_ptr<struct itimerspec> gettime();	
	
	// returns the number of expirations (triggers) that have occurred
	// since the last read() or settime();
	uint64_t read();
};

}

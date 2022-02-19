
#include "timer.hpp"
#include <unistd.h>
#include <system_error>
#include <fcntl.h>
namespace polly {

Timer::Timer(int clockid, int flags) {
	fd = timerfd_create(clockid, flags);
	if (fd == -1)
		throw std::system_error(errno, std::generic_category(), "timerfd_create() failed");

}

void Timer::disarm() {
	struct itimerspec disarmed = {};
	disarmed.it_value.tv_nsec = 0;
	disarmed.it_value.tv_sec = 0;
	int result = timerfd_settime(fd, 0, &disarmed, nullptr);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "timerfd_settime() failed");
	}
}

void Timer::settime(long int seconds, long int nanos, bool oneshot) {

	struct itimerspec time = {};
	
	time.it_interval.tv_sec = oneshot ? 0 : seconds;
	time.it_interval.tv_nsec = oneshot ? 0 : nanos;

	time.it_value.tv_sec = seconds;
	time.it_value.tv_nsec = nanos;

	int result = timerfd_settime(fd, 0, &time, nullptr);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "timerfd_settime() failed");
	}
}

void Timer::settime(struct itimerspec& spec) {
	int result = timerfd_settime(fd, 0, &spec, nullptr);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "timerfd_settime() failed");
	}
}

uint64_t Timer::read() {
	uint64_t exp = 0;
	int result = ::read(fd, &exp, sizeof(exp));

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "read() failed");
	}
	return exp;
}

std::unique_ptr<struct itimerspec> Timer::gettime() {
	auto time = std::make_unique<struct itimerspec>();

	int result = ::timerfd_gettime(fd, time.get());
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "timerfd_gettime() failed");
	}
	return time;
}

}

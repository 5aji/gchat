// timer.hpp - example Epoll compatible wrapper for timerfd
// (c) Saji Champlin 2022

#pragma once
#include "filedes.hpp"
#include <memory>
#include <sys/timerfd.h>
namespace polly {

// The timer class uses the linux timerfd system to expose an epoll-compatible
// timer API for multiplexed I/O applications. It can be used for either
class Timer : public FileDes<Timer> {

  public:
    Timer(int clockid = CLOCK_REALTIME, int flags = 0);
    // keep our copy ctor and move ctor (assignment is inherited)
    Timer(const Timer &other) : FileDes(other){};

    Timer(Timer &&other) : FileDes(other){};

    // Disable the timer, but keep the descriptor open. Useful
    // if you want to keep a timer allocated but don't want it to activate.
    void disarm();

    // Set the timer to activate in seconds + nanos time. If oneshot is false,
    // it will repeat at the same interval. If it's true, it will only activate
    // once.
    void settime(long int seconds, long int nanos, bool oneshot);

    // Set the timer spec.
    void settime(struct itimerspec &spec);
    // returns the current specifications of the timer, including time remaining
    // before it activates.
    std::unique_ptr<struct itimerspec> gettime();

    // returns the number of expirations (triggers) that have occurred
    // since the last read() or settime();
    uint64_t read();
};

} // namespace polly

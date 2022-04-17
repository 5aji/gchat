// FileDes.hpp

#pragma once
#include <fcntl.h>
#include <functional>
#include <system_error>
#include <unistd.h>
namespace polly {

// Challenge: we want Epoll to have a bunch of pointers to a base class, and
// then call a function on that base class which does operations (for example,
// clearing a timer, or recv from a socket). In other words, we want it to have
// a handle(int events) method, which can parse the events bitmask and perform
// operations on itself like read() or accept(). We could just use a simple
// abstract base class and add specific code to each derived class to handle
// it, but this adds a new challenge: if we want to change behavior for a
// specific instance of a timer, for example, we would have to extend timer and
// override the handle() function. This is bloated and frustrating to work with
// on an application level. What if we had some sort of callback function that
// could be provided by user code? it would work well for lambdas or full
// functions with a void(Socket&, int) signature. But the problem is, how can we
// access derived-specific functions if it's part of the base class? this is
// where the CTRP (Curiously recurring template pattern) enters the scene.
// Essentially, we have a template base class, and then use the template
// parameter to define functions that work derived classes. Then, when we
// inherit from base, we pass our new class as a template parameter. Like this:
//
// `class Derived : public Base<Derived>`
//
// However, since Base is a template class, we can't just refer to it
// homogenously, so we add an AbstractBase which contains virtual functions that
// we want Epoll to be able to call (handle, get_fd) and then inherit from that
// in our template base class. It's a way to reduce the amount of copy-pasted
// code that needs to be written for every file descriptor wrapper. You can read
// more here: https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern

static int dup(int fd) {
    int res = ::dup(fd);
    if (res < 0)
        throw std::system_error(errno, std::generic_category(), "dup() failed");
    return res;
}

// The underlying base class which we use to store variants
class AbstractFileDes {
  protected:
    int fd = -1;

  public:
    virtual void handle(int events) = 0;
    virtual int get_fd() { return fd; }
    virtual void close() {
        if (fd != -1)
            ::close(fd);
    }
    virtual void setnonblocking(bool mode) {
        if (fd == -1)
            return;
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "fcntl() failed:");
        }
        if (mode) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        int result = fcntl(fd, F_SETFL, flags);
        if (result == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "fcntl() failed:");
        }
    }
    virtual ~AbstractFileDes() { close(); }
};

// spicy sauce. adds type-specific information so we don't have to
// copy the same code everywhere. Everything should inherit this. Functions
// that don't need T should be in AbstractFileDes. Note that constructors
// are exempt from this due to needing direct inheritance for delegation.
template <typename T> class FileDes : public AbstractFileDes {
  public:
    using handler_t = std::function<void(T &, int)>;

  protected:
    FileDes() = default;
    FileDes(const FileDes &other) { // copy constructor
        fd = dup(other.fd);
    };
    FileDes(FileDes &&other) { // move ctor
        fd = other.fd;
        other.fd = -1;
    };
    handler_t handler;

  public:
    void set_handler(handler_t new_handler) { handler = new_handler; }
    void handle(int events) final { handler(static_cast<T &>(*this), events); }
    // copy operator
    T &operator=(const T &other) {
        if (fd != other.fd) {
            close();
            fd = dup(other.fd);
        }
        return *this;
    }

    // move operator
    T &operator=(T &&other) {
        close();
        fd = other.fd;
        other.fd = -1;
        return *this;
    }
};
} // namespace polly

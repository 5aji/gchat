// polly.hpp - C++ wrapper for the Linux epoll system.
// (c) Saji Champlin 2022.
#pragma once
// I'm not going to comment much on the semantics of epoll syscalls.
// But I will say that this is deeply flawed code, and it's not *all*
// my fault
#include "filedes.hpp"
#include <any>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <vector>
namespace polly {

// One of the challenges is reconstructing a list of "things" that represent
// underlying file descriptors. In our case, since we use the Netty::Socket
// abstraction layer, we can't just store the fd in the epoll_event structure,
// since it might be closed when we try to reconstruct it if the Socket leaves
// scope. But then, we *also* can't store a void* to the Socket itself, since
// if it leaves scope, that pointer will be broken and isn't safe memory usage.

// So what's the solution? The best way I can think of is to use shared
// pointers and a map of weak_ptr stored in the Epoll class. Then, when it
// comes to adding a thing (Socket), we take a weak pointer to it, add the weak
// pointer to our map, *and then store the key with the epoll_event*. This way,
// when it comes time to reconstruct, we can middleman the returned epoll_event
// and get the index out, and look up the key to get the weak_ptr, which we can
// easily runtime-check using lock(). It could also be a shared_ptr, which
// would cause it to never leave scope.

// One challenge is making the wrapper work with multiple different file
// descriptors, and handling them in unique ways. Initially this code used
// templates, and as long as the template class had a get_fd function the code
// would work. However,

// another issue is handling the return of epoll_wait, which returns the event
// that happened plus the associated data that it happened to. Since we have
// the map of fd -> weak_ptr<FileDes>, we can use that to get the underlying
// object. However, we *don't* know what it is. Is it a Socket? Timer?
// something else entirely? who knows. To get around this, and prevent slow,
// bad RTTI, instead of trying to type check, we can just call a virtual
// function .handle() on the object behind the weak_ptr. This way it will be
// smart and use the one it needs to. The downside to this approach is that the
// code must be structured in an odd way, and we must extend one of the file
// descriptor classes to add custom behavior.

// A simplified Epoll wrapper. It uses a file descriptor wrapper class that has
// a get_fd() method. It doesn't support modification of the existing fds
// stored.
class Epoll : public FileDes<Epoll> {
    int fd = -1;
    // the lut contains a bunch of weak_ptrs to the wrappers.
    std::map<int, std::shared_ptr<AbstractFileDes>> lut;

    // A response from wait(). It just has a pointer to the
    // original instance as well as the events that have happened
    // to it.
  public:
    // Create a new Epoll file descriptor. It uses
    // the EPOLL_CLOEXEC flag to prevent issues when forking, making
    // it safer to use in multiprocess systems.
    Epoll(int flags = EPOLL_CLOEXEC) { fd = epoll_create1(flags); };

    Epoll(const Epoll &other) : FileDes(other) { lut = other.lut; }
    Epoll(Epoll &&other) : FileDes(other) {
        lut = other.lut;
        other.lut.clear();
    }

    // Closes the epoll file descriptor. Also clears the internal look up table.
    void close() {
        lut.clear();
        ::close(fd);
    };

    void add_item(std::shared_ptr<AbstractFileDes> item, uint32_t events) {
        int item_fd =
            item->get_fd(); // this is where the next addition will go.
        epoll_event ev;
        ev.events = events;
        ev.data.fd = item_fd;
        int result = epoll_ctl(fd, EPOLL_CTL_ADD, item_fd, &ev);
        if (result == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "epoll_ctl() failed");
        }
        // it didn't fail, so we add our new guy to the lut.
        lut.emplace(item_fd, item);
    };

    // same as add_item, but for a vector of items. uses the same events list
    // for all items, so if you want more fine-grained control, you'll have to
    // split it up.
    void add_items(std::vector<std::shared_ptr<AbstractFileDes>> items,
                   uint32_t events) {
        for (auto item : items) {
            add_item(item, events);
        }
    }

    // Wait for any events to occur. If timeout is -1, will block forever.
    // Will return a list of structs containing a pointer to the original
    // object, as well as the event that occured. See Epoll::event_result for
    // more details.
    void wait(int timeout) {
        std::array<struct epoll_event, 100>
            events; // We can change this value later, but I doubt it will be
                    // needed.
        int nevents = epoll_wait(fd, events.data(), 100, timeout);

        if (nevents == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "epoll_wait() failed");
        }
        // we have events! let's turn them into a list of event_results.
        for (int i = 0; i < nevents; i++) {
            auto evnt = events.at(i);
            lut.at(evnt.data.fd)->handle(evnt.events);
        }
    }
    // TODO: Modify item? not sure how that'd work.

    // Removes an item from the epoll interest list. If it's already gone, it
    // won't throw an exeception.
    void delete_item(AbstractFileDes &item) {
        // NOTE: we have to remove from epoll before clearing from lut, since
        // the lut removal might cause the fd wrapper to destruct, making the
        // fd invalid.
        int result = epoll_ctl(fd, EPOLL_CTL_DEL, item.get_fd(), nullptr);

        if (result == -1 &&
            errno != ENOENT) { // if there's no entry, we don't care.
            throw std::system_error(errno, std::generic_category(),
                                    "epoll_ctl() failed");
        }
        lut.erase(item.get_fd());
    };
};

} // namespace polly

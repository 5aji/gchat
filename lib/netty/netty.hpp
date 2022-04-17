// netty - A simple socket library for C++
// (c) Saji Champlin 2022
#pragma once
#include "polly/filedes.hpp"
#include <memory>
#include <netdb.h>
#include <sys/socket.h>
#include <vector>
namespace Netty {

// ======== Misc. Syscall Wrappers ========

// Convert addrinfo to string for printing.
std::string inet_ntop(struct addrinfo &addr);
/* std::string inet_ntop(int family, struct sockaddr& addr, socklen_t addrlen);
 */

// ======== addrinfo stuff =========
// TODO: Make addrinfo wrapper? override ++ to go to info->ai_next if available
// (like iterator). Could also add copy and move stuff.
// this is a helper class for managing the lifecycle of a addrinfo struct.
// it lets us use std::unique_ptr with it.
struct addrinfo_deleter {
    void operator()(addrinfo *info) const { freeaddrinfo(info); }
};

// Wrapper class to help with addrinfo memory safety. When using this,
// C++ will ensure that the struct is freed properly after it leaves scope,
// preventing memory leaks.
using addrinfo_p = std::unique_ptr<struct addrinfo, addrinfo_deleter>;

// Helper function that calls getaddrinfo and handles creating the result
// pointer as well as error checking.
addrinfo_p getaddrinfo(std::string node, std::string service,
                       struct addrinfo *hints);

// The best function. You just give it a name, a port/port identifier, and
// whether it is passive or not. it handles everything else. Memory Safe!
// Results can be passed directly to Socket for quick initialization.
addrinfo_p getaddrinfo(std::string node, std::string service, bool passive);

// Create a new addrinfo struct with sane defaults (TCP and either ipv6 or ipv4)
addrinfo_p make_addrinfo(bool passive) noexcept;

// A Socket is the base class for network communication. It wraps the linux
// socket api in a safe manner. It helps set up connections and performs
// error checking for all operations using C++ exceptions.
class Socket : public polly::FileDes<Socket> {
    addrinfo_p info = nullptr;

    // private constructor for accept() *only*
    Socket(int new_fd, addrinfo_p addrinfo) : info(move(addrinfo)) {
        fd = new_fd;
    }
    // actually calls the socket() call, creating the file descriptor.
    void open();

  public:
    Socket() : info(make_addrinfo(false)) { open(); };
    Socket(addrinfo_p addrinfo) : info(move(addrinfo)) { open(); };
    Socket(const Socket &other) : FileDes(other), info(other.info.get()){};
    Socket(Socket &&other) : FileDes(other), info(std::move(other.info)){};

    // Allows for setting a socket after the fact (for whatever reason)
    void setaddrinfo(addrinfo_p new_info);

    // Incomplete wrapper to set socket options. Doesn't support socket options
    // that don't take an int.
    void setsockopt(int optname, int value);

    // initiate a connection with the stored addrinfo
    void connect();

    // Send a sequence of NetworkPackets to the connection.
    int send(std::vector<std::uint8_t> const &buf);

    // receive Packets.
    std::vector<std::uint8_t> recv(int size);

    // recv all packets, only works with nonblocking.
    std::vector<std::uint8_t> recv_all();

    // bind to address
    void bind();

    // Start listening on this socket.
    void listen();

    // take a new connection request, accept it, and return a new socket for
    // this connection.
    Socket accept();
};

} // namespace Netty

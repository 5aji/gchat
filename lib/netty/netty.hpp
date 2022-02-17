// netty - A simple socket library for C++
// (c) Saji Champlin 2022
#pragma once
#include <sys/socket.h>
#include <netdb.h>
#include <vector>
#include <memory>
namespace Netty {


// ======== Misc. Syscall Wrappers ========

// Convert addrinfo to string for printing.
std::string inet_ntop(struct addrinfo& addr); 
/* std::string inet_ntop(int family, struct sockaddr& addr, socklen_t addrlen); */


// ======== addrinfo stuff =========

// this is a helper class for managing the lifecycle of a addrinfo struct.
// it lets us use std::unique_ptr with it.
struct addrinfo_deleter {
	void operator()(addrinfo* info) const {
		freeaddrinfo(info);
	}
};


// Wrapper class to help with addrinfo memory safety. When using this,
// C++ will ensure that the struct is freed properly after it leaves scope.
using addrinfo_p =  std::unique_ptr<struct addrinfo, addrinfo_deleter>;

// Helper function that calls getaddrinfo and handles creating the result pointer
// as well as error checking.
addrinfo_p getaddrinfo(std::string node, std::string service, struct addrinfo* hints);

// The best function. You just give it a name, a port/port identifier, and
// whether it is passive or not. it handles everything else. Memory Safe!
// Results can be passed directly to Socket for quick initialization.
addrinfo_p getaddrinfo(std::string node, std::string service, bool passive);

// Create a new addrinfo struct with sane defaults (TCP and either ipv6 or ipv4)
addrinfo_p make_addrinfo(bool passive) noexcept;


// A Socket is the base class for network communication. It wraps the linux
// socket api in a safe manner. It helps set up connections and performs
// error checking for all operations using C++ exceptions.
class Socket {
	int sock_fd = -1;
	addrinfo_p info = nullptr;

	// internal function
	bool islistening();

	public:
	Socket(): sock_fd(-1), info(nullptr) { }; // null addrinfo 
	Socket(addrinfo_p addrinfo) : info(move(addrinfo)) { };
	Socket(int fd) : sock_fd(fd) { }; 
	Socket(int fd, addrinfo_p addrinfo) : sock_fd(fd), info(move(addrinfo)) {};

	~Socket();

	// Allows for setting a socket after the fact (for whatever reason)
	void setaddrinfo(addrinfo_p new_info);

	// Incomplete wrapper to set socket options. Doesn't support socket options
	// that don't take an int.
	void setsockopt(int optname, int value);

	// set nonblocking to `mode`.
	void setnonblocking(bool mode);

	// returns the FD of the socket. Not sure why you'd want this.
	int get_socket() { return sock_fd; };

	// actually calls the socket() call, creating the file descriptor.
	void open();


	// initiate a connection with the stored addrinfo
	void connect();

	// Send a sequence of NetworkPackets to the connection.
	int send(std::vector<uint8_t>& buf);

	// receive Packets.
	int recv(std::vector<uint8_t>& buf);

	// bind to address
	void bind();

	// Start listening on this socket.
	void listen();

	// take a new connection request, accept it, and return a new socket for this connection.
	Socket accept();

	// we can manually close the socket for whatever reason. automatically called by the destructor.
	void close();

};

// A ServerSocket is a socket with additional tooling to help with a server type architecture.
// ServerSockets can bind and listen for connections more easily.
class ServerSocket: Socket {

};


// A ClientSocket is a socket designed to initiate a connection with a server.
// It includes tools to help connecting to a server.
class ClientSocket: Socket {

};

}

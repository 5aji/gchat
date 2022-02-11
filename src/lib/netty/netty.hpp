// netty - A simple socket library for C++
// (c) Saji Champlin 2022
#pragma once
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>

namespace Netty {

// A NetworkPacket is any data structure that can be sent over the network.
// it has a serialize and deserialize function that can be used to add to 
// a transmission.
class NetworkPacket {
	public:
	virtual void serialize(std::vector<uint8_t>& buf);
	virtual void deserialize(std::vector<uint8_t>& buf);
	virtual ~NetworkPacket();
};

// this is a helper class for managing the lifecycle of a addrinfo struct.
// it lets us use std::unique_ptr with it.
struct addrinfo_deleter {
	void operator()(addrinfo* info) const {
		freeaddrinfo(info);
	}
};

typedef std::unique_ptr<struct addrinfo, addrinfo_deleter> addrinfo_p;

// A Socket is the base class for network communication. It wraps the linux
// socket api in a safe manner. It helps set up connections and performs
// error checking for all operations using C++ exceptions instead of errno.
class Socket {
	int sock_fd;
	addrinfo_p info;
	public:
	Socket(); // null addrinfo 
	Socket(std::string hostname, std::string port); // construct our own addr info
	Socket(addrinfo_p addrinfo); // take prebuilt addrinfo (might not be used);
	Socket(int fd); // takes an existing socket (for things like accept)

	// we can't use the default destructor since we have a fd.
	~Socket();

	// if the socket is created by hostname or addrinfo, we must call socket() at some point.
	void open();

	void connect();
	// Send a sequence of NetworkPackets to the connection.
	void send(std::vector<uint8_t>& buf);

	// receive Packets.
	void recv(std::vector<uint8_t>& buf);

	// bind to address
	void bind();

	// Start listening on this socket.
	void listen();

	// take a new connection request, accept it, and return a new socket for this connection.
	std::unique_ptr<Socket> accept();

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

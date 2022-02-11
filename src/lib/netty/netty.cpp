

#include "netty.hpp"
#include <system_error>
#include <unistd.h>
namespace Netty {

Socket::Socket() {
	sock_fd = -1;
	
}

Socket::Socket(std::string hostname, std::string port) {
	
	// do some gettaddrinfo shenanigans.
	getaddrinfo(hostname.c_str(), port.c_str(), NULL, NULL);

}


// Creates a socket class given a addrinfo struct.
// addrinfo can be generated with getaddrinfo()

Socket::Socket(addrinfo_p a) {
	sock_fd = -1;
	info = move(a);
};

// Create a Socket with just the file descriptor.
Socket::Socket(int fd) {
	sock_fd = fd;
	// we assume the fd is an open socket and won't ask more questions.
}

void Socket::open() {
	if (sock_fd != -1) {
		return;
	}
	// TODO: Check if info is valid.	
	sock_fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);

	if (sock_fd == -1) {
		throw std::system_error(errno, std::generic_category(), "socket() failed:");
	}
}


void Socket::close() {
	int res = ::close(sock_fd);
	::freeaddrinfo(info.get());
}

void Socket::bind() {
	int result = ::bind(sock_fd, info->ai_addr, info->ai_addrlen);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "bind() failed:");
	}
}


void Socket::listen() {
	int result = ::listen(sock_fd, 20); // TODO: set a different backlog?

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "listen() failed:");
	}

}

void Socket::connect() {
	int result = ::connect(sock_fd, info->ai_addr, info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "connect() failed:");
	}
}



}


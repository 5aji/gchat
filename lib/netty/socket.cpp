

#include "netty.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <system_error>
namespace Netty {


void Socket::open() {
	if (sock_fd != -1) {
		return;
	}
	if (!info) {
		return; // addrinfo is missing.
	}
	sock_fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);

	if (sock_fd == -1) {
		throw std::system_error(errno, std::generic_category(), "socket() failed");
	}
}

void Socket::close() {
	int res = ::close(sock_fd);
	// we don't need to call freeaddrinfo since it's called automatically.
}

void Socket::setaddrinfo(addrinfo_p new_info) {
	info = move(new_info);
}

Socket::~Socket() {
	close();
}

void Socket::bind() {
	int result = ::bind(sock_fd, info->ai_addr, info->ai_addrlen);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "bind() failed");
	}
}


void Socket::listen() {
	int result = ::listen(sock_fd, 20); // TODO: set a different backlog?

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "listen() failed");
	}

}

// accept a connection.
Socket Socket::accept() {
	addrinfo_p new_info = make_addrinfo(false);
	int result = ::accept(sock_fd, new_info->ai_addr, &new_info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "accept() failed");
	}	
	return Socket(result, move(new_info));
}

void Socket::connect() {
	// TODO: traverse the linked list for more results if connection fails.
	int result = ::connect(sock_fd, info->ai_addr, info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "connect() failed");
	}
}

int Socket::recv(std::vector<uint8_t>& buf) {
	int bytes = ::recv(sock_fd, buf.data(), buf.size(), 0);
	if (bytes == -1) {
		throw std::system_error(errno, std::generic_category(), "recv() failed");
	}
	return bytes;
}

int Socket::send(std::vector<uint8_t>& buf) {
	// this function is a bit harder, since we want to send all the data
	// sometimes the send() call only sends part of it.
	
	int len = buf.size();
	int sent = 0;
	while (len > 0) {
		int n = ::send(sock_fd, buf.data() + sent, len, 0);
		if (n == -1) {
			throw std::system_error(errno, std::generic_category(), "send() failed:");
		}
		sent += n;
		len -= n;
	}

	return sent;
}

void Socket::setsockopt(int optname, int value) {
	int result = ::setsockopt(sock_fd, SOL_SOCKET, optname, &value, sizeof(value));
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "setsockopt() failed:");
	}
}

void Socket::setnonblocking(bool mode) {
	int flags = fcntl(sock_fd, F_GETFL);
	if (flags == -1) {
		throw std::system_error(errno, std::generic_category(), "fcntl() failed:");
	}
	if (mode) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}
	int result = fcntl(sock_fd, F_SETFL, flags);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "fcntl() failed:");
	}

}

}


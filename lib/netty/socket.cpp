

#include "netty.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <system_error>
namespace Netty {



void Socket::open() {
	if (fd != -1) {
		return;
	}
	if (!info) {
		return; // addrinfo is missing.
	}
	fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);

	if (fd == -1) {
		throw std::system_error(errno, std::generic_category(), "socket() failed");
	}
}

void Socket::setaddrinfo(addrinfo_p new_info) {
	info = move(new_info);
}

void Socket::bind() {
	int result = ::bind(fd, info->ai_addr, info->ai_addrlen);

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "bind() failed");
	}
}


void Socket::listen() {
	int result = ::listen(fd, 20); // TODO: set a different backlog?

	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "listen() failed");
	}

}

Socket Socket::accept() {
	addrinfo_p new_info = make_addrinfo(false);
	int result = ::accept(fd, new_info->ai_addr, &new_info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "accept() failed");
	}	
	return Socket(result, move(new_info));
}

void Socket::connect() {
	// TODO: traverse the linked list for more results if connection fails.
	int result = ::connect(fd, info->ai_addr, info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "connect() failed");
	}
}

int Socket::recv(std::vector<uint8_t>& buf) {
	int bytes = ::recv(fd, buf.data(), buf.size(), 0);
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
		int n = ::send(fd, buf.data() + sent, len, 0);
		if (n == -1) {
			throw std::system_error(errno, std::generic_category(), "send() failed:");
		}
		sent += n;
		len -= n;
	}

	return sent;
}

void Socket::setsockopt(int optname, int value) {
	int result = ::setsockopt(fd, SOL_SOCKET, optname, &value, sizeof(value));
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "setsockopt() failed:");
	}
}

}


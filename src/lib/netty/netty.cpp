

#include "netty.hpp"
namespace Netty {


std::string inet_ntop(int af, struct addrinfo& addr) {
	char result[256];
	auto res = ::inet_ntop(af, addr.ai_addr, result, addr.ai_addrlen);
	if (res == nullptr) {
		throw std::system_error(errno, std::generic_category(), "inet_ntop() failed:");
	}

	return std::string(result);
}

// addrinfo helper function that safely wraps the results.
addrinfo_p getaddrinfo(std::string node, std::string service, struct addrinfo* hints) {
	const char* addrnode;
	if (node == "") {
		addrnode = nullptr;
	} else {
		addrnode = node.c_str();
	}

	struct addrinfo* results = {};
	int err = ::getaddrinfo(addrnode, service.c_str(), hints, &results);
	if (err != 0) {
		throw std::runtime_error(gai_strerror(err));
	}
	// unruly hack since getaddrinfo is strange.
	return addrinfo_p(results);
}

addrinfo_p getaddrinfo(std::string node, std::string service, bool passive) {
	addrinfo_p hints = make_addrinfo(passive);
	if (passive) {
		return getaddrinfo("", service, hints.get());
	} else {
		return getaddrinfo(node, service, hints.get());
	}
}

	


addrinfo_p make_addrinfo(bool passive) noexcept {
	struct addrinfo* addr = new struct addrinfo;
	std::memset(addr, 0, sizeof(struct addrinfo));
	addr->ai_family = AF_UNSPEC;
	addr->ai_socktype = SOCK_STREAM;
	if (passive) {
		addr->ai_flags = AI_PASSIVE;
	}
	return addrinfo_p(addr);
}


void Socket::open() {
	if (sock_fd != -1) {
		return;
	}
	if (!info) {
		return; // addrinfo is missing.
	}
	sock_fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);

	if (sock_fd == -1) {
		throw std::system_error(errno, std::generic_category(), "socket() failed:");
	}
}


void Socket::close() {
	int res = ::close(sock_fd);
	// we don't need to call freeaddrinfo since it's called automatically.
}

void Socket::setaddrinfo(addrinfo_p new_info) {
	info = move(new_info);
}

// Destroys a socket (called automatically). Closes the file descriptor and frees the
// addrinfo stored (if any)
Socket::~Socket() {
	close();
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

// accept a connection.
Socket Socket::accept() {
	addrinfo_p new_info = make_addrinfo(false);
	int result = ::accept(sock_fd, new_info->ai_addr, &new_info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "accept() failed:");
	}	
	return Socket(result, move(new_info));
}

void Socket::connect() {
	// TODO: traverse the linked list for more results if connection fails.
	int result = ::connect(sock_fd, info->ai_addr, info->ai_addrlen);
	if (result == -1) {
		throw std::system_error(errno, std::generic_category(), "connect() failed:");
	}
}

int Socket::recv(std::vector<uint8_t>& buf) {
	int bytes = ::recv(sock_fd, buf.data(), buf.size(), 0);
	if (bytes == -1) {
		throw std::system_error(errno, std::generic_category(), "recv() failed:");
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

}


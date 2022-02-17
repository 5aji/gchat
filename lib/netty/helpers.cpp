
#include "netty.hpp"
#include <cstring>
#include <arpa/inet.h>
#include <system_error>
namespace Netty {

std::string inet_ntop(int af, struct addrinfo& addr) {
	char result[256];
	auto res = ::inet_ntop(af, addr.ai_addr, result, addr.ai_addrlen);
	if (res == nullptr) {
		throw std::system_error(errno, std::generic_category(), "inet_ntop() failed");
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
}

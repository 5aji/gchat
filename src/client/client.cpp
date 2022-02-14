// Client main source.
#include "netty/netty.hpp"
#include <arpa/inet.h>
#include <cstdio>
int main() {

	

	auto gotten = Netty::getaddrinfo("localhost", "5555", false);

	auto sock = Netty::Socket(move(gotten));

	sock.open();
	sock.connect();

	std::string message = "Hello world!";

	std::vector<uint8_t> vec(message.begin(), message.end());


	sock.send(vec);
	return -1;
}

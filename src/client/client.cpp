// Client main source.
#include "netty/netty.hpp"
#include <arpa/inet.h>
#include <cstdio>
int main() {

	

	Netty::addrinfo_p gotten = Netty::getaddrinfo("localhost", "5555", false);

	Netty::Socket sock = Netty::Socket(move(gotten));

	sock.open();
	sock.connect();

	std::string message = "Hello world!";

	std::vector<uint8_t> vec(message.begin(), message.end());


	sock.send(vec); // send data!
	return -1;
}

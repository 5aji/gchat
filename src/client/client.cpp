// Client main source.
#include "netty/netty.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include <memory>

using test = struct {
	int a;
	int b;
};
int main() {

	/* auto epoll = polly::Epoll<Netty::Socket>(); */

	/* Netty::addrinfo_p gotten = Netty::getaddrinfo("localhost", "5555", false); */


	/* auto sock = std::make_shared<Netty::Socket>(move(gotten)); */

	/* sock->open(); */
	/* sock->connect(); */

	/* std::string message = "Hello world!"; */

	/* std::vector<uint8_t> vec(message.begin(), message.end()); */


	/* sock->send(vec); // send data! */

	auto timer1 = std::make_shared<polly::Timer>();
	timer1->setnonblocking(true);
	timer1->settime(3, 0, false);

	auto timer2 = std::make_shared<polly::Timer>();
	timer2->setnonblocking(true);
	timer2->settime(5, 0, false);
	auto epoll = polly::Epoll<polly::Timer>();

	epoll.add_item(timer1, EPOLLIN);
	
	epoll.add_item(timer2, EPOLLIN);
	while (1) {
		std::cout << "waiting..." << std::endl;
		auto results = epoll.wait(-1);
		std::cout << "got something" << std::endl;
		for (auto i : results) {
			auto timmy = i.object.lock();
			std::cout << "fd:" << timmy->get_fd() << " read(): " << timmy->read() << std::endl;
		}
	}

	std::cout << "done" << std::endl;

	return 0;
}

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
	int my_val = 0;
	auto timer1 = std::make_shared<polly::Timer>();
	timer1->setnonblocking(true);
	timer1->settime(3, 0, false);
	timer1->set_handler([&my_val](polly::Timer& t, int events) {
			t.read();
			std::cout << my_val << "hi from timer 1\n";
			});

	auto timer2 = std::make_shared<polly::Timer>();
	timer2->setnonblocking(true);
	timer2->settime(5, 0, false);
	timer2->set_handler([&my_val](polly::Timer& t, int events) {
			t.read();
			if (my_val > 5) my_val = 0;
			std::cout << "hi from timer 2 \n";
			});

	auto epoll = polly::Epoll();

	epoll.add_item(timer1, EPOLLIN);	
	epoll.add_item(timer2, EPOLLIN);

	while (1) {
		std::cout << "waiting..." << std::endl;
		epoll.wait(-1);
		std::cout << "got something" << std::endl;
		my_val++;
	}

	std::cout << "done" << std::endl;

	return 0;
}

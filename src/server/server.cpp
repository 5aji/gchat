// Server main source

#include "netty/netty.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include <memory>

struct ClientSession {
	int count = 0;
};

// TODO: some way of mapping AbstractFileDes (compare operators?)
// some state mechanism. serialization of messages.
int main() {
	
	Netty::addrinfo_p gotten = Netty::getaddrinfo("localhost", "5555", true);


	auto listen_socket = std::make_shared<Netty::Socket>(move(gotten));
	listen_socket->setnonblocking(true);

	listen_socket->bind();
	listen_socket->listen();

	auto epoll = polly::Epoll();
	auto clients = std::map<int, ClientSession>{};

	auto client_handler = [&epoll, &clients](Netty::Socket& s, int events) {
		if (events & EPOLLRDHUP) {
			std::cout << "connection closed\n";
			epoll.delete_item(s);
			return;
		}
		if (events & EPOLLIN) {
			std::cout << "got a client msg\n";
			std::vector<std::byte> data = s.recv_all();
			s.send(data);
			return;
		}
		std::cout << "bad\n";	

	};

	listen_socket->set_handler([&epoll, &clients, &client_handler](Netty::Socket& s, int events) {
			std::cout << "got a conn\n";
			auto new_sock = std::make_shared<Netty::Socket>(s.accept());
			new_sock->setnonblocking(true);
			new_sock->set_handler(client_handler);
			epoll.add_item(new_sock, EPOLLIN | EPOLLRDHUP);
			});


	listen_socket->setsockopt(SO_REUSEADDR, 1);

	epoll.add_item(listen_socket, EPOLLIN);

	std::cout << "Starting server\n";
	while(1) {
		epoll.wait(-1);
	}
}

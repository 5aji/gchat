// Server main source

#include "netty/netty.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include "surreal/surreal.hpp"
#include <memory>



struct ClientSession {
	int count = 0;
	bool authed = false;
	// other session info goes here.
};

// TODO: some way of mapping AbstractFileDes (compare operators?)
// some state tracking system.
int main() {
	
	Netty::addrinfo_p gotten = Netty::getaddrinfo("localhost", "5555", true);


	// testing surreal
	auto myBuf = surreal::DataBuf();

	auto obj = surreal::TestObject{};

	myBuf.serialize(obj);

	auto listen_socket = std::make_shared<Netty::Socket>(move(gotten));
	listen_socket->setnonblocking(true);
	
	listen_socket->bind();
	listen_socket->listen();

	auto epoll = polly::Epoll();
	

	// big state table. maps connections (file descriptors) to sessions.
	auto client_sessions = std::map<int, ClientSession>{};

	// the client handler function. It will manage the lifetime of the
	// connection and recieve messages from the socket.
	auto client_handler = [&](Netty::Socket& s, int events) {
		if (events & EPOLLRDHUP) {
			std::cout << "connection closed\n";
			client_sessions.erase(s.get_fd()); // close the session.
			epoll.delete_item(s);
			return;
		}
		if (events & EPOLLIN) {
			std::cout << "got a client msg\n";
			auto data = s.recv_all();
			ClientSession ses = client_sessions[s.get_fd()];
			// parse data.
			ses.count++;
			s.send(myBuf);
			client_sessions[s.get_fd()] = ses;
			return;
		}
		std::cout << "bad\n";	

	};

	listen_socket->set_handler([&epoll, &client_sessions, &client_handler](Netty::Socket& s, int events) {
			std::cout << "\n";
			auto new_sock = std::make_shared<Netty::Socket>(s.accept());
			client_sessions[new_sock->get_fd()] =  ClientSession{};
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

// Client main source.
#include "netty/netty.hpp"
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include "surreal/surreal.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include <memory>

int main() {

    Netty::addrinfo_p address = Netty::getaddrinfo("localhost", "5555", false);

    auto sock = std::make_shared<Netty::Socket>(move(address));

    sock->connect();

    sock->setnonblocking(true);
    std::string message = "Hello world!";

    std::vector<uint8_t> vec(message.begin(), message.end());

    /* sock->send(vec); // send data! */
    auto timer2 = std::make_shared<polly::Timer>();
    timer2->setnonblocking(true);
    timer2->settime(5, 0, false);
    timer2->set_handler([&sock, &vec](polly::Timer &t, int events) {
        t.read();
        std::cout << "hi from timer 2 \n";
        sock->send(vec);
    });

    sock->set_handler([](Netty::Socket &s, int events) {
        if (events & EPOLLIN) {
            std::cout << "got a msg\n";
            auto data = s.recv_all();
            // the response is always a testobject.
            for (auto val : data) {
                std::cout << std::hex << (int)val << " ";
            }
            std::cout << std::endl << std::dec;
            surreal::TestObject obj{};
            auto buf = surreal::DataBuf(data.begin(), data.end());
            buf.deserialize(obj);
            std::cout << obj.test << std::endl;
            /* std::cout << obj; */
            // do nothing
        }
    });

    auto epoll = polly::Epoll();

    epoll.add_item(timer2, EPOLLIN);
    epoll.add_item(sock, EPOLLIN);

    /* std::cout << test << "\n"; */

    while (1) {
        std::cout << "waiting..." << std::endl;
        epoll.wait(-1);
    }

    std::cout << "done" << std::endl;

    return 0;
}

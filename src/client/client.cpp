// Client main source.
#include "libchat.hpp"
#include "netty/netty.hpp"
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include "surreal/surreal.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <csignal>
#include <memory>
#include <queue>

// track if we are authenticated or not.
struct AuthState {
    bool authed = false;
    std::string username = "";
};
AuthState auth_state;
// when we send a message, we push the message_t to the queue, then when we recv
// OK or Error, we use this to track which message actually was ok or errrored.
// this way if we register twice and the second one fails, we can accurately
// state that the *second* one failed, not the first one. likewise, if we send login twice,
// we only really update auth_state for the login that was correct.
std::queue<std::pair<message_t, Packet_t>> ack_queue;

// to prevent overrunning the unknown-size packet send buffer, we use our own
// queue of frames.
std::queue<Frame> send_queue;

// file parser. returns an int, number of seconds to delay before calling again.
// has 2 magic return values. -1 causes the parser to be run again, -2 disables the 
// timer permanently.
int parseFile(std::ifstream &file) {
    std::string command;
    file >> command;
    if (file.eof()) {
        print("reached end of file. client will remain connected");
        return -2;
    }
    if (command == "DELAY") {
        int delay;
        file >> delay;
        print("executing DELAY " + std::to_string(delay));
        // breaks the parseFile Loop and sets the timer to the returned value.
        return delay;
    } else if (command == "REGISTER") {
        std::string username;
        std::string password;
        file >> username >> password;
        print("executing REGISTER " + username + " " + password);
        LoginPacket packet{.username = username, .password = password};
        auto f = make_frame(message_t::MSG_REGISTER, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_REGISTER, packet));
    } else if (command == "LOGIN") {
        std::string username;
        std::string password;
        file >> username >> password;
        print("executing LOGIN " + username + " " + password);
        LoginPacket packet{.username = username, .password = password};
        auto f = make_frame(message_t::MSG_LOGIN, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_LOGIN, packet));
    } else if (command == "SEND") {
        std::string message;
        std::getline(file, message);
        MessagePacket packet{.message = message,
                             .username = auth_state.username};
        auto f = make_frame(message_t::MSG_SEND, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_SEND, packet));
    } else if (command == "SEND2") {
        std::string destination;
        file >> destination;
        std::string message;
        std::getline(file, message);
        MessagePacket packet{.message = message,
                             .username = auth_state.username,
                             .destination = destination};
        auto f = make_frame(message_t::MSG_SEND, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_SEND, packet));
    } else if (command == "SENDA") {
        std::string message;
        std::getline(file, message);
        MessagePacket packet{.message = message};
        auto f = make_frame(message_t::MSG_SEND, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_SEND, packet));
    } else if (command == "SENDA2") {
        std::string destination;
        file >> destination;
        std::string message;
        std::getline(file, message);
        MessagePacket packet{.message = message, .destination = destination};
        auto f = make_frame(message_t::MSG_SEND, packet);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_SEND, packet));
    } else if (command == "SENDF") {
        // TODO: send file to all"
    } else if (command == "SENDF2") {
        // TODO: send file to single user
    } else if (command == "LIST") {
        auto f = make_frame(message_t::MSG_GETLIST);
        send_queue.push(f);
        ack_queue.push(std::pair(message_t::MSG_GETLIST, std::monostate()));
    } else {
        print("Invalid command detected, skipping line...");
        
    }
    return -1; // in the timer callback, a -1 means continue calling parseFile.
}

// handles server responses. uses ack_queue to track association to the message
// we sent. can update state this way, since it also tracks the contents of the
// sent packet. Handles printing out messages and stuff.
std::optional<Frame> serverHandler(message_t resp, Packet_t pkt) {
    message_t msg_ack = ack_queue.front().first;
    Packet_t msg_pkt = ack_queue.front().second;
    auto sent_name = message_names.find(msg_ack);
    if (resp == message_t::MSG_OK) {
        print(sent_name->second + " OK!");
        // if the message we sent was a login, we know it worked now and can set
        // our username
        if (msg_ack == message_t::MSG_LOGIN) {
            auto p = std::get<LoginPacket>(msg_pkt);
            auth_state.username = p.username;
            auth_state.authed = true;
        }
        // pop the ack queue.
        ack_queue.pop();
    }
    auto error_name = error_meanings.find(resp);
    if (error_name != error_meanings.end() &&
        sent_name != message_names.end()) {
        // we have an error response and the sent type can be printed, so print
        // generic.
        print(sent_name->second + " failed: " + error_name->second);
        ack_queue.pop();
    }

    if (resp == message_t::MSG_SEND) {
        // display the message.
        auto message = std::get<MessagePacket>(pkt);
        std::string username = message.username;
        if (username == "") {
            username = "Anonymous";
        }
        print(username + " said: " + message.message);
    }
    if (resp == message_t::MSG_LIST) {
        // print list of members
        auto message = std::get<ListPacket>(pkt);
        std::string users;
        for (const auto& u : message.users) {
            users.append("\t" + u + "\n");
        }
        print("Currently (" + std::to_string(message.users.size()) + ") users online:\n" + users); 
    }
    if (resp == message_t::MSG_XFER) {
    }
    return std::nullopt;
}
int main(int argc, char * argv[]) {

    if (argc != 4) {
        print("ERROR: missing arguments. stopping.");
        exit(-1);
    }

    std::string addr = argv[1];
    std::string port = argv[2];
    std::string command_filename = argv[3];

    Netty::addrinfo_p address;
    try {
        address = Netty::getaddrinfo(addr, port, false);
    } catch (std::runtime_error& e) {
        print("ERROR: couldn't resolve server address: ");
        print(e.what());
        exit(-1);
    }

    auto sock = std::make_shared<Netty::Socket>(move(address));

    auto epoll = polly::Epoll();
    auto timer2 = std::make_shared<polly::Timer>();
    std::ifstream commands;
    commands.open(command_filename);
    // set up socket
    try {
        sock->connect();
    } catch (std::system_error& e) {
        print("Error connecting, exiting...");
        print(e.what());
        exit(-1);
    }
    sock->setnonblocking(true);

    // set up timer.
    timer2->setnonblocking(true);
    timer2->settime(1, 0, false);
    timer2->set_handler([&](polly::Timer &t, int events) {
        t.read(); // clear the timer.
        int delay = -1;
        while (delay == -1) { // run until we hit a real delay.
            delay = parseFile(commands);
            if (send_queue.size() > 0) {
                epoll.set_events(*sock, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
            }
            if (delay == -2) {
                t.disarm();
                delay = 0;
                break;
            }
        }
        t.settime(delay, 0, false);
    });

    // this function handles all the socket events and calls the correct helper
    // functions. It is responsible for data serialization and sending queued
    // messages.
    sock->set_handler([&](Netty::Socket &s, int events) {
        static recv_state r_state;
        if (events & EPOLLRDHUP) {
            // close the socket. error?
            print("ERROR: server connection closed. Exiting...");
            epoll.delete_item(s);
            s.close();
            exit(-1);
        }
        if (events & EPOLLIN) {
            auto new_data = s.recv(1024);
            r_state.data.insert(r_state.data.end(), new_data.begin(),
                                new_data.end());
            // std::cout << "got " << new_data.size() << " bytes, total " <<
            // r_state.data.size() << "\n";
            // // if the size of the buffer is greater than the size of our
            // frame header, then std::cout << "awaiting " <<
            // r_state.desired_bytes << " for a " << (r_state.await_header ?
            // "header" : "frame") << "\n"; we decode the header and update our
            // new thing.
            while (r_state.data.size() >= r_state.desired_bytes) {
                if (r_state.await_header) {
                    // std::cout << "got enough data for a header\n";
                    auto header = surreal::DataBuf(r_state.data.begin(),
                                                   r_state.data.begin() +
                                                       r_state.desired_bytes);
                    r_state.data.erase(r_state.data.begin(),
                                       r_state.data.begin() +
                                           r_state.desired_bytes);
                    unsigned char dummy;
                    header.deserialize(dummy);
                    if (dummy != 0xFE) {
                        throw std::runtime_error(
                            "frame misalignment. this should never happen");
                    }
                    std::size_t payload_size;
                    header.deserialize(payload_size);
                    r_state.await_header = false;
                    r_state.desired_bytes = payload_size;
                } else {
                    // we have enough bytes for the intended frame.
                    // std::cout << "got enough data for the frame\n";
                    // copy the vector
                    auto frame_buf = std::vector<uint8_t>(
                        r_state.data.begin(),
                        r_state.data.begin() + r_state.desired_bytes);
                    r_state.data.erase(r_state.data.begin(),
                                       r_state.data.begin() +
                                           r_state.desired_bytes);
                    auto message = get_frame(frame_buf);
                    auto type = std::get<message_t>(message);
                    auto payload = std::get<Packet_t>(message);
                    auto response = serverHandler(type, payload);
                    if (response.has_value()) {
                        send_queue.push(response.value());
                        epoll.set_events(s, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
                    }

                    // reset for the next header.
                    r_state.await_header = true;
                    r_state.desired_bytes = header_size;
                }
            }
        }
        if (events & EPOLLOUT) {
            if (send_queue.size() == 0) {
                // disable epollout events for now.
                epoll.set_events(s, EPOLLIN | EPOLLRDHUP);
                return;
            }
            // pull frame from queue and send it.
            s.send_delimited(send_queue.front());
            send_queue.pop();
            // we assume frames are always smaller than the kernel send buffer.
        }
    });

    // add both items to the epoll list.
    epoll.add_item(timer2, EPOLLIN);
    epoll.add_item(sock, EPOLLIN | EPOLLOUT | EPOLLRDHUP);

    std::signal(SIGINT, sighandler);
    while (1) {
        try {
            epoll.wait(-1);
        } catch (std::system_error& e) {
            if (quit.load()) break;
            else throw e;
        }
    }

    return 0;
}

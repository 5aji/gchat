// Server main source

#include "libchat.hpp"
#include "netty/netty.hpp"
#include "polly/polly.hpp"
#include "polly/timer.hpp"
#include "surreal/surreal.hpp"
#include "datastore.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <ios>
#include <iostream>
#include <memory>
#include <queue>
#include <stdlib.h>
#include <csignal>
#include <time.h>

// A container for client connection state.
// contains their username, whether or not they are authenticated,
// as well as sending and recv queues.
struct ClientSession {
    bool authed = false;
    std::string username;
    recv_state r_state;
    std::queue<Frame> send_queue;
};

// big state table. Maps connections (file descriptors) to sessions (connection
// state)
auto socket_sessions = std::map<int, std::shared_ptr<ClientSession>>{};
// a map of usernames to sessions, managed by login/logout
auto username_sessions =
    std::map<std::string, std::shared_ptr<ClientSession>>{};

// on-disk stuff.

auto store = DataStore<ServerData>("serverdata.bin");


// takes an input frame and gives an appropriate response.
std::optional<Frame> clientHandler(message_t msg, Packet_t pkt,
                                   std::shared_ptr<ClientSession> session) {
    if (msg == message_t::MSG_REGISTER) {
        // check that username doesn't exist,
        auto contents = std::get<LoginPacket>(pkt);
        if (store.data.find_user(contents.username) != store.data.user_database.end()) {
            // user exists.
            return make_frame(message_t::ERR_USEREXISTS);
        }
        // store password and return ok
        store.data.user_database.push_back(contents);
        return make_frame(message_t::MSG_OK);
    }
    if (msg == message_t::MSG_LOGIN) {
        auto contents = std::get<LoginPacket>(pkt);
        auto pw = store.data.find_user(contents.username);
        if (pw == store.data.user_database.end()) {
            return make_frame(message_t::ERR_NOTREGISTERED);
        }
        if (pw->password != contents.password) {
            return make_frame(message_t::ERR_PASSWRONG);
        }
        if (username_sessions.find(contents.username) !=
            username_sessions.end()) {
            return make_frame(message_t::ERR_ALREADYLOGGEDIN);
        }
        if (session->authed) {
            return make_frame(message_t::ERR_ALREADYLOGGEDIN);
        }
        // set authed and username.
        session->authed = true;
        session->username = contents.username;
        // add username + session pointer.
        username_sessions[contents.username] = session;
        // restore messages and clear them.
        std::for_each(store.data.get_user_msgs(contents.username), store.data.offline_msgs.end(),
                [&session](const MessagePacket& m){
                    session->send_queue.push(make_frame(message_t::MSG_SEND, m));
                });
        store.data.clear_user_msgs(contents.username);
        return make_frame(message_t::MSG_OK);
    }
    if (msg == message_t::MSG_SEND) {
        if (!session->authed) {
            return make_frame(message_t::ERR_NOLOGIN);
        }
        // to broadcast, loop over username_sessions and put frame on send_queue
        // if username not current user for DMs, try accessing the session
        // directly.
        auto contents = std::get<MessagePacket>(pkt);
        if (contents.username != session->username && contents.username != "") {
            // not an anonymous and not a message from us, so we respond with an
            // error. NOPERMS. This means that some clients can have permissions
            // to send as anyone. (admins).
            return make_frame(message_t::ERR_NOPERMS);
        }
        auto message = make_frame(msg, contents);
        if (contents.destination == "") {
            // broadcast-type message.
            for (const auto &[name, ses] : username_sessions) {
                if (name != session->username) {
                    ses->send_queue.push(message);
                }
            }
        } else {
            try {
                username_sessions.at(contents.destination)
                    ->send_queue.push(message);
            } catch (std::out_of_range &e) {
                if (store.data.find_user(contents.destination) != store.data.user_database.end()) {
                   store.data.offline_msgs.push_back(contents); 
                } else {
                    return make_frame(message_t::ERR_NOSUCHUSER);
                }
            }
        }
        return make_frame(message_t::MSG_OK);
    }
    if (msg == message_t::MSG_XFER) {
        if (!session->authed) {
            return make_frame(message_t::ERR_NOLOGIN);
        }

        auto contents = std::get<FilePacket>(pkt);
        if (contents.username != session->username && contents.username != "") {
            return make_frame(message_t::ERR_NOPERMS);
        }
        auto message = make_frame(msg, contents);
        if (contents.destination == "") {
            // broadcast-type message.
            for (const auto &[name, ses] : username_sessions) {
                if (name != session->username) {
                    ses->send_queue.push(message);
                }
            }
        } else {
            try {
                username_sessions.at(contents.destination)
                    ->send_queue.push(message);
            } catch (std::out_of_range &e) {
                return make_frame(message_t::ERR_NOSUCHUSER);
            }
        }

    }
    if (msg == message_t::MSG_LOGOUT) {
        // TODO: if we are already logged out, should this fail with NOLOGIN?
        session->authed = false;
        username_sessions.erase(session->username);
        return make_frame(message_t::MSG_OK);
    }

    if (msg == message_t::MSG_GETLIST) {
        if (!session->authed) {
            return make_frame(message_t::ERR_NOLOGIN);
        }
        std::vector<std::string> users;
        for (const auto& lp : store.data.user_database) {
            users.push_back(lp.username);
        }

        ListPacket pack { .users = users };

        return make_frame(message_t::MSG_LIST, pack);
     
    }

    return std::nullopt;
}
int main(int argc, char* argv[]) {

    if (argc != 2) {
        print("ERROR: incorrect number of arguments. usage: ./server <port>");
        exit(-1);
    }

    std::string port = argv[1];
    if (port == "reset") {
        print("resetting internal database");
        store.reset();

        exit(0);
    }

    Netty::addrinfo_p gotten;
    try {
        gotten = Netty::getaddrinfo("", port, true);
    } catch (std::runtime_error& e) {
        print("ERROR: couldn't resolve port:");
        print(e.what());
        exit(-1);
    }

    store.load();

    auto listen_socket = std::make_shared<Netty::Socket>(move(gotten));
    auto epoll = polly::Epoll();

    listen_socket->setnonblocking(true);

    listen_socket->bind();
    listen_socket->listen();

    // the client handler function. It will manage the lifetime of the
    // connection and receive messages from the socket.
    auto client_handler = [&](Netty::Socket &s, int events) {
        auto session = socket_sessions[s.get_fd()];
        if (events & EPOLLRDHUP) {
            print("closing connection " + std::to_string(s.get_fd()));
            if (session->authed) {
                username_sessions.erase(session->username);
            }
            socket_sessions.erase(s.get_fd()); // cleanup the session.
            epoll.delete_item(s);
            return;
        }
        if (events & EPOLLIN) {
            auto new_data = s.recv(1024);
            recv_state &r_state = session->r_state;
            session->r_state.data.insert(session->r_state.data.end(),
                                         new_data.begin(), new_data.end());
            // 	std::cout << "got " << new_data.size() << " bytes, total " <<
            // r_state.data.size() << "\n"; std::cout << "awaiting " <<
            // r_state.desired_bytes << " for a " << (r_state.await_header ?
            // "header" : "frame") << "\n";
            while (r_state.data.size() >= r_state.desired_bytes) {
                if (r_state.await_header) {
                    auto header = surreal::DataBuf(r_state.data.begin(),
                                                   r_state.data.begin() +
                                                       r_state.desired_bytes);
                    r_state.data.erase(r_state.data.begin(),
                                       r_state.data.begin() +
                                           r_state.desired_bytes);
                    unsigned char dummy;
                    header.deserialize(dummy);
                    if (dummy != 0xFE) {
                        throw std::runtime_error("frame bad");
                    }

                    std::size_t payload_size;
                    header.deserialize(payload_size);
                    // std::cout << "got enough data for a header, packet size:
                    // " << payload_size << std::endl;
                    r_state.await_header = false;
                    r_state.desired_bytes = payload_size;
                } else {
                    // we have enough bytes for the intended frame.
                    // std::cout << "got enough data for a frame\n";
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
                    auto response = clientHandler(type, payload, session);
                    if (response.has_value()) {
                        session->send_queue.push(response.value());
                        epoll.set_events(s, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
                    }

                    // reset for the next header.
                    r_state.await_header = true;
                    r_state.desired_bytes = header_size;
                }
            }
        }
        if (events & EPOLLOUT) {
            // we can write,
            if (session->send_queue.size() == 0) {
                epoll.set_events(s, EPOLLIN | EPOLLRDHUP);
                return;
            }
            s.send_delimited(session->send_queue.front());
            session->send_queue.pop();
        }
    };

    listen_socket->set_handler([&epoll, &client_handler](Netty::Socket &s,
                                                         int events) {
        auto new_sock = std::make_shared<Netty::Socket>(s.accept());
        socket_sessions[new_sock->get_fd()] = std::make_shared<ClientSession>();
        new_sock->setnonblocking(true);
        new_sock->set_handler(client_handler);
        epoll.add_item(new_sock, EPOLLIN | EPOLLRDHUP);
    });

    listen_socket->setsockopt(SO_REUSEADDR, 1);

    epoll.add_item(listen_socket, EPOLLIN);

    std::signal(SIGINT, sighandler);
    print("Server starting...");
    while (1) {
        try {
            epoll.wait(-1);
        } catch (std::system_error& e) {
            if (quit.load()) {
                break;
            } else throw e;
        }
    }
    print("Shutting down...");
    return 0;
}

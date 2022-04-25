// libchat - common core for GopherChat wire protocol
// Saji Champlin 2022

#pragma once
#include "netty/netty.hpp"
#include "surreal/surreal.hpp"
#include <cstddef>
#include <iomanip>
#include <map>
#include <stddef.h>
#include <stdint.h>
#include <variant>
#include <vector>
#include <atomic>
#include <unistd.h>
#include <signal.h>
#define FILE_BLOCKSIZE 2048

// we use this to check at runtime if the message we recieved has
// the right protocol spec (no mismatching allowed).
#define PROTOCOL_VERSION 0
// Enum message types which are stored in the header.
enum class message_t {
    MSG_OK = 0,
    MSG_REGISTER,
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_SEND,
    MSG_XFER,
    MSG_GETLIST,
    MSG_LIST, // actually recv the list

    // ERROR messages. empty data payload.
    ERR_NOLOGIN = 400, // when a client isn't logged in
    ERR_NOTREGISTERED, // username not registered.
    ERR_ALREADYLOGGEDIN,
    ERR_USEREXISTS, // username already taken, can't re-register.
    ERR_NOSUCHUSER, // can't message user since they don't exist.
    ERR_PASSWRONG,  // wrong password
    ERR_NOPERMS,    // tried to send message as a different user.
    ERR_NOSUCHFILE
};
const std::map<message_t, std::string> message_names{
    {message_t::MSG_OK, "OK"},
    {message_t::MSG_REGISTER, "REGISTER"},
    {message_t::MSG_LOGIN, "LOGIN"},
    {message_t::MSG_LOGOUT, "LOGOUT"},
    {message_t::MSG_SEND, "SEND"},
    {message_t::MSG_XFER, "XFER"},
    {message_t::MSG_GETLIST, "LIST"},
    {message_t::ERR_NOLOGIN, "NOLOGIN"},
    {message_t::ERR_NOTREGISTERED, "NOTREGISTERED"},
};

const std::map<message_t, std::string> error_meanings{
    {message_t::ERR_NOLOGIN, "not logged in"},
    {message_t::ERR_NOTREGISTERED, "not registered"},
    {message_t::ERR_ALREADYLOGGEDIN, "logged in elsewhere"},
    {message_t::ERR_USEREXISTS, "username already registered"},
    {message_t::ERR_NOSUCHUSER, "user does not exist"},
    {message_t::ERR_PASSWRONG, "incorrect password"},
    {message_t::ERR_NOPERMS, "tried to send message as a different user"},
};
// we could later just make this a std::string if we want (would have to add
// serialization stuff)
using shortstring = std::string;

// The messageframe contains a message type, a protocol version (for
// simple checking purpose) and the actual message itself. the serialization
// library will handle the variable-sized data vector. (it will be encoded).
struct Frame {
    char version = PROTOCOL_VERSION;
    message_t type;

    std::vector<std::uint8_t> data; // the actual data

    // we use type to determine how to deserialize data.
    // assuming that it works... surreal might need more error checking.

    MAKE_SERIAL(version, type, data)
    operator std::vector<uint8_t>() {
        auto buf = surreal::DataBuf();
        buf.serialize(*this);
        return buf;
    }
};

struct MessagePacket {
    std::string message;
    shortstring username; // if blank, (all 0s), then it's an anonymous message.
    shortstring destination; // if all 0s, global message, else send message to
                             // this user.

    MAKE_SERIAL(username, destination, message)
};

struct LoginPacket {
    shortstring username;
    shortstring password;

    MAKE_SERIAL(username, password)
};

struct FilePacket {
    bool eof = false;
    std::string filename; // where to store the file
    std::vector<char> data;
    shortstring destination; // user to send the file to
    static constexpr auto max_size = 1024;

    MAKE_SERIAL(eof, filename, data, destination)
};

// contains a list of users currently logged on.
struct ListPacket {
    std::vector<shortstring> users;
    MAKE_SERIAL(users)
};

// END packet definitions

// all possible packets. Monostate is so that it can be "empty".
using Packet_t = std::variant<std::monostate, MessagePacket, LoginPacket,
                              FilePacket, ListPacket>;


// return the message type of the frame. Useful for server responses.
message_t get_message(const std::vector<std::uint8_t> &data) {
    Frame frame;
    auto buf = surreal::DataBuf(data.begin(), data.end());
    buf.deserialize(frame);
    if (frame.version != PROTOCOL_VERSION) {
        throw std::runtime_error("Protocol version didn't match!");
    }

    return frame.type;
}

/**
 * Unpack a frame from a TCP data stream.
 */
std::pair<message_t, Packet_t> get_frame(std::vector<std::uint8_t> data) {
    Frame frame = {};
    // take data, deserialize it into frame.
    auto buf = surreal::DataBuf(data.begin(), data.end());
    buf.deserialize(frame);

    if (frame.version != PROTOCOL_VERSION) {
        throw std::runtime_error("Protocol version didn't match!");
    }

    if ((frame.type == message_t::MSG_LOGIN) ||
        (frame.type == message_t::MSG_REGISTER)) {
        LoginPacket res;
        auto obj = surreal::DataBuf(frame.data.begin(), frame.data.end());
        obj.deserialize(res);
        return std::pair(frame.type, res);
    } else if (frame.type == message_t::MSG_LIST) {
        ListPacket res;
        auto obj = surreal::DataBuf(frame.data.begin(), frame.data.end());
        obj.deserialize(res);
        return std::pair(frame.type, res);
    } else if (frame.type == message_t::MSG_SEND) {
        MessagePacket res;
        auto obj = surreal::DataBuf(frame.data.begin(), frame.data.end());
        obj.deserialize(res);
        return std::pair(frame.type, res);
    } else if (frame.type == message_t::MSG_XFER) {
        FilePacket res;
        auto obj = surreal::DataBuf(frame.data.begin(), frame.data.end());
        obj.deserialize(res);
        return std::pair(frame.type, res);
    }

    // no data, so we just return the message type.
    auto s = Packet_t{};
    return std::pair(frame.type, s);
}

/*
 * Create a frame from a packet object
 */
template <surreal::has_members T> Frame make_frame(message_t msg, T &object) {
    Frame result = {};
    result.data = surreal::DataBuf(object);
    result.type = msg;
    return result;
}

// shorthand for sending error messages.
Frame make_frame(message_t msg) {
    Frame result = {};
    result.type = msg;
    return result;
}

// returns true if the string is within spec.
// we could turn this off and we would be able to use long usernames/passwords.
bool string_okay(std::string str) {
    if ((str.length() > 8) || (str.length() < 4)) {
        return false;
    }
    for (char &c : str) {
        if (!std::isalnum(c))
            return false;
    }
    return true;
}

constexpr auto header_size = sizeof(char) + sizeof(std::size_t);
struct recv_state {
    std::vector<std::uint8_t> data;
    std::size_t desired_bytes = header_size;
    bool await_header = true;
};

// print helper. prints timestamp plus message.
void print(std::string msg) {
    std::time_t time = std::time(nullptr);

    std::cout << std::put_time(std::localtime(&time), "%F %T") << ":" + msg
              << std::endl;
}


// helpers for intercepting sigint (allow for proper destructor calling).

// instead of volatile, use atomic.
std::atomic<bool> quit(false);
auto sighandler = [](int m) {
	quit.store(true);
};


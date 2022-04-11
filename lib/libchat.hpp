// libchat - common core for GopherChat wire protocol
// Saji Champlin 2022
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <cstddef>
#include "netty/netty.hpp"
#include "surreal/surreal.hpp"
#define FILE_BLOCKSIZE 2048

// we use this to check at runtime if the message we recieved has
// the right protocol spec (no mismatching allowed).
#define PROTOCOL_VERSION 0
// Enum message types which are stored in the header.
enum class message_t:char {
	MSG_REGISTER = 0,
	MSG_LOGIN,
	MSG_LOGOUT,
	MSG_RECV, // sent by the server, tells client that there is a message.
	MSG_RECV_XFER,
	MSG_SEND_GLOBAL,
	MSG_SEND_DIRECT,
	MSG_XFER_GLOBAL,
	MSG_XFER_DIRECT,
	MSG_LIST,
	MSG_ENCRYPTED
};

template<class T>
using entry = std::pair<message_t, T>;

/* constexpr map = */ 


// The messageframe contains a message type, a protocol version (for
// simple checking purpose) and the actual message itself. the serialization
// library will handle the variable-sized data vector. (it will be encoded).
struct Frame {
	char version = PROTOCOL_VERSION;
	message_t type;

	std::vector<std::uint8_t> data; // the actual data

	// we use type to determine how to deserialize data.
	// assuming that it works... surreal might need more error checking.
	
	MAKE_SERIAL(version,type,data);
};

struct MessagePacket  {
	std::array<char, 256> message;
	std::array<char, 8> username;
	std::array<char, 8> destination;
};

struct LoginPacket {
	std::array<char, 8> username;
	std::array<char, 8> password;
};

struct FilePacket {
	uint32_t count;
	std::array<uint8_t, FILE_BLOCKSIZE> data;
	std::array<char,8> username;
	std::array<char,8> password;
};


Frame get_frame(std::vector<std::uint8_t> data) {
	Frame frame = {};
	// take data, deserialize it into frame.
	auto buf = surreal::DataBuf(data.begin(), data.end());
	buf.deserialize(frame);

	if (frame.version != PROTOCOL_VERSION) {
		throw std::runtime_error("Protocol version didn't match!");
	}
	return frame;
}

template<typename T>
/* requires std::invocable<surreal::DataBuf::serialize, T> */
Frame make_frame(T& object) {
	Frame result = {};

}


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
enum message_t:char {
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




// The messageframe contains 
struct MsgFrame {
	static constexpr std::byte version{0};
	message_t type;
	uint32_t size;
	std::byte data[]; // the raw bytes for the payload.
	// the program should use the message_t and size_t to allocate
	// and deserialize the data.
	template <surreal::Serializable T>
	MsgFrame(T& object, message_t type) {

	}
};

struct MessagePacket  {
	char message[256];
	char username[8]; // who sent it
	char destination[8]; // who should recieve it (empty for global)
};

struct LoginPacket {
	char username[8];
	char password[8];
};

struct FilePacket {
	uint32_t count;
	uint8_t data[FILE_BLOCKSIZE];
	uint8_t username[8];
	uint8_t destination[8];
};


// libchat - common core for GopherChat wire protocol
// Saji Champlin 2022
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <cstddef>
#include "netty.hpp"
#define FILE_BLOCKSIZE 2048
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


// The header contains the type of the message (which indicates
// what kind of struct we should cast to) as well as the size
// of the data (which tells us how much to malloc).
class HeaderPacket : NetworkPacket {
	message_t type;
	uint32_t size;
};

class MessagePacket : NetworkPacket {
	char message[256];
	char username[8]; // who sent it
	char destination[8]; // who should recieve it (empty for global)
};

class LoginPacket : NetworkPacket {
	char username[8];
	char password[8];
};


class FilePacket : NetworkPacket {
	uint32_t count;
	uint8_t data[FILE_BLOCKSIZE];
	uint8_t username[8];
	uint8_t destination[8];
};


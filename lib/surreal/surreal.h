// surreal - A dumb/bad/stupid C++ data serialization/deserialization library
// (c) Saji Champlin 2022
// don't actually use this for anything serious.
// it's probably broken in several ways and will set your computer on fire.

#include <vector>
#include <cstdint>
#include <memory>
namespace surreal {

// We define a basic interface for a struct that can be serialized
// and deserialized into bytes. Implementations will have to handle
// network-ordering and such.
struct Serializable {
	virtual void serialize(std::vector<uint8_t>& buf);
	virtual void deserialize(std::vector<uint8_t>& buf);
	virtual ~Serializable();
};

// This type can be used to contain a list of Serializable structs.

using SerializableVec = std::vector<std::unique_ptr<Serializable>>;

// This function takes a list of structs that implement Serializable
// and returns a sequence of bytes that can be sent over the network.
std::vector<uint8_t> serialize(SerializableVec structs);

SerializableVec deserialize(std::vector<uint8_t> buf);

}

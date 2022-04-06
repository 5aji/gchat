// surreal - A dumb/bad/stupid C++ data serialization/deserialization library
// (c) Saji Champlin 2022
// don't actually use this for anything serious.
// it's probably broken in several ways and will set your computer on fire.
#include <iostream>
#include <vector>
#include <tuple>
#include <deque>
#include <concepts>
#include <ranges>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
/* #include < */

// a macro to generate variables containing all the member variables
// of a struct. This way we can iterate over them at compile time.
#define MAKE_SERIAL(...) 	\
	constexpr auto members() const noexcept {	\
		return std::tie(__VA_ARGS__);		\
	}					\
	constexpr auto members() noexcept {		\
		return std::tie(__VA_ARGS__);		\
	}					\


namespace surreal {

// we do a little bit of concepts
// this prevents overloading all the other operators when
// we template, reducing noise and protecting the user from being dumb.
// also, it makes things more clear at compile time, as it will clearly state
// that it's not Serializable.
template <typename T>
concept Serializable = requires(T v) {
	v.members();
};



// We create a databuf class that can be used like a queue. We also want to
// be able to access it as a vector for sending over the network.
// TODO: evaluate if we shouldn't use a class (instead make the container passed by reference
// + using templates).
class DataBuf {
	std::deque<std::uint8_t> data;
public:
	// the socket stuff expects vectors of uint8_t, so we add a special
	// conversion so we can get the databuf as a vector. auto casting should
	// then work fine. (if not a bit slow).
	operator std::vector<std::uint8_t>(){
		return std::vector<std::uint8_t>{data.begin(), data.end()};
	}

	// now we define how to serialize and deserialize various data types.
	
	// You'll notice the use of memcpy. This may seem stupid, but it's important
	// since it's the only way to reinterpret the exact bits of each type
	// without hell breaking loose (or UB/IB). You could technically use 
	// reinterpret cast with address-of and pointer dereference but that's a lot uglier
	// IMO.
	
	// first, for things that are the same size as uint8_t (int8_t, char)
	// no need to byte swap things here.
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint8_t))
	void serialize(T const& value) {
		uint8_t tmp;
		std::memcpy(&tmp, &value, sizeof(T));
		data.emplace_back(tmp);
	}
	
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint8_t))
	void deserialize(T& value) {
		std::memcpy(&value, data.begin(), sizeof(T));
		data.pop_front();
	}


	// next is for 2 byte things, this uses htons and ntohs	to preserve byte ordering.
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint16_t))
	void serialize(T const& value) {
		uint16_t tmp;
		std::memcpy(&tmp, &value, sizeof(T));
		tmp = htons(tmp);	
		data.emplace_back(static_cast<uint8_t>(tmp));
		data.emplace_back(static_cast<uint8_t>(tmp >> 8));
	}
	
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint16_t))
	void deserialize(T& value) {
		uint16_t tmp = data.front();
		data.pop_front();
		tmp |= (data.front() << 8);
		data.pop_front();
		tmp = ntohs(tmp);
		std::memcpy(&value, &tmp, sizeof(T));
	}

	// finally, 32bit things. at this point we write loops to keep things short.
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint32_t))
	void serialize(T const& value) {
		uint32_t tmp;
		std::memcpy(&tmp, &value, sizeof(T));
		tmp = htonl(tmp);
		for (int i = 0; i < 4; i++)
			data.emplace_back(tmp >> (8 * i));
	}
	
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint32_t))
	void deserialize(T& value) {
		uint32_t tmp;
		for (int i = 0; i < 4; i++) {
			tmp |= (data.front() << (8 * i));
			data.pop_front();
		}
		tmp = ntohl(tmp);
		std::memcpy(&value, &tmp, sizeof(T));
	}


	// and 64 bit things. To do this, we just split it in half and use the 32 bit stuff.
	
	template<typename T>
	requires (sizeof(T) == sizeof(std::uint64_t))
	void serialize(T const& value) {
		std::uint64_t tmp;
		std::memcpy(&tmp, &value, sizeof(T));
		std::uint32_t high = tmp >> 32;
		std::uint32_t low = tmp;
		serialize(high);
		serialize(low);
	}


	template<typename T>
	requires (sizeof(T) == sizeof(std::uint64_t))
	void deserialize(T& value) {
		std::uint32_t high;
		std::uint32_t low;
		deserialize(high);
		deserialize(low);
		std::uint64_t tmp = (static_cast<std::uint64_t>(high) << 32) | low;
		std::memcpy(value, &tmp, sizeof(std::uint64_t));
	}
	
	// next, we define the struct functions. These rely on the struct having a .members() method
	// that returns a tuple of all the variables. See above for the macro used to do this. These
	// are pretty modern functions, requiring fold expressions to unwrap variadic arguments.
	// See below for an example of a similar application to generically print structs.
	template<Serializable T>
	void serialize(T const& object) {

		std::apply([&](auto const... data) {
			(serialize(data), ...);

		}, object.members());
	
	}
	
	template<Serializable T>
	void deserialize(T& object) {	
		std::apply([&](auto const... data) {
			(deserialize(data), ...);

		}, object.members());
	}


	// now we get serious. Vectors and strings and array-like-things.
	// The in-memory format should store the number of elements as a 32 bit int.
	// we *don't* need to store type here, since we aren't responsible for making determinations
	// about type.
	
	template<typename T>
	requires std::ranges::sized_range<T>
	void serialize(T const& range) {
		serialize(std::ranges::size(range));

		for (const auto &value : range) {
			serialize(value);
		}
	}

	template<typename T>
	requires std::ranges::sized_range<T>
	void deserialize(T& range){
		// a bit funky.
		std::size_t size;
		deserialize(size);
		if (size > std::ranges::size(range)) {
			// the range is too small. hopefully this never happens.
			throw std::range_error("size of data larger than destination");
		}

		for (auto& value : range) {
			deserialize(value);
		}
	}


};



// a simple POC for how this will go down for structs. We can use the MAKE_SERIAL macro
// and the Serializable concept to create a generic function that will print
// all the values to a comma seperated list in an ostream.
template <Serializable T>
std::ostream& operator << (std::ostream& os, T const& obj) {

  std::apply([&os](auto const fst, auto const... rest) {
    os << fst;
    ((os << ", " << rest), ...); // This is called a "fold expression".
  }, obj.members());
  return os;
}


// just for kicks, here's a dummy object.
struct TestObject {
	int score = 65;
	char a = 'a';
	char letter = 'c';
	int other = 123;
	std::vector<char> v_char = {'a','b','c'};
	MAKE_SERIAL(score,a,letter, other, v_char)
};


}


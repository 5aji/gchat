// surreal - A dumb/bad/stupid C++ data serialization/deserialization library
// (c) Saji Champlin 2022
// don't actually use this for anything serious.
// it's probably broken in several ways and will set your computer on fire.
#include <iostream>
#include <vector>
#include <tuple>
/* #include < */
#define MAKE_SERIAL(...) 	\
	constexpr auto members() const noexcept {	\
		return std::tie(__VA_ARGS__);		\
	}					\
	constexpr auto members() noexcept {		\
		return std::tie(__VA_ARGS__);		\
	}					\


namespace surreal {

// we do a little bit of concepts
template <typename T>
concept Serializable = requires(T v) {
	v.members();
};

using databuf = std::vector<std::uint8_t>;

template <typename T>
databuf& serialize(databuf& buf, T value);

template <typename T>
databuf& deserialize(databuf& buf, T& value);

template <Serializable T>
databuf& serialize(databuf& buf, T object) {

}

template <Serializable T>
databuf& deserialize(databuf& buf, T& object) {

}


template <Serializable T>
std::ostream& operator << (std::ostream& os, T const& obj) {

  std::apply([&os](auto const fst, auto const... rest) {
    ((os << ", " << rest), ...);
  }, obj.members());
  return os;
}

struct TestObject {
	int score = 0;
	char letter = 'c';
	MAKE_SERIAL(score,letter)
};


}


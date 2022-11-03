// surreal - A dumb/bad/stupid C++ data serialization/deserialization library
// (c) Saji Champlin 2022
// don't actually use this for anything serious.
// it's probably broken in several ways and will set your computer on fire.
#pragma once
#include <concepts>
#include <cstring>
#include <deque>
#include <endian.h>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <vector>
/* #include < */

// the hton stuff doesn't go up to 64 bits, which we want for std::size to work
// easily. so we instead use htobe16 instead. The only consequence of this is
// that the system no longer works if the network byte order is little endian
// but it should work within itself. also network byte order is big endian.

// a macro to generate variables containing all the member variables
// of a struct. This way we can iterate over them at compile time.
// we use forward_as_tuple so references get forwarded correctly (tie and
// make_tuple both copy lvalues).
#define MAKE_SERIAL(...)                                                       \
    constexpr auto members() const noexcept { return std::tie(__VA_ARGS__); }  \
    constexpr auto members() noexcept { return std::tie(__VA_ARGS__); }

namespace surreal {

// we do a little bit of concepts
// this prevents overloading all the other operators when
// we template, reducing noise and protecting the user from being dumb.
// also, it makes things more clear at compile time, as it will clearly state
// that it's not Serializable.
template <typename T>
concept has_members = requires(T v) {
    v.members();
};

// We create a databuf class that can be used like a queue. We also want to
// be able to access it as a vector for sending over the network.
// TODO: evaluate if we shouldn't use a class (instead make the container passed
// by reference
// + using templates).
class DataBuf {
    std::deque<std::uint8_t> data;
    void debug(std::string msg) {
        std::cout << "contents: \n";
        std::cout << msg << std::endl;
        for (auto val : data) {
            std::cout << std::hex << (int)val << " ";
        }
        std::cout << std::endl;
    }

  public:
    // the socket stuff expects vectors of uint8_t, so we add a special
    // conversion so we can get the databuf as a vector. auto casting should
    // then work fine. (if not a bit slow).
    operator std::vector<std::uint8_t>() {
        return std::vector<std::uint8_t>{data.begin(), data.end()};
    }
    DataBuf(){}; // empty buffer for creating packets.
    template <typename I, typename S>
    requires std::input_iterator<I> && std::sentinel_for<S, I> DataBuf(I begin,
                                                                       S end)
        : data(begin, end) {}

    template <typename range>
    requires std::ranges::range<range> DataBuf(range r)
        : data(std::ranges::begin(r)) {}

    // serialize anything.
    template <typename T>
    requires(!std::same_as<T, DataBuf>) // don't override defaulted copy/move
                                        // constructors.
        DataBuf(T &object) {
        serialize(object);
    }

    // now we define how to serialize and deserialize various data types.
    // the order for this stuff is *important* since we don't declare then
    // define. order of specializations determines which ones are preferred.

    // You'll notice the use of memcpy. This may seem stupid, but it's important
    // since it's the only way to reinterpret the exact bits of each type
    // without hell breaking loose (or UB/IB). You could technically use
    // reinterpret cast with address-of and pointer dereference but that's a lot
    // uglier IMO.

    // next, we define the struct functions. These rely on the struct having a
    // .members() method that returns a tuple of all the variables. See above
    // for the macro used to do this. These are pretty modern functions,
    // requiring fold expressions to unwrap variadic arguments. See below for an
    // example of a similar application to generically print structs.
    template <has_members T> void serialize(T const &object) {

        std::apply([&](auto const &...data) { (serialize(data), ...); },
                   object.members());
    }

    template <has_members T> void deserialize(T &object) {
        std::apply([&](auto &...data) { (deserialize(data), ...); },
                   object.members());
    }

    template <typename T> void serialize(const std::vector<T> &vec) {
        auto size = std::size(vec);
        serialize(std::size(vec));

        for (const auto &value : vec) {
            serialize(value);
        }
    }

    template <typename T> void deserialize(std::vector<T> &vec) {
        // clear the vector.
        vec.clear();
        std::size_t size;
        deserialize(size);
        for (int i = 0; i < size; i++) {
            T thing;
            deserialize(thing);
            vec.emplace_back(thing);
        }
    }

    // std::array.
    template <typename T, std::size_t N>
    void serialize(const std::array<T, N> &arr) {
        auto size = N;
        serialize(size);

        for (const auto &value : arr) {
            serialize(value);
        }
    }

    template <typename T, std::size_t N>
    void deserialize(std::array<T, N> &arr) {
        std::size_t size;
        deserialize(size);
        if (size != N) {
            throw std::runtime_error(
                "expected and actual array size do not match");
        }

        for (int i = 0; i < size; i++) {
            T thing;
            deserialize(thing);
            arr[i] = thing;
        }
    }
    // strings are just like vectors.
    void serialize(const std::string &str) {
        std::vector<std::uint8_t> vec(str.begin(), str.end());
        serialize(vec);
    }
    void deserialize(std::string &str) {
        std::vector<std::uint8_t> vec;
        deserialize(vec);
        str.assign(vec.begin(), vec.end());
    }

    // first, for things that are the same size as uint8_t (int8_t, char)
    // no need to byte swap things here.
    template <typename T>
    requires(sizeof(T) == sizeof(std::uint8_t)) void serialize(T const &value) {
        uint8_t tmp;
        std::memcpy(&tmp, &value, sizeof(T));
        data.emplace_back(tmp);
    }

    template <typename T>
    requires(sizeof(T) == sizeof(std::uint8_t)) void deserialize(T &value) {
        auto tmp = data.front();
        std::memcpy(&value, &tmp, sizeof(T));
        data.pop_front();
    }

    // next is for 2 byte things, this uses htons and ntohs	to preserve byte
    // ordering.
    template <typename T>
    requires(sizeof(T) ==
             sizeof(std::uint16_t)) void serialize(T const &value) {
        uint16_t tmp;
        std::memcpy(&tmp, &value, sizeof(T));
        tmp = htobe16(tmp);
        uint8_t bytes[sizeof(T)];
        std::memcpy(&bytes, &tmp, sizeof(T));

        for (int i = 0; i < sizeof(T); i++) {
            data.emplace_back(bytes[i]);
        }
    }

    template <typename T>
    requires(sizeof(T) == sizeof(std::uint16_t)) void deserialize(T &value) {
        uint8_t bytes[sizeof(T)];
        for (int i = 0; i < sizeof(T); i++) {
            bytes[i] = data.front();
            data.pop_front();
        }
        uint16_t tmp;
        std::memcpy(&tmp, bytes, sizeof(T));
        tmp = be16toh(tmp);
        std::memcpy(&value, &tmp, sizeof(T));
    }

    // finally, 32bit things. at this point we write loops to keep things short.
    template <typename T>
    requires(sizeof(T) ==
             sizeof(std::uint32_t)) void serialize(T const &value) {
        uint32_t tmp;
        std::memcpy(&tmp, &value, sizeof(T));
        tmp = htobe32(tmp);
        uint8_t bytes[sizeof(T)];
        std::memcpy(&bytes, &tmp, sizeof(T));

        for (int i = 0; i < sizeof(T); i++) {
            data.emplace_back(bytes[i]);
        }
    }

    template <typename T>
    requires(sizeof(T) == sizeof(std::uint32_t)) void deserialize(T &value) {
        uint8_t bytes[sizeof(T)];
        for (int i = 0; i < sizeof(T); i++) {
            bytes[i] = data.front();
            data.pop_front();
        }
        uint32_t tmp;
        std::memcpy(&tmp, bytes, sizeof(T));
        tmp = be32toh(tmp);
        std::memcpy(&value, &tmp, sizeof(T));
    }

    // and 64 bit things. To do this, we just split it in half and use the 32
    // bit stuff.

    template <typename T>
    requires(sizeof(T) ==
             sizeof(std::uint64_t)) void serialize(T const &value) {
        uint64_t tmp;
        std::memcpy(&tmp, &value, sizeof(T));
        tmp = htobe64(tmp);

        uint8_t bytes[sizeof(T)];
        std::memcpy(bytes, &tmp, sizeof(T));

        for (int i = 0; i < sizeof(T); i++) {
            data.emplace_back(bytes[i]);
        }
    }

    template <typename T>
    requires(sizeof(T) == sizeof(std::uint64_t)) void deserialize(T &value) {
        uint8_t bytes[sizeof(T)];
        for (int i = 0; i < sizeof(T); i++) {
            bytes[i] = data.front();
            data.pop_front();
        }
        uint64_t tmp;
        std::memcpy(&tmp, bytes, sizeof(T));
        tmp = be64toh(tmp);
        std::memcpy(&value, &tmp, sizeof(T));
    }
};

// a simple POC for how this will go down for structs. We can use the
// MAKE_SERIAL macro and the Serializable concept to create a generic function
// that will print all the values to a comma seperated list in an ostream.
template <has_members T>
std::ostream &operator<<(std::ostream &os, const T &obj) {

    std::apply(
        [&os](auto const fst, auto const... rest) {
            os << fst;
	    // fuck it
            ((os << ", " << rest), ...);
        },
        obj.members());
    return os;
}

// just for kicks, here's a dummy object.
struct TestObject {
    int score = 65;
    char a = 'a';
    char letter = 'c';
    int other = 123;
    std::size_t test = 555;
    std::array<char, 2> arr{'h', 'i'};
    std::vector<char> vec{'j', 'k'};
    MAKE_SERIAL(score, a, letter, test, arr, vec)
};

} // namespace surreal

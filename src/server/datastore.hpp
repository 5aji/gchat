// A datastore is an on-disk arbitrary binary storage thing.
#pragma once


#include "surreal/surreal.hpp"
#include "libchat.hpp"
#include <fstream>
#include <sys/stat.h>
#include <iostream>

template<typename T>
class DataStore {

    std::string filename;
public:
    T data;
    DataStore(std::string fname) {
        filename = fname;
    }


    ~DataStore() {
        save();
    }
    void reset() {
        data = T();
        save();
    }

    void save() {

        std::vector<uint8_t> vec = surreal::DataBuf(data);

        std::ofstream f(filename, std::ios::out | std::ios::binary | std::ios::trunc);

        f.write(reinterpret_cast<char*>(vec.data()), vec.size());
    }
    void load() {
        // if file not exist, default initializer and return
        // open the file at the end so that we can use tellg to get file size.
        std::ifstream f(filename, std::ios::in | std::ios::binary | std::ios::ate);

        if (!f.good()) {
            data = T();
            return;
        }

        auto fsize = f.tellg();
        f.seekg(std::ios::beg);

        std::vector<char> vec(fsize);

        f.read(vec.data(),fsize);

        auto buf = surreal::DataBuf(vec.begin(), vec.end());

        buf.deserialize(data);
        
    }
};



struct ServerData {
    // this is a list of username:password that we use to authenticate.
    std::vector<LoginPacket> user_database;


    auto find_user(std::string user) {
        auto lamb = [user](LoginPacket l) {
            return l.username == user;
        };
        return std::find_if(user_database.begin(), user_database.end(), lamb);
    }

    std::vector<MessagePacket> offline_msgs;

    auto get_user_msgs(std::string user) {
        auto lamb = [user](MessagePacket m) {
            return m.destination == user;
        };
        return std::find_if(offline_msgs.begin(), offline_msgs.end(), lamb);
    }
    auto clear_user_msgs(std::string user) {
        auto lamb = [user](MessagePacket m) {
            return m.destination == user;
        };
        return offline_msgs.erase(std::remove_if(offline_msgs.begin(), offline_msgs.end(), lamb),
                offline_msgs.end());
    }
    MAKE_SERIAL(user_database, offline_msgs)
};



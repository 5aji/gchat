
CC:=gcc
CXX:=g++

SRC_DIR := src
LIB_DIR := lib
BIN_DIR := bin
CPPFLAGS+= -MMD -std=c++17 # modern C++ + auto dependency generation.

# we have to add the lib folders to the -I flags.
CPPFLAGS += -I$(LIB_DIR)/

LIB_FILES := $(wildcard $(LIB_DIR)/**/*.cpp)
.PHONY: all clean 

# compile library files.
all: bin/client bin/server
# client files
CLIENT_FILES := $(wildcard $(SRC_DIR)/client/*.cpp)
$(BIN_DIR)/client: $(LIB_FILES:.cpp=.o) $(CLIENT_FILES:.cpp=.o)
	$(CXX) -o $@ $(CPPFLAGS) $^

SERVER_FILES := $(wildcard $(SRC_DIR)/server/*.cpp)
$(BIN_DIR)/server: $(LIB_FILES:.cpp=.o) $(SERVER_FILES:.cpp=.o)
	$(CXX) -o $@ $(CPPFLAGS) $^

ALL_FILES := $(LIB_FILES) $(SERVER_FILES) $(CLIENT_FILES)

clean: $(ALL_FILES:.cpp=.o) $(ALL_FILES:.cpp=.d) $(wildcard $(BIN_DIR)/*)
	rm $^

-include $(ALL_FILES:.cpp=.d)

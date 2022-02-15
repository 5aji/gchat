
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
all: bin/client
# client files
CLIENT_FILES := $(wildcard $(SRC_DIR)/client/*.cpp)
$(BIN_DIR)/client: $(LIB_FILES:.cpp=.o) $(CLIENT_FILES:.cpp=.o)
	$(CXX) -o $@ $(CPPFLAGS) $^

server: 

clean: $(wildcard ./**/*.d) $(wildcard ./**/*.o) $(wildcard $(BIN_DIR)/*)
	rm $^

-include $(LIB_FILES:%.cpp=%.d) $(CLIENT_FILES:%.cpp=%.d)

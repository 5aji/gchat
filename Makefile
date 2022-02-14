
CC:=gcc
CXX:=g++

SRC_DIR := src/
LIB_DIR := lib/
CPPFLAGS+= -MMD -std=c++17 # modern C++ with auto dependency generation.

LIBS := $(SRC_DIR)$(LIB_DIR) # a list of library folders.

# we have to add the lib folders to the -I flags.
CPPFLAGS += $(patsubst %, -I%, $(LIBS))

LIB_FILES := $(wildcard src/lib/*/*.cpp)
.PHONY: server all

# compile library files.
all: client server


# client files
CLIENT_FILES = $(wildcard src/client/*.cpp)
client: $(LIB_FILES:%.cpp=%.o) $(CLIENT_FILES:%.cpp=%.o)
	$(CXX) -o $@ $(CPPFLAGS) $^

server: 

clean: $(LIB_FILES:%.cpp=%.d) $(CLIENT_FILES:%.cpp=%.d) $(LIB_FILES:%.cpp=%.o) $(CLIENT_FILES:%.cpp=%.o)
	rm $^

-include $(LIB_FILES:%.cpp=%.d) $(CLIENT_FILES:%.cpp=%.d)

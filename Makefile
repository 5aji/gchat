
CC:=gcc
CXX:=g++

SRC_DIR := src/
LIB_DIR := lib/
CPPFLAGS+= -MMD -std=c++17 # modern C++ with auto dependency generation.

LIBS := $(SRC_DIR)$(LIB_DIR) # a list of library folders.

# we have to add the lib folders to the -I flags.
CPPFLAGS += $(patsubst %, -I%, $(LIBS))

LIB_FILES := $(wildcard src/lib/*.cpp)
.PHONY: server all

# compile library files.
all: client server


# client files
CLIENT_FILES = $(wildcard src/client/*.cpp)
client: $(LIB_FILES:%.cpp=%.o) $(CLIENT_FILES:%.cpp=%.o)


server: 


-include $(LIB_FILES:%.cpp=%.d)
-include $(CLIENT_FILES:%.cpp=%.d)

GopherChat
===========

CSCI 4211 Project
Written by Saji Champlin. All code is my own.

This repository contains code implementing the GopherChat specification
for CSCI 4211 Spring 2022. GopherChat specifies a single server
and client interface for private/public messages, with support for
anonymous messages, file transfers, users list, and server side monitoring.

This project further extends the requirements with a threading server
with load balancing, offline messages, and simple encryption. It also supports both IPv4
and IPv6.


Struture
============

This project contains three libraries, both written specifically for this project
(they are not libraries I had lying around). The first library is a wrapper
for the linux socket API to make it easier to handle. More details can be found
in the lib/netty folder.

The second library is called `surreal` (like serial, for serialization).
It is an advanced (but jank) serialization/deserialization library
for C++20 that can convert structs, any list, and most data types
to/from binary (not tested on floats). This is used as the backbone
for the third library

The third library uses the surreal library to implement a message framing
system that contains data serialized by `surreal`. This whole packet is
then serialized and sent over the network. There is a custom deserialization
function that reads the message type and calls the correct deserializer
(and allocates the correct sized byte array).


Finally, there are two folders in the source directory, src/client and src/server.
These names are self-explanatory.


Compilation
=============

A Makefile is included to compile both programs. Furthermore, there is a special test function
that runs the testbenches. This is provided for convenience and should not be used in place of
proper TA testing.


Execution
============

Resulting binaries are placed into the bin/ folder, which will
be created if it does not exist.





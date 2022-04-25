GopherChat
===========

CSCI 4211 Project
Written by Saji Champlin. All code is my own.

This repository contains code implementing the GopherChat specification
for CSCI 4211 Spring 2022. GopherChat specifies a single server
and client interface for private/public messages, with support for
anonymous messages, file transfers, users list, and server side monitoring.

This project meets most of the requirements in the specification. It also exceeds some,
and adds some new features.

Improvements:
- arbitrary length usernames, passwords (supports any character), messages, and file sizes.
- offline messages which persist after server restart (saved to disk).
- uncapped user count, including saving to disk.
- authentication of messages. Can't spoof sending a message as a different user.
- extensible framework for adding custom behaviors/messages.


It also has some flaws.

Flaws/Issues:
- clients do not wait for ACK before sending next packet. So LOGIN followed immediately by SEND *will* work,
  (assuming login works), but the message will be anonymous, since the client does not know the status (and
  therefore the username) of the login packet. This is because each login is individually tracked. In other words,
  if I do LOGIN (good) followed instantly by LOGIN (bad), the client will know (!) that the first one succeeded and
  set the username to the first login. This is a nasty edge case that is hard to deal with unless you want to wait
  for each packet to get acked before sending the next one (slow).
- Offline messages currently do not appear different than normal ones. This would be easy to add to the message packet,
  but I have been awake for 29 hours and need to sleep.
- Files are jank. I wasn't sure what exactly "concurrency" meant (I didn't want to have to use multiple sockets).
  Currently the file sending command loads the entire file into the transmit buffer of the client, which will block
  other messages from sending before it is done. This could be fixed by using std::priority_queue and also multiplexing,
  but there's not a good place in the code to multiplex stuff.


Struture
============

This project contains four libraries, both written specifically for this project
(they are not libraries I had lying around). The first library is a wrapper
for the linux socket API to make it easier to handle. More details can be found
in the lib/netty folder. 

The second library is called `surreal` (like serial, for serialization).
It is an advanced (but jank) serialization/deserialization library
for C++20 that can convert structs, vectors/arrays/strings, and most integral data types
to/from binary (only works for systems with IEEE floats). This is used as the backbone
for the libchat library. it uses a macro on structs to serialize struct members, and supports
nested structs.

The third library is an epoll() wrapper and a timerFD wrapper as well as base classes
to implement other wrappers around file descriptors. There was a plan at one point to use
this with signalfd and on-disk files for "real" multiplexed operation, but it got scrapped.

The last library uses the surreal library to implement a message framing
system that contains data serialized by `surreal`. This whole packet is
then serialized and sent over the network. There is a custom deserialization
function that reads the message type and calls the correct deserializer
(and allocates the correct sized byte array). It also contains helper functions
to handle creating these frames, and some common code between server and client.


Finally, there are two folders in the source directory, src/client and src/server.
These names are self-explanatory.


Compilation
=============

THIS PROGRAM ***REQUIRES*** C++20! 

It will *not* compile on anything less than that!
Tested versions of GCC include the latest release, as well as the last in the gcc-10 series.

A Makefile is included to compile both programs.

If you struggle to get a new enough version of GCC for ubuntu 18.04, I think you can use the backports
to get it. There's also the toolchain PPA which has 11 for sure. But you might need to install multiple things
from it (including a new libstdc++)

If you run into issues, please reach out. I can prepare statically linked binaries for use on 18.04.

Execution
============

Resulting binaries are placed into the bin/ folder, which will
be created if it does not exist. they are called according to the specification
in project.pdf.








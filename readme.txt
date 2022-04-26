GopherChat
===========

CSCI 4211 Project
Written by Saji Champlin. All code is my own.

This repository contains code implementing the GopherChat specification
for CSCI 4211 Spring 2022. GopherChat specifies a single server
and client interface for private/public messages, with support for
anonymous messages, file transfers, users listing, and server side monitoring.

This project meets all of the requirements in the specification. It also exceeds some,
and adds offline messaging as an add-on feature. A test is included to demonstrate the offline capability.

NOTE: this program requires C++20 features to compile! Please use GCC 10 or newer.


Features: (part of the spec)
- Account registration (REGISTER saji pass) 
- LOGIN and LOGOUT (LOGIN saji pass; LOGOUT)
- All send types, including anonymous private messages.
- File transfers, including private transfers.
- Online users list.
- Server-side logging of client activity. (missing file size).


Improvements (beyond the spec):
- arbitrary length usernames, passwords (supports any character), messages, and file names/sizes.
- offline messages which persist after server restart (saved to disk) and are sent when a user logs in.
- uncapped user count, including saving credentials to disk.
- authentication of messages. Can't spoof sending a message as a different user with i.e a patched client
- extensible framework for adding custom behaviors/messages.
- Files can be sent from subdirectories (with limitations. It will not create missing subdirectories. Don't rely on it).
- high-speed concurrent file transfers. The transfers do not block sending or receiving of other messages.
	In other words, the transfers are interleaved with normal messages sent after the transfer starts.



It also has some flaws. Some of these are by design, others were not clearly outlined in the spec.

Flaws/Issues:
- users can't login at multiple places.
- server does not know the file size of transferred files. This is not ever calculated nor included in the packets.
  therefore it cannot print the file size. This is because the file streams are kept open until the input file stream 
  gets EOF, which is a bool that is put on the packet. If the receiving side sees the EOF, it knows that it can close the file.
- clients do not wait for ACK before sending next packet. So LOGIN followed immediately by SEND *will* work,
  (assuming login works), but the message will be anonymous, since the client does not know the status (and
  therefore the username) of the login packet. This is because each login is individually tracked. In other words,
  if I do LOGIN (good) followed instantly by LOGIN (bad), the client will know (!) that the first one succeeded and
  set the username to the first login. This is a nasty edge case that is hard to deal with unless you want to wait
  for each packet to get acked before sending the next one (slow).
- Offline messages currently do not appear different than normal ones. This would be easy to add to the message packet,
  but I have been awake for 29 hours and need to sleep.
- There is some strange state invariants (loosely guaranteed behavior). The two main ones are that when files are added
to the transmission job list, it is possible (not easily guaranteed) that the socket transmission queue will disable transmission
and won't turn it back on. This is because EPOLLOUT would otherwise trigger constantly burning cycles and hammering the system.
So there is a send_queue queue which all other tasks push frames to send to. When the queue empties, the socket handler first
runs the file sending jobs (which push more frames to the send_queue). If there are then frames in the queue, it sends them, but
if there aren't, it disables EPOLLOUT.

For the server, it has a similar mechanism, but for all the client facing sockets. This was a problem when a client wanted to send
to another client, because that function did not have access to the Epoll wrapper (or the file descriptor). to get around this,
I use the fact that if we are handling a client, after we finish, we will exit the epoll_wait command and loop in the while(1) loop in main.
Therefore, we can then check every socket and see if it's got stuff in the send queue. if it does, we enable it. This is *slow* and probably
one of the main bottlenecks of the system, since it's O(N) where N is number of clients. Everything else in the server code should be O(1).

While it is possible that either of these loose contracts fail (resulting in deadlocks) I haven't seen it happen. The code just does not make
any strong guarantees.


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
These names are self-explanatory. There's also a quick datastore class in the server
code that uses surreal to save/load data to disk.


Compilation
=============

THIS PROGRAM ***REQUIRES*** C++20! 

It will *not* compile on anything less than that!
Tested versions of GCC include the latest release, as well as the last in the gcc 10 series.

A Makefile is included to compile both programs. Running `make` will build everything. You may want to 
add -j to make it faster (the template metaprogramming increases the compilation times).

If you struggle to get a new enough version of GCC for ubuntu 18.04, I think you can use the backports
to get it. There's also the toolchain PPA which has 11 for sure. But you might need to install multiple things
from it (including a new libstdc++)

If you run into issues, please reach out. I can prepare statically linked binaries for use on 18.04. I can also
try and compile it on an Ubuntu 18.04 VM to create steps to compile it.

Execution
============

Resulting binaries are placed into the bin/ folder, which will
be created if it does not exist. They are called according to the specification
in project.pdf.



Notes
===========

This code is a mess. There is a ton of duplicated code between client and server, but I didn't have time to make it common since
a lot of it was deeply integrated into surrounding code. There are multiple layers of packet framing which could probably be compacted.
There are enough imports to make me sad. A lot of these are not needed and are definitely violating good import practice.
There's multiple loosely coupled behaviors. Things are named strangely. I had to hack in a side channel to the EPoll 
wrapper so I could modify things by their file descriptor directly, which is bad design. There is usage of lambdas
where there should instead be functions mapped with std::bind or something since they got so big.

Here's some things that might make it easier to understand.

- Both programs use a send/receive queue model. Sending is easy. The receive buffer code is the mess you see in the lambdas
  in main(). It is a FSM that reads custom "headers" directly (rather than frames, which is made redundant). the header contains
  a magic 0xFE and then 8 bytes of the packet size (in bytes). Then the receiving side waits for that many bytes to come in before
  deserializing the frame and then running either serverHandler (on the client side) or clientHandler (on the server side).
- the client/serverHandler functions respond to packets received by the server/client respectively. For the client, this is mostly printing,
  but it also handles file writing and responses to messages in the ack_queue. After some messages are sent, they are pushed to the ack_queue
  to later be tracked. This is how the client knows what login was actually accepted. File packets are not acked. everything else is.

  For the server, this manages the ClientSession, which is a struct of session state for each connection. There is a map of client
  sessions that can be indexed by file descriptor, but also by username. The username -> session map is managed with logout/login,
  and the fd->session map is managed by the socket handler functions. It can also access other sessions by username, and the global datastore as well.
  Importantly, the fd and username maps use shared_ptr, so they update each other.

- The client also contains a command parser function that uses an ifstream to read the command file and create frames that are pushed onto the send queue.
  It is called by a timer_fd which is used to implement delays. Basically, the timerfd callback is fired when the timer expires. It calls the parseFile
  function which executes one command and returns an int. IF the int is -1, the timer callback runs it again. This is what most commands use. For DELAY,
  it returns a non-negative integer which is then used to set the timer. After running, the timer callback checks the send_queue and enables EPOLLOUT events
  if it has elements.
- The sending and receiving of files uses a list of  FileJob structs to send and a mapping of filenames to ofstreams to receive. When a client receives an
  xfer packet from the server it looks at the filename and tries to get the ofstream (if it doesn't exist, it creates the ofstream and adds it to the map).
  Then it writes the contents of the FilePacket.data to the file, and checks FilePacket.eof to know if it should close the file (and print a message).
  To send, a filejob is created that contains an ifstream, as well as parameters to be set in the outgoing filepackets. When the send queue is empty,
  the socket handler will run all the filejobs that are active, which populates the send queue. If it is still empty after running, then it disables
  the EPOLLOUT event. Creating the filejob enables the EPOLLOUT event, but this is a weakly-coupled behavior.

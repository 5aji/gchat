GopherChat
===========

CSCI 4211 Project
Written by Saji Champlin. All code is my own.

This repository contains code implementing the GopherChat specification
for CSCI 4211 Spring 2022. GopherChat specifies a single server
and client interface for private/public messages, with support for
anonymous messages, file transfers, users list, and server side monitoring.

This project further extends the requirements with blah blah blah.



Struture
============

This project contains two libraries, both written specifically for this project
(they are not libraries I had lying around). The first library is a wrapper
for the linux socket API to make it easier to handle. More details can be found
in the lib/netty folder.

The second library specifies the data structures used for the GopherChat protocol.
Since the document doesn't have any requirements for the underlying protocol,
this library contains structs and helper functions to facilitate communication
between server and client.


Finally, there are two folders in the source directory, src/client and src/server.
These names are self-explanatory.


Compilation
=============

A Makefile is included to compile both programs. Furthermore, there is a special test function
that runs the testbenches. This is provided for convenience and should not be used in place of
proper TA testing.





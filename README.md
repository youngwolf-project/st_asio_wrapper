st_asio_wrapper
===============

Asynchronous c/s network framework; Based on Boost.Asio; Very efficient.

The include folder, which I call it normal edition or standard edition relative to compatible edition
contains all the library's codes;
All other folders except compatible_edition are demos:

asio_server:
Demonstrate how to implement tcp servers, it cantains two servers, one is the simplest server,
which just send data from keyboard, and receive data from socket;
one is echo server, which send every received data back.

asio_client:
Demonstrate how to implement tcp client, it simply send data from keyboard to the server.

test_client:
Used to test the server(echo server)'s performance.

file_server file_client:
A file transfer service. At client, use 'get <file name1> [file name2] [...]' to fetch files.

udp_client:
Demonstrate how to implement upd communication.

Compiler requirement:
Normal edition need vc2010 or higher, or gcc4.6 or higher;
Compatible edition does not have the limition as normal edition does, only need you to compile boost successfully.

Boost requirement:
1.49 or highter, early edition maybe work, but I'm not sure.

Debug environment:
Win7 64bit
Ubuntu 12.10 32bit
Ubuntu 12.10 64bit
Fedora 18 64bit

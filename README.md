st_asio_wrapper
===============

An asynchronous c/s framework based on Boost.Asio.
Based on message just like UDP with several couple of build-in packer and unpacker.
Support replacing packer and unpacker at run-time.
Automatically reconnect to the server after link broken.
Widely support timers.
Support object pool, object reuse.
Support message buffer.
Support ssl.
Support TCP/UDP.

Directory compatible_edition has the same structure as st_asio_wrapper does, which is so called compatible edition, because it doesn't used any C++0x feature, so it can be compiled under old compilers which don't support C++0x.

All source codes are placed in directory include, other directories hold demos:

asio_server:
Demonstrate how to implement tcp servers, it cantains two servers, one is the simplest server, which just send characters from keyboard to all asio_clients, and receive messages from all asio_clients (then display them), one is echo server, which send every received message from test_clients back.

asio_client:
Demonstrate how to implement tcp client, it simply send characters from keyboard to asio_server, and receive messages from asio_server (then display them).

test_client:
Used to test echo server's performance.

file_server file_client:
A file transfer service. At client, use 'get <file name1> [file name2] [...]' to fetch files.

udp_client:
Demonstrate how to implement UDP communication.

Compiler requirement:
Normal edition need vc2010 or higher, or gcc4.6 or higher;
Compatible edition does not have the limition as normal edition does, only need you to compile boost successfully.

Boost requirement:
1.49 or highter, early edition maybe can work, but I'm not sure.

Debug environment:
Win8 vc2008/vc2015 32/64bit
Debian 7.x 32/64bit
Fedora 23 64bit

email: mail2tao@163.com
Community on QQ: 198941541

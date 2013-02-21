st_asio_wrapper
===============

Asynchronous c/s network framework; Based on Boost.Asio; Very efficient.

The include folder(I call it normal edition) contains all the library's codes;

asio_server is a demo, which demonstrate how to implement tcp server using st_asio_wrapper, it cantains two servers,
one is the simplist server, which just send data from keyboard, and receive data from socket; one is echo server,
which send every received data back.

asio_client is a demo, which demonstrate how to implement tcp client using st_asio_wrapper, it simply send data
from keyboard to the server.

file_server is a demo, which supply file transfer service.
file_client is a demo, which supply file transfer service; use 'get <file name>' to fetch files.

test_client is a demo, which is used to test the server's performance(need to connect to the echo server).

udp_client is a demo, which demonstrate how to implement upd communication.

Compiler requirement:
Normal edition need vc2010 or higher, and gcc4.6 or higher;
Compatible edition does not have the limition of normal edition, only need you compile boost successfully.

Boost requirement:
1.49 or highter, early edition maybe work, but I'm not certain.

Debug environment:
Win7 64bit
Ubuntu 12.10 32bit
Ubuntu 12.10 64bit
Fedora 18 64bit


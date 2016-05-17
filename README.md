st_asio_wrapper
===============
Overview
-
st_asio_wrapper is an asynchronous c/s framework based on Boost.Asio, besides all benefits brought by Boost and Boost.Asio, it also contain: </br>
1. Based on message just like UDP with several couple of build-in packer and unpacker;</br>
2. Support packer and unpacker customization, and replacing packer and unpacker at run-time;</br>
3. Automatically reconnect to the server after link broken;</br>
4. Widely support timers;</br>
5. Support object pool, object reusing;</br>
6. Support message buffer;</br>
7. Support ssl;</br>
8. Support TCP/UDP.</br>
Quick start:
-
###server:
Derive your own socket from `st_server_socket`, you must at least re-write the `on_msg` or `on_msg_handle` virtual function and handle messages in it;</br>
Create a `st_service_pump` object, create a `st_server` object, call `st_service_pump::start_service`;</br>
Call `st_server_socket::send_msg` when you have messages need to send.</br>
###client:
Derive your own socket from `st_connector`, you must at least re-write the `on_msg` or `on_msg_handle` virtual function and handle messages in it;</br>
Create a `st_service_pump` object, create a `st_tcp_client` object, set server address via `st_connector::set_server_addr`, call `st_service_pump::start_service`;</br>
Call `st_connector::send_msg` when you have messages need to send.</br>
Directory structure:
-
Directory `compatible_edition` has the same structure as `st_asio_wrapper`, it is so called compatible edition, because it doesn't used any C++0x features, so it can be compiled by old compilers which don't support C++0x.</br>
All source codes are placed in directory `include`, other directories hold demos (except `compatible_edition`).</br>
Demos:
-
###asio_server:
Demonstrate how to implement tcp servers, it cantains two servers, one is the simplest server (normal server), which just send characters from keyboard to all `asio_clients`, and receive messages from all `asio_clients` (then display them); the other is echo server, which send every received message from `test_clients` back.</br>
###asio_client:
Demonstrate how to implement tcp client, it simply send characters from keyboard to `asio_server`, and receive messages from `asio_server` (then display them).</br>
###test_client:
Used to test the performance of `echo server`.</br>
###file_server:
A file transfer server.</br>
###file_client:
A file transfer client, use `get <file name1> [file name2] [...]` to fetch files from `file_server`.</br>
###udp_client:
Demonstrate how to implement UDP communication.</br>
###ssl_test:
Demonstrate how to implement TCP communication with ssl.</br>
Compiler requirement:
-
Normal edition need Visual C++ 10.0, GCC 4.6 or Clang 3.1 at least;</br>
Compatible edition does not have any limitations, just need you to compile boost successfully.</br>
Boost requirement:
-
1.49 or highter, earlier edition maybe can work, but I'm not sure.</br>
Debug environment:
-
win10 vc2008/vc2015 32/64 bit</br>
Win7 vc2010 32/64 bit</br>
Debian 7/8 32/64 bit</br>
Ubuntu 16 64 bit</br>
Fedora 23 64 bit</br>
FreeBSD 10 32/64 bit</br>
email: mail2tao@163.com
-
Community on QQ: 198941541
-

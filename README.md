st_asio_wrapper
===============
Overview
-
st_asio_wrapper is an asynchronous c/s framework based on Boost.Asio, besides all benefits brought by Boost and Boost.Asio, it also contains: </br>
1. Based on message just like UDP with several couple of build-in packer and unpacker;</br>
2. Support packer and unpacker customization, and replacing packer and unpacker at run-time;</br>
3. Automatically reconnect to the server after link broken;</br>
4. Support object pool, object reusing and restoration;</br>
5. Worker thread management;</br>
6. Support message buffer;</br>
7. Widely support timers;</br>
8. Support TCP/UDP;</br>
9. Support ssl;</br>

Quick start:
-
### server:
Derive your own socket from `server_socket_base`, you must at least re-write the `on_msg` or `on_msg_handle` virtual function and handle messages in it;</br>
Create a `service_pump` object, create a `server_base<your_socket>` object, call `service_pump::start_service`;</br>
Call `server_socket_base::send_msg` when you have messages need to send.</br>
### client:
Derive your own socket from `client_socket_base`, you must at least re-write the `on_msg` or `on_msg_handle` virtual function and handle messages in it;</br>
Create a `service_pump` object, create a `multi_client_base<your_socket>` object, add some socket via `multi_client_base::add_socket`, call `service_pump::start_service`;</br>
Call `client_socket_base::send_msg` when you have messages need to send.</br>

Directory structure:
-
All source codes are placed in directory `include`, other directories hold demos, documents are placed in directory `doc`.</br>

Demos:
-
### echo_server:
Demonstrate how to implement tcp servers, it cantains two servers, one is the simplest server (normal server), which just send characters from keyboard to all clients (from `client` demo), and receive messages from all clients (from `client` demo), then display them; the other is echo server, which send every received message from `echo_client` demo back.</br>
### client:
Demonstrate how to implement tcp client, it simply send characters from keyboard to normal server in `echo_server`, and receive messages from normal server in `echo_server`, then display them.</br>
### echo_client:
Used to test `st_asio_wrapper`'s performance (whith `echo server`).</br>
### file_server:
A file transfer server.</br>
### file_client:
A file transfer client, use `get <file name1> [file name2] [...]` to fetch files from `file_server`.</br>
### udp_client:
Demonstrate how to implement UDP communication.</br>
### ssl_test:
Demonstrate how to implement TCP communication with ssl.</br>

Compiler requirement:
-
No special limitations, just need you to compile boost successfully.</br>

Boost requirement:
-
1.49 or highter, earlier edition maybe can work, but I'm not sure.</br>

email: mail2tao@163.com
-
Community on QQ: 198941541
-

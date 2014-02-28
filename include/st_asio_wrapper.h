/*
 * st_asio_wrapper_verification.h
 *
 *  Created on: 2012-10-21
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this head file contain some macros used to verify the compiler and update logs etc.

 * license: www.boost.org/LICENSE_1_0.txt
 *
 * versions and updates:
 * 1.0	2012.7.7
 * first release
 *
 * 1.1	2012.7.13
 * support custom msg unpacking, that is, st_asio_wrapper server with third party client,
 * or st_asio_wrapper client with third party server. if both endpoint are st_asio_wrapper,
 * please don't use this feature(may reduce efficiency).
 *
 * 1.2	2012.7.18
 * st_server support change listen address, and, of course, you need re-start_service
 * add set_server_addr for this purpose
 *
 * fix bug: after re-start_service, some member variables of st_socket not been reseted
 * add reset_unpacker_state for this purpose
 *
 * add a helper function list_all_client to show all the clients in server endpoint
 *
 * 1.3	2012.7.23
 * support custom msg packing, this is a patch to 1.1 version
 * In 1.1, I only add support to custom msg unpacking, and forgot to add support to custom msg packing
 * I'm sorry for that
 *
 * add helper function send_native_smg to support native msg, which st_asio_wrapper will not pack
 * it if you use the default packer
 *
 * 1.4	2012.7.27
 * strip custom msg packing and unpacking from st_socket to keep them conciseness
 *
 * optimize pop_all_pending_msg so that it can work efficiently in none c++0x standard environment, which
 * has no move constructor semantics
 *
 * now, if you need to handle custom msg, please inherit i_packer and i_unpacker to implement your own
 * packer and unpacker, then, re-write create_packer and create_unpacker to return them,
 * also, you can change packer and unpacker at runtime via inner_packer and inner_unpacker
 *
 * rename on_parse_error to on_unpack_error
 *
 * 1.5	2012.8.2
 * Add ipv6 support, at both endpoint, use set_server_addr with an ipv6 address to open this feature
 * Empty address will cause st_asio_wrapper to choose ipv4; A valid port is necessary in any case
 *
 * Add some helper functions
 *
 * 1.6	2012.8.7
 * Add msg recv buffer
 *
 * Change the virtual function on_msg's semantics, it return a boolean value to indicate whether
 * to use recv buffer. msgs in recv buffer will be dispatched asynchronously in on_msg_handle()
 *
 * change some internal variable's names
 *
 * 1.7	2012.8.14
 * change bind to boost::bind to resolve the compile error in vc
 * fix bug: previous msgs may be overwritten by next msg fragment in default unpacker
 * change i_unpacker interface
 * fix bug: default show_client in st_server may throw exception
 * fix bug: unified_out's internal buffer may be overwritten because of multi-thread call
 * rename client_base to st_socket
 * rename server_base to st_server
 * rename cclient to st_client
 * rename sclient to server_socket and move it to st_server class
 * rename st_asio_wrapper_client_base.h to st_asio_wrapper_socket.h
 * rename st_asio_wrapper_server_base.h to st_asio_wrapper_server.h
 * rename st_asio_wrapper_cclient.h to st_asio_wrapper_client.h
 * drop st_asio_wrapper_sclient.h(move it's content to st_asio_wrapper_server.h)
 *
 * 1.8	2012.8.21
 * Increase the robustness
 * Add is_listening() member function to st_server, which is used to check the server's listening status
 * Graceful stop service: wait all async call finish
 * Clean the clients list at server endpoint automatically and periodically for memory saving
 * Decompose on_socket_close to on_recv_error and on_send_error
 *
 * 1.9	2012.8.28
 * Use array instead of char[] in default unpacker for more safety
 * Use placeholders instead of _1 and _2 for more compatibility
 * Support more than one async_accept delivery concurrently, see ASYNC_ACCEPT_NUM macro in st_server class
 * BUG fix: use async_write instead of tcp::socket::async_send, which may cause data sending incomplete
 *
 * 2.0	2012.9.2
 * Add a separated timer to auto invoke clear_all_closed_socket()(move the closed clients to
 * st_server::temp_client_can), use AUTO_CLEAR_CLOSED_SOCKET to open this feature and
 *
 * CLEAR_CLOSED_SOCKET_INTERVAL to set the interval
 *
 * Add REUSE_CLIENT macro to open client objects pool feature
 *
 * If REUSE_CLIENT defined, st_server will not free the closed clients(in st_server::temp_client_can)
 * automatically and periodically, but try to reuse them when need new client object. In this case,
 * the client number in st_server::temp_client_can may keeps very large if no or few client objects
 * will be consumed after a certain duration. So, I provide free_client() to free a specific number of
 * client objects for memory saving
 *
 * If REUSE_CLIENT not defined, st_server will free the closed clients(in st_server::temp_client_can)
 * automatically and periodically, and use CLIENT_FREE_INTERVAL to set the interval.
 * In this case, clients in temp_client_can will not be reused but be freed directly
 * (at the specific interval), and there's no free_client() function because not needed
 *
 * 2.1 2012.9.9
 * Interface signature standardization
 * Add virtual on_connect() to st_client, which will be invoked immediately after the connection establish
 * Not send the ending '\0' character in st_socket::send_msg() and st_socket::send_native_msg() any more
 * Not send the ending '\0' character in st_server::broadcast_msg() and st_server::broadcast_native_msg() any more
 *
 * Add virtual function on_all_msg_send() to st_socket, which will be invoked when the send buffer
 * become empty(edge-triggered)
 *
 * Add performance test project, see performance_test folder
 * Fix st_client bug: after stop_server(), st_client may still try to re-connect to the server
 * Change the default behavior of on_msg_handle() and on_msg(), see the comments of them for more details
 *
 * Drop create_packer() and create_unpacker() virtual function because call them in constructor
 * can not reach the purpose of be rewritten, now, if you want to use custom packer and unpacker,
 * you must use inner_packer() and inner_unpacker() to set the custom packer and unpacker at runtime,
 * see echo_server in asio_server.
 *
 * Drop on_recv_buffer_overflow(), now st_socket can guarantee working aright when the recv buffer overflow
 *
 * Add a parameter can_overflow to send_msg, send_vative_msg, broadcast_msg and broadcast_native_msg to
 * indicate weather can ignore the send buffer limitation, this is useful when need send msgs but can't
 * block to wait the send buffer become available, for example, in on_msg()
 *
 * 2.2	2012.9.17
 * Add timer support to st_asio_wrapper, see st_asio_wrapper::st_timer for more details
 *
 * Timers are already used in st_socket, st_client and st_server, please pay attention to the
 * reserved timers, which are listed in st_asio_wrapper_timer.h, and do not use them.
 *
 * Use set_timer() to start a timer, and stop_timer() to stop a timer.
 * When invoke set_timer(), if the specific timer id is already exist, st_timer will update the timer and restart it.
 *
 * When timers run out, the virtual function on_timer() will be invoked, you must handle all interested
 * timers and invoke father's on_timer() for the rests
 *
 * In the same object, different timers(different id) will concurrently invoke on_timer(), relatively,
 * same timer will invoke sequentially.
 *
 * In different objects, all timers will concurrently invoke on_timer().
 *
 * Demo with timer please refer to file_server, an application based on st_asio_wrapper for
 * file transfer and simple talk.
 *
 * Add FORCE_TO_USE_MSG_RECV_BUFFER macro to force to use the msg recv buffer at compile time for
 * performance improvement, see FORCE_TO_USE_MSG_RECV_BUFFER for more details
 *
 * 2.3	2012.10.7
 * Drop overuse of shared_ptr on msgs
 * Add vc2010 support and vc compiler version verification
 * Strip connection logic from st_client to st_connector
 *
 * Add a test framework st_test_client to client endpoint for server pressure test, and a demo
 * test_client base on it.
 *
 * Fix bug: std::advance can't move the iterator to the head direction in this context. by the way,
 * there's no problem under linux
 *
 * 2.4	2012.10.19
 * Use decltype to reduce input.
 * Use macro to reduce input and make code more tidy.
 *
 * Use uint64_t data type replacing size_t to avoid overflow under 32bit system in
 * performance/asio_client and performance/test_client, this is actually a small bug but is
 * none of st_asio_wrapper's business, it's the demo's.
 *
 * Change the behaviour of st_test_client::random_send_msg() and st_test_client::random_send_vative_msg():
 * not set seed for rand() any more, it's the user's responsibility to invoke srand() if necessarily.
 *
 * Change the return type of st_test_client::get_recv_bytes() from size_t to uint64_t to avoid overflow
 * under 32bit system.
 *
 * Strip update logs, compiler verification, etc from st_asio_wrapper_socket.h to
 * st_asio_wrapper_verfication.h.
 *
 * Add WANT_MSG_SEND_NOTIFY macro to decide weather to open msg send notification(on_msg_send) or not,
 * this is a compile time optimization
 *
 * Add WANT_ALL_MSG_SEND_NOTIFY macro to decide weather to open all msg send notification(on_all_msg_send)
 * or not, this is a compile time optimization
 *
 * 2.5	2012.11.3
 * Fix warning: under 32bit gcc, use %lu(d) to print size_t raise warnings.
 *
 * Fix warning: under 32bit gcc, use %ld(d) to print uint_64_t raise warnings
 * (personal behavior of performance/asio_client and performance/test_client).
 *
 * Fix warning: under 32bit gcc, use %ld(d) to print __off64_t raise warnings
 * (personal behavior of file_client).
 *
 * Add vc2008 support, which means not require c++0x any more, and I call it compatible edition against
 * normal edition. In order to achieve high efficiency(as std::move does), I use shared_ptr to avoid memory
 * copys, so, the interface has been changed; And, in order to keep the conciseness of normal edition,
 * I strip it from normal edition and put it into compatible_edition folder.
 *
 * Fix bug: container::set does not have front() member function(personal behavior of vc2010).
 *
 * 2.6	2012.11.19
 * Unify some interfaces of compatible edition's st_socket to normal edition, please do remember to change
 * your code according to the following virtual function's signature(been changed), include the classes
 * that indirectly inherit from st_socket:
 * st_socket::on_msg
 * st_socket::on_msg_handle
 * st_socket::on_msg_send
 * st_socket::on_all_msg_send
 *
 * 2.7	2012.12.5
 * Add udp support, st_udp_socket encapsulate data sending and receiving, st_udp_client is the service.
 * The following virtual function's signature have been changed for more compact, if you have rewrote some of
 * these virtual functions(include indirect inheritance, for example, server_socket, test_socket), do not forget
 * to change them. I'm sorry for these changes:
 * st_socket::on_msg
 * st_socket::on_msg_handle
 * st_socket::on_msg_send
 * st_socket::on_all_msg_send
 * st_udp_socket::on_msg
 * st_udp_socket::on_msg_handle
 * st_udp_socket::on_msg_send
 * st_udp_socket::on_all_msg_send
 *
 * Add NOT_REUSE_ADDRESS macro to decide weather to reuse address or not.
 *
 * 2.7	2012.12.12
 * Use boost::bind and boost::mem_fn to simplify the codes, now, the normal and compatible edition
 * looks more similar than before.
 *
 * Add graceful closing socket.
 * Fix st_timer's potential bug.
 *
 * 2.8	2012.12.23
 * Fix graceful closing bug, it doesn't actually wait to receive all remaining data
 * Delete stopping member variable from st_client and st_test_client
 * Change virtual function is_ok() to is_send_allowed() for more readable
 *
 * 2.9	2013.1.21
 * Fix st_udp_client bug: call send_msg() before service startup will cause the msg been buffered until next send_msg().
 * Add multi-link support to st_client and st_udp_client, like test_client.
 * Drop demo performance_test/asio_client.
 * Drop test_client class(replaced by st_client).
 * Change some member variable's access right from private to protect for more convenient to inherit.
 *
 * 3.0	2013.1.27
 * Add support for sharing io_service and threads between services(st_server, st_client, st_udp_client)
 *
 * Strip shared_ptr from msgs in compatible edition, now, the compatible edition can achieve almost the same
 * performance as normal edition does.
 *
 * 3.1	2013.2.23
 * Move multi_service's logic to service_pump for code simplification and error probability reduction.
 * Drop the multi_service class.
 *
 * 3.2	2013.3.26
 * Unify i_packer interface between standard and compatible edition.
 *
 * Add a new function run_service to st_service_pump, which works like io_service::run,
 * so, it will block until service run out, and do not call stop_service any more.
 *
 * Add a virtual callback function on_exception to st_service_pump, it will be invoked when exception caught,
 * you can rewrite it do decide weather to continue(return true) or stop the service(return false),
 * notice: you need to define ENHANCED_STABILITY macro to gain this feature.
 *
 * 3.3	2013.4.13
 * Add template support to st_sudp_client(now is st_sudp_client_base), st_udp_client(now is st_udp_client_base),
 * st_sclient(now is st_sclient_base), st_client(now is st_client_base) and st_server(now is st_server_base).
 *
 * Now, st_sudp_client is a typedef of st_sudp_client_base<st_udp_socket>, st_udp_client is a typedef of
 * st_udp_client_base<st_udp_socket>, st_sclient is a typedef of st_sclient_base<st_connector>, st_client is a
 * typedef of st_client_base<st_connector> and st_server is a typedef of st_server_base<st_server_socket>
 *
 * Change the class server_socket to st_server_socket, and bring it out of st_server.
 * Add an interface i_server, which is implemented by st_server_base, and used by st_server_socket.
 *
 * In st_server_socket::on_recv_error, if the AUTO_CLEAR_CLOSED_SOCKET macro been defined, then simply force_close
 * itself instead of call i_server::del_client.
 *
 * Notice: If you encounter compile errors such as can't convert from A to A& with boost before 1.53 or gcc before 4.7,
 * please wrapper A with boost::ref
 *
 * 3.4	2013.7.11
 * Add reconnecting support to st_connector class: use true to invoke force_close or graceful_close.
 * Other small changes please refer to svn or git logs.
 *
 * 3.5	2013.7.28
 * Code refactoring.
 *
 * Add safe_send_msg and safe_broadcast_msg, they can guarantee send msg successfully just like use can_overflow
 * equal to true to invoke send_msg or broadcast_msg, except that when the send buffer is full, they will wait until
 * the send buffer becomes available.
 *
 * Add support for suspending msg dispatching in runtime.
 * Change class st_socket to st_tcp_socket.
 * Change class st_sclient to st_tcp_sclient.
 * Change class st_client to st_tcp_client.
 * Change class st_sudp_client to st_udp_sclient.
 *
 * Add class st_client_base which is the father of st_tcp_client and st_udp_client, and provide the common logic
 * of the two classes.
 *
 * Add class st_socket which is the father of st_tcp_socket and st_udp_socket, and provide the common logic
 * of the two classes.
 *
 * Correct some spelling mistake and impertinent spelling, include both code(for example, class name) and comment.
 *
 * 3.6	2013.8.11
 * Change macro REUSE_CLIENT to REUSE_OBJECT, concept remains unchanged.
 * Change macro MAX_CLIENT_NUM to MAX_OBJECT_NUM, concept remains unchanged.
 * Add support for suspending msg sending in runtime.
 * Abstract the logic about object pool from st_server_base to st_object_pool.
 * Add new class st_object_pool.
 * Apply the object pool logic to class st_client(inherit from st_object_pool).
 * Drop reuse(), use reset() instead.
 * Change macro CLIENT_FREE_INTERVAL to SOCKET_FREE_INTERVAL, concept remains unchanged.
 * Change macro INVALID_LINK_MAX_DURATION to CLOSED_SOCKET_MAX_DURATION, concept remains unchanged.
 *
 * Change the name of the member variables and functions which named *client* to *object* in st_object_pool,
 * concept remains unchanged.
 *
 * 3.7	2013.8.20
 * Protected some member functions to avoid abusing them.
 * Change code to avoid compiler crash before vc2012, gcc doesn't have this problem.
 * Forbid using some classes(eg st_timer) directly.
 * Correct some error comments in st_asio_wrapper_timer.h.
 *
 * Forbid invoding st_socket::start() repeatedly, this is always forbidden, but it's the user's responsiblity
 * to avoid this before this release.
 *
 * Move the logic of suspending sending msg to st_socket for code conciseness, to approach this, add two pure virtual
 * functions: do_start and do_send_msg to st_socket.
 *
 * Fix bug: Invoking suspend_dispatch_msg() in on_msg cause dead lock.
 * Fix bug: st_socket may stop continue recv msg in certain situations.
 * Fix bug: Invode st_server::del_client may cause mutex problem on recv buffer.
 * Add some helper function to get the recv buffer size, peek the recv buffer, etc.
 *
 * 3.8	2013.9.15
 * Fix bug: Invoking safe_send_msg in on_msg may cause dead lock, on_msg_handle also has this problem when all
 * service threads are blocked in on_msg_handle(call safe_send_msg)
 *
 * Add post_msg to resolve above issue, you are highly recommended to use these functions in on_msg and
 * on_msg_handle functions, for others, you are recommended to use send_msg or safe_send_msg instead.
 *
 * Make start_service sync.
 *
 * Add support for adding service to st_service_pump at runtime.
 *
 * 3.9	2014.3.9
 * Add support for asio.ssl.
 */

#ifndef ST_ASIO_WRAPPER_H_
#define ST_ASIO_WRAPPER_H_

#define ST_ASIO_WRAPPER_VERSION 30900

#if !defined _MSC_VER && !defined __GNUC__
#error st_asio_wrapper only support vc and gcc.
#endif

#if defined _MSC_VER && _MSC_VER < 1600
#error st_asio_wrapper must be compiled with vc2010 or higher.
#endif

//After a roughly reading from gcc.gnu.org, I guess that the minimum version of gcc that support c++0x
//is 4.6, so, I supply the following compiler verification. If there's something wrong, you can freely
//modify them, and if you let me know, I'll be very appreciated.
#if defined __GNUC__ && (__GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ < 6)
#error st_asio_wrapper must be compiled with gcc4.6 or higher.
#endif

#endif /* ST_ASIO_WRAPPER_H_ */

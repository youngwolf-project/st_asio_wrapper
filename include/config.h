/*
 * config.h
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
 * Known issues:
 * 1. since 1.2.0, before boost-1.55, compatible edition(now is st_asio_wrapper)'s st_object(now is tracked_executor)::is_last_async_call() cannot work properly,
 *    this is because before asio calling any callbacks, it copied the callback(not a good behavior), this causes is_last_async_call() never return true,
 *    so objects in st_object_pool can never be reused or freed. To fix this issue, we must not define ST_ASIO_ENHANCED_STABILITY macro(for now, you should define
 *    macro ST_ASIO_DELAY_CLOSE to a value that bigger than zero).
 * 2. since 1.3.5 until 1.4, heartbeat function cannot work properly between windows (at least win-10) and Ubuntu (at least Ubuntu-16.04).
 * 3. since 1.3.5 until 1.4, UDP doesn't support heartbeat because UDP doesn't support OOB data.
 * 4. since 1.3.5 until 1.4, SSL doesn't support heartbeat because SSL doesn't support OOB data.
 * 5. with old openssl (at least 0.9.7), ssl::client_socket_base and ssl_server_socket_base are not reusable, I'm not sure in which version,
 *    they became available, seems it's 1.0.0.
 * 6. since 1.0.0 until 2.1.0, async_write and async_read are not mutexed on the same socket, which is a violation of asio threading model.
 *
 * change log:
 * 2012.7.7
 * Created
 *
 * 2012.7.7 - 2016.7.7
 * Beta edition
 *
 * 2016.7.7		version 1.0.0
 * First release
 *
 * 2016.7.17	version 1.0.1
 * Support boost-1.49, it's the minimum version for st_asio_wrapper because of boost::container.
 *
 * 2016.8.7		version 1.1.0
 * Add stream_unpacker to receive native data.
 * asio_server and test_client are now able to start with configurable thread number.
 * Add efficiency statistic for performance tuning, and it can be shut down.
 * Add pingpong test.
 * Move packers and unpackers out of 'include' directory (now in 'ext' directory), they don't belong to st_asio_wrapper library.
 *
 * 2016.8.16	version 1.1.1
 * Fix bug: the last msg will not be re-dispatched if on_msg_handle returned false until new msgs come in.
 * Drop timer TIMER_SUSPEND_DISPATCH_MSG, it's useless.
 * Send more msgs in one async_write call.
 * Fix annotations.
 *
 * 2016.9.4		version 1.1.2
 * Renamed proxy_buffer to auto_buffer.
 * Added a new replaceable buffer shared_buffer.
 * Fix bug: if receive buffer overflow and on_msg() returns false (which means using receive buffer),
 *  st_socket will call on_msg() again and again (asynchronously) for the same msg until receive buffer becomes available.
 *
 * 2016.9.13	version 1.1.3
 * Support optional timers (deadline_timer, steady_timer and system_timer).
 * Split ext/st_asio_wrapper_net.h into ext/st_asio_wrapper_client.h, ext/st_asio_wrapper_server.h and ext/st_asio_wrapper_udp.h.
 * Add virtual function st_server_base::on_accept_error, it controls whether to continue the acceptance or not when error occurred.
 *
 * 2016.9.22	version 1.2.0
 * Add st_socket::on_close() virtual function, if you defined ST_ASIO_ENHANCED_STABILITY macro, st_asio_wrapper guarantee this is the
 *  last invocation on this st_socket, you can free any resources that belong to this st_socket, except this st_socket itself, because
 *  this st_socket may is being maintained by st_object_pool, in other words, you will never free an object which is maintained by shared_ptr.
 * Add st_socket::is_closable() virtual function, st_connector need it when the link is broken and reconnecting is taking place, under this
 *  situation, we will never reuse or free this st_connector.
 * Add st_object::is_last_async_call() function, it provide 100% safety to reuse or free an object (need to define ST_ASIO_ENHANCED_STABILITY macro).
 * Rename all force_close functions to force_shutdown;
 * Rename all graceful_close functions to graceful_shutdown;
 * Rename all do_close functions to shutdown;
 * Rename close_all_client function to shutdown_all_client.
 * Rename ST_ASIO_GRACEFUL_CLOSE_MAX_DURATION macro to ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION.
 * Rename ST_ASIO_OBSOLETED_OBJECT_LIFE_TIME macro to ST_ASIO_DELAY_CLOSE.
 * Change default value of ST_ASIO_FREE_OBJECT_INTERVAL macro from 10 to 60.
 * Drop const qualifier for is_send_allowed() function.
 * Strip boost::bind and boost::ref for standard edition.
 *
 * 2016.10.8	version 1.3.0
 * Drop original congestion control (because it cannot totally resolve dead loop) and add a semi-automatic congestion control.
 * Demonstrate how to use the new semi-automatic congestion control (asio_server, test_client, pingpong_server and pingpong_client).
 * Drop post_msg_buffer and corresponding functions (like post_msg()) and timer (st_socket::TIMER_HANDLE_POST_BUFFER).
 * Optimize locks on message sending and dispatching.
 * Add enum shutdown_states.
 * st_timer now can be used independently.
 * Add a new type st_timer::tid to represent timer ID.
 * Add a new packer--fixed_length_packer.
 * Add a new class--message_queue.
 *
 * 2016.10.16	version 1.3.1
 * Support non-lock queue, it's totally not thread safe and lock-free, it can improve IO throughput with particular business.
 * Demonstrate how and when to use non-lock queue as the input and output message buffer.
 * Queues (and their internal containers) used as input and output message buffer are now configurable (by macros or template arguments).
 * New macros--ST_ASIO_INPUT_QUEUE, ST_ASIO_INPUT_CONTAINER, ST_ASIO_OUTPUT_QUEUE and ST_ASIO_OUTPUT_CONTAINER.
 * In contrast to non_lock_queue, rename message_queue to lock_queue.
 * Move container related classes and functions from st_asio_wrapper_base.h to st_asio_wrapper_container.h.
 * Improve efficiency in scenarios of low throughput like pingpong test.
 * Move class statistic and obj_with_begin_time out of st_socket to reduce template tiers.
 *
 * 2016.11.13	version 1.3.2
 * Use ST_ASIO_DELAY_CLOSE instead of ST_ASIO_ENHANCED_STABILITY macro to control delay close duration,
 *  0 is an equivalent of defining ST_ASIO_ENHANCED_STABILITY, other values keep the same meanings as before.
 * Move st_socket::closing related logic to st_object.
 * Make st_socket::id(boost::uint_fast64_t) private to avoid changing IDs by users.
 * Call close at the end of shutdown function, just for safety.
 * Introduce lock-free mechanism for some appropriate logics (many requesters, only one can succeed, others will fail rather than wait).
 * Remove all mutex (except mutex in st_object_pool, st_service_pump, lock_queue and st_udp_socket).
 * Sharply simplified st_timer class.
 *
 * 2016.11.20	version 1.3.3
 * Simplify header files' dependence.
 * Yield to Visual C++ 10.0
 * Add Visual C++ solution and project files (Visual C++ 11.0 and 9.0).
 *
 * 2016.12.6	version 1.3.4
 * Compatible edition support c++0x (11, 14, 17, ...) features too.
 * Monitor time consumptions for message packing and unpacking.
 * Fix bug: pop_first_pending_send_msg and pop_first_pending_recv_msg cannot compile.
 *
 * 2017.1.1		version 1.3.5
 * Support heartbeat (via OOB data), see ST_ASIO_HEARTBEAT_INTERVAL macro for more details.
 * Support scatter-gather buffers when receiving messages, this feature needs modification of i_unpacker, you must explicitly define
 *  ST_ASIO_SCATTERED_RECV_BUFFER macro to open it, this is just for compatibility.
 * Simplify lock-free mechanism.
 * Optimize container insertion (use series of emplace functions instead).
 * Demo test_client support alterable number of sending thread (before, it's a hard code 16).
 * Fix bug: In extreme cases, messages may get starved in receive buffer and will not be dispatched until arrival of next message.
 * Fix bug: In extreme cases, messages may get starved in send buffer and will not be sent until arrival of next message.
 * Fix bug: Sometimes, st_connector_base cannot reconnect to the server after link broken.
 *
 * ===============================================================
 * 2017.5.30	version 1.4.0
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Virtual function reset_state in i_packer, i_unpacker and i_udp_unpacker have been renamed to reset.
 * Virtual function is_send_allowed has been renamed to is_ready, it also means ready to receive messages
 *  since message sending cannot be suspended any more.
 * Virtual function on_msg_handle has been changed, the link_down variable will not be presented any more.
 * Interface i_server::del_client has been renamed to i_server::del_socket.
 * Function inner_packer and inner_unpacker have been renamed to packer and unpacker.
 * All add_client functions have been renamed to add_socket.
 *
 * HIGHLIGHT:
 * Support object restoration (on server side), see macro ST_ASIO_RESTORE_OBJECT for more details.
 * Re-implement heartbeat function, use user data (so need packer and unpacker's support) instead of OOB, now there will be no any
 *  limitations for using heartbeat.
 * Refactor and optimize ssl objects, now st_ssl_connector_base and st_ssl_server_socket_base are reusable,
 *  just need you to define macro ST_ASIO_REUSE_SSL_STREAM.
 *
 * FIX:
 * Before on_close() to be called, st_socket::start becomes available (so user can call it falsely).
 * If a timer failed or stopped by callback, its status not set properly (should be set to TIMER_CANCELED).
 * Make ssl shutting down thread safe.
 * Make reconnecting after all async invocations (like object reusing or restoration).
 *
 * ENHANCEMENTS:
 * Virtual function i_packer::pack_heartbeat been introduced to support heartbeat function.
 * Interface i_server::restore_socket been added, see macro ST_ASIO_RESTORE_OBJECT for more details.
 * Be able to manually start heartbeat function without defining macro ST_ASIO_HEARTBEAT_INTERVAL, see demo asio_server for more details.
 * Support sync mode when sending messages, it's also a type of congestion control like safe_send_msg.
 * Expand enum st_tcp_socket::shutdown_states, now it's able to represent all SOCKET status (connected, shutting down and broken),
 *  so rename it to link_status.
 * Enhance class timer (function is_timer and another stop_all_timer been added).
 * Support all buffer types that boost::asio supported when receiving messages, use macro ST_ASIO_RECV_BUFFER_TYPE (it's the only way) to define the buffer type,
 *  it's effective for both TCP and UDP.
 *
 * DELETION:
 * Drop st_server_base::shutdown_all_client, use st_server_base::force_shutdown instead.
 * Drop ST_ASIO_DISCARD_MSG_WHEN_LINK_DOWN macro and related logic, because it brings complexity and race condition,
 *  and are not very useful.
 * Drop socket::is_closable, st_connector_base will overwrite socket::on_close to implement reconnecting mechanism.
 * Not support pausing message sending and dispatching any more, because they bring complexity and race condition,
 *  and are not very useful.
 *
 * REFACTORING:
 * Move heartbeat function from st_connector_base and st_server_socket_base to st_socket, so introduce new virtual function
 *  virtual void on_heartbeat_error() to st_socket, subclass need to implement it.
 * Move async shutdown function from st_connector_base and st_server_socket_base to st_socket, so introduce new virtual function
 *  virtual void on_async_shutdown_error() to st_socket, subclass need to implement it.
 * Move handshake from st_ssl_server_base to st_ssl_server_socket_base.
 *
 * REPLACEMENTS:
 * Use boost::mutex instead of boost::shared_mutex, the former is more efficient in st_asio_wrapper's usage scenario.
 * Move macro ST_ASIO_SCATTERED_RECV_BUFFER from st_asio_wrapper to st_asio_wrapper::ext, because it doesn't belong to st_asio_wrapper any more after introduced
 *  macro ST_ASIO_RECV_BUFFER_TYPE.
 * Move macro ST_ASIO_MSG_BUFFER_SIZE from st_asio_wrapper to st_asio_wrapper::ext, because it doesn't belong to st_asio_wrapper.
 * Use boost::unordered::unordered_map instead of unordered_set, because we used one of the overloaded function find, which is marked as 'not encouraged to use'.
 *
 * ===============================================================
 * 2017.6.19	version 1.4.1
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 *
 * HIGHLIGHT:
 *
 * FIX:
 * Fix race condition on member variable last_send_msg in st_tcp_socket_base.
 *
 * ENHANCEMENTS:
 * Optimize reconnecting mechanism.
 * Enhance class st_timer.
 *
 * DELETION:
 *
 * REFACTORING:
 *
 * REPLACEMENTS:
 * Rename st_connector_base and st_ssl_connector_base to st_client_socket_base and st_ssl_client_socket_base, the former is still available, but is just an alias.
 *
 * ===============================================================
 * 2017.7.9		version 2.0.0
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * No error_code will be presented anymore when call io_context::run, suggest to define macro ST_ASIO_ENHANCED_STABILITY.
 * i_unpacker has been moved to namespace tcp, i_udp_unpacker has been moved to namespace udp and renamed to i_unpacker.
 *
 * HIGHLIGHT:
 * Add two demos for concurrent test.
 * Support unstripped message (take the default unpacker for example, it will not strip message header in parse_msg), this feature is disabled by default,
 *  you can call i_unpacker's void stripped(bool) function to enable it.
 *  udp::i_unpacker doesn't have this feature, it always and only support unstripped message.
 *
 * FIX:
 * Fix reconnecting mechanism in demo ssl_test.
 *
 * ENHANCEMENTS:
 * Truly support asio 1.11 (don't use deprecated functions and classes any more), and of course, asio 1.10 will be supported too.
 *
 * DELETION:
 * Drop standard edition, use ascs (https://github.com/youngwolf-project/ascs) instead, it doesn't depend boost, but can work with boost.
 *
 * REFACTORING:
 * Rename the name of classes and header files according to ascs, relocate header files according to ascs, macros been kept.
 *
 * REPLACEMENTS:
 * Use mutable_buffer and const_buffer instead of mutable_buffers_1 and const_buffers_1 if possible, this can gain some performance improvement.
 * Call force_shutdown instead of graceful_shutdown in tcp::client_base::uninit().
 *
 * ===============================================================
 * 2017.7.23	version 2.0.1
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * i_server has been moved from st_asio_wrapper to st_asio_wrapper::tcp.
 *
 * HIGHLIGHT:
 * Support decreasing (increasing already supported) the number of service thread at runtime by defining ST_ASIO_DECREASE_THREAD_AT_RUNTIME macro,
 *  suggest to define ST_ASIO_AVOID_AUTO_STOP_SERVICE macro too.
 *
 * FIX:
 * Always directly shutdown ssl::client_socket_base if macro ST_ASIO_REUSE_SSL_STREAM been defined.
 * Make queue::clear and swap thread-safe if possible.
 *
 * ENHANCEMENTS:
 * Optimized and simplified auto_buffer, shared_buffer and ext::basic_buffer.
 * Optimized class obj_with_begin_time.
 * Not use sending buffer (send_msg_buffer) if possible.
 * Reduced stopped() invocation (because it needs locks).
 * Introduced boost::asio::io_service::work (boost::asio::executor_work_guard) by defining ST_ASIO_AVOID_AUTO_STOP_SERVICE macro.
 * Add function service_pump::service_thread_num to fetch the real number of service thread (must define ST_ASIO_DECREASE_THREAD_AT_RUNTIME macro).
 *
 * DELETION:
 *
 * REFACTORING:
 * Move all deprecated classes (connector_base, client_base, service_base) to alias.h
 * Refactor the mechanism of message sending.
 *
 * REPLACEMENTS:
 * Rename tcp::client_base to tcp::multi_client_base, ext::tcp::client to ext::tcp::multi_client, udp::service_base to udp::multi_service_base,
 *  ext::udp::service to ext::udp::multi_service. Old ones are still available, but have became alias.
 *
 * ===============================================================
 * 2017.9.17	version 2.0.2
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Function object_pool::invalid_object_pop only pop obsoleted objects with no additional reference.
 * socket::stat.last_recv_time will not be updated before tcp::socket_base::on_connect any more.
 * For ssl socket, on_handshake will be invoked before on_connect (before, on_connect is before on_handshake).
 *
 * HIGHLIGHT:
 *
 * FIX:
 * If start the same timer and return false in the timer's call_back, its status will be set to TIMER_CANCELED (the right value should be TIMER_OK).
 * If call stop_service after service_pump stopped, timer TIMER_DELAY_CLOSE will be left behind and be triggered after the next start_service,
 *  this will bring disorders to st_asio_wrapper::socket.
 *
 * ENHANCEMENTS:
 * During congestion controlling, retry interval can be changed at runtime, you can use this feature for performance tuning,
 *  see macro ST_ASIO_MSG_HANDLING_INTERVAL_STEP1 and ST_ASIO_MSG_HANDLING_INTERVAL_STEP2 for more details.
 * Avoid decreasing the number of service thread to less than one.
 * Add a helper function object_pool::get_statistic.
 * Add another overload of function object_pool::invalid_object_pop.
 * Introduce asio::defer to object, be careful to use it.
 * Add link's break time and establish time to the statistic object.
 * Move virtual function client_socket_base::on_connect to tcp::socket_base, so server_socket_base will have it too (and ssl sockets).
 *
 * DELETION:
 * Drop useless variables which need macro ST_ASIO_DECREASE_THREAD_AT_RUNTIME to be defined.
 *
 * REFACTORING:
 * Move variable last_send_time and last_recv_time from st_asio_wrapper::socket to st_asio_wrapper::socket::stat (a statistic object).
 * Move common operations in client_socket_base::do_start and server_socket_base::do_start to tcp::socket_base::do_start and socket::do_start.
 *
 * REPLACEMENTS:
 * Always use io_context instead of io_service (before asio 1.11, io_context will be a typedef of io_service).
 *
 * ===============================================================
 * 2017.9.28	version 2.0.3
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 *
 * HIGHLIGHT:
 *
 * FIX:
 * Support unmovable buffers (for example: a very short std::string).
 * Eliminate race condition in udp::socket_base.
 * Eliminate race condition in demo file_client.
 * Avoid division by zero error in demo file_client.
 *
 * ENHANCEMENTS:
 *
 * DELETION:
 *
 * REFACTORING:
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2017.12.25	version 2.0.4
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 *
 * HIGHLIGHT:
 * Support boost-1.66
 *
 * FIX:
 * Fix race condition when controlling the number of service thread at runtime.
 * Fix compiler warnings.
 *
 * ENHANCEMENTS:
 * Add verification for object allocation.
 * Add log if stop_timer failed.
 * Hide variable server in server_socket_base, use get_server function instead.
 * A boolean value instead of void will be returned from set_timer and update_timer_info.
 *
 * DELETION:
 *
 * REFACTORING:
 * Move function show_info from server_socket_base and client_socket_base to tcp::show_info.
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2018.4.18	version 2.0.5
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Do reconnecting in client_socket_base::after_close rather than in client_socket_base::on_close.
 *
 * HIGHLIGHT:
 *
 * FIX:
 * Reconnecting may happen in st_asio_wrapper::socket::reset, it's not a right behavior.
 *
 * ENHANCEMENTS:
 * Add st_asio_wrapper::socket::after_close virtual function, a good case for using it is to reconnect the server in client_socket_base.
 *
 * DELETION:
 *
 * REFACTORING:
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2018.5.20	version 2.1.0
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Not support sync sending mode any more.
 * Explicitly need macro ST_ASIO_PASSIVE_RECV to gain the ability of changing the unpacker at runtime.
 * Function disconnect, force_shutdown and graceful_shutdown in udp::socket_base will now be performed asynchronously.
 * Not support macro ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER any more, which means now we have the behavior as this macro always defined,
 *  thus, virtual function st_asio_wrapper::socket::on_msg() is useless and also has been deleted.
 * statistic.handle_time_2_sum has been renamed to handle_time_sum.
 * Macro ST_ASIO_MSG_HANDLING_INTERVAL_STEP1 has been renamed to ST_ASIO_MSG_RESUMING_INTERVAL.
 * Macro ST_ASIO_MSG_HANDLING_INTERVAL_STEP2 has been renamed to ST_ASIO_MSG_HANDLING_INTERVAL.
 * st_asio_wrapper::socket::is_sending_msg() has been renamed to is_sending().
 * st_asio_wrapper::socket::is_dispatching_msg() has been renamed to is_dispatching().
 * typedef st_asio_wrapper::socket::in_container_type has been renamed to in_queue_type.
 * typedef st_asio_wrapper::socket::out_container_type has been renamed to out_queue_type.
 * Wipe default value for parameter can_overflow in function send_msg, send_native_msg, safe_send_msg, safe_send_native_msg,
 *  broadcast_msg, broadcast_native_msg, safe_broadcast_msg and safe_broadcast_native_msg, this is because we added template parameter to some of them,
 *  and the compiler will complain (ambiguity) if we omit the can_overflow parameter. So take send_msg function for example, if you omitted can_overflow
 *  before, then in 2.1, you must supplement it, like send_msg(...) -> send_msg(..., false).
 *
 * HIGHLIGHT:
 * After introduced boost::asio::io_context::strand (which is required, see FIX section for more details), we wiped two atomic in st_asio_wrapper::socket.
 * Introduced macro ST_ASIO_DISPATCH_BATCH_MSG, then all messages will be dispatched via on_handle_msg with a variable-length container.
 *
 * FIX:
 * Wiped race condition between async_read and async_write on the same st_asio_wrapper::socket, so sync sending mode will not be supported any more.
 *
 * ENHANCEMENTS:
 * Explicitly define macro ST_ASIO_PASSIVE_RECV to gain the ability of changing the unpacker at runtime.
 * Add function st_asio_wrapper::socket::is_reading() if macro ST_ASIO_PASSIVE_RECV been defined, otherwise, the socket will always be reading.
 * Add function st_asio_wrapper::socket::is_recv_buffer_available(), you can use it before calling recv_msg() to avoid receiving buffer overflow.
 * Add typedef st_asio_wrapper::socket::in_container_type to represent the container type used by in_queue_type (sending buffer).
 * Add typedef st_asio_wrapper::socket::out_container_type to represent the container type used by out_queue_type (receiving buffer).
 * Generalize function send_msg, send_native_msg, safe_send_msg, safe_send_native_msg, broadcast_msg, broadcast_native_msg,
 *  safe_broadcast_msg and safe_broadcast_native_msg.
 *
 * DELETION:
 * Deleted macro ST_ASIO_SEND_BUFFER_TYPE.
 * Deleted virtual function bool st_asio_wrapper::socket::on_msg().
 * Not support sync sending mode any more, so we reduced an atomic object in st_asio_wrapper::socket.
 *
 * REFACTORING:
 * If you want to change unpacker at runtime, first, you must define macro ST_ASIO_PASSIVE_RECV, second, you must call st_asio_wrapper::socket::recv_msg and
 *  guarantee only zero or one recv_msg invocation (include initiating and asynchronous operation, this may need mutex, please carefully design your logic),
 *  see file_client for more details.
 * Class object has been split into executor and tracked_executor, object_pool use the former, and st_asio_wrapper::socket use the latter.
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2018.8.2		version 2.1.1
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * The data type of timer ID has been changed from unsigned char to unsigned short.
 *
 * HIGHLIGHT:
 * Dynamically allocate timers when needed (multi-threading related behaviors kept as before, so we must introduce a mutex for st_asio_wrapper::timer object).
 *
 * FIX:
 *
 * ENHANCEMENTS:
 * The range of timer ID has been expanded from [0, 256) to [0, 65536).
 * Add new macro ST_ASIO_ALIGNED_TIMER to align timers.
 *
 * DELETION:
 *
 * REFACTORING:
 * Realigned member variables for st_asio_wrapper::socket to save a few memory.
 * Make demos more easier to use.
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2018.8.22	version 2.1.2
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * If macro ST_ASIO_PASSIVE_RECV been defined, you may receive empty messages in on_msg() or on_msg_handle() and sync_recv_msg(), this makes you always having
 *  the chance to call recv_msg().
 * i_unpacker has been moved from namespace st_asio_wrapper::tcp and st_asio_wrapper::udp to namespace st_asio_wrapper, and the signature of
 *  st_asio_wrapper::udp::i_unpacker::parse_msg has been changed to obey st_asio_wrapper::tcp::i_unpacker::parse_msg.
 *
 * HIGHLIGHT:
 * Fully support sync message sending and receiving (even be able to mix with async message sending and receiving without any limitations), but please note
 *  that this feature will slightly impact efficiency even if you always use async message sending and receiving, so only open this feature when really needed.
 *
 * FIX:
 * Fix race condition when aligning timers, see macro ST_ASIO_ALIGNED_TIMER for more details.
 * Fix error check for UDP on cygwin and mingw.
 * Fix bug: demo file_client may not be able to receive all content of the file it required if you get more than one file in a single request.
 *
 * ENHANCEMENTS:
 * Add new macro ST_ASIO_SYNC_SEND and ST_ASIO_SYNC_RECV to support sync message sending and receiving.
 *
 * DELETION:
 *
 * REFACTORING:
 * i_unpacker has been moved from namespace st_asio_wrapper::tcp and st_asio_wrapper::udp to namespace st_asio_wrapper, and the signature of
 *  st_asio_wrapper::udp::i_unpacker::parse_msg has been changed to obey st_asio_wrapper::tcp::i_unpacker::parse_msg, the purpose of this change is to make
 *  socket::sync_recv_msg() can be easily implemented, otherwise, sync_recv_msg() must be implemented by tcp::socket_base and udp::socket_base respectively.
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2018.10.1	version 2.1.3
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * With sync message sending, if you rewrite virtual function on_close, you must also call super class' on_close.
 *
 * HIGHLIGHT:
 * Support sync message dispatching, it's like previous on_msg() callback but with a message container instead of a message (and many other
 *  differences, see macro ST_ASIO_SYNC_DISPATCH for more details), and we also name it on_msg().
 * Support timed waiting when doing sync message sending and receiving.
 * Support pre-creating server sockets in server_base (before accepting connections), this is very useful if creating server socket is very expensive.
 *
 * FIX:
 * Fix spurious awakenings when doing sync message sending and receiving.
 * Fix statistics for batch message dispatching.
 *
 * ENHANCEMENTS:
 * Add virtual function find_socket to interface i_server.
 * Support timed waiting when doing sync message sending and receiving, please note that after timeout, sync operations can succeed in the future because
 *  st_asio_wrapper uses async operations to simulate sync operations. for sync receiving, missed messages can be dispatched via the next sync receiving,
 *  on_msg (if macro ST_ASIO_SYNC_DISPATCH been defined) and / or on_msg_handle.
 * Add virtual function server_socket_base::async_accept_num() to support pre-creating server sockets at runtime, by default, it returns ST_ASIO_ASYNC_ACCEPT_NUM,
 *  see demo concurrent_server and macro ST_ASIO_ASYNC_ACCEPT_NUM for more details.
 *
 * DELETION:
 *
 * REFACTORING:
 * Hide member variables as many as possible for developers.
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2019.1.1		version 2.1.4
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * The virtual function socket::on_send_error has been moved to tcp::socket_base and udp::socket_base.
 * The signature of virtual function socket::on_send_error has been changed, a container holding messages that were failed to send will be provided.
 * Failure of binding or listening in server_base will not stop the service_pump any more.
 * Virtual function client_socket_base::prepare_reconnect() now only control the retry times and delay time after reconnecting failed.
 *
 * HIGHLIGHT:
 *
 * FIX:
 * If give up connecting (prepare_reconnect returns -1 or call close_reconnect), st_asio_wrapper::socket::started() still returns true (should be false).
 *
 * ENHANCEMENTS:
 * Expose server_base's acceptor via next_layer().
 * Prefix suffix packer and unpacker support heartbeat.
 * New demo socket_management demonstrates how to manage sockets if you use other keys rather than the original id.
 * Control reconnecting more flexibly, see function client_socket_base::open_reconnect and client_socket_base::close_reconnect for more details.
 * client_socket_base support binding to a specific local address.
 *
 * DELETION:
 *
 * REFACTORING:
 *
 * REPLACEMENTS:
 *
 * ===============================================================
 * 2019.3.3		version 2.2.0
 *
 * SPECIAL ATTENTION (incompatible with old editions):
 * Socket used by tcp::multi_client_base, ssl::multi_client_base and udp::multi_socket_service needs to provide a constructor which accept
 *  a reference of i_matrix instead of a reference of asio::io_context.
 * Limit send and recv buffers by actual utilization (in byte) rather than message number before, so macro ST_ASIO_MAX_MSG_NUM been renamed to
 *  ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF, and unit been changed to byte.
 * statistic.send_msg_sum may be bigger than before (but statistic.send_byte_sum will be the same), see ENHANCEMENTS section for more details.
 * Use st_asio_wrapper::list instead of boost::container::list, the former guarantee that emplace_back() function always return the reference of
 *  the newly added item.
 * Make function tcp::socket_base::reset to be virtual.
 * Rename replaceable_unpacker to unpacker2, replaceable_udp_unpacker to udp_unpacker2, replaceable_packer to packer2, because their names confuse
 *  users, any packer or unpacker is replaceable for those packer or unpacker that has the same msg_type.
 *
 * HIGHLIGHT:
 * Make client_socket_base be able to call multi_client_base (via i_matrix) like server_socket_base call server_base (via i_server),
 *  and so does ssl::client_socket_base and udp::socket_base.
 * Promote performance by reducing memory replications if you already generated the message body and it can be swapped into st_asio_wrapper.
 * Introduce shared_mutex, it can promote performance if you find or traverse (via do_something_to_all or do_something_to_one) objects frequently.
 *
 * FIX:
 * Disable warnings C4521 and C4521 for Visual C++.
 *
 * ENHANCEMENTS:
 * Introduce macro ST_ASIO_RECONNECT to control the default switch of reconnecting mechanism.
 * Introduce macro ST_ASIO_HIDE_WARNINGS to hide compilation warnings generated by st_asio_wrapper.
 * Introduce macro ST_ASIO_SHARED_MUTEX_TYPE and ST_ASIO_SHARED_LOCK_TYPE, they're used during searching or traversing (via do_something_to_all or
 *  do_something_to_one) objects in object_pool, if you search or traverse objects frequently, use shared_mutex and shared_lock instead of mutex
 *  and unique_lock will promote performance, otherwise, do not define these two macros (so they will be mutex and unique_lock by default).
 * Introduce three additional overloads of pack_msg virtual function to i_packer, they accept one or more than one i_packer::msg_type (not packed) that belong to
 *  the same message.
 * Add three overloads to send_(native_)msg, safe_send_(native_)msg, sync_send_(native_)msg and sync_safe_send_(native_)msg respectively (just on TCP),
 *  they accept one or more than one rvalue reference of in_msg_type, this will reduce one memory replication, and the statistic.send_msg_sum will be one bigger
 *  than before because the packer will add an additional message just represent the header to avoid copying the message bodies.
 * Control send and recv buffer accurately rather than just message number before, see macro ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF for more details.
 * direct_send_msg and direct_sync_send_msg support batch operation.
 * Introduce virtual function type_name() and type_id() to st_asio_wrapper::socket, they can identify whether a given two st_asio_wrapper::socket has the same type.
 * force_shutdown and graceful_shutdown support reconnecting even if the link has broken.
 * Extract function start_listen from current implementations.
 * Support concurrency hint for io_context if possible.
 * Demonstrate how to accept just one client at server endpoint in demo echo_server.
 *
 * DELETION:
 *
 * REFACTORING:
 * Unify all container to st_asio_wrapper::list (before, some of them were boost::container::list).
 *
 * REPLACEMENTS:
 *
 */

#ifndef ST_ASIO_CONFIG_H_
#define ST_ASIO_CONFIG_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#define ST_ASIO_VER		20200	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ST_ASIO_VERSION	"2.2.0"

//#define ST_ASIO_HIDE_WARNINGS

//boost and compiler check
#ifdef _MSC_VER
	#define ST_ASIO_SF "%Iu" //format used to print 'size_t'
	#define ST_ASIO_LLF "%I64u" //format used to print 'boost::uint_fast64_t'

	#if _MSC_VER < 1700
		#define ST_THIS //workaround to make up the BOOST_AUTO's defect on vc2008 and compiler crush before vc2012
	#else
		#define ST_THIS this->
		#if !defined(ST_ASIO_HIDE_WARNINGS) && _MSC_VER >= 1800
			#pragma message("Your compiler is Visual C++ 12.0 (2013) or higher, you can use ascs to gain some performance improvement.")
		#endif
	#endif
#elif defined(__GNUC__)
	#define ST_ASIO_SF "%zu" //format used to print 'size_t'
	#define ST_THIS this->
	#ifdef __x86_64__
		#define ST_ASIO_LLF "%lu" //format used to print 'boost::uint_fast64_t'
	#else
		#define ST_ASIO_LLF "%llu" //format used to print 'boost::uint_fast64_t'
	#endif
	#ifdef __clang__
		#if !defined(ST_ASIO_HIDE_WARNINGS) && (__clang_major__ > 3 || __clang_major__ == 3 && __clang_minor__ >= 1)
			#warning Your compiler is Clang 3.1 or higher, you can use ascs to gain some performance improvement.
		#endif
	#elif !defined(ST_ASIO_HIDE_WARNINGS) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6)
		#warning Your compiler is GCC 4.6 or higher, you can use ascs to gain some performance improvement.
	#endif

	#if !defined(ST_ASIO_HIDE_WARNINGS) && (defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(__cplusplus) && __cplusplus >= 201103L)
		#warning st_asio_wrapper does not need any c++11 features.
	#endif
#else
	#error st_asio_wrapper only support Visual C++, GCC and Clang.
#endif

#if BOOST_VERSION < 104900
	#error st_asio_wrapper only support boost 1.49 or higher.
#elif !defined(BOOST_THREAD_USES_CHRONO)
	namespace boost {namespace this_thread {static inline void sleep_for(const chrono::milliseconds& rel_time) {sleep(posix_time::milliseconds(rel_time.count()));}}}
#endif

#if BOOST_ASIO_VERSION < 101100
namespace boost {namespace asio {typedef io_service io_context;}}
#define make_strand_handler(S, F) S.wrap(F)
#else
#define make_strand_handler(S, F) boost::asio::bind_executor(S, F)
#endif
//boost and compiler check

//configurations

#ifndef ST_ASIO_SERVER_IP
#define ST_ASIO_SERVER_IP			"127.0.0.1"
#endif
#ifndef ST_ASIO_SERVER_PORT
#define ST_ASIO_SERVER_PORT			5051
#elif ST_ASIO_SERVER_PORT <= 0
	#error server port must be bigger than zero.
#endif

#ifdef ST_ASIO_MAX_MSG_NUM
	#ifdef _MSC_VER
		#pragma message("macro ST_ASIO_MAX_MSG_NUM is deprecated, use ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF instead, the meanings also changed too, please note.")
	#else
		#warning macro ST_ASIO_MAX_MSG_NUM is deprecated, use ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF instead, the meanings also changed too, please note.
	#endif
#endif

//send buffer's maximum size (bytes), it will be expanded dynamically (not fixed) within this range.
#ifndef ST_ASIO_MAX_SEND_BUF
#define ST_ASIO_MAX_SEND_BUF		1048576 //1M
#elif ST_ASIO_MAX_SEND_BUF <= 15
	#error message capacity must be bigger than zero.
#endif

//recv buffer's maximum size (bytes), it will be expanded dynamically (not fixed) within this range.
#ifndef ST_ASIO_MAX_RECV_BUF
#define ST_ASIO_MAX_RECV_BUF		1048576 //1M
#elif ST_ASIO_MAX_RECV_BUF <= 15
	#error message capacity must be bigger than zero.
#endif

//buffer (on stack) size used when writing logs.
#ifndef ST_ASIO_UNIFIED_OUT_BUF_NUM
#define ST_ASIO_UNIFIED_OUT_BUF_NUM	2048
#endif

//use customized log system (you must provide unified_out::fatal_out/error_out/warning_out/info_out/debug_out)
//#define ST_ASIO_CUSTOM_LOG

//don't write any logs.
//#define ST_ASIO_NO_UNIFIED_OUT

//if defined, service_pump will catch exceptions for boost::asio::io_context::run().
//#define ST_ASIO_ENHANCED_STABILITY

//if defined, boost::asio::steady_timer will be used in st_asio_wrapper::timer.
//#define ST_ASIO_USE_STEADY_TIMER
//if defined, boost::asio::system_timer will be used in st_asio_wrapper::timer.
//#define ST_ASIO_USE_SYSTEM_TIMER
//otherwise, boost::asio::deadline_timer will be used

//after this duration, this socket can be freed from the heap or reused,
//you must define this macro as a value, not just define it, the value means the duration, unit is second.
//a value equal to zero will cause st_asio_wrapper to use a mechanism to guarantee 100% safety when reusing or freeing this socket,
//st_asio_wrapper will hook all async calls to avoid this socket to be reused or freed before all async calls finish
//or been interrupted (of course, this mechanism will slightly impact efficiency).
#ifndef ST_ASIO_DELAY_CLOSE
	#if BOOST_VERSION < 105500
	#define ST_ASIO_DELAY_CLOSE	5 //seconds, cannot guarantee 100% safety when reusing or freeing this socket (asio 1.10.1 fixed this issue)
	#else
	#define ST_ASIO_DELAY_CLOSE	0 //seconds, guarantee 100% safety when reusing or freeing this socket
	#endif
#elif ST_ASIO_DELAY_CLOSE < 0
	#error "delay close duration must be bigger than or equal to zero."
#elif BOOST_VERSION < 105500 && 0 == ST_ASIO_DELAY_CLOSE
	#error "before boost-1.55, macro ST_ASIO_DELAY_CLOSE must be bigger than zero, so no 100% safety will be guaranteed when reusing or freeing this socket."
#endif

//full statistic include time consumption, or only numerable informations will be gathered
//#define ST_ASIO_FULL_STATISTIC

//after every msg sent, call st_asio_wrapper::socket::on_msg_send()
//#define ST_ASIO_WANT_MSG_SEND_NOTIFY

//after sending buffer became empty, call st_asio_wrapper::socket::on_all_msg_send()
//#define ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY

//max number of objects object_pool can hold.
#ifndef ST_ASIO_MAX_OBJECT_NUM
#define ST_ASIO_MAX_OBJECT_NUM	4096
#elif ST_ASIO_MAX_OBJECT_NUM <= 0
	#error object capacity must be bigger than zero.
#endif

//if defined, objects will never be freed, but remain in object_pool waiting for reuse.
//#define ST_ASIO_REUSE_OBJECT

//this macro has the same effects as macro ST_ASIO_REUSE_OBJECT (it will overwrite the latter), except:
//reuse will not happen when create new connections, but just happen when invoke i_server::restore_socket.
//you may ask, for what purpose we introduced this feature?
//consider following situation:
//if a specific link is down, and the client has reconnected to the server, on the server side, how does the new server_socket_base
//restore all user data (because you don't want to nor need to reestablish them) and keep its id?
//before this feature been introduced, it's almost impossible.
//according to above explanation, we know that:
//1. like object pool, only objects in invalid_object_can can be restored;
//2. client need to inform server_socket_base the former id (or something else which can be used to calculate the former id
//   on the server side) after reconnected to the server;
//3. this feature needs user's support (send former id to server side on client side, invoke i_server::restore_socket in server_socket_base);
//4. do not define this macro on client side nor for UDP.
//#define ST_ASIO_RESTORE_OBJECT

//define ST_ASIO_REUSE_OBJECT or ST_ASIO_RESTORE_OBJECT macro will enable object pool, all objects in invalid_object_can will
// never be freed, but kept for reuse, otherwise, object_pool will free objects in invalid_object_can automatically and periodically,
//ST_ASIO_FREE_OBJECT_INTERVAL means the interval, unit is second, see invalid_object_can in object_pool class for more details.
#if !defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
	#ifndef ST_ASIO_FREE_OBJECT_INTERVAL
	#define ST_ASIO_FREE_OBJECT_INTERVAL	60 //seconds
	#elif ST_ASIO_FREE_OBJECT_INTERVAL <= 0
		#error free object interval must be bigger than zero.
	#endif
#endif

//define ST_ASIO_CLEAR_OBJECT_INTERVAL macro to let object_pool to invoke clear_obsoleted_object() automatically and periodically
//this feature may affect performance with huge number of objects, so re-write server_socket_base::on_recv_error and invoke object_pool::del_object()
//is recommended for long-term connection system, but for short-term connection system, you are recommended to open this feature.
//you must define this macro as a value, not just define it, the value means the interval, unit is second
//#define ST_ASIO_CLEAR_OBJECT_INTERVAL		60 //seconds
#if defined(ST_ASIO_CLEAR_OBJECT_INTERVAL) && ST_ASIO_CLEAR_OBJECT_INTERVAL <= 0
	#error clear object interval must be bigger than zero.
#endif

//IO thread number
//listening, msg sending and receiving, msg handling (on_msg() and on_msg_handle()), all timers (include user timers) and other asynchronous calls (from executor)
//keep big enough, no empirical value I can suggest, you must try to find it out in your own environment
#ifndef ST_ASIO_SERVICE_THREAD_NUM
#define ST_ASIO_SERVICE_THREAD_NUM	8
#elif ST_ASIO_SERVICE_THREAD_NUM <= 0
	#error service thread number be bigger than zero.
#endif

//graceful shutdown must finish within this duration, otherwise, socket will be forcedly shut down.
#ifndef ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION
#define ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION	5 //seconds
#elif ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION <= 0
	#error graceful shutdown duration must be bigger than zero.
#endif

//if connecting (or reconnecting) failed, delay how much milliseconds before reconnecting, negative value means stop reconnecting,
//you can also rewrite tcp::client_socket_base::prepare_reconnect(), and return a negative value.
#ifndef ST_ASIO_RECONNECT_INTERVAL
#define ST_ASIO_RECONNECT_INTERVAL	500 //millisecond(s)
#endif

//define ST_ASIO_RECONNECT macro as false, then no reconnecting will be performed after the link broken passively.
//if proactively shutdown the link, reconnect or not depends on the reconnect parameter passed via disconnect, force_shutdown and graceful_shutdown.
#ifndef ST_ASIO_RECONNECT
#define ST_ASIO_RECONNECT	true
#endif

//how many async_accept delivery concurrently, an equivalent way is to rewrite virtual function server_socket_base::async_accept_num().
//server_base will pre-create ST_ASIO_ASYNC_ACCEPT_NUM server sockets before starting accepting connections, but we may don't want so many
// async_accept delivery all the time, what should we do?
//the answer is, we can rewrite virtual function server_socket_base::start_next_accept() and decide whether to continue the async accepting or not,
// for example, you normally have about 1000 connections, so you define ST_ASIO_ASYNC_ACCEPT_NUM as 1000, and:
// 1. rewrite virtual function server_socket_base::on_accept() and count how many connections already established;
// 2. rewrite virtual function server_socket_base::start_next_accept(), if we have already established 990 connections (or even more),
//    then call server_socket_base::start_next_accept() to continue the async accepting, otherwise, just return (to stop the async accepting).
// after doing that, the server will not create server sockets during accepting the 1st to 990th connections, and after the 990th connection,
// the server will always keep 10 async accepting (which also means after accepted a new connection, a new server socket will be created),
// see demo concurrent_server for more details.
//this feature is very useful if creating server socket is very expensive (which means creating server socket may affect the process or even
// the system seriously) and the total number of connections is basically stable (or you have a limitation for the number of connections).
//even creating server socket is very cheap, augmenting this macro can speed up connection accepting especially when many connecting request flood in.
#ifndef ST_ASIO_ASYNC_ACCEPT_NUM
#define ST_ASIO_ASYNC_ACCEPT_NUM	16
#elif ST_ASIO_SERVER_PORT <= 0
	#error async accept number must be bigger than zero.
#endif

//in server_base::set_server_addr and set_local_addr, if the IP is empty, ST_ASIO_(TCP/UDP)_DEFAULT_IP_VERSION will define the IP version,
// or the IP version will be deduced by the IP address.
//boost::asio::ip::(tcp/udp)::v4() means ipv4 and boost::asio::ip::(tcp/udp)::v6() means ipv6.
#ifndef ST_ASIO_TCP_DEFAULT_IP_VERSION
#define ST_ASIO_TCP_DEFAULT_IP_VERSION boost::asio::ip::tcp::v4()
#endif
#ifndef ST_ASIO_UDP_DEFAULT_IP_VERSION
#define ST_ASIO_UDP_DEFAULT_IP_VERSION boost::asio::ip::udp::v4()
#endif

//close port reuse
//#define ST_ASIO_NOT_REUSE_ADDRESS

#ifndef ST_ASIO_INPUT_QUEUE
#define ST_ASIO_INPUT_QUEUE lock_queue
#endif
#ifndef ST_ASIO_INPUT_CONTAINER
#define ST_ASIO_INPUT_CONTAINER list
#endif
#ifndef ST_ASIO_OUTPUT_QUEUE
#define ST_ASIO_OUTPUT_QUEUE lock_queue
#endif
#ifndef ST_ASIO_OUTPUT_CONTAINER
#define ST_ASIO_OUTPUT_CONTAINER list
#endif
//we also can control the queues (and their containers) via template parameters on class 'client_socket_base'
//'server_socket_base', 'ssl::client_socket_base' and 'ssl::server_socket_base'.
//we even can let a socket to use different queue (and / or different container) for input and output via template parameters.

//buffer type used when receiving messages (unpacker's prepare_next_recv() need to return this type)
#ifndef ST_ASIO_RECV_BUFFER_TYPE
	#if BOOST_ASIO_VERSION >= 101100
	#define ST_ASIO_RECV_BUFFER_TYPE boost::asio::mutable_buffer
	#else
	#define ST_ASIO_RECV_BUFFER_TYPE boost::asio::mutable_buffers_1
	#endif
#endif

#ifndef ST_ASIO_HEARTBEAT_INTERVAL
#define ST_ASIO_HEARTBEAT_INTERVAL	0 //second(s), disable heartbeat by default, just for compatibility
#endif
//at every ST_ASIO_HEARTBEAT_INTERVAL second(s):
// 1. tcp::socket_base will send an heartbeat if no messages been sent within this interval,
// 2. tcp::socket_base will check the link's connectedness, see ST_ASIO_HEARTBEAT_MAX_ABSENCE macro for more details.
//less than or equal to zero means disable heartbeat, then you can send and check heartbeat with you own logic by calling
//tcp::socket_base::check_heartbeat (do above steps one time) or tcp::socket_base::start_heartbeat (do above steps regularly).

#ifndef ST_ASIO_HEARTBEAT_MAX_ABSENCE
#define ST_ASIO_HEARTBEAT_MAX_ABSENCE	3 //times of ST_ASIO_HEARTBEAT_INTERVAL
#elif ST_ASIO_HEARTBEAT_MAX_ABSENCE <= 0
	#error heartbeat absence must be bigger than zero.
#endif
//if no any messages (include heartbeat) been received within ST_ASIO_HEARTBEAT_INTERVAL * ST_ASIO_HEARTBEAT_MAX_ABSENCE second(s), shut down the link.

//#define ST_ASIO_REUSE_SSL_STREAM
//if you need ssl::client_socket_base to be able to reconnect the server, or to open object pool in ssl::object_pool, you must define this macro.
//I tried many ways, only one way can make boost::asio::ssl::stream reusable, which is:
// don't call any shutdown functions of boost::asio::ssl::stream, just call boost::asio::ip::tcp::socket's shutdown function,
// this seems not a normal procedure, but it works, I believe that asio's defect caused this problem.

//#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
//wrap service_pump with boost::asio::io_service::work (boost::asio::executor_work_guard), then it will never run out until you explicitly call stop_service().

//#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//enable decreasing service thread at runtime.

#ifndef ST_ASIO_MSG_RESUMING_INTERVAL
#define ST_ASIO_MSG_RESUMING_INTERVAL	50 //milliseconds
#elif ST_ASIO_MSG_RESUMING_INTERVAL < 0
	#error the interval of msg resuming must be bigger than or equal to zero.
#endif
//msg receiving
//if receiving buffer is overflow, message receiving will stop and resume after the buffer becomes available, 
//this is the interval of receiving buffer checking.
//this value can be changed via st_asio_wrapper::socket::msg_resuming_interval(size_t) at runtime.

#ifndef ST_ASIO_MSG_HANDLING_INTERVAL
#define ST_ASIO_MSG_HANDLING_INTERVAL	50 //milliseconds
#elif ST_ASIO_MSG_HANDLING_INTERVAL < 0
	#error the interval of msg handling must be bigger than or equal to zero.
#endif
//msg handling
//call on_msg_handle, if failed, retry it after ST_ASIO_MSG_HANDLING_INTERVAL milliseconds later.
//this value can be changed via st_asio_wrapper::socket::msg_handling_interval(size_t) at runtime.

//#define ST_ASIO_PASSIVE_RECV
//to gain the ability of changing the unpacker at runtime, with this macro, st_asio_wrapper will not do message receiving automatically (except the firt one),
// so you need to manually call recv_msg(), if you need to change the unpacker, do it before recv_msg() invocation, please note.
//during async message receiving, calling recv_msg() will fail, this is by design to avoid asio::io_context using up all virtual memory.
//because user can greedily call recv_msg(), it's your responsibility to keep the recv buffer from overflowed, please pay special attention.
//this macro also makes you to be able to pause message receiving, then, if there's no other tasks (like timers), service_pump will stop itself,
// to avoid this, please define macro ST_ASIO_AVOID_AUTO_STOP_SERVICE.

//#define ST_ASIO_DISPATCH_BATCH_MSG
//all messages will be dispatched via on_handle_msg with a variable-length container, this will change the signature of function on_msg_handle,
//it's very useful if you want to re-dispatch message in your own logic or with very simple message handling (such as echo server).
//it's your responsibility to remove handled messages from the container (can be a part of them).

//#define ST_ASIO_ALIGNED_TIMER
//for example, start a timer at xx:xx:xx, interval is 10 seconds, the callback will be called at (xx:xx:xx + 10), and suppose that the callback
//returned at (xx:xx:xx + 11), then the interval will be temporarily changed to 9 seconds to make the next callback to be called at (xx:xx:xx + 20),
//if you don't define this macro, the next callback will be called at (xx:xx:xx + 21), please note.

//#define ST_ASIO_SYNC_SEND
//#define ST_ASIO_SYNC_RECV
//define these macro to gain additional series of sync message sending and receiving, they are:
// sync_send_msg
// sync_send_native_msg
// sync_safe_send_msg
// sync_safe_send_native_msg
// sync_recv_msg
//please note that:
// this feature will slightly impact efficiency even if you always use async message sending and receiving, so only open this feature when really needed.
// we must avoid to do sync message sending and receiving in service threads.
// if prior sync_recv_msg() not returned, the second sync_recv_msg() will return false immediately.
// with macro ST_ASIO_PASSIVE_RECV, in sync_recv_msg(), recv_msg() will be automatically called.
// after returned from sync_recv_msg(), st_asio_wrapper will not maintain those messages any more.

//Sync operations are not tracked by tracked_executor, please note.
//Sync operations can be performed with async operations concurrently.
//If both sync message receiving and async message receiving exist, sync receiving has the priority no matter it was initiated before async receiving or not.

//#define ST_ASIO_SYNC_DISPATCH
//with this macro, virtual size_t on_msg(list<OutMsgType>& msg_can) will be provided, you can rewrite it and handle all or a part of the
// messages like virtual function on_msg_handle (with macro ST_ASIO_DISPATCH_BATCH_MSG), if your logic is simple enough (like echo or pingpong test),
// this feature is recommended because it can slightly improve efficiency.
//now we have three ways to handle messages (sync_recv_msg, on_msg and on_msg_handle), the invocation order is the same as listed, if messages been successfully
// dispatched to sync_recv_msg, then the second two will not be called, otherwise messages will be dispatched to on_msg, if on_msg only handled a part of (include
// zero) the messages, then on_msg_handle will continue to dispatch the rest of them asynchronously, this will disorder messages because on_msg_handle and the next
// on_msg (new messages arrived) can be invoked concurrently, please note. as before, on_msg will block the next receiving but only on current socket.
//if you cannot handle all of the messages in on_msg (like echo_server), you should not use sync message dispatching except you can bear message disordering.

//if you search or traverse (via do_something_to_all or do_something_to_one) objects in object_pool frequently,
// use shared_mutex with shared_lock instead of mutex with unique_lock will promote performance, otherwise, do not define these two macros.
#ifndef ST_ASIO_SHARED_MUTEX_TYPE
#define ST_ASIO_SHARED_MUTEX_TYPE	boost::mutex
#endif
#ifndef ST_ASIO_SHARED_LOCK_TYPE
#define ST_ASIO_SHARED_LOCK_TYPE	boost::unique_lock
#endif

//configurations

#endif /* ST_ASIO_CONFIG_H_ */

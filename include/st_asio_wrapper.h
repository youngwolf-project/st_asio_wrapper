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
 * Add virtual function st_server_base::on_accept_error, it controls whether to continue the acception or not when error occurred.
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
 * known issues:
 * 1. On boost-1.49, st_object::is_last_async_call() cannot work properly, it is because before asio call any callbacks, it copied the callback(not a
 *    good behaviour), this cause st_object::is_last_async_call() never return true, so objects in st_object_pool can never be reused or freed.
 *    To fix this issue, we must not define ST_ASIO_ENHANCED_STABILITY macro.
 *    BTW, boost-1.61 doesn't have such issue, I'm not sure in which edition, asio fixed this defect, if you have other versions, please help me to
 *    find out the minimum version via the following steps:
 *     1. Compile demo asio_server and test_client;
 *     2. Run asio_server and test_client without any parameters;
 *     3. Stop test_client (input 'quit');
 *     4. Stop asio_server (input 'quit');
 *     5. If asio_server successfully quitted, means this edition doesn't have above defect.
 *
 */

#ifndef ST_ASIO_WRAPPER_H_
#define ST_ASIO_WRAPPER_H_

#define ST_ASIO_WRAPPER_VER		10200	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ST_ASIO_WRAPPER_VERSION	"1.2.0"

#ifdef _MSC_VER
	static_assert(_MSC_VER >= 1600, "st_asio_wrapper must be compiled with Visual C++ 10.0 or higher.");
#elif defined(__GNUC__)
	//After a roughly reading from gcc.gnu.org and clang.llvm.org, I believed that the minimum version of GCC and Clang that support c++0x
	//are 4.6 and 3.1, so, I supply the following compiler verification. If there's something wrong, you can freely modify them,
	//and if you let me know, I'll be very appreciated.
	#ifdef __clang__
		static_assert(__clang_major__ > 3 || __clang_major__ == 3 && __clang_minor__ >= 1, "st_asio_wrapper must be compiled with Clang 3.1 or higher.");
	#elif __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ < 6
		#error st_asio_wrapper must be compiled with GCC 4.6 or higher.
	#endif

	#if !defined(__GXX_EXPERIMENTAL_CXX0X__) && (!defined(__cplusplus) || __cplusplus < 201103L)
		#error st_asio_wrapper(standard edition) need c++0x support, please open these features.
	#endif
#else
	#error st_asio_wrapper only support Visual C++, GCC and Clang.
#endif

static_assert(BOOST_VERSION >= 104900, "st_asio_wrapper only support boost 1.49 or higher.");

#endif /* ST_ASIO_WRAPPER_H_ */

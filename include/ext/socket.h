/*
 * socket.h
 *
 *  Created on: 2023-1-1
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * customize socket by event registration instead of overwrite virtual functions.
 */

#ifndef ST_ASIO_EXT_SOCKET_H_
#define ST_ASIO_EXT_SOCKET_H_

#include <boost/functional.hpp>

#include "../base.h"

namespace st_asio_wrapper { namespace ext {

#define call_cb_void(fun) virtual void fun() {if (cb_##fun.first) cb_##fun.first(this); if (cb_##fun.second) Socket::fun();}
#define call_cb_1_void(fun, p) {if (cb_##fun.first) cb_##fun.first(this, p); if (cb_##fun.second) Socket::fun(p);}
#define call_cb_2_void(fun, p1, p2) {if (cb_##fun.first) cb_##fun.first(this, p1, p2); if (cb_##fun.second) Socket::fun(p1, p2);}

#define call_cb_combine(fun) virtual bool fun() {bool re = cb_##fun.first ? cb_##fun.first(this) : true; if (re && cb_##fun.second) re = Socket::fun(); return re;}
#define call_cb_1_return(init, fun, p) {BOOST_AUTO(re, init); if (cb_##fun.first) re = cb_##fun.first(this, p); if (cb_##fun.second) re = Socket::fun(p); return re;}

#define register_cb(fun, init) \
template<typename CallBack> void register_##fun(const CallBack& cb, bool pass_on = init) {cb_##fun.first = cb; cb_##fun.second = pass_on;}

template<typename Socket> class g_socket : public Socket //udp socket will use g_socket only
{
public:
	typedef Socket raw_socket;

public:
	template<typename Arg> g_socket(Arg& arg) : Socket(arg) {first_init();}
	template<typename Arg1, typename Arg2> g_socket(Arg1& arg1, Arg2& arg2) : Socket(arg1, arg2) {first_init();}

	register_cb(obsoleted, true)
	register_cb(is_ready, true)
	register_cb(send_heartbeat, false)
	register_cb(reset, true)
	register_cb(on_heartbeat_error, true)
	register_cb(on_send_error, true)
	register_cb(on_recv_error, true)
	register_cb(on_close, true)
	register_cb(after_close, true)
#ifdef ST_ASIO_SYNC_DISPATCH
	register_cb(on_msg, false)
#endif
	register_cb(on_msg_handle, false)
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	register_cb(on_msg_send, false)
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	register_cb(on_all_msg_send, false)
#endif
#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	register_cb(calc_shrink_size, false)
	register_cb(on_msg_discard, false)
#endif

public:
	call_cb_combine(obsoleted)
	call_cb_combine(is_ready)
	call_cb_void(send_heartbeat)
	call_cb_void(reset)

protected:
	call_cb_combine(on_heartbeat_error)
	virtual void on_send_error(const boost::system::error_code& ec, typename Socket::in_container_type& msg_can) call_cb_2_void(on_send_error, ec, msg_can)
	virtual void on_recv_error(const boost::system::error_code& ec) call_cb_1_void(on_recv_error, ec)
	call_cb_void(on_close)
	call_cb_void(after_close)

#ifdef ST_ASIO_SYNC_DISPATCH
	virtual size_t on_msg(list<typename Socket::out_msg_type>& msg_can) call_cb_1_return((size_t) 0, on_msg, msg_can)
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(typename Socket::out_queue_type& msg_can) call_cb_1_return((size_t) 0, on_msg_handle, msg_can)
#else
	virtual bool on_msg_handle(typename Socket::out_msg_type& msg) call_cb_1_return(false, on_msg_handle, msg)
#endif

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(typename Socket::in_msg_type& msg) call_cb_1_void(on_msg_send, msg)
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	virtual void on_all_msg_send(typename Socket::in_msg_type& msg) call_cb_1_void(on_all_msg_send, msg)
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	virtual size_t calc_shrink_size(size_t current_size) call_cb_1_return((size_t) 0, calc_shrink_size, current_size)
	virtual void on_msg_discard(typename Socket::in_container_type& msg_can) call_cb_1_void(on_msg_discard, msg_can)
#endif

private:
	void first_init()
	{
		cb_obsoleted.second = true;
		cb_is_ready.second = true;
		cb_send_heartbeat.second = true;
		cb_reset.second = true;
		cb_on_heartbeat_error.second = true;
		cb_on_send_error.second = true;
		cb_on_recv_error.second = true;
		cb_on_close.second = true;
		cb_after_close.second = true;

#ifdef ST_ASIO_SYNC_DISPATCH
		cb_on_msg.second = true;
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
		cb_on_msg_handle.second = true;
#else
		cb_on_msg_handle.second = true;
#endif

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
		cb_on_msg_send.second = true;
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
		cb_on_all_msg_send.second = true;
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
		cb_calc_shrink_size.second = true;
		cb_on_msg_discard.second = true;
#endif
	}

private:
	std::pair<boost::function<bool(Socket*)>, bool> cb_obsoleted;
	std::pair<boost::function<bool(Socket*)>, bool> cb_is_ready;
	std::pair<boost::function<void(Socket*)>, bool> cb_send_heartbeat;
	std::pair<boost::function<void(Socket*)>, bool> cb_reset;
	std::pair<boost::function<bool(Socket*)>, bool> cb_on_heartbeat_error;
	std::pair<boost::function<void(Socket*, const boost::system::error_code&, typename Socket::in_container_type&)>, bool> cb_on_send_error;
	std::pair<boost::function<void(Socket*, const boost::system::error_code&)>, bool> cb_on_recv_error;
	std::pair<boost::function<void(Socket*)>, bool> cb_on_close;
	std::pair<boost::function<void(Socket*)>, bool> cb_after_close;

#ifdef ST_ASIO_SYNC_DISPATCH
	std::pair<boost::function<size_t(Socket*, list<typename Socket::out_msg_type>&)>, bool> cb_on_msg;
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	std::pair<boost::function<size_t(Socket*, typename Socket::out_queue_type&)>, bool> cb_on_msg_handle;
#else
	std::pair<boost::function<bool(Socket*, typename Socket::out_msg_type&)>, bool> cb_on_msg_handle;
#endif

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	std::pair<boost::function<void(Socket*, typename Socket::in_msg_type&)>, bool> cb_on_msg_send;
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	std::pair<boost::function<void(Socket*, typename Socket::in_msg_type&)>, bool> cb_on_all_msg_send;
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	std::pair<boost::function<size_t(Socket*, size_t)>, bool> cb_calc_shrink_size;
	std::pair<boost::function<void(Socket*, typename Socket::in_container_type&)>, bool> cb_on_msg_discard;
#endif
};

template<typename Socket> class tcp_socket : public g_socket<Socket>
{
public:
	template<typename Arg> tcp_socket(Arg& arg) : g_socket<Socket>(arg) {first_init();}
	template<typename Arg1, typename Arg2> tcp_socket(Arg1& arg1, Arg2& arg2) : g_socket<Socket>(arg1, arg2) {first_init();}

	register_cb(on_connect, false)
	register_cb(on_unpack_error, true)
	register_cb(on_async_shutdown_error, true)

protected:
	call_cb_void(on_connect)
	call_cb_void(on_unpack_error)
	call_cb_void(on_async_shutdown_error)

private:
	void first_init()
	{
		cb_on_connect.second = true;
		cb_on_unpack_error.second = true;
		cb_on_async_shutdown_error.second = true;
	}

private:
	std::pair<boost::function<void(Socket*)>, bool> cb_on_connect;
	std::pair<boost::function<void(Socket*)>, bool> cb_on_unpack_error;
	std::pair<boost::function<void(Socket*)>, bool> cb_on_async_shutdown_error;
};

template<typename Socket> class c_socket : public tcp_socket<Socket> //for client socket
{
public:
	template<typename Arg> c_socket(Arg& arg) : tcp_socket<Socket>(arg) {first_init();}
	template<typename Arg1, typename Arg2> c_socket(Arg1& arg1, Arg2& arg2) : tcp_socket<Socket>(arg1, arg2) {first_init();}

	register_cb(prepare_reconnect, false)

protected:
	virtual int prepare_reconnect(const boost::system::error_code& ec) call_cb_1_return(0, prepare_reconnect, ec)

private:
	void first_init() {cb_prepare_reconnect.second = true;}

private:
	std::pair<boost::function<int(Socket*, const boost::system::error_code&)>, bool> cb_prepare_reconnect;
};

template<typename Socket> class s_socket : public tcp_socket<Socket> //for server socket
{
public:
	template<typename Arg> s_socket(Arg& arg) : tcp_socket<Socket>(arg) {first_init();}
	template<typename Arg1, typename Arg2> s_socket(Arg1& arg1, Arg2& arg2) : tcp_socket<Socket>(arg1, arg2) {first_init();}

	register_cb(take_over, false)

public:
	virtual void take_over(boost::shared_ptr<typename Socket::type_of_object_restore> socket_ptr) call_cb_1_void(take_over, socket_ptr)

private:
	void first_init() {cb_take_over.second = true;}

private:
	std::pair<boost::function<void(Socket*, boost::shared_ptr<typename Socket::type_of_object_restore>)>, bool> cb_take_over;
};

}} //namespace

#endif /* ST_ASIO_EXT_SOCKET_H_ */

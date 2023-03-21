/*
 * socket.h
 *
 *  Created on: 2023-1-1
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * customize sockets/server/object_pool by event registration instead of overwrite virtual functions.
 */

#ifndef ST_ASIO_EXT_CALLBACKS_H_
#define ST_ASIO_EXT_CALLBACKS_H_

#include <boost/functional.hpp>

#include "../base.h"

namespace st_asio_wrapper { namespace ext { namespace callbacks {

#define call_cb_void(super, fun) virtual void fun() {if (cb_##fun.first) cb_##fun.first(this); if (!cb_##fun.second) super::fun();}
#define call_cb_1_void(super, fun, p) {if (cb_##fun.first) cb_##fun.first(this, p); if (!cb_##fun.second) super::fun(p);}
#define call_cb_2_void(super, fun, p1, p2) {if (cb_##fun.first) cb_##fun.first(this, p1, p2); if (!cb_##fun.second) super::fun(p1, p2);}

#define call_cb_combine(super, fun) virtual bool fun() {bool re = cb_##fun.first ? cb_##fun.first(this) : true; if (re && !cb_##fun.second) re = super::fun(); return re;}
#define call_cb_return(super, type, fun) virtual type fun() {type re = type(); if (cb_##fun.first) re = cb_##fun.first(this); if (!cb_##fun.second) re = super::fun(); return re;}
#define call_cb_1_combine(super, fun, p) {bool re = cb_##fun.first ? cb_##fun.first(this, p) : true; if (re && !cb_##fun.second) re = super::fun(p); return re;}
#define call_cb_1_return(super, type, fun, p) {type re = type(); if (cb_##fun.first) re = cb_##fun.first(this, p); if (!cb_##fun.second) re = super::fun(p); return re;}
#define call_cb_2_combine(super, fun, p1, p2) {bool re = cb_##fun.first ? cb_##fun.first(this, p1, p2) : true; if (re && !cb_##fun.second) re = super::fun(p1, p2); return re;}

#define register_cb_1(fun, init) \
template<typename CallBack> void register_##fun(const CallBack& cb, bool pass_on = init) {cb_##fun.first = cb; cb_##fun.second = !pass_on;} \
void register_##fun(fo_##fun* cb, bool pass_on = init) {cb_##fun.first = boost::bind(cb, boost::placeholders::_1); cb_##fun.second = !pass_on;}
#define register_cb_2(fun, init) \
template<typename CallBack> void register_##fun(const CallBack& cb, bool pass_on = init) {cb_##fun.first = cb; cb_##fun.second = !pass_on;} \
void register_##fun(fo_##fun* cb, bool pass_on = init) {cb_##fun.first = boost::bind(cb, boost::placeholders::_1, boost::placeholders::_2); cb_##fun.second = !pass_on;}
#define register_cb_3(fun, init) \
template<typename CallBack> void register_##fun(const CallBack& cb, bool pass_on = init) {cb_##fun.first = cb; cb_##fun.second = !pass_on;} \
void register_##fun(fo_##fun* cb, bool pass_on = init) \
	{cb_##fun.first = boost::bind(cb, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3); cb_##fun.second = !pass_on;}

template<typename Socket> class g_socket : public Socket //udp socket will use g_socket only
{
public:
	typedef bool fo_obsoleted(Socket*);
	typedef bool fo_is_ready(Socket*);
	typedef void fo_send_heartbeat(Socket*);
	typedef void fo_reset(Socket*);
	typedef bool fo_on_heartbeat_error(Socket*);
	typedef void fo_on_send_error(Socket*, const boost::system::error_code&, typename Socket::in_container_type&);
	typedef void fo_on_recv_error(Socket*, const boost::system::error_code&);
	typedef void fo_on_close(Socket*);
	typedef void fo_after_close(Socket*);

#ifdef ST_ASIO_SYNC_DISPATCH
	typedef size_t fo_on_msg(Socket*, list<typename Socket::out_msg_type>&);
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	typedef size_t fo_on_msg_handle(Socket*, typename Socket::out_queue_type&);
#else
	typedef bool fo_on_msg_handle(Socket*, typename Socket::out_msg_type&);
#endif

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	typedef void fo_on_msg_send(Socket*, typename Socket::in_msg_type&);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	typedef void fo_on_all_msg_send(Socket*, typename Socket::in_msg_type&);
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	typedef size_t fo_calc_shrink_size(Socket*, size_t);
	typedef void fo_on_msg_discard(Socket*, typename Socket::in_container_type&);
#endif

public:
	template<typename Arg> g_socket(Arg& arg) : Socket(arg) {}
	template<typename Arg1, typename Arg2> g_socket(Arg1& arg1, Arg2& arg2) : Socket(arg1, arg2) {}

	register_cb_1(obsoleted, true)
	register_cb_1(is_ready, true)
	register_cb_1(send_heartbeat, false)
	register_cb_1(reset, true)
	register_cb_1(on_heartbeat_error, true)
	register_cb_3(on_send_error, true)
	register_cb_2(on_recv_error, true)
	register_cb_1(on_close, true)
	register_cb_1(after_close, true)
#ifdef ST_ASIO_SYNC_DISPATCH
	register_cb_2(on_msg, false)
#endif
	register_cb_2(on_msg_handle, false)
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	register_cb_2(on_msg_send, false)
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	register_cb_2(on_all_msg_send, false)
#endif
#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	register_cb_2(calc_shrink_size, false)
	register_cb_2(on_msg_discard, false)
#endif

public:
	call_cb_combine(Socket, obsoleted)
	call_cb_void(Socket, reset)

private:
	call_cb_combine(Socket, is_ready)
	call_cb_void(Socket, send_heartbeat)
	call_cb_combine(Socket, on_heartbeat_error)
	virtual void on_send_error(const boost::system::error_code& ec, typename Socket::in_container_type& msg_can) call_cb_2_void(Socket, on_send_error, ec, msg_can)
	virtual void on_recv_error(const boost::system::error_code& ec) call_cb_1_void(Socket, on_recv_error, ec)
	call_cb_void(Socket, on_close)
	call_cb_void(Socket, after_close)

#ifdef ST_ASIO_SYNC_DISPATCH
	virtual size_t on_msg(list<typename Socket::out_msg_type>& msg_can) call_cb_1_return(Socket, size_t, on_msg, msg_can)
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(typename Socket::out_queue_type& msg_can) call_cb_1_return(Socket, size_t, on_msg_handle, msg_can)
#else
	virtual bool on_msg_handle(typename Socket::out_msg_type& msg) call_cb_1_combine(Socket, on_msg_handle, msg)
#endif

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(typename Socket::in_msg_type& msg) call_cb_1_void(Socket, on_msg_send, msg)
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	virtual void on_all_msg_send(typename Socket::in_msg_type& msg) call_cb_1_void(Socket, on_all_msg_send, msg)
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	virtual size_t calc_shrink_size(size_t current_size) call_cb_1_return(Socket, size_t, calc_shrink_size, current_size)
	virtual void on_msg_discard(typename Socket::in_container_type& msg_can) call_cb_1_void(Socket, on_msg_discard, msg_can)
#endif

private:
	std::pair<boost::function<fo_obsoleted>, bool> cb_obsoleted;
	std::pair<boost::function<fo_is_ready>, bool> cb_is_ready;
	std::pair<boost::function<fo_send_heartbeat>, bool> cb_send_heartbeat;
	std::pair<boost::function<fo_reset>, bool> cb_reset;
	std::pair<boost::function<fo_on_heartbeat_error>, bool> cb_on_heartbeat_error;
	std::pair<boost::function<fo_on_send_error>, bool> cb_on_send_error;
	std::pair<boost::function<fo_on_recv_error>, bool> cb_on_recv_error;
	std::pair<boost::function<fo_on_close>, bool> cb_on_close;
	std::pair<boost::function<fo_after_close>, bool> cb_after_close;

#ifdef ST_ASIO_SYNC_DISPATCH
	std::pair<boost::function<fo_on_msg>, bool> cb_on_msg;
#endif
	std::pair<boost::function<fo_on_msg_handle>, bool> cb_on_msg_handle;
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	std::pair<boost::function<fo_on_msg_send>, bool> cb_on_msg_send;
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	std::pair<boost::function<fo_on_all_msg_send>, bool> cb_on_all_msg_send;
#endif

#ifdef ST_ASIO_SHRINK_SEND_BUFFER
	std::pair<boost::function<fo_calc_shrink_size>, bool> cb_calc_shrink_size;
	std::pair<boost::function<fo_on_msg_discard>, bool> cb_on_msg_discard;
#endif
};

template<typename Socket> class tcp_socket : public g_socket<Socket>
{
public:
	typedef void fo_on_connect(Socket*);
	typedef void fo_on_unpack_error(Socket*);
	typedef void fo_on_async_shutdown_error(Socket*);

public:
	template<typename Arg> tcp_socket(Arg& arg) : g_socket<Socket>(arg) {}
	template<typename Arg1, typename Arg2> tcp_socket(Arg1& arg1, Arg2& arg2) : g_socket<Socket>(arg1, arg2) {}

	register_cb_1(on_connect, false)
	register_cb_1(on_unpack_error, true)
	register_cb_1(on_async_shutdown_error, true)

private:
	call_cb_void(Socket, on_connect)
	call_cb_void(Socket, on_unpack_error)
	call_cb_void(Socket, on_async_shutdown_error)

private:
	std::pair<boost::function<fo_on_connect>, bool> cb_on_connect;
	std::pair<boost::function<fo_on_unpack_error>, bool> cb_on_unpack_error;
	std::pair<boost::function<fo_on_async_shutdown_error>, bool> cb_on_async_shutdown_error;
};

template<typename Socket> class c_socket : public tcp_socket<Socket> //for client socket
{
public:
	typedef int fo_prepare_reconnect(Socket*, const boost::system::error_code&);

public:
	template<typename Arg> c_socket(Arg& arg) : tcp_socket<Socket>(arg) {}
	template<typename Arg1, typename Arg2> c_socket(Arg1& arg1, Arg2& arg2) : tcp_socket<Socket>(arg1, arg2) {}

	register_cb_2(prepare_reconnect, false)

private:
	virtual int prepare_reconnect(const boost::system::error_code& ec) call_cb_1_return(Socket, int, prepare_reconnect, ec)

private:
	std::pair<boost::function<fo_prepare_reconnect>, bool> cb_prepare_reconnect;
};

template<typename Socket> class s_socket : public tcp_socket<Socket> //for server socket
{
public:
	typedef void fo_take_over(Socket*, boost::shared_ptr<typename Socket::type_of_object_restore>);

public:
	template<typename Arg> s_socket(Arg& arg) : tcp_socket<Socket>(arg) {}
	template<typename Arg1, typename Arg2> s_socket(Arg1& arg1, Arg2& arg2) : tcp_socket<Socket>(arg1, arg2) {}

	register_cb_2(take_over, false)

public:
	virtual void take_over(boost::shared_ptr<typename Socket::type_of_object_restore> socket_ptr) call_cb_1_void(Socket, take_over, socket_ptr)

private:
	std::pair<boost::function<fo_take_over>, bool> cb_take_over;
};

template<typename Server> class server : public Server //for server
{
public:
	typedef int fo_async_accept_num(Server*);
	typedef void fo_start_next_accept(Server*);
	typedef bool fo_on_accept(Server*, typename Server::object_ctype&);
	typedef bool fo_on_accept_error(Server*, const boost::system::error_code&, typename Server::object_ctype&);

public:
	template<typename Arg> server(Arg& arg) : Server(arg) {}
	template<typename Arg1, typename Arg2> server(Arg1& arg1, Arg2& arg2) : Server(arg1, arg2) {}

	register_cb_1(async_accept_num, false)
	register_cb_1(start_next_accept, true)
	register_cb_2(on_accept, true)
	register_cb_3(on_accept_error, true)

private:
	call_cb_return(Server, int, async_accept_num)
	call_cb_void(Server, start_next_accept)
	virtual bool on_accept(typename Server::object_ctype& socket_ptr) call_cb_1_combine(Server, on_accept, socket_ptr)
	virtual bool on_accept_error(const boost::system::error_code& ec, typename Server::object_ctype& socket_ptr) call_cb_2_combine(Server, on_accept_error, ec, socket_ptr)

private:
	std::pair<boost::function<fo_async_accept_num>, bool> cb_async_accept_num;
	std::pair<boost::function<fo_start_next_accept>, bool> cb_start_next_accept;
	std::pair<boost::function<fo_on_accept>, bool> cb_on_accept;
	std::pair<boost::function<fo_on_accept_error>, bool> cb_on_accept_error;
};

template<typename ObjectPool> class object_pool : public ObjectPool
{
public:
	typedef void fo_on_create(ObjectPool*, typename ObjectPool::object_ctype&);

public:
	template<typename Arg> object_pool(Arg& arg) : ObjectPool(arg) {}

	register_cb_2(on_create, false)

private:
	virtual void on_create(typename ObjectPool::object_ctype& object_ptr) call_cb_1_void(ObjectPool, on_create, object_ptr)

private:
	std::pair<boost::function<fo_on_create>, bool> cb_on_create;
};

}}} //namespace

#endif /* ST_ASIO_EXT_CALLBACKS_H_ */

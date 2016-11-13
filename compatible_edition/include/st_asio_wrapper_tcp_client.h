/*
 * st_asio_wrapper_tcp_client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at TCP client endpoint
 */

#ifndef ST_ASIO_WRAPPER_TCP_CLIENT_H_
#define ST_ASIO_WRAPPER_TCP_CLIENT_H_

#include "st_asio_wrapper_client.h"

namespace st_asio_wrapper
{

template<typename Socket, typename Pool = st_object_pool<Socket> >
class st_tcp_client_base : public st_client<Socket, Pool>
{
protected:
	typedef st_client<Socket, Pool> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_tcp_client_base(st_service_pump& service_pump_) : super(service_pump_) {}
	template<typename Arg>
	st_tcp_client_base(st_service_pump& service_pump_, Arg arg) : super(service_pump_, arg) {}

	//connected link size, may smaller than total object size(st_object_pool::size)
	size_t valid_size()
	{
		size_t size = 0;
		ST_THIS do_something_to_all(boost::lambda::if_then(boost::lambda::bind(&Socket::is_connected, *boost::lambda::_1), ++boost::lambda::var(size)));
		return size;
	}

	using super::add_client;
	typename Pool::object_type add_client()
	{
		BOOST_AUTO(client_ptr, ST_THIS create_object());
		return ST_THIS add_client(client_ptr, false) ? client_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_client(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		BOOST_AUTO(client_ptr, ST_THIS create_object());
		client_ptr->set_server_addr(port, ip);
		return ST_THIS add_client(client_ptr, false) ? client_ptr : typename Pool::object_type();
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a client_ptr parameter will remove the link from object pool first, then call corresponding function, if you want to reconnect to the server,
	//please call client_ptr's 'disconnect' 'force_shutdown' or 'graceful_shutdown' with true 'reconnect' directly.
	void disconnect(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->disconnect(false);}
	void disconnect(bool reconnect = false) {ST_THIS do_something_to_all(boost::bind(&Socket::disconnect, _1, reconnect));}
	void force_shutdown(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->force_shutdown(false);}
	void force_shutdown(bool reconnect = false) {ST_THIS do_something_to_all(boost::bind(&Socket::force_shutdown, _1, reconnect));}
	void graceful_shutdown(typename Pool::object_ctype& client_ptr, bool sync = true) {ST_THIS del_object(client_ptr); client_ptr->graceful_shutdown(false, sync);}
	void graceful_shutdown(bool reconnect = false, bool sync = true) {ST_THIS do_something_to_all(boost::bind(&Socket::graceful_shutdown, _1, reconnect, sync));}

protected:
	virtual void uninit() {ST_THIS stop(); graceful_shutdown();}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_TCP_CLIENT_H_ */

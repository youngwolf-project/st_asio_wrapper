/*
 * client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_TCP_CLIENT_H_
#define ST_ASIO_TCP_CLIENT_H_

#include "../socket_service.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Socket> class single_client_base : public single_socket_service<Socket>
{
public:
	single_client_base(service_pump& service_pump_) : single_socket_service<Socket>(service_pump_) {}
	template<typename Arg>
	single_client_base(service_pump& service_pump_, Arg& arg) : single_socket_service<Socket>(service_pump_, arg) {}
};

template<typename Socket, typename Pool = object_pool<Socket> >
class multi_client_base : public multi_socket_service<Socket, Pool>
{
private:
	typedef multi_socket_service<Socket, Pool> super;

public:
	multi_client_base(service_pump& service_pump_) : super(service_pump_) {}
	template<typename Arg>
	multi_client_base(service_pump& service_pump_, const Arg& arg) : super(service_pump_, arg) {}

	//connected link size, may smaller than total object size (object_pool::size)
	size_t valid_size()
	{
		size_t size = 0;
		ST_THIS do_something_to_all(boost::lambda::if_then(boost::lambda::bind(&Socket::is_connected, *boost::lambda::_1), ++boost::lambda::var(size)));
		return size;
	}

	using super::add_socket;
	typename Pool::object_type add_socket()
	{
		BOOST_AUTO(socket_ptr, ST_THIS create_object());
		return ST_THIS add_socket(socket_ptr, false) ? socket_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		BOOST_AUTO(socket_ptr, ST_THIS create_object());
		if (!socket_ptr)
			return typename Pool::object_type();
		else
		{
			socket_ptr->set_server_addr(port, ip);
			return ST_THIS add_socket(socket_ptr, false) ? socket_ptr : typename Pool::object_type();
		}
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//send message with sync mode
	TCP_BROADCAST_MSG(sync_broadcast_msg, sync_send_msg)
	TCP_BROADCAST_MSG(sync_broadcast_native_msg, sync_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function, if you want to reconnect to the server,
	//please call socket_ptr's 'disconnect' 'force_shutdown' or 'graceful_shutdown' with true 'reconnect' directly.
	void disconnect(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->disconnect(false);}
	void disconnect(bool reconnect = false) {ST_THIS do_something_to_all(boost::bind(&Socket::disconnect, _1, reconnect));}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->force_shutdown(false);}
	void force_shutdown(bool reconnect = false) {ST_THIS do_something_to_all(boost::bind(&Socket::force_shutdown, _1, reconnect));}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr, bool sync = true) {ST_THIS del_object(socket_ptr); socket_ptr->graceful_shutdown(false, sync);}
	void graceful_shutdown(bool reconnect = false, bool sync = true) {ST_THIS do_something_to_all(boost::bind(&Socket::graceful_shutdown, _1, reconnect, sync));}

protected:
	virtual void uninit() {ST_THIS stop(); force_shutdown();} //if you wanna graceful shutdown, call graceful_shutdown before service_pump::stop_service invocation.
};

}} //namespace

#endif /* ST_ASIO_TCP_CLIENT_H_ */

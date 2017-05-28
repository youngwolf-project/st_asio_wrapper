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

#ifdef ST_ASIO_HAS_TEMPLATE_USING
template<typename Socket> using st_tcp_sclient_base = st_sclient<Socket>;
#else
template<typename Socket> class st_tcp_sclient_base : public st_sclient<Socket>
{
public:
	st_tcp_sclient_base(st_service_pump& service_pump_) : st_sclient<Socket>(service_pump_) {}
	template<typename Arg>
	st_tcp_sclient_base(st_service_pump& service_pump_, Arg& arg) : st_sclient<Socket>(service_pump_, arg) {}
};
#endif

template<typename Socket, typename Pool = st_object_pool<Socket>>
class st_tcp_client_base : public st_client<Socket, Pool>
{
private:
	typedef st_client<Socket, Pool> super;

public:
	st_tcp_client_base(st_service_pump& service_pump_) : super(service_pump_) {}
	template<typename Arg>
	st_tcp_client_base(st_service_pump& service_pump_, const Arg& arg) : super(service_pump_, arg) {}

	//connected link size, may smaller than total object size(st_object_pool::size)
	size_t valid_size()
	{
		size_t size = 0;
		this->do_something_to_all([&size](typename Pool::object_ctype& item) {if (item->is_connected()) ++size;});
		return size;
	}

	using super::add_socket;
	typename Pool::object_type add_socket()
	{
		auto socket_ptr(this->create_object());
		return this->add_socket(socket_ptr, false) ? socket_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		auto socket_ptr(this->create_object());
		socket_ptr->set_server_addr(port, ip);
		return this->add_socket(socket_ptr, false) ? socket_ptr : typename Pool::object_type();
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//send message with sync mode
	TCP_BROADCAST_MSG(sync_broadcast_msg, sync_send_msg)
	TCP_BROADCAST_MSG(sync_broadcast_native_msg, sync_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function, if you want to reconnect to the server,
	//please call socket_ptr's 'disconnect' 'force_shutdown' or 'graceful_shutdown' with true 'reconnect' directly.
	void disconnect(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->disconnect(false);}
	void disconnect(bool reconnect = false) {this->do_something_to_all([=](typename Pool::object_ctype& item) {item->disconnect(reconnect);});}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {this->del_object(socket_ptr); socket_ptr->force_shutdown(false);}
	void force_shutdown(bool reconnect = false) {this->do_something_to_all([=](typename Pool::object_ctype& item) {item->force_shutdown(reconnect);});}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr, bool sync = true) {this->del_object(socket_ptr); socket_ptr->graceful_shutdown(false, sync);}
	void graceful_shutdown(bool reconnect = false, bool sync = true) {this->do_something_to_all([=](typename Pool::object_ctype& item) {item->graceful_shutdown(reconnect, sync);});}

protected:
	virtual void uninit() {this->stop(); graceful_shutdown();}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_TCP_CLIENT_H_ */

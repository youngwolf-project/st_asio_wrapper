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

#include "st_asio_wrapper_connector.h"
#include "st_asio_wrapper_client.h"

namespace st_asio_wrapper
{

typedef st_sclient<st_connector> st_tcp_sclient;

template<typename Socket = st_connector, typename Pool = st_object_pool<Socket>>
class st_tcp_client_base : public st_client<Socket, Pool>
{
public:
	st_tcp_client_base(st_service_pump& service_pump_) : st_client<Socket, Pool>(service_pump_) {}
	template<typename Arg>
	st_tcp_client_base(st_service_pump& service_pump_, Arg arg) : st_client<Socket, Pool>(service_pump_, arg) {}

	//connected link size, may smaller than total object size(st_object_pool::size)
	size_t valid_size()
	{
		size_t size = 0;
		ST_THIS do_something_to_all([&size](typename Pool::object_ctype& item) {
			if (item->is_connected())
				++size;
		});
		return size;
	}

	using st_client<Socket, Pool>::add_client;
	typename Pool::object_type add_client()
	{
		auto client_ptr(ST_THIS create_object());
		return ST_THIS add_client(client_ptr) ? client_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_client(unsigned short port, const std::string& ip = SERVER_IP)
	{
		auto client_ptr(ST_THIS create_object());
		client_ptr->set_server_addr(port, ip);
		return ST_THIS add_client(client_ptr) ? client_ptr : typename Pool::object_type();
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
	//please call client_ptr's 'disconnect' 'force_close' or 'graceful_close' with true 'reconnect' directly.
	void disconnect(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->disconnect(false);}
	void disconnect(bool reconnect = false) {ST_THIS do_something_to_all([=](typename Pool::object_ctype& item) {item->disconnect(reconnect);});}
	void force_close(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->force_close(false);}
	void force_close(bool reconnect = false) {ST_THIS do_something_to_all([=](typename Pool::object_ctype& item) {item->force_close(reconnect);});}
	void graceful_close(typename Pool::object_ctype& client_ptr, bool sync = true) {ST_THIS del_object(client_ptr); client_ptr->graceful_close(false, sync);}
	void graceful_close(bool reconnect = false, bool sync = true) {ST_THIS do_something_to_all([=](typename Pool::object_ctype& item) {item->graceful_close(reconnect, sync);});}

protected:
	virtual void uninit() {ST_THIS stop(); graceful_close();}
};
typedef st_tcp_client_base<> st_tcp_client;

} //namespace

#endif /* ST_ASIO_WRAPPER_TCP_CLIENT_H_ */

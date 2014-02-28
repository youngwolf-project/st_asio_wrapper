/*
 * st_asio_wrapper_tcp_client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at tcp client endpoint
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
		ST_THIS do_something_to_all([&](typename st_tcp_client_base::object_ctype& item) {
			if (item->is_connected())
				++size;
		});
		return size;
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

protected:
	virtual void uninit()
		{ST_THIS stop(); ST_THIS do_something_to_all(boost::bind(&Socket::graceful_close, _1, false));}
};
typedef st_tcp_client_base<> st_tcp_client;

} //namespace

#endif /* ST_ASIO_WRAPPER_TCP_CLIENT_H_ */

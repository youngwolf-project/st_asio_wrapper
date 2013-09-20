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

template<typename Socket = st_connector>
class st_tcp_client_base : public st_client<Socket>
{
public:
	st_tcp_client_base(st_service_pump& service_pump_) : st_client<Socket>(service_pump_) {}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket's send buffer
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

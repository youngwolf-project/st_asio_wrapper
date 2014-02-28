/*
 * st_asio_wrapper_server_socket.h
 *
 *  Created on: 2013-4-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVER_SOCKET_H_
#define ST_ASIO_WRAPPER_SERVER_SOCKET_H_

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_tcp_socket.h"

namespace st_asio_wrapper
{

class i_server
{
public:
	virtual st_service_pump& get_service_pump() = 0;
	virtual const st_service_pump& get_service_pump() const = 0;
	virtual void del_client(const boost::shared_ptr<st_timer>& client_ptr) = 0;
};

template<typename Socket = boost::asio::ip::tcp::socket, typename Server = i_server>
SHARED_OBJECT_T(st_server_socket_base, st_tcp_socket_base, Socket, Server)
{
public:
	st_server_socket_base(Server& server_) : st_tcp_socket_base<Socket>(server_.get_service_pump()), server(server_) {}

	template<typename Arg>
	st_server_socket_base(Server& server_, Arg& arg) :
		st_tcp_socket_base<Socket>(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this st_server_socket_base when invoke it
	//notice, when resue this st_server_socket_base, st_object_pool will invoke reset(), child must re-write this
	//to init all member variables, and then do not forget to invoke st_server_socket_base::reset() to init father's
	//member variables
	virtual void reset() {st_tcp_socket_base<Socket>::reset();}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS get_io_service().stopped())
		{
			ST_THIS do_recv_msg();
			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); ST_THIS force_close();}
	//do not forget to force_close this st_tcp_socket_base<Socket>(in del_client(), there's a force_close() invocation)
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		ST_THIS show_info("client:", "quit.");
		ST_THIS force_close();
#else
		server.del_client(boost::dynamic_pointer_cast<st_timer>(ST_THIS shared_from_this()));
#endif
	}

protected:
	Server& server;
};
typedef st_server_socket_base<> st_server_socket;

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_SOCKET_H_ */

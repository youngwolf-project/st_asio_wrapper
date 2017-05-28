/*
 * st_asio_wrapper_udp_client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef ST_ASIO_WRAPPER_UDP_CLIENT_H_
#define ST_ASIO_WRAPPER_UDP_CLIENT_H_

#include "st_asio_wrapper_client.h"

namespace st_asio_wrapper
{

template<typename Socket> class st_udp_sclient_base : public st_sclient<Socket>
{
public:
	st_udp_sclient_base(st_service_pump& service_pump_) : st_sclient<Socket>(service_pump_) {}
};

template<typename Socket, typename Pool = st_object_pool<Socket> >
class st_udp_client_base : public st_client<Socket, Pool>
{
private:
	typedef st_client<Socket, Pool> super;

public:
	st_udp_client_base(st_service_pump& service_pump_) : super(service_pump_) {}

	using super::add_socket;
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = std::string())
	{
		BOOST_AUTO(socket_ptr, ST_THIS create_object());
		socket_ptr->set_local_addr(port, ip);
		return ST_THIS add_socket(socket_ptr) ? socket_ptr : typename Pool::object_type();
	}

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function
	void disconnect(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::disconnect));}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::force_shutdown));}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->graceful_shutdown();}
	void graceful_shutdown() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::graceful_shutdown));}

protected:
	virtual void uninit() {ST_THIS stop(); graceful_shutdown();}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UDP_CLIENT_H_ */

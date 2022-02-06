/*
 * socket_service.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP socket service
 */

#ifndef ST_ASIO_UDP_SOCKET_SERVICE_H_
#define ST_ASIO_UDP_SOCKET_SERVICE_H_

#include "../socket_service.h"

namespace st_asio_wrapper { namespace udp {

template<typename Socket> class single_socket_service_base : public single_socket_service<Socket>
{
public:
	single_socket_service_base(service_pump& service_pump_) : single_socket_service<Socket>(service_pump_) {}
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix>
class multi_socket_service_base : public multi_socket_service<Socket, Pool, Matrix>
{
private:
	typedef multi_socket_service<Socket, Pool, Matrix> super;

public:
	multi_socket_service_base(service_pump& service_pump_) : super(service_pump_) {}

	using super::add_socket;
	typename Pool::object_type add_socket(unsigned short port, const std::string& ip = std::string(), unsigned additional_io_context_refs = 0)
	{
		BOOST_AUTO(socket_ptr, ST_THIS create_object());
		if (!socket_ptr)
			return socket_ptr;

		socket_ptr->set_local_addr(port, ip);
		return add_socket(socket_ptr, additional_io_context_refs) ? socket_ptr : typename Pool::object_type();
	}
	typename Pool::object_type add_socket(unsigned short port, unsigned short peer_port, const std::string& ip = std::string(), const std::string& peer_ip = std::string(),
		unsigned additional_io_context_refs = 0)
	{
		BOOST_AUTO(socket_ptr, ST_THIS create_object());
		if (!socket_ptr)
			return socket_ptr;

		socket_ptr->set_local_addr(port, ip);
		socket_ptr->set_peer_addr(peer_port, peer_ip);
		return add_socket(socket_ptr, additional_io_context_refs) ? socket_ptr : typename Pool::object_type();
	}

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function
	void disconnect(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::disconnect));}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::force_shutdown));}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->graceful_shutdown();}
	void graceful_shutdown() {ST_THIS do_something_to_all(boost::mem_fn(&Socket::graceful_shutdown));}

protected:
	virtual void uninit() {ST_THIS stop(); force_shutdown();}
};

}} //namespace

#endif /* ST_ASIO_UDP_SOCKET_SERVICE_H_ */

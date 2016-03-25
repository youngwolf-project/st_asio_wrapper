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

#include "st_asio_wrapper_udp_socket.h"
#include "st_asio_wrapper_client.h"

namespace st_asio_wrapper
{

typedef st_sclient<st_udp_socket> st_udp_sclient;

template<typename Socket = st_udp_socket, typename Pool = st_object_pool<Socket>>
class st_udp_client_base : public st_client<Socket, Pool>
{
public:
	st_udp_client_base(st_service_pump& service_pump_) : st_client<Socket, Pool>(service_pump_) {}

	using st_client<Socket, Pool>::add_client;
	typename Pool::object_type add_client(unsigned short port, const std::string& ip = std::string())
	{
		auto client_ptr(ST_THIS create_object());
		client_ptr->set_local_addr(port, ip);
		return ST_THIS add_client(client_ptr) ? client_ptr : typename Pool::object_type();
	}

	//functions with a client_ptr parameter will remove the link from object pool first, then call corresponding function
	void disconnect(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->disconnect();}
	void disconnect() {ST_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->disconnect();});}
	void force_close(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->force_close();}
	void force_close() {ST_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->force_close();});}
	void graceful_close(typename Pool::object_ctype& client_ptr) {ST_THIS del_object(client_ptr); client_ptr->graceful_close();}
	void graceful_close() {ST_THIS do_something_to_all([](typename Pool::object_ctype& item) {item->graceful_close();});}

protected:
	virtual void uninit() {ST_THIS stop(); graceful_close();}
};
typedef st_udp_client_base<> st_udp_client;

} //namespace

#endif /* ST_ASIO_WRAPPER_TEST_CLIENT_H_ */

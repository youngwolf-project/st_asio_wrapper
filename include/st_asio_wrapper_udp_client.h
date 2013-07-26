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

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_udp_socket.h"

namespace st_asio_wrapper
{

//only support one udp socket
template<typename Socket = st_udp_socket>
class st_sudp_client_base : public st_service_pump::i_service, public Socket
{
public:
	st_sudp_client_base(st_service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_),
		service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init() {Socket::reset(); Socket::start(); Socket::send_msg();}
	virtual void uninit() {Socket::graceful_close(); Socket::direct_dispatch_all_msg();}

protected:
	st_service_pump& service_pump;
};
typedef st_sudp_client_base<> st_sudp_client;

template<typename Socket = st_udp_socket>
class st_udp_client_base : public st_service_pump::i_service
{
public:
	st_udp_client_base(st_service_pump& service_pump_) : i_service(service_pump_), service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init()
	{
		do_something_to_all(boost::mem_fn(&Socket::reset));
		do_something_to_all(boost::mem_fn(&Socket::start));
		do_something_to_all(boost::mem_fn((bool (Socket::*)()) &Socket::send_msg));
	}
	virtual void uninit()
	{
		do_something_to_all(boost::mem_fn(&Socket::graceful_close));
		do_something_to_all(boost::mem_fn(&Socket::direct_dispatch_all_msg));
	}

	//not protected by mutex, please note
	DO_SOMETHING_TO_ALL(client_can)
	DO_SOMETHING_TO_ONE(client_can)

	void add_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		assert(client_ptr && &client_ptr->get_io_service() == &service_pump);
		mutex::scoped_lock lock(client_can_mutex);
		client_can.push_back(client_ptr);
		if (service_pump.is_service_started()) //service already started
		{
			client_ptr->reset();
			client_ptr->start();
		}
	}

	boost::shared_ptr<Socket> add_client(unsigned short port, const std::string& ip = std::string())
	{
		auto client_ptr(boost::make_shared<Socket>(get_service_pump()));
		client_ptr->set_local_addr(port, ip);
		add_client(client_ptr);

		return client_ptr;
	}

	boost::shared_ptr<Socket> add_client()
	{
		auto client_ptr(boost::make_shared<Socket>(get_service_pump()));
		add_client(client_ptr);

		return client_ptr;
	}

	void del_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		mutex::scoped_lock lock(client_can_mutex);
		//client_can does not contain any duplicate items
		client_can.remove(client_ptr);
	}

	void del_all_client()
	{
		mutex::scoped_lock lock(client_can_mutex);
		client_can.clear();
	}

	//not protected by mutex, please note
	bool send_msg(const udp::endpoint& peer_addr, const std::string& str)
		{return client_can.empty() ? false : client_can.front()->send_msg(peer_addr, str);}
	bool send_native_msg(const udp::endpoint& peer_addr, const std::string& str)
		{return client_can.empty() ? false : client_can.front()->send_native_msg(peer_addr, str);}

protected:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<Socket>> client_can;
	mutex client_can_mutex;

	st_service_pump& service_pump;
};
typedef st_udp_client_base<> st_udp_client;

} //namespace

#endif /* ST_ASIO_WRAPPER_TEST_CLIENT_H_ */

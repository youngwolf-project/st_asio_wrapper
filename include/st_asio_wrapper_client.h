/*
 * st_asio_wrapper_client.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_WRAPPER_CLIENT_H_
#define ST_ASIO_WRAPPER_CLIENT_H_

#include "st_asio_wrapper_object_pool.h"

namespace st_asio_wrapper
{

//only support one link
template<typename Socket>
class st_sclient : public st_service_pump::i_service, public Socket
{
public:
	st_sclient(st_service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_) {}

	virtual void init() {ST_THIS reset(); ST_THIS start(); ST_THIS send_msg();}
	virtual void uninit() {ST_THIS graceful_close();}
};

template<typename Socket>
class st_client : public st_object_pool<Socket>
{
protected:
	st_client(st_service_pump& service_pump_) : st_object_pool<Socket>(service_pump_) {}

public:
	virtual void init()
	{
		ST_THIS do_something_to_all(boost::mem_fn(&Socket::reset));
		ST_THIS do_something_to_all(boost::mem_fn(&Socket::start));
		ST_THIS do_something_to_all(boost::mem_fn((bool (Socket::*)()) &Socket::send_msg));

		ST_THIS start();
	}

	bool add_client(const boost::shared_ptr<Socket>& client_ptr, bool reset = true)
	{
		if (ST_THIS add_object(client_ptr))
		{
			if (ST_THIS get_service_pump().is_service_started()) //service already started
			{
				if (reset)
					client_ptr->reset();
				client_ptr->start();
			}

			return true;
		}

		return false;
	}

	boost::shared_ptr<Socket> add_client(unsigned short port, const std::string& ip = std::string())
	{
		auto client_ptr(create_client());
		client_ptr->set_server_addr(port, ip);
		return add_client(client_ptr) ? client_ptr : boost::shared_ptr<Socket>();
	}

	//this method only used with st_tcp_socket and it's derived class
	//if you need to change the server address, please use create_client() to create a client, then,
	//set the server address, finally, invoke bool add_client(const boost::shared_ptr<Socket>&, bool)
	boost::shared_ptr<Socket> add_client()
	{
		auto client_ptr(create_client());
		return add_client(client_ptr) ? client_ptr : boost::shared_ptr<Socket>();
	}

	//this method simply create a class derived from st_socket from heap, secondly you must invoke
	//bool add_client(const boost::shared_ptr<Socket>&, bool) before this socket can send or recv msgs.
	//for st_udp_socket, you also need to invoke set_local_addr() before add_client(), please note
	boost::shared_ptr<Socket> create_client()
	{
		auto client_ptr(ST_THIS reuse_object());
		return client_ptr ? client_ptr : boost::make_shared<Socket>(ST_THIS get_service_pump());
	}

	void disconnect(const boost::shared_ptr<Socket>& client_ptr) {force_close(client_ptr);}
	void force_close(const boost::shared_ptr<Socket>& client_ptr)
		{if (ST_THIS del_object(client_ptr)) client_ptr->force_close();}
	void graceful_close(const boost::shared_ptr<Socket>& client_ptr)
		{if (ST_THIS del_object(client_ptr)) client_ptr->graceful_close();}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CLIENT_H_ */

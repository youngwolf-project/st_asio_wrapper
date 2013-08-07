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

#include <boost/smart_ptr.hpp>

#include "st_asio_wrapper_service_pump.h"

namespace st_asio_wrapper
{

//only support one link
template<typename Socket>
class st_sclient_base : public st_service_pump::i_service, public Socket
{
public:
	st_sclient_base(st_service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_) {}

	virtual void init() {this->reset(); this->start(); this->send_msg();}
	virtual void uninit() {this->graceful_close(); this->direct_dispatch_all_msg();}
};

template<typename Socket>
class st_client_base : public st_service_pump::i_service
{
public:
	st_client_base(st_service_pump& service_pump_) : i_service(service_pump_) {}

	virtual void init()
	{
		do_something_to_all(boost::mem_fn(&Socket::reset));
		do_something_to_all(boost::mem_fn(&Socket::start));
		do_something_to_all(boost::mem_fn((bool (Socket::*)()) &Socket::send_msg));
	}
	virtual void uninit() {do_something_to_all(boost::mem_fn(&Socket::direct_dispatch_all_msg));}

	//not protected by mutex, please note
	DO_SOMETHING_TO_ALL(client_can)
	DO_SOMETHING_TO_ONE(client_can)

	void add_client(const boost::shared_ptr<Socket>& client_ptr, bool reset = true)
	{
		assert(client_ptr && &client_ptr->get_io_service() == &service_pump);
		mutex::scoped_lock lock(client_can_mutex);
		client_can.push_back(client_ptr);
		if (service_pump.is_service_started()) //service already started
		{
			if (reset)
				client_ptr->reset();
			client_ptr->start();
		}
	}

	boost::shared_ptr<Socket> add_client(unsigned short port, const std::string& ip = std::string())
	{
		BOOST_AUTO(client_ptr, boost::make_shared<Socket>(boost::ref(get_service_pump())));
		client_ptr->set_server_addr(port, ip);
		add_client(client_ptr);

		return client_ptr;
	}

	//this method only used with st_tcp_socket and it's derived class
	boost::shared_ptr<Socket> add_client()
	{
		BOOST_AUTO(client_ptr, boost::make_shared<Socket>(boost::ref(get_service_pump())));
		add_client(client_ptr);

		return client_ptr;
	}

	//this method only used with st_udp_socket and it's derived class, it simply create a st_udp_socket or
	//a derived class from heap, secondly you must invoke set_local_addr() and add_client() before this
	//udp socket can send or recv msgs.
	boost::shared_ptr<Socket> create_client() {return boost::make_shared<Socket>(boost::ref(get_service_pump()));}

	//please carefully invode the following two methods,
	//be ensure that there's no any operations performed on this socket when invoke them
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

	size_t size()
	{
		mutex::scoped_lock lock(client_can_mutex);
		return client_can.size();
	}

	//not protected by mutex, please notice.
	boost::shared_ptr<Socket> at(size_t index)
	{
		assert(index < client_can.size());
		return index < client_can.size() ?
			*(boost::next(client_can.begin(), index)) : boost::shared_ptr<Socket>();
	}

	boost::shared_ptr<const Socket> at(size_t index) const
	{
		assert(index < client_can.size());
		return index < client_can.size() ?
			*(boost::next(client_can.begin(), index)) : boost::shared_ptr<const Socket>();
	}

protected:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<Socket> > client_can;
	mutex client_can_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CLIENT_H_ */

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

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_connector.h"

namespace st_asio_wrapper
{

//only support one link
template<typename Socket = st_connector>
class st_sclient_base : public st_service_pump::i_service, public Socket
{
public:
	st_sclient_base(st_service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_),
		service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init() {Socket::reset(); Socket::start();}
	virtual void uninit()
	{
		//graceful closing can prevent reconnecting using is_closing()
		//if you use force_close() to stop service, please use reconnecting control(RE_CONNECT_CONTROL macro)
		//to prevent reconnecting, like this:
		//Socket::set_re_connect_times(0);
		//Socket::force_close();

		//reconnecting occured in on_recv_error(), we must guarantee that it's not because of stopping service
		//that made recv error, and then proceed to reconnect.
		Socket::graceful_close();
		Socket::direct_dispatch_all_msg();
	}

protected:
	st_service_pump& service_pump;
};
typedef st_sclient_base<> st_sclient;

template<typename Socket = st_connector>
class st_client_base : public st_service_pump::i_service
{
public:
	st_client_base(st_service_pump& service_pump_) : i_service(service_pump_), service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init()
	{
		do_something_to_all(boost::mem_fn(&Socket::reset));
		do_something_to_all(boost::mem_fn(&Socket::start));
	}
	virtual void uninit()
	{
<<<<<<< HEAD
<<<<<<< HEAD
=======
		//graceful closing can prevent reconnecting using is_closing()
		//if you use force_close() to stop service, please use reconnecting control(RE_CONNECT_CONTROL macro)
		//to prevent reconnecting, like this:
		//do_something_to_all(boost::bind(&Socket::set_re_connect_times, _1, 0));
		//do_something_to_all(boost::mem_fn(&Socket::force_close));

		//reconnecting occured in on_recv_error(), we must guarantee that it's not because of stopping service
		//that made recv error, and then proceed to reconnect.
>>>>>>> parent of 7f18591... removed some incorrect comments.
		do_something_to_all(boost::mem_fn(&Socket::graceful_close));
=======
		//graceful closing can prevent reconnecting using is_closing()
		//if you use force_close() to stop service, please use reconnecting control(RE_CONNECT_CONTROL macro)
		//to prevent reconnecting, like this:
		//do_something_to_all(boost::bind(&Socket::set_re_connect_times, _1, 0));
		//do_something_to_all(boost::mem_fn(&Socket::force_close));

		//reconnecting occured in on_recv_error(), we must guarantee that it's not because of stopping service
		//that made recv error, and then proceed to reconnect.
		do_something_to_all(boost::bind(&Socket::graceful_close, _1, false));
>>>>>>> 1312ea1179a1fb5204be533d473a899efcc48b8f
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
			client_ptr->start();
	}

	boost::shared_ptr<Socket> add_client(unsigned short port, const std::string& ip = std::string())
	{
		auto client_ptr(boost::make_shared<Socket>(get_service_pump()));
		client_ptr->set_server_addr(port, ip);
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

	size_t size()
	{
		mutex::scoped_lock lock(client_can_mutex);
		return client_can.size();
	}

	//not protected by mutex, please notice.
	boost::shared_ptr<Socket> at(size_t index)
	{
		assert(index < client_can.size());
		return index < client_can.size() ? *(std::next(std::begin(client_can), index)) : boost::shared_ptr<Socket>();
	}

	boost::shared_ptr<const Socket> at(size_t index) const
	{
		assert(index < client_can.size());
		return index < client_can.size() ? *(std::next(std::begin(client_can), index)) : boost::shared_ptr<const Socket>();
	}

	///////////////////////////////////////////////////
	//msg sending interface
	BROADCAST_MSG(broadcast_msg, send_msg)
	BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<Socket>> client_can;
	mutex client_can_mutex;

	st_service_pump& service_pump;
};
typedef st_client_base<> st_client;

} //namespace

#endif /* ST_ASIO_WRAPPER_CLIENT_H_ */

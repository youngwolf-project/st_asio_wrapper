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

///////////////////////////////////////////////////
//msg sending interface
#define BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{do_something_to_all(boost::bind(&st_socket::SEND_FUNNAME, _1, pstr, len, num, can_overflow));} \
SEND_MSG_CALL_SWITCH(FUNNAME, void)
//msg sending interface
///////////////////////////////////////////////////

namespace st_asio_wrapper
{

//only support one link
class st_sclient : public st_service_pump::i_service, public st_connector
{
public:
	st_sclient(st_service_pump& service_pump_) : i_service(service_pump_), st_connector(service_pump_),
		service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init() {reset(); start();}
	virtual void uninit()
	{
		//graceful closing can prevent reconnecting using is_closing()
		//if you use force_close() to stop service, please use reconnecting control(RE_CONNECT_CONTROL macro)
		//to prevent reconnecting, like this:
		//set_re_connect_times(0);
		//force_close();

		//reconnecting occured in on_recv_error(), we must guarantee that it's not because of stopping service
		//that made recv error, and then proceed to reconnect.
		graceful_close();
		direct_dispatch_all_msg();
	}

protected:
	st_service_pump& service_pump;
};

class st_client : public st_service_pump::i_service
{
public:
	st_client(st_service_pump& service_pump_) : i_service(service_pump_), service_pump(service_pump_) {}
	st_service_pump& get_service_pump() {return service_pump;}
	const st_service_pump& get_service_pump() const {return service_pump;}

	virtual void init()
	{
		do_something_to_all(boost::mem_fn(&st_connector::reset));
		do_something_to_all(boost::mem_fn(&st_connector::start));
	}
	virtual void uninit()
	{
		//graceful closing can prevent reconnecting using is_closing()
		//if you use force_close() to stop service, please use reconnecting control(RE_CONNECT_CONTROL macro)
		//to prevent reconnecting, like this:
		//do_something_to_all(boost::bind(&st_connector::set_re_connect_times, _1, 0));
		//do_something_to_all(boost::mem_fn(&st_connector::force_close));

		//reconnecting occured in on_recv_error(), we must guarantee that it's not because of stopping service
		//that made recv error, and then proceed to reconnect.
		do_something_to_all(boost::mem_fn(&st_connector::graceful_close));
		do_something_to_all(boost::mem_fn(&st_connector::direct_dispatch_all_msg));
	}

	//not protected by mutex, please note
	DO_SOMETHING_TO_ALL(client_can)
	DO_SOMETHING_TO_ONE(client_can)

	void add_client(const boost::shared_ptr<st_connector>& client_ptr)
	{
		assert(client_ptr && &client_ptr->get_io_service() == &service_pump);
		mutex::scoped_lock lock(client_can_mutex);
		client_can.push_back(client_ptr);
		if (service_pump.is_service_started()) //service already started
			client_ptr->start();
	}

	void del_client(const boost::shared_ptr<st_connector>& client_ptr)
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

	///////////////////////////////////////////////////
	//msg sending interface
	BROADCAST_MSG(broadcast_msg, send_msg)
	BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<st_connector> > client_can;
	mutex client_can_mutex;

	st_service_pump& service_pump;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CLIENT_H_ */

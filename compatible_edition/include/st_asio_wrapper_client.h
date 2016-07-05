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
	using Socket::TIMER_BEGIN;
	using Socket::TIMER_END;

	st_sclient(st_service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_) {}
	template<typename Arg>
	st_sclient(st_service_pump& service_pump_, Arg& arg) : i_service(service_pump_), Socket(service_pump_, arg) {}

protected:
	virtual bool init() {ST_THIS reset(); ST_THIS start(); return Socket::started();}
	virtual void uninit() {ST_THIS graceful_close();}
};

template<typename Socket, typename Pool>
class st_client : public Pool
{
protected:
	using Pool::TIMER_BEGIN;
	using Pool::TIMER_END;

	st_client(st_service_pump& service_pump_) : Pool(service_pump_) {}
	template<typename Arg>
	st_client(st_service_pump& service_pump_, Arg arg) : Pool(service_pump_, arg) {}

	virtual bool init()
	{
		ST_THIS do_something_to_all(boost::mem_fn(&Socket::reset));
		ST_THIS do_something_to_all(boost::mem_fn(&Socket::start));
		ST_THIS start();
		return true;
	}

public:
	//parameter reset valid only if the service pump already started, or service pump will call object pool's init function before start service pump
	bool add_client(typename Pool::object_ctype& client_ptr, bool reset = true)
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

	//unseal object creation for client endpoint object pool.
	using Pool::create_object;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CLIENT_H_ */

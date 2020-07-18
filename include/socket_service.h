/*
 * socket_service.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_SOCKET_SERVICE_H_
#define ST_ASIO_SOCKET_SERVICE_H_

#include "object_pool.h"

namespace st_asio_wrapper
{

//only support one socket
template<typename Socket>
class single_socket_service : public service_pump::i_service, public Socket
{
public:
	single_socket_service(service_pump& service_pump_) : i_service(service_pump_), Socket(service_pump_) {}
	template<typename Arg> single_socket_service(service_pump& service_pump_, Arg& arg) : i_service(service_pump_), Socket(service_pump_, arg) {}

	using Socket::id; //release these functions

protected:
	virtual bool init() {ST_THIS start(); return Socket::started();}
	virtual void uninit() {ST_THIS graceful_shutdown();} //if you wanna force shutdown, call force_shutdown before service_pump::stop_service invocation.

private:
	using Socket::get_matrix; //hide this function
};

template<typename Socket, typename Pool, typename Matrix>
class multi_socket_service : public Matrix, public Pool
{
protected:
	multi_socket_service(service_pump& service_pump_) : Pool(service_pump_) {}
	template<typename Arg> multi_socket_service(service_pump& service_pump_, const Arg& arg) : Pool(service_pump_, arg) {}

	virtual bool init()
	{
		ST_THIS do_something_to_all(boost::lambda::bind(&Socket::start, *boost::lambda::_1));
		ST_THIS start();
		return true;
	}

public:
	//implement i_matrix's pure virtual functions
	virtual bool started() const {return ST_THIS service_started();}
	virtual service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const service_pump& get_service_pump() const {return Pool::get_service_pump();}
	virtual bool socket_exist(boost::uint_fast64_t id) {return ST_THIS exist(id);}
	virtual boost::shared_ptr<tracked_executor> find_socket(boost::uint_fast64_t id) {return ST_THIS find(id);}

	typename Pool::object_type create_object() {return Pool::create_object(boost::ref(*this));}
	template<typename Arg> typename Pool::object_type create_object(Arg& arg) {return Pool::create_object(boost::ref(*this), arg);}

	//parameter reset valid only if the service pump already started, or service pump will call object pool's init function before start service pump
	bool add_socket(typename Pool::object_ctype& socket_ptr)
	{
		if (ST_THIS add_object(socket_ptr))
		{
			if (get_service_pump().is_service_started()) //service already started
				socket_ptr->start();

			return true;
		}

		return false;
	}
};

} //namespace

#endif /* ST_ASIO_SOCKET_SERVICE_H_ */

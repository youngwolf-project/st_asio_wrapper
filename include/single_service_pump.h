/*
 * single_service_pump.h
 *
 *  Created on: 2019-5-17
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * one service_pump for one service.
 */

#ifndef _ST_ASIO_SINGLE_SERVICE_PUMP_H_
#define _ST_ASIO_SINGLE_SERVICE_PUMP_H_

#include "service_pump.h"

namespace st_asio_wrapper
{

template<typename Service> class single_service_pump : public service_pump, public Service
{

public:
	using service_pump::start_service;
	using service_pump::stop_service;

public:
#if BOOST_ASIO_VERSION >= 101200
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	single_service_pump(int concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE) : service_pump(concurrency_hint), Service(boost::ref(*(service_pump*) this)) {}
	template<typename Arg> single_service_pump(Arg& arg, int concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE) :
		service_pump(concurrency_hint), Service(boost::ref(*(service_pump*) this), arg) {}
#else
	//single_service_pump always think it's using multiple io_context
	single_service_pump(int concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE) : service_pump(concurrency_hint, true), Service(boost::ref(*(service_pump*) this)) {}
	template<typename Arg> single_service_pump(Arg& arg, int concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE) :
		service_pump(concurrency_hint, true), Service(boost::ref(*(service_pump*) this), arg) {}
#endif
#else
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	single_service_pump() : Service(boost::ref(*(service_pump*) this)) {}
	template<typename Arg> single_service_pump(Arg& arg) : Service(boost::ref(*(service_pump*) this), arg) {}
#else
	//single_service_pump always think it's using multiple io_context
	single_service_pump() : service_pump(true), Service(boost::ref(*(service_pump*) this)) {}
	template<typename Arg> single_service_pump(Arg& arg) : service_pump(true), Service(boost::ref(*(service_pump*) this), arg) {}
#endif
#endif
};

} //namespace

#endif /* _ST_ASIO_SINGLE_SERVICE_PUMP_H_ */

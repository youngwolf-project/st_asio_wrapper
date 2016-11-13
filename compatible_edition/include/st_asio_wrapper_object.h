/*
 * st_asio_wrapper_object.h
 *
 *  Created on: 2016-6-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef ST_ASIO_WRAPPER_OBJECT_H_
#define ST_ASIO_WRAPPER_OBJECT_H_

#include <boost/function.hpp>

#include "st_asio_wrapper_base.h"

namespace st_asio_wrapper
{

class st_object
{
protected:
	st_object(boost::asio::io_service& _io_service_) : io_service_(_io_service_) {reset();}
	virtual ~st_object() {}

public:
	bool stopped() const {return io_service_.stopped();}

#if 0 == ST_ASIO_DELAY_CLOSE
	void post(const boost::function<void()>& handler) {io_service_.post((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}

	typedef boost::function<void(const boost::system::error_code&)> handler_with_error;
	handler_with_error make_handler_error(const handler_with_error& handler) const {return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1));}

	typedef boost::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;
	handler_with_error_size make_handler_error_size(const handler_with_error_size& handler) const
		{return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1, boost::lambda::_2));}

	bool is_async_calling() const {return !async_call_indicator.unique();}
	bool is_last_async_call() const {return async_call_indicator.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	void reset() {async_call_indicator = boost::make_shared<char>('\0');}

protected:
	boost::shared_ptr<char> async_call_indicator;
#else
	template<typename F> void post(const F& handler) {io_service_.post(handler);}
	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

protected:
	void reset() {set_async_calling(false);}

protected:
	bool async_calling;
#endif

	boost::asio::io_service& io_service_;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_OBJECT_H_ */

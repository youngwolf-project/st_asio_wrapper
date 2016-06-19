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

#ifdef ST_ASIO_ENHANCED_STABILITY
	void post(const boost::function<void()>& handler) {io_service_.post(boost::bind(&st_object::post_handler, this, async_call_indicator, handler));}
	bool is_async_calling() const {return !async_call_indicator.unique();}

	boost::function<void(const boost::system::error_code&)> make_handler_error(const boost::function<void(const boost::system::error_code&)>& handler) const
		{return boost::bind(&st_object::error_handler, this, async_call_indicator, handler, boost::asio::placeholders::error);}

	boost::function<void(const boost::system::error_code&, size_t)> make_handler_error_size(const boost::function<void(const boost::system::error_code&, size_t)>& handler) const
		{return boost::bind(&st_object::error_size_handler, this, async_call_indicator, handler, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);}

protected:
	void reset() {async_call_indicator = boost::make_shared<char>('\0');}
	void post_handler(const boost::shared_ptr<char>& unused, const boost::function<void()>& handler) const {handler();}
	void error_handler(const boost::shared_ptr<char>& unused, const boost::function<void(const boost::system::error_code&)>& handler,
		const boost::system::error_code& ec) const {handler(ec);}
	void error_size_handler(const boost::shared_ptr<char>& unused, const boost::function<void(const boost::system::error_code&, size_t)>& handler,
		const boost::system::error_code& ec, size_t bytes_transferred) const {handler(ec, bytes_transferred);}

protected:
	boost::shared_ptr<char> async_call_indicator;
#else
	template<typename CompletionHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void())
	post(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {io_service_.post(handler);}
	bool is_async_calling() const {return false;}

	template<typename F>
	inline const F& make_handler_error(const F& f) const {return f;}

	template<typename F>
	inline const F& make_handler_error_size(const F& f) const {return f;}

protected:
	void reset() {}
#endif

protected:
	boost::asio::io_service& io_service_;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_OBJECT_H_ */


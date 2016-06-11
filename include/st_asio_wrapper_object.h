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
	boost::asio::io_service& get_io_service() {return io_service_;}
	const boost::asio::io_service& get_io_service() const {return io_service_;}

	template<typename CompletionHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ())
#ifdef ST_ASIO_ENHANCED_STABILITY
	post(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {auto unused(ST_THIS async_call_indicator); io_service_.post([=]() {handler();});}
	bool is_async_calling() const {return !async_call_indicator.unique();}

	std::function<void(const boost::system::error_code&)> make_handler_error(const std::function<void(const boost::system::error_code&)>& handler)
		{boost::shared_ptr<char>unused(async_call_indicator); return [=](const boost::system::error_code& ec) {handler(ec);};}

	std::function<void(const boost::system::error_code&, size_t)> make_handler_error_size(const std::function<void(const boost::system::error_code&, size_t)>& handler)
		{boost::shared_ptr<char>unused(async_call_indicator); return [=](const boost::system::error_code& ec, size_t bytes_transferred) {handler(ec, bytes_transferred);};}

protected:
	void reset() {async_call_indicator = boost::make_shared<char>('\0');}

protected:
	boost::shared_ptr<char> async_call_indicator;
#else
	post(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {io_service_.post(handler);}
	bool is_async_calling() const {return false;}

	template<typename F>
	const F& make_handler_error(const F& f) {return f;}

	template<typename F>
	const F& make_handler_error_size(const F& f) {return f;}

protected:
	void reset() {}
#endif

protected:
	boost::asio::io_service& io_service_;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_OBJECT_H_ */


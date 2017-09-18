/*
 * object.h
 *
 *  Created on: 2016-6-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef ST_ASIO_OBJECT_H_
#define ST_ASIO_OBJECT_H_

#include <boost/function.hpp>

#include "base.h"

namespace st_asio_wrapper
{

class object
{
protected:
	virtual ~object() {}

public:
	bool stopped() const {return io_context_.stopped();}

#if 0 == ST_ASIO_DELAY_CLOSE
	typedef boost::function<void(const boost::system::error_code&)> handler_with_error;
	typedef boost::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;

	#if BOOST_ASIO_VERSION >= 101100
	void post(const boost::function<void()>& handler) {boost::asio::post(io_context_, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void defer(const boost::function<void()>& handler) {boost::asio::defer(io_context_, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#else
	void post(const boost::function<void()>& handler) {io_context_.post((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#endif

	handler_with_error make_handler_error(const handler_with_error& handler) const {return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1));}
	handler_with_error_size make_handler_error_size(const handler_with_error_size& handler) const
		{return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1, boost::lambda::_2));}

	bool is_async_calling() const {return !async_call_indicator.unique();}
	bool is_last_async_call() const {return async_call_indicator.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	object(boost::asio::io_context& _io_context_) : async_call_indicator(boost::make_shared<char>('\0')), io_context_(_io_context_) {}
	boost::shared_ptr<char> async_call_indicator;
#else
	#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(const F& handler) {boost::asio::post(io_context_, handler);}
	template<typename F> void defer(const F& handler) {boost::asio::defer(io_context_, handler);}
	#else
	template<typename F> void post(const F& handler) {io_context_.post(handler);}
	#endif

	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

protected:
	object(boost::asio::io_context& _io_context_) : async_calling(false), io_context_(_io_context_) {}
	bool async_calling;
#endif

	boost::asio::io_context& io_context_;
};

} //namespace

#endif /* ST_ASIO_OBJECT_H_ */

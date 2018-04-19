/*
 * stracked_executor.h
 *
 *  Created on: 2018-4-19
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ST_ASIO_TRACKED_EXECUTOR_H_
#define _ST_ASIO_TRACKED_EXECUTOR_H_

#include "executor.h"

namespace st_asio_wrapper
{

#if 0 == ST_ASIO_DELAY_CLOSE
class tracked_executor : public executor
{
protected:
	tracked_executor(boost::asio::io_context& io_context_) : executor(io_context_), async_call_indicator(boost::make_shared<char>('\0')) {}

public:
	typedef boost::function<void(const boost::system::error_code&)> handler_with_error;
	typedef boost::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;

	#if BOOST_ASIO_VERSION >= 101100
	void post(const boost::function<void()>& handler) {boost::asio::post(io_context_, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void defer(const boost::function<void()>& handler) {boost::asio::defer(io_context_, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch(const boost::function<void()>& handler) {boost::asio::dispatch(io_context_, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void post_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::post(strand, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void defer_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::defer(strand, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::dispatch(strand, (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#else
	void post(const boost::function<void()>& handler) {io_context_.post((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch(const boost::function<void()>& handler) {io_context_.dispatch((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void post_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{strand.post((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{strand.dispatch((async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#endif

	handler_with_error make_handler_error(const handler_with_error& handler) const {return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1));}
	handler_with_error_size make_handler_error_size(const handler_with_error_size& handler) const
		{return (async_call_indicator, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1, boost::lambda::_2));}

	bool is_async_calling() const {return !async_call_indicator.unique();}
	bool is_last_async_call() const {return async_call_indicator.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

private:
	boost::shared_ptr<char> async_call_indicator;
};
#else
class tracked_executor : public executor
{
protected:
	tracked_executor(boost::asio::io_context& io_context_) : executor(io_context_), async_calling(false) {}

public:
	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

private:
	bool async_calling;
};
#endif

} //namespace

#endif /* _ST_ASIO_TRACKED_EXECUTOR_H_ */

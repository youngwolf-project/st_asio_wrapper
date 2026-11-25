/*
 * executor.h
 *
 *  Created on: 2016-6-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ST_ASIO_EXECUTOR_H_
#define _ST_ASIO_EXECUTOR_H_

#include <boost/asio.hpp>

namespace st_asio_wrapper
{

class executor
{
protected:
	executor(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}
	virtual ~executor() {}

	template<typename F> inline const F& track_async_call(const F& handler) const {return handler;}
	template<typename F> inline const F& track_async_call_error(const F& handler) const {return track_async_call(handler);}
	template<typename F> inline const F& track_async_call_error_size(const F& handler) const {return track_async_call(handler);}

public:
	bool stopped() const {return io_context_.stopped();}

#if BOOST_ASIO_VERSION >= 101100
	template<typename F> void post(const F& handler) const {boost::asio::post(io_context_, handler);}
	template<typename F> void defer(const F& handler) const {boost::asio::defer(io_context_, handler);}
	template<typename F> void dispatch(const F& handler) const {boost::asio::dispatch(io_context_, handler);}
	template<typename F> void post_strand(const boost::asio::io_context::strand& strand, const F& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::post(strand, handler);}
	template<typename F> void defer_strand(const boost::asio::io_context::strand& strand, const F& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::defer(strand, handler);}
	template<typename F> void dispatch_strand(const boost::asio::io_context::strand& strand, const F& handler) const
		{assert(&io_context_ == &strand.context()); boost::asio::dispatch(strand, handler);}
#else
	template<typename F> void post(const F& handler) {io_context_.post(handler);}
	template<typename F> void dispatch(const F& handler) {io_context_.dispatch(handler);}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, const F& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.post(handler);}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, const F& handler)
		{assert(&io_context_ == &strand.get_io_service()); strand.dispatch(handler);}
#endif

protected:
	boost::asio::io_context& io_context_;
};

} //namespace

#endif /* _ST_ASIO_EXECUTOR_H_ */

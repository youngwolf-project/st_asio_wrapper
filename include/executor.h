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

#include "config.h"

namespace st_asio_wrapper
{

class executor
{
protected:
	virtual ~executor() {}
	executor(boost::asio::io_context& _io_context_) : io_context_(_io_context_) {}

public:
	bool stopped() const {return io_context_.stopped();}

#if BOOST_ASIO_VERSION >= 101100
	template <typename F> inline static boost::asio::executor_binder<typename boost::asio::decay<F>::type, boost::asio::io_context::strand>
		make_strand(boost::asio::io_context::strand& strand, BOOST_ASIO_MOVE_ARG(F) f) {return boost::asio::bind_executor(strand, BOOST_ASIO_MOVE_CAST(F)(f));}

	template<typename F> void post(const F& handler) {boost::asio::post(io_context_, handler);}
	template<typename F> void defer(const F& handler) {boost::asio::defer(io_context_, handler);}
	template<typename F> void dispatch(const F& handler) {boost::asio::dispatch(io_context_, handler);}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, const F& handler) {boost::asio::post(strand, handler);}
	template<typename F> void defer_strand(boost::asio::io_context::strand& strand, const F& handler) {boost::asio::defer(strand, handler);}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, const F& handler) {boost::asio::dispatch(strand, handler);}
#else
	template <typename F> static boost::asio::detail::wrapped_handler<boost::asio::strand, F> make_strand(boost::asio::io_context::strand& strand, F f) {return strand.wrap(f);}

	template<typename F> void post(const F& handler) {io_context_.post(handler);}
	template<typename F> void dispatch(const F& handler) {io_context_.dispatch(handler);}
	template<typename F> void post_strand(boost::asio::io_context::strand& strand, const F& handler) {strand.post(handler);}
	template<typename F> void dispatch_strand(boost::asio::io_context::strand& strand, const F& handler) {strand.dispatch(handler);}
#endif

	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

protected:
	boost::asio::io_context& io_context_;
};

} //namespace

#endif /* _ST_ASIO_EXECUTOR_H_ */

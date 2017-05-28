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

#include "st_asio_wrapper_base.h"

namespace st_asio_wrapper
{

class st_object
{
protected:
	virtual ~st_object() {}

public:
	bool stopped() const {return io_service_.stopped();}

#if 0 == ST_ASIO_DELAY_CLOSE
	typedef std::function<void(const boost::system::error_code&)> handler_with_error;
	typedef std::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;

#if (defined(_MSC_VER) && _MSC_VER >= 1900) || (defined(__cplusplus) && __cplusplus > 201103L)
	template<typename F> void post(F&& handler) {io_service_.post([unused(this->async_call_indicator), handler(std::move(handler))]() {handler();});}
	template<typename F> void post(const F& handler) {io_service_.post([unused(this->async_call_indicator), handler]() {handler();});}

	template<typename F> handler_with_error make_handler_error(F&& handler) const {return [unused(this->async_call_indicator), handler(std::move(handler))](const auto& ec) {handler(ec);};}
	template<typename F> handler_with_error make_handler_error(const F& handler) const {return [unused(this->async_call_indicator), handler](const auto& ec) {handler(ec);};}

	template<typename F> handler_with_error_size make_handler_error_size(F&& handler) const
		{return [unused(this->async_call_indicator), handler(std::move(handler))](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{return [unused(this->async_call_indicator), handler](const auto& ec, auto bytes_transferred) {handler(ec, bytes_transferred);};}
#else
	template<typename F> void post(const F& handler) {auto unused(async_call_indicator); io_service_.post([=]() {handler();});}
	template<typename F> handler_with_error make_handler_error(const F& handler) const {auto unused(async_call_indicator); return [=](const boost::system::error_code& ec) {handler(ec);};}
	template<typename F> handler_with_error_size make_handler_error_size(const F& handler) const
		{auto unused(async_call_indicator); return [=](const boost::system::error_code& ec, size_t bytes_transferred) {handler(ec, bytes_transferred);};}
#endif

	bool is_async_calling() const {return !async_call_indicator.unique();}
	bool is_last_async_call() const {return async_call_indicator.use_count() <= 2;} //can only be called in callbacks
	inline void set_async_calling(bool) {}

protected:
	st_object(boost::asio::io_service& _io_service_) : async_call_indicator(boost::make_shared<char>('\0')), io_service_(_io_service_) {}
	boost::shared_ptr<char> async_call_indicator;
#else
	template<typename F> void post(F&& handler) {io_service_.post(std::move(handler));}
	template<typename F> void post(const F& handler) {io_service_.post(handler);}

	template<typename F> inline F&& make_handler_error(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error(const F& f) const {return f;}

	template<typename F> inline F&& make_handler_error_size(F&& f) const {return std::move(f);}
	template<typename F> inline const F& make_handler_error_size(const F& f) const {return f;}

	inline bool is_async_calling() const {return async_calling;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {async_calling = value;}

protected:
	st_object(boost::asio::io_service& _io_service_) : async_calling(false), io_service_(_io_service_) {}
	bool async_calling;
#endif

	boost::asio::io_service& io_service_;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_OBJECT_H_ */

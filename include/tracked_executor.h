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
class tracked_executor
{
protected:
	virtual ~tracked_executor() {}
	tracked_executor(boost::asio::io_context& _io_context_) : io_context_(_io_context_), aci(boost::make_shared<char>('\0')), aci_min_ref(3)
	{
#if defined(_MSC_VER) || (105500 <= BOOST_VERSION && BOOST_VERSION < 107000) || defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(__cplusplus) && __cplusplus >= 201103L
		aci_min_ref = 2;
#endif
	}

public:
	typedef boost::function<void(const boost::system::error_code&)> handler_with_error;
	typedef boost::function<void(const boost::system::error_code&, size_t)> handler_with_error_size;

	bool stopped() const {return io_context_.stopped();}

	#if BOOST_ASIO_VERSION >= 101100
	void post(const boost::function<void()>& handler) {boost::asio::post(io_context_, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void defer(const boost::function<void()>& handler) {boost::asio::defer(io_context_, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch(const boost::function<void()>& handler) {boost::asio::dispatch(io_context_, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void post_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::post(strand, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void defer_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::defer(strand, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{boost::asio::dispatch(strand, (aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#else
	void post(const boost::function<void()>& handler) {io_context_.post((aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch(const boost::function<void()>& handler) {io_context_.dispatch((aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void post_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{strand.post((aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	void dispatch_strand(boost::asio::io_context::strand& strand, const boost::function<void()>& handler)
		{strand.dispatch((aci, boost::lambda::bind(boost::lambda::unlambda(handler))));}
	#endif

	handler_with_error make_handler_error(const handler_with_error& handler) const {return (aci, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1));}
	handler_with_error_size make_handler_error_size(const handler_with_error_size& handler) const
		{return (aci, boost::lambda::bind(boost::lambda::unlambda(handler), boost::lambda::_1, boost::lambda::_2));}

	bool is_async_calling() const {return !aci.unique();}
	bool is_last_async_call() //can only be called in callbacks
	{
		long cur_ref = aci.use_count();
		if (cur_ref > aci_min_ref)
			return false;
		else if (cur_ref < aci_min_ref)
			printf("fault error, please contact the author immediately with these two number -- (%ld/%ld), and the version of the boost and the compiler.",
				cur_ref, aci_min_ref--);

		return true;
	}
	inline void set_async_calling(bool) {}

protected:
	boost::asio::io_context& io_context_;

private:
	boost::shared_ptr<char> aci; //asynchronous calling indicator
	long aci_min_ref;
};
#else
class tracked_executor : public executor
{
protected:
	tracked_executor(boost::asio::io_context& io_context_) : executor(io_context_), aci(false) {}

public:
	inline bool is_async_calling() const {return aci;}
	inline bool is_last_async_call() const {return true;}
	inline void set_async_calling(bool value) {aci = value;}

private:
	bool aci; //asynchronous calling indicator
};
#endif

} //namespace

#endif /* _ST_ASIO_TRACKED_EXECUTOR_H_ */

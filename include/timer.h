/*
 * timer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * timer base class
 */

#ifndef ST_ASIO_TIMER_H_
#define ST_ASIO_TIMER_H_

#ifdef ST_ASIO_USE_STEADY_TIMER
#include <boost/asio/steady_timer.hpp>
#elif defined(ST_ASIO_USE_SYSTEM_TIMER)
#include <boost/asio/system_timer.hpp>
#endif

#include "object.h"

//If you inherit a class from class X, your own timer ids must begin from X::TIMER_END
namespace st_asio_wrapper
{

//timers are identified by id.
//for the same timer in the same timer object, any manipulations are not thread safe, please pay special attention.
//to resolve this defect, we must add a mutex member variable to timer_info, it's not worth. otherwise, they are thread safe.
//
//suppose you have more than one service thread(see service_pump for service thread number controlling), then:
//for same timer object: same timer, on_timer is called in sequence
//for same timer object: distinct timer, on_timer is called concurrently
//for distinct timer object: on_timer is always called concurrently
class timer : public object
{
public:
#ifdef ST_ASIO_USE_STEADY_TIMER
	typedef boost::chrono::milliseconds milliseconds;
	typedef boost::asio::steady_timer timer_type;
#elif defined(ST_ASIO_USE_SYSTEM_TIMER)
	typedef boost::chrono::milliseconds milliseconds;
	typedef boost::asio::system_timer timer_type;
#else
	typedef boost::posix_time::milliseconds milliseconds;
	typedef boost::asio::deadline_timer timer_type;
#endif

	typedef unsigned char tid;
	static const tid TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status {TIMER_FAKE, TIMER_OK, TIMER_CANCELED};

		tid id;
		unsigned char seq;
		timer_status status;
		size_t interval_ms;
		boost::function<bool (tid)> call_back; //return true from call_back to continue the timer, or the timer will stop
		boost::shared_ptr<timer_type> timer;

		timer_info() : seq(-1), status(TIMER_FAKE), interval_ms(0) {}
	};

	typedef const timer_info timer_cinfo;
	typedef std::vector<timer_info> container_type;

	timer(boost::asio::io_context& io_context_) : object(io_context_), timer_can((tid) -1)
		{tid id = -1; do_something_to_all(boost::lambda::bind(&timer_info::id, boost::lambda::_1) = ++boost::lambda::var(id));}

	//after this call, call_back cannot be used again, please note.
	bool update_timer_info(tid id, size_t interval, boost::function<bool(tid)>& call_back, bool start = false)
	{
		timer_info& ti = timer_can[id];

		if (timer_info::TIMER_FAKE == ti.status)
			try {ti.timer = boost::make_shared<timer_type>(boost::ref(io_context_));}
			catch (const std::exception& e) {unified_out::error_out("cannot create timer %d (%s)", ti.id, e.what()); return false;}
		ti.status = timer_info::TIMER_OK;
		ti.interval_ms = interval;
		ti.call_back.swap(call_back);

		if (start)
			start_timer(ti);

		return true;
	}
	bool update_timer_info(tid id, size_t interval, const boost::function<bool (tid)>& call_back, bool start = false)
		{BOOST_AUTO(unused, call_back); return update_timer_info(id, interval, unused, start);}

	void change_timer_status(tid id, timer_info::timer_status status) {timer_can[id].status = status;}
	void change_timer_interval(tid id, size_t interval) {timer_can[id].interval_ms = interval;}

	//after this call, call_back cannot be used again, please note.
	void change_timer_call_back(tid id, boost::function<bool(tid)>& call_back) {timer_can[id].call_back.swap(call_back);}
	void change_timer_call_back(tid id, const boost::function<bool(tid)>& call_back) {BOOST_AUTO(unused, call_back); change_timer_call_back(id, unused);}

	//after this call, call_back cannot be used again, please note.
	bool set_timer(tid id, size_t interval, boost::function<bool(tid)>& call_back) {return update_timer_info(id, interval, call_back, true);}
	bool set_timer(tid id, size_t interval, const boost::function<bool(tid)>& call_back) {return update_timer_info(id, interval, call_back, true);}

	bool start_timer(tid id)
	{
		timer_info& ti = timer_can[id];

		if (timer_info::TIMER_FAKE == ti.status)
			return false;

		ti.status = timer_info::TIMER_OK;
		start_timer(ti); //if timer already started, this will cancel it first

		return true;
	}

	timer_info find_timer(tid id) const {return timer_can[id];}
	bool is_timer(tid id) const {return timer_info::TIMER_OK == timer_can[id].status;}
	void stop_timer(tid id) {stop_timer(timer_can[id]);}
	void stop_all_timer() {do_something_to_all(boost::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, _1));}
	void stop_all_timer(tid excepted_id)
	{
		do_something_to_all(boost::lambda::if_then(excepted_id != boost::lambda::bind(&timer_info::id, boost::lambda::_1),
			boost::lambda::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, boost::lambda::_1)));
	}

	DO_SOMETHING_TO_ALL(timer_can)
	DO_SOMETHING_TO_ONE(timer_can)

protected:
	void start_timer(timer_info& ti)
	{
		assert(timer_info::TIMER_OK == ti.status);

#if BOOST_ASIO_VERSION >= 101100 && (defined(ST_ASIO_USE_STEADY_TIMER) || defined(ST_ASIO_USE_SYSTEM_TIMER))
		ti.timer->expires_after(milliseconds(ti.interval_ms));
#else
		ti.timer->expires_from_now(milliseconds(ti.interval_ms));
#endif
		ti.timer->async_wait(make_handler_error(boost::bind(&timer::timer_handler, this, boost::asio::placeholders::error, boost::ref(ti), ++ti.seq)));
	}

	void stop_timer(timer_info& ti)
	{
		if (timer_info::TIMER_OK == ti.status) //enable stopping timers that has been stopped
		{
			try {ti.timer->cancel();} catch (const boost::system::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", ti.id, e.code().value(), e.what());}
			ti.status = timer_info::TIMER_CANCELED;
		}
	}

	void timer_handler(const boost::system::error_code& ec, timer_info& ti, unsigned char prev_seq)
	{
		//return true from call_back to continue the timer, or the timer will stop
		if (!ec && ti.call_back(ti.id) && timer_info::TIMER_OK == ti.status)
			start_timer(ti);
		else if (prev_seq == ti.seq) //exclude a particular situation--start the same timer in call_back and return false
			ti.status = timer_info::TIMER_CANCELED;
	}

protected:
	container_type timer_can;

private:
	using object::io_context_;
};

} //namespace

#endif /* ST_ASIO_TIMER_H_ */


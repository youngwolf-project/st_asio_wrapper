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

#include "base.h"

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
template<typename Executor>
class timer : public Executor
{
public:
#if defined(ST_ASIO_USE_STEADY_TIMER) || defined(ST_ASIO_USE_SYSTEM_TIMER)
	#ifdef BOOST_ASIO_HAS_STD_CHRONO
		typedef std::chrono::milliseconds milliseconds;
	#else
		typedef boost::chrono::milliseconds milliseconds;
	#endif

	#ifdef ST_ASIO_USE_STEADY_TIMER
		typedef boost::asio::steady_timer timer_type;
	#else
		typedef boost::asio::system_timer timer_type;
	#endif
#else
	typedef boost::posix_time::milliseconds milliseconds;
	typedef boost::asio::deadline_timer timer_type;
#endif

	typedef unsigned short tid;
	static const tid TIMER_END = 0; //subclass' id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status {TIMER_CREATED, TIMER_STARTED, TIMER_CANCELED};

		tid id;
		unsigned char seq;
		timer_status status;
		unsigned interval_ms;
		timer_type timer;
		boost::function<bool (tid)> call_back; //return true from call_back to continue the timer, or the timer will stop

		timer_info(tid id_, boost::asio::io_context& io_context_) : id(id_), seq(-1), status(TIMER_CREATED), interval_ms(0), timer(io_context_) {}
		bool operator ==(const timer_info& other) {return id == other.id;}
		bool operator ==(tid id_) {return id == id_;}
	};
	typedef const timer_info timer_cinfo;

	timer(boost::asio::io_context& io_context_) : Executor(io_context_) {}
	~timer() {stop_all_timer();}

	//after this call, call_back cannot be used again, please note.
	bool create_or_update_timer(tid id, unsigned interval, boost::function<bool(tid)>& call_back, bool start = false)
	{
		timer_info* ti = NULL;
		{
			boost::lock_guard<boost::mutex> lock(timer_can_mutex);
			BOOST_AUTO(iter, std::find(timer_can.begin(), timer_can.end(), id));
			if (iter == timer_can.end())
			{
				try {timer_can.emplace_back(id, boost::ref(io_context_)); ti = &timer_can.back();}
				catch (const std::exception& e) {unified_out::error_out("cannot create timer %d (%s)", id, e.what()); return false;}
			}
			else
				ti = &*iter;
		}
		assert(NULL != ti);

		ti->interval_ms = interval;
		ti->call_back.swap(call_back);

		if (start)
			start_timer(*ti);

		return true;
	}
	bool create_or_update_timer(tid id, unsigned interval, const boost::function<bool (tid)>& call_back, bool start = false)
		{BOOST_AUTO(unused, call_back); return create_or_update_timer(id, interval, unused, start);}

	bool change_timer_status(tid id, typename timer_info::timer_status status) {BOOST_AUTO(ti, find_timer(id)); return NULL != ti ? ti->status = status, true : false;}
	bool change_timer_interval(tid id, size_t interval) {BOOST_AUTO(ti, find_timer(id)); return NULL != ti ? ti->interval_ms = interval, true : false;}

	//after this call, call_back cannot be used again, please note.
	bool change_timer_call_back(tid id, boost::function<bool(tid)>& call_back) {BOOST_AUTO(ti, find_timer(id)); return NULL != ti ? ti->call_back.swap(call_back), true : false; }
	bool change_timer_call_back(tid id, const boost::function<bool(tid)>& call_back) {BOOST_AUTO(unused, call_back); return change_timer_call_back(id, unused);}

	//after this call, call_back cannot be used again, please note.
	bool set_timer(tid id, unsigned interval, boost::function<bool(tid)>& call_back) {return create_or_update_timer(id, interval, call_back, true);}
	bool set_timer(tid id, unsigned interval, const boost::function<bool(tid)>& call_back) {return create_or_update_timer(id, interval, call_back, true);}

	timer_info* find_timer(tid id)
	{
		boost::lock_guard<boost::mutex> lock(timer_can_mutex);
		BOOST_AUTO(iter, std::find(timer_can.begin(), timer_can.end(), id));
		if (iter != timer_can.end())
			return &*iter;

		return NULL;
	}

	bool is_timer(tid id) {BOOST_AUTO(ti, find_timer(id)); return NULL != ti ? timer_info::TIMER_STARTED == ti->status : false;}
	bool start_timer(tid id) {BOOST_AUTO(ti, find_timer(id)); return NULL != ti ? start_timer(*ti) : false;}
	void stop_timer(tid id) {BOOST_AUTO(ti, find_timer(id)); if (NULL != ti) stop_timer(*ti);}
	void stop_all_timer() {do_something_to_all(boost::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, _1));}
	void stop_all_timer(tid excepted_id)
	{
		do_something_to_all(boost::lambda::if_then(excepted_id != boost::lambda::bind(&timer_info::id, boost::lambda::_1),
			boost::lambda::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, boost::lambda::_1)));
	}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

protected:
	bool start_timer(timer_info& ti)
	{
		if (!ti.call_back)
			return false;

		ti.status = timer_info::TIMER_STARTED;
#if BOOST_ASIO_VERSION >= 101100 && (defined(ST_ASIO_USE_STEADY_TIMER) || defined(ST_ASIO_USE_SYSTEM_TIMER))
		ti.timer.expires_after(milliseconds(ti.interval_ms));
#else
		ti.timer.expires_from_now(milliseconds(ti.interval_ms));
#endif
		ti.timer.async_wait(ST_THIS make_handler_error(boost::bind(&timer::timer_handler, this, boost::asio::placeholders::error, boost::ref(ti), ++ti.seq)));
		return true;
	}

	void timer_handler(const boost::system::error_code& ec, timer_info& ti, unsigned char prev_seq)
	{
		//return true from call_back to continue the timer, or the timer will stop
#ifdef ST_ASIO_ALIGNED_TIMER
		BOOST_AUTO(begin_time, boost::chrono::system_clock::now());
		if (!ec && ti.call_back(ti.id) && timer_info::TIMER_STARTED == ti.status)
		{
			unsigned elapsed_ms = (unsigned) boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::system_clock::now() - begin_time).count();
			if (elapsed_ms > ti.interval_ms)
				elapsed_ms %= ti.interval_ms;

			ti.interval_ms -= elapsed_ms;
			start_timer(ti);
			ti.interval_ms += elapsed_ms;
		}
#else
		if (!ec && ti.call_back(ti.id) && timer_info::TIMER_STARTED == ti.status)
			start_timer(ti);
#endif
		else if (prev_seq == ti.seq) //exclude a particular situation--start the same timer in call_back and return false
			ti.status = timer_info::TIMER_CANCELED;
	}

	void stop_timer(timer_info& ti)
	{
		if (timer_info::TIMER_STARTED == ti.status) //enable stopping timers that has been stopped
		{
			try {ti.timer.cancel();}
			catch (const boost::system::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", ti.id, e.code().value(), e.what());}
			ti.status = timer_info::TIMER_CANCELED;
		}
	}

private:
	typedef boost::container::list<timer_info> container_type;
	container_type timer_can;
	boost::mutex timer_can_mutex;

	using Executor::io_context_;
};

} //namespace

#endif /* ST_ASIO_TIMER_H_ */

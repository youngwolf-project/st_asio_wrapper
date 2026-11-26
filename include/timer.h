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

#include <boost/functional.hpp>
#ifdef ST_ASIO_USE_STEADY_TIMER
#include <boost/asio/steady_timer.hpp>
#else
#include <boost/asio/system_timer.hpp>
#endif

#include "base.h"

namespace st_asio_wrapper
{

//timers are identified by id and thread safe
//if you inherit a class from class X, your own timer ids must begin from X::TIMER_END
//suppose you have more than one service thread (see service_pump for service thread number controlling), then:
//in same timer object: same timer, callback is called in sequence, but:
// if you set the same timer during the execution of the callback (for example, in its callback), and timeout happens before the callback returns,
// then the two callbacks will be performed concurrently.
//in same timer object: distinct timer, callback is called concurrently
//in distinct timer objects: callback is always called concurrently
template<typename Executor>
class timer : public Executor
{
public:
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

	typedef unsigned short tid;
	typedef boost::function<bool(tid)> handler_with_tid;
	static const tid TIMER_END = 0; //subclass' id must begin from parent class' TIMER_END

	struct timer_info
	{
		enum timer_status {TIMER_CREATED, TIMER_STARTED, TIMER_CANCELED};

		tid id;
		unsigned char seq;
		timer_status status;
		unsigned interval_ms;
		mutable boost::mutex mutex;
		timer_type timer;
		handler_with_tid handler; //return true from handler to continue the timer, or the timer will stop

		timer_info(tid id_, boost::asio::io_context& io_context_) : id(id_), seq(-1), status(TIMER_CREATED), interval_ms(0), timer(io_context_) {}
		bool operator ==(const timer_info& other) const {return id == other.id;}
		bool operator ==(tid id_) const {return id == id_;}
	};
	typedef const timer_info timer_cinfo;

	timer(boost::asio::io_context& io_context_) : Executor(io_context_), io_context_refs(1) {}
	~timer() {stop_all_timer();}

	unsigned get_io_context_refs() const {return io_context_refs;}
	void add_io_context_refs(unsigned count)
	{
		if (count > 0)
		{
			io_context_refs += count;
			attach_io_context(io_context_, count);
		}
	}
	void sub_io_context_refs(unsigned count)
	{
		if (count > 0 && io_context_refs >= count)
		{
			io_context_refs -= count;
			detach_io_context(io_context_, count);
		}
	}
	void clear_io_context_refs() {sub_io_context_refs(io_context_refs);}

	template<typename T> bool create_or_update_timer(tid id, unsigned interval, const T& handler, bool start = false)
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

		boost::lock_guard<boost::mutex> lock(ti->mutex);
		ti->interval_ms = interval;
		ti->handler = handler;

		if (start)
			start_timer(*ti);

		return true;
	}
	template<typename T> bool set_timer(tid id, unsigned interval, const T& handler) {return create_or_update_timer(id, interval, handler, true);}

	template<typename T> bool change_timer(tid id, const T& setter)
	{
		BOOST_AUTO(ti, find_timer(id));
		if (NULL == ti)
			return false;

		boost::lock_guard<boost::mutex> lock(ti->mutex);
		return setter(ti);
	}
	bool change_timer_status(tid id, typename timer_info::timer_status status) {return change_timer(id, (boost::lambda::_1->*&timer_info::status = status, true));}
	bool change_timer_interval(tid id, unsigned interval) {return change_timer(id, (boost::lambda::_1->*&timer_info::interval_ms = interval, true));}
	template<typename T> bool change_timer_handler(tid id, const T& handler)
		{return change_timer(id, (boost::lambda::_1->*&timer_info::handler = boost::lambda::var(boost::cref(handler)), true));}

	timer_cinfo* find_timer(tid id) const
	{
		boost::lock_guard<boost::mutex> lock(timer_can_mutex);
		BOOST_AUTO(iter, std::find(timer_can.begin(), timer_can.end(), id));
		if (iter != timer_can.end())
			return &*iter;

		return NULL;
	}

	bool is_timer(tid id) const
	{
		BOOST_AUTO(ti, find_timer(id));
		if (NULL == ti)
			return false;

		boost::lock_guard<boost::mutex> lock(ti->mutex);
		return timer_info::TIMER_STARTED == ti->status;
	}

	bool start_timer(tid id) {BOOST_AUTO(ti, find_timer(id)); if (NULL == ti) return false; boost::lock_guard<boost::mutex> lock(ti->mutex); return start_timer(*ti);}
	void stop_timer(tid id) {BOOST_AUTO(ti, find_timer(id)); if (NULL != ti) stop_timer(*ti);}
	void stop_all_timer() {do_something_to_all(boost::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, boost::placeholders::_1));}
	void stop_all_timer(tid excepted_id)
	{
		do_something_to_all(boost::lambda::if_then(excepted_id != boost::lambda::bind(&timer_info::id, boost::lambda::_1),
			boost::lambda::bind((void (timer::*) (timer_info&)) &timer::stop_timer, this, boost::lambda::_1)));
	}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex, boost::lock_guard<boost::mutex>)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex, boost::lock_guard<boost::mutex>)

protected:
	void reset_io_context_refs() {if (0 == io_context_refs) add_io_context_refs(1);}
	virtual void attach_io_context(boost::asio::io_context& io_context_, unsigned refs) {}
	virtual void detach_io_context(boost::asio::io_context& io_context_, unsigned refs) {}

private:
	timer_info* find_timer(tid id) {return const_cast<timer_info*>(static_cast<const timer*>(this)->find_timer(id));}

	bool start_timer(timer_info& ti) {return start_timer(ti, ti.interval_ms);}
	bool start_timer(timer_info& ti, unsigned interval_ms)
	{
		ti.status = timer_info::TIMER_STARTED;
#if BOOST_ASIO_VERSION >= 101100
		ti.timer.expires_after(milliseconds(interval_ms));
#else
		ti.timer.expires_from_now(milliseconds(interval_ms));
#endif
		//if timer already started, this will cancel it first
		ti.timer.async_wait(ST_THIS track_async_call_error(boost::bind(&timer::timer_handler, this, boost::asio::placeholders::error, boost::ref(ti), ++ti.seq)));
		return true;
	}

	void timer_handler(const boost::system::error_code& ec, timer_info& ti, unsigned char prev_seq)
	{
#ifdef ST_ASIO_ALIGNED_TIMER
		BOOST_AUTO(begin_time, boost::chrono::system_clock::now());
#endif
		boost::unique_lock<boost::mutex> lock(ti.mutex);
		//the same timer is started again, or
		//the timer has been stopped, which means stop_timer can also invalidate cumulative timer callbacks
		if (prev_seq != ti.seq || timer_info::TIMER_STARTED != ti.status)
			;
		else if (ec || !ti.handler)
		{
			ti.status = timer_info::TIMER_CANCELED;
			ti.handler = NULL;
		}
		else
		{
			BOOST_AUTO(handler, ti.handler); //in case the handler is changed in the execution of the handler (for example, via set_timer)
			lock.unlock();
			bool re = handler(ti.id);
			lock.lock();

			//the same timer is started or stopped in the handler or during the execution of the handler
			if (prev_seq != ti.seq || timer_info::TIMER_STARTED != ti.status)
				;
			else if (!re)
			{
				ti.status = timer_info::TIMER_CANCELED;
				ti.handler = NULL;
			}
			else
			{
#ifdef ST_ASIO_ALIGNED_TIMER
				unsigned elapsed_ms = (unsigned) boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::system_clock::now() - begin_time).count();
				if (elapsed_ms > ti.interval_ms)
					elapsed_ms %= ti.interval_ms;

				start_timer(ti, ti.interval_ms - elapsed_ms);
#else
				start_timer(ti);
#endif
			}
		}
	}

	void stop_timer(timer_info& ti)
	{
		boost::lock_guard<boost::mutex> lock(ti.mutex);
		if (timer_info::TIMER_STARTED == ti.status) //enable stopping timers that has been stopped
		{
			ti.status = timer_info::TIMER_CANCELED; //invalidate cumulative timer callbacks first
			ti.handler = NULL;
			try {ti.timer.cancel();}
			catch (const boost::system::system_error& e) {unified_out::error_out("cannot stop timer %d (%d %s)", ti.id, e.code().value(), e.what());}
		}
	}

private:
	typedef boost::container::list<timer_info> container_type;
	container_type timer_can;
	mutable boost::mutex timer_can_mutex;

	using Executor::io_context_;
	unsigned io_context_refs;
};

} //namespace

#endif /* ST_ASIO_TIMER_H_ */

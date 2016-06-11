/*
 * st_asio_wrapper_timer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * timer base class
 */

#ifndef ST_ASIO_WRAPPER_TIMER_H_
#define ST_ASIO_WRAPPER_TIMER_H_

#include <boost/container/set.hpp>
#include <boost/function.hpp>

#include "st_asio_wrapper_base.h"

//If you inherit a class from class X, your own timer ids must begin from X::TIMER_END
namespace st_asio_wrapper
{

//timers are identified by id.
//for the same timer in the same st_timer, set_timer and stop_timer are not thread safe, please pay special attention.
//to resolve this defect, we must add a mutex member variable to timer_info, it's not worth
//
//suppose you have more than one service thread(see st_service_pump for service thread number control), then:
//same st_timer, same timer, on_timer is called sequentially
//same st_timer, different timer, on_timer is called concurrently
//different st_timer, on_timer is always called concurrently
class st_timer
{
protected:
	struct timer_info
	{
		enum timer_status {TIMER_OK, TIMER_CANCELED};

		unsigned char id;
		timer_status status;
		size_t milliseconds;
		boost::function<bool (unsigned char)> call_back;
		boost::shared_ptr<boost::asio::deadline_timer> timer;

		bool operator <(const timer_info& other) const {return id < other.id;}
	};

	static const unsigned char TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	st_timer(boost::asio::io_service& _io_service_) : io_service_(_io_service_) {}
	virtual ~st_timer() {}

public:
	typedef timer_info object_type;
	typedef const object_type object_ctype;
	typedef boost::container::set<object_type> container_type;

#ifdef ST_ASIO_ENHANCED_STABILITY
	void post(const boost::function<void()>& handler) {io_service_.post(boost::bind(&st_timer::post_handler, this, async_call_indicator, handler));}
	bool is_async_calling() const {return !async_call_indicator.unique();}
#else
	template<typename CompletionHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ()) post(BOOST_ASIO_MOVE_ARG(CompletionHandler) handler) {io_service_.post(handler);}
	bool is_async_calling() const {return false;}
#endif

	//after this call, call_back cannot be used again, please note.
	void update_timer_info(unsigned char id, size_t milliseconds, boost::function<bool(unsigned char)>& call_back, bool start = false)
	{
		object_type ti = {id};

		//lock timer_can
		timer_can_mutex.lock_upgrade();
		BOOST_AUTO(iter, timer_can.find(ti));
		if (iter == timer_can.end())
		{
			timer_can_mutex.unlock_upgrade_and_lock();
			iter = timer_can.insert(ti).first;
			timer_can_mutex.unlock();

			iter->timer = boost::make_shared<boost::asio::deadline_timer>(boost::ref(io_service_));
		}
		else
			timer_can_mutex.unlock_upgrade();

		//items in timer_can not locked
		iter->status = object_type::TIMER_OK;
		iter->milliseconds = milliseconds;
		iter->call_back.swap(call_back);

		if (start)
			start_timer(*iter);
	}
	void update_timer_info(unsigned char id, size_t milliseconds, const boost::function<bool (unsigned char)>& call_back, bool start = false)
		{BOOST_AUTO(tmp_call_back, call_back); update_timer_info(id, milliseconds, tmp_call_back, start);}

	//after this call, call_back cannot be used again, please note.
	void set_timer(unsigned char id, size_t milliseconds, boost::function<bool(unsigned char)>& call_back) {update_timer_info(id, milliseconds, call_back, true);}
	void set_timer(unsigned char id, size_t milliseconds, const boost::function<bool(unsigned char)>& call_back)
		{BOOST_AUTO(tmp_call_back, call_back); update_timer_info(id, milliseconds, tmp_call_back, true);}

	object_type find_timer(unsigned char id)
	{
		object_type ti = {id, object_type::TIMER_CANCELED, 0, NULL};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		BOOST_AUTO(iter, timer_can.find(ti));
		if (iter == timer_can.end())
			return *iter;
		else
			return ti;
	}

	bool start_timer(unsigned char id)
	{
		object_type ti = {id};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		BOOST_AUTO(iter, timer_can.find(ti));
		if (iter == timer_can.end())
			return false;
		lock.unlock();

		start_timer(*iter);
		return true;
	}

	void stop_timer(unsigned char id)
	{
		object_type ti = {id};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		BOOST_AUTO(iter, timer_can.find(ti));
		if (iter != timer_can.end())
		{
			lock.unlock();
			stop_timer(*iter);
		}
	}

	boost::asio::io_service& get_io_service() {return io_service_;}
	const boost::asio::io_service& get_io_service() const {return io_service_;}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

	void stop_all_timer() {do_something_to_all(boost::bind((void (st_timer::*) (object_type&)) &st_timer::stop_timer, this, _1));}

protected:
#ifdef ST_ASIO_ENHANCED_STABILITY
	void init() {async_call_indicator = boost::make_shared<char>('\0');}
	void post_handler(const boost::shared_ptr<char>& async_call_indicator, const boost::function<void()>& handler) {handler();}
#else
	void init() {}
#endif

	void start_timer(object_ctype& ti)
	{
		ti.timer->expires_from_now(boost::posix_time::milliseconds(ti.milliseconds));
		ti.timer->async_wait(boost::bind(&st_timer::timer_handler, this,
#ifdef ST_ASIO_ENHANCED_STABILITY
			async_call_indicator,
#endif
			boost::asio::placeholders::error, boost::ref(ti)));
	}

	void stop_timer(object_type& ti)
	{
		boost::system::error_code ec;
		ti.timer->cancel(ec);
		ti.status = object_type::TIMER_CANCELED;
	}

	void timer_handler(
#ifdef ST_ASIO_ENHANCED_STABILITY
		const boost::shared_ptr<char>& async_call_indicator,
#endif
		const boost::system::error_code& ec, object_ctype& ti)
	{
		//return true from call_back to continue the timer, or the timer will stop
		if (!ec && ti.call_back(ti.id) && object_type::TIMER_OK == ti.status)
			start_timer(ti);
	}

	boost::asio::io_service& io_service_;
	container_type timer_can;
	boost::shared_mutex timer_can_mutex;

#ifdef ST_ASIO_ENHANCED_STABILITY
	boost::shared_ptr<char> async_call_indicator;
#endif
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_TIMER_H_ */


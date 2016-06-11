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

#include "st_asio_wrapper_object.h"
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
class st_timer : public st_object
{
protected:
	struct timer_info
	{
		enum timer_status {TIMER_OK, TIMER_CANCELED};

		unsigned char id;
		timer_status status;
		size_t milliseconds;
		boost::function<bool(unsigned char)> call_back;
		boost::shared_ptr<boost::asio::deadline_timer> timer;

		bool operator <(const timer_info& other) const {return id < other.id;}
	};

	static const unsigned char TIMER_END = 0; //user timer's id must begin from parent class' TIMER_END

	st_timer(boost::asio::io_service& _io_service_) : st_object(_io_service_) {}
	virtual ~st_timer() {}

public:
	typedef timer_info object_type;
	typedef const object_type object_ctype;
	typedef boost::container::set<object_type> container_type;

	void update_timer_info(unsigned char id, size_t milliseconds, boost::function<bool(unsigned char)>&& call_back, bool start = false)
	{
		object_type ti = {id};

		//lock timer_can
		timer_can_mutex.lock_upgrade();
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
		{
			timer_can_mutex.unlock_upgrade_and_lock();
			iter = timer_can.insert(ti).first;
			timer_can_mutex.unlock();

			iter->timer = boost::make_shared<boost::asio::deadline_timer>(get_io_service());
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
	void update_timer_info(unsigned char id, size_t milliseconds, const boost::function<bool(unsigned char)>& call_back, bool start = false)
		{update_timer_info(id, milliseconds, boost::function<bool(unsigned char)>(call_back), start);}

	void set_timer(unsigned char id, size_t milliseconds, boost::function<bool(unsigned char)>&& call_back) {update_timer_info(id, milliseconds, std::move(call_back), true);}
	void set_timer(unsigned char id, size_t milliseconds, const boost::function<bool(unsigned char)>& call_back) {update_timer_info(id, milliseconds, call_back, true);}

	object_type find_timer(unsigned char id)
	{
		object_type ti = {id, object_type::TIMER_CANCELED, 0};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
			return *iter;
		else
			return ti;
	}

	bool start_timer(unsigned char id)
	{
		object_type ti = {id};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
			return false;
		lock.unlock();

		start_timer(*iter);
		return true;
	}

	void stop_timer(unsigned char id)
	{
		object_type ti = {id};

		boost::shared_lock<boost::shared_mutex> lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter != std::end(timer_can))
		{
			lock.unlock();
			stop_timer(*iter);
		}
	}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

	void stop_all_timer() {do_something_to_all([this](object_type& item) {ST_THIS stop_timer(item);});}

protected:
	void reset() {st_object::reset();}

	void start_timer(object_ctype& ti)
	{
		ti.timer->expires_from_now(boost::posix_time::milliseconds(ti.milliseconds));
		//return true from call_back to continue the timer, or the timer will stop
		ti.timer->async_wait(
			make_handler_error([this, &ti](const boost::system::error_code& ec) {if (!ec && ti.call_back(ti.id) && st_timer::object_type::TIMER_OK == ti.status) ST_THIS start_timer(ti);}));
	}

	void stop_timer(object_type& ti)
	{
		boost::system::error_code ec;
		ti.timer->cancel(ec);
		ti.status = object_type::TIMER_CANCELED;
	}

	container_type timer_can;
	boost::shared_mutex timer_can_mutex;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_TIMER_H_ */


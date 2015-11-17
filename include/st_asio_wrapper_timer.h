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

#include "st_asio_wrapper_base.h"

/*
* Please pay attention to the following reserved timer ids:
* st_object_pool: 0 - 9
* st_socket: 0 - 9

* st_tcp_socket_base: inherit from st_socket
* st_connector_base: inherit from st_tcp_socket_base and plus 10 - 19
* st_tcp_sclient: inherit from st_connector
*
* st_udp_socket_base: inherit from st_socket
* st_udp_sclient: inherit from st_udp_socket
*
* st_server_socket_base: inherit from st_tcp_socket_base and plus 10 - 19
* st_server_base: inherit from st_object_pool
*
* st_client: inherit from st_object_pool
* st_tcp_client_base: inherit from st_client_base
* st_udp_client_base: inherit from st_client_base
*/

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
		const void* user_data; //if needed, you must take the responsibility to manage this memory
		boost::shared_ptr<boost::asio::deadline_timer> timer;

		bool operator <(const timer_info& other) const {return id < other.id;}
	};

	st_timer(boost::asio::io_service& _io_service_) : io_service_(_io_service_) {}
	virtual ~st_timer() {}

public:
	typedef timer_info object_type;
	typedef const object_type object_ctype;
	typedef boost::container::set<object_type> container_type;

	void update_timer_info(unsigned char id, size_t milliseconds, const void* user_data, bool start = false)
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

			iter->timer = boost::make_shared<boost::asio::deadline_timer>(io_service_);
		}
		else
			timer_can_mutex.unlock_upgrade();

		//items in timer_can not locked
		iter->status = object_type::TIMER_OK;
		iter->milliseconds = milliseconds;
		iter->user_data = user_data;

		if (start)
			start_timer(*iter);
	}

	void set_timer(unsigned char id, size_t milliseconds, const void* user_data) {update_timer_info(id, milliseconds, user_data, true);}

	object_type find_timer(unsigned char id)
	{
		object_type ti = {id, object_type::TIMER_CANCELED, 0, nullptr};

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

	boost::asio::io_service& get_io_service() {return io_service_;}
	const boost::asio::io_service& get_io_service() const {return io_service_;}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

	void stop_all_timer() {do_something_to_all([this](object_type& item) {ST_THIS stop_timer(item);});}

protected:
	//return true to continue the timer, or the timer will stop
	virtual bool on_timer(unsigned char id, const void* user_data) {return false;}

	void start_timer(object_ctype& ti)
	{
		ti.timer->expires_from_now(boost::posix_time::milliseconds(ti.milliseconds));
		ti.timer->async_wait(boost::bind(&st_timer::timer_handler, this, boost::asio::placeholders::error, boost::ref(ti)));
	}

	void stop_timer(object_type& ti)
	{
		boost::system::error_code ec;
		ti.timer->cancel(ec);
		ti.status = object_type::TIMER_CANCELED;
	}

	void timer_handler(const boost::system::error_code& ec, object_ctype& ti)
	{
		if (!ec && on_timer(ti.id, ti.user_data) && object_type::TIMER_OK == ti.status)
			start_timer(ti);
	}

	boost::asio::io_service& io_service_;
	container_type timer_can;
	boost::shared_mutex timer_can_mutex;
};

} //namespace

#endif /* ifndef ST_ASIO_WRAPPER_TIMER_H_ */


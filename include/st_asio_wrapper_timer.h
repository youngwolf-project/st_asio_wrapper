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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/container/set.hpp>
using namespace boost::system;
using namespace boost::asio;

/*
* Please pay attention to the following reserved timer ids:
* st_object_pool: 0 - 9
* st_socket: 0 - 9

* st_tcp_socket: inherit from st_socket
* st_connector: inherit from st_tcp_socket and plus 10 - 19
* st_tcp_sclient: inherit from st_connector
*
* st_udp_socket: inherit from st_socket
* st_udp_sclient: inherit from st_udp_socket
*
* st_server_socket_base: inherit from st_tcp_socket
* st_server_base: inherit from st_object_pool
*
* st_client_base: inherit from st_object_pool
* st_tcp_client_base: inherit from st_client_base
* st_udp_client_base: inherit from st_client_base
*/

namespace st_asio_wrapper
{

//timers are indentified by id.
//for the same timer in the same st_timer, set_timer and stop_timer are not thread safe,
//please pay special attention. to resolve this defect, we must add a mutex member variable to timer_info,
//it's not worth
//
//suppose you have more than one service thread(see st_service_pump for service thread number control), then:
//same st_timer, same timer, on_timer is called sequently
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
		boost::shared_ptr<deadline_timer> timer;

		bool operator <(const timer_info& other) const {return id < other.id;}
	};

	st_timer(io_service& _io_service_) : io_service_(_io_service_) {}
	virtual ~st_timer() {}

public:
	void set_timer(unsigned char id, size_t milliseconds, const void* user_data)
	{
		timer_info ti = {id};

		mutex::scoped_lock lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter == std::end(timer_can))
		{
			iter = timer_can.insert(ti).first;
			iter->timer = boost::make_shared<deadline_timer>(io_service_);
		}
		lock.unlock();

		iter->status = timer_info::TIMER_OK;
		iter->milliseconds = milliseconds;
		iter->user_data = user_data;

		start_timer(*iter);
	}

	void stop_timer(unsigned char id)
	{
		timer_info ti = {id};

		mutex::scoped_lock lock(timer_can_mutex);
		auto iter = timer_can.find(ti);
		if (iter != std::end(timer_can))
		{
			lock.unlock();
			stop_timer(*iter);
		}
	}

	DO_SOMETHING_TO_ALL_MUTEX(timer_can, timer_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(timer_can, timer_can_mutex)

	void stop_all_timer()
		{do_something_to_all(boost::bind((void (st_timer::*) (timer_info&)) &st_timer::stop_timer, this, _1));}

protected:
	//return true to continue the timer, or the timer will stop
	virtual bool on_timer(unsigned char id, const void* user_data) {return false;}

	void start_timer(const timer_info& ti)
	{
		ti.timer->expires_from_now(posix_time::milliseconds(ti.milliseconds));
		ti.timer->async_wait(boost::bind(&st_timer::timer_handler, this, placeholders::error, boost::ref(ti)));
	}

	void stop_timer(timer_info& ti)
	{
		error_code ec;
		ti.timer->cancel(ec);
		ti.status = timer_info::TIMER_CANCELED;
	}

	void timer_handler(const error_code& ec, const timer_info& ti)
	{
		if (!ec && on_timer(ti.id, ti.user_data) && timer_info::TIMER_OK == ti.status)
			start_timer(ti);
	}

	io_service& io_service_;
	container::set<timer_info> timer_can;
	mutex timer_can_mutex;
};

}

#endif /* ifndef ST_ASIO_WRAPPER_TIMER_H_ */


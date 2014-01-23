/*
 * st_asio_wrapper_service_pump.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at both server and client endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVICE_PUMP_H_
#define ST_ASIO_WRAPPER_SERVICE_PUMP_H_

#include <boost/container/list.hpp>

#include "st_asio_wrapper_base.h"

//io thread number
//listen, all msg send and recv, msg handle(on_msg_handle() and on_msg()) will use these threads
//keep big enough, empirical value need you to try to find out in your own environment
#ifndef ST_SERVICE_THREAD_NUM
#define ST_SERVICE_THREAD_NUM 8
#endif

namespace st_asio_wrapper
{

class st_service_pump : public boost::asio::io_service
{
public:
	class i_service
	{
	protected:
		i_service(st_service_pump& service_pump_) : service_pump(service_pump_), started(false), id_(0), data(nullptr)
			{service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		//for the same i_service, start_service and stop_service are not thread safe, please pay special attention.
		//to resolve this defect, we must add a mutex member variable to i_service, it's not worth
		void start_service() {if (!started) {started = true; init();}}
		void stop_service() {if (started) {started = false; uninit();}}
		bool is_started() const {return started;}

		void id(int id) {id_ = id;}
		int id() const {return id_;}
		void user_data(void* data_) {data = data_;}
		void* user_data() {return data;}

		st_service_pump& get_service_pump() {return service_pump;}
		const st_service_pump& get_service_pump() const {return service_pump;}

	protected:
		virtual void init() = 0;
		virtual void uninit() = 0;

	protected:
		st_service_pump& service_pump;

	private:
		bool started;
		int id_;
		void* data; //magic data, you can use it in any way
	};

public:
	typedef i_service* object_type;
	typedef const object_type object_ctype;
	typedef boost::container::list<object_type> container_type;

	st_service_pump() : started(false) {}

	object_type find(int id)
	{
		boost::mutex::scoped_lock lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can),
			[=](object_ctype& item) {return id == item->id();});
		return iter == std::end(service_can) ? nullptr : *iter;
	}

	void remove(object_type i_service_)
	{
		assert(nullptr != i_service_);

		boost::mutex::scoped_lock lock(service_can_mutex);
		service_can.remove(i_service_);
		lock.unlock();

		stop_and_free(i_service_);
	}

	void remove(int id)
	{
		boost::mutex::scoped_lock lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can),
			[=](object_ctype& item) {return id == item->id();});
		if (iter != std::end(service_can))
		{
			auto i_service_ = *iter;
			service_can.erase(iter);
			lock.unlock();

			stop_and_free(i_service_);
		}
	}

	void clear()
	{
		container_type temp_service_can;

		boost::mutex::scoped_lock lock(service_can_mutex);
		temp_service_can.splice(std::end(temp_service_can), service_can);
		lock.unlock();

		st_asio_wrapper::do_something_to_all(temp_service_can, boost::bind(&st_service_pump::stop_and_free, this, _1));
	}

	void start_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			service_thread = boost::thread(boost::bind(&st_service_pump::run_service, this, thread_num));
			auto loop_num = 10;
			while (--loop_num >= 0 && !is_service_started())
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
		}
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	void stop_service() {end_service();}
	//only used when stop_service() can not stop the service(been blocked and can not return)
	void force_stop_service()
	{
		if (is_service_started())
		{
			do_something_to_all(boost::mem_fn(&i_service::stop_service));
			auto loop_num = 20; //one second
			while (is_service_started())
			{
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
				if (--loop_num <= 0)
				{
					stop();
					loop_num = 0x7fffffff;
				}
			}
		}
	}

	//if you add a service after start_service, use this to start it
	void start_service(object_type i_service_, int thread_num = ST_SERVICE_THREAD_NUM)
	{
		assert(nullptr != i_service_);

		if (is_service_started())
			i_service_->start_service();
		else
			start_service(thread_num);
	}
	void stop_service(object_type i_service_) {assert(nullptr != i_service_); i_service_->stop_service();}

	bool is_running() const {return !stopped();}
	bool is_service_started() const {return started;}

	//this function works like start_service except that it will block until all services run out
	void run_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			reset(); //this is needed when restart service
			do_something_to_all(boost::mem_fn(&i_service::start_service));
			do_service(thread_num);
		}
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	void end_service()
	{
		if (is_service_started())
		{
			do_something_to_all(boost::mem_fn(&i_service::stop_service));
			while (is_service_started())
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
		}
	}

protected:
	void stop_and_free(object_type i_service_)
	{
		assert(nullptr != i_service_);

		i_service_->stop_service();
		free(i_service_);
	}
	virtual void free(object_type i_service_) {} //if needed, rewrite this to free the service

#ifdef ENHANCED_STABILITY
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::info_out("service pump exception: %s.", e.what());
		return true; //continue this io_service::run, if needed, rewrite this to decide whether to continue or not
	}

	size_t run(boost::system::error_code& ec)
	{
		while (true)
		{
			try {return io_service::run(ec);}
			catch (const std::exception& e) {if (!on_exception(e)) return 0;}
		}
	}
#endif

	DO_SOMETHING_TO_ALL_MUTEX(service_can, service_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(service_can, service_can_mutex)

private:
	void add(object_type i_service_)
	{
		assert(nullptr != i_service_);

		boost::mutex::scoped_lock lock(service_can_mutex);
		service_can.push_back(i_service_);
	}

	void do_service(int thread_num)
	{
		started = true;
		unified_out::info_out("service pump started.");

		--thread_num;
		boost::thread_group tg;
		for (auto i = 0; i < thread_num; ++i)
			tg.create_thread(boost::bind(&st_service_pump::run, this, boost::system::error_code()));
		boost::system::error_code ec;
		run(ec);

		if (thread_num > 0)
			tg.join_all();

		unified_out::info_out("service pump end.");
		started = false;
	}

protected:
	container_type service_can;
	boost::mutex service_can_mutex;
	boost::thread service_thread;
	bool started;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVICE_PUMP_H_ */

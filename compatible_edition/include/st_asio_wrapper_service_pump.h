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

class st_service_pump : public io_service
{
public:
	class i_service
	{
	protected:
		i_service(st_service_pump& service_pump_) : service_pump(service_pump_), id(0) {service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		virtual void init() = 0;
		virtual void uninit() = 0;

		void set_id(int _id) {id = _id;}
		int get_id() const {return id;}
		bool is_equal_to(int id) const {return get_id() == id;}

		st_service_pump& get_service_pump() {return service_pump;}
		const st_service_pump& get_service_pump() const {return service_pump;}

	protected:
		st_service_pump& service_pump;

	private:
		int id;
	};

public:
	virtual ~st_service_pump() {clear();}

	i_service* find(int id)
	{
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(),
			std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		return iter == service_can.end() ? NULL : *iter;
	}
	void remove(i_service* i_service_) {assert(NULL != i_service_); free(i_service_); service_can.remove(i_service_);}
	void remove(int id)
	{
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(),
			std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		if (iter != service_can.end())
		{
			free(*iter);
			service_can.erase(iter);
		}
	}
	void clear() {do_something_to_all(boost::bind(&st_service_pump::free, this, _1)); service_can.clear();}

	void start_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		reset(); //this is needed when re-start_service
		do_something_to_all(boost::mem_fn(&i_service::init));
		service_thread = thread(boost::bind(&st_service_pump::do_service, this, thread_num));
		this_thread::yield();
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example,
	//close the application
	bool stop_service()
	{
		if (!is_service_started())
			return false;

		do_something_to_all(boost::mem_fn(&i_service::uninit));
		service_thread.join();
		return true;
	}
	//only used when stop_service() can not stop the service(been blocked and can not return)
	void force_stop_service(){stop(); service_thread.join();}
	bool is_running() const {return !stopped();}
	bool is_service_started() const {return service_thread.get_id() != thread::id();}

	//this function works like start_service except that it will block until service run out,
	//do not use stop_service(from other thread because this thread has been blocked) to unblock it,
	//instead, use stop function which is implemented by io_service(also from other thread)
	void run_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		reset(); //this is needed when re-start_service
		do_something_to_all(boost::mem_fn(&i_service::init));
		do_service(thread_num);
	}

protected:
	virtual void free(i_service* i_service_) {} //if needed, rewrite this to free the service

#ifdef ENHANCED_STABILITY
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::info_out("service pump exception: %s.", e.what());
		return true; //continue this service pump
	}

	size_t run(error_code& ec)
	{
		while (true)
		{
			try {return io_service::run(ec);}
			catch (const std::exception& e) {if (!on_exception(e)) return 0;}
		}
	}
#endif

	DO_SOMETHING_TO_ALL(service_can)
	DO_SOMETHING_TO_ONE(service_can)

private:
	void add(i_service* i_service_) {assert(NULL != i_service_); service_can.push_back(i_service_);}

	void do_service(int thread_num)
	{
		unified_out::info_out("service pump started.");

		--thread_num;
		thread_group tg;
		for (int i = 0; i < thread_num; ++i)
			tg.create_thread(boost::bind(&st_service_pump::run, this, error_code()));
		error_code ec;
		run(ec);

		if (thread_num > 0)
			tg.join_all();

		unified_out::info_out("service pump end.");
	}

protected:
	container::list<i_service*> service_can; //not protected by mutex, please note
	thread service_thread;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVICE_PUMP_H_ */

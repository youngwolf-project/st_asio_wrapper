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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/container/list.hpp>
using namespace boost::asio;
using namespace boost::system;

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
	public:
		virtual void init() = 0;
		virtual void uninit() = 0;

		i_service(st_service_pump& service_pump_) : id(0) {service_pump_.add(this);}
		virtual ~i_service() {}

		void set_id(int _id) {id = _id;}
		int get_id() const {return id;}

	private:
		int id;
	};

public:
	virtual ~st_service_pump() {clear();}
	virtual void free(i_service* i_service_) {} //if needed, rewrite this to free the service

	i_service* find(int id)
	{
		auto iter = std::find_if(std::begin(service_can), std::end(service_can),
			[=](decltype(*std::begin(service_can))& item) {return id == item->get_id();});
		return iter == std::end(service_can) ? nullptr : *iter;
	}
	void remove(i_service* i_service_) {assert(nullptr != i_service_); free(i_service_); service_can.remove(i_service_);}
	void remove(int id)
	{
		auto iter = std::find_if(std::begin(service_can), std::end(service_can),
			[=](decltype(*std::begin(service_can))& item) {return id == item->get_id();});
		if (iter != std::end(service_can))
		{
			free(*iter);
			service_can.erase(iter);
		}
	}
	void clear() {do_something_to_all(boost::bind(&st_service_pump::free, this, _1)); service_can.clear();}

	bool start_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		reset(); //this is needed when re-start_service
		do_something_to_all(boost::mem_fn(&i_service::init));
		thread t(boost::bind(&st_service_pump::do_service, this, thread_num));
		this_thread::yield();
		t.swap(service_thread);
		return true;
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

private:
	void add(i_service* i_service_) {assert(nullptr != i_service_); service_can.push_back(i_service_);}

#ifdef ENHANCED_STABILITY
	void safe_run()
	{
		while (true)
		{
			error_code ec;
			try {run(ec); break;}
			catch (std::exception& e){}
		}
	}
#endif

	void do_service(int thread_num)
	{
		unified_out::info_out("service pump started.");

		--thread_num;
		thread_group tg;
		for (auto i = 0; i < thread_num; ++i)
#ifdef ENHANCED_STABILITY
			tg.create_thread(boost::bind(&st_service_pump::safe_run, this));
		safe_run();
#else
			tg.create_thread(boost::bind(&st_service_pump::run, this, error_code()));
		error_code ec;
		run(ec);
#endif
		if (thread_num > 0)
			tg.join_all();

		unified_out::info_out("service pump end.");
	}

protected:
	DO_SOMETHING_TO_ALL(service_can)
	DO_SOMETHING_TO_ONE(service_can)

protected:
	container::list<i_service*> service_can; //not protected by mutex, please note
	thread service_thread;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVICE_PUMP_H_ */

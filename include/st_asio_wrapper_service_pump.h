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

#include "st_asio_wrapper_base.h"

//IO thread number
//listen, msg send and receive, msg handle(on_msg_handle() and on_msg()) will use these threads
//keep big enough, no empirical value i can suggest, you must try to find it in your own environment
#ifndef ST_ASIO_SERVICE_THREAD_NUM
#define ST_ASIO_SERVICE_THREAD_NUM	8
#endif
static_assert(ST_ASIO_SERVICE_THREAD_NUM > 0, "service thread number be bigger than zero.");

namespace st_asio_wrapper
{

class st_service_pump : public boost::asio::io_service
{
public:
	class i_service
	{
	protected:
		i_service(st_service_pump& service_pump_) : sp(service_pump_), started(false), id_(0), data(nullptr) {service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		//for the same i_service, start_service and stop_service are not thread safe,
		//to resolve this defect, we must add a mutex member variable to i_service, it's not worth
		void start_service() {if (!started) started = init();}
		void stop_service() {if (started) uninit(); started = false;}
		bool is_started() const {return started;}

		void id(int id) {id_ = id;}
		int id() const {return id_;}
		void user_data(void* data_) {data = data_;}
		void* user_data() const {return data;}

		st_service_pump& get_service_pump() {return sp;}
		const st_service_pump& get_service_pump() const {return sp;}

	protected:
		virtual bool init() = 0;
		virtual void uninit() = 0;

	protected:
		st_service_pump& sp;

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
	virtual ~st_service_pump() {stop_service();}

	object_type find(int id)
	{
		boost::lock_guard<boost::mutex> lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can), [id](object_ctype& item) {return id == item->id();});
		return iter == std::end(service_can) ? nullptr : *iter;
	}

	void remove(object_type i_service_)
	{
		assert(nullptr != i_service_);

		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		service_can.remove(i_service_);
		lock.unlock();

		stop_and_free(i_service_);
	}

	void remove(int id)
	{
		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		auto iter = std::find_if(std::begin(service_can), std::end(service_can), [id](object_ctype& item) {return id == item->id();});
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

		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		temp_service_can.splice(std::end(temp_service_can), service_can);
		lock.unlock();

		st_asio_wrapper::do_something_to_all(temp_service_can, [this](object_type& item) {this->stop_and_free(item);});
	}

	void start_service(int thread_num = ST_ASIO_SERVICE_THREAD_NUM) {if (!is_service_started()) do_service(thread_num);}
	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	void stop_service()
	{
		if (is_service_started())
		{
			end_service();
			wait_service();
		}
	}

	//if you add a service after start_service, use this to start it
	void start_service(object_type i_service_, int thread_num = ST_ASIO_SERVICE_THREAD_NUM)
	{
		assert(nullptr != i_service_);

		if (is_service_started())
			i_service_->start_service();
		else
			start_service(thread_num);
	}
	void stop_service(object_type i_service_) {assert(nullptr != i_service_); i_service_->stop_service();}

	//this function works like start_service except that it will block until all services run out
	void run_service(int thread_num = ST_ASIO_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			do_service(thread_num - 1);

			boost::system::error_code ec;
			run(ec);

			wait_service();
		}
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	//only for service pump started by 'run_service', this function will return immediately,
	//only the return from 'run_service' means service pump ended.
	void end_service() {if (is_service_started()) do_something_to_all([](object_type& item) {item->stop_service();});}

	bool is_running() const {return !stopped();}
	bool is_service_started() const {return started;}
	void add_service_thread(int thread_num) {for (auto i = 0; i < thread_num; ++i) service_threads.create_thread([this]() {boost::system::error_code ec; this->run(ec);});}

protected:
	void do_service(int thread_num)
	{
		started = true;
		unified_out::info_out("service pump started.");

		reset(); //this is needed when restart service
		do_something_to_all([](object_type& item) {item->start_service();});
		add_service_thread(thread_num);
	}
	void wait_service() {service_threads.join_all(); unified_out::info_out("service pump end."); started = false;}

	void stop_and_free(object_type i_service_)
	{
		assert(nullptr != i_service_);

		i_service_->stop_service();
		free(i_service_);
	}
	virtual void free(object_type i_service_) {} //if needed, rewrite this to free the service

#ifdef ST_ASIO_ENHANCED_STABILITY
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::error_out("service pump exception: %s.", e.what());
		return true; //continue this boost::asio::io_service::run, if needed, rewrite this to decide whether to continue or not
	}

	size_t run(boost::system::error_code& ec)
	{
		while (true)
		{
			try {return boost::asio::io_service::run(ec);}
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

		boost::lock_guard<boost::mutex> lock(service_can_mutex);
		service_can.emplace_back(i_service_);
	}

protected:
	container_type service_can;
	boost::mutex service_can_mutex;
	boost::thread_group service_threads;
	bool started;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVICE_PUMP_H_ */

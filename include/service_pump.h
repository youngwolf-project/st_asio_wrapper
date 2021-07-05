/*
 * service_pump.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at both server and client endpoint
 */

#ifndef ST_ASIO_SERVICE_PUMP_H_
#define ST_ASIO_SERVICE_PUMP_H_

#include "base.h"

namespace st_asio_wrapper
{

class service_pump : public boost::asio::io_context
{
public:
	class i_service
	{
	protected:
		i_service(service_pump& service_pump_) : sp(service_pump_), started_(false), id_(0), data(NULL) {sp.add(this);}
		virtual ~i_service() {sp.remove(this);}

	public:
		//for the same i_service, start_service and stop_service are not thread safe,
		//to resolve this defect, we must add a mutex member variable to i_service, it's not worth
		void start_service() {if (!started_) started_ = init();}
		void stop_service() {if (started_) uninit(); started_ = false;}
		bool service_started() const {return started_;}

		void id(int id) {id_ = id;}
		int id() const {return id_;}
		bool is_equal_to(int id) const {return id_ == id;}
		void user_data(void* data_) {data = data_;}
		void* user_data() const {return data;}

		service_pump& get_service_pump() {return sp;}
		const service_pump& get_service_pump() const {return sp;}

	protected:
		virtual bool init() = 0;
		virtual void uninit() = 0;

	protected:
		service_pump& sp;

	private:
		bool started_;
		int id_;
		void* data; //magic data, you can use it in any way
	};

public:
	typedef i_service* object_type;
	typedef const object_type object_ctype;
	typedef boost::container::list<object_type> container_type;

#if BOOST_ASIO_VERSION >= 101200
	service_pump(int concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE) : boost::asio::io_context(concurrency_hint), started(false)
#else
	service_pump() : started(false)
#endif
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
		, real_thread_num(0), del_thread_num(0)
#endif
#ifdef ST_ASIO_AVOID_AUTO_STOP_SERVICE
#if BOOST_ASIO_VERSION >= 101100
		, work(get_executor())
#else
		, work(boost::make_shared<boost::asio::io_service::work>(boost::ref(*this)))
#endif
#endif
	{}
	virtual ~service_pump() {stop_service();}

	object_type find(int id)
	{
		boost::lock_guard<boost::mutex> lock(service_can_mutex);
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(), std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		return iter == service_can.end() ? NULL : *iter;
	}

	void remove(object_type i_service_)
	{
		assert(NULL != i_service_);

		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		service_can.remove(i_service_);
		lock.unlock();

		stop_and_free(i_service_);
	}

	void remove(int id)
	{
		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(), std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		if (iter != service_can.end())
		{
			object_type i_service_ = *iter;
			service_can.erase(iter);
			lock.unlock();

			stop_and_free(i_service_);
		}
	}

	void clear()
	{
		container_type temp_service_can;

		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		temp_service_can.splice(temp_service_can.end(), service_can);
		lock.unlock();

		st_asio_wrapper::do_something_to_all(temp_service_can, boost::bind(&service_pump::stop_and_free, this, boost::placeholders::_1));
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

	//if you add a service after start_service(), use this to start it
	void start_service(object_type i_service_, int thread_num = ST_ASIO_SERVICE_THREAD_NUM)
	{
		assert(NULL != i_service_);

		if (is_service_started())
			i_service_->start_service();
		else
			start_service(thread_num);
	}
	//doesn't like stop_service(), this will asynchronously stop service i_service_, you must NOT free i_service_ immediately,
	// otherwise, you may lead segment fault if you freed i_service_ before any async operation ends.
	//my suggestion is, DO NOT free i_service_, we can suspend it (by your own implementation and invocation rather than
	// stop_service(object_type)).
	//BTW, all i_services managed by this service_pump can be safely freed after stop_service().
	void stop_service(object_type i_service_) {assert(NULL != i_service_); i_service_->stop_service();}

	//this function works like start_service() except that it will block until all services run out
	void run_service(int thread_num = ST_ASIO_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			do_service(thread_num - 1);
			run();
			wait_service();
		}
	}

	//stop the service, must be invoked explicitly when the service need to stop, for example, close the application
	//only for service pump started by 'run_service', this function will return immediately,
	//only the return from 'run_service' means service pump ended.
	void end_service()
	{
		if (is_service_started())
		{
#ifdef ST_ASIO_AVOID_AUTO_STOP_SERVICE
			work.reset();
#endif
			do_something_to_all(boost::mem_fn(&i_service::stop_service));
		}
	}

	bool is_running() const {return !stopped();}
	bool is_service_started() const {return started;}

	void add_service_thread(int thread_num) {for (int i = 0; i < thread_num; ++i) service_threads.create_thread(boost::bind(&service_pump::run, this));}
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	void del_service_thread(int thread_num) {if (thread_num > 0) {del_thread_num += thread_num;}}
	int service_thread_num() const {return real_thread_num;}
#endif

protected:
	void do_service(int thread_num)
	{
		started = true;
		unified_out::info_out("service pump started.");

#if BOOST_ASIO_VERSION >= 101100
		restart(); //this is needed when restart service
#else
		reset(); //this is needed when restart service
#endif
		do_something_to_all(boost::mem_fn(&i_service::start_service));
		add_service_thread(thread_num);
	}

	void wait_service()
	{
		service_threads.join_all();

		started = false;
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
		del_thread_num = 0;
#endif
		unified_out::info_out("service pump end.");
	}

	void stop_and_free(object_type i_service_)
	{
		assert(NULL != i_service_);

		i_service_->stop_service();
		free(i_service_);
	}
	virtual void free(object_type i_service_) {} //if needed, rewrite this to free the service

#ifndef ST_ASIO_NO_TRY_CATCH
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::error_out("service pump exception: %s.", e.what());
		return true; //continue, if needed, rewrite this to decide whether to continue or not
	}
#endif

#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	size_t run()
	{
		size_t n = 0;

		std::stringstream os;
		os << "service thread[" << boost::this_thread::get_id() << "] begin.";
		unified_out::info_out(os.str().data());
		++real_thread_num;
		while (true)
		{
			if (del_thread_num > 0)
			{
				if (--del_thread_num >= 0)
				{
					if (--real_thread_num > 0) //forbid to stop all service thread
						break;
					else
						++real_thread_num;
				}
				else
					++del_thread_num;
			}

			//we cannot always decrease service thread timely (because run_one can block).
			size_t this_n = 0;
#ifdef ST_ASIO_NO_TRY_CATCH
			this_n = boost::asio::io_context::run_one();
#else
			try {this_n = boost::asio::io_context::run_one();} catch (const std::exception& e) {if (!on_exception(e)) break;}
#endif
			if (this_n > 0)
				n += this_n; //n can overflow, please note.
			else
			{
				--real_thread_num;
				break;
			}
		}
		os.str("");
		os << "service thread[" << boost::this_thread::get_id() << "] end.";
		unified_out::info_out(os.str().data());

		return n;
	}
#elif !defined(ST_ASIO_NO_TRY_CATCH)
	size_t run() {while (true) {try {return boost::asio::io_context::run();} catch (const std::exception& e) {if (!on_exception(e)) return 0;}}}
#endif

	DO_SOMETHING_TO_ALL_MUTEX(service_can, service_can_mutex, boost::lock_guard<boost::mutex>)
	DO_SOMETHING_TO_ONE_MUTEX(service_can, service_can_mutex, boost::lock_guard<boost::mutex>)

private:
	void add(object_type i_service_)
	{
		assert(NULL != i_service_);

		boost::unique_lock<boost::mutex> lock(service_can_mutex);
		service_can.emplace_back(i_service_);
		lock.unlock();

		if (is_service_started())
			unified_out::warning_out("service been added, please remember to call start_service for it!");
	}

private:
	bool started;
	container_type service_can;
	boost::mutex service_can_mutex;
	boost::thread_group service_threads;

#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	atomic_int_fast32_t real_thread_num;
	atomic_int_fast32_t del_thread_num;
#endif

#ifdef ST_ASIO_AVOID_AUTO_STOP_SERVICE
#if BOOST_ASIO_VERSION >= 101100
	boost::asio::executor_work_guard<executor_type> work;
#else
	boost::shared_ptr<boost::asio::io_service::work> work;
#endif
#endif
};

} //namespace

#endif /* ST_ASIO_SERVICE_PUMP_H_ */

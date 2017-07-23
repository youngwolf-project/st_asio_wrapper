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

class service_pump : public boost::asio::io_service
{
public:
	class i_service
	{
	protected:
		i_service(service_pump& service_pump_) : sp(service_pump_), started(false), id_(0), data(NULL) {service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		//for the same i_service, start_service and stop_service are not thread safe,
		//to resolve this defect, we must add a mutex member variable to i_service, it's not worth
		void start_service() {if (!started) started = init();}
		void stop_service() {if (started) uninit(); started = false;}
		bool is_started() const {return started;}

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
		bool started;
		int id_;
		void* data; //magic data, you can use it in any way
	};

public:
	typedef i_service* object_type;
	typedef const object_type object_ctype;
	typedef boost::container::list<object_type> container_type;

	service_pump() : started(false), real_thread_num(0), del_thread_num(0), del_thread_req(false) {}
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

		st_asio_wrapper::do_something_to_all(temp_service_can, boost::bind(&service_pump::stop_and_free, this, _1));
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
		assert(NULL != i_service_);

		if (is_service_started())
			i_service_->start_service();
		else
			start_service(thread_num);
	}
	void stop_service(object_type i_service_) {assert(NULL != i_service_); i_service_->stop_service();}

	//this function works like start_service except that it will block until all services run out
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
	void del_service_thread(int thread_num) {if (thread_num > 0) {del_thread_num.fetch_add(thread_num, boost::memory_order_relaxed); del_thread_req = true;}}
	int service_thread_num() const {return real_thread_num.load(boost::memory_order_relaxed);}
#endif

protected:
	void do_service(int thread_num)
	{
#ifdef ST_ASIO_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION >= 101100
		work = boost::make_shared<boost::asio::executor_work_guard<executor_type>>(get_executor());
#else
		work = boost::make_shared<boost::asio::io_service::work>(boost::ref(*this));
#endif
#endif
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
		del_thread_num.store(0, boost::memory_order_relaxed);

		unified_out::info_out("service pump end.");
	}

	void stop_and_free(object_type i_service_)
	{
		assert(NULL != i_service_);

		i_service_->stop_service();
		free(i_service_);
	}
	virtual void free(object_type i_service_) {} //if needed, rewrite this to free the service

#ifdef ST_ASIO_ENHANCED_STABILITY
	virtual bool on_exception(const boost::system::system_error& e)
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
			if (del_thread_req)
			{
				if (del_thread_num.fetch_sub(1, boost::memory_order_relaxed) > 0)
					break;
				else
				{
					del_thread_req = false;
					del_thread_num.fetch_add(1, boost::memory_order_relaxed);
				}
			}

			//we cannot always decrease service thread timely (because run_one can block).
			size_t this_n = 0;
#ifdef ST_ASIO_ENHANCED_STABILITY
			try {this_n = boost::asio::io_service::run_one();} catch (const boost::system::system_error& e) {if (!on_exception(e)) break;}
#else
			this_n = boost::asio::io_service::run_one();
#endif
			if (this_n > 0)
				n += this_n; //n can overflow, please note.
			else
				break;
		}
		--real_thread_num;
		os.str(""); os << "service thread[" << boost::this_thread::get_id() << "] end.";
		unified_out::info_out(os.str().data());

		return n;
	}
#else
#ifdef ST_ASIO_ENHANCED_STABILITY
	size_t run() {while (true) {try {return boost::asio::io_service::run();} catch (const boost::system::system_error& e) {if (!on_exception(e)) return 0;}}}
#endif
#endif

	DO_SOMETHING_TO_ALL_MUTEX(service_can, service_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(service_can, service_can_mutex)

private:
	void add(object_type i_service_)
	{
		assert(NULL != i_service_);

		boost::lock_guard<boost::mutex> lock(service_can_mutex);
		service_can.emplace_back(i_service_);
	}

protected:
	bool started;

	container_type service_can;
	boost::mutex service_can_mutex;

	boost::thread_group service_threads;
	atomic_int_fast32_t real_thread_num;
	atomic_int_fast32_t del_thread_num;
	bool del_thread_req;

#ifdef ST_ASIO_AVOID_AUTO_STOP_SERVICE
#if ASIO_VERSION >= 101100
	boost::shared_ptr<boost::asio::executor_work_guard<executor_type>> work;
#else
	boost::shared_ptr<boost::asio::io_service::work> work;
#endif
#endif
};

} //namespace

#endif /* ST_ASIO_SERVICE_PUMP_H_ */

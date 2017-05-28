/*
 * st_asio_wrapper_object_pool.h
 *
 *  Created on: 2013-8-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both TCP and UDP socket
 * this class can only manage objects that inherit from st_socket
 */

#ifndef ST_ASIO_WRAPPER_OBJECT_POOL_H_
#define ST_ASIO_WRAPPER_OBJECT_POOL_H_

#include <boost/unordered_map.hpp>

#include "st_asio_wrapper_timer.h"
#include "st_asio_wrapper_service_pump.h"

#ifndef ST_ASIO_MAX_OBJECT_NUM
#define ST_ASIO_MAX_OBJECT_NUM	4096
#endif
static_assert(ST_ASIO_MAX_OBJECT_NUM > 0, "object capacity must be bigger than zero.");

//ST_ASIO_RESTORE_OBJECT has the same effects as macro ST_ASIO_REUSE_OBJECT (it will overwrite the latter), except:
//reuse will not happen when create new connections, but just happen when invoke i_server::restore_socket.
//you may ask, for what purpose we introduced this feature?
//consider following situation:
//if a specific link is down, and the client has reconnected to the server, on the server side, how does the new st_server_socket_base
//restore all user data (because you don't want to nor need to reestablish them) and keep its id?
//before this feature been introduced, it's almost impossible.
//according to above explanation, we know that:
//1. like object pool, only objects in invalid_object_can can be restored;
//2. client need to inform st_server_socket_base the former id (or something else which can be used to calculate the former id
//   on the server side) after reconnected to the server;
//3. this feature needs user's support (send former id to server side on client side, invoke i_server::restore_socket in st_server_socket_base);
//4. do not define this macro on client side nor for UDP.

//define ST_ASIO_REUSE_OBJECT or ST_ASIO_RESTORE_OBJECT macro will enable object pool, all objects in invalid_object_can will
// never be freed, but kept for reuse, otherwise, st_object_pool will free objects in invalid_object_can automatically and periodically,
//ST_ASIO_FREE_OBJECT_INTERVAL means the interval, unit is second, see invalid_object_can in st_object_pool class for more details.
#if !defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
	#ifndef ST_ASIO_FREE_OBJECT_INTERVAL
	#define ST_ASIO_FREE_OBJECT_INTERVAL	60 //seconds
	#elif ST_ASIO_FREE_OBJECT_INTERVAL <= 0
		#error free object interval must be bigger than zero.
	#endif
#endif

//define ST_ASIO_CLEAR_OBJECT_INTERVAL macro to let st_object_pool to invoke clear_obsoleted_object() automatically and periodically
//this feature may affect performance with huge number of objects, so re-write st_server_socket_base::on_recv_error and invoke st_object_pool::del_object()
//is recommended for long-term connection system, but for short-term connection system, you are recommended to open this feature.
//you must define this macro as a value, not just define it, the value means the interval, unit is second
//#define ST_ASIO_CLEAR_OBJECT_INTERVAL		60 //seconds
#if defined(ST_ASIO_CLEAR_OBJECT_INTERVAL) && ST_ASIO_CLEAR_OBJECT_INTERVAL <= 0
	#error clear object interval must be bigger than zero.
#endif

namespace st_asio_wrapper
{

template<typename Object>
class st_object_pool : public st_service_pump::i_service, protected st_timer
{
public:
	typedef boost::shared_ptr<Object> object_type;
	typedef const object_type object_ctype;
	typedef boost::unordered::unordered_map<uint_fast64_t, object_type> container_type;

	static const tid TIMER_BEGIN = st_timer::TIMER_END;
	static const tid TIMER_FREE_SOCKET = TIMER_BEGIN;
	static const tid TIMER_CLEAR_SOCKET = TIMER_BEGIN + 1;
	static const tid TIMER_END = TIMER_BEGIN + 10;

protected:
	st_object_pool(st_service_pump& service_pump_) : i_service(service_pump_), st_timer(service_pump_), cur_id(-1), max_size_(ST_ASIO_MAX_OBJECT_NUM) {}

	void start()
	{
#if !defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
		set_timer(TIMER_FREE_SOCKET, 1000 * ST_ASIO_FREE_OBJECT_INTERVAL, [this](tid id)->bool {this->free_object(); return true;});
#endif
#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
		set_timer(TIMER_CLEAR_SOCKET, 1000 * ST_ASIO_CLEAR_OBJECT_INTERVAL, [this](tid id)->bool {this->clear_obsoleted_object(); return true;});
#endif
	}

	void stop() {stop_all_timer();}

	bool add_object(object_ctype& object_ptr)
	{
		assert(object_ptr && !object_ptr->is_equal_to(-1));

		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		return object_can.size() < max_size_ ? object_can.emplace(object_ptr->id(), object_ptr).second : false;
	}

	//only add object_ptr to invalid_object_can when it's in object_can, this can avoid duplicated items in invalid_object_can, because invalid_object_can is a list,
	//there's no way to check the existence of an item in a list efficiently.
	bool del_object(object_ctype& object_ptr)
	{
		assert(object_ptr);

		boost::unique_lock<boost::mutex> lock(object_can_mutex);
		auto exist = object_can.erase(object_ptr->id()) > 0;
		lock.unlock();

		if (exist)
		{
			boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
			invalid_object_can.emplace_back(object_ptr);
		}

		return exist;
	}

	virtual void on_create(object_ctype& object_ptr) {}

	void init_object(object_ctype& object_ptr)
	{
		if (object_ptr)
		{
			object_ptr->id(1 + cur_id.fetch_add(1, boost::memory_order_relaxed));
			on_create(object_ptr);
		}
		else
			unified_out::error_out("create object failed!");
	}

	//change object_ptr's id to id, and reinsert it into object_can.
	//there MUST exist an object in invalid_object_can whose id is equal to id to guarantee the id has been abandoned
	// (checking existence of such object in object_can is NOT enough, because there're some sockets used by async
	// acception, they don't exist in object_can nor invalid_object_can), further more, the invalid object MUST be
	//obsoleted and has no additional reference.
	//return the invalid object (null means failure), please note that the invalid object has been removed from invalid_object_can.
	object_type change_object_id(object_ctype& object_ptr, uint_fast64_t id)
	{
		assert(object_ptr && !object_ptr->is_equal_to(-1));

		boost::unique_lock<boost::mutex> lock(invalid_object_can_mutex);
		auto iter = std::find_if(std::begin(invalid_object_can), std::end(invalid_object_can), [id](object_ctype& item) {return item->is_equal_to(id);});
		//cannot use invalid_object_pop(uint_fast64_t), it's too arbitrary
		if (iter != std::end(invalid_object_can) && (*iter).unique() && (*iter)->obsoleted())
		{
			auto invalid_object_ptr(std::move(*iter));
			invalid_object_can.erase(iter);
			lock.unlock();

			assert(!find(id));

			boost::lock_guard<boost::mutex> lock(object_can_mutex);
			object_can.erase(object_ptr->id());
			object_ptr->id(id);
			object_can.emplace(id, object_ptr); //must succeed

			return invalid_object_ptr;
		}

		return object_type();
	}

#if defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
	object_type reuse_object()
	{
		boost::unique_lock<boost::mutex> lock(invalid_object_can_mutex);
		for (auto iter = std::begin(invalid_object_can); iter != std::end(invalid_object_can); ++iter)
			if ((*iter).unique() && (*iter)->obsoleted())
			{
				auto object_ptr(std::move(*iter));
				invalid_object_can.erase(iter);
				lock.unlock();

				object_ptr->reset();
				return object_ptr;
			}

		return object_type();
	}

	template<typename Arg>
	object_type create_object(Arg& arg)
	{
		auto object_ptr = reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg);

		init_object(object_ptr);
		return object_ptr;
	}

	template<typename Arg1, typename Arg2>
	object_type create_object(Arg1& arg1, Arg2& arg2)
	{
		auto object_ptr = reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg1, arg2);

		init_object(object_ptr);
		return object_ptr;
	}
#else
	template<typename Arg>
	object_type create_object(Arg& arg)
	{
		auto object_ptr = boost::make_shared<Object>(arg);
		init_object(object_ptr);
		return object_ptr;
	}

	template<typename Arg1, typename Arg2>
	object_type create_object(Arg1& arg1, Arg2& arg2)
	{
		auto object_ptr = boost::make_shared<Object>(arg1, arg2);
		init_object(object_ptr);
		return object_ptr;
	}
#endif

	object_type create_object() {return create_object(sp);}

public:
	//to configure unordered_set(for example, set factor or reserved size), not thread safe, so must be called before service_pump startup.
	container_type& container() {return object_can;}

	size_t max_size() const {return max_size_;}
	void max_size(size_t _max_size) {max_size_ = _max_size;}

	size_t size()
	{
		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		return object_can.size();
	}

	object_type find(uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		auto iter = object_can.find(id);
		return iter != std::end(object_can) ? iter->second : object_type();
	}

	//this method has linear complexity, please note.
	object_type at(size_t index)
	{
		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		assert(index < object_can.size());
		return index < object_can.size() ? std::next(std::begin(object_can), index)->second : object_type();
	}

	size_t invalid_object_size()
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		return invalid_object_can.size();
	}

	//this method has linear complexity, please note.
	object_type invalid_object_find(uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		auto iter = std::find_if(std::begin(invalid_object_can), std::end(invalid_object_can), [id](object_ctype& item) {return item->is_equal_to(id);});
		return iter == std::end(invalid_object_can) ? object_type() : *iter;
	}

	//this method has linear complexity, please note.
	object_type invalid_object_at(size_t index)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		assert(index < invalid_object_can.size());
		return index < invalid_object_can.size() ? *std::next(std::begin(invalid_object_can), index) : object_type();
	}

	//this method has linear complexity, please note.
	object_type invalid_object_pop(uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		auto iter = std::find_if(std::begin(invalid_object_can), std::end(invalid_object_can), [id](object_ctype& item) {return item->is_equal_to(id);});
		if (iter != std::end(invalid_object_can))
		{
			auto object_ptr(std::move(*iter));
			invalid_object_can.erase(iter);
			return object_ptr;
		}
		return object_type();
	}

	void list_all_object() {do_something_to_all([](object_ctype& item) {item->show_info("", "");});}

	//Kick out obsoleted objects
	//Consider the following assumptions:
	//1.You didn't invoke del_object in on_recv_error or other places.
	//2.For some reason(I haven't met yet), on_recv_error not been invoked
	//st_object_pool will automatically invoke this function if ST_ASIO_CLEAR_OBJECT_INTERVAL been defined
	size_t clear_obsoleted_object()
	{
		decltype(invalid_object_can) objects;

		boost::unique_lock<boost::mutex> lock(object_can_mutex);
		for (auto iter = std::begin(object_can); iter != std::end(object_can);)
			if (iter->second->obsoleted())
			{
				objects.emplace_back(std::move(iter->second));
				iter = object_can.erase(iter);
			}
			else
				++iter;
		lock.unlock();

		auto size = objects.size();
		if (0 != size)
		{
			unified_out::warning_out(ST_ASIO_SF " object(s) been kicked out!", size);

			boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
			invalid_object_can.splice(std::end(invalid_object_can), objects);
		}

		return size;
	}

	//free a specific number of objects
	//if you used object pool(define ST_ASIO_REUSE_OBJECT or ST_ASIO_RESTORE_OBJECT), you can manually call this function to free some objects
	// after the object pool(invalid_object_size()) goes big enough for memory saving (because the objects in invalid_object_can
	// are waiting for reusing and will never be freed).
	//if you don't used object pool, st_object_pool will invoke this function automatically and periodically, so you don't need to invoke this function exactly
	//return affected object number.
	size_t free_object(size_t num = -1)
	{
		size_t num_affected = 0;

		boost::unique_lock<boost::mutex> lock(invalid_object_can_mutex);
		for (auto iter = std::begin(invalid_object_can); num > 0 && iter != std::end(invalid_object_can);)
			if ((*iter)->obsoleted())
			{
				--num;
				++num_affected;
				iter = invalid_object_can.erase(iter);
			}
			else
				++iter;
		lock.unlock();

		if (num_affected > 0)
			unified_out::warning_out(ST_ASIO_SF " object(s) been freed!", num_affected);

		return num_affected;
	}

	template<typename _Predicate> void do_something_to_all(const _Predicate& __pred)
		{boost::lock_guard<boost::mutex> lock(object_can_mutex); for (typename container_type::value_type& item : object_can) __pred(item.second);}

	template<typename _Predicate> void do_something_to_one(const _Predicate& __pred)
		{boost::lock_guard<boost::mutex> lock(object_can_mutex); for (auto iter = std::begin(object_can); iter != std::end(object_can); ++iter) if (__pred(iter->second)) break;}

protected:
	st_atomic_uint_fast64 cur_id;

	container_type object_can;
	boost::mutex object_can_mutex;
	size_t max_size_;

	//because all objects are dynamic created and stored in object_can, maybe when receiving error occur
	//(you are recommended to delete the object from object_can, for example via i_server::del_socket), some other asynchronous calls are still queued in boost::asio::io_service,
	//and will be dequeued in the future, we must guarantee these objects not be freed from the heap or reused, so we move these objects from object_can to invalid_object_can,
	//and free them from the heap or reuse them in the near future.
	//if ST_ASIO_CLEAR_OBJECT_INTERVAL been defined, clear_obsoleted_object() will be invoked automatically and periodically to move all invalid objects into invalid_object_can.
	boost::container::list<object_type> invalid_object_can;
	boost::mutex invalid_object_can_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_OBJECT_POOL_H_ */

/*
 * object_pool.h
 *
 *  Created on: 2013-8-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both TCP and UDP socket
 * this class can only manage objects that inherit from st_asio_wrapper::socket
 */

#ifndef ST_ASIO_OBJECT_POOL_H_
#define ST_ASIO_OBJECT_POOL_H_

#include <boost/unordered_map.hpp>

#include "timer.h"
#include "service_pump.h"

namespace st_asio_wrapper
{

template<typename Object>
class object_pool : public service_pump::i_service, protected timer
{
public:
	typedef boost::shared_ptr<Object> object_type;
	typedef const object_type object_ctype;
	typedef boost::unordered::unordered_map<boost::uint_fast64_t, object_type> container_type;

	static const tid TIMER_BEGIN = timer::TIMER_END;
	static const tid TIMER_FREE_SOCKET = TIMER_BEGIN;
	static const tid TIMER_CLEAR_SOCKET = TIMER_BEGIN + 1;
	static const tid TIMER_END = TIMER_BEGIN + 10;

protected:
	object_pool(service_pump& service_pump_) : i_service(service_pump_), timer(service_pump_), cur_id(-1), max_size_(ST_ASIO_MAX_OBJECT_NUM) {}

	void start()
	{
#if !defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
		set_timer(TIMER_FREE_SOCKET, 1000 * ST_ASIO_FREE_OBJECT_INTERVAL, (boost::lambda::bind(&object_pool::free_object, this, -1), true));
#endif
#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
		set_timer(TIMER_CLEAR_SOCKET, 1000 * ST_ASIO_CLEAR_OBJECT_INTERVAL, (boost::lambda::bind(&object_pool::clear_obsoleted_object, this), true));
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
		bool exist = object_can.erase(object_ptr->id()) > 0;
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
	object_type change_object_id(object_ctype& object_ptr, boost::uint_fast64_t id)
	{
		assert(object_ptr && !object_ptr->is_equal_to(-1));

		BOOST_AUTO(old_object_ptr, invalid_object_pop(id));
		if (old_object_ptr)
		{
			assert(!find(id));

			boost::lock_guard<boost::mutex> lock(object_can_mutex);
			object_can.erase(object_ptr->id());
			object_ptr->id(id);
			object_can.emplace(id, object_ptr); //must succeed
		}

		return old_object_ptr;
	}

#if defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_RESTORE_OBJECT)
	object_type reuse_object()
	{
		BOOST_AUTO(object_ptr, invalid_object_pop());
		if (object_ptr)
			object_ptr->reset();

		return object_ptr;
	}

	template<typename Arg>
	object_type create_object(Arg& arg)
	{
		BOOST_AUTO(object_ptr, reuse_object());
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg);

		init_object(object_ptr);
		return object_ptr;
	}

	template<typename Arg1, typename Arg2>
	object_type create_object(Arg1& arg1, Arg2& arg2)
	{
		BOOST_AUTO(object_ptr, reuse_object());
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg1, arg2);

		init_object(object_ptr);
		return object_ptr;
	}
#else
	template<typename Arg>
	object_type create_object(Arg& arg)
	{
		BOOST_AUTO(object_ptr, boost::make_shared<Object>(arg));
		init_object(object_ptr);
		return object_ptr;
	}

	template<typename Arg1, typename Arg2>
	object_type create_object(Arg1& arg1, Arg2& arg2)
	{
		BOOST_AUTO(object_ptr, boost::make_shared<Object>(arg1, arg2));
		init_object(object_ptr);
		return object_ptr;
	}
#endif

	object_type create_object() {return create_object(boost::ref(sp));}

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

	object_type find(boost::uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		BOOST_AUTO(iter, object_can.find(id));
		return iter != object_can.end() ? iter->second : object_type();
	}

	//this method has linear complexity, please note.
	object_type at(size_t index)
	{
		boost::lock_guard<boost::mutex> lock(object_can_mutex);
		assert(index < object_can.size());
		return index < object_can.size() ? boost::next(object_can.begin(), index)->second : object_type();
	}

	size_t invalid_object_size()
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		return invalid_object_can.size();
	}

	//this method has linear complexity, please note.
	object_type invalid_object_find(boost::uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		BOOST_AUTO(iter, std::find_if(invalid_object_can.begin(), invalid_object_can.end(), boost::bind(&Object::is_equal_to, _1, id)));
		return iter == invalid_object_can.end() ? object_type() : *iter;
	}

	//this method has linear complexity, please note.
	object_type invalid_object_at(size_t index)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		assert(index < invalid_object_can.size());
		return index < invalid_object_can.size() ? *boost::next(invalid_object_can.begin(), index) : object_type();
	}

	//this method has linear complexity, please note.
	object_type invalid_object_pop(boost::uint_fast64_t id)
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		BOOST_AUTO(iter, std::find_if(invalid_object_can.begin(), invalid_object_can.end(), boost::bind(&Object::is_equal_to, _1, id)));
		if (iter != invalid_object_can.end() && (*iter).unique() && (*iter)->obsoleted())
		{
			BOOST_AUTO(object_ptr, *iter);
			invalid_object_can.erase(iter);
			return object_ptr;
		}
		return object_type();
	}

	//this method has linear complexity, please note.
	object_type invalid_object_pop()
	{
		boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
		for (BOOST_AUTO(iter, invalid_object_can.begin()); iter != invalid_object_can.end(); ++iter)
			if ((*iter).unique() && (*iter)->obsoleted())
			{
				BOOST_AUTO(object_ptr, *iter);
				invalid_object_can.erase(iter);
				return object_ptr;
			}
		return object_type();
	}

	//Kick out obsoleted objects
	//Consider the following assumptions:
	//1.You didn't invoke del_object in on_recv_error or other places.
	//2.For some reason(I haven't met yet), on_recv_error not been invoked
	//object_pool will automatically invoke this function if ST_ASIO_CLEAR_OBJECT_INTERVAL been defined
	size_t clear_obsoleted_object()
	{
		BOOST_TYPEOF(invalid_object_can) objects;

		boost::unique_lock<boost::mutex> lock(object_can_mutex);
		for (BOOST_AUTO(iter, object_can.begin()); iter != object_can.end();)
			if (iter->second->obsoleted())
			{
				objects.emplace_back(iter->second);
				iter = object_can.erase(iter);
			}
			else
				++iter;
		lock.unlock();

		size_t size = objects.size();
		if (0 != size)
		{
			unified_out::warning_out(ST_ASIO_SF " object(s) been kicked out!", size);

			boost::lock_guard<boost::mutex> lock(invalid_object_can_mutex);
			invalid_object_can.splice(invalid_object_can.end(), objects);
		}

		return size;
	}

	//free a specific number of objects
	//if you used object pool(define ST_ASIO_REUSE_OBJECT or ST_ASIO_RESTORE_OBJECT), you can manually call this function to free some objects
	// after the object pool(invalid_object_size()) gets big enough for memory saving (because the objects in invalid_object_can
	// are waiting for reusing and will never be freed).
	//if you don't used object pool, object_pool will invoke this function automatically and periodically, so you don't need to invoke this function exactly
	//return affected object number.
	size_t free_object(size_t num = -1)
	{
		size_t num_affected = 0;

		boost::unique_lock<boost::mutex> lock(invalid_object_can_mutex);
		for (BOOST_AUTO(iter, invalid_object_can.begin()); num > 0 && iter != invalid_object_can.end();)
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

	void list_all_object() {do_something_to_all(boost::bind(&Object::show_info, _1, "", ""));}
	statistic get_statistic() {statistic stat; do_something_to_all(stat += boost::lambda::bind(&Object::get_statistic, *boost::lambda::_1)); return stat;}

	template<typename _Predicate> void do_something_to_all(const _Predicate& __pred)
		{boost::lock_guard<boost::mutex> lock(object_can_mutex); for (BOOST_AUTO(iter, object_can.begin()); iter != object_can.end(); ++iter) __pred(iter->second);}

	template<typename _Predicate> void do_something_to_one(const _Predicate& __pred)
		{boost::lock_guard<boost::mutex> lock(object_can_mutex); for (BOOST_AUTO(iter, object_can.begin()); iter != object_can.end(); ++iter) if (__pred(iter->second)) break;}

protected:
	atomic_uint_fast64 cur_id;

	container_type object_can;
	boost::mutex object_can_mutex;
	size_t max_size_;

	//because all objects are dynamic created and stored in object_can, maybe when receiving error occur
	//(you are recommended to delete the object from object_can, for example via i_server::del_socket), some other asynchronous calls are still queued in boost::asio::io_context,
	//and will be dequeued in the future, we must guarantee these objects not be freed from the heap or reused, so we move these objects from object_can to invalid_object_can,
	//and free them from the heap or reuse them in the near future.
	//if ST_ASIO_CLEAR_OBJECT_INTERVAL been defined, clear_obsoleted_object() will be invoked automatically and periodically to move all invalid objects into invalid_object_can.
	boost::container::list<object_type> invalid_object_can;
	boost::mutex invalid_object_can_mutex;
};

} //namespace

#endif /* ST_ASIO_OBJECT_POOL_H_ */

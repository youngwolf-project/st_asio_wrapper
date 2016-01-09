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

#include <boost/atomic.hpp>
#include <boost/unordered_set.hpp>

#include "st_asio_wrapper_timer.h"
#include "st_asio_wrapper_service_pump.h"

#ifndef MAX_OBJECT_NUM
#define MAX_OBJECT_NUM				4096
#endif

//something like memory pool, if you open REUSE_OBJECT, all object in temp_object_can will never be freed, but waiting for reuse
//or, st_object_pool will free the objects in temp_object_can automatically and periodically, use SOCKET_FREE_INTERVAL to set the interval,
//see temp_object_can at the end of st_object_pool class for more details.
#ifndef REUSE_OBJECT
	#ifndef SOCKET_FREE_INTERVAL
	#define SOCKET_FREE_INTERVAL	10 //seconds, validate only if REUSE_OBJECT not defined
	#endif
#endif

//define this to have st_server_socket_base invoke st_object_pool::clear_all_closed_object() automatically and periodically
//this feature may influence performance when huge number of objects exist,
//so, re-write st_server_socket_base::on_recv_error and invoke st_object_pool::del_object() is recommended in long connection system
//in short connection system, you are recommended to open this feature, use CLEAR_CLOSED_SOCKET_INTERVAL to set the interval
#ifdef AUTO_CLEAR_CLOSED_SOCKET
#ifndef CLEAR_CLOSED_SOCKET_INTERVAL
#define CLEAR_CLOSED_SOCKET_INTERVAL	60 //seconds, validate only if AUTO_CLEAR_CLOSED_SOCKET defined
#endif
#endif

#ifndef CLOSED_SOCKET_MAX_DURATION
	#define CLOSED_SOCKET_MAX_DURATION	5 //seconds
	//after this duration, the corresponding object can be freed from the heap or be reused
#endif

namespace st_asio_wrapper
{

template<typename Object>
class st_object_pool : public st_service_pump::i_service, public st_timer
{
public:
	typedef boost::shared_ptr<Object> object_type;
	typedef const object_type object_ctype;

protected:
	struct st_object_hasher
	{
	public:
		size_t operator()(object_ctype& object_ptr) const {return (size_t) object_ptr->id();}
		size_t operator()(uint_fast64_t id) const {return (size_t) id;}
	};

	struct st_object_equal
	{
	public:
		bool operator()(object_ctype& left, object_ctype& right) const {return left->id() == right->id();}
		bool operator()(uint_fast64_t id, object_ctype& right) const {return id == right->id();}
	};

public:
	typedef boost::unordered::unordered_set<object_type, st_object_hasher, st_object_equal> container_type;

protected:
	struct temp_object
	{
		const time_t closed_time;
		object_ctype object_ptr;

		temp_object(object_ctype& object_ptr_) : closed_time(time(nullptr)), object_ptr(object_ptr_) {assert(object_ptr);}

		bool is_timeout() const {return is_timeout(time(nullptr));}
		bool is_timeout(time_t now) const {return closed_time <= now - CLOSED_SOCKET_MAX_DURATION;}
	};

protected:
	st_object_pool(st_service_pump& service_pump_) : i_service(service_pump_), st_timer(service_pump_), cur_id(-1) {}

	void start()
	{
#ifndef REUSE_OBJECT
		set_timer(0, 1000 * SOCKET_FREE_INTERVAL, nullptr);
#endif
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		set_timer(1, 1000 * CLEAR_CLOSED_SOCKET_INTERVAL, nullptr);
#endif
	}

	void stop() {stop_all_timer();}

	bool add_object(object_ctype& object_ptr)
	{
		assert(object_ptr && &object_ptr->get_io_service() == &get_service_pump());

		boost::unique_lock<boost::shared_mutex> lock(object_can_mutex);
		return object_can.size() < MAX_OBJECT_NUM ? object_can.insert(object_ptr).second : false;
	}

	//only add object_ptr to temp_object_can when it's in object_can, this can avoid duplicated items in temp_object_can, because temp_object_can is a list, there's no way to check the existence
	//of an item in a list efficiently.
	bool del_object(object_ctype& object_ptr)
	{
		assert(object_ptr);

		boost::unique_lock<boost::shared_mutex> lock(object_can_mutex);
		auto exist = object_can.erase(object_ptr) > 0;
		lock.unlock();

		if (exist)
		{
			boost::unique_lock<boost::shared_mutex> lock(temp_object_can_mutex);
			temp_object_can.push_back(object_ptr);
		}

		return exist;
	}

	virtual object_type reuse_object()
	{
#ifdef REUSE_OBJECT
		boost::unique_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		//objects are order by time, so we don't have to go through all items in temp_object_can
		for (auto iter = std::begin(temp_object_can); iter != std::end(temp_object_can) && iter->is_timeout(); ++iter)
			if (iter->object_ptr.unique() && iter->object_ptr->obsoleted())
			{
				auto object_ptr(std::move(iter->object_ptr));
				temp_object_can.erase(iter);
				lock.unlock();

				object_ptr->reset();
				return object_ptr;
			}
#endif

		return object_type();
	}

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
#ifndef REUSE_OBJECT
		case 0:
			free_object();
			return true;
			break;
#endif
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		case 1:
			clear_all_closed_object();
			return true;
			break;
#endif
		case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

public:
	object_type create_object()
	{
		auto object_ptr = reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(service_pump);
		if (object_ptr)
			object_ptr->id(++cur_id);

		return object_ptr;
	}

	template<typename Arg>
	object_type create_object(Arg& arg)
	{
		auto object_ptr = reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg);
		if (object_ptr)
			object_ptr->id(++cur_id);

		return object_ptr;
	}

	//to configure unordered_set(for example, setup factor or reserved size), not locked the mutex, so must called before service_pump starting up.
	container_type& container() {return object_can;}

	size_t size()
	{
		boost::shared_lock<boost::shared_mutex> lock(object_can_mutex);
		return object_can.size();
	}

	size_t closed_object_size()
	{
		boost::shared_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		return temp_object_can.size();
	}

	//this method has linear complexity, please note.
	object_type at(size_t index)
	{
		boost::shared_lock<boost::shared_mutex> lock(object_can_mutex);
		assert(index < object_can.size());
		return index < object_can.size() ? *(std::next(std::begin(object_can), index)) : object_type();
	}

	//this method has linear complexity, please note.
	object_type closed_object_at(size_t index)
	{
		boost::shared_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		assert(index < temp_object_can.size());
		return index < temp_object_can.size() ? std::next(std::begin(temp_object_can), index)->object_ptr : object_type();
	}

	object_type find(uint_fast64_t id)
	{
		boost::shared_lock<boost::shared_mutex> lock(object_can_mutex);
		auto iter = object_can.find(id, st_object_hasher(), st_object_equal());
		return iter != std::end(object_can) ? *iter : object_type();
	}

	object_type closed_object_find(uint_fast64_t id)
	{
		boost::shared_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		for (auto iter = std::begin(temp_object_can); iter != std::end(temp_object_can); ++iter)
			if (id == iter->object_ptr->id())
				return iter->object_ptr;
		return object_type();
	}

	object_type closed_object_pop(uint_fast64_t id)
	{
		boost::shared_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		for (auto iter = std::begin(temp_object_can); iter != std::end(temp_object_can); ++iter)
			if (id == iter->object_ptr->id())
			{
				auto object_ptr = iter->object_ptr;
				temp_object_can.erase(iter);
				return object_ptr;
			}
		return object_type();
	}

	void list_all_object() {do_something_to_all([](object_ctype& item) {item->show_info("", ""); });}

	//Clear all closed objects from the set
	//Consider the following assumption:
	//1.You don't invoke del_object in on_recv_error and on_send_error, or close the socket in on_unpack_error
	//2.For some reason(I haven't met yet), on_recv_error, on_send_error and on_unpack_error not invoked
	//st_object_pool will automatically invoke this function if AUTO_CLEAR_CLOSED_SOCKET been defined
	size_t clear_all_closed_object()
	{
		container_type objects;

		boost::unique_lock<boost::shared_mutex> lock(object_can_mutex);
		for (auto iter = std::begin(object_can); iter != std::end(object_can);)
			if ((*iter).unique() && (*iter)->obsoleted())
			{
				objects.insert(*iter);
				iter = object_can.erase(iter);
			}
			else
				++iter;
		lock.unlock();

		auto size = objects.size();
		if (0 != size)
		{
			boost::unique_lock<boost::shared_mutex> lock(temp_object_can_mutex);
			temp_object_can.insert(std::end(temp_object_can), std::begin(objects), std::end(objects));
		}

		return size;
	}

	//free a specific number of objects
	//if you use object pool(define REUSE_OBJECT), you may need to free some objects after the object pool(get_closed_object_size()) goes big enough for memory saving
	//(because the objects in temp_object_can are waiting for reuse and will never be freed)
	//if you don't use object pool, st_object_pool will invoke this automatically and periodically, so you don't need to invoke this exactly
	void free_object(size_t num = -1)
	{
		if (0 == num)
			return;

		boost::unique_lock<boost::shared_mutex> lock(temp_object_can_mutex);
		//objects are order by time, so we don't have to go through all items in temp_object_can
		for (auto iter = std::begin(temp_object_can); num > 0 && iter != std::end(temp_object_can) && iter->is_timeout();)
			if (iter->object_ptr.unique() && iter->object_ptr->obsoleted())
			{
				iter = temp_object_can.erase(iter);
				--num;
			}
			else
				++iter;
	}

	DO_SOMETHING_TO_ALL_MUTEX(object_can, object_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(object_can, object_can_mutex)

protected:
	boost::atomic_uint_fast64_t cur_id;

	container_type object_can;
	boost::shared_mutex object_can_mutex;

	//because all objects are dynamic created and stored in object_can, maybe when receiving error occur
	//(you are recommended to delete the object from object_can, for example via st_server_base::del_client), some other asynchronous calls are still queued in boost::asio::io_service,
	//and will be dequeued in the future, we must guarantee these objects not be freed from the heap, so we move these objects from object_can to temp_object_can,
	//and free them from the heap in the near future, see CLOSED_SOCKET_MAX_DURATION macro for more details.
	//if AUTO_CLEAR_CLOSED_SOCKET been defined, clear_all_closed_object() will be invoked automatically and periodically to move all closed objects into temp_object_can.
	boost::container::list<temp_object> temp_object_can;
	boost::shared_mutex temp_object_can_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_OBJECT_POOL_H_ */

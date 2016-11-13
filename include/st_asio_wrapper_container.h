/*
 * st_asio_wrapper_container.h
 *
 *  Created on: 2016-10-12
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * containers.
 */

#ifndef ST_ASIO_WRAPPER_CONTAINER_H_
#define ST_ASIO_WRAPPER_CONTAINER_H_

#include <boost/thread.hpp>
#include <boost/container/list.hpp>
#include <boost/typeof/typeof.hpp>

#include "st_asio_wrapper.h"

//msg send and recv buffer's maximum size (list::size()), corresponding buffers are expanded dynamically, which means only allocate memory when needed.
#ifndef ST_ASIO_MAX_MSG_NUM
#define ST_ASIO_MAX_MSG_NUM		1024
#elif ST_ASIO_MAX_MSG_NUM <= 0
	#error message capacity must be bigger than zero.
#endif

#ifndef ST_ASIO_INPUT_QUEUE
#define ST_ASIO_INPUT_QUEUE lock_queue
#endif
#ifndef ST_ASIO_INPUT_CONTAINER
#define ST_ASIO_INPUT_CONTAINER list
#endif
#ifndef ST_ASIO_OUTPUT_QUEUE
#define ST_ASIO_OUTPUT_QUEUE lock_queue
#endif
#ifndef ST_ASIO_OUTPUT_CONTAINER
#define ST_ASIO_OUTPUT_CONTAINER list
#endif

namespace st_asio_wrapper
{

//st_asio_wrapper requires that container must take one and only one template argument.
template <class T>
class list : public boost::container::list<T>
{
protected:
	typedef boost::container::list<T> super;

public:
	list() {}
	list(size_t size) : super(size) {}
};

class dummy_lockable
{
public:
	typedef boost::lock_guard<dummy_lockable> lock_guard;

	//lockable, dummy
	void lock() const {}
	void unlock() const {}
};

class lockable
{
public:
	typedef boost::lock_guard<lockable> lock_guard;

	//lockable
	void lock() {mutex.lock();}
	void unlock() {mutex.unlock();}

private:
	boost::shared_mutex mutex;
};

//Container must at least has the following functions:
// Container() and Container(size_t) constructor
// size
// empty
// clear
// swap
// push_back(const T& item)
// push_back(T&& item)
// front
// pop_front
template<typename T, typename Container, typename Lockable>
class queue : public Container, public Lockable
{
public:
	typedef T data_type;
	typedef Container super;
	typedef queue<T, Container, Lockable> me;

	queue() {}
	queue(size_t size) : super(size) {}

	bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T&& item) {typename Lockable::lock_guard lock(*this); return enqueue_(std::move(item));}
	bool try_dequeue(T& item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}

	bool enqueue_(const T& item) {this->push_back(item); return true;}
	bool enqueue_(T&& item) {this->push_back(std::move(item)); return true;}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}
};

template<typename T, typename Container> using non_lock_queue = queue<T, Container, dummy_lockable>; //totally not thread safe
template<typename T, typename Container> using lock_queue = queue<T, Container, lockable>;

//it's not thread safe for 'other', please note. for 'dest', depends on 'Q'
template<typename Q>
size_t move_items_in(Q& dest, Q& other, size_t max_size = ST_ASIO_MAX_MSG_NUM)
{
	if (other.empty())
		return 0;

	auto cur_size = dest.size();
	if (cur_size >= max_size)
		return 0;

	size_t num = 0;
	typename Q::data_type item;

	typename Q::lock_guard lock(dest);
	while (cur_size < max_size && other.try_dequeue_(item)) //size not controlled accurately
	{
		dest.enqueue_(std::move(item));
		++cur_size;
		++num;
	}

	return num;
}

//it's not thread safe for 'other', please note. for 'dest', depends on 'Q'
template<typename Q, typename Q2>
size_t move_items_in(Q& dest, Q2& other, size_t max_size = ST_ASIO_MAX_MSG_NUM)
{
	if (other.empty())
		return 0;

	auto cur_size = dest.size();
	if (cur_size >= max_size)
		return 0;

	size_t num = 0;

	typename Q::lock_guard lock(dest);
	while (cur_size < max_size && !other.empty()) //size not controlled accurately
	{
		dest.enqueue_(std::move(other.front()));
		other.pop_front();
		++cur_size;
		++num;
	}

	return num;
}

template<typename _Can>
bool splice_helper(_Can& dest_can, _Can& src_can, size_t max_size = ST_ASIO_MAX_MSG_NUM)
{
	auto size = dest_can.size();
	if (size < max_size) //dest_can can hold more items.
	{
		size = max_size - size; //maximum items this time can handle
		auto begin_iter = std::begin(src_can), end_iter = std::end(src_can);
		if (src_can.size() > size) //some items left behind
		{
			auto left_num = src_can.size() - size;
			end_iter = left_num > size ? std::next(begin_iter, size) : std::prev(end_iter, left_num); //find the minimum movement
		}
		else
			size = src_can.size();
		//use size to avoid std::distance() call, so, size must correct
		dest_can.splice(std::end(dest_can), src_can, begin_iter, end_iter, size);

		return size > 0;
	}

	return false;
}

} //namespace

#endif /* ST_ASIO_WRAPPER_CONTAINER_H_ */

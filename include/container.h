/*
 * container.h
 *
 *  Created on: 2016-10-12
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * containers.
 */

#ifndef ST_ASIO_CONTAINER_H_
#define ST_ASIO_CONTAINER_H_

#include "base.h"

namespace st_asio_wrapper
{

class dummy_lockable
{
public:
	typedef boost::lock_guard<dummy_lockable> lock_guard;

	//lockable, dummy
	bool is_lockable() const {return false;}
	void lock() const {}
	void unlock() const {}
};

class lockable
{
public:
	typedef boost::lock_guard<lockable> lock_guard;

	//lockable
	bool is_lockable() const {return true;}
	void lock() {mutex.lock();}
	void unlock() {mutex.unlock();}

private:
	boost::mutex mutex; //boost::mutex is more efficient than boost::shared_mutex
};

//Container must at least has the following functions (like boost::container::list):
// Container() and Container(size_t) constructor
// empty, must be thread safe, but doesn't have to be consistent
// clear
// swap
// emplace_back(), must return the reference of the new item
// splice(iter, Container&)
// splice(iter, Container&, iter, iter)
// front
// pop_front
// back
// begin
// end
template<typename Container, typename Lockable> //thread safety depends on Container or Lockable
class queue : private Container, public Lockable
{
public:
	typedef typename Container::value_type value_type;
	typedef typename Container::size_type size_type;
	typedef typename Container::reference reference;
	typedef typename Container::const_reference const_reference;
	using Container::size;
	using Container::empty;

	queue() : total_size(0) {}
	queue(size_t capacity) : Container(capacity), total_size(0) {}

	//thread safe
	bool is_thread_safe() const {return Lockable::is_lockable();}
	size_t size_in_byte() const {return total_size;}
	void clear() {typename Lockable::lock_guard lock(*this); Container::clear(); total_size = 0;}
	void swap(Container& can)
	{
		size_t size_in_byte = st_asio_wrapper::get_size_in_byte(can);

		typename Lockable::lock_guard lock(*this);
		Container::swap(can);
		total_size = size_in_byte;
	}

	template<typename T> bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	template<typename T> bool enqueue(T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	void move_items_in(Container& src, size_t size_in_byte = 0) {typename Lockable::lock_guard lock(*this); move_items_in_(src, size_in_byte);}
	bool try_dequeue(reference item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}
	void move_items_out(Container& dest, size_t max_item_num = -1) {typename Lockable::lock_guard lock(*this); move_items_out_(dest, max_item_num);}
	void move_items_out(size_t max_size_in_byte, Container& dest) {typename Lockable::lock_guard lock(*this); move_items_out_(max_size_in_byte, dest);}
	template<typename _Predicate> void do_something_to_all(const _Predicate& __pred) {typename Lockable::lock_guard lock(*this); do_something_to_all_(__pred);}
	template<typename _Predicate> void do_something_to_one(const _Predicate& __pred) {typename Lockable::lock_guard lock(*this); do_something_to_one_(__pred);}
	//thread safe

	//not thread safe
	template<typename T> bool enqueue_(const T& item)
	{
		try
		{
			ST_THIS emplace_back(item);
			total_size += item.size();
		}
		catch (const std::exception& e)
		{
			unified_out::error_out("cannot hold more objects (%s)", e.what());
			return false;
		}

		return true;
	}

	template<typename T> bool enqueue_(T& item) //after this, item will becomes empty, please note.
	{
		try
		{
			size_t size = item.size();
			ST_THIS emplace_back().swap(item); //with c++0x, this can be emplace_back(item)
			total_size += size;
		}
		catch (const std::exception& e)
		{
			unified_out::error_out("cannot hold more objects (%s)", e.what());
			return false;
		}

		return true;
	}

	void move_items_in_(Container& src, size_t size_in_byte = 0)
	{
		if (0 == size_in_byte)
			size_in_byte = st_asio_wrapper::get_size_in_byte(src);
		else
			assert(st_asio_wrapper::get_size_in_byte(src) == size_in_byte);

		ST_THIS splice(ST_THIS end(), src);
		total_size += size_in_byte;
	}

	bool try_dequeue_(reference item) {if (ST_THIS empty()) return false; item.swap(ST_THIS front()); ST_THIS pop_front(); total_size -= item.size(); return true;}

	void move_items_out_(Container& dest, size_t max_item_num = -1)
	{
		if ((size_t) -1 == max_item_num)
		{
			dest.splice(dest.end(), *this);
			total_size = 0;
		}
		else if (max_item_num > 0)
		{
			size_t size = 0, index = 0;
			BOOST_AUTO(end_iter, ST_THIS begin());
			for (; end_iter != ST_THIS end() && index++ < max_item_num; ++end_iter)
				size += end_iter->size();

			move_items_out(dest, end_iter, size);
		}
	}

	void move_items_out_(size_t max_size_in_byte, Container& dest)
	{
		if ((size_t) -1 == max_size_in_byte)
			move_items_out_(dest);
		else
		{
			size_t size = 0;
			BOOST_AUTO(end_iter, ST_THIS begin());
			while (end_iter != ST_THIS end())
			{
				size += end_iter++->size();
				if (size >= max_size_in_byte)
					break;
			}

			move_items_out(dest, end_iter, size);
		}
	}

	template<typename _Predicate>
	void do_something_to_all_(const _Predicate& __pred) {for (BOOST_AUTO(iter, ST_THIS begin()); iter != ST_THIS end(); ++iter) __pred(*iter);}
	template<typename _Predicate>
	void do_something_to_all_(const _Predicate& __pred) const {for (BOOST_AUTO(iter, ST_THIS begin()); iter != ST_THIS end(); ++iter) __pred(*iter);}

	template<typename _Predicate>
	void do_something_to_one_(const _Predicate& __pred) {for (BOOST_AUTO(iter, ST_THIS begin()); iter != ST_THIS end(); ++iter) if (__pred(*iter)) break;}
	template<typename _Predicate>
	void do_something_to_one_(const _Predicate& __pred) const {for (BOOST_AUTO(iter, ST_THIS begin()); iter != ST_THIS end(); ++iter) if (__pred(*iter)) break;}
	//not thread safe

protected:
	void move_items_out(Container& dest, typename Container::const_iterator end_iter, size_t size)
	{
		if (end_iter == ST_THIS end())
			dest.splice(dest.end(), *this);
		else
			dest.splice(dest.end(), *this, ST_THIS begin(), end_iter);

		total_size -= size;
	}

private:
	size_t total_size;
};

//st_asio_wrapper requires that queue must take one and only one template argument
template<typename Container> class non_lock_queue : public queue<Container, dummy_lockable> //thread safety depends on Container
{
public:
	non_lock_queue() {}
	non_lock_queue(size_t capacity) : queue<Container, dummy_lockable>(capacity) {}
};
template<typename Container> class lock_queue : public queue<Container, lockable>
{
public:
	lock_queue() {}
	lock_queue(size_t capacity) : queue<Container, lockable>(capacity) {}
};

} //namespace

#endif /* ST_ASIO_CONTAINER_H_ */

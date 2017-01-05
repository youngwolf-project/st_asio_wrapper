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

#include "st_asio_wrapper_base.h"

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
// size (must be thread safe, but doesn't have to be coherent, std::list before gcc 5 doesn't meet this requirement, boost::container::list always does)
// empty (must be thread safe, but doesn't have to be coherent)
// clear
// swap
// emplace_back(const T& item)
// emplace_back()
// splice(Container::const_iterator, boost::container::list<T>&), after this, boost::container::list<T> must be empty
// front
// back
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

	using Container::size;
	using Container::clear;
	using Container::swap;

	//thread safe
	bool enqueue(const T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	bool enqueue(T& item) {typename Lockable::lock_guard lock(*this); return enqueue_(item);}
	void move_items_in(boost::container::list<T>& can) {typename Lockable::lock_guard lock(*this); move_items_in_(can);}
	bool try_dequeue(T& item) {typename Lockable::lock_guard lock(*this); return try_dequeue_(item);}

	//not thread safe
	bool enqueue_(const T& item) {this->emplace_back(item); return true;}
	bool enqueue_(T& item) {this->emplace_back(); this->back().swap(item); return true;} //after this, item will becomes empty, please note.
	void move_items_in_(boost::container::list<T>& can) {this->splice(this->end(), can);}
	bool try_dequeue_(T& item) {if (this->empty()) return false; item.swap(this->front()); this->pop_front(); return true;}
};

template<typename T, typename Container>
class non_lock_queue : public queue<T, Container, dummy_lockable> //totally not thread safe
{
protected:
	typedef queue<T, Container, dummy_lockable> super;

public:
	non_lock_queue() {}
	non_lock_queue(size_t size) : super(size) {}
};

template<typename T, typename Container>
class lock_queue : public queue<T, Container, lockable>
{
protected:
	typedef queue<T, Container, lockable> super;

public:
	lock_queue() {}
	lock_queue(size_t size) : super(size) {}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CONTAINER_H_ */

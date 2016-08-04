/*
 * st_asio_wrapper_ext.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * extensional, replaceable and indispensable components.
 */

#ifndef ST_ASIO_WRAPPER_EXT_H_
#define ST_ASIO_WRAPPER_EXT_H_

#include <string>
#include <sstream>
#include <boost/array.hpp>
#include <boost/container/set.hpp>

#include "../st_asio_wrapper_base.h"

namespace st_asio_wrapper { namespace ext {

//buffers who implemented i_buffer interface can be wrapped by replaceable_buffer
class string_buffer : public std::string, public i_buffer
{
public:
	virtual bool empty() const {return std::string::empty();}
	virtual size_t size() const {return std::string::size();}
	virtual const char* data() const {return std::string::data();}
};

class basic_buffer : public boost::noncopyable
{
public:
	basic_buffer() {do_detach();}
	basic_buffer(size_t len) {do_detach(); assign(len);}
	basic_buffer(basic_buffer&& other) {do_attach(other.buff, other.len, other.buff_len); other.do_detach();}
	virtual ~basic_buffer() {free();}

	void assign(size_t len) {free(); do_attach(new char[len], len, len);}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return nullptr == buff ? 0 : len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {swap(std::move(other));}
	void clear() {free();}

	//functions needed by packer and unpacker
	char* data() {return buff;}
	void swap(basic_buffer&& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(buff_len, other.buff_len);}
	bool size(size_t _len) {assert(_len <= buff_len); return (_len <= buff_len) ? (len = _len, true) : false;}
	size_t buffer_size() const {return nullptr == buff ? 0 : buff_len;}

protected:
	void do_attach(char* _buff, size_t _len, size_t _buff_len) {buff = _buff; len = _len; buff_len = _buff_len;}
	void do_detach() {buff = nullptr; len = buff_len = 0;}
	void free() {delete[] buff; do_detach();}

protected:
	char* buff;
	size_t len, buff_len;
};

class pooled_buffer : public basic_buffer
{
public:
	class i_memory_pool
	{
	public:
		typedef pooled_buffer buffer_type;
		typedef const buffer_type buffer_ctype;

		virtual buffer_type checkout(size_t block_size, bool best_fit = false) = 0;
		virtual void checkin(buffer_type& buff) = 0;
		virtual void checkin(buffer_type&& buff) = 0;
	};

public:
	pooled_buffer() : pool(nullptr) {}
	pooled_buffer(i_memory_pool* _pool) : pool(_pool) {}
	pooled_buffer(i_memory_pool* _pool, size_t len) : basic_buffer(len), pool(_pool) {}
	pooled_buffer(pooled_buffer&& other) : basic_buffer(std::move(other)), pool(other.pool) {other.pool = nullptr;}
	~pooled_buffer() {clear();}

	//redefine basic_buffer::clear(), it's very important, otherwise, this buffer will be freed from heap
	void clear() {if (nullptr != pool) pool->checkin(*this);}

	void swap(pooled_buffer& other) {swap(std::move(other));}
	void swap(pooled_buffer&& other) {basic_buffer::swap(other); std::swap(pool, other.pool);}

	void mem_pool(i_memory_pool& _pool) {pool = &_pool;}
	i_memory_pool* mem_pool() {return pool;}

protected:
	i_memory_pool* pool;
};

class memory_pool : public pooled_buffer::i_memory_pool
{
protected:
	class memory_pool_impl
	{
	public:
		memory_pool_impl(memory_pool& _pool_mgr) : allocated_times_(0), reused_times_(0), reclaimed_times_(0), pool_mgr(_pool_mgr) {}
		memory_pool_impl(memory_pool& _pool_mgr, size_t block_count, size_t block_size) : allocated_times_(0), reused_times_(0), reclaimed_times_(0), pool_mgr(_pool_mgr)
			{init_pool(block_count, block_size);}

		//not thread safe, and can call many times before using this memory pool
		void init_pool(size_t block_count, size_t block_size) {for (size_t i = 0; i < block_count; ++i) pool.insert(buffer_type(&pool_mgr, block_size)); allocated_times_ += block_count;}

		size_t available_size() {boost::shared_lock<boost::shared_mutex> lock(mutex); return pool.size();}
		uint_fast64_t available_buffer_size() {uint_fast64_t size = 0; do_something_to_all(pool, mutex, [&](buffer_ctype& item) {size += item.buffer_size();}); return size;}
		uint_fast64_t allocated_times() const {return allocated_times_;}
		uint_fast64_t reused_times() const {return reused_times_;}
		uint_fast64_t reclaimed_times() const {return reclaimed_times_;}

		buffer_type checkout(size_t block_size, bool best_fit = false)
		{
			boost::unique_lock<boost::shared_mutex> lock(mutex);
			if (!pool.empty())
			{
				auto max_buffer_size = std::begin(pool)->buffer_size();
				if (max_buffer_size >= block_size)
				{
					auto hit_iter = std::begin(pool); //worst fit
					if (best_fit && max_buffer_size > block_size)
					{
						hit_iter = std::end(pool);
						for (auto iter = pool.rbegin(); iter != pool.rend(); ++iter)
							if (iter->buffer_size() >= block_size)
							{
								hit_iter = std::prev(iter.base());
								break;
							}
					}

					if (hit_iter != std::end(pool))
					{
						auto buff(std::move(*hit_iter));
						buff.size(block_size);
						pool.erase(hit_iter);
						++reused_times_;

						return buff;
					}
				}
			}
			++allocated_times_;
			lock.unlock();

			return buffer_type(&pool_mgr, block_size);
		}

		void checkin(buffer_type& buff) {checkin(std::move(buff));}
		void checkin(buffer_type&& buff)
		{
			if (buff.buffer_size() > 0)
			{
				boost::unique_lock<boost::shared_mutex> lock(mutex);
				pool.insert(std::move(buff));
				++reclaimed_times_;
			}
		}

	protected:
		struct buffer_compare {bool operator()(buffer_ctype& left, buffer_ctype& right) const {return left.buffer_size() > right.buffer_size();}};
		boost::container::multiset<buffer_type, buffer_compare> pool;
		boost::shared_mutex mutex;
		uint_fast64_t allocated_times_, reused_times_, reclaimed_times_;

		memory_pool& pool_mgr;
	}; //class memory_pool_impl

	struct memory_pool_info
	{
		memory_pool_info(memory_pool& pool, size_t _min_size, size_t _max_size) : impl(pool), min_size(_min_size), max_size(_max_size) {assert(min_size < max_size);}

		memory_pool_impl impl;
		size_t min_size, max_size; //responsible for memory allocating with size in [min_size, max_size)
	};

	typedef boost::shared_ptr<memory_pool_info> object_type;
	typedef const object_type object_ctype;
	typedef boost::container::list<object_type> container_type;

public:
	memory_pool() : stopped(false) {pool_impls.push_back(boost::make_shared<memory_pool_info>(*this, 0, -1));}
	~memory_pool() {stopped = true;}

	void add_pool(size_t min_size, size_t max_size) {assert(min_size < max_size); pool_impls.push_front(boost::make_shared<memory_pool_info>(*this, min_size, max_size));}
	void init_pool(size_t block_count, size_t block_size)
	{
		for (auto iter = std::begin(pool_impls); iter != std::end(pool_impls); ++iter)
			if ((*iter)->min_size <= block_size && block_size <  (*iter)->max_size)
				return (*iter)->impl.init_pool(block_count, block_size);
	}

	size_t available_size() const {size_t size = 0; do_something_to_all(pool_impls, [&](object_ctype& item) {size += item->impl.available_size();}); return size;}
	uint_fast64_t available_buffer_size() const {uint_fast64_t size = 0; do_something_to_all(pool_impls, [&](object_ctype& item) {size += item->impl.available_buffer_size();}); return size;}
	uint_fast64_t allocated_times() const {uint_fast64_t size = 0; do_something_to_all(pool_impls, [&](object_ctype& item) {size += item->impl.allocated_times();}); return size;}
	uint_fast64_t reused_times() const {uint_fast64_t size = 0; do_something_to_all(pool_impls, [&](object_ctype& item) {size += item->impl.reused_times();}); return size;}
	uint_fast64_t reclaimed_times() const {uint_fast64_t size = 0; do_something_to_all(pool_impls, [&](object_ctype& item) {size += item->impl.reclaimed_times();}); return size;}
	std::string get_statistic() const
	{
		std::ostringstream s;
		s << "available size(blocks): " << available_size() << ", available size(total bytes): " << available_buffer_size() << std::endl
			<< "allocated times: " << allocated_times() << ", reused times: " << reused_times() << ", reclaimed times: " << reclaimed_times();

		return s.str();
	}

	virtual buffer_type checkout(size_t block_size, bool best_fit = false)
	{
		if (!stopped)
			for (auto iter = std::begin(pool_impls); iter != std::end(pool_impls); ++iter)
				if ((*iter)->min_size <= block_size && block_size <  (*iter)->max_size)
					return (*iter)->impl.checkout(block_size, best_fit);

		return buffer_type();
	}

	virtual void checkin(buffer_type& buff) {checkin(std::move(buff));}
	virtual void checkin(buffer_type&& buff)
	{
		if (!stopped)
			for (auto iter = std::begin(pool_impls); iter != std::end(pool_impls); ++iter)
				if ((*iter)->min_size <= buff.buffer_size() && buff.buffer_size() <  (*iter)->max_size)
					return (*iter)->impl.checkin(std::move(buff));
	}

protected:
	bool stopped;
	container_type pool_impls;
};

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

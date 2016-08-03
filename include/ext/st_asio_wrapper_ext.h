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

class basic_buffer
{
public:
	basic_buffer() {do_detach();}
	basic_buffer(size_t len) {do_detach(); assign(len);}
	basic_buffer(basic_buffer&& other) {do_attach(other.buff, other.len, other.buff_len); other.do_detach();}
	~basic_buffer() {free();}

	void assign(size_t len) {free(); do_attach(new char[len], len, len);}
	void free() {delete[] buff; do_detach();}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len);}
	void clear() {free();}

	//functions needed by packer and unpacker
	char* data() {return buff;}
	bool size(size_t _len) {assert(_len <= buff_len); return (_len <= buff_len) ? (len = _len, true) : false;}
	size_t buffer_size() const {return buff_len;}

protected:
	void do_attach(char* _buff, size_t _len, size_t _buff_len) {buff = _buff; len = _len; buff_len = _buff_len;}
	void do_detach() {buff = nullptr; len = buff_len = 0;}

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
		typedef pooled_buffer raw_buffer_type;
		typedef const raw_buffer_type raw_buffer_ctype;
		typedef boost::shared_ptr<raw_buffer_type> buffer_type;
		typedef const buffer_type buffer_ctype;

		virtual buffer_type checkout(size_t block_size, bool best_fit = false) = 0;
		virtual void checkin(raw_buffer_type& buff) = 0;
	};

public:
	pooled_buffer(i_memory_pool& _pool) : pool(_pool) {}
	pooled_buffer(i_memory_pool& _pool, size_t len) : basic_buffer(len), pool(_pool) {}
	pooled_buffer(pooled_buffer&& other) : basic_buffer(std::move(other)), pool(other.pool) {}
	~pooled_buffer() {pool.checkin(*this);}

protected:
	i_memory_pool& pool;
};

class memory_pool : public pooled_buffer::i_memory_pool
{
public:
	memory_pool() : stopped_(false) {}
	memory_pool(size_t block_count, size_t block_size) : stopped_(false) {init_pool(block_count, block_size);}

	void stop() {stopped_ = true;}
	bool stopped() const {return stopped_;}

	//not thread safe, and can call many times before using this memory pool
	void init_pool(size_t block_count, size_t block_size) {for (size_t i = 0; i < block_count; ++i) pool.insert(boost::make_shared<raw_buffer_type>(*this, block_size));}
	size_t available_size() {boost::shared_lock<boost::shared_mutex> lock(mutex); return pool.size();}
	uint_fast64_t available_buffer_size() {uint_fast64_t size = 0; do_something_to_all(pool, mutex, [&](buffer_ctype& item) {size += item->buffer_size();}); return size;}

public:
	virtual buffer_type checkout(size_t block_size, bool best_fit = false)
	{
		if (stopped())
			return buffer_type();

		boost::unique_lock<boost::shared_mutex> lock(mutex);
		if (!pool.empty())
		{
			auto max_buffer_size = (*std::begin(pool))->buffer_size();
			if (max_buffer_size >= block_size)
			{
				auto hit_iter = std::begin(pool); //worst fit
				if (best_fit && max_buffer_size > block_size)
				{
					hit_iter = std::end(pool);
					for (auto iter = pool.rbegin(); iter != pool.rend(); ++iter)
						if ((*iter)->buffer_size() >= block_size)
						{
							hit_iter = std::prev(iter.base());
							break;
						}
				}

				if (hit_iter != std::end(pool))
				{
					auto buff(std::move(*hit_iter));
					buff->size(block_size);
					pool.erase(hit_iter);

					return buff;
				}
			}
		}
		lock.unlock();

		return boost::make_shared<raw_buffer_type>(*this, block_size);
	}

	virtual void checkin(raw_buffer_type& buff)
	{
		if (stopped())
			return;

		boost::unique_lock<boost::shared_mutex> lock(mutex);
		pool.insert(boost::make_shared<raw_buffer_type>(std::move(buff)));
	}

protected:
	struct buffer_compare {bool operator()(buffer_ctype& left, buffer_ctype& right) const {return left->buffer_size() > right->buffer_size();}};

	boost::container::multiset<buffer_type, buffer_compare> pool;
	boost::shared_mutex mutex;
	bool stopped_;
};

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

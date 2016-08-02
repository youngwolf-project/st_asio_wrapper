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

class most_primitive_buffer
{
public:
	most_primitive_buffer() {do_detach();}
	most_primitive_buffer(size_t len) {do_detach(); assign(len);}
	most_primitive_buffer(most_primitive_buffer&& other) {do_attach(other.buff, other.len, other.buff_len); other.do_detach();}
	~most_primitive_buffer() {free();}

	void assign(size_t len) {free(); do_attach(new char[len], len, len);}
	void free() {delete[] buff; do_detach();}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return len;}
	const char* data() const {return buff;}
	void swap(most_primitive_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len);}
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

//not thread safe, please pay attention
class memory_pool
{
public:
	typedef most_primitive_buffer raw_object_type;
	typedef const raw_object_type raw_object_ctype;
	typedef boost::shared_ptr<most_primitive_buffer> object_type;
	typedef const object_type object_ctype;

	memory_pool() {}
	memory_pool(size_t block_count, size_t block_size) {init_pool(block_count, block_size);}
	//not thread safe
	void init_pool(size_t block_count, size_t block_size) {for (size_t i = 0; i < block_count; ++i) pool.push_back(boost::make_shared<most_primitive_buffer>(block_size));}

	size_t size() {boost::shared_lock<boost::shared_mutex> lock(mutex); return pool.size();} //memory block amount
	uint_fast64_t buffer_size() {uint_fast64_t size = 0; do_something_to_all(pool, mutex, [&](object_ctype& item) {size += item->buffer_size();}); return size;}

	object_type ask_memory(size_t block_size, bool best_fit = false)
	{
		boost::unique_lock<boost::shared_mutex> lock(mutex);

		object_type re;
		if (best_fit)
		{
			object_type candidate;
			size_t gap = -1;
			for (auto iter = std::begin(pool); iter != std::end(pool); ++iter)
				if (iter->unique() && (*iter)->buffer_size() >= block_size)
				{
					auto this_gap = (*iter)->buffer_size() - block_size;
					if (this_gap < gap)
					{
						candidate = *iter;
						if (0 == (gap = this_gap))
							break;
					}
				}
			re = candidate;
		}
		else
			for (auto iter = std::begin(pool); !re && iter != std::end(pool); ++iter)
				if (iter->unique() && (*iter)->buffer_size() >= block_size)
					re = *iter;

		if (re)
			re->size(block_size);
		else
		{
			re = boost::make_shared<most_primitive_buffer>(block_size);
			pool.push_back(re);
		}

		return re;
	}

protected:
	boost::container::list<object_type> pool;
	boost::shared_mutex mutex;
};

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

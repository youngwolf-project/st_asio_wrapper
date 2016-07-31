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
	most_primitive_buffer(most_primitive_buffer&& other) {do_attach(other.buff, other.len); other.do_detach();}
	~most_primitive_buffer() {free();}

	void assign(size_t _len) {free(); do_attach(new char[_len], _len);}
	void free() {delete buff; do_detach();}

	//the following five functions (char* data() is used by unpackers only, not counted in) are needed by st_asio_wrapper,
	//for other functions, depends on the implementation of your packer and unpacker.
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return len;}
	const char* data() const {return buff;}
	char* data() {return buff;}
	void swap(most_primitive_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len);}
	void clear() {free();}

protected:
	void do_attach(char* _buff, size_t _len) {buff = _buff; len = _len;}
	void do_detach() {buff = nullptr; len = 0;}

protected:
	char* buff;
	size_t len;
};

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

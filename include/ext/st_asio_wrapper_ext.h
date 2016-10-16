/*
 * st_asio_wrapper_ext.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * extensional common.
 */

#ifndef ST_ASIO_WRAPPER_EXT_H_
#define ST_ASIO_WRAPPER_EXT_H_

#include <string>

#include "../st_asio_wrapper_base.h"

namespace st_asio_wrapper { namespace ext {

//implement i_buffer interface, then string_buffer can be wrapped by replaceable_buffer
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
	~basic_buffer() {clear();}

	basic_buffer& operator=(basic_buffer&& other) {clear(); swap(other); return *this;}
	void assign(size_t len) {clear(); do_attach(new char[len], len, len);}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || nullptr == buff;}
	size_t size() const {return nullptr == buff ? 0 : len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(buff_len, other.buff_len);}
	void clear() {delete[] buff; do_detach();}

	//functions needed by packer and unpacker
	char* data() {return buff;}
	bool size(size_t _len) {assert(_len <= buff_len); return (_len <= buff_len) ? (len = _len, true) : false;}
	size_t buffer_size() const {return nullptr == buff ? 0 : buff_len;}

protected:
	void do_attach(char* _buff, size_t _len, size_t _buff_len) {buff = _buff; len = _len; buff_len = _buff_len;}
	void do_detach() {buff = nullptr; len = buff_len = 0;}

protected:
	char* buff;
	size_t len, buff_len;
};

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

/*
 * ext.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * extensional common.
 */

#ifndef ST_ASIO_EXT_H_
#define ST_ASIO_EXT_H_

#include "../base.h"

//the size of the buffer used when receiving msg, must equal to or larger than the biggest msg size,
//the bigger this buffer is, the more msgs can be received in one time if there are enough msgs buffered in the SOCKET.
//for unpackers who use fixed buffer, every unpacker has a fixed buffer with this size, every tcp::socket_base has an unpacker,
//so this size is not the bigger the better, bigger buffers may waste more memory.
//if you customized the packer and unpacker, the above principle maybe not right anymore, it should depends on your implementations.
#ifndef ST_ASIO_MSG_BUFFER_SIZE
#define ST_ASIO_MSG_BUFFER_SIZE	4000
#elif ST_ASIO_MSG_BUFFER_SIZE > 100 * 1024 * 1024
	#error invalid message buffer size.
#endif

//#define ST_ASIO_SCATTERED_RECV_BUFFER
//define this macro will introduce scatter-gather buffers when doing async read, it's very useful under certain situations (for example, ring buffer).
//this macro is used by unpackers only, it doesn't belong to st_asio_wrapper.

#ifdef ST_ASIO_HUGE_MSG
#define ST_ASIO_HEAD_TYPE	boost::uint32_t
#define ST_ASIO_HEAD_H2N	htonl
#define ST_ASIO_HEAD_N2H	ntohl
#if ST_ASIO_MSG_BUFFER_SIZE < 4
	#error invalid message buffer size.
#endif
#else
#define ST_ASIO_HEAD_TYPE	boost::uint16_t
#define ST_ASIO_HEAD_H2N	htons
#define ST_ASIO_HEAD_N2H	ntohs
#if ST_ASIO_MSG_BUFFER_SIZE < 2
	#error invalid message buffer size.
#endif
#endif
#define ST_ASIO_HEAD_LEN	(sizeof(ST_ASIO_HEAD_TYPE))

namespace st_asio_wrapper { namespace ext {

//implement i_buffer interface, then protocol can be changed at runtime, see demo file_server for more details.
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
	~basic_buffer() {clear();}

	void assign(size_t len) {clear(); do_attach(new char[len], len, len);}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || NULL == buff;}
	size_t size() const {return NULL == buff ? 0 : len;}
	const char* data() const {return buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(buff_len, other.buff_len);}
	void clear() {delete[] buff; do_detach();}

	//functions needed by packer and unpacker
	char* data() {return buff;}
	bool shrink_size(size_t _len) {assert(_len <= buff_len); return (_len <= buff_len) ? (len = _len, true) : false;}
	size_t buffer_size() const {return NULL == buff ? 0 : buff_len;}

protected:
	void do_attach(char* _buff, size_t _len, size_t _buff_len) {buff = _buff; len = _len; buff_len = _buff_len;}
	void do_detach() {buff = NULL; len = buff_len = 0;}

protected:
	char* buff;
	size_t len, buff_len;
};

}} //namespace

#endif /* ST_ASIO_EXT_H_ */

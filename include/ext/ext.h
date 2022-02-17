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

//a substitute of std::string, because std::string has a small defect which is terrible for unpacking scenario (and protobuf encoding),
// it cannot change its size without fill the extended buffers.
//please note that basic_buffer won't append '\0' to the end of the string (std::string will do), you must appen '\0' characters
// by your own if you want to print it via "%s" formatter, e.g. basic_buffer("123", 4), bb.append("abc", 4).
class basic_buffer
{
public:
	basic_buffer() {do_detach();}
	virtual ~basic_buffer() {clean();}
	explicit basic_buffer(size_t len) {do_detach(); resize(len);}
	basic_buffer(size_t count, char c) {do_detach(); append(count, c);}
	basic_buffer(const char* buff) {do_detach(); operator+=(buff);}
	basic_buffer(const char* buff, size_t len) {do_detach(); append(buff, len);}
	basic_buffer(const basic_buffer& other) {do_detach(); append(other.buff, other.len);}
	basic_buffer(const basic_buffer& other, size_t pos) {do_detach(); if (pos < other.len) append(boost::next(other.buff, pos), other.len - pos);}
	basic_buffer(const basic_buffer& other, size_t pos, size_t len)
	{
		do_detach();
		if (pos < other.len)
			append(boost::next(other.buff, pos), pos + len > other.len ? other.len - pos : len);
	}

	inline basic_buffer& operator=(char c) {resize(0); return operator+=(c);}
	inline basic_buffer& operator=(const char* buff) {resize(0); return operator+=(buff);}
	inline basic_buffer& operator=(const basic_buffer& other) {resize(0); return operator+=(other);}

	inline basic_buffer& operator+=(char c) {return append(c);}
	inline basic_buffer& operator+=(const char* buff) {return append(buff);}
	inline basic_buffer& operator+=(const basic_buffer& other) {return append(other);}

	basic_buffer& append(char c) {return append(1, c);}
	basic_buffer& append(size_t count, char c)
	{
		if (count > 0)
		{
			check_length(count);

			size_t capacity = len + count;
			if (capacity > cap)
				reserve(capacity);

			memset(boost::next(buff, len), c, count);
			len += (unsigned) count;
		}

		return *this;
	}

	basic_buffer& append(const basic_buffer& other) {return append(other.data(), other.size());}
	basic_buffer& append(const char* buff) {return append(buff, strlen(buff));}
	basic_buffer& append(const char* _buff, size_t _len)
	{
		if (NULL != _buff && _len > 0)
		{
			check_length(_len);

			size_t capacity = len + _len;
			if (capacity > cap)
				reserve(capacity);

			memcpy(boost::next(buff, len), _buff, _len);
			len += (unsigned) _len;
		}

		return *this;
	}

	//nonstandard, delete the last character if it's '\0' before appending another string.
	//this feature makes basic_buffer to be able to works as std::string, which will append '\0' automatically.
	//please note that the second verion of append2 still needs you to provide '\0' at the end of the fed string.
	//usage:
	/*
		basic_buffer bb("1"); //not include the '\0' character
		printf("%s\n", bb.data()); //not ok, random characters can be outputted after "1", use basic_buffer bb("1", 2) instead.
		bb.append2("23"); //include the '\0' character, but just append2, please note.
		printf("%s\n", bb.data()); //ok, additional '\0' has been added so no random characters can be outputted after "123".
		bb.append2("456", 4); //include the '\0' character
		printf("%s\n", bb.data()); //ok, additional '\0' has been added so no random characters can be outputted after "123456".
		bb.append2("789", 3); //not include the '\0' character
		printf("%s\n", bb.data()); //not ok, random characters can be outputted after "123456789"
		bb.append2("000");
		printf("%s\n", bb.data()); //ok, the last character '9' will not be deleted because it's not '\0', so the output is "123456789000".
		printf("%zu\n", bb.size());
		//the final length of bb is 13, just include the last '\0' character, all middle '\0' characters have been deleted.
	*/
	//by the way, deleting the last character which is not '\0' before appending another string is very efficient, please use it freely.
	basic_buffer& append2(const char* buff) {return append(buff, strlen(buff) + 1);} //include the '\0' character
	basic_buffer& append2(const char* _buff, size_t _len)
	{
		if (len > 0 && '\0' == *boost::next(buff, len - 1))
			resize(len - 1); //delete the last character if it's '\0'

		return append(_buff, _len);
	}

	basic_buffer& erase(size_t pos = 0, size_t len = UINT_MAX)
	{
		if (pos > size())
			throw "out of range.";

		if (len >= size())
			resize(pos);
		else
		{
			size_t start = size() - len;
			if (pos >= start)
				resize(pos);
			else
			{
				memmove(boost::next(buff, pos), boost::next(buff, pos + len), size() - pos - len);
				resize(size() - len);
			}
		}

		return *this;
	}

	void resize(size_t _len) //won't fill the extended buffers
	{
		if (_len > cap)
			reserve(_len);

		len = (unsigned) _len;
	}

	void assign(size_t len) {resize(len);}
	void assign(const char* buff, size_t len) {resize(0); append(buff, len);}

	void shrink_to_fit() {reserve(0);}
	void reserve(size_t capacity)
	{
		if (capacity > max_size())
			throw "too big memory request!";
		else if (capacity < len)
			capacity = len;
		else if (0 == capacity)
			capacity = 16;

		if (capacity != cap)
		{
#ifdef _MSC_VER
			if (cap < capacity && capacity < cap + cap / 2) //memory expansion strategy -- quoted from std::string.
			{
				capacity = cap + cap / 2;
#else
			if (cap < capacity && capacity < 2 * cap) //memory expansion strategy -- quoted from std::string.
			{
				capacity = 2 * cap;
#endif
				if (capacity > max_size())
					capacity = max_size();
			}

			size_t new_cap = capacity & (unsigned) (max_size() << 4);
			if (capacity > new_cap)
			{
				new_cap += 0x10;
				if (capacity > new_cap || new_cap > max_size()) //overflow
					new_cap = max_size();
			}

			if (new_cap != cap)
			{
				char* new_buff = (char*) realloc(buff, new_cap);
				if (NULL == new_buff)
					throw "no sufficient memory!";

				cap = (unsigned) new_cap;
				buff = new_buff;
			}
		}
	}

	size_t max_size() const {return UINT_MAX;}
	size_t capacity() const {return cap;}

	//the following five functions are needed by st_asio_wrapper
	void clear() {resize(0);}
	void clean() {free(buff); do_detach();}
	const char* data() const {return buff;}
	size_t size() const {return NULL == buff ? 0 : len;}
	bool empty() const {return 0 == len || NULL == buff;}
	void swap(basic_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len); std::swap(cap, other.cap);}

	//functions needed by packer and unpacker
	char* data() {return buff;}

protected:
	void do_attach(char* _buff, size_t _len, size_t capacity) {buff = _buff; len = (unsigned) _len; cap = (unsigned) capacity;}
	void do_detach() {buff = NULL; len = cap = 0;}

	void check_length(size_t add_len) {if (add_len > max_size() || max_size() - add_len < len) throw "too big memory request!";}

protected:
	char* buff;
	unsigned len, cap;
};

}} //namespace

#endif /* ST_ASIO_EXT_H_ */

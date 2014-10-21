/*
 * st_asio_wrapper_unpacker.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * unpacker base class
 */

#ifndef ST_ASIO_WRAPPER_UNPACKER_H_
#define ST_ASIO_WRAPPER_UNPACKER_H_

#include <boost/array.hpp>
#include <boost/container/list.hpp>

#include "st_asio_wrapper_base.h"

#ifdef HUGE_MSG
#define HEAD_TYPE	uint32_t
#define HEAD_N2H	ntohl
#else
#define HEAD_TYPE	uint16_t
#define HEAD_N2H	ntohs
#endif
#define HEAD_LEN	(sizeof(HEAD_TYPE))

namespace st_asio_wrapper
{

class i_unpacker
{
public:
	virtual void reset_unpacker_state() = 0;
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can) = 0;
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) = 0;
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() = 0;
};

class unpacker : public i_unpacker
{
public:
	unpacker() {reset_unpacker_state();}
	size_t used_buffer_size() const {return cur_data_len;} //how many data have been received
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means don't know

public:
	virtual void reset_unpacker_state() {cur_msg_len = -1; cur_data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		//len + msg
		cur_data_len += bytes_transferred;

		auto pnext = std::begin(raw_buff);
		auto unpack_ok = true;
		while (unpack_ok) //considering stick package problem, we need a loop
			if ((size_t) -1 != cur_msg_len)
			{
				//cur_msg_len now can be assigned in the completion_condition function, or in the following 'else if',
				//so, we must verify cur_msg_len at the very beginning of using it, not at the assignment as we do
				//before, please pay special attention
				if (cur_msg_len > MAX_MSG_LEN || cur_msg_len <= HEAD_LEN)
					unpack_ok = false;
				else if (cur_data_len >= cur_msg_len) //one msg received
				{
					msg_can.resize(msg_can.size() + 1);
					msg_can.back().assign(std::next(pnext, HEAD_LEN), cur_msg_len - HEAD_LEN);
					cur_data_len -= cur_msg_len;
					std::advance(pnext, cur_msg_len);
					cur_msg_len = -1;
				}
				else
					break;
			}
			else if (cur_data_len >= HEAD_LEN) //the msg's head been received, stick package found
				cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) pnext);
			else
				break;

		if (pnext == std::begin(raw_buff)) //we should have at lest got one msg.
			unpack_ok = false;
		else if (unpack_ok && cur_data_len > 0)
			memcpy(std::begin(raw_buff), pnext, cur_data_len); //left behind unparsed msg

		//when unpack failed, some successfully parsed msgs may still returned via msg_can(stick package), please note.
		if (!unpack_ok)
			reset_unpacker_state();

		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce async call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = cur_data_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		if ((size_t) -1 == cur_msg_len)
		{
			if (data_len >= HEAD_LEN) //the msg's head been received
			{
				cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) std::begin(raw_buff));
				if (cur_msg_len > MAX_MSG_LEN || cur_msg_len <= HEAD_LEN) //invalid msg, stop reading
					return 0;
			}
			else
				return MAX_MSG_LEN - data_len; //read as many as possible
		}

		return data_len >= cur_msg_len ? 0 : MAX_MSG_LEN - data_len;
		//read as many as possible except that we have already got a entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(cur_data_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + cur_data_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t cur_msg_len; //-1 means head has not received, so, doesn't know the whole msg length.
	size_t cur_data_len; //include head
};

class fixed_length_unpacker : public i_unpacker
{
public:
	fixed_length_unpacker() {reset_unpacker_state();}

	void fixed_length(size_t length) {assert(length > 0); _fixed_length = length;}
	size_t fixed_length() const {return _fixed_length;}

public:
	virtual void reset_unpacker_state() {cur_data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		//len + msg
		cur_data_len += bytes_transferred;
		if (cur_data_len < _fixed_length)
		{
			reset_unpacker_state();
			return false;
		}

		auto pnext = std::begin(raw_buff);
		while (cur_data_len >= _fixed_length) //considering stick package problem, we need a loop
		{
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(pnext, _fixed_length);
			cur_data_len -= _fixed_length;
			std::advance(pnext, _fixed_length);
		}

		if (cur_data_len > 0) //left behind unparsed msg
			memcpy(std::begin(raw_buff), pnext, cur_data_len);

		return true;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce async call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = cur_data_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		return data_len >= _fixed_length ? 0 : _fixed_length - data_len;
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(cur_data_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + cur_data_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t cur_data_len;
	size_t _fixed_length;
};

class prefix_suffix_unpacker : public i_unpacker
{
public:
	prefix_suffix_unpacker() {reset_unpacker_state();}

	void prefix_suffix(const std::string& prefix, const std::string& suffix)
		{_prefix = prefix; _suffix = suffix; assert(!suffix.empty()); assert(MAX_MSG_LEN > _prefix.size() + _suffix.size());}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

	//like strstr, except support \0 in the middle of mem and sub_mem
	static void* memmem(void* mem, size_t len, const void* sub_mem, size_t sub_len)
	{
		if (nullptr != mem && nullptr != sub_mem && sub_len <= len)
		{
			auto valid_len = len - sub_len;
			for (size_t i = 0; i <= valid_len; ++i, mem = (char*) mem + 1)
				if (0 == memcmp(mem, sub_mem, sub_len))
					return mem;
		}

		return nullptr;
	}

public:
	virtual void reset_unpacker_state() {cur_data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		//len + msg
		cur_data_len += bytes_transferred;

		auto min_len = _prefix.size() + _suffix.size();
		auto unpack_ok = true;
		auto pnext = std::begin(raw_buff);
		while (true)
		{
			if (cur_data_len >= _prefix.size() && 0 != memcmp(_prefix.data(), pnext, _prefix.size()))
			{
				unpack_ok = false;
				break;
			}
			else if (cur_data_len < min_len)
				break; //half-baked msg

			auto end = (char*) memmem(std::next(pnext, _prefix.size()), cur_data_len - _prefix.size(),
				_suffix.data(), _suffix.size());
			if (nullptr == end)
				break; //half-baked msg
			else
			{
				msg_can.resize(msg_can.size() + 1);
				auto msg_len = std::distance(pnext, end) - _prefix.size();
				msg_can.back().assign(std::next(pnext, _prefix.size()), msg_len);
				cur_data_len -= min_len + msg_len;
				std::advance(pnext, min_len + msg_len);
			}
		}

		if (pnext == std::begin(raw_buff)) //we should have at lest got one msg.
			unpack_ok = false;
		else if (unpack_ok && cur_data_len > 0)
			memcpy(std::begin(raw_buff), pnext, cur_data_len); //left behind unparsed msg

		//when unpack failed, some successfully parsed msgs may still returned via msg_can(stick package), please note.
		if (!unpack_ok)
			reset_unpacker_state();

		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce async call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = cur_data_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		if (data_len >= _prefix.size() && 0 != memcmp(_prefix.data(), std::begin(raw_buff), _prefix.size()))
			return 0; //invalid msg, stop reading

		auto min_len = _prefix.size() + _suffix.size();
		if (data_len >= min_len &&
			(nullptr != memmem(std::next(std::begin(raw_buff), _prefix.size()), data_len - _prefix.size(),
				_suffix.data(), _suffix.size()) || //got a msg
			data_len >= MAX_MSG_LEN)) //invalid msg, stop reading
			return 0;

		return MAX_MSG_LEN - data_len; //read as many as possible except that we have already got a entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(cur_data_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + cur_data_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t cur_data_len;
	std::string _prefix, _suffix;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UNPACKER_H_ */

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

template<typename MsgType>
class i_unpacker
{
protected:
	virtual ~i_unpacker() {}

public:
	virtual void reset_state() = 0;
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<MsgType>& msg_can) = 0;
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) = 0;
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() = 0;
};

class unpacker : public i_unpacker<std::string>
{
public:
	unpacker() {reset_state();}
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means don't know

	bool parse_msg(size_t bytes_transferred, boost::container::list<std::pair<const char*, size_t>>& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MAX_MSG_LEN);

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
				else if (remain_len >= cur_msg_len) //one msg received
				{
					msg_can.push_back(std::make_pair(std::next(pnext, HEAD_LEN), cur_msg_len - HEAD_LEN));
					remain_len -= cur_msg_len;
					std::advance(pnext, cur_msg_len);
					cur_msg_len = -1;
				}
				else
					break;
			}
			else if (remain_len >= HEAD_LEN) //the msg's head been received, stick package found
				cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) pnext);
			else
				break;

		if (pnext == std::begin(raw_buff)) //we should have at least got one msg.
			unpack_ok = false;

		return unpack_ok;
	}

public:
	virtual void reset_state() {cur_msg_len = -1; remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		boost::container::list<std::pair<const char*, size_t>> msg_pos_can;
		auto unpack_ok = parse_msg(bytes_transferred, msg_pos_can);
		do_something_to_all(msg_pos_can, [&](decltype(*std::begin(msg_pos_can))& item) {
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(item.first, item.second);
		});

		if (unpack_ok && remain_len > 0)
		{
			auto pnext = std::next(msg_pos_can.back().first, msg_pos_can.back().second);
			memcpy(std::begin(raw_buff), pnext, remain_len); //left behind unparsed msg
		}

		//if unpacking failed, successfully parsed msgs will still returned via msg_can(stick package), please note.
		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce asynchronous call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		if ((size_t) -1 == cur_msg_len && data_len >= HEAD_LEN) //the msg's head been received
		{
			cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) std::begin(raw_buff));
			if (cur_msg_len > MAX_MSG_LEN || cur_msg_len <= HEAD_LEN) //invalid msg, stop reading
				return 0;
		}

		return data_len >= cur_msg_len ? 0 : boost::asio::detail::default_max_transfer_size;
		//read as many as possible except that we have already got an entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

protected:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t cur_msg_len; //-1 means head has not received, so, doesn't know the whole msg length.
	size_t remain_len; //half-baked msg
};

class fixed_length_unpacker : public i_unpacker<std::string>
{
public:
	fixed_length_unpacker() {reset_state();}

	void fixed_length(size_t fixed_length) {assert(0 < fixed_length && fixed_length <= MAX_MSG_LEN); _fixed_length = fixed_length;}
	size_t fixed_length() const {return _fixed_length;}

public:
	virtual void reset_state() {remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MAX_MSG_LEN);

		auto pnext = std::begin(raw_buff);
		while (remain_len >= _fixed_length) //considering stick package problem, we need a loop
		{
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(pnext, _fixed_length);
			remain_len -= _fixed_length;
			std::advance(pnext, _fixed_length);
		}

		if (pnext == std::begin(raw_buff)) //we should have at least got one msg.
			return false;
		else if (remain_len > 0) //left behind unparsed msg
			memcpy(std::begin(raw_buff), pnext, remain_len);

		return true;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce asynchronous call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		return data_len >= _fixed_length ? 0 : boost::asio::detail::default_max_transfer_size;
		//read as many as possible except that we have already got an entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t _fixed_length;
	size_t remain_len; //half-baked msg
};

class prefix_suffix_unpacker : public i_unpacker<std::string>
{
public:
	prefix_suffix_unpacker() {reset_state();}

	void prefix_suffix(const std::string& prefix, const std::string& suffix) {assert(!suffix.empty() && prefix.size() + suffix.size() < MAX_MSG_LEN); _prefix = prefix; _suffix = suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

	size_t peek_msg(size_t data_len, const char* buff)
	{
		assert(nullptr != buff);

		if ((size_t) -1 == first_msg_len && data_len >= _prefix.size())
		{
			if (0 != memcmp(_prefix.data(), buff, _prefix.size()))
				return 0; //invalid msg, stop reading
			else
				first_msg_len = 0; //prefix been checked.
		}

		auto min_len = _prefix.size() + _suffix.size();
		if (data_len > min_len)
		{
			auto end = (const char*) memmem(std::next(buff, _prefix.size()), data_len - _prefix.size(), _suffix.data(), _suffix.size());
			if (nullptr != end)
			{
				first_msg_len = std::distance(buff, end) + _suffix.size(); //got a msg
				return 0;
			}
			else if (data_len >= MAX_MSG_LEN)
				return 0; //invalid msg, stop reading
		}

		return boost::asio::detail::default_max_transfer_size; //read as many as possible
	}

	//like strstr, except support \0 in the middle of mem and sub_mem
	static const void* memmem(const void* mem, size_t len, const void* sub_mem, size_t sub_len)
	{
		if (nullptr != mem && nullptr != sub_mem && sub_len <= len)
		{
			auto valid_len = len - sub_len;
			for (size_t i = 0; i <= valid_len; ++i, mem = (const char*) mem + 1)
				if (0 == memcmp(mem, sub_mem, sub_len))
					return mem;
		}

		return nullptr;
	}

public:
	virtual void reset_state() {first_msg_len = -1; remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MAX_MSG_LEN);

		auto min_len = _prefix.size() + _suffix.size();
		auto unpack_ok = true;
		auto pnext = std::begin(raw_buff);
		while ((size_t) -1 != first_msg_len && 0 != first_msg_len)
		{
			assert(first_msg_len > min_len);
			auto msg_len = first_msg_len - min_len;

			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(std::next(pnext, _prefix.size()), msg_len);
			remain_len -= first_msg_len;
			std::advance(pnext, first_msg_len);
			first_msg_len = -1;

			if (boost::asio::detail::default_max_transfer_size == peek_msg(remain_len, pnext))
				break;
			else if ((size_t) -1 == first_msg_len)
				unpack_ok = false;
		}

		if (pnext == std::begin(raw_buff)) //we should have at least got one msg.
			return false;
		else if (unpack_ok && remain_len > 0)
			memcpy(std::begin(raw_buff), pnext, remain_len); //left behind unparsed msg

		//if unpacking failed, successfully parsed msgs will still returned via msg_can(stick package), please note.
		return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce asynchronous call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = remain_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		return peek_msg(data_len, std::begin(raw_buff));
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	std::string _prefix, _suffix;
	size_t first_msg_len;
	size_t remain_len; //half-baked msg
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UNPACKER_H_ */

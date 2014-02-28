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
	virtual size_t used_buffer_size() const {return 0;} //how many data have been received
	virtual size_t current_msg_length() const {return -1;} //current msg's total length, -1 means don't know
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<std::string>& msg_can) = 0;
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) = 0;
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() = 0;
};

class unpacker : public i_unpacker
{
public:
	unpacker() {reset_unpacker_state();}

public:
	virtual void reset_unpacker_state() {cur_msg_len = -1; cur_data_len = 0;}
	virtual size_t used_buffer_size() const {return cur_data_len;}
	virtual size_t current_msg_length() const {return cur_msg_len;}
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
				//so, we must verify cur_msg_len at the very begining of using it, not at the assignment as we do
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

		if (!unpack_ok)
			reset_unpacker_state();
		else if (cur_data_len > 0 && pnext > std::begin(raw_buff)) //left behind unparsed msg
			memcpy(std::begin(raw_buff), pnext, cur_data_len);

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

} //namespace

#endif /* ST_ASIO_WRAPPER_UNPACKER_H_ */

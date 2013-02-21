/*
 * st_asio_wrapper_unpacker.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com QQ: 676218192
 *
 * unpacker base class
 */

#ifndef ST_ASIO_WRAPPER_UNPACKER_H_
#define ST_ASIO_WRAPPER_UNPACKER_H_

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/container/list.hpp>
using namespace boost::asio;

#include "st_asio_wrapper_base.h"

namespace st_asio_wrapper
{

class i_unpacker
{
public:
	virtual void reset_unpacker_state() = 0;
	virtual bool on_recv(size_t bytes_transferred, container::list<std::string>& msg_can) = 0;
	virtual mutable_buffers_1 prepare_next_recv(size_t& min_recv_len) = 0;
};

class unpacker : public i_unpacker
{
public:
	unpacker() {reset_unpacker_state();}

public:
	virtual void reset_unpacker_state() {total_data_len = -1; data_len = 0;}
	virtual bool on_recv(size_t bytes_transferred, container::list<std::string>& msg_can)
	{
		//len(unsigned) + msg
		data_len += bytes_transferred;

		auto pnext = std::begin(raw_buff);
		auto unpack_ok = true;
		while (unpack_ok)
		{
			if ((size_t) -1 != total_data_len)
			{
				if (data_len >= total_data_len) //one msg received
				{
					msg_can.push_back(std::string(std::next(pnext, HEAD_LEN), total_data_len - HEAD_LEN));
					data_len -= total_data_len;
					std::advance(pnext, total_data_len);
					total_data_len = -1;
				}
				else
					break;
			}
			else if (data_len >= HEAD_LEN) //the msg's head been received
			{
				total_data_len = ntohs(*(unsigned short*) pnext);
				if (total_data_len > MAX_MSG_LEN || total_data_len <= HEAD_LEN)
					unpack_ok = false;
			}
			else
				break;
		}

		if (!unpack_ok)
			reset_unpacker_state();
		else if (data_len > 0 && pnext > std::begin(raw_buff)) //left behind unparsed msg
			memcpy(std::begin(raw_buff), pnext, data_len);

		return unpack_ok;
	}

	virtual mutable_buffers_1 prepare_next_recv(size_t& min_recv_len)
	{
		assert(data_len < MAX_MSG_LEN);
		min_recv_len = ((size_t) -1 == total_data_len ? HEAD_LEN : total_data_len) - data_len;
		assert(0 < min_recv_len && min_recv_len <= MAX_MSG_LEN);
		//use mutable_buffer can protect raw_buff from accessing overflow
		return buffer(buffer(raw_buff) + data_len);
	}

private:
	array<char, MAX_MSG_LEN> raw_buff;
	size_t total_data_len; //-1 means head has not received
	size_t data_len; //include head
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UNPACKER_H_ */

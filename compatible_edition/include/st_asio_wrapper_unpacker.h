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
#define HEAD_TYPE	boost::uint32_t
#define HEAD_N2H	ntohl
#else
#define HEAD_TYPE	boost::uint16_t
#define HEAD_N2H	ntohs
#endif
#define HEAD_LEN	(sizeof(HEAD_TYPE))

namespace st_asio_wrapper
{

template<typename MsgType>
class i_unpacker
{
public:
	typedef boost::container::list<MsgType> container_type;

protected:
	virtual ~i_unpacker() {}

public:
	virtual void reset_state() = 0;
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can) = 0;
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) = 0;
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() = 0;
};

template<typename MsgType>
class i_udp_unpacker
{
protected:
	virtual ~i_udp_unpacker() {}

public:
	virtual void parse_msg(MsgType& msg, size_t bytes_transferred) = 0;
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() = 0;
};

class unpacker : public i_unpacker<std::string>
{
public:
	unpacker() {reset_state();}
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means don't know

	bool parse_msg(size_t bytes_transferred, boost::container::list<std::pair<const char*, size_t> >& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MSG_BUFFER_SIZE);

		const char* pnext = raw_buff.begin();
		bool unpack_ok = true;
		while (unpack_ok) //considering stick package problem, we need a loop
			if ((size_t) -1 != cur_msg_len)
			{
				//cur_msg_len now can be assigned in the completion_condition function, or in the following 'else if',
				//so, we must verify cur_msg_len at the very beginning of using it, not at the assignment as we do
				//before, please pay special attention
				if (cur_msg_len > MSG_BUFFER_SIZE || cur_msg_len <= HEAD_LEN)
					unpack_ok = false;
				else if (remain_len >= cur_msg_len) //one msg received
				{
					msg_can.push_back(std::make_pair(pnext + HEAD_LEN, cur_msg_len - HEAD_LEN));
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

		if (pnext == raw_buff.begin()) //we should have at least got one msg.
			unpack_ok = false;

		return unpack_ok;
	}

public:
	virtual void reset_state() {cur_msg_len = -1; remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		boost::container::list<std::pair<const char*, size_t> > msg_pos_can;
		bool unpack_ok = parse_msg(bytes_transferred, msg_pos_can);
		for (BOOST_AUTO(iter, msg_pos_can.begin()); iter != msg_pos_can.end(); ++iter)
		{
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(iter->first, iter->second);
		}

		if (unpack_ok && remain_len > 0)
		{
			const char* pnext = msg_pos_can.back().first + msg_pos_can.back().second;
			memcpy(raw_buff.begin(), pnext, remain_len); //left behind unparsed msg
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

		size_t data_len = remain_len + bytes_transferred;
		assert(data_len <= MSG_BUFFER_SIZE);

		if ((size_t) -1 == cur_msg_len && data_len >= HEAD_LEN) //the msg's head been received
		{
			cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) raw_buff.begin());
			if (cur_msg_len > MSG_BUFFER_SIZE || cur_msg_len <= HEAD_LEN) //invalid msg, stop reading
				return 0;
		}

		return data_len >= cur_msg_len ? 0 : boost::asio::detail::default_max_transfer_size;
		//read as many as possible except that we have already got an entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MSG_BUFFER_SIZE);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

protected:
	boost::array<char, MSG_BUFFER_SIZE> raw_buff;
	size_t cur_msg_len; //-1 means head has not received, so, doesn't know the whole msg length.
	size_t remain_len; //half-baked msg
};

class udp_unpacker : public i_udp_unpacker<std::string>
{
public:
	virtual void parse_msg(std::string& msg, size_t bytes_transferred) {assert(bytes_transferred <= MSG_BUFFER_SIZE); msg.assign(raw_buff.data(), bytes_transferred);}
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() {return boost::asio::buffer(raw_buff);}

protected:
	boost::array<char, MSG_BUFFER_SIZE> raw_buff;
};

class replaceable_unpacker : public i_unpacker<replaceable_buffer>, public unpacker
{
public:
	virtual void reset_state() {unpacker::reset_state();}
	virtual bool parse_msg(size_t bytes_transferred, i_unpacker<replaceable_buffer>::container_type& msg_can)
	{
		unpacker::container_type tmp_can;
		bool unpack_ok = unpacker::parse_msg(bytes_transferred, tmp_can);
		for (BOOST_AUTO(iter, tmp_can.begin()); iter != tmp_can.end(); ++iter)
		{
			BOOST_AUTO(com, boost::make_shared<buffer>());
			com->swap(*iter);
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().raw_buffer(com);
		}

		//when unpack failed, some successfully parsed msgs may still returned via msg_can(stick package), please note.
		return unpack_ok;
	}

	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) {return unpacker::completion_condition(ec, bytes_transferred);}
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() {return unpacker::prepare_next_recv();}
};

class replaceable_udp_unpacker : public i_udp_unpacker<replaceable_buffer>
{
public:
	virtual void parse_msg(replaceable_buffer& msg, size_t bytes_transferred)
	{
		assert(bytes_transferred <= MSG_BUFFER_SIZE);
		BOOST_AUTO(com, boost::make_shared<buffer>());
		com->assign(raw_buff.data(), bytes_transferred);
		msg.raw_buffer(com);
	}
	virtual boost::asio::mutable_buffers_1 prepare_next_recv() {return boost::asio::buffer(raw_buff);}

protected:
	boost::array<char, MSG_BUFFER_SIZE> raw_buff;
};

class fixed_length_unpacker : public i_unpacker<std::string>
{
public:
	fixed_length_unpacker() {reset_state();}

	void fixed_length(size_t fixed_length) {assert(0 < fixed_length && fixed_length <= MSG_BUFFER_SIZE); _fixed_length = fixed_length;}
	size_t fixed_length() const {return _fixed_length;}

public:
	virtual void reset_state() {remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MSG_BUFFER_SIZE);

		const char* pnext = raw_buff.begin();
		while (remain_len >= _fixed_length) //considering stick package problem, we need a loop
		{
			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(pnext, _fixed_length);
			remain_len -= _fixed_length;
			std::advance(pnext, _fixed_length);
		}

		if (pnext == raw_buff.begin()) //we should have at least got one msg.
			return false;
		else if (remain_len > 0) //left behind unparsed msg
			memcpy(raw_buff.begin(), pnext, remain_len);

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

		size_t data_len = remain_len + bytes_transferred;
		assert(data_len <= MSG_BUFFER_SIZE);

		return data_len >= _fixed_length ? 0 : boost::asio::detail::default_max_transfer_size;
		//read as many as possible except that we have already got an entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MSG_BUFFER_SIZE);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

private:
	boost::array<char, MSG_BUFFER_SIZE> raw_buff;
	size_t _fixed_length;
	size_t remain_len; //half-baked msg
};

class prefix_suffix_unpacker : public i_unpacker<std::string>
{
public:
	prefix_suffix_unpacker() {reset_state();}

	void prefix_suffix(const std::string& prefix, const std::string& suffix) {assert(!suffix.empty() && prefix.size() + suffix.size() < MSG_BUFFER_SIZE); _prefix = prefix; _suffix = suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

	size_t peek_msg(size_t data_len, const char* buff)
	{
		assert(NULL != buff);

		if ((size_t) -1 == first_msg_len && data_len >= _prefix.size())
		{
			if (0 != memcmp(_prefix.data(), buff, _prefix.size()))
				return 0; //invalid msg, stop reading
			else
				first_msg_len = 0; //prefix been checked.
		}

		size_t min_len = _prefix.size() + _suffix.size();
		if (data_len > min_len)
		{
			const char* end = (const char*) memmem(buff + _prefix.size(), data_len - _prefix.size(), _suffix.data(), _suffix.size());
			if (NULL != end)
			{
				first_msg_len = end - buff + _suffix.size(); //got a msg
				return 0;
			}
			else if (data_len >= MSG_BUFFER_SIZE)
				return 0; //invalid msg, stop reading
		}

		return boost::asio::detail::default_max_transfer_size; //read as many as possible
	}

	//like strstr, except support \0 in the middle of mem and sub_mem
	static const void* memmem(const void* mem, size_t len, const void* sub_mem, size_t sub_len)
	{
		if (NULL != mem && NULL != sub_mem && sub_len <= len)
		{
			size_t valid_len = len - sub_len;
			for (size_t i = 0; i <= valid_len; ++i, mem = (const char*) mem + 1)
				if (0 == memcmp(mem, sub_mem, sub_len))
					return mem;
		}

		return NULL;
	}

public:
	virtual void reset_state() {first_msg_len = -1; remain_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		//length + msg
		remain_len += bytes_transferred;
		assert(remain_len <= MSG_BUFFER_SIZE);

		size_t min_len = _prefix.size() + _suffix.size();
		bool unpack_ok = true;
		const char* pnext = raw_buff.begin();
		while ((size_t) -1 != first_msg_len && 0 != first_msg_len)
		{
			assert(first_msg_len > min_len);
			size_t msg_len = first_msg_len - min_len;

			msg_can.resize(msg_can.size() + 1);
			msg_can.back().assign(pnext + _prefix.size(), msg_len);
			remain_len -= first_msg_len;
			std::advance(pnext, first_msg_len);
			first_msg_len = -1;

			if (boost::asio::detail::default_max_transfer_size == peek_msg(remain_len, pnext))
				break;
			else if ((size_t) -1 == first_msg_len)
				unpack_ok = false;
		}

		if (pnext == raw_buff.begin()) //we should have at least got one msg.
			return false;
		else if (unpack_ok && remain_len > 0)
			memcpy(raw_buff.begin(), pnext, remain_len); //left behind unparsed msg

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

		size_t data_len = remain_len + bytes_transferred;
		assert(data_len <= MSG_BUFFER_SIZE);

		return peek_msg(data_len, raw_buff.begin());
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(remain_len < MSG_BUFFER_SIZE);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + remain_len);
	}

private:
	boost::array<char, MSG_BUFFER_SIZE> raw_buff;
	std::string _prefix, _suffix;
	size_t first_msg_len;
	size_t remain_len; //half-baked msg
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UNPACKER_H_ */

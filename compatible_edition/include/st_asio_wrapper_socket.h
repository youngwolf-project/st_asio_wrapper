/*
 * st_asio_wrapper_socket.h
 *
 *  Created on: 2013-8-4
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both tcp and udp socket
 */

#ifndef ST_ASIO_WRAPPER_SOCKET_H_
#define ST_ASIO_WRAPPER_SOCKET_H_

#include <boost/smart_ptr.hpp>
#include <boost/container/list.hpp>

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_timer.h"

namespace st_asio_wrapper
{

template<typename MsgType, typename Socket>
class st_socket: public Socket, public st_timer
{
public:
	st_socket(io_service& io_service_) : Socket(io_service_), st_timer(io_service_),
		packer_(boost::make_shared<packer>()) {reset_state();}
	virtual ~st_socket() {}

	virtual void start() = 0;
	void reset_state()
	{
		sending = false;
		dispatching = false;
		suspend_dispatch_msg_ = false;
	}

	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();
	}

	void suspend_dispatch_msg(bool suspend) {suspend_dispatch_msg_ = suspend;}
	bool suspend_dispatch_msg() const {return suspend_dispatch_msg_;}

	//get or change the packer at runtime
	boost::shared_ptr<i_packer> inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer>& _packer_) {packer_ = _packer_;};

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed
	//no matter whether the send buffer is available
	bool is_send_buffer_available()
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return send_msg_buffer.size() < MAX_MSG_NUM;
	}

	//how many msgs waiting for send
	size_t get_pending_msg_num()
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return send_msg_buffer.size();
	}

	//the msg's format please refer to on_msg_send
	void peek_first_pending_msg(MsgType& msg)
	{
		msg.clear();
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (!send_msg_buffer.empty())
			msg = send_msg_buffer.front();
	}

	//the msg's format please refer to on_msg_send
	void pop_first_pending_msg(MsgType& msg)
	{
		msg.clear();
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (!send_msg_buffer.empty())
		{
			msg.swap(send_msg_buffer.front());
			send_msg_buffer.pop_front();
		}
	}

	//clear all pending msgs
	void pop_all_pending_msg(container::list<MsgType>& unsend_msg_list)
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		unsend_msg_list.splice(unsend_msg_list.end(), send_msg_buffer);
	}

	//must used after the service stopped
	void direct_dispatch_all_msg()
	{
		//mutex::scoped_lock lock(recv_msg_buffer_mutex);
		if (!recv_msg_buffer.empty() || !temp_msg_buffer.empty())
		{
			recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer);
			st_asio_wrapper::do_something_to_all(recv_msg_buffer, boost::bind(&st_socket::on_msg_handle, this, _1));
			recv_msg_buffer.clear();
		}

		dispatching = false;
	}

protected:
	virtual bool is_send_allowed() const {return this->is_open();}
	//can send data or not(just put into send buffer)

	//generally, you need not re-write this for link broken judgment(tcp)
	virtual void on_send_error(const error_code& ec)
		{unified_out::error_out("send msg error: %d %s", ec.value(), ec.message().data());}

	#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own recv buffer, you can move the msg to your own recv buffer,
	//and return false, then, handle the msg as your own strategy(may be you'll need a msg dispatch thread)
	//or, you can handle the msg at here and return false, but this will reduce efficiency(
	//because this msg handling block the next msg receiving on the same st_tcp_socket) unless you can
	//handle the msg very fast(which will inversely more efficient, because msg recv buffer and msg dispatching
	//are not needed any more).
	//
	//return true means use the msg recv buffer, you must handle the msgs in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(MsgType& msg) = 0;
#endif

	//handling msg at here will not block msg receiving
	//if on_msg() return false, this function will not be invoked due to no msgs need to dispatch
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual void on_msg_handle(MsgType& msg) = 0;

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch (id)
		{
		case 0: //delay put msgs into recv buffer because of recv buffer overflow
			if (dispatch_msg())
				start(); //recv msg sequentially, that means second recv only after first recv success
			else
				return true;
			break;
		case 1: //suspend dispatch msgs
			{
				mutex::scoped_lock lock(recv_msg_buffer_mutex);
				do_dispatch_msg();
			}
			break;
		case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

	bool dispatch_msg()
	{
		if (temp_msg_buffer.empty())
			return true;
		else if (suspend_dispatch_msg_)
			return false;

		bool dispatch = false;
		mutex::scoped_lock lock(recv_msg_buffer_mutex);
		size_t msg_num = recv_msg_buffer.size();

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER //inefficient
		for (BOOST_AUTO(iter, temp_msg_buffer.begin()); iter != temp_msg_buffer.end();)
			if (!on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else if (msg_num < MAX_MSG_NUM) //msg recv buffer available
			{
				dispatch = true;
				recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer, iter++);
				++msg_num;
			}
			else
				++iter;
#else //efficient
		if (msg_num < MAX_MSG_NUM) //msg recv buffer available
		{
			dispatch = true;
			msg_num = MAX_MSG_NUM - msg_num; //max msg number this time can handle
			BOOST_AUTO(begin_iter, temp_msg_buffer.begin()); BOOST_AUTO(end_iter, temp_msg_buffer.end());
			if (temp_msg_buffer.size() > msg_num) //some msgs left behind
			{
				size_t left_num = temp_msg_buffer.size() - msg_num;
				if (left_num > msg_num) //find the minimum movement
					std::advance(end_iter = begin_iter, msg_num);
				else
					std::advance(end_iter, -(typename container::list<MsgType>::iterator::difference_type) left_num);
			}
			else
				msg_num = temp_msg_buffer.size();
			//use msg_num to avoid std::distance() call, so, msg_num must correct
			recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer, begin_iter, end_iter, msg_num);
		}
#endif

		if (dispatch)
			do_dispatch_msg();

		return temp_msg_buffer.empty();
	}

	void msg_handler()
	{
		on_msg_handle(last_dispatch_msg); //must before next msg dispatch to keep sequence
		mutex::scoped_lock lock(recv_msg_buffer_mutex);
		dispatching = false;
		//dispatch msg sequentially, that means second dispatch only after first dispatch success
		do_dispatch_msg();
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg()
	{
		if (suspend_dispatch_msg_)
		{
			if (!dispatching && !recv_msg_buffer.empty())
				set_timer(1, 50, NULL);
		}
		else
		{
			io_service& io_service_ = this->get_io_service();
			if (io_service_.stopped())
				dispatching = false;
			else if (!dispatching && !recv_msg_buffer.empty())
			{
				dispatching = true;
				last_dispatch_msg.swap(recv_msg_buffer.front());
				io_service_.post(boost::bind(&st_socket::msg_handler, this));
				recv_msg_buffer.pop_front();
			}
		}
	}

protected:
	MsgType last_send_msg, last_dispatch_msg;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<MsgType> send_msg_buffer;
	mutex send_msg_buffer_mutex;
	bool sending;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	//using this msg recv buffer or not is decided by the return value of on_msg()
	//see on_msg() for more details
	container::list<MsgType> recv_msg_buffer;
	mutex recv_msg_buffer_mutex;
	bool dispatching;

	//if on_msg() return true, which means use the msg recv buffer,
	//st_socket will invoke dispatch_msg() when got some msgs. if the msgs can't push into recv_msg_buffer
	//because of recv buffer overflow, st_socket will delay 50 milliseconds(nonblocking) to invoke
	//dispatch_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.
	container::list<MsgType> temp_msg_buffer;

	boost::shared_ptr<i_packer> packer_;
	bool suspend_dispatch_msg_;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */
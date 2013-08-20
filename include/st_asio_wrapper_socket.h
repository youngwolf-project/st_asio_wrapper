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
protected:
	st_socket(io_service& io_service_) : Socket(io_service_), st_timer(io_service_),
		packer_(boost::make_shared<packer>()) {reset_state();}

	void reset_state()
	{
		sending = suspend_send_msg_ = false;
		dispatching = suspend_dispatch_msg_ = false;
		started_ = false;
	}

	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();
	}

public:
	bool started() const {return started_;}
	void start()
	{
		mutex::scoped_lock lock(start_mutex);
		if (!started_)
			started_ = do_start();
	}
	bool send_msg() //return false if send buffer is empty or sending not allowed or io_service stopped
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return do_send_msg();
	}

	void suspend_send_msg(bool suspend) {if (!(suspend_send_msg_ = suspend)) send_msg();}
	bool suspend_send_msg() const {return suspend_send_msg_;}

	void suspend_dispatch_msg(bool suspend)
	{
		suspend_dispatch_msg_ = suspend;
		stop_timer(1);
		do_dispatch_msg(true);
	}
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

	//how many msgs waiting for sending(sending_msg = true) or dispatching
	size_t get_pending_msg_num(bool sending_msg = true)
	{
		if (sending_msg)
		{
			mutex::scoped_lock lock(send_msg_buffer_mutex);
			return send_msg_buffer.size();
		}
		else
		{
			mutex::scoped_lock lock(recv_msg_buffer_mutex);
			return recv_msg_buffer.size();
		}
	}

	void peek_first_pending_msg(MsgType& msg, bool sending_msg = true)
	{
		msg.clear();
		if (sending_msg) //the msg's format please refer to on_msg_send
		{
			mutex::scoped_lock lock(send_msg_buffer_mutex);
			if (!send_msg_buffer.empty())
				msg = send_msg_buffer.front();
		}
		else //msg is unpacked
		{
			mutex::scoped_lock lock(recv_msg_buffer_mutex);
			if (!recv_msg_buffer.empty())
				msg = recv_msg_buffer.front();
		}
	}

	void pop_first_pending_msg(MsgType& msg, bool sending_msg = true)
	{
		msg.clear();
		if (sending_msg) //the msg's format please refer to on_msg_send
		{
			mutex::scoped_lock lock(send_msg_buffer_mutex);
			if (!send_msg_buffer.empty())
			{
				msg.swap(send_msg_buffer.front());
				send_msg_buffer.pop_front();
			}
		}
		else
		{
			mutex::scoped_lock lock(recv_msg_buffer_mutex);
			if (!recv_msg_buffer.empty())
			{
				msg.swap(recv_msg_buffer.front());
				recv_msg_buffer.pop_front();
			}
		}
	}

	//clear all pending msgs
	void pop_all_pending_msg(container::list<MsgType>& msg_list, bool sending_msg = true)
	{
		if (sending_msg)
		{
			mutex::scoped_lock lock(send_msg_buffer_mutex);
			msg_list.splice(msg_list.end(), send_msg_buffer);
		}
		else
		{
			mutex::scoped_lock lock(recv_msg_buffer_mutex);
			msg_list.splice(msg_list.end(), recv_msg_buffer);
		}
	}

protected:
	virtual bool do_start() = 0;
	//must mutex send_msg_buffer before invoke this function
	virtual bool do_send_msg() = 0;

	virtual bool is_send_allowed() const {return !suspend_send_msg_;}
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
			dispatch_msg();
			break;
		case 1: //suspend dispatch msgs
			do_dispatch_msg(true);
			break;
		case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

	//can only be invoked after socket closed
	void direct_dispatch_all_msg() {suspend_dispatch_msg(false);}
	void dispatch_msg()
	{
		auto dispatch = false;
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
		for (auto iter = std::begin(temp_msg_buffer); !suspend_dispatch_msg_ && iter != std::end(temp_msg_buffer);)
			if (!on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else
			{
				mutex::scoped_lock lock(recv_msg_buffer_mutex);
				auto msg_num = recv_msg_buffer.size();
				if (msg_num < MAX_MSG_NUM) //msg recv buffer available
				{
					dispatch = true;
					recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer, iter++);
				}
				else
					++iter;
			}
#else
		if (!temp_msg_buffer.empty())
		{
			mutex::scoped_lock lock(recv_msg_buffer_mutex);
			auto msg_num = recv_msg_buffer.size();
			if (msg_num < MAX_MSG_NUM) //msg recv buffer available
			{
				dispatch = true;
				msg_num = MAX_MSG_NUM - msg_num; //max msg number this time can handle
				auto begin_iter = std::begin(temp_msg_buffer), end_iter = std::end(temp_msg_buffer);
				if (temp_msg_buffer.size() > msg_num) //some msgs left behind
				{
					auto left_num = temp_msg_buffer.size() - msg_num;
					//find the minimum movement
					end_iter = left_num > msg_num ? std::next(begin_iter, msg_num) : std::prev(end_iter, left_num);
				}
				else
					msg_num = temp_msg_buffer.size();
				//use msg_num to avoid std::distance() call, so, msg_num must correct
				recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer, begin_iter, end_iter, msg_num);
			}
		}
#endif

		if (dispatch)
			do_dispatch_msg(true);

		if (temp_msg_buffer.empty())
			do_start(); //recv msg sequentially, that means second recv only after first recv success
		else
			set_timer(0, 50, nullptr);
	}

	void msg_handler()
	{
		on_msg_handle(last_dispatch_msg); //must before next msg dispatch to keep sequence
		mutex::scoped_lock lock(recv_msg_buffer_mutex);
		dispatching = false;
		//dispatch msg sequentially, that means second dispatch only after first dispatch success
		do_dispatch_msg(false);
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg(bool need_lock)
	{
		mutex::scoped_lock lock;
		if (need_lock)
			lock = mutex::scoped_lock(recv_msg_buffer_mutex);

		if (suspend_dispatch_msg_)
		{
			if (!dispatching && !recv_msg_buffer.empty())
				set_timer(1, 24 * 60 * 60 * 1000, nullptr); //one day
		}
		else
		{
			auto& io_service_ = ST_THIS get_io_service();
			auto dispatch_all = false;
			if (io_service_.stopped())
				dispatch_all = !(dispatching = false);
			else if (!dispatching)
			{
				if (!ST_THIS is_open())
					dispatch_all = true;
				else if (!recv_msg_buffer.empty())
				{
					dispatching = true;
					last_dispatch_msg.swap(recv_msg_buffer.front());
					io_service_.post(boost::bind(&st_socket::msg_handler, this));
					recv_msg_buffer.pop_front();
				}
			}

			if (dispatch_all)
			{
				recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer);
				st_asio_wrapper::do_something_to_all(recv_msg_buffer,
					boost::bind(&st_socket::on_msg_handle, this, _1));
				recv_msg_buffer.clear();
			}
		}
	}

protected:
	MsgType last_send_msg, last_dispatch_msg;
	boost::shared_ptr<i_packer> packer_;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<MsgType> send_msg_buffer;
	mutex send_msg_buffer_mutex;
	bool sending, suspend_send_msg_;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	//using this msg recv buffer or not is decided by the return value of on_msg()
	//see on_msg() for more details
	container::list<MsgType> recv_msg_buffer;
	mutex recv_msg_buffer_mutex;
	bool dispatching, suspend_dispatch_msg_;

	//if on_msg() return true, which means use the msg recv buffer,
	//st_socket will invoke dispatch_msg() when got some msgs. if the msgs can't push into recv_msg_buffer
	//because of recv buffer overflow, st_socket will delay 50 milliseconds(nonblocking) to invoke
	//dispatch_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.
	container::list<MsgType> temp_msg_buffer;

	bool started_; //has started or not
	mutex start_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

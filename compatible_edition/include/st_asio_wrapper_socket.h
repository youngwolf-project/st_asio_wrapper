/*
 * st_asio_wrapper_socket.h
 *
 *  Created on: 2013-8-4
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both TCP and UDP socket
 */

#ifndef ST_ASIO_WRAPPER_SOCKET_H_
#define ST_ASIO_WRAPPER_SOCKET_H_

#include <boost/container/list.hpp>

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_timer.h"

#ifndef DEFAULT_PACKER
#define DEFAULT_PACKER packer
#endif

namespace st_asio_wrapper
{

enum BufferType {POST_BUFFER, SEND_BUFFER, RECV_BUFFER};

#define post_msg_buffer ST_THIS msg_buffer[0]
#define post_msg_buffer_mutex ST_THIS msg_buffer_mutex[0]
#define send_msg_buffer ST_THIS msg_buffer[1]
#define send_msg_buffer_mutex ST_THIS msg_buffer_mutex[1]
#define recv_msg_buffer ST_THIS msg_buffer[2]
#define recv_msg_buffer_mutex ST_THIS msg_buffer_mutex[2]
#define temp_msg_buffer ST_THIS msg_buffer[3]

template<typename MsgType, typename Socket, typename MsgDataType = MsgType>
class st_socket: public st_timer
{
public:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	typedef boost::container::list<MsgType> container_type;

protected:
	st_socket(boost::asio::io_service& io_service_) : st_timer(io_service_), next_layer_(io_service_),
		packer_(boost::make_shared<DEFAULT_PACKER>()) {reset_state();}

	template<typename Arg>
	st_socket(boost::asio::io_service& io_service_, Arg& arg) : st_timer(io_service_), next_layer_(io_service_, arg),
		packer_(boost::make_shared<DEFAULT_PACKER>()) {reset_state();}

	void reset_state()
	{
		posting = false;
		sending = suspend_send_msg_ = false;
		dispatching = suspend_dispatch_msg_ = false;
		started_ = false;
	}

	void clear_buffer()
	{
		post_msg_buffer.clear();
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();
	}

public:
	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

#ifdef REUSE_OBJECT
	virtual bool reusable()
	{
		if (started())
			return false;

		boost::mutex::scoped_lock lock(recv_msg_buffer_mutex, boost::try_to_lock);
		return lock.owns_lock(); //if recv_msg_buffer_mutex has been locked, then this socket should not be reused.
	}
#endif

	bool started() const {return started_;}
	void start()
	{
		boost::mutex::scoped_lock lock(start_mutex);
		if (!started_)
			started_ = do_start();
	}
	bool send_msg() //return false if send buffer is empty or sending not allowed or io_service stopped
	{
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
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
	boost::shared_ptr<i_packer<MsgDataType> > inner_packer() {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer<MsgDataType> >& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed
	//no matter whether the send buffer is available
	bool is_send_buffer_available()
	{
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		return send_msg_buffer.size() < MAX_MSG_NUM;
	}

	//don't use the packer but insert into the send_msg_buffer directly
	bool direct_send_msg(const MsgType& msg, bool can_overflow = false)
		{MsgType tmp_msg(msg); return direct_send_msg(tmp_msg, can_overflow);}
	//after this call, msg becomes empty, please note.
	bool direct_send_msg(MsgType& msg, bool can_overflow = false)
	{
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM)
			return do_direct_send_msg(msg);

		return false;
	}

	bool direct_post_msg(const MsgType& msg, bool can_overflow = false)
		{MsgType tmp_msg(msg); return direct_post_msg(tmp_msg, can_overflow);}
	//after this call, msg becomes empty, please note.
	bool direct_post_msg(MsgType& msg, bool can_overflow = false)
	{
		if (direct_send_msg(msg, can_overflow))
			return true;
		else
		{
			boost::mutex::scoped_lock lock(post_msg_buffer_mutex);
			return do_direct_post_msg(msg);
		}
	}

	//how many msgs waiting for sending(sending_msg = true) or dispatching
	size_t get_pending_msg_num(BufferType buffer_type = SEND_BUFFER)
	{
		boost::mutex::scoped_lock lock(msg_buffer_mutex[buffer_type]);
		return msg_buffer[buffer_type].size();
	}

	void peek_first_pending_msg(MsgType& msg, BufferType buffer_type = SEND_BUFFER)
	{
		msg.clear();
		//msgs in send buffer and post buffer are packed
		//msgs in receive buffer are unpacked
		boost::mutex::scoped_lock lock(msg_buffer_mutex[buffer_type]);
		if (!msg_buffer[buffer_type].empty())
			msg = msg_buffer[buffer_type].front();
	}

	void pop_first_pending_msg(MsgType& msg, BufferType buffer_type = SEND_BUFFER)
	{
		msg.clear();
		//msgs in send buffer and post buffer are packed
		//msgs in receive buffer are unpacked
		boost::mutex::scoped_lock lock(msg_buffer_mutex[buffer_type]);
		if (!msg_buffer[buffer_type].empty())
		{
			msg.swap(msg_buffer[buffer_type].front());
			msg_buffer[buffer_type].pop_front();
		}
	}

	//clear all pending msgs
	void pop_all_pending_msg(container_type& msg_list, BufferType buffer_type = SEND_BUFFER)
	{
		boost::mutex::scoped_lock lock(msg_buffer_mutex[buffer_type]);
		msg_list.splice(msg_list.end(), msg_buffer[buffer_type]);
	}

protected:
	virtual bool do_start() = 0;
	//must mutex send_msg_buffer before invoke this function
	virtual bool do_send_msg() = 0;

	virtual bool is_send_allowed() const {return !suspend_send_msg_;}
	//can send data or not(just put into send buffer)

	//generally, you need not re-write this for link broken judgment(TCP)
	virtual void on_send_error(const boost::system::error_code& ec)
		{unified_out::error_out("send msg error: %d %s", ec.value(), ec.message().data());}

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own receive buffer, you can move the msg to your own receive buffer,
	//then, handle the msg as your own strategy(may be you'll need a msg dispatch thread)
	//or, you can handle the msg at here, but this will reduce efficiency(because this msg handling block
	//the next msg receiving on the same st_socket) unless you can handle the msg very fast(which will
	//inversely more efficient, because msg receive buffer and msg dispatching are not needed any more).
	//
	//return true means msg been handled, st_socket will not maintain it anymore, return false means
	//msg cannot be handled right now, you must handle it in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(MsgType& msg) = 0;
#endif

	//handling msg in om_msg_handle() will not block msg receiving on the same st_socket
	//return true means msg been handled, false means msg cannot be handled right now, and st_socket will
	//re-dispatch it asynchronously
	//if link_down is true, no matter return true or false, st_socket will not maintain this msg anymore,
	//and continue dispatch the next msg continuously
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg_handle(MsgType& msg, bool link_down) = 0;

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg(remain in packed)
	//if the msg is custom packed, then obviously you know it
	//or the msg is packed as: length(2 bytes) + original msg, see st_asio_wrapper::packer for more details
	virtual void on_msg_send(MsgType& msg) {}
#endif
#ifdef WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty, msg remain in packed
	virtual void on_all_msg_send(MsgType& msg) {}
#endif

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch (id)
		{
		case 0: //delay put msgs into receive buffer cause of receive buffer overflow
			dispatch_msg();
			break;
		case 1: //suspend dispatch msgs
			do_dispatch_msg(true);
			break;
		case 2:
			{
				bool empty;
				boost::mutex::scoped_lock lock(post_msg_buffer_mutex);
				{
					boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
					if (splice_helper(send_msg_buffer, post_msg_buffer))
						do_send_msg();
				}
				posting = !(empty = post_msg_buffer.empty());
				lock.unlock();

				if (empty)
					do_dispatch_msg(true);

				return !empty; //continue the timer if not empty
			}
			break;
		case 3: //re-dispatch
			do_dispatch_msg(true);
			break;
		case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

	void dispatch_msg()
	{
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
		bool dispatch = false;
		for (BOOST_AUTO(iter, temp_msg_buffer.begin());
			!suspend_dispatch_msg_ && !posting && iter != temp_msg_buffer.end();)
			if (on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else
			{
				boost::mutex::scoped_lock lock(recv_msg_buffer_mutex);
				size_t msg_num = recv_msg_buffer.size();
				if (msg_num < MAX_MSG_NUM) //msg receive buffer available
				{
					dispatch = true;
					recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer, iter++);
				}
				else
					++iter;
			}

		if (dispatch)
			do_dispatch_msg(true);
#else
		if (!temp_msg_buffer.empty())
		{
			boost::mutex::scoped_lock lock(recv_msg_buffer_mutex);
			if (splice_helper(recv_msg_buffer, temp_msg_buffer))
				do_dispatch_msg(false);
		}
#endif

		if (temp_msg_buffer.empty())
			do_start(); //receive msg sequentially, which means second receiving only after first receiving success
		else
			set_timer(0, 50, NULL);
	}

	void msg_handler()
	{
		bool re = on_msg_handle(last_dispatch_msg, false); //must before next msg dispatch to keep sequence
		boost::mutex::scoped_lock lock(recv_msg_buffer_mutex);
		dispatching = false;
		if (!re) //dispatch failed, re-dispatch
		{
			recv_msg_buffer.push_front(MsgType());
			recv_msg_buffer.front().swap(last_dispatch_msg);
			set_timer(3, 50, NULL);
		}
		else //dispatch msg sequentially, which means second dispatch only after first dispatch success
			do_dispatch_msg(false);
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg(bool need_lock)
	{
		boost::mutex::scoped_lock lock(recv_msg_buffer_mutex, boost::defer_lock);
		if (need_lock) lock.lock();

		if (suspend_dispatch_msg_)
		{
			if (!dispatching && !recv_msg_buffer.empty())
				set_timer(1, 24 * 60 * 60 * 1000, NULL); //one day
		}
		else if (!posting)
		{
			BOOST_AUTO(&io_service_, get_io_service());
			bool dispatch_all = false;
			if (io_service_.stopped())
				dispatch_all = !(dispatching = false);
			else if (!dispatching)
			{
				if (!lowest_layer().is_open())
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
#ifdef FORCE_TO_USE_MSG_RECV_BUFFER
				//the msgs in temp_msg_buffer are discarded if we don't used msg receive buffer, it's very hard to resolve this defect,
				//so, please be very carefully if you decide to resolve this issue;
				//the biggest problem is calling force_close in on_msg.
				recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer);
#endif
#ifndef DISCARD_MSG_WHEN_LINK_DOWN
				st_asio_wrapper::do_something_to_all(recv_msg_buffer, boost::bind(&st_socket::on_msg_handle, this, _1, true));
#endif
				recv_msg_buffer.clear();
			}
		}
	}

	//must mutex send_msg_buffer before invoke this function
	bool do_direct_send_msg(MsgType& msg)
	{
		if (!msg.empty())
		{
			send_msg_buffer.resize(send_msg_buffer.size() + 1);
			send_msg_buffer.back().swap(msg);
			do_send_msg();
		}

		return true;
	}

	//must mutex post_msg_buffer before invoke this function
	bool do_direct_post_msg(MsgType& msg)
	{
		if (!msg.empty())
		{
			post_msg_buffer.resize(post_msg_buffer.size() + 1);
			post_msg_buffer.back().swap(msg);
			if (!posting)
			{
				posting = true;
				set_timer(2, 50, NULL);
			}
		}

		return true;
	}

protected:
	Socket next_layer_;

	MsgType last_send_msg, last_dispatch_msg;
	boost::shared_ptr<i_packer<MsgDataType> > packer_;

	container_type msg_buffer[4];
	//if on_msg() return true, which means use the msg receive buffer,
	//st_socket will invoke dispatch_msg() when got some msgs. if these msgs can't push into recv_msg_buffer
	//cause of receive buffer overflow, st_socket will delay 50 milliseconds(non-blocking) to invoke
	//dispatch_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.
	boost::mutex msg_buffer_mutex[3];

	bool posting;
	bool sending, suspend_send_msg_;
	bool dispatching, suspend_dispatch_msg_;

	bool started_; //has started or not
	boost::mutex start_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

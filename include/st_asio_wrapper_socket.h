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

template<typename MsgType, typename Socket>
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
	boost::shared_ptr<i_packer> inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer>& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed
	//no matter whether the send buffer is available
	bool is_send_buffer_available()
	{
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		return send_msg_buffer.size() < MAX_MSG_NUM;
	}

	//don't use the packer but insert into the send_msg_buffer directly
	bool direct_send_msg(const MsgType& msg, bool can_overflow = false)
		{return direct_send_msg(MsgType(msg), can_overflow);}
	bool direct_send_msg(MsgType&& msg, bool can_overflow = false)
	{
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM)
			return do_direct_send_msg(std::move(msg));

		return false;
	}

	bool direct_post_msg(const MsgType& msg, bool can_overflow = false)
		{return direct_post_msg(MsgType(msg), can_overflow);}
	bool direct_post_msg(MsgType&& msg, bool can_overflow = false)
	{
		if (direct_send_msg(std::move(msg), can_overflow))
			return true;
		else
		{
			boost::mutex::scoped_lock lock(post_msg_buffer_mutex);
			return do_direct_post_msg(std::move(msg));
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
		//msgs in recv buffer are unpacked
		boost::mutex::scoped_lock lock(msg_buffer_mutex[buffer_type]);
		if (!msg_buffer[buffer_type].empty())
			msg = msg_buffer[buffer_type].front();
	}

	void pop_first_pending_msg(MsgType& msg, BufferType buffer_type = SEND_BUFFER)
	{
		msg.clear();
		//msgs in send buffer and post buffer are packed
		//msgs in recv buffer are unpacked
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

	//generally, you need not re-write this for link broken judgment(tcp)
	virtual void on_send_error(const boost::system::error_code& ec)
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

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg(remain in packed)
	//if the msg is custom packed, then obviously you know it
	//or the msg is packed as: len(2 bytes) + original msg, see st_asio_wrapper::packer for more details
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
		case 0: //delay put msgs into recv buffer because of recv buffer overflow
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
		case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
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
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
		auto dispatch = false;
		for (auto iter = std::begin(temp_msg_buffer);
			!suspend_dispatch_msg_ && !posting && iter != std::end(temp_msg_buffer);)
			if (!on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else
			{
				boost::mutex::scoped_lock lock(recv_msg_buffer_mutex);
				auto msg_num = recv_msg_buffer.size();
				if (msg_num < MAX_MSG_NUM) //msg recv buffer available
				{
					dispatch = true;
					recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer, iter++);
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
			do_start(); //recv msg sequentially, that means second recv only after first recv success
		else
			set_timer(0, 50, nullptr);
	}

	void msg_handler()
	{
		on_msg_handle(last_dispatch_msg); //must before next msg dispatch to keep sequence
		boost::mutex::scoped_lock lock(recv_msg_buffer_mutex);
		dispatching = false;
		//dispatch msg sequentially, that means second dispatch only after first dispatch success
		do_dispatch_msg(false);
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg(bool need_lock)
	{
		boost::mutex::scoped_lock lock;
		if (need_lock)
			lock = boost::mutex::scoped_lock(recv_msg_buffer_mutex);

		if (suspend_dispatch_msg_)
		{
			if (!dispatching && !recv_msg_buffer.empty())
				set_timer(1, 24 * 60 * 60 * 1000, nullptr); //one day
		}
		else if (!posting)
		{
			auto& io_service_ = get_io_service();
			auto dispatch_all = false;
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
				recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer);
#endif
				//the msgs in temp_msg_buffer are discarded, it's very hard to resolve this defect,
				//so, please be very carefully if you decide to resolve this issue;
				//the biggest problem is calling force_close in on_msg.
				st_asio_wrapper::do_something_to_all(recv_msg_buffer,
					boost::bind(&st_socket::on_msg_handle, this, _1));
				recv_msg_buffer.clear();
			}
		}
	}

	//must mutex send_msg_buffer before invoke this function
	bool do_direct_send_msg(MsgType&& msg)
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
	bool do_direct_post_msg(MsgType&& msg)
	{
		if (!msg.empty())
		{
			post_msg_buffer.resize(post_msg_buffer.size() + 1);
			post_msg_buffer.back().swap(msg);
			if (!posting)
			{
				posting = true;
				set_timer(2, 50, nullptr);
			}
		}

		return true;
	}

protected:
	Socket next_layer_;

	MsgType last_send_msg, last_dispatch_msg;
	boost::shared_ptr<i_packer> packer_;

	container_type msg_buffer[4];
	//if on_msg() return true, which means use the msg recv buffer,
	//st_socket will invoke dispatch_msg() when got some msgs. if these msgs can't push into recv_msg_buffer
	//because of recv buffer overflow, st_socket will delay 50 milliseconds(nonblocking) to invoke
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

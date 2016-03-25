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

template<typename Socket, typename Packer, typename Unpacker, typename InMsgType = typename Packer::msg_type, typename OutMsgType = typename Unpacker::msg_type>
class st_socket: public st_timer
{
public:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	typedef boost::container::list<InMsgType> in_container_type;
	typedef typename Unpacker::container_type out_container_type;

protected:
	st_socket(boost::asio::io_service& io_service_) : st_timer(io_service_), _id(-1), next_layer_(io_service_), packer_(boost::make_shared<Packer>()) {init();}

	template<typename Arg>
	st_socket(boost::asio::io_service& io_service_, Arg& arg) : st_timer(io_service_), _id(-1), next_layer_(io_service_, arg), packer_(boost::make_shared<Packer>()) {init();}

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
	//please do not change id at runtime via the following function, except this st_socket is not managed by st_object_pool,
	//it should only be used by st_object_pool when this st_socket being reused or creating new st_socket.
	void id(uint_fast64_t id) {_id = id;}
	uint_fast64_t id() const {return _id;}

	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

	virtual bool obsoleted()
	{
		if (started())
			return false;

#ifdef ENHANCED_STABILITY
		if (!async_call_indicator.unique())
			return false;
#endif

		boost::unique_lock<boost::shared_mutex> lock(recv_msg_buffer_mutex, boost::try_to_lock);
		return lock.owns_lock() && recv_msg_buffer.empty(); //if successfully locked, means this st_socket is idle
	}

	bool started() const {return started_;}
	void start()
	{
		boost::unique_lock<boost::shared_mutex> lock(start_mutex);
		if (!started_)
			started_ = do_start();
	}
	bool send_msg() //return false if send buffer is empty or sending not allowed or io_service stopped
	{
		boost::unique_lock<boost::shared_mutex> lock(send_msg_buffer_mutex);
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
	//changing packer at runtime is not thread-safe, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_packer<typename Packer::msg_type>> inner_packer() {return packer_;}
	boost::shared_ptr<const i_packer<typename Packer::msg_type>> inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer<typename Packer::msg_type>>& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed no matter whether the send buffer is available or not,
	//this can exhaust all virtual memory, please pay special attentions.
	bool is_send_buffer_available()
	{
		boost::shared_lock<boost::shared_mutex> lock(send_msg_buffer_mutex);
		return send_msg_buffer.size() < MAX_MSG_NUM;
	}

	//don't use the packer but insert into send buffer directly
	bool direct_send_msg(const InMsgType& msg, bool can_overflow = false) {return direct_send_msg(InMsgType(msg), can_overflow);}
	bool direct_send_msg(InMsgType&& msg, bool can_overflow = false)
	{
		boost::unique_lock<boost::shared_mutex> lock(send_msg_buffer_mutex);
		return can_overflow || send_msg_buffer.size() < MAX_MSG_NUM ? do_direct_send_msg(std::move(msg)) : false;
	}

	bool direct_post_msg(const InMsgType& msg, bool can_overflow = false) {return direct_post_msg(InMsgType(msg), can_overflow);}
	bool direct_post_msg(InMsgType&& msg, bool can_overflow = false)
	{
		if (direct_send_msg(std::move(msg), can_overflow))
			return true;

		boost::unique_lock<boost::shared_mutex> lock(post_msg_buffer_mutex);
		return do_direct_post_msg(std::move(msg));
	}

	//how many msgs waiting for sending or dispatching
	GET_PENDING_MSG_NUM(get_pending_post_msg_num, post_msg_buffer, post_msg_buffer_mutex)
	GET_PENDING_MSG_NUM(get_pending_send_msg_num, send_msg_buffer, send_msg_buffer_mutex)
	GET_PENDING_MSG_NUM(get_pending_recv_msg_num, recv_msg_buffer, recv_msg_buffer_mutex)

	PEEK_FIRST_PENDING_MSG(peek_first_pending_post_msg, post_msg_buffer, post_msg_buffer_mutex, InMsgType)
	PEEK_FIRST_PENDING_MSG(peek_first_pending_send_msg, send_msg_buffer, send_msg_buffer_mutex, InMsgType)
	PEEK_FIRST_PENDING_MSG(peek_first_pending_recv_msg, recv_msg_buffer, recv_msg_buffer_mutex, OutMsgType)

	POP_FIRST_PENDING_MSG(pop_first_pending_post_msg, post_msg_buffer, post_msg_buffer_mutex, InMsgType)
	POP_FIRST_PENDING_MSG(pop_first_pending_send_msg, send_msg_buffer, send_msg_buffer_mutex, InMsgType)
	POP_FIRST_PENDING_MSG(pop_first_pending_recv_msg, recv_msg_buffer, recv_msg_buffer_mutex, OutMsgType)

	//clear all pending msgs
	POP_ALL_PENDING_MSG(pop_all_pending_post_msg, post_msg_buffer, post_msg_buffer_mutex, in_container_type)
	POP_ALL_PENDING_MSG(pop_all_pending_send_msg, send_msg_buffer, send_msg_buffer_mutex, in_container_type)
	POP_ALL_PENDING_MSG(pop_all_pending_recv_msg, recv_msg_buffer, recv_msg_buffer_mutex, out_container_type)

protected:
	virtual bool do_start() = 0;
	virtual bool do_send_msg() = 0; //must mutex send_msg_buffer before invoke this function

	virtual bool is_send_allowed() const {return !suspend_send_msg_;} //can send msg or not(just put into send buffer)

	//generally, you don't have to rewrite this to maintain the status of connections(TCP)
	virtual void on_send_error(const boost::system::error_code& ec) {unified_out::error_out("send msg error (%d %s)", ec.value(), ec.message().data());}
	//receiving error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const boost::system::error_code& ec) = 0;

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own receive buffer, you can move the msg to your own receive buffer, then handle them as your own strategy(may be you'll need a msg dispatch thread),
	//or you can handle the msg at here, but this will reduce efficiency because this msg handling will block the next msg receiving on the same st_socket,
	//but if you can handle the msg very fast, you are recommended to handle them at here, which will inversely more efficient,
	//because msg receive buffer and msg dispatching are not needed any more.
	//
	//return true means msg been handled, st_socket will not maintain it anymore, return false means msg cannot be handled right now, you must handle it in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(OutMsgType& msg) = 0;
#endif

	//handling msg in om_msg_handle() will not block msg receiving on the same st_socket
	//return true means msg been handled, false means msg cannot be handled right now, and st_socket will re-dispatch it asynchronously
	//if link_down is true, no matter return true or false, st_socket will not maintain this msg anymore, and continue dispatch the next msg continuously
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg_handle(OutMsgType& msg, bool link_down) = 0;

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_msg_send(InMsgType& msg) {}
#endif
#ifdef WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_all_msg_send(InMsgType& msg) {}
#endif

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch (id)
		{
		case 0: //delay putting msgs into receive buffer cause of receive buffer overflow
			dispatch_msg();
			break;
		case 1: //suspend dispatching msgs
			do_dispatch_msg(true);
			break;
		case 2:
			{
				boost::unique_lock<boost::shared_mutex> lock(post_msg_buffer_mutex);
				{
					boost::unique_lock<boost::shared_mutex> lock(send_msg_buffer_mutex);
					if (splice_helper(send_msg_buffer, post_msg_buffer))
						do_send_msg();
				}

				auto empty = post_msg_buffer.empty();
				posting = !empty;
				lock.unlock();

				if (empty)
					do_dispatch_msg(true);

				return !empty; //continue the timer if some msgs still left behind
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
		auto dispatch = false;
		for (auto iter = std::begin(temp_msg_buffer); !suspend_dispatch_msg_ && !posting && iter != std::end(temp_msg_buffer);)
			if (on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else
			{
				boost::unique_lock<boost::shared_mutex> lock(recv_msg_buffer_mutex);
				if (recv_msg_buffer.size() < MAX_MSG_NUM) //msg receive buffer available
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
			boost::unique_lock<boost::shared_mutex> lock(recv_msg_buffer_mutex);
			if (splice_helper(recv_msg_buffer, temp_msg_buffer))
				do_dispatch_msg(false);
		}
#endif

		if (temp_msg_buffer.empty())
			do_start(); //receive msg sequentially, which means second receiving only after first receiving success
		else
			set_timer(0, 50, nullptr);
	}

	void msg_handler()
	{
		bool re = on_msg_handle(last_dispatch_msg, false); //must before next msg dispatching to keep sequence
		boost::unique_lock<boost::shared_mutex> lock(recv_msg_buffer_mutex);
		dispatching = false;
		if (!re) //dispatch failed, re-dispatch
		{
			recv_msg_buffer.push_front(OutMsgType());
			recv_msg_buffer.front().swap(last_dispatch_msg);
			set_timer(3, 50, nullptr);
		}
		else //dispatch msg sequentially, which means second dispatching only after first dispatching success
			do_dispatch_msg(false);

		if (!dispatching)
			last_dispatch_msg.clear();
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg(bool need_lock)
	{
		boost::unique_lock<boost::shared_mutex> lock(recv_msg_buffer_mutex, boost::defer_lock);
		if (need_lock) lock.lock();

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
					io_service_.post([this]() {ST_THIS msg_handler();});
					recv_msg_buffer.pop_front();
				}
			}

			if (dispatch_all)
			{
#ifdef FORCE_TO_USE_MSG_RECV_BUFFER
				//the msgs in temp_msg_buffer are discarded if we don't used msg receive buffer, it's very hard to resolve this defect,
				//so, please be very carefully if you decide to resolve this issue, the biggest problem is calling force_close in on_msg.
				recv_msg_buffer.splice(std::end(recv_msg_buffer), temp_msg_buffer);
#endif
#ifndef DISCARD_MSG_WHEN_LINK_DOWN
				st_asio_wrapper::do_something_to_all(recv_msg_buffer, [this](OutMsgType& msg) {ST_THIS on_msg_handle(msg, true);});
#endif
				recv_msg_buffer.clear();
			}
		}
	}

	//must mutex send_msg_buffer before invoke this function
	bool do_direct_send_msg(InMsgType&& msg)
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
	bool do_direct_post_msg(InMsgType&& msg)
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

	void init()
	{
		reset_state();
#ifdef ENHANCED_STABILITY
		async_call_indicator = boost::make_shared<char>('\0');
#endif
	}

protected:
	uint_fast64_t _id;
	Socket next_layer_;

	InMsgType last_send_msg;
	OutMsgType last_dispatch_msg;
	boost::shared_ptr<i_packer<typename Packer::msg_type>> packer_;

	in_container_type post_msg_buffer, send_msg_buffer;
	out_container_type recv_msg_buffer, temp_msg_buffer;
	//st_socket will invoke dispatch_msg() when got some msgs. if these msgs can't push into recv_msg_buffer cause of receive buffer overflow,
	//st_socket will delay 50 milliseconds(non-blocking) to invoke dispatch_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.
	boost::shared_mutex post_msg_buffer_mutex, send_msg_buffer_mutex;
	boost::shared_mutex recv_msg_buffer_mutex;

	bool posting;
	bool sending, suspend_send_msg_;
	bool dispatching, suspend_dispatch_msg_;

	bool started_; //has started or not
	boost::shared_mutex start_mutex;

#ifdef ENHANCED_STABILITY
	boost::shared_ptr<char> async_call_indicator;
#endif
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

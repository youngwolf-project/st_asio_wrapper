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

#include "st_asio_wrapper_base.h"
#include "st_asio_wrapper_timer.h"

//after this duration, this st_socket can be freed from the heap or reused,
//you must define this macro as a value, not just define it, the value means the duration, unit is second.
//if macro ST_ASIO_ENHANCED_STABILITY been defined, this macro will always be zero.
#ifdef ST_ASIO_ENHANCED_STABILITY
	#if defined(ST_ASIO_DELAY_CLOSE) && ST_ASIO_DELAY_CLOSE != 0
		#warning ST_ASIO_DELAY_CLOSE will always be zero if ST_ASIO_ENHANCED_STABILITY macro been defined.
	#endif
	#undef ST_ASIO_DELAY_CLOSE
	#define ST_ASIO_DELAY_CLOSE 0
#else
	#ifndef ST_ASIO_DELAY_CLOSE
	#define ST_ASIO_DELAY_CLOSE	5 //seconds
	#elif ST_ASIO_DELAY_CLOSE <= 0
		#error "ST_ASIO_DELAY_CLOSE must be bigger than zero."
	#endif
#endif

namespace st_asio_wrapper
{

template<typename Socket, typename Packer, typename Unpacker, typename InMsgType, typename OutMsgType,
	template<typename, typename> class InQueue, template<typename> class InContainer,
	template<typename, typename> class OutQueue, template<typename> class OutContainer>
class st_socket: public st_timer
{
protected:
	typedef obj_with_begin_time<InMsgType> in_msg;
	typedef obj_with_begin_time<OutMsgType> out_msg;
	typedef InQueue<in_msg, InContainer<in_msg> > in_container_type;
	typedef OutQueue<out_msg, OutContainer<out_msg> > out_container_type;

	static const tid TIMER_BEGIN = st_timer::TIMER_END;
	static const tid TIMER_HANDLE_MSG = TIMER_BEGIN;
	static const tid TIMER_DISPATCH_MSG = TIMER_BEGIN + 1;
	static const tid TIMER_DELAY_CLOSE = TIMER_BEGIN + 2;
	static const tid TIMER_END = TIMER_BEGIN + 10;

	st_socket(boost::asio::io_service& io_service_) : st_timer(io_service_), _id(-1), next_layer_(io_service_), packer_(boost::make_shared<Packer>()), started_(false) {reset_state();}
	template<typename Arg>
	st_socket(boost::asio::io_service& io_service_, Arg& arg) : st_timer(io_service_), _id(-1), next_layer_(io_service_, arg), packer_(boost::make_shared<Packer>()), started_(false) {reset_state();}

	void reset()
	{
		reset_state();
		clear_buffer();
		stat.reset();

		st_timer::reset();
	}

	void reset_state()
	{
		packer_->reset_state();

		sending = paused_sending = false;
		dispatching = paused_dispatching = congestion_controlling = false;
#ifndef ST_ASIO_ENHANCED_STABILITY
		closing = false;
#endif
//		started_ = false;
	}

	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();

		last_dispatch_msg.clear();
	}

public:
	//please do not change id at runtime via the following function, except this st_socket is not managed by st_object_pool,
	//it should only be used by st_object_pool when reusing or creating new st_socket.
	void id(boost::uint_fast64_t id) {assert(!started_); if (started_) unified_out::error_out("id is unchangeable!"); else _id = id;}
	boost::uint_fast64_t id() const {return _id;}
	bool is_equal_to(boost::uint_fast64_t id) const {return _id == id;}

	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

	virtual bool obsoleted()
	{
#ifndef ST_ASIO_ENHANCED_STABILITY
		return started() || closing || ST_THIS is_async_calling() ? false : recv_msg_buffer.empty() && recv_msg_buffer.idle();
#else
		return !started() && !ST_THIS is_async_calling();
#endif
	}

	bool started() const {return started_;}
	void start()
	{
		boost::unique_lock<boost::shared_mutex> lock(start_mutex);
		if (!started_)
			started_ = do_start();
	}

	//return false if send buffer is empty or sending not allowed or io_service stopped
	bool send_msg()
	{
		if (!sending)
		{
			boost::unique_lock<boost::shared_mutex> lock(send_mutex);
			if (!sending)
			{
				sending = true;
				lock.unlock();

				if (!do_send_msg())
					sending = false;
			}
		}

		return sending;
	}

	void suspend_send_msg(bool suspend) {if (!(paused_sending = suspend)) send_msg();}
	bool suspend_send_msg() const {return paused_sending;}

	void suspend_dispatch_msg(bool suspend) {if (!(paused_dispatching = suspend)) dispatch_msg();}
	bool suspend_dispatch_msg() const {return paused_dispatching;}

	void congestion_control(bool enable) {congestion_controlling = enable; unified_out::warning_out("%s congestion control.", enable ? "open" : "close");}
	bool congestion_control() const {return congestion_controlling;}

	const struct statistic& get_statistic() const {return stat;}

	//get or change the packer at runtime
	//changing packer at runtime is not thread-safe, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_packer<typename Packer::msg_type> > inner_packer() {return packer_;}
	boost::shared_ptr<const i_packer<typename Packer::msg_type> > inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer<typename Packer::msg_type> >& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed no matter the sending buffer is available or not,
	//this can exhaust all virtual memory, please pay special attentions.
	bool is_send_buffer_available() const {return send_msg_buffer.size() < ST_ASIO_MAX_MSG_NUM;}

	//don't use the packer but insert into send buffer directly
	bool direct_send_msg(const InMsgType& msg, bool can_overflow = false) {InMsgType unused(msg); return direct_send_msg(unused, can_overflow);}
	//after this call, msg becomes empty, please note.
	bool direct_send_msg(InMsgType& msg, bool can_overflow = false) {return can_overflow || is_send_buffer_available() ? do_direct_send_msg(msg) : false;}

	//how many msgs waiting for sending or dispatching
	GET_PENDING_MSG_NUM(get_pending_send_msg_num, send_msg_buffer)
	GET_PENDING_MSG_NUM(get_pending_recv_msg_num, recv_msg_buffer)

	POP_FIRST_PENDING_MSG(pop_first_pending_send_msg, send_msg_buffer, InMsgType)
	POP_FIRST_PENDING_MSG(pop_first_pending_recv_msg, recv_msg_buffer, OutMsgType)

	//clear all pending msgs
	POP_ALL_PENDING_MSG(pop_all_pending_send_msg, send_msg_buffer, in_container_type)
	POP_ALL_PENDING_MSG(pop_all_pending_recv_msg, recv_msg_buffer, out_container_type)

protected:
	virtual bool do_start() = 0;
	virtual bool do_send_msg() = 0; //st_socket will guarantee not call this function in more than one thread concurrently.
	virtual void do_recv_msg() = 0;

	virtual bool is_closable() {return true;}
	virtual bool is_send_allowed() {return !paused_sending;} //can send msg or not(just put into send buffer)

	//generally, you don't have to rewrite this to maintain the status of connections(TCP)
	virtual void on_send_error(const boost::system::error_code& ec) {unified_out::error_out("send msg error (%d %s)", ec.value(), ec.message().data());}
	//receiving error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const boost::system::error_code& ec) = 0;
	//if ST_ASIO_ENHANCED_STABILITY macro been defined, in this callback, st_socket guarantee that there's no any async call associated it,
	//include user timers(created by set_timer()) and user async calls(started via post()),
	//this means you can clean up any resource in this st_socket except this st_socket itself, because this st_socket maybe is being maintained by st_object_pool.
	//if ST_ASIO_ENHANCED_STABILITY macro not defined, st_socket simply call this callback ST_ASIO_DELAY_CLOSE seconds later after link down, no any guarantees.
	virtual void on_close() {unified_out::info_out("on_close()");}

#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
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

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_msg_send(InMsgType& msg) {}
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty
	//notice: the msg is packed, using inconstant is for the convenience of swapping
	virtual void on_all_msg_send(InMsgType& msg) {}
#endif

	//subclass notify st_socket the shutdown event.
	void close()
	{
		if (is_closable())
		{
#ifndef ST_ASIO_ENHANCED_STABILITY
			closing = true;
#endif
			set_timer(TIMER_DELAY_CLOSE, ST_ASIO_DELAY_CLOSE * 1000 + 50, boost::bind(&st_socket::timer_handler, this, _1));
		}
	}

	//call this in subclasses' recv_handler only
	//subclasses must guarantee not call this function in more than one thread concurrently.
	void handle_msg()
	{
		BOOST_TYPEOF(temp_msg_buffer) temp_buffer;
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
		if (!temp_msg_buffer.empty() && !paused_dispatching && !congestion_controlling)
		{
			BOOST_AUTO(begin_time, statistic::local_time());
			for (BOOST_AUTO(iter, temp_msg_buffer.begin()); !paused_dispatching && !congestion_controlling && iter != temp_msg_buffer.end();)
				if (on_msg(*iter))
					temp_msg_buffer.erase(iter++);
				else
					temp_buffer.splice(temp_buffer.end(), temp_msg_buffer, iter++);

			stat.handle_time_1_sum += statistic::local_time() - begin_time;
		}
#else
		temp_buffer.swap(temp_msg_buffer);
#endif

		if (move_items_in(recv_msg_buffer, temp_buffer, -1) > 0)
			dispatch_msg();

		if (temp_msg_buffer.empty() && recv_msg_buffer.size() < ST_ASIO_MAX_MSG_NUM)
			do_recv_msg(); //receive msg sequentially, which means second receiving only after first receiving success
		else
		{
			recv_idle_begin_time = statistic::local_time();
			set_timer(TIMER_HANDLE_MSG, 50, boost::bind(&st_socket::timer_handler, this, _1));
		}
	}

	//return false if receiving buffer is empty or dispatching not allowed or io_service stopped
	bool dispatch_msg()
	{
		if (!dispatching)
		{
			boost::unique_lock<boost::shared_mutex> lock(dispatch_mutex);
			if (!dispatching)
			{
				dispatching = true;
				lock.unlock();

				if (!do_dispatch_msg())
					dispatching = false;
			}
		}

		return dispatching;
	}

	//return false if receiving buffer is empty or dispatching not allowed or io_service stopped
	bool do_dispatch_msg()
	{
		if (paused_dispatching)
			;
		else if (stopped())
		{
#ifndef ST_ASIO_DISCARD_MSG_WHEN_LINK_DOWN
			if (!last_dispatch_msg.empty())
			{
				on_msg_handle(last_dispatch_msg, true);
				last_dispatch_msg.clear();
			}

			out_msg msg;
			typename out_container_type::lock_guard lock(recv_msg_buffer);
			while (recv_msg_buffer.try_dequeue_(msg))
				on_msg_handle(msg, true);
#endif
		}
		else if (!last_dispatch_msg.empty() || recv_msg_buffer.try_dequeue(last_dispatch_msg))
		{
			post(boost::bind(&st_socket::msg_handler, this));
			return true;
		}

		return false;
	}

	bool do_direct_send_msg(InMsgType& msg)
	{
		if (!msg.empty())
		{
			in_msg unused;
			unused.swap(msg);
			send_msg_buffer.enqueue(unused);
			send_msg();
		}

		return true;
	}

private:
	bool timer_handler(tid id)
	{
		switch (id)
		{
		case TIMER_HANDLE_MSG:
			stat.recv_idle_sum += statistic::local_time() - recv_idle_begin_time;
			handle_msg();
			break;
		case TIMER_DISPATCH_MSG:
			dispatch_msg();
			break;
		case TIMER_DELAY_CLOSE:
			if (!ST_THIS is_last_async_call())
				return true;
			else if (lowest_layer().is_open())
			{
				boost::system::error_code ec;
				lowest_layer().close(ec);
			}
			on_close();
#ifndef ST_ASIO_ENHANCED_STABILITY
			closing = false;
#endif
			break;
		default:
			assert(false);
			break;
		}

		return false;
	}

	void msg_handler()
	{
		BOOST_AUTO(begin_time, statistic::local_time());
		stat.dispatch_dealy_sum += begin_time - last_dispatch_msg.begin_time;
		bool re = on_msg_handle(last_dispatch_msg, false); //must before next msg dispatching to keep sequence
		BOOST_AUTO(end_time, statistic::local_time());
		stat.handle_time_2_sum += end_time - begin_time;

		if (!re) //dispatch failed, re-dispatch
		{
			last_dispatch_msg.restart(end_time);
			dispatching = false;
			set_timer(TIMER_DISPATCH_MSG, 50, boost::bind(&st_socket::timer_handler, this, _1));
		}
		else //dispatch msg sequentially, which means second dispatching only after first dispatching success
		{
			last_dispatch_msg.clear();
			if (!do_dispatch_msg())
			{
				dispatching = false;
				if (!recv_msg_buffer.empty())
					dispatch_msg(); //just make sure no pending msgs
			}
		}
	}

protected:
	boost::uint_fast64_t _id;
	Socket next_layer_;

	out_msg last_dispatch_msg;
	boost::shared_ptr<i_packer<typename Packer::msg_type> > packer_;

	in_container_type send_msg_buffer;
	out_container_type recv_msg_buffer;
	boost::container::list<out_msg> temp_msg_buffer;
	//st_socket will invoke handle_msg() when got some msgs. if these msgs can't be pushed into recv_msg_buffer because of:
	// 1. msg dispatching suspended;
	// 2. congestion control opened;
	//st_socket will delay 50 milliseconds(non-blocking) to invoke handle_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.

	bool sending, paused_sending;
	boost::shared_mutex send_mutex;
	bool dispatching, paused_dispatching, congestion_controlling;
	boost::shared_mutex dispatch_mutex;
#ifndef ST_ASIO_ENHANCED_STABILITY
	bool closing;
#endif

	bool started_; //has started or not
	boost::shared_mutex start_mutex;

	struct statistic stat;
	typename statistic::stat_time recv_idle_begin_time;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

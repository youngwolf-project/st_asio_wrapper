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

#include "st_asio_wrapper_timer.h"

//after this duration, this st_socket can be freed from the heap or reused,
//you must define this macro as a value, not just define it, the value means the duration, unit is second.
//a value equal to zero will cause st_socket to use a mechanism to guarantee 100% safety when reusing or freeing it,
//st_object will hook all async calls to avoid this st_socket to be reused or freed before all async calls finish
//or been interrupted (of course, this mechanism will slightly impact efficiency).
#ifndef ST_ASIO_DELAY_CLOSE
#define ST_ASIO_DELAY_CLOSE	0 //seconds, guarantee 100% safety when reusing or freeing this st_socket
#endif
static_assert(ST_ASIO_DELAY_CLOSE >= 0, "delay close duration must be bigger than or equal to zero.");

namespace st_asio_wrapper
{

template<typename Socket, typename Packer, typename Unpacker, typename InMsgType, typename OutMsgType,
	template<typename, typename> class InQueue, template<typename> class InContainer,
	template<typename, typename> class OutQueue, template<typename> class OutContainer>
class st_socket: public st_timer
{
protected:
	static const tid TIMER_BEGIN = st_timer::TIMER_END;
	static const tid TIMER_HANDLE_MSG = TIMER_BEGIN;
	static const tid TIMER_DISPATCH_MSG = TIMER_BEGIN + 1;
	static const tid TIMER_DELAY_CLOSE = TIMER_BEGIN + 2;
	static const tid TIMER_END = TIMER_BEGIN + 10;

	st_socket(boost::asio::io_service& io_service_) : st_timer(io_service_), next_layer_(io_service_) {first_init();}
	template<typename Arg> st_socket(boost::asio::io_service& io_service_, Arg& arg) : st_timer(io_service_), next_layer_(io_service_, arg) {first_init();}

	//helper function, just call it in constructor
	void first_init()
	{
		_id = -1;
		packer_ = boost::make_shared<Packer>();
		sending = paused_sending = false;
		dispatching = paused_dispatching = false;
		congestion_controlling = false;
		started_ = false;
		send_atomic.store(0, boost::memory_order_relaxed);
		dispatch_atomic.store(0, boost::memory_order_relaxed);
		start_atomic.store(0, boost::memory_order_relaxed);
	}

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
	}

	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();

		last_dispatch_msg.clear();
	}

public:
	typedef obj_with_begin_time<InMsgType> in_msg;
	typedef obj_with_begin_time<OutMsgType> out_msg;
	typedef InQueue<in_msg, InContainer<in_msg>> in_container_type;
	typedef OutQueue<out_msg, OutContainer<out_msg>> out_container_type;

	uint_fast64_t id() const {return _id;}
	bool is_equal_to(uint_fast64_t id) const {return _id == id;}

	Socket& next_layer() {return next_layer_;}
	const Socket& next_layer() const {return next_layer_;}
	typename Socket::lowest_layer_type& lowest_layer() {return next_layer().lowest_layer();}
	const typename Socket::lowest_layer_type& lowest_layer() const {return next_layer().lowest_layer();}

	virtual bool obsoleted() {return !dispatching && !started() && !is_async_calling();}

	bool started() const {return started_;}
	void start()
	{
		if (!started_)
		{
			scope_atomic_lock<> lock(start_atomic);
			if (!started_ && lock.locked())
				started_ = do_start();
		}
	}

	//return false if send buffer is empty or sending not allowed
	bool send_msg()
	{
		if (!sending)
		{
			scope_atomic_lock<> lock(send_atomic);
			if (!sending && lock.locked())
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
	bool is_sending_msg() const {return sending;}

	//for a st_socket that has been shut down, resuming message dispatching will not take effect for left messages.
	void suspend_dispatch_msg(bool suspend) {if (!(paused_dispatching = suspend)) dispatch_msg();}
	bool suspend_dispatch_msg() const {return paused_dispatching;}
	bool is_dispatching_msg() const {return dispatching;}

	void congestion_control(bool enable) {congestion_controlling = enable;}
	bool congestion_control() const {return congestion_controlling;}

	//in st_asio_wrapper, it's thread safe to access stat without mutex, because for a specific member of stat, st_asio_wrapper will never access it concurrently.
	//in other words, in a specific thread, st_asio_wrapper just access only one member of stat.
	//but user can access stat out of st_asio_wrapper via get_statistic function, although user can only read it, there's still a potential risk,
	//so whether it's thread safe or not depends on std::chrono::system_clock::duration.
	//i can make it thread safe in st_asio_wrapper, but is it worth to do so? this is a problem.
	const struct statistic& get_statistic() const {return stat;}

	//get or change the packer at runtime
	//changing packer at runtime is not thread-safe, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_packer<typename Packer::msg_type>> inner_packer() {return packer_;}
	boost::shared_ptr<const i_packer<typename Packer::msg_type>> inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer<typename Packer::msg_type>>& _packer_) {packer_ = _packer_;}

	//if you use can_overflow = true to invoke send_msg or send_native_msg, it will always succeed no matter the sending buffer is available or not,
	//this can exhaust all virtual memory, please pay special attentions.
	bool is_send_buffer_available() const {return send_msg_buffer.size() < ST_ASIO_MAX_MSG_NUM;}

	//don't use the packer but insert into send buffer directly
	bool direct_send_msg(const InMsgType& msg, bool can_overflow = false) {return direct_send_msg(InMsgType(msg), can_overflow);}
	bool direct_send_msg(InMsgType&& msg, bool can_overflow = false) {return can_overflow || is_send_buffer_available() ? do_direct_send_msg(std::move(msg)) : false;}

	//how many msgs waiting for sending or dispatching
	GET_PENDING_MSG_NUM(get_pending_send_msg_num, send_msg_buffer)
	GET_PENDING_MSG_NUM(get_pending_recv_msg_num, recv_msg_buffer)

	POP_FIRST_PENDING_MSG(pop_first_pending_send_msg, send_msg_buffer, in_msg)
	POP_FIRST_PENDING_MSG(pop_first_pending_recv_msg, recv_msg_buffer, out_msg)

	//clear all pending msgs
	POP_ALL_PENDING_MSG(pop_all_pending_send_msg, send_msg_buffer, in_container_type)
	POP_ALL_PENDING_MSG(pop_all_pending_recv_msg, recv_msg_buffer, out_container_type)

protected:
	virtual bool do_start() = 0;
	virtual bool do_send_msg() = 0; //st_socket will guarantee not call this function in more than one thread concurrently.
	virtual void do_recv_msg() = 0;

	virtual bool is_closable() {return true;}
	virtual bool is_send_allowed() {return !paused_sending && !stopped();} //can send msg or not(just put into send buffer)

	//generally, you don't have to rewrite this to maintain the status of connections(TCP)
	virtual void on_send_error(const boost::system::error_code& ec) {unified_out::error_out("send msg error (%d %s)", ec.value(), ec.message().data());}
	//receiving error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const boost::system::error_code& ec) = 0;
	//if ST_ASIO_DELAY_CLOSE is equal to zero, in this callback, st_socket guarantee that there's no any other async call associated it,
	//include user timers(created by set_timer()) and user async calls(started via post()), this means you can clean up any resource
	//in this st_socket except this st_socket itself, because this st_socket maybe is being maintained by st_object_pool.
	//otherwise (bigger than zero), st_socket simply call this callback ST_ASIO_DELAY_CLOSE seconds later after link down, no any guarantees.
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

	//subclass notify shutdown event, not thread safe
	void close()
	{
		if (started_)
		{
			started_ = false;

			if (is_closable())
			{
				set_async_calling(true);
				set_timer(TIMER_DELAY_CLOSE, ST_ASIO_DELAY_CLOSE * 1000 + 50, [this](tid id)->bool {return ST_THIS timer_handler(id);});
			}
		}
	}

	//call this in subclasses' recv_handler only
	//subclasses must guarantee not call this function in more than one thread concurrently.
	void handle_msg()
	{
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
		decltype(temp_msg_buffer) temp_buffer;
		if (!temp_msg_buffer.empty() && !paused_dispatching && !congestion_controlling)
		{
			auto_duration(stat.handle_time_1_sum);
			for (auto iter = std::begin(temp_msg_buffer); !paused_dispatching && !congestion_controlling && iter != std::end(temp_msg_buffer);)
				if (on_msg(*iter))
					temp_msg_buffer.erase(iter++);
				else
					temp_buffer.splice(std::end(temp_buffer), temp_msg_buffer, iter++);
		}
#else
		auto temp_buffer(std::move(temp_msg_buffer));
#endif

		if (!temp_buffer.empty())
		{
			recv_msg_buffer.move_items_in(temp_buffer);
			dispatch_msg();
		}

		if (temp_msg_buffer.empty() && recv_msg_buffer.size() < ST_ASIO_MAX_MSG_NUM)
			do_recv_msg(); //receive msg sequentially, which means second receiving only after first receiving success
		else
		{
			recv_idle_begin_time = statistic::local_time();
			set_timer(TIMER_HANDLE_MSG, 50, [this](tid id)->bool {return ST_THIS timer_handler(id);});
		}
	}

	//return false if receiving buffer is empty or dispatching not allowed (include io_service stopped)
	bool dispatch_msg()
	{
		if (!dispatching)
		{
			scope_atomic_lock<> lock(dispatch_atomic);
			if (!dispatching && lock.locked())
			{
				dispatching = true;
				lock.unlock();

				if (!do_dispatch_msg())
					dispatching = false;
			}
		}

		return dispatching;
	}

	//return false if receiving buffer is empty or dispatching not allowed (include io_service stopped)
	bool do_dispatch_msg()
	{
		if (paused_dispatching)
			;
		else if (stopped())
		{
#ifndef ST_ASIO_DISCARD_MSG_WHEN_LINK_DOWN
			if (!last_dispatch_msg.empty())
				on_msg_handle(last_dispatch_msg, true);
#endif
			typename out_container_type::lock_guard lock(recv_msg_buffer);
#ifndef ST_ASIO_DISCARD_MSG_WHEN_LINK_DOWN
			while (recv_msg_buffer.try_dequeue_(last_dispatch_msg))
				on_msg_handle(last_dispatch_msg, true);
#endif
			recv_msg_buffer.clear();
			last_dispatch_msg.clear();
		}
		else if (!last_dispatch_msg.empty() || recv_msg_buffer.try_dequeue(last_dispatch_msg))
		{
			post([this]() {ST_THIS msg_handler();});
			return true;
		}

		return false;
	}

	bool do_direct_send_msg(InMsgType&& msg)
	{
		if (msg.empty())
			unified_out::error_out("found an empty message, please check your packer.");
		else
		{
			send_msg_buffer.enqueue(in_msg(std::move(msg)));
			send_msg();
		}

		//even if we meet an empty message (most likely, this is because message length is too long, or insufficient memory), we still return true, why?
		//please think about the function safe_send_(native_)msg, if we keep returning false, it will enter a dead loop.
		//the packer provider has the responsibility to write detailed reasons down when packing message failed.
		return true;
	}

private:
	//please do not change id at runtime via the following function, except this st_socket is not managed by st_object_pool,
	//it should only be used by st_object_pool when reusing or creating new st_socket.
	template<typename Object> friend class st_object_pool;
	void id(uint_fast64_t id) {_id = id;}

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
			if (!is_last_async_call())
			{
				stop_all_timer();
				revive_timer(TIMER_DELAY_CLOSE);

				return true;
			}
			else if (lowest_layer().is_open())
			{
				boost::system::error_code ec;
				lowest_layer().close(ec);
			}
			on_close();
			set_async_calling(false);

			break;
		default:
			assert(false);
			break;
		}

		return false;
	}

	void msg_handler()
	{
		auto begin_time = statistic::local_time();
		stat.dispatch_dealy_sum += begin_time - last_dispatch_msg.begin_time;
		bool re = on_msg_handle(last_dispatch_msg, false); //must before next msg dispatching to keep sequence
		auto end_time = statistic::local_time();
		stat.handle_time_2_sum += end_time - begin_time;

		if (!re) //dispatch failed, re-dispatch
		{
			last_dispatch_msg.restart(end_time);
			dispatching = false;
			set_timer(TIMER_DISPATCH_MSG, 50, [this](tid id)->bool {return ST_THIS timer_handler(id);});
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
	uint_fast64_t _id;
	Socket next_layer_;

	out_msg last_dispatch_msg;
	boost::shared_ptr<i_packer<typename Packer::msg_type>> packer_;

	in_container_type send_msg_buffer;
	out_container_type recv_msg_buffer;
	boost::container::list<out_msg> temp_msg_buffer;
	//subclass will invoke handle_msg() when got some msgs. if these msgs can't be pushed into recv_msg_buffer because of:
	// 1. msg dispatching suspended;
	// 2. congestion control opened;
	//st_socket will delay 50 milliseconds(non-blocking) to invoke handle_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.

	volatile bool sending;
	bool paused_sending;
	st_atomic_size_t send_atomic;

	volatile bool dispatching;
	bool paused_dispatching;
	st_atomic_size_t dispatch_atomic;

	volatile bool congestion_controlling;

	volatile bool started_; //has started or not
	st_atomic_size_t start_atomic;

	struct statistic stat;
	typename statistic::stat_time recv_idle_begin_time;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

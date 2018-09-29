/*
 * socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef ST_ASIO_TCP_SOCKET_H_
#define ST_ASIO_TCP_SOCKET_H_

#include "../socket.h"

namespace st_asio_wrapper { namespace tcp {

template <typename Socket, typename Packer, typename Unpacker,
	template<typename, typename> class InQueue, template<typename> class InContainer,
	template<typename, typename> class OutQueue, template<typename> class OutContainer>
class socket_base : public socket<Socket, Packer, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

private:
	typedef socket<Socket, Packer, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

protected:
	enum link_status {CONNECTED, FORCE_SHUTTING_DOWN, GRACEFUL_SHUTTING_DOWN, BROKEN};

	socket_base(boost::asio::io_context& io_context_) : super(io_context_), strand(io_context_) {first_init();}
	template<typename Arg> socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg), strand(io_context_) {first_init();}

	//helper function, just call it in constructor
	void first_init() {status = BROKEN; unpacker_ = boost::make_shared<Unpacker>();}

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

	virtual bool obsoleted() {return !is_shutting_down() && super::obsoleted();}
	virtual bool is_ready() {return is_connected();}
	virtual void send_heartbeat()
	{
		auto_duration dur(stat.pack_time_sum);
		in_msg_type msg;
		packer_->pack_heartbeat(msg);
		dur.end();
		ST_THIS do_direct_send_msg(msg);
	}

	//reset all, be ensure that there's no any operations performed on this tcp::socket_base when invoke it
	void reset() {status = BROKEN; last_send_msg.clear(); unpacker_->reset(); super::reset();}

	//SOCKET status
	bool is_broken() const {return BROKEN == status;}
	bool is_connected() const {return CONNECTED == status;}
	bool is_shutting_down() const {return FORCE_SHUTTING_DOWN == status || GRACEFUL_SHUTTING_DOWN == status;}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		BOOST_AUTO(local_ep, ST_THIS lowest_layer().local_endpoint(ec));
		if (!ec)
		{
			BOOST_AUTO(remote_ep, ST_THIS lowest_layer().remote_endpoint(ec));
			if (!ec)
				unified_out::info_out("%s (%s:%hu %s:%hu) %s", head,
					local_ep.address().to_string().data(), local_ep.port(),
					remote_ep.address().to_string().data(), remote_ep.port(), tail);
		}
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		BOOST_AUTO(local_ep, ST_THIS lowest_layer().local_endpoint(ec2));
		if (!ec2)
		{
			BOOST_AUTO(remote_ep, ST_THIS lowest_layer().remote_endpoint(ec2));
			if (!ec2)
				unified_out::info_out("%s (%s:%hu %s:%hu) %s (%d %s)", head,
					local_ep.address().to_string().data(), local_ep.port(),
					remote_ep.address().to_string().data(), remote_ep.port(), tail, ec.value(), ec.message().data());
		}
	}

	void show_status() const
	{
		unified_out::info_out(
			"\n\tid: " ST_ASIO_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ST_ASIO_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\tlink status: %d"
			"\n\trecv suspended: %d",
			ST_THIS id(), ST_THIS started(), ST_THIS is_sending(),
#ifdef ST_ASIO_PASSIVE_RECV
			ST_THIS is_reading(),
#endif
			ST_THIS is_dispatching(), status, ST_THIS is_recv_idle());
	}

	//get or change the unpacker at runtime
	boost::shared_ptr<i_unpacker<out_msg_type> > unpacker() {return unpacker_;}
	boost::shared_ptr<const i_unpacker<out_msg_type> > unpacker() const {return unpacker_;}
#ifdef ST_ASIO_PASSIVE_RECV
	//changing unpacker must before calling st_asio_wrapper::socket::recv_msg, and define ST_ASIO_PASSIVE_RECV macro.
	void unpacker(const boost::shared_ptr<i_unpacker<out_msg_type> >& _unpacker_) {unpacker_ = _unpacker_;}
	virtual void recv_msg() {if (!reading && is_ready()) ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_recv_msg, this));}
#endif

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false, do_direct_send_msg) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true, do_direct_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ST_ASIO_SYNC_SEND
	TCP_SYNC_SEND_MSG(sync_send_msg, false, do_direct_sync_send_msg) //use the packer with native = false to pack the msgs
	TCP_SYNC_SEND_MSG(sync_send_native_msg, true, do_direct_sync_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	void force_shutdown() {if (FORCE_SHUTTING_DOWN != status) shutdown();}
	void graceful_shutdown(bool sync) //will block until shutdown success or time out if sync equal to true
	{
		if (is_broken())
			shutdown();
		else if (!is_shutting_down())
		{
			status = GRACEFUL_SHUTTING_DOWN;

			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			if (ec) //graceful shutdown is impossible
				shutdown();
			else if (!sync)
				ST_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, boost::lambda::if_then_else_return(boost::lambda::bind(&socket_base::async_shutdown_handler, this,
					ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100), true, false));
			else
			{
				int loop_num = ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100; //seconds to 10 milliseconds
				while (--loop_num >= 0 && GRACEFUL_SHUTTING_DOWN == status)
					boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
				if (loop_num < 0) //graceful shutdown is impossible
				{
					unified_out::info_out("failed to graceful shutdown within %d seconds", ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
					shutdown();
				}
			}
		}
	}

	virtual bool do_start()
	{
		status = CONNECTED;
		stat.establish_time = time(NULL);

		on_connect(); //in this virtual function, stat.last_recv_time has not been updated (super::do_start will update it), please note
		return super::do_start();
	}

#ifdef ST_ASIO_SYNC_SEND
	virtual void on_close() {for (BOOST_AUTO(iter, last_send_msg.begin()); iter != last_send_msg.end(); ++iter) if (iter->cv) iter->cv->notify_all(); super::on_close();}
#endif

	virtual void on_connect() {}
	//msg can not be unpacked
	//the socket is still available, so don't need to shutdown this tcp::socket_base
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

private:
#ifndef ST_ASIO_PASSIVE_RECV
	virtual void recv_msg() {ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_recv_msg, this));}
#endif
	virtual void send_msg() {ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_send_msg, this, false));}

	void shutdown()
	{
		if (!is_broken())
			status = FORCE_SHUTTING_DOWN; //not thread safe because of this assignment
		ST_THIS close();
	}

	size_t completion_checker(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		auto_duration dur(stat.unpack_time_sum);
		return ST_THIS unpacker_->completion_condition(ec, bytes_transferred);
	}

	void do_recv_msg()
	{
#ifdef ST_ASIO_PASSIVE_RECV
		if (reading)
			return;
#endif
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);
		if (0 == boost::asio::buffer_size(recv_buff))
			unified_out::error_out("The unpacker returned an empty buffer, quit receiving!");
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = true;
#endif
			boost::asio::async_read(ST_THIS next_layer(), recv_buff,
				boost::bind(&socket_base::completion_checker, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred), make_strand_handler(strand,
				ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
		}
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			stat.last_recv_time = time(NULL);

			auto_duration dur(stat.unpack_time_sum);
			bool unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			if (!unpack_ok)
				on_unpack_error(); //the user will decide whether to reset the unpacker or not in this callback

#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			if (ST_THIS handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			if (ec)
				ST_THIS on_recv_error(ec);
			else if (ST_THIS handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
	}

	bool do_send_msg(bool in_strand)
	{
		if (!in_strand && sending)
			return true;

		boost::container::list<boost::asio::const_buffer> bufs;
		{
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			const size_t max_send_size = 1;
#else
			const size_t max_send_size = boost::asio::detail::default_max_transfer_size;
#endif
			size_t size = 0;
			typename super::in_msg msg;
			BOOST_AUTO(end_time, statistic::now());

			typename super::in_queue_type::lock_guard lock(send_msg_buffer);
			while (send_msg_buffer.try_dequeue_(msg))
			{
				stat.send_delay_sum += end_time - msg.begin_time;
				size += msg.size();
				last_send_msg.emplace_back();
				last_send_msg.back().swap(msg);
				bufs.emplace_back(last_send_msg.back().data(), last_send_msg.back().size());
				if (size >= max_send_size)
					break;
			}
		}

		if ((sending = !bufs.empty()))
		{
			last_send_msg.front().restart();
			boost::asio::async_write(ST_THIS next_layer(), bufs, make_strand_handler(strand,
				ST_THIS make_handler_error_size(boost::bind(&socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
			return true;
		}

		return false;
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			stat.last_send_time = time(NULL);

			stat.send_byte_sum += bytes_transferred;
			stat.send_time_sum += statistic::now() - last_send_msg.front().begin_time;
			stat.send_msg_sum += last_send_msg.size();
#ifdef ST_ASIO_SYNC_SEND
			for (BOOST_AUTO(iter, last_send_msg.begin()); iter != last_send_msg.end(); ++iter)
				if (iter->cv)
				{
					iter->cv->signaled = true;
					iter->cv->notify_one();
				}
#endif
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(last_send_msg.front());
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
			if (send_msg_buffer.empty())
				ST_THIS on_all_msg_send(last_send_msg.back());
#endif
			last_send_msg.clear();
			if (!do_send_msg(true) && !send_msg_buffer.empty()) //send msg in sequence
				do_send_msg(true); //just make sure no pending msgs
		}
		else
		{
			ST_THIS on_send_error(ec);
			last_send_msg.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			sending = false;
		}
	}

	bool async_shutdown_handler(size_t loop_num)
	{
		if (GRACEFUL_SHUTTING_DOWN == status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS change_timer_call_back(TIMER_ASYNC_SHUTDOWN, boost::lambda::if_then_else_return(boost::lambda::bind(&socket_base::async_shutdown_handler, this,
					loop_num), true, false));
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
				on_async_shutdown_error();
			}
		}

		return false;
	}

protected:
	volatile link_status status;

private:
	using super::stat;
	using super::packer_;
	using super::temp_msg_can;

	using super::send_msg_buffer;
	using super::sending;

#ifdef ST_ASIO_PASSIVE_RECV
	using super::reading;
#endif

	boost::shared_ptr<i_unpacker<out_msg_type> > unpacker_;
	boost::container::list<typename super::in_msg> last_send_msg;
	boost::asio::io_context::strand strand;
};

}} //namespace

#endif /* ST_ASIO_TCP_SOCKET_H_ */

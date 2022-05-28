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

template<typename Socket, typename OutMsgType> class reader_writer : public Socket
{
public:
	reader_writer(boost::asio::io_context& io_context_) : Socket(io_context_) {}
	template<typename Arg> reader_writer(boost::asio::io_context& io_context_, Arg& arg) : Socket(io_context_, arg) {}

	typedef boost::function<void(const boost::system::error_code& ec, size_t bytes_transferred)> ReadWriteCallBack;

protected:
	bool async_read(const ReadWriteCallBack& call_back)
	{
		BOOST_AUTO(recv_buff, ST_THIS unpacker()->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);
		if (0 == boost::asio::buffer_size(recv_buff))
		{
			unified_out::error_out(ST_ASIO_LLF " the unpacker returned an empty buffer, quit receiving!", ST_THIS id());
			return false;
		}

		boost::asio::async_read(ST_THIS next_layer(), recv_buff,
			boost::bind(&reader_writer::completion_checker, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred), call_back);
		return true;
	}
	bool parse_msg(size_t bytes_transferred, list<OutMsgType>& msg_can) {return ST_THIS unpacker()->parse_msg(bytes_transferred, msg_can);}

	size_t batch_msg_send_size() const
	{
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
		return 0;
#else
		return boost::asio::detail::default_max_transfer_size;
#endif
	}
	template<typename Buffer>
	void async_write(const Buffer& msg_can, const ReadWriteCallBack& call_back) {boost::asio::async_write(ST_THIS next_layer(), msg_can, call_back);}

private:
	size_t completion_checker(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		auto_duration dur(ST_THIS stat.unpack_time_sum);
		return ST_THIS unpacker()->completion_condition(ec, bytes_transferred);
	}
};

template<typename Socket, typename Packer, typename Unpacker,
	template<typename> class InQueue, template<typename> class InContainer, template<typename> class OutQueue, template<typename> class OutContainer,
	template<typename, typename> class ReaderWriter = reader_writer>
class socket_base : public ReaderWriter<socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>, typename Unpacker::msg_type>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

private:
	typedef ReaderWriter<socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>, out_msg_type> super;

protected:
	enum link_status {CONNECTED, FORCE_SHUTTING_DOWN, GRACEFUL_SHUTTING_DOWN, BROKEN, HANDSHAKING};

	socket_base(boost::asio::io_context& io_context_) : super(io_context_), status(BROKEN) {}
	template<typename Arg> socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg), status(BROKEN) {}

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
		ST_THIS packer()->pack_heartbeat(msg);
		dur.end();

		if (!msg.empty())
			do_direct_send_msg(msg);
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function, so if you want to do some additional initialization
	// for this socket, do it at here and in the constructor.
	//for tcp::single_client_base and ssl::single_client_base, this virtual function will never be called, please note.
	virtual void reset() {status = BROKEN; sending_msgs.clear(); super::reset();}

	//SOCKET status
	link_status get_link_status() const {return status;}
	bool is_broken() const {return BROKEN == status;}
	bool is_connected() const {return CONNECTED == status;}
	bool is_shutting_down() const {return FORCE_SHUTTING_DOWN == status || GRACEFUL_SHUTTING_DOWN == status;}

	std::string endpoint_to_string(const boost::asio::ip::tcp::endpoint& ep) const {return ep.address().to_string() + ':' + boost::to_string(ep.port());}
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
	std::string endpoint_to_string(const boost::asio::local::stream_protocol::endpoint& ep) const {return ep.path();}
#endif

	void show_info(const char* head = NULL, const char* tail = NULL) const
	{
		boost::system::error_code ec;
		BOOST_AUTO(local_ep, ST_THIS lowest_layer().local_endpoint(ec));
		if (!ec)
		{
			BOOST_AUTO(remote_ep, ST_THIS lowest_layer().remote_endpoint(ec));
			if (!ec)
				unified_out::info_out(ST_ASIO_LLF " %s (%s %s) %s", ST_THIS id(), NULL == head ? "" : head,
					endpoint_to_string(local_ep).data(), endpoint_to_string(remote_ep).data(), NULL == tail ? "" : tail);
		}
	}

	void show_info(const boost::system::error_code& ec, const char* head = NULL, const char* tail = NULL) const
	{
		boost::system::error_code ec2;
		BOOST_AUTO(local_ep, ST_THIS lowest_layer().local_endpoint(ec2));
		if (!ec2)
		{
			BOOST_AUTO(remote_ep, ST_THIS lowest_layer().remote_endpoint(ec2));
			if (!ec2)
				unified_out::info_out(ST_ASIO_LLF " %s (%s %s) %s (%d %s)", ST_THIS id(), NULL == head ? "" : head,
					endpoint_to_string(local_ep).data(), endpoint_to_string(remote_ep).data(), NULL == tail ? "" : tail, ec.value(), ec.message().data());
		}
	}

	void show_status() const
	{
		std::stringstream s;

		if (stat.establish_time > stat.break_time)
		{
			s << "\n\testablish time: ";
			log_formater::to_time_str(stat.establish_time, s);
		}
		else if (stat.break_time > 0)
		{
			s << "\n\tbreak time: ";
			log_formater::to_time_str(stat.break_time, s);
		}

		if (stat.last_send_time > 0)
		{
			s << "\n\tlast send time: ";
			log_formater::to_time_str(stat.last_send_time, s);
		}

		if (stat.last_recv_time > 0)
		{
			s << "\n\tlast recv time: ";
			log_formater::to_time_str(stat.last_recv_time, s);
		}

		unified_out::info_out(
			"\n\tid: " ST_ASIO_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ST_ASIO_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\tlink status: %d"
			"\n\trecv suspended: %d"
			"\n\tsend buffer usage: %.2f%%"
			"\n\trecv buffer usage: %.2f%%"
			"%s",
			ST_THIS id(), ST_THIS started(), ST_THIS is_sending(),
#ifdef ST_ASIO_PASSIVE_RECV
			ST_THIS is_reading(),
#endif
			ST_THIS is_dispatching(), status, ST_THIS is_recv_idle(),
			ST_THIS send_buf_usage() * 100.f, ST_THIS recv_buf_usage() * 100.f, s.str().data());
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//if the message already packed, do call direct_send_msg or direct_sync_send_msg to reduce unnecessary memory replication, if you will not
	// use it any more, call the one that accepts reference of a message.
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ST_ASIO_SYNC_SEND
	TCP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	TCP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
#ifdef ST_ASIO_EXPOSE_SEND_INTERFACE
	using super::send_msg;
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
			ST_THIS lowest_layer().shutdown(boost::asio::socket_base::shutdown_send, ec);
			if (ec) //graceful shutdown is impossible
				shutdown();
			else if (!sync)
				ST_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, boost::lambda::if_then_else_return(boost::lambda::bind(&socket_base::shutdown_handler, this,
					ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100), true, false));
			else
			{
				int loop_num = ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100; //seconds to 10 milliseconds
				while (--loop_num >= 0 && GRACEFUL_SHUTTING_DOWN == status)
					boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
				if (loop_num < 0) //graceful shutdown is impossible
				{
					unified_out::info_out(ST_ASIO_LLF " failed to graceful shutdown within %d seconds", ST_THIS id(), ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
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

#ifdef ST_ASIO_WANT_BATCH_MSG_SEND_NOTIFY
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	#ifdef _MSC_VER
		#pragma message("macro ST_ASIO_WANT_BATCH_MSG_SEND_NOTIFY will not take effect with macro ST_ASIO_WANT_MSG_SEND_NOTIFY.")
	#else
		#warning macro ST_ASIO_WANT_BATCH_MSG_SEND_NOTIFY will not take effect with macro ST_ASIO_WANT_MSG_SEND_NOTIFY.
	#endif
#else
	//some messages have been sent to the kernel buffer
	//notice: messages are packed, using inconstant reference is for the ability of swapping
	virtual void on_msg_send(typename super::in_container_type& msg_can) = 0;
#endif
#endif

	//generally, you don't have to rewrite this to maintain the status of connections
	//msg_can contains messages that were failed to send and tcp::socket_base will not hold them any more, if you want to re-send them in the future,
	// you must take over them and re-send (at any time) them via direct_send_msg.
	//DO NOT hold msg_can for further usage, just swap its content with your own container in this virtual function.
	virtual void on_send_error(const boost::system::error_code& ec, typename super::in_container_type& msg_can)
		{unified_out::error_out(ST_ASIO_LLF " send msg error (%d %s)", ST_THIS id(), ec.value(), ec.message().data());}

	virtual void on_recv_error(const boost::system::error_code& ec) = 0;

	virtual void on_close()
	{
#ifdef ST_ASIO_SYNC_SEND
		for (BOOST_AUTO(iter, sending_msgs.begin()); iter != sending_msgs.end(); ++iter)
			if (iter->p)
				iter->p->set_value(NOT_APPLICABLE);
#endif
		status = BROKEN;
		super::on_close();
	}

	virtual void on_connect() {}
	//msg can not be unpacked
	//the socket is still available, so don't need to shutdown this tcp::socket_base if the unpacker can continue to work.
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

private:
	using super::close;
	using super::handle_error;
	using super::handle_msg;
	using super::do_direct_send_msg;
#ifdef ST_ASIO_SYNC_SEND
	using super::do_direct_sync_send_msg;
#endif

	void close_() {close(true);} //workaround for old compilers, otherwise, we can bind to close directly in dispatch_strand
	void shutdown()
	{
		if (is_broken())
			ST_THIS dispatch_strand(rw_strand, boost::bind(&socket_base::close_, this));
		else
		{
			status = FORCE_SHUTTING_DOWN; //not thread safe because of this assignment
			close();
		}
	}

	virtual void do_recv_msg()
	{
#ifdef ST_ASIO_PASSIVE_RECV
		if (reading || !is_ready())
			return;
#endif
#ifdef ST_ASIO_PASSIVE_RECV
		if (ST_THIS async_read(make_strand_handler(rw_strand,
			ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)))))
			reading = true;
#else
		ST_THIS async_read(make_strand_handler(rw_strand,
			ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
#endif
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
#ifdef ST_ASIO_PASSIVE_RECV
		reading = false; //clear reading flag before calling handle_msg() to make sure that recv_msg() is available in on_msg() and on_msg_handle()
#endif
		bool need_next_recv = false;
		if (bytes_transferred > 0)
		{
			stat.last_recv_time = time(NULL);

			auto_duration dur(stat.unpack_time_sum);
			bool unpack_ok = ST_THIS parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			if (!unpack_ok)
			{
				if (!ec)
					on_unpack_error();

				ST_THIS unpacker()->reset(); //user can get the left half-baked msg in unpacker's reset()
			}

			need_next_recv = handle_msg(); //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
		}
		else if (!ec)
		{
			assert(false);
			unified_out::error_out(ST_ASIO_LLF " read 0 byte without any errors which is unexpected, please check your unpacker!", ST_THIS id());
		}

		if (ec)
		{
			handle_error();
			on_recv_error(ec);
		}
		//if you wrote an terrible unpacker whoes completion_condition always returns 0, it will cause st_asio_wrapper to occupies almost all CPU resources
		// because of following do_recv_msg() invocation (rapidly and repeatedly), please note.
		else if (need_next_recv)
			do_recv_msg(); //receive msg in sequence
	}

	virtual bool do_send_msg(bool in_strand = false)
	{
		if (!in_strand && sending)
			return true;

		BOOST_AUTO(end_time, statistic::now());
		send_buffer.move_items_out(ST_THIS batch_msg_send_size(), sending_msgs);
		std::vector<boost::asio::const_buffer> bufs;
		bufs.reserve(sending_msgs.size());
		for (BOOST_AUTO(iter, sending_msgs.begin()); iter != sending_msgs.end(); ++iter)
		{
			stat.send_delay_sum += end_time - iter->begin_time;
			bufs.push_back(boost::asio::const_buffer(iter->data(), iter->size()));
		}

		if ((sending = !bufs.empty()))
		{
			sending_msgs.front().restart();
			ST_THIS async_write(bufs, make_strand_handler(rw_strand,
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
			stat.send_time_sum += statistic::now() - sending_msgs.front().begin_time;
			stat.send_msg_sum += sending_msgs.size();
#ifdef ST_ASIO_SYNC_SEND
			for (BOOST_AUTO(iter, sending_msgs.begin()); iter != sending_msgs.end(); ++iter)
				if (iter->p)
					iter->p->set_value(SUCCESS);
#endif
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(sending_msgs.front());
#elif defined(ST_ASIO_WANT_BATCH_MSG_SEND_NOTIFY)
			on_msg_send(sending_msgs);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
			if (send_buffer.empty())
#if defined(ST_ASIO_WANT_MSG_SEND_NOTIFY) || !defined(ST_ASIO_WANT_BATCH_MSG_SEND_NOTIFY)
				ST_THIS on_all_msg_send(sending_msgs.back());
#else
			{
				if (!sending_msgs.empty())
					ST_THIS on_all_msg_send(sending_msgs.back());
				else //on_msg_send consumed all messages
				{
					in_msg_type msg;
					ST_THIS on_all_msg_send(msg);
				}
			}
#endif
#endif
			sending_msgs.clear();
#ifndef ST_ASIO_ARBITRARY_SEND
			if (!do_send_msg(true) && !send_buffer.empty()) //send msg in sequence
#endif
				do_send_msg(true); //just make sure no pending msgs
		}
		else
		{
#ifdef ST_ASIO_SYNC_SEND
			for (BOOST_AUTO(iter, sending_msgs.begin()); iter != sending_msgs.end(); ++iter)
				if (iter->p)
					iter->p->set_value(NOT_APPLICABLE);
#endif
			on_send_error(ec, sending_msgs);
			sending_msgs.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			sending = false;
		}
	}

	bool shutdown_handler(size_t loop_num)
	{
		if (GRACEFUL_SHUTTING_DOWN == status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS change_timer_call_back(TIMER_ASYNC_SHUTDOWN, boost::lambda::if_then_else_return(boost::lambda::bind(&socket_base::shutdown_handler, this,
					loop_num), true, false));
				return true;
			}
			else
			{
				unified_out::info_out(ST_ASIO_LLF " failed to graceful shutdown within %d seconds", ST_THIS id(), ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
				on_async_shutdown_error();
			}
		}

		return false;
	}

protected:
	volatile link_status status;

private:
	using super::stat;
	using super::temp_msg_can;

	using super::send_buffer;
	using super::sending;

#ifdef ST_ASIO_PASSIVE_RECV
	using super::reading;
#endif
	using super::rw_strand;

	typename super::in_container_type sending_msgs;
};

}} //namespace

#endif /* ST_ASIO_TCP_SOCKET_H_ */

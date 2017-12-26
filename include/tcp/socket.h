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
class socket_base : public socket<Socket, Packer, Unpacker, typename Packer::msg_type, typename Unpacker::msg_type, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

private:
	typedef socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

protected:
	enum link_status {CONNECTED, FORCE_SHUTTING_DOWN, GRACEFUL_SHUTTING_DOWN, BROKEN};

	socket_base(boost::asio::io_context& io_context_) : super(io_context_) {first_init();}
	template<typename Arg> socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {first_init();}

	//helper function, just call it in constructor
	void first_init() {status = BROKEN; unpacker_ = boost::make_shared<Unpacker>();}

public:
	static const timer::tid TIMER_BEGIN = super::TIMER_END;
	static const timer::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const timer::tid TIMER_END = TIMER_BEGIN + 10;

	virtual bool obsoleted() {return !is_shutting_down() && super::obsoleted();}
	virtual bool is_ready() {return is_connected();}
	virtual void send_heartbeat()
	{
		auto_duration dur(ST_THIS stat.pack_time_sum);
		in_msg_type msg;
		ST_THIS packer_->pack_heartbeat(msg);
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

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_unpacker<out_msg_type> > unpacker() {return unpacker_;}
	boost::shared_ptr<const i_unpacker<out_msg_type> > unpacker() const {return unpacker_;}
	void unpacker(const boost::shared_ptr<i_unpacker<out_msg_type> >& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//send message with sync mode
	//return 0 means empty message or this socket is busy on sending messages
	//return -1 means error occurred, otherwise the number of bytes been sent
	TCP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	size_t direct_sync_send_msg(in_msg_ctype& msg)
	{
		if (msg.empty())
			unified_out::error_out("empty message, will not send it.");
		else if (ST_THIS lock_sending_flag())
			return do_sync_send_msg(msg);

		return 0;
	}
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
					boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(10));
				if (loop_num < 0) //graceful shutdown is impossible
				{
					unified_out::info_out("failed to graceful shutdown within %d seconds", ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
					shutdown();
				}
			}
		}
	}

	//send message with sync mode
	//return -1 means error occurred, otherwise the number of bytes been sent
	size_t do_sync_send_msg(in_msg_ctype& msg)
	{
		boost::system::error_code ec;
		auto_duration dur(ST_THIS stat.send_time_sum);
		size_t send_size = boost::asio::write(ST_THIS next_layer(), ST_ASIO_SEND_BUFFER_TYPE(msg.data(), msg.size()), ec);
		dur.end();

		send_handler(ec, send_size);
		return ec ? -1 : send_size;
	}

	virtual bool do_start()
	{
		status = CONNECTED;
		ST_THIS stat.establish_time = time(NULL);

		on_connect(); //in this virtual function, this->stat.last_recv_time has not been updated, please note
		return super::do_start();
	}

	//return false if send buffer is empty
	virtual bool do_send_msg()
	{
		boost::container::list<boost::asio::const_buffer> bufs;
		{
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			const size_t max_send_size = 1;
#else
			const size_t max_send_size = boost::asio::detail::default_max_transfer_size;
#endif
			size_t size = 0;
			typename super::in_msg msg;
			BOOST_AUTO(end_time, statistic::local_time());

			typename super::in_container_type::lock_guard lock(ST_THIS send_msg_buffer);
			while (ST_THIS send_msg_buffer.try_dequeue_(msg))
			{
				ST_THIS stat.send_delay_sum += end_time - msg.begin_time;
				size += msg.size();
				last_send_msg.emplace_back();
				last_send_msg.back().swap(msg);
				bufs.emplace_back(last_send_msg.back().data(), last_send_msg.back().size());
				if (size >= max_send_size)
					break;
			}
		}

		if (bufs.empty())
			return false;

		last_send_msg.front().restart();
		boost::asio::async_write(ST_THIS next_layer(), bufs,
			ST_THIS make_handler_error_size(boost::bind(&socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
		return true;
	}

	virtual bool do_send_msg(in_msg_type& msg)
	{
		last_send_msg.emplace_back(boost::ref(msg));
		boost::asio::async_write(ST_THIS next_layer(), ST_ASIO_SEND_BUFFER_TYPE(last_send_msg.back().data(), last_send_msg.back().size()),
			ST_THIS make_handler_error_size(boost::bind(&socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
		return true;
	}

	virtual void do_recv_msg()
	{
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);

		boost::asio::async_read(ST_THIS next_layer(), recv_buff,
			boost::bind(&socket_base::completion_checker, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred),
			ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	virtual void on_connect() {}
	//msg can not be unpacked
	//the link is still available, so don't need to shutdown this tcp::socket_base at both client and server endpoint
	virtual void on_unpack_error() = 0;
	virtual void on_async_shutdown_error() = 0;

#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}

private:
	void shutdown()
	{
		if (!is_broken())
			status = FORCE_SHUTTING_DOWN; //not thread safe because of this assignment
		ST_THIS close();
	}

	size_t completion_checker(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		auto_duration dur(ST_THIS stat.unpack_time_sum);
		return ST_THIS unpacker_->completion_condition(ec, bytes_transferred);
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			ST_THIS stat.last_recv_time = time(NULL);

			typename Unpacker::container_type temp_msg_can;
			auto_duration dur(ST_THIS stat.unpack_time_sum);
			bool unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_can);
			dur.end();

			size_t msg_num = temp_msg_can.size();
			if (msg_num > 0)
			{
				ST_THIS stat.recv_msg_sum += msg_num;
				ST_THIS temp_msg_buffer.resize(ST_THIS temp_msg_buffer.size() + msg_num);
				BOOST_AUTO(op_iter, ST_THIS temp_msg_buffer.rbegin());
				for (BOOST_AUTO(iter, temp_msg_can.rbegin()); iter != temp_msg_can.rend(); ++op_iter, ++iter)
				{
					ST_THIS stat.recv_byte_sum += iter->size();
					op_iter->swap(*iter);
				}
			}
			ST_THIS handle_msg();

			if (!unpack_ok)
				on_unpack_error(); //the user will decide whether to reset the unpacker or not in this callback
		}
		else
			ST_THIS on_recv_error(ec);
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			ST_THIS stat.last_send_time = time(NULL);

			ST_THIS stat.send_byte_sum += bytes_transferred;
			if (last_send_msg.empty()) //send message with sync mode
				++ST_THIS stat.send_msg_sum;
			else
			{
				ST_THIS stat.send_time_sum += statistic::local_time() - last_send_msg.front().begin_time;
				ST_THIS stat.send_msg_sum += last_send_msg.size();
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
				ST_THIS on_msg_send(last_send_msg.front());
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
				if (ST_THIS send_msg_buffer.empty())
					ST_THIS on_all_msg_send(last_send_msg.back());
#endif
				last_send_msg.clear();
			}

			if (!do_send_msg()) //send msg in sequence
			{
				ST_THIS sending = false;
				if (!ST_THIS send_msg_buffer.empty())
					ST_THIS send_msg(); //just make sure no pending msgs
			}
		}
		else
		{
			ST_THIS on_send_error(ec);
			last_send_msg.clear(); //clear sending messages after on_send_error, then user can decide how to deal with them in on_send_error

			ST_THIS sending = false; //must after the erasure of last_send_msg to avoid race condition
		}
	}

	bool async_shutdown_handler(size_t loop_num)
	{
		if (GRACEFUL_SHUTTING_DOWN == status)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS update_timer_info(TIMER_ASYNC_SHUTDOWN, 10, boost::lambda::if_then_else_return(boost::lambda::bind(&socket_base::async_shutdown_handler, this,
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
	boost::container::list<typename super::in_msg> last_send_msg;
	boost::shared_ptr<i_unpacker<out_msg_type> > unpacker_;

	volatile link_status status;
};

}} //namespace

#endif /* ST_ASIO_TCP_SOCKET_H_ */

/*
 * st_asio_wrapper_tcp_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef ST_ASIO_WRAPPER_TCP_SOCKET_H_
#define ST_ASIO_WRAPPER_TCP_SOCKET_H_

#include "st_asio_wrapper_socket.h"
#include "st_asio_wrapper_unpacker.h"

#ifndef GRACEFUL_CLOSE_MAX_DURATION
	#define GRACEFUL_CLOSE_MAX_DURATION	5 //seconds, maximum waiting seconds while graceful closing
#endif

#ifndef DEFAULT_UNPACKER
#define DEFAULT_UNPACKER unpacker
#endif

namespace st_asio_wrapper
{
namespace st_tcp
{

template <typename Socket, typename Packer, typename Unpacker>
class st_tcp_socket_base : public st_socket<Socket, Packer, Unpacker>
{
public:
	typedef typename Packer::msg_type in_msg_type;
	typedef typename Packer::msg_ctype in_msg_ctype;
	typedef typename Unpacker::msg_type out_msg_type;
	typedef typename Unpacker::msg_ctype out_msg_ctype;

protected:
	st_tcp_socket_base(boost::asio::io_service& io_service_) : st_socket<Socket, Packer, Unpacker>(io_service_), unpacker_(boost::make_shared<Unpacker>()) {reset_state(); close_state = 0;}

	template<typename Arg>
	st_tcp_socket_base(boost::asio::io_service& io_service_, Arg& arg) : st_socket<Socket, Packer, Unpacker>(io_service_, arg), unpacker_(boost::make_shared<Unpacker>()) {reset_state(); close_state = 0;}

public:
	virtual bool obsoleted() {return !is_closing() && st_socket<Socket, Packer, Unpacker>::obsoleted();}

	//reset all, be ensure that there's no any operations performed on this st_tcp_socket_base when invoke it
	void reset() {reset_state(); ST_THIS clear_buffer();}
	void reset_state()
	{
		unpacker_->reset_state();
		st_socket<Socket, Packer, Unpacker>::reset_state();
	}

	bool is_closing() const {return 0 != close_state;}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_unpacker<out_msg_type>> inner_unpacker() {return unpacker_;}
	boost::shared_ptr<const i_unpacker<out_msg_type>> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_unpacker<out_msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}

	using st_socket<Socket, Packer, Unpacker>::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket_base's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//like safe_send_msg and safe_send_native_msg, but non-block
	TCP_POST_MSG(post_msg, false)
	TCP_POST_MSG(post_native_msg, true)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	void force_close() {if (1 != close_state) clean_up();}
	bool graceful_close(bool sync = true) //will block until closing success or time out if sync equal to true
	{
		if (is_closing())
			return false;
		else
			close_state = 2;

		boost::system::error_code ec;
		ST_THIS lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		if (ec) //graceful closing is impossible
		{
			clean_up();
			return false;
		}

		if (sync)
		{
			auto loop_num = GRACEFUL_CLOSE_MAX_DURATION * 100; //seconds to 10 milliseconds
			while (--loop_num >= 0 && is_closing())
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(10));
			if (loop_num < 0) //graceful closing is impossible
			{
				unified_out::info_out("failed to graceful close within %d seconds", GRACEFUL_CLOSE_MAX_DURATION);
				clean_up();
			}
		}

		return !sync;
	}

	//must mutex send_msg_buffer before invoke this function
	virtual bool do_send_msg()
	{
		if (!is_send_allowed() || ST_THIS get_io_service().stopped())
			ST_THIS sending = false;
		else if (!ST_THIS sending && !ST_THIS send_msg_buffer.empty())
		{
			ST_THIS sending = true;
			ST_THIS last_send_msg.swap(ST_THIS send_msg_buffer.front());
			boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(ST_THIS last_send_msg.data(), ST_THIS last_send_msg.size()),
				boost::bind(&st_tcp_socket_base::send_handler, this,
#ifdef ENHANCED_STABILITY
					ST_THIS async_call_indicator,
#endif
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			ST_THIS send_msg_buffer.pop_front();
		}

		return ST_THIS sending;
	}

	virtual bool is_send_allowed() const {return !is_closing() && st_socket<Socket, Packer, Unpacker>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	//msg can not be unpacked
	//the link can continue to use, so don't need to close the st_tcp_socket_base at both client and server endpoint
	virtual void on_unpack_error() = 0;

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return true;}

	//start the asynchronous read
	//it's child's responsibility to invoke this properly, because st_tcp_socket_base doesn't know any of the connection status
	void do_recv_msg()
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		if (boost::asio::buffer_size(recv_buff) > 0)
			boost::asio::async_read(ST_THIS next_layer(), recv_buff,
				boost::bind(&i_unpacker<out_msg_type>::completion_condition, unpacker_, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred),
				boost::bind(&st_tcp_socket_base::recv_handler, this,
#ifdef ENHANCED_STABILITY
					ST_THIS async_call_indicator,
#endif
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	void clean_up()
	{
		close_state = 1;
		ST_THIS stop_all_timer();
		reset_state();

		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			ST_THIS lowest_layer().close(ec);
		}
	}

	void recv_handler(
#ifdef ENHANCED_STABILITY
		boost::shared_ptr<char> async_call_indicator,
#endif
		const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			auto unpack_ok = unpacker_->parse_msg(bytes_transferred, ST_THIS temp_msg_buffer);
			ST_THIS dispatch_msg();

			if (!unpack_ok)
			{
				on_unpack_error();
				//reset unpacker's state after on_unpack_error(), so user can get the left half-baked msg in on_unpack_error()
				unpacker_->reset_state();
			}
		}
		else
			ST_THIS on_recv_error(ec);
	}

	void send_handler(
#ifdef ENHANCED_STABILITY
		boost::shared_ptr<char> async_call_indicator,
#endif
		const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred > 0);
#ifdef WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(ST_THIS last_send_msg);
#endif
		}
		else
			ST_THIS on_send_error(ec);

		boost::unique_lock<boost::shared_mutex> lock(ST_THIS send_msg_buffer_mutex);
		ST_THIS sending = false;

		//send msg sequentially, that means second send only after first send success
		if (!ec)
#ifdef WANT_ALL_MSG_SEND_NOTIFY
			if (!do_send_msg())
				ST_THIS on_all_msg_send(ST_THIS last_send_msg);
#else
			do_send_msg();
#endif

		if (!ST_THIS sending)
			ST_THIS last_send_msg.clear();
	}

protected:
	boost::shared_ptr<i_unpacker<out_msg_type>> unpacker_;
	int close_state; //2-the first step of graceful close, 1-force close, 0-normal state
};

} //namespace st_tcp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_tcp; //compatible with old version which doesn't have st_tcp namespace.

#endif /* ST_ASIO_WRAPPER_TCP_SOCKET_H_ */

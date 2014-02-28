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
	#define GRACEFUL_CLOSE_MAX_DURATION	5 //seconds, max waiting seconds while graceful closing
#endif

#ifndef DEFAULT_UNPACKER
#define DEFAULT_UNPACKER unpacker
#endif

namespace st_asio_wrapper
{
namespace st_tcp
{

typedef std::string msg_type;
typedef const msg_type msg_ctype;

template <typename Socket>
class st_tcp_socket_base : public st_socket<msg_type, Socket>
{
protected:
	st_tcp_socket_base(boost::asio::io_service& io_service_) :
		st_socket<msg_type, Socket>(io_service_), unpacker_(boost::make_shared<DEFAULT_UNPACKER>()) {reset_state();}

	template<typename Arg>
	st_tcp_socket_base(boost::asio::io_service& io_service_, Arg& arg) :
		st_socket<msg_type, Socket>(io_service_, arg), unpacker_(boost::make_shared<DEFAULT_UNPACKER>()) {reset_state();}

public:
	//reset all, be ensure that there's no any operations performed on this st_tcp_socket_base when invoke it
	void reset() {ST_THIS reset_state(); ST_THIS clear_buffer();}
	void reset_state()
	{
		reset_unpacker_state();
		st_socket<msg_type, Socket>::reset_state();
		closing = false;
	}

	void disconnect() {force_close();}
	void force_close() {clean_up();}
	void graceful_close() //will block until closing success or timeout
	{
		closing = true;

		boost::system::error_code ec;
		ST_THIS lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		if (ec) //graceful disconnecting is impossible
			clean_up();
		else
		{
			auto loop_num = GRACEFUL_CLOSE_MAX_DURATION * 100; //seconds to 10 milliseconds
			while (--loop_num >= 0 && closing)
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(10));
			if (loop_num < 0) //graceful disconnecting is impossible
				clean_up();
		}
	}
	bool is_closing() const {return closing;}

	//get or change the unpacker at runtime
	boost::shared_ptr<i_unpacker> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_unpacker>& _unpacker_) {unpacker_ = _unpacker_;}

	using st_socket<msg_type, Socket>::send_msg;
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

	void show_info(const char* head, const char* tail)
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().remote_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

protected:
	//must mutex send_msg_buffer before invoke this function
	virtual bool do_send_msg()
	{
		if (!is_send_allowed() || ST_THIS get_io_service().stopped())
			ST_THIS sending = false;
		else if (!ST_THIS sending && !send_msg_buffer.empty())
		{
			ST_THIS sending = true;
			ST_THIS last_send_msg.swap(send_msg_buffer.front());
			boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(ST_THIS last_send_msg),
				boost::bind(&st_tcp_socket_base::send_handler, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			send_msg_buffer.pop_front();
		}

		return ST_THIS sending;
	}

	virtual bool is_send_allowed() const
		{return !is_closing() && st_socket<msg_type, Socket>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	//msg can not be unpacked
	//the link can continue to use, but need not close the st_tcp_socket_base at both client and server endpoint
	virtual void on_unpack_error() = 0;

	//recv error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const boost::system::error_code& ec) = 0;

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own recv buffer, you can move the msg to your own recv buffer,
	//and return false, then, handle the msg as your own strategy(may be you'll need a msg dispatch thread)
	//or, you can handle the msg at here and return false, but this will reduce efficiency(
	//because this msg handling block the next msg receiving on the same st_tcp_socket_base) unless you can
	//handle the msg very fast(which will inversely more efficient, because msg recv buffer and msg dispatching
	//are not needed any more).
	//
	//return true means use the msg recv buffer, you must handle the msgs in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return false;}
#endif

	//handling msg at here will not block msg receiving
	//if on_msg() return false, this function will not be invoked due to no msgs need to dispatch
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual void on_msg_handle(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data());}

	//start the async read
	//it's child's responsibility to invoke this properly,
	//because st_tcp_socket_base doesn't know any of the connection status
	void do_recv_msg()
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		if (boost::asio::buffer_size(recv_buff) > 0)
			boost::asio::async_read(ST_THIS next_layer(), recv_buff, boost::bind(&i_unpacker::completion_condition, unpacker_,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred),
				boost::bind(&st_tcp_socket_base::recv_handler, this,
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	//reset unpacker's state, generally used when unpack error occur
	void reset_unpacker_state() {unpacker_->reset_unpacker_state();}

	void clean_up()
	{
		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			ST_THIS lowest_layer().close(ec);
		}

		ST_THIS stop_all_timer();
		ST_THIS direct_dispatch_all_msg();
		reset_state();
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			auto unpack_ok = unpacker_->parse_msg(bytes_transferred, temp_msg_buffer);
			ST_THIS dispatch_msg();

			if (!unpack_ok)
				on_unpack_error();
		}
		else
			on_recv_error(ec);
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred > 0);
#ifdef WANT_MSG_SEND_NOTIFY
			on_msg_send(last_send_msg);
#endif
		}
		else
			ST_THIS on_send_error(ec);

		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		ST_THIS sending = false;

		//send msg sequentially, that means second send only after first send success
		if (!ec && !do_send_msg())
		{
#ifdef WANT_ALL_MSG_SEND_NOTIFY
			lock.unlock();
			on_all_msg_send(last_send_msg);
#endif
		}
	}

protected:
	boost::shared_ptr<i_unpacker> unpacker_;
	bool closing;
};

} //namespace st_tcp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_tcp; //compatible with old version which doesn't have st_tcp namespace.

#endif /* ST_ASIO_WRAPPER_TCP_SOCKET_H_ */

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

using namespace boost::asio::ip;

#ifndef GRACEFUL_CLOSE_MAX_DURATION
	#define GRACEFUL_CLOSE_MAX_DURATION	5 //seconds, max waiting seconds while graceful closing
#endif

namespace st_asio_wrapper
{
namespace st_tcp
{

typedef std::string msg_type;
typedef const msg_type msg_ctype;

class st_tcp_socket : public st_socket<msg_type, tcp::socket>
{
public:
	st_tcp_socket(io_service& io_service_) : st_socket(io_service_), unpacker_(boost::make_shared<unpacker>())
		{reset_state();}

	//reset all, be ensure that there's no any operations performed on this st_tcp_socket when invoke it
	void reset() {reset_state(); clear_buffer();}
	void reset_state()
	{
		reset_unpacker_state();
		st_socket::reset_state();
		closing = false;
	}

	void disconnect() {force_close();}
	void force_close() {clean_up();}
	void graceful_close() //will block until closing success or timeout
	{
		closing = true;

		error_code ec;
		shutdown(tcp::socket::shutdown_send, ec);
		if (ec) //graceful disconnecting is impossible
			clean_up();
		else
		{
			int loop_num = GRACEFUL_CLOSE_MAX_DURATION * 100; //seconds to 10 milliseconds
			while (--loop_num >= 0 && closing)
				this_thread::sleep(get_system_time() + posix_time::milliseconds(10));
			if (loop_num >= 0) //graceful disconnecting is impossible
				clean_up();
		}
	}
	bool is_closing() const {return closing;}

	//get or change the unpacker at runtime
	boost::shared_ptr<i_unpacker> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_unpacker>& _unpacker_) {unpacker_ = _unpacker_;}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	TCP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket's send buffer
	TCP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	TCP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//don't use the packer but insert into the send_msg_buffer directly
	bool direct_send_msg(msg_ctype& msg, bool can_overflow = false)
	{
		msg_type tmp_msg(msg);
		return direct_send_msg(tmp_msg, can_overflow);
	}

	//after this call, msg becomes empty, please note.
	bool direct_send_msg(msg_type& msg, bool can_overflow = false)
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM)
			return direct_insert_msg(msg);

		return false;
	}

	//send buffered msgs, return false if send buffer is empty or invalidate status
	bool send_msg()
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return do_send_msg();
	}

	void suspend_send_msg(bool suspend)
	{
		st_socket<msg_type, tcp::socket>::suspend_send_msg(suspend);
		if (!st_socket<msg_type, tcp::socket>::suspend_send_msg())
			send_msg();
	}

	void show_info(const char* head, const char* tail)
	{
		error_code ec;
		BOOST_AUTO(ep, remote_endpoint(ec));
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

protected:
	virtual bool is_send_allowed() const {return !is_closing() && st_socket<msg_type, tcp::socket>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	//msg can not be unpacked
	//the link can continue to use, but need not close the st_tcp_socket at both client and server endpoint
	virtual void on_unpack_error() = 0;

	//recv error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const error_code& ec) = 0;

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
	virtual bool on_msg(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return false;}
#endif

	//handling msg at here will not block msg receiving
	//if on_msg() return false, this function will not be invoked due to no msgs need to dispatch
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual void on_msg_handle(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data());}

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg(remain in packed)
	//if the msg is custom packed, then obviously you know it
	//or the msg is packed as: len(2 bytes) + original msg, see st_asio_wrapper::packer for more details
	virtual void on_msg_send(msg_type& msg) {}
#endif
#ifdef WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty, msg remain in packed
	virtual void on_all_msg_send(msg_type& msg) {}
#endif

	//start the async read
	//it's child's responsibility to invoke this properly,
	//because st_tcp_socket doesn't know any of the connection status
	void do_recv_msg()
	{
		size_t min_recv_len;
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv(min_recv_len));
		if (buffer_size(recv_buff) > 0)
			async_read(*this, recv_buff, transfer_at_least(min_recv_len),
				boost::bind(&st_tcp_socket::recv_handler, this, placeholders::error, placeholders::bytes_transferred));
	}

	//reset unpacker's state, generally used when unpack error occur
	void reset_unpacker_state() {unpacker_->reset_unpacker_state();}

	void clean_up()
	{
		if (is_open())
		{
			error_code ec;
			shutdown(tcp::socket::shutdown_both, ec);
			close(ec);
		}

		stop_all_timer();
		reset_state();
	}

	void recv_handler(const error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			bool unpack_ok = unpacker_->on_recv(bytes_transferred, temp_msg_buffer);
			dispatch_msg();

			if (!unpack_ok)
				on_unpack_error();
		}
		else
			on_recv_error(ec);
	}

	void send_handler(const error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred > 0);
#ifdef WANT_MSG_SEND_NOTIFY
			on_msg_send(last_send_msg);
#endif
		}
		else
			on_send_error(ec);

		mutex::scoped_lock lock(send_msg_buffer_mutex);
		sending = false;

		//send msg sequentially, that means second send only after first send success
		if (!ec && !do_send_msg())
		{
#ifdef WANT_ALL_MSG_SEND_NOTIFY
			lock.unlock();
			on_all_msg_send(last_send_msg);
#endif
		}
	}

	//must mutex send_msg_buffer before invoke this function
	bool do_send_msg()
	{
		if (!is_send_allowed() || get_io_service().stopped())
			sending = false;
		else if (!sending && !send_msg_buffer.empty())
		{
			sending = true;
			last_send_msg.swap(send_msg_buffer.front());
			async_write(*this, buffer(last_send_msg), boost::bind(&st_tcp_socket::send_handler, this,
				placeholders::error, placeholders::bytes_transferred));
			send_msg_buffer.pop_front();
		}

		return sending;
	}

	//must mutex send_msg_buffer before invoke this function
	bool direct_insert_msg(msg_type& msg)
	{
		if (!msg.empty())
		{
			send_msg_buffer.resize(send_msg_buffer.size() + 1);
			send_msg_buffer.back().swap(msg);
			do_send_msg();
		}

		return true;
	}

protected:
	boost::shared_ptr<i_unpacker> unpacker_;
	bool closing;
};

} //namespace st_tcp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_tcp;

#endif /* ST_ASIO_WRAPPER_TCP_SOCKET_H_ */

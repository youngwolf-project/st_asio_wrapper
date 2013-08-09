/*
 * st_asio_wrapper_udp_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef ST_ASIO_WRAPPER_UDP_SOCKET_H_
#define ST_ASIO_WRAPPER_UDP_SOCKET_H_

#include <boost/array.hpp>

#include "st_asio_wrapper_socket.h"

using namespace boost::asio::ip;

//in set_local_addr, if the ip is empty, UDP_DEFAULT_IP_VERSION will define the ip version,
//or, the ip version will be deduced by the ip address.
//udp::v4() means ipv4 and udp::v6() means ipv6.
#ifndef UDP_DEFAULT_IP_VERSION
#define UDP_DEFAULT_IP_VERSION udp::v4()
#endif

namespace st_asio_wrapper
{
namespace st_udp
{

struct udp_msg
{
	udp::endpoint peer_addr;
	std::string str;

	void swap(udp_msg& other) {std::swap(peer_addr, other.peer_addr); str.swap(other.str);}
	void swap(const udp::endpoint& addr, std::string& tmp_str) {peer_addr = addr; str.swap(tmp_str);}
	void clear() {peer_addr = udp::endpoint(); str.clear();}
	bool operator==(const udp_msg& other) const {return this == &other;}
};
typedef udp_msg msg_type;
typedef const msg_type msg_ctype;

class st_udp_socket : public st_socket<udp_msg, udp::socket>
{
public:
	st_udp_socket(io_service& io_service_) : st_socket(io_service_) {reset_state();}

	void set_local_addr(unsigned short port, const std::string& ip = std::string())
	{
		error_code ec;
		if (ip.empty())
			local_addr = udp::endpoint(UDP_DEFAULT_IP_VERSION, port);
		else
		{
			local_addr = udp::endpoint(address::from_string(ip, ec), port);
			assert(!ec);
		}
	}

	//reset all, be ensure that there's no any operations performed on this st_udp_socket when invoke it
	//notice, when resue this st_udp_socket, st_object_pool will invoke reset(), child must re-write this to init
	//all member variables, and then do not forget to invoke st_udp_socket::reset() to init father's
	//member variables
	virtual void reset()
	{
		reset_state();
		clear_buffer();

		error_code ec;
		close(ec);
		open(local_addr.protocol(), ec); assert(!ec);
#ifndef NOT_REUSE_ADDRESS
		set_option(socket_base::reuse_address(true), ec); assert(!ec);
#endif
		bind(local_addr, ec); assert(!ec);
		if (ec) {unified_out::error_out("bind failed.");}
	}

	virtual void start()
	{
		if (!get_io_service().stopped())
			async_receive_from(buffer(raw_buff), peer_addr,
				boost::bind(&st_udp_socket::recv_handler, this, placeholders::error, placeholders::bytes_transferred));
	}

	void disconnect() {force_close();}
	void force_close() {clean_up();}
	void graceful_close() {clean_up();}

	//udp does not need a unpacker

	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_udp_socket's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//don't use the packer but insert into the send_msg_buffer directly
	bool direct_send_msg(const udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false)
		{return direct_send_msg(peer_addr, std::string(str), can_overflow);}
	bool direct_send_msg(const udp::endpoint& peer_addr, std::string&& str, bool can_overflow = false)
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM)
			return direct_insert_msg(peer_addr, std::move(str));

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
		st_socket<msg_type, udp::socket>::suspend_send_msg(suspend);
		if (!st_socket<msg_type, udp::socket>::suspend_send_msg())
			send_msg();
	}

	void show_info(const char* head, const char* tail)
	{
		error_code ec;
		auto ep = local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

protected:
	virtual bool is_send_allowed() const {return is_open() && st_socket<msg_type, udp::socket>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	virtual void on_recv_error(const error_code& ec)
		{unified_out::error_out("recv msg error: %d %s", ec.value(), ec.message().data());}

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
	//notice: using inconstant is for the convenience of swapping
	virtual bool on_msg(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.str.size(), msg.str.data()); return false;}
#endif

	//handling msg at here will not block msg receiving
	//if on_msg() return false, this function will not be invoked due to no msgs need to dispatch
	//notice: using inconstant is for the convenience of swapping
	virtual void on_msg_handle(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.str.size(), msg.str.data());}

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer
	virtual void on_msg_send(msg_type& msg) {}
#endif
#ifdef WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty
	virtual void on_all_msg_send(msg_type& msg) {}
#endif

	void clean_up()
	{
		if (is_open())
		{
			error_code ec;
			shutdown(udp::socket::shutdown_both, ec);
			close(ec);
		}

		stop_all_timer();
		reset_state();
	}

	void recv_handler(const error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			std::string tmp_str(raw_buff.data(), bytes_transferred);
			temp_msg_buffer.resize(temp_msg_buffer.size() + 1);
			temp_msg_buffer.back().swap(peer_addr, tmp_str);
			dispatch_msg();
		}
#ifdef _MSC_VER
		else if (WSAECONNREFUSED == ec.value())
			start();
#endif
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
		//under windows, send a msg to addr_any may cause sending errors, please note
		//for udp in st_asio_wrapper, sending error will not stop the following sending.

		mutex::scoped_lock lock(send_msg_buffer_mutex);
		sending = false;

		//send msg sequentially, that means second send only after first send success
		if (!do_send_msg())
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
			async_send_to(buffer(last_send_msg.str), last_send_msg.peer_addr,
				boost::bind(&st_udp_socket::send_handler, this, placeholders::error, placeholders::bytes_transferred));
			send_msg_buffer.pop_front();
		}

		return sending;
	}

	//must mutex send_msg_buffer before invoke this function
	bool direct_insert_msg(const udp::endpoint& peer_addr, std::string&& str)
	{
		if (!str.empty())
		{
			send_msg_buffer.resize(send_msg_buffer.size() + 1);
			send_msg_buffer.back().swap(peer_addr, str);
			do_send_msg();
		}

		return true;
	}

protected:
	array<char, MAX_MSG_LEN> raw_buff;
	udp::endpoint peer_addr, local_addr;
};

} //namespace st_udp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_udp;

#endif /* ST_ASIO_WRAPPER_UDP_SOCKET_H_ */

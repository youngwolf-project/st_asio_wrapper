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

//in set_local_addr, if the ip is empty, UDP_DEFAULT_IP_VERSION will define the ip version,
//or, the ip version will be deduced by the ip address.
//boost::asio::ip::udp::v4() means ipv4 and boost::asio::ip::udp::v6() means ipv6.
#ifndef UDP_DEFAULT_IP_VERSION
#define UDP_DEFAULT_IP_VERSION boost::asio::ip::udp::v4()
#endif

namespace st_asio_wrapper
{
namespace st_udp
{

struct udp_msg
{
	boost::asio::ip::udp::endpoint peer_addr;
	std::string str;

	void swap(udp_msg& other) {std::swap(peer_addr, other.peer_addr); str.swap(other.str);}
	void swap(const boost::asio::ip::udp::endpoint& addr, std::string& tmp_str) {peer_addr = addr; str.swap(tmp_str);}
	void clear() {peer_addr = boost::asio::ip::udp::endpoint(); str.clear();}
	bool empty() const {return str.empty();}
	bool operator==(const udp_msg& other) const {return this == &other;}
};
typedef udp_msg msg_type;
typedef const msg_type msg_ctype;

template <typename Socket = boost::asio::ip::udp::socket>
class st_udp_socket_base : public st_socket<msg_type, Socket>
{
public:
	st_udp_socket_base(boost::asio::io_service& io_service_) :
		st_socket<msg_type, Socket>(io_service_) {ST_THIS reset_state();}

	//reset all, be ensure that there's no any operations performed on this st_udp_socket when invoke it
	//notice, when resue this st_udp_socket, st_object_pool will invoke reset(), child must re-write this to init
	//all member variables, and then do not forget to invoke st_udp_socket::reset() to init father's
	//member variables
	virtual void reset()
	{
		ST_THIS reset_state();
		ST_THIS clear_buffer();

		boost::system::error_code ec;
		ST_THIS lowest_layer().close(ec);
		ST_THIS lowest_layer().open(local_addr.protocol(), ec); assert(!ec);
#ifndef NOT_REUSE_ADDRESS
		ST_THIS lowest_layer().set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		ST_THIS lowest_layer().bind(local_addr, ec); assert(!ec);
		if (ec) {unified_out::error_out("bind failed.");}
	}

	void set_local_addr(unsigned short port, const std::string& ip = std::string())
	{
		boost::system::error_code ec;
		if (ip.empty())
			local_addr = boost::asio::ip::udp::endpoint(UDP_DEFAULT_IP_VERSION, port);
		else
		{
			local_addr = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(ip, ec), port);
			assert(!ec);
		}
	}
	const boost::asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}

	void disconnect() {force_close();}
	void force_close() {clean_up();}
	void graceful_close() {clean_up();}

	//udp does not need a unpacker

	using st_socket<msg_type, Socket>::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_udp_socket's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//like safe_send_msg and safe_send_native_msg, but non-block
	UDP_POST_MSG(post_msg, false)
	UDP_POST_MSG(post_native_msg, true)
	//msg sending interface
	///////////////////////////////////////////////////

	void show_info(const char* head, const char* tail)
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS get_io_service().stopped())
		{
			ST_THIS next_layer().async_receive_from(boost::asio::buffer(raw_buff), peer_addr,
				boost::bind(&st_udp_socket_base::recv_handler, this,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

			return true;
		}

		return false;
	}

	//must mutex send_msg_buffer before invoke this function
	virtual bool do_send_msg()
	{
		if (!is_send_allowed() || ST_THIS get_io_service().stopped())
			ST_THIS sending = false;
		else if (!ST_THIS sending && !send_msg_buffer.empty())
		{
			ST_THIS sending = true;
			ST_THIS last_send_msg.swap(send_msg_buffer.front());
			ST_THIS next_layer().async_send_to(boost::asio::buffer(ST_THIS last_send_msg.str),
				ST_THIS last_send_msg.peer_addr, boost::bind(&st_udp_socket_base::send_handler, this,
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			send_msg_buffer.pop_front();
		}

		return ST_THIS sending;
	}

	virtual bool is_send_allowed() const
		{return ST_THIS lowest_layer().is_open() && st_socket<msg_type, boost::asio::ip::udp::socket>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	virtual void on_recv_error(const boost::system::error_code& ec)
		{unified_out::error_out("recv msg error: %d %s", ec.value(), ec.message().data());}

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own recv buffer, you can move the msg to your own recv buffer,
	//and return false, then, handle the msg as your own strategy(may be you'll need a msg dispatch thread)
	//or, you can handle the msg at here and return false, but this will reduce efficiency(
	//because this msg handling block the next msg receiving on the same st_udp_socket) unless you can
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

	void clean_up()
	{
		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
			ST_THIS lowest_layer().close(ec);
		}

		ST_THIS stop_all_timer();
		ST_THIS direct_dispatch_all_msg();
		ST_THIS reset_state();
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			std::string tmp_str(raw_buff.data(), bytes_transferred);
			temp_msg_buffer.resize(temp_msg_buffer.size() + 1);
			temp_msg_buffer.back().swap(peer_addr, tmp_str);
			ST_THIS dispatch_msg();
		}
#ifdef _MSC_VER
		else if (WSAECONNREFUSED == ec.value())
			do_start();
#endif
		else
			on_recv_error(ec);
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred > 0);
#ifdef WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(last_send_msg);
#endif
		}
		else
			ST_THIS on_send_error(ec);

		boost::mutex::scoped_lock lock(send_msg_buffer_mutex);
		ST_THIS sending = false;

		//send msg sequentially, that means second send only after first send success
		//under windows, send a msg to addr_any may cause sending errors, please note
		//for udp in st_asio_wrapper, sending error will not stop the following sending.
		if (!do_send_msg())
		{
#ifdef WANT_ALL_MSG_SEND_NOTIFY
			lock.unlock();
			on_all_msg_send(last_send_msg);
#endif
		}
	}

protected:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	boost::asio::ip::udp::endpoint peer_addr, local_addr;
};
typedef st_udp_socket_base<> st_udp_socket;

} //namespace st_udp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_udp; //compatible with old version which doesn't have st_udp namespace.

#endif /* ST_ASIO_WRAPPER_UDP_SOCKET_H_ */

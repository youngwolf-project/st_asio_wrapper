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
#include "st_asio_wrapper_unpacker.h"

//in set_local_addr, if the IP is empty, UDP_DEFAULT_IP_VERSION will define the IP version,
//or, the IP version will be deduced by the IP address.
//boost::asio::ip::udp::v4() means ipv4 and boost::asio::ip::udp::v6() means ipv6.
#ifndef UDP_DEFAULT_IP_VERSION
#define UDP_DEFAULT_IP_VERSION boost::asio::ip::udp::v4()
#endif

#ifndef DEFAULT_UDP_UNPACKER
#define DEFAULT_UDP_UNPACKER udp_unpacker
#endif

namespace st_asio_wrapper
{
namespace st_udp
{

template <typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UDP_UNPACKER, typename Socket = boost::asio::ip::udp::socket>
class st_udp_socket_base : public st_socket<Socket, Packer, Unpacker, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>>
{
public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

public:
	st_udp_socket_base(boost::asio::io_service& io_service_) : st_socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type>(io_service_), unpacker_(boost::make_shared<Unpacker>())
		{ST_THIS reset_state();}

	//reset all, be ensure that there's no any operations performed on this st_udp_socket when invoke it
	//please note, when reuse this st_udp_socket, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_udp_socket::reset() to initialize father's
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
		if (ec)
			unified_out::error_out("bind failed.");
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			local_addr = boost::asio::ip::udp::endpoint(UDP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
			auto addr = boost::asio::ip::address::from_string(ip, ec);
			if (ec)
				return false;

			local_addr = boost::asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}
	const boost::asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}

	void disconnect() {force_close();}
	void force_close() {show_info("link:", "been closed."); clean_up();}
	void graceful_close() {force_close();}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_udp_unpacker<typename Packer::msg_type>> inner_unpacker() {return unpacker_;}
	boost::shared_ptr<const i_udp_unpacker<typename Packer::msg_type>> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_udp_unpacker<typename Packer::msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}

	using st_socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type>::send_msg;
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

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().c_str(), local_addr.port(), tail);}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS get_io_service().stopped())
		{
			ST_THIS next_layer().async_receive_from(unpacker_->prepare_next_recv(), peer_addr,
				boost::bind(&st_udp_socket_base::recv_handler, this,
#ifdef ENHANCED_STABILITY
					ST_THIS async_call_indicator,
#endif
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
		else if (!ST_THIS sending && !ST_THIS send_msg_buffer.empty())
		{
			ST_THIS sending = true;
			ST_THIS last_send_msg.swap(ST_THIS send_msg_buffer.front());
			ST_THIS next_layer().async_send_to(boost::asio::buffer(ST_THIS last_send_msg.data(), ST_THIS last_send_msg.size()), ST_THIS last_send_msg.peer_addr,
				boost::bind(&st_udp_socket_base::send_handler, this,
#ifdef ENHANCED_STABILITY
					ST_THIS async_call_indicator,
#endif
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			ST_THIS send_msg_buffer.pop_front();
		}

		return ST_THIS sending;
	}

	virtual bool is_send_allowed() const {return ST_THIS lowest_layer().is_open() && st_socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type>::is_send_allowed();}
	//can send data or not(just put into send buffer)

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return true;}

	void clean_up()
	{
		ST_THIS stop_all_timer();
		ST_THIS reset_state();

		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
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
			ST_THIS temp_msg_buffer.resize(ST_THIS temp_msg_buffer.size() + 1);
			ST_THIS temp_msg_buffer.back().swap(peer_addr, unpacker_->parse_msg(bytes_transferred));
			ST_THIS dispatch_msg();
		}
#ifdef _MSC_VER
		else if (boost::asio::error::connection_refused == ec || boost::asio::error::connection_reset == ec)
			do_start();
#endif
		else
			on_recv_error(ec);
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
		//under windows, send a msg to addr_any may cause sending errors, please note
		//for UDP in st_asio_wrapper, sending error will not stop the following sending.
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
	boost::shared_ptr<i_udp_unpacker<typename Packer::msg_type>> unpacker_;
	boost::asio::ip::udp::endpoint peer_addr, local_addr;
};
typedef st_udp_socket_base<> st_udp_socket;

} //namespace st_udp
} //namespace st_asio_wrapper

using namespace st_asio_wrapper::st_udp; //compatible with old version which doesn't have st_udp namespace.

#endif /* ST_ASIO_WRAPPER_UDP_SOCKET_H_ */

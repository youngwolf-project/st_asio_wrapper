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

#include "st_asio_wrapper_socket.h"
#include "st_asio_wrapper_container.h"

//in set_local_addr, if the IP is empty, ST_ASIO_UDP_DEFAULT_IP_VERSION will define the IP version,
//or, the IP version will be deduced by the IP address.
//boost::asio::ip::udp::v4() means ipv4 and boost::asio::ip::udp::v6() means ipv6.
#ifndef ST_ASIO_UDP_DEFAULT_IP_VERSION
#define ST_ASIO_UDP_DEFAULT_IP_VERSION boost::asio::ip::udp::v4()
#endif

namespace st_asio_wrapper
{

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ip::udp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_udp_socket_base : public st_socket<Socket, Packer, Unpacker, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>, InQueue, InContainer, OutQueue, OutContainer>
{
protected:
	typedef st_socket<Socket, Packer, Unpacker, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_udp_socket_base(boost::asio::io_service& io_service_) : super(io_service_), unpacker_(boost::make_shared<Unpacker>()) {}

	//reset all, be ensure that there's no any operations performed on this st_udp_socket when invoke it
	//please note, when reuse this st_udp_socket, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_udp_socket::reset() to initialize father's
	//member variables
	virtual void reset()
	{
		reset_state();
		super::reset();

		boost::system::error_code ec;
		ST_THIS lowest_layer().open(local_addr.protocol(), ec); assert(!ec);
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
		ST_THIS lowest_layer().set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		ST_THIS lowest_layer().bind(local_addr, ec); assert(!ec);
		if (ec)
			unified_out::error_out("bind failed.");
	}

	void reset_state()
	{
		unpacker_->reset_state();
		super::reset_state();
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			local_addr = boost::asio::ip::udp::endpoint(ST_ASIO_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
			if (ec)
				return false;

			local_addr = boost::asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}
	const boost::asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shut down."); shutdown();}
	void graceful_shutdown() {force_shutdown();}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type> > inner_unpacker() {return unpacker_;}
	boost::shared_ptr<const i_udp_unpacker<typename Unpacker::msg_type> > inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type> >& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
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

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().data(), local_addr.port(), tail);}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS stopped())
		{
			do_recv_msg();
			return true;
		}

		return false;
	}

	//ascs::socket will guarantee not call this function in more than one thread concurrently.
	//return false if send buffer is empty or sending not allowed or io_service stopped
	virtual bool do_send_msg()
	{
		if (!ST_THIS send_msg_buffer.empty() && is_send_allowed() && ST_THIS send_msg_buffer.try_dequeue(last_send_msg))
		{
			ST_THIS stat.send_delay_sum += statistic::local_time() - last_send_msg.begin_time;

			last_send_msg.restart();
			boost::shared_lock<boost::shared_mutex> lock(shutdown_mutex);
			ST_THIS next_layer().async_send_to(boost::asio::buffer(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr,
				ST_THIS make_handler_error_size(boost::bind(&st_udp_socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));

			return true;
		}

		return false;
	}

	virtual void do_recv_msg()
	{
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);

		boost::shared_lock<boost::shared_mutex> lock(shutdown_mutex);
		ST_THIS next_layer().async_receive_from(recv_buff, peer_addr,
			ST_THIS make_handler_error_size(boost::bind(&st_udp_socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	virtual bool is_send_allowed() {return ST_THIS lowest_layer().is_open() && super::is_send_allowed();}
	//can send data or not(just put into send buffer)

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}

	void shutdown()
	{
		boost::unique_lock<boost::shared_mutex> lock(shutdown_mutex);

		ST_THIS stop_all_timer();
		ST_THIS close();

		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
			ST_THIS lowest_layer().close(ec);
		}
	}

private:
	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			++ST_THIS stat.recv_msg_sum;
			ST_THIS stat.recv_byte_sum += bytes_transferred;
			ST_THIS temp_msg_buffer.emplace_back();
			ST_THIS temp_msg_buffer.back().swap(peer_addr);
			unpacker_->parse_msg(ST_THIS temp_msg_buffer.back(), bytes_transferred);
			ST_THIS handle_msg();
		}
#ifdef _MSC_VER
		else if (boost::asio::error::connection_refused == ec || boost::asio::error::connection_reset == ec)
			do_start();
#endif
		else
			on_recv_error(ec);
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred == last_send_msg.size());

			ST_THIS stat.send_time_sum += statistic::local_time() - last_send_msg.begin_time;
			ST_THIS stat.send_byte_sum += bytes_transferred;
			++ST_THIS stat.send_msg_sum;
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(last_send_msg);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
			if (ST_THIS send_msg_buffer.empty())
				ST_THIS on_all_msg_send(last_send_msg);
#endif
		}
		else
			ST_THIS on_send_error(ec);
		last_send_msg.clear();

		//send msg sequentially, which means second sending only after first sending success
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequence sendings.
		if (!do_send_msg())
		{
			ST_THIS sending = false;
			if (!ST_THIS send_msg_buffer.empty())
				ST_THIS send_msg(); //just make sure no pending msgs
		}
	}

protected:
	typename super::in_msg last_send_msg;
	boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type> > unpacker_;
	boost::asio::ip::udp::endpoint peer_addr, local_addr;

	boost::shared_mutex shutdown_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UDP_SOCKET_H_ */

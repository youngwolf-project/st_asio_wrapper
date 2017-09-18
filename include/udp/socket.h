/*
 * socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP socket
 */

#ifndef ST_ASIO_UDP_SOCKET_H_
#define ST_ASIO_UDP_SOCKET_H_

#include "../socket.h"
#include "../container.h"

namespace st_asio_wrapper { namespace udp {

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ip::udp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class socket_base : public socket<Socket, Packer, Unpacker, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

private:
	typedef socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	socket_base(boost::asio::io_context& io_context_) : super(io_context_), unpacker_(boost::make_shared<Unpacker>()) {}

	virtual bool is_ready() {return ST_THIS lowest_layer().is_open();}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr);
		ST_THIS packer_->pack_heartbeat(msg);
		ST_THIS do_direct_send_msg(msg);
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset()
	{
		boost::system::error_code ec;
		if (!ST_THIS lowest_layer().is_open()) {ST_THIS lowest_layer().open(local_addr.protocol(), ec); assert(!ec);} //user maybe has opened this socket (to set options for example)
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
		ST_THIS lowest_layer().set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		ST_THIS lowest_layer().bind(local_addr, ec); assert(!ec);
		if (ec)
			unified_out::error_out("bind failed.");

		last_send_msg.clear();
		unpacker_->reset();
		super::reset();
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	const boost::asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}
	bool set_peer_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(peer_addr, port, ip);}
	const boost::asio::ip::udp::endpoint& get_peer_addr() const {return peer_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shut down."); shutdown();}
	void graceful_shutdown() {force_shutdown();}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> > unpacker() {return unpacker_;}
	boost::shared_ptr<const i_unpacker<typename Unpacker::msg_type> > unpacker() const {return unpacker_;}
	void unpacker(const boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> >& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::socket_base's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)
	//send message with sync mode
	//return 0 means empty message or this socket is busy on sending messages
	//return -1 means error occurred, otherwise the number of bytes been sent
	UDP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	size_t direct_sync_send_msg(typename Packer::msg_ctype& msg) {return direct_sync_send_msg(peer_addr, msg);}
	size_t direct_sync_send_msg(const boost::asio::ip::udp::endpoint& peer_addr, typename Packer::msg_ctype& msg)
	{
		if (msg.empty())
			unified_out::error_out("empty message, will not send it.");
		else if (ST_THIS lock_sending_flag())
			return do_sync_send_msg(peer_addr, msg);

		return 0;
	}
	//msg sending interface
	///////////////////////////////////////////////////

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().data(), local_addr.port(), tail);}

protected:
	//send message with sync mode
	//return -1 means error occurred, otherwise the number of bytes been sent
	size_t do_sync_send_msg(typename Packer::msg_ctype& msg) {return do_sync_send_msg(peer_addr, msg);}
	size_t do_sync_send_msg(const boost::asio::ip::udp::endpoint& peer_addr, typename Packer::msg_ctype& msg)
	{
		boost::system::error_code ec;
		auto_duration dur(ST_THIS stat.send_time_sum);
		size_t send_size = ST_THIS next_layer().send_to(ST_ASIO_SEND_BUFFER_TYPE(msg.data(), msg.size()), peer_addr, 0, ec);
		dur.end();

		send_handler(ec, send_size);
		return ec ? -1 : send_size;
	}

	//return false if send buffer is empty
	virtual bool do_send_msg()
	{
		if (ST_THIS send_msg_buffer.try_dequeue(last_send_msg))
		{
			ST_THIS stat.send_delay_sum += statistic::local_time() - last_send_msg.begin_time;

			last_send_msg.restart();
			boost::lock_guard<boost::mutex> lock(shutdown_mutex);
			ST_THIS next_layer().async_send_to(ST_ASIO_SEND_BUFFER_TYPE(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr,
				ST_THIS make_handler_error_size(boost::bind(&socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));

			return true;
		}

		return false;
	}

	virtual bool do_send_msg(in_msg_type& msg)
	{
		last_send_msg = msg;
		ST_THIS next_layer().async_send_to(ST_ASIO_SEND_BUFFER_TYPE(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr,
				ST_THIS make_handler_error_size(boost::bind(&socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));

		return true;
	}

	virtual void do_recv_msg()
	{
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);

		boost::lock_guard<boost::mutex> lock(shutdown_mutex);
		ST_THIS next_layer().async_receive_from(recv_buff, temp_addr,
			ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

	virtual bool on_heartbeat_error()
	{
		ST_THIS stat.last_recv_time = time(NULL); //avoid repetitive warnings
		unified_out::warning_out("%s:%hu is not available", peer_addr.address().to_string().data(), peer_addr.port());
		return true;
	}

#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}
#endif

	virtual bool on_msg_handle(out_msg_type& msg) {unified_out::debug_out("recv(" ST_ASIO_SF "): %s", msg.size(), msg.data()); return true;}

	void shutdown()
	{
		boost::lock_guard<boost::mutex> lock(shutdown_mutex);

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
			ST_THIS stat.last_recv_time = time(NULL);

			out_msg_type msg(temp_addr);
			unpacker_->parse_msg(msg, bytes_transferred);
			if (!msg.empty())
			{
				++ST_THIS stat.recv_msg_sum;
				ST_THIS stat.recv_byte_sum += msg.size();
				ST_THIS temp_msg_buffer.emplace_back(boost::ref(msg));
			}
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
			ST_THIS stat.last_send_time = time(NULL);

			ST_THIS stat.send_byte_sum += bytes_transferred;
			++ST_THIS stat.send_msg_sum;
			if (!last_send_msg.empty())
			{
				assert(bytes_transferred == last_send_msg.size());

				ST_THIS stat.send_time_sum += statistic::local_time() - last_send_msg.begin_time;
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
				ST_THIS on_msg_send(last_send_msg);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
				if (ST_THIS send_msg_buffer.empty())
					ST_THIS on_all_msg_send(last_send_msg);
#endif
			}
		}
		else
			ST_THIS on_send_error(ec);
		last_send_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sendings.
		if (!do_send_msg())
		{
			ST_THIS sending = false;
			if (!ST_THIS send_msg_buffer.empty())
				ST_THIS send_msg(); //just make sure no pending msgs
		}
	}

	bool set_addr(boost::asio::ip::udp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = boost::asio::ip::udp::endpoint(ST_ASIO_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
			BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec));
#else
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
#endif
			if (ec)
				return false;

			endpoint = boost::asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}

protected:
	typename super::in_msg last_send_msg;
	boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> > unpacker_;
	boost::asio::ip::udp::endpoint local_addr;
	boost::asio::ip::udp::endpoint temp_addr; //used when receiving messages
	boost::asio::ip::udp::endpoint peer_addr;

	boost::mutex shutdown_mutex;
};

}} //namespace

#endif /* ST_ASIO_UDP_SOCKET_H_ */

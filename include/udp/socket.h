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
class socket_base : public socket<Socket, Packer, udp_msg<typename Packer::msg_type>, udp_msg<typename Unpacker::msg_type>, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

private:
	typedef socket<Socket, Packer, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	socket_base(boost::asio::io_context& io_context_) : super(io_context_), unpacker_(boost::make_shared<Unpacker>()), has_bound(false), strand(io_context_) {}

	virtual bool is_ready() {return has_bound;}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr);
		packer_->pack_heartbeat(msg);
		do_direct_send_msg(msg);
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset()
	{
		has_bound = false;

		boost::system::error_code ec;
		if (!ST_THIS lowest_layer().is_open()) {ST_THIS lowest_layer().open(local_addr.protocol(), ec); assert(!ec);} //user maybe has opened this socket (to set options for example)
		if (ST_THIS lowest_layer().is_open())
		{
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
			ST_THIS lowest_layer().set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
			ST_THIS lowest_layer().bind(local_addr, ec); assert(!ec);
			if (!(has_bound = !ec))
				unified_out::error_out("bind failed.");
		}

		last_send_msg.clear();
		unpacker_->reset();
		super::reset();
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	const boost::asio::ip::udp::endpoint& get_local_addr() const {return local_addr;}
	bool set_peer_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(peer_addr, port, ip);}
	const boost::asio::ip::udp::endpoint& get_peer_addr() const {return peer_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shutting down."); ST_THIS dispatch_strand(strand, boost::bind(&socket_base::shutdown, this));}
	void graceful_shutdown() {force_shutdown();}

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().data(), local_addr.port(), tail);}

	void show_status() const
	{
		unified_out::info_out(
			"\n\tid: " ST_ASIO_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ST_ASIO_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\trecv suspended: %d",
			ST_THIS id(), ST_THIS started(), ST_THIS is_sending(),
#ifdef ST_ASIO_PASSIVE_RECV
			ST_THIS is_reading(),
#endif
			ST_THIS is_dispatching(), ST_THIS is_recv_idle());
	}

	//get or change the unpacker at runtime
	//changing unpacker at runtime is not thread-safe, this operation can only be done in on_msg(), reset() or constructor, please pay special attention
	//we can resolve this defect via mutex, but i think it's not worth, because this feature is not frequently used
	boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> > unpacker() {return unpacker_;}
	boost::shared_ptr<const i_unpacker<typename Unpacker::msg_type> > unpacker() const {return unpacker_;}
#ifdef ST_ASIO_PASSIVE_RECV
	//changing unpacker must before calling st_asio_wrapper::socket::recv_msg, and define ST_ASIO_PASSIVE_RECV macro.
	void unpacker(const boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> >& _unpacker_) {unpacker_ = _unpacker_;}
	virtual void recv_msg() {if (!reading && is_ready()) ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_recv_msg, this));}
#endif

	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false, do_direct_send_msg) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true, do_direct_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::socket_base's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ST_ASIO_SYNC_SEND
	UDP_SYNC_SEND_MSG(sync_send_msg, false, do_direct_sync_send_msg) //use the packer with native = false to pack the msgs
	UDP_SYNC_SEND_MSG(sync_send_native_msg, true, do_direct_sync_send_msg) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into tcp::socket_base's send buffer
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	virtual bool do_start() {return has_bound ? super::do_start() : false;}

	//msg was failed to send and udp::socket_base will not hold it any more, if you want to re-send it in the future,
	// you must take over it and re-send (at any time) it via direct_send_msg.
	//DO NOT hold msg for future using, just swap its content with your own message in this virtual function.
	virtual void on_send_error(const boost::system::error_code& ec, typename super::in_msg& msg)
		{unified_out::error_out("send msg error (%d %s)", ec.value(), ec.message().data());}

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

	virtual bool on_heartbeat_error()
	{
		stat.last_recv_time = time(NULL); //avoid repetitive warnings
		unified_out::warning_out("%s:%hu is not available", peer_addr.address().to_string().data(), peer_addr.port());
		return true;
	}

#ifdef ST_ASIO_SYNC_SEND
	virtual void on_close() {if (last_send_msg.p) last_send_msg.p->set_value(NOT_APPLICABLE); super::on_close();}
#endif

private:
#ifndef ST_ASIO_PASSIVE_RECV
	virtual void recv_msg() {ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_recv_msg, this));}
#endif
	virtual void send_msg() {ST_THIS dispatch_strand(strand, boost::bind(&socket_base::do_send_msg, this, false));}

	using super::close;
	using super::handle_msg;
	using super::do_direct_send_msg;
#ifdef ST_ASIO_SYNC_SEND
	using super::do_direct_sync_send_msg;
#endif

	void shutdown()
	{
		ST_THIS stop_all_timer();
		close();

		if (ST_THIS lowest_layer().is_open())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
			ST_THIS lowest_layer().close(ec);
		}
	}

	void do_recv_msg()
	{
#ifdef ST_ASIO_PASSIVE_RECV
		if (reading)
			return;
#endif
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);
		if (0 == boost::asio::buffer_size(recv_buff))
			unified_out::error_out("The unpacker returned an empty buffer, quit receiving!");
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = true;
#endif
			ST_THIS next_layer().async_receive_from(recv_buff, temp_addr, make_strand_handler(strand,
				ST_THIS make_handler_error_size(boost::bind(&socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
		}
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			stat.last_recv_time = time(NULL);

			typename Unpacker::container_type msg_can;
			unpacker_->parse_msg(bytes_transferred, msg_can);

#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
				temp_msg_can.emplace_back(temp_addr, boost::ref(*iter));
			if (handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)
			if (ec && boost::asio::error::connection_refused != ec && boost::asio::error::connection_reset != ec)
#else
			if (ec)
#endif
				on_recv_error(ec);
			else if (handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
	}

	bool do_send_msg(bool in_strand)
	{
		if (!in_strand && sending)
			return true;

		if ((sending = send_msg_buffer.try_dequeue(last_send_msg)))
		{
			stat.send_delay_sum += statistic::now() - last_send_msg.begin_time;

			last_send_msg.restart();
			ST_THIS next_layer().async_send_to(boost::asio::buffer(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr, make_strand_handler(strand,
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
			stat.send_time_sum += statistic::now() - last_send_msg.begin_time;
			++stat.send_msg_sum;
#ifdef ST_ASIO_SYNC_SEND
			if (last_send_msg.p)
				last_send_msg.p->set_value(SUCCESS);
#endif
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(last_send_msg);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
			if (send_msg_buffer.empty())
				ST_THIS on_all_msg_send(last_send_msg);
#endif
		}
		else
			on_send_error(ec, last_send_msg);
		last_send_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		if (ec && (boost::asio::error::not_socket == ec || boost::asio::error::bad_descriptor == ec))
			return;

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sending.
		if (!do_send_msg(true) && !send_msg_buffer.empty())
			do_send_msg(true); //just make sure no pending msgs
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

private:
	using super::stat;
	using super::packer_;
	using super::temp_msg_can;

	using super::send_msg_buffer;
	using super::sending;

#ifdef ST_ASIO_PASSIVE_RECV
	using super::reading;
#endif

	bool has_bound;
	typename super::in_msg last_send_msg;
	boost::shared_ptr<i_unpacker<typename Unpacker::msg_type> > unpacker_;
	boost::asio::ip::udp::endpoint local_addr;
	boost::asio::ip::udp::endpoint temp_addr; //used when receiving messages
	boost::asio::ip::udp::endpoint peer_addr;

	boost::asio::io_context::strand strand;
};

}} //namespace

#endif /* ST_ASIO_UDP_SOCKET_H_ */

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
public:
	typedef udp_msg<typename Packer::msg_type> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

private:
	typedef st_socket<Socket, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	st_udp_socket_base(boost::asio::io_service& io_service_) : super(io_service_), unpacker_(boost::make_shared<Unpacker>()) {}

	virtual bool is_ready() {return this->lowest_layer().is_open();}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr, this->packer_->pack_heartbeat());
		this->do_direct_send_msg(std::move(msg));
	}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, st_object_pool will invoke this function
	virtual void reset()
	{
		boost::system::error_code ec;
		if (!this->lowest_layer().is_open()) {this->lowest_layer().open(local_addr.protocol(), ec); assert(!ec);} //user maybe has opened this socket (to set options for example)
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
		this->lowest_layer().set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		this->lowest_layer().bind(local_addr, ec); assert(!ec);
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
	boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type>> unpacker() {return unpacker_;}
	boost::shared_ptr<const i_udp_unpacker<typename Unpacker::msg_type>> unpacker() const {return unpacker_;}
	void unpacker(const boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type>>& _unpacker_) {unpacker_ = _unpacker_;}

	using super::send_msg;
	///////////////////////////////////////////////////
	//msg sending interface
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_udp_socket's send buffer
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
		else if (!this->sending && !this->stopped() && is_ready())
		{
			scope_atomic_lock<> lock(this->send_atomic);
			if (!this->sending && lock.locked())
			{
				this->sending = true;
				lock.unlock();

				return do_sync_send_msg(peer_addr, msg);
			}
		}

		return 0;
	}
	//msg sending interface
	///////////////////////////////////////////////////

	void show_info(const char* head, const char* tail) const {unified_out::info_out("%s %s:%hu %s", head, local_addr.address().to_string().data(), local_addr.port(), tail);}

protected:
	virtual bool do_start()
	{
		this->last_recv_time = time(nullptr);
#if ST_ASIO_HEARTBEAT_INTERVAL > 0
		this->start_heartbeat(ST_ASIO_HEARTBEAT_INTERVAL);
#endif
		do_recv_msg();

		return true;
	}

	//send message with sync mode
	//return -1 means error occurred, otherwise the number of bytes been sent
	size_t do_sync_send_msg(typename Packer::msg_ctype& msg) {return do_sync_send_msg(peer_addr, msg);}
	size_t do_sync_send_msg(const boost::asio::ip::udp::endpoint& peer_addr, typename Packer::msg_ctype& msg)
	{
		boost::system::error_code ec;
		auto_duration dur(this->stat.send_time_sum);
		auto send_size = this->next_layer().send_to(boost::asio::buffer(msg.data(), msg.size()), peer_addr, 0, ec);
		dur.end();

		send_handler(ec, send_size);
		return ec ? -1 : send_size;
	}

	//return false if send buffer is empty
	virtual bool do_send_msg()
	{
		if (this->send_msg_buffer.try_dequeue(last_send_msg))
		{
			this->stat.send_delay_sum += statistic::local_time() - last_send_msg.begin_time;

			last_send_msg.restart();
			boost::lock_guard<boost::mutex> lock(shutdown_mutex);
			this->next_layer().async_send_to(boost::asio::buffer(last_send_msg.data(), last_send_msg.size()), last_send_msg.peer_addr,
				this->make_handler_error_size([this](const boost::system::error_code& ec, size_t bytes_transferred) {this->send_handler(ec, bytes_transferred);}));

			return true;
		}

		return false;
	}

	virtual void do_recv_msg()
	{
		auto recv_buff = unpacker_->prepare_next_recv();
		assert(boost::asio::buffer_size(recv_buff) > 0);

		boost::lock_guard<boost::mutex> lock(shutdown_mutex);
		this->next_layer().async_receive_from(recv_buff, temp_addr,
			this->make_handler_error_size([this](const boost::system::error_code& ec, size_t bytes_transferred) {this->recv_handler(ec, bytes_transferred);}));
	}

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out("recv msg error (%d %s)", ec.value(), ec.message().data());
	}

	virtual bool on_heartbeat_error()
	{
		this->last_recv_time = time(nullptr); //avoid repetitive warnings
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

		this->stop_all_timer();
		this->close();

		if (this->lowest_layer().is_open())
		{
			boost::system::error_code ec;
			this->lowest_layer().shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
			this->lowest_layer().close(ec);
		}
	}

private:
	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			this->last_recv_time = time(nullptr);

			auto msg = this->unpacker_->parse_msg(bytes_transferred);
			if (!msg.empty())
			{
				++this->stat.recv_msg_sum;
				this->stat.recv_byte_sum += msg.size();
				this->temp_msg_buffer.emplace_back();
				this->temp_msg_buffer.back().swap(temp_addr, std::move(msg));
			}
			this->handle_msg();
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
			this->last_send_time = time(nullptr);

			this->stat.send_byte_sum += bytes_transferred;
			++this->stat.send_msg_sum;
			if (!last_send_msg.empty())
			{
				assert(bytes_transferred == last_send_msg.size());

				this->stat.send_time_sum += statistic::local_time() - last_send_msg.begin_time;
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
				this->on_msg_send(last_send_msg);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
				if (this->send_msg_buffer.empty())
					this->on_all_msg_send(last_send_msg);
#endif
			}
		}
		else
			this->on_send_error(ec);
		last_send_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sendings.
		if (!do_send_msg())
		{
			this->sending = false;
			if (!this->send_msg_buffer.empty())
				this->send_msg(); //just make sure no pending msgs
		}
	}

	bool set_addr(boost::asio::ip::udp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = boost::asio::ip::udp::endpoint(ST_ASIO_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
			auto addr = boost::asio::ip::address::from_string(ip, ec);
			if (ec)
				return false;

			endpoint = boost::asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}

protected:
	typename super::in_msg last_send_msg;
	boost::shared_ptr<i_udp_unpacker<typename Unpacker::msg_type>> unpacker_;
	boost::asio::ip::udp::endpoint local_addr;
	boost::asio::ip::udp::endpoint temp_addr; //used when receiving messages
	boost::asio::ip::udp::endpoint peer_addr;

	boost::mutex shutdown_mutex;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_UDP_SOCKET_H_ */

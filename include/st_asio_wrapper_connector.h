/*
 * st_asio_wrapper_connector.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_WRAPPER_CONNECTOR_H_
#define ST_ASIO_WRAPPER_CONNECTOR_H_

#include "st_asio_wrapper_tcp_socket.h"
#include "st_asio_wrapper_container.h"

#ifndef ST_ASIO_SERVER_IP
#define ST_ASIO_SERVER_IP			"127.0.0.1"
#endif
#ifndef ST_ASIO_SERVER_PORT
#define ST_ASIO_SERVER_PORT			5050
#endif
static_assert(ST_ASIO_SERVER_PORT > 0, "server port must be bigger than zero.");
#ifndef ST_ASIO_RECONNECT_INTERVAL
#define ST_ASIO_RECONNECT_INTERVAL	500 //millisecond(s), negative number means don't reconnect the server
#endif

namespace st_asio_wrapper
{

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_connector_base : public st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const st_timer::tid TIMER_BEGIN = super::TIMER_END;
	static const st_timer::tid TIMER_CONNECT = TIMER_BEGIN;
	static const st_timer::tid TIMER_END = TIMER_BEGIN + 10;

	st_connector_base(boost::asio::io_service& io_service_) : super(io_service_), need_reconnect(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}
	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) : super(io_service_, arg), need_reconnect(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, st_object_pool will invoke this function
	virtual void reset() {need_reconnect = true; super::reset();}
	virtual bool obsoleted() {return !need_reconnect && super::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		boost::system::error_code ec;
		auto addr = boost::asio::ip::address::from_string(ip, ec);
		if (ec)
			return false;

		server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	//if the connection is broken unexpectedly, st_connector_base will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (super::link_status::FORCE_SHUTTING_DOWN != this->status)
			show_info("client link:", "been shut down.");

		need_reconnect = reconnect;
		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (this->is_broken())
			return force_shutdown(reconnect);
		else if (!this->is_shutting_down())
			show_info("client link:", "being shut down gracefully.");

		need_reconnect = reconnect;
		super::graceful_shutdown(sync);
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		auto ep = this->lowest_layer().local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().data(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		auto ep = this->lowest_layer().local_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().data(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!this->is_connected())
			this->lowest_layer().async_connect(server_addr, this->make_handler_error([this](const boost::system::error_code& ec) {this->connect_handler(ec);}));
		else
		{
			this->last_recv_time = time(nullptr);
#if ST_ASIO_HEARTBEAT_INTERVAL > 0
			this->start_heartbeat(ST_ASIO_HEARTBEAT_INTERVAL);
#endif
			this->send_msg(); //send buffer may have msgs, send them
			this->do_recv_msg();
		}

		return true;
	}

	//after how much time(ms), st_connector_base will try to reconnect to the server, negative means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		show_info("client link:", "broken/been shut down", ec);

		force_shutdown(this->is_shutting_down() ? need_reconnect : prepare_reconnect(ec) >= 0);
		this->status = super::link_status::BROKEN;
	}

	virtual void on_async_shutdown_error() {force_shutdown(need_reconnect);}
	virtual bool on_heartbeat_error()
	{
		show_info("client link:", "broke unexpectedly.");
		force_shutdown(this->is_shutting_down() ? need_reconnect : prepare_reconnect(boost::system::error_code(boost::asio::error::network_down)) >= 0);
		return false;
	}

	//reconnect at here rather than in on_recv_error to make sure that there's no any async invocations performed on this socket before reconnectiong
	virtual void on_close()
	{
		if (!need_reconnect)
			super::on_close();
		else
		{
			this->stop_all_timer();
			this->start();
		}
	}

	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if ((boost::asio::error::operation_aborted != ec || need_reconnect) && !this->stopped())
		{
#ifdef _WIN32
			if (boost::asio::error::connection_refused != ec && boost::asio::error::network_unreachable != ec && boost::asio::error::timed_out != ec)
#endif
			{
				boost::system::error_code ec;
				this->lowest_layer().close(ec);
			}

			auto delay = prepare_reconnect(ec);
			if (delay >= 0)
			{
				this->set_timer(TIMER_CONNECT, delay, [this](st_timer::tid id)->bool {this->do_start(); return false;});
				return true;
			}
		}

		return false;
	}

private:
	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			this->status = super::link_status::CONNECTED;
			on_connect();
			do_start();
		}
		else
			prepare_next_reconnect(ec);
	}

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool need_reconnect;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

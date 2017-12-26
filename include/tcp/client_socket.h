/*
 * client_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_CLIENT_SOCKET_H_
#define ST_ASIO_CLIENT_SOCKET_H_

#include "socket.h"
#include "../container.h"

namespace st_asio_wrapper { namespace tcp {

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const timer::tid TIMER_BEGIN = super::TIMER_END;
	static const timer::tid TIMER_CONNECT = TIMER_BEGIN;
	static const timer::tid TIMER_END = TIMER_BEGIN + 10;

	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_), need_reconnect(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}
	template<typename Arg>
	client_socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg), need_reconnect(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset() {need_reconnect = true; super::reset();}
	virtual bool obsoleted() {return !need_reconnect && super::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
		BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec));
#else
		BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
#endif
		if (ec)
			return false;

		server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	//if the connection is broken unexpectedly, client_socket_base will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (super::FORCE_SHUTTING_DOWN != ST_THIS status)
			ST_THIS show_info("client link:", "been shut down.");

		need_reconnect = reconnect;
		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (ST_THIS is_broken())
			return force_shutdown(reconnect);
		else if (!ST_THIS is_shutting_down())
			ST_THIS show_info("client link:", "being shut down gracefully.");

		need_reconnect = reconnect;
		super::graceful_shutdown(sync);
	}

protected:
	virtual bool do_start() //connect
	{
		assert(!ST_THIS is_connected());

		ST_THIS lowest_layer().async_connect(server_addr, ST_THIS make_handler_error(boost::bind(&client_socket_base::connect_handler, this, boost::asio::placeholders::error)));
		return true;
	}

	virtual void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec) //already started, so cannot call start()
			super::do_start();
		else
			prepare_next_reconnect(ec);
	}

	//after how much time(ms), client_socket_base will try to reconnect to the server, negative value means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		ST_THIS show_info("client link:", "broken/been shut down", ec);

		force_shutdown(ST_THIS is_shutting_down() ? need_reconnect : prepare_reconnect(ec) >= 0);
		ST_THIS status = super::BROKEN;
	}

	virtual void on_async_shutdown_error() {force_shutdown(need_reconnect);}
	virtual bool on_heartbeat_error()
	{
		ST_THIS show_info("client link:", "broke unexpectedly.");
		force_shutdown(ST_THIS is_shutting_down() ? need_reconnect : prepare_reconnect(boost::system::error_code(boost::asio::error::network_down)) >= 0);
		return false;
	}

	//reconnect at here rather than in on_recv_error to make sure that there's no any async invocations performed on this socket before reconnectiong
	virtual void on_close() {if (need_reconnect) ST_THIS start(); else super::on_close();}

	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if (ST_THIS started() && (boost::asio::error::operation_aborted != ec || need_reconnect) && !ST_THIS stopped())
		{
#ifdef _WIN32
			if (boost::asio::error::connection_refused != ec && boost::asio::error::network_unreachable != ec && boost::asio::error::timed_out != ec)
#endif
			{
				boost::system::error_code ec;
				ST_THIS lowest_layer().close(ec);
			}

			int delay = prepare_reconnect(ec);
			if (delay >= 0)
			{
				ST_THIS set_timer(TIMER_CONNECT, delay, (boost::lambda::bind(&client_socket_base::do_start, this), false));
				return true;
			}
		}

		return false;
	}

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool need_reconnect;
};

}} //namespace

#endif /* ST_ASIO_CLIENT_SOCKET_H_ */

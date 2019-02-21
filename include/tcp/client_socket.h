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

namespace st_asio_wrapper { namespace tcp {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_CONNECT = TIMER_BEGIN;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {first_init();}
	template<typename Arg> client_socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {first_init();}

	client_socket_base(Matrix& matrix_) : super(matrix_.get_service_pump()) {first_init(&matrix_);}
	template<typename Arg> client_socket_base(Matrix& matrix_, Arg& arg) : super(matrix_.get_service_pump(), arg) {first_init(&matrix_);}

	virtual const char* type_name() const {return "TCP (client endpoint)";}
	virtual int type_id() const {return 1;}

	//reset all, be ensure that no operations performed on this socket when invoke it, subclass must rewrite this function to initialize itself, and then
	// call superclass' reset function, before reusing this socket, object_pool will invoke this function
	virtual void reset() {need_reconnect = ST_ASIO_RECONNECT; super::reset();}

	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP) {return set_addr(server_addr, port, ip);}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}
	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	const boost::asio::ip::tcp::endpoint& get_local_addr() const {return local_addr;}

	//if you don't want to reconnect to the server after link broken, define macro ST_ASIO_RECONNECT as false, call close_reconnect() in on_connect()
	// or rewrite after_close() virtual function and do nothing in it.
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	//disconnect(bool), force_shutdown(bool) and graceful_shutdown(bool, bool) can overwrite reconnecting behavior, please note.
	//reset() virtual function will set reconnecting behavior according to macro ST_ASIO_RECONNECT, please note.
	void open_reconnect() {need_reconnect = true;}
	void close_reconnect() {need_reconnect = false;}

	//if the connection is broken unexpectedly, client_socket_base will try to reconnect to the server automatically (if need_reconnect is true).
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		need_reconnect = reconnect;

		if (!ST_THIS started() && reconnect)
			return ST_THIS start();
		else if (super::FORCE_SHUTTING_DOWN != ST_THIS status)
			ST_THIS show_info("client link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		need_reconnect = reconnect;

		if (!ST_THIS started() && reconnect)
			return ST_THIS start();
		else if (ST_THIS is_broken())
			return force_shutdown(reconnect);
		else if (!ST_THIS is_shutting_down())
			ST_THIS show_info("client link:", "being shut down gracefully.");

		super::graceful_shutdown(sync);
	}

protected:
	//helper function, just call it in constructor
	void first_init(Matrix* matrix_ = NULL) {need_reconnect = ST_ASIO_RECONNECT; matrix = matrix_; set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	Matrix* get_matrix() {return matrix;}
	const Matrix* get_matrix() const {return matrix;}

	virtual bool do_start() //connect
	{
		assert(!ST_THIS is_connected());

		BOOST_AUTO(&lowest_object, ST_THIS lowest_layer());
		if (0 != local_addr.port() || !local_addr.address().is_unspecified())
		{
			boost::system::error_code ec;
			if (!lowest_object.is_open()) //user maybe has opened this socket (to set options for example)
			{
				lowest_object.open(local_addr.protocol(), ec); assert(!ec);
				if (ec)
				{
					unified_out::error_out("cannot create socket: %s", ec.message().data());
					return false;
				}
			}

			lowest_object.bind(local_addr, ec);
			if (ec && boost::asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		lowest_object.async_connect(server_addr, ST_THIS make_handler_error(boost::bind(&client_socket_base::connect_handler, this, boost::asio::placeholders::error)));
		return true;
	}

	virtual void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec) //already started, so cannot call start()
			super::do_start();
		else
			prepare_next_reconnect(ec);
	}

	//after how much time (ms), client_socket_base will try to reconnect the server, negative value means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		ST_THIS show_info("client link:", "broken/been shut down", ec);

		force_shutdown(need_reconnect);
		ST_THIS status = super::BROKEN;
	}

	virtual void on_async_shutdown_error() {force_shutdown(need_reconnect);}
	virtual bool on_heartbeat_error()
	{
		ST_THIS show_info("client link:", "broke unexpectedly.");

		force_shutdown(need_reconnect);
		return false;
	}

	//reconnect at here rather than in on_recv_error to make sure no async invocations performed on this socket before reconnecting.
	//if you don't want to reconnect the server after link broken, rewrite this virtual function and do nothing in it or call close_reconnt().
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	virtual void after_close() {if (need_reconnect) ST_THIS start();}

private:
	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if (need_reconnect && ST_THIS started() && !ST_THIS stopped())
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

		super::force_shutdown();
		return false;
	}

	bool set_addr(boost::asio::ip::tcp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = boost::asio::ip::tcp::endpoint(ST_ASIO_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
			BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec)); assert(!ec);
#else
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec)); assert(!ec);
#endif
			if (ec)
			{
				unified_out::error_out("invalid IP address %s.", ip.data());
				return false;
			}

			endpoint = boost::asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}

private:
	bool need_reconnect;
	boost::asio::ip::tcp::endpoint server_addr;
	boost::asio::ip::tcp::endpoint local_addr;

	Matrix* matrix;
};

}} //namespace

#endif /* ST_ASIO_CLIENT_SOCKET_H_ */

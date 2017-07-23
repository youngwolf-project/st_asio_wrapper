/*
 * ssl.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support boost::asio::ssl
 */

#ifndef ST_ASIO_SSL_H_
#define ST_ASIO_SSL_H_

#include <boost/asio/ssl.hpp>

#include "../../object_pool.h"
#include "../client_socket.h"
#include "../client.h"
#include "../server_socket.h"
#include "../server.h"

namespace st_asio_wrapper { namespace ssl {

template <typename Socket>
class socket : public Socket
{
#if defined(ST_ASIO_REUSE_OBJECT) && !defined(ST_ASIO_REUSE_SSL_STREAM)
	#error please define ST_ASIO_REUSE_SSL_STREAM macro explicitly if you need boost::asio::ssl::stream to be reusable!
#endif

public:
	template<typename Arg>
	socket(Arg& arg, boost::asio::ssl::context& ctx) : Socket(arg, ctx), authorized_(false) {}

	virtual bool is_ready() {return authorized_ && Socket::is_ready();}
	virtual void reset() {authorized_ = false;}
	bool authorized() const {return authorized_;}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (is_ready())
		{
			authorized_ = false;
#ifndef ST_ASIO_REUSE_SSL_STREAM
			ST_THIS status = Socket::GRACEFUL_SHUTTING_DOWN;
			ST_THIS show_info("ssl link:", "been shut down.");
			boost::system::error_code ec;
			ST_THIS next_layer().shutdown(ec);

			if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown will get error eof.
				unified_out::info_out("shutdown ssl link failed (maybe intentionally because of reusing)");
#endif
		}

		Socket::on_recv_error(ec);
	}

	virtual void on_handshake(const boost::system::error_code& ec)
	{
		if (!ec)
			unified_out::info_out("handshake success.");
		else
			unified_out::error_out("handshake failed: %s", ec.message().data());
	}

	void handle_handshake(const boost::system::error_code& ec) {on_handshake(ec); if (!ec) {authorized_ = true; Socket::do_start();}}

	void shutdown_ssl(bool sync = true)
	{
		if (!is_ready())
		{
			Socket::force_shutdown();
			return;
		}

		authorized_ = false;
		ST_THIS status = Socket::GRACEFUL_SHUTTING_DOWN;

		if (!sync)
		{
			ST_THIS show_info("ssl link:", "been shutting down.");
			ST_THIS next_layer().async_shutdown(ST_THIS make_handler_error(boost::bind(&socket::async_shutdown_handler, this, boost::asio::placeholders::error)));
		}
		else
		{
			ST_THIS show_info("ssl link:", "been shut down.");
			boost::system::error_code ec;
			ST_THIS next_layer().shutdown(ec);

			if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown will get error eof.
				unified_out::info_out("shutdown ssl link failed (maybe intentionally because of reusing)");
		}
	}

private:
	void async_shutdown_handler(const boost::system::error_code& ec)
	{
		if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown will get error eof.
			unified_out::info_out("async shutdown ssl link failed (maybe intentionally because of reusing)");
	}

protected:
	volatile bool authorized_;
};

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public socket<tcp::client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket<tcp::client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> > super;

public:
	client_socket_base(boost::asio::io_service& io_service_, boost::asio::ssl::context& ctx) : super(io_service_, ctx) {}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
#ifdef ST_ASIO_REUSE_SSL_STREAM
	void force_shutdown(bool reconnect = false) {ST_THIS authorized_ = false; super::force_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false, bool sync = true) {ST_THIS authorized_ = false; super::graceful_shutdown(reconnect, sync);}
#else
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
			unified_out::error_out("reconnecting mechanism is not available, please define macro ST_ASIO_REUSE_SSL_STREAM");

		ST_THIS need_reconnect = false; //ignore reconnect parameter
		ST_THIS shutdown_ssl(sync);
	}
#endif

protected:
	virtual bool do_start() //add handshake
	{
		if (!ST_THIS is_connected())
			super::do_start();
		else if (!ST_THIS authorized())
			ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::client,
				ST_THIS make_handler_error(boost::bind(&client_socket_base::handle_handshake, this, boost::asio::placeholders::error)));

		return true;
	}

#ifndef ST_ASIO_REUSE_SSL_STREAM
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return -1;}
	virtual void on_recv_error(const boost::system::error_code& ec) {ST_THIS need_reconnect = false; super::on_recv_error(ec);}
#endif
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec) {super::handle_handshake(ec); if (ec) force_shutdown();}
};

template<typename Object>
class object_pool : public st_asio_wrapper::object_pool<Object>
{
private:
	typedef st_asio_wrapper::object_pool<Object> super;

public:
	object_pool(service_pump& service_pump_, boost::asio::ssl::context::method m) : super(service_pump_), ctx(m) {}
	boost::asio::ssl::context& context() {return ctx;}

	typename object_pool::object_type create_object() {return create_object(boost::ref(ST_THIS sp));}
	template<typename Arg>
	typename object_pool::object_type create_object(Arg& arg) {return super::create_object(arg, boost::ref(ctx));}

protected:
	boost::asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class server_socket_base : public socket<tcp::server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket<tcp::server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> > super;

public:
	server_socket_base(Server& server_, boost::asio::ssl::context& ctx) : super(server_, ctx) {}

#ifdef ST_ASIO_REUSE_SSL_STREAM
	void disconnect() {force_shutdown();}
	void force_shutdown() {ST_THIS authorized_ = false; super::force_shutdown();}
	void graceful_shutdown(bool sync = false) {ST_THIS authorized_ = false; super::graceful_shutdown(sync);}
#else
	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown(bool sync = false) {ST_THIS shutdown_ssl(sync);}
#endif

protected:
	virtual bool do_start() //add handshake
	{
		if (!ST_THIS authorized())
			ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::server,
				ST_THIS make_handler_error(boost::bind(&server_socket_base::handle_handshake, this, boost::asio::placeholders::error)));

		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec) {super::handle_handshake(ec); if (ec) this->server.del_socket(ST_THIS shared_from_this());}
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = tcp::i_server> class server_base : public tcp::server_base<Socket, Pool, Server>
{
public:
	server_base(service_pump& service_pump_, boost::asio::ssl::context::method m) : tcp::server_base<Socket, Pool, Server>(service_pump_, m) {}
};
template<typename Socket> class single_client_base : public tcp::single_client_base<Socket>
{
public:
	single_client_base(service_pump& service_pump_, boost::asio::ssl::context& ctx) : tcp::single_client_base<Socket>(service_pump_, ctx) {}
};
template<typename Socket, typename Pool = object_pool<Socket> > class multi_client_base : public tcp::multi_client_base<Socket, Pool>
{
public:
	multi_client_base(service_pump& service_pump_, boost::asio::ssl::context::method m) : tcp::multi_client_base<Socket, Pool>(service_pump_, m) {}
};

}} //namespace

#endif /* ST_ASIO_SSL_H_ */

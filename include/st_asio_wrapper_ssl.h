/*
 * st_asio_wrapper_ssl.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support boost::asio::ssl
 */

#ifndef ST_ASIO_WRAPPER_SSL_H_
#define ST_ASIO_WRAPPER_SSL_H_

#include <boost/asio/ssl.hpp>

#include "st_asio_wrapper_object_pool.h"
#include "st_asio_wrapper_connector.h"
#include "st_asio_wrapper_tcp_client.h"
#include "st_asio_wrapper_server_socket.h"
#include "st_asio_wrapper_server.h"

namespace st_asio_wrapper
{

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
			this->status = Socket::link_status::GRACEFUL_SHUTTING_DOWN;
			this->show_info("ssl link:", "been shut down.");
			boost::system::error_code ec;
			this->next_layer().shutdown(ec);

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
		this->status = Socket::link_status::GRACEFUL_SHUTTING_DOWN;

		if (!sync)
		{
			this->show_info("ssl link:", "been shutting down.");
			this->next_layer().async_shutdown(this->make_handler_error([this](const boost::system::error_code& ec) {
				if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown will get error eof.
					unified_out::info_out("async shutdown ssl link failed (maybe intentionally because of reusing)");
			}));
		}
		else
		{
			this->show_info("ssl link:", "been shut down.");
			boost::system::error_code ec;
			this->next_layer().shutdown(ec);

			if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown will get error eof.
				unified_out::info_out("shutdown ssl link failed (maybe intentionally because of reusing)");
		}
	}

protected:
	volatile bool authorized_;
};

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_ssl_connector_base : public socket<st_connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<st_connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	st_ssl_connector_base(boost::asio::io_service& io_service_, boost::asio::ssl::context& ctx) : super(io_service_, ctx) {}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (reconnect)
		{
			this->authorized_ = false;
			super::force_shutdown(true);
		}
		else
			graceful_shutdown();
	}
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
		{
			this->authorized_ = false;
			super::graceful_shutdown(true, sync);
		}
		else
		{
			this->need_reconnect = false;
			this->shutdown_ssl(sync);
		}
	}

protected:
	virtual bool do_start() //add handshake
	{
		if (!this->is_connected())
			super::do_start();
		else if (!this->authorized())
			this->next_layer().async_handshake(boost::asio::ssl::stream_base::client, this->make_handler_error([this](const boost::system::error_code& ec) {this->handle_handshake(ec);}));

		return true;
	}

#ifndef ST_ASIO_REUSE_SSL_STREAM
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return -1;}
	virtual void on_recv_error(const boost::system::error_code& ec) {this->need_reconnect = false; super::on_recv_error(ec);}
#endif
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec) {super::handle_handshake(ec); if (ec) force_shutdown();}
};

template<typename Object>
class st_ssl_object_pool : public st_object_pool<Object>
{
private:
	typedef st_object_pool<Object> super;

public:
	st_ssl_object_pool(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : super(service_pump_), ctx(m) {}
	boost::asio::ssl::context& context() {return ctx;}

	using super::create_object;
	typename st_ssl_object_pool::object_type create_object() {return create_object(this->sp, ctx);}
	template<typename Arg>
	typename st_ssl_object_pool::object_type create_object(Arg& arg) {return create_object(arg, ctx);}

protected:
	boost::asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_ssl_server_socket_base : public socket<st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	st_ssl_server_socket_base(Server& server_, boost::asio::ssl::context& ctx) : super(server_, ctx) {}

#ifdef ST_ASIO_REUSE_SSL_STREAM
	void disconnect() {force_shutdown();}
	void force_shutdown() {this->authorized_ = false; super::force_shutdown();}
	void graceful_shutdown(bool sync = false) {this->authorized_ = false; super::graceful_shutdown(sync);}
#else
	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown(bool sync = false) {this->shutdown_ssl(sync);}
#endif

protected:
	virtual bool do_start() //add handshake
	{
		if (!this->authorized())
			this->next_layer().async_handshake(boost::asio::ssl::stream_base::server, this->make_handler_error([this](const boost::system::error_code& ec) {this->handle_handshake(ec);}));

		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec) {super::handle_handshake(ec); if (ec) this->server.del_socket(this->shared_from_this());}
};

#ifdef ST_ASIO_HAS_TEMPLATE_USING
template<typename Socket, typename Pool = st_ssl_object_pool<Socket>, typename Server = i_server>
using st_ssl_server_base = st_server_base<Socket, Pool, Server>;
template<typename Socket> using st_ssl_tcp_sclient_base = st_tcp_sclient_base<Socket>;
template<typename Socket, typename Pool = st_ssl_object_pool<Socket>> using st_ssl_tcp_client_base = st_tcp_client_base<Socket, Pool>;
#else
template<typename Socket, typename Pool = st_ssl_object_pool<Socket>, typename Server = i_server>
class st_ssl_server_base : public st_server_base<Socket, Pool, Server>
{
public:
	st_ssl_server_base(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : st_server_base<Socket, Pool, Server>(service_pump_, m) {}
};
template<typename Socket> class st_ssl_tcp_sclient_base : public st_tcp_sclient_base<Socket>
{
public:
	st_ssl_tcp_sclient_base(st_service_pump& service_pump_, boost::asio::ssl::context& ctx) : st_tcp_sclient_base<Socket>(service_pump_, ctx) {}
};
template<typename Socket, typename Pool = st_ssl_object_pool<Socket>> class st_ssl_tcp_client_base : public st_tcp_client_base<Socket, Pool>
{
public:
	st_ssl_tcp_client_base(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : st_tcp_client_base<Socket, Pool>(service_pump_, m) {}
};
#endif

} //namespace

#endif /* ST_ASIO_WRAPPER_SSL_H_ */

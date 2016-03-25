/*
 * st_asio_wrapper_ssl_object.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support asio::ssl
 */

#ifndef ST_ASIO_WRAPPER_SSL_H_
#define ST_ASIO_WRAPPER_SSL_H_

#include <boost/asio/ssl.hpp>

#include "st_asio_wrapper_object_pool.h"
#include "st_asio_wrapper_tcp_client.h"
#include "st_asio_wrapper_server.h"

#ifdef REUSE_OBJECT
	#error boost::asio::ssl::stream not support reuse!
#endif

namespace st_asio_wrapper
{

template <typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UNPACKER, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
class st_ssl_connector_base : public st_connector_base<Packer, Unpacker, Socket>
{
public:
	st_ssl_connector_base(boost::asio::io_service& io_service_, boost::asio::ssl::context& ctx) : st_connector_base<Packer, Unpacker, Socket>(io_service_, ctx), authorized_(false) {}

	virtual void reset() {authorized_ = false; st_connector_base<Packer, Unpacker, Socket>::reset();}
	bool authorized() const {return authorized_;}

	void disconnect(bool reconnect = false) {force_close(reconnect);}
	void force_close(bool reconnect = false)
	{
		if (reconnect)
			unified_out::error_out("boost::asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			st_connector_base<Packer, Unpacker, Socket>::force_close(false);
	}

	void graceful_close(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
			unified_out::error_out("boost::asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			st_connector_base<Packer, Unpacker, Socket>::graceful_close(false, sync);
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS get_io_service().stopped())
		{
			if (ST_THIS reconnecting && !ST_THIS is_connected())
				ST_THIS lowest_layer().async_connect(ST_THIS server_addr, boost::bind(&st_ssl_connector_base::connect_handler, this, boost::asio::placeholders::error));
			else if (!authorized_)
				ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::client, boost::bind(&st_ssl_connector_base::handshake_handler, this, boost::asio::placeholders::error));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {authorized_ = false; st_connector_base<Packer, Unpacker, Socket>::on_unpack_error();}
	virtual void on_recv_error(const boost::system::error_code& ec) {authorized_ = false; st_connector_base<Packer, Unpacker, Socket>::on_recv_error(ec);}
	virtual void on_handshake(bool result)
	{
		if (result)
			unified_out::info_out("handshake success.");
		else
		{
			unified_out::error_out("handshake failed!");
			ST_THIS force_close(false);
		}
	}
	virtual bool is_send_allowed() const {return authorized() && st_connector_base<Packer, Unpacker, Socket>::is_send_allowed();}

	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			ST_THIS connected = true;
			ST_THIS reconnecting = false;
			ST_THIS on_connect();
			do_start();
		}
		else
			st_connector_base<Packer, Unpacker, Socket>::connect_handler(ec);
	}

	void handshake_handler(const boost::system::error_code& ec)
	{
		on_handshake(!ec);
		if (ec)
			unified_out::error_out("handshake failed: %s", ec.message().data());
		else
		{
			authorized_ = true;
			ST_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
	}

	bool shutdown_ssl()
	{
		bool re = false;
		if (!ST_THIS is_closing() && authorized_)
		{
			ST_THIS show_info("ssl client link:", "been shutting down.");
			ST_THIS close_state = 2;
			ST_THIS reconnecting = false;
			authorized_ = false;

			boost::system::error_code ec;
			ST_THIS next_layer().shutdown(ec);

			re = !ec;
		}

		return re;
	}

protected:
	bool authorized_;
};
typedef st_ssl_connector_base<> st_ssl_connector;
typedef st_sclient<st_ssl_connector> st_ssl_tcp_sclient;

template<typename Object>
class st_ssl_object_pool : public st_object_pool<Object>
{
public:
	st_ssl_object_pool(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : st_object_pool<Object>(service_pump_), ctx(m) {}
	boost::asio::ssl::context& ssl_context() {return ctx;}

	typename st_ssl_object_pool::object_type create_object()
	{
		auto object_ptr = ST_THIS reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(ST_THIS service_pump, ctx);
		if (object_ptr)
			object_ptr->id(++ST_THIS cur_id);

		return object_ptr;
	}
	template<typename Arg>
	typename st_ssl_object_pool::object_type create_object(Arg& arg)
	{
		auto object_ptr = ST_THIS reuse_object();
		if (!object_ptr)
			object_ptr = boost::make_shared<Object>(arg, ctx);
		if (object_ptr)
			object_ptr->id(++ST_THIS cur_id);

		return object_ptr;
	}

protected:
	boost::asio::ssl::context ctx;
};
typedef st_tcp_client_base<st_ssl_connector, st_ssl_object_pool<st_ssl_connector>> st_ssl_tcp_client;

template<typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UNPACKER, typename Server = i_server, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
class st_ssl_server_socket_base : public st_server_socket_base<Packer, Unpacker, Server, Socket>
{
public:
	st_ssl_server_socket_base(Server& server_, boost::asio::ssl::context& ctx) : st_server_socket_base<Packer, Unpacker, Server, Socket>(server_, ctx) {}
};
typedef st_ssl_server_socket_base<> st_ssl_server_socket;

template<typename Socket = st_ssl_server_socket, typename Pool = st_ssl_object_pool<Socket>, typename Server = i_server>
class st_ssl_server_base : public st_server_base<Socket, Pool, Server>
{
public:
	st_ssl_server_base(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : st_server_base<Socket, Pool, Server>(service_pump_, m) {}

protected:
	virtual void on_handshake(bool result, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		if (result)
			client_ptr->show_info("handshake with", "success.");
		else
			client_ptr->show_info("handshake with", "failed!");
	}

	virtual void start_next_accept()
	{
		auto client_ptr = ST_THIS create_object(boost::ref(*this));
		ST_THIS acceptor.async_accept(client_ptr->lowest_layer(), boost::bind(&st_ssl_server_base::accept_handler, this, boost::asio::placeholders::error, client_ptr));
	}

protected:
	void accept_handler(const boost::system::error_code& ec, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		if (!ec)
		{
			if (ST_THIS on_accept(client_ptr))
				client_ptr->next_layer().async_handshake(boost::asio::ssl::stream_base::server,
					boost::bind(&st_ssl_server_base::handshake_handler, this, boost::asio::placeholders::error, client_ptr));

			start_next_accept();
		}
		else
			ST_THIS stop_listen();
	}

	void handshake_handler(const boost::system::error_code& ec, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		on_handshake(!ec, client_ptr);
		if (ec)
			unified_out::error_out("handshake failed: %s", ec.message().data());
		else if (ST_THIS add_client(client_ptr))
		{
			client_ptr->start();
			return;
		}
		else
			client_ptr->show_info("client:", "been refused cause of too many clients.");

		client_ptr->force_close();
	}
};
typedef st_ssl_server_base<> st_ssl_server;

} //namespace

#endif /* ST_ASIO_WRAPPER_SSL_H_ */

/*
 * st_asio_wrapper_ssl.h
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
#include "st_asio_wrapper_connector.h"
#include "st_asio_wrapper_tcp_client.h"
#include "st_asio_wrapper_server_socket.h"
#include "st_asio_wrapper_server.h"
#include "st_asio_wrapper_container.h"

#ifdef ST_ASIO_REUSE_OBJECT
	#error boost::asio::ssl::stream not support reuse!
#endif

namespace st_asio_wrapper
{

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_ssl_connector_base : public st_connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
protected:
	typedef st_connector_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_ssl_connector_base(boost::asio::io_service& io_service_, boost::asio::ssl::context& ctx) : super(io_service_, ctx), authorized_(false) {}

	virtual void reset() {authorized_ = false; super::reset();}
	bool authorized() const {return authorized_;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (reconnect)
			unified_out::error_out("boost::asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			super::force_shutdown(false);
	}

	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (reconnect)
			unified_out::error_out("boost::asio::ssl::stream not support reuse!");

		if (!shutdown_ssl())
			super::graceful_shutdown(false, sync);
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS stopped())
		{
			if (ST_THIS reconnecting && !ST_THIS is_connected())
				ST_THIS lowest_layer().async_connect(ST_THIS server_addr, ST_THIS make_handler_error([this](const boost::system::error_code& ec) {ST_THIS connect_handler(ec);}));
			else if (!authorized_)
				ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::client,
					ST_THIS make_handler_error([this](const boost::system::error_code& ec) {ST_THIS handshake_handler(ec);}));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {authorized_ = false; super::on_unpack_error();}
	virtual void on_recv_error(const boost::system::error_code& ec) {authorized_ = false; super::on_recv_error(ec);}
	virtual void on_handshake(const boost::system::error_code& ec)
	{
		if (!ec)
			unified_out::info_out("handshake success.");
		else
			unified_out::error_out("handshake failed: %s", ec.message().data());
	}
	virtual bool is_send_allowed() {return authorized() && super::is_send_allowed();}

	bool shutdown_ssl()
	{
		bool re = false;
		if (!ST_THIS is_shutting_down() && authorized_)
		{
			ST_THIS show_info("ssl client link:", "been shut down.");
			ST_THIS shutdown_state = super::GRACEFUL;
			ST_THIS reconnecting = false;
			authorized_ = false;

			boost::system::error_code ec;
			ST_THIS next_layer().shutdown(ec);

			re = !ec;
		}

		return re;
	}

private:
	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			ST_THIS connected = ST_THIS reconnecting = false;
			ST_THIS reset_state();
			ST_THIS on_connect();
			do_start();
		}
		else
			ST_THIS prepare_next_reconnect(ec);
	}

	void handshake_handler(const boost::system::error_code& ec)
	{
		on_handshake(ec);
		if (!ec)
		{
			authorized_ = true;
			ST_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
		else
			force_shutdown(false);
	}

protected:
	bool authorized_;
};

template<typename Object>
class st_ssl_object_pool : public st_object_pool<Object>
{
protected:
	typedef st_object_pool<Object> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_ssl_object_pool(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : super(service_pump_), ctx(m) {}
	boost::asio::ssl::context& ssl_context() {return ctx;}

	using super::create_object;
	typename st_ssl_object_pool::object_type create_object() {return create_object(ST_THIS sp, ctx);}
	template<typename Arg>
	typename st_ssl_object_pool::object_type create_object(Arg& arg) {return create_object(arg, ctx);}

protected:
	boost::asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
#if !defined(_MSC_VER) || _MSC_VER >= 1800
using st_ssl_server_socket_base = st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>;
#else
class st_ssl_server_socket_base : public st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
protected:
	typedef st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_ssl_server_socket_base(Server& server_, boost::asio::ssl::context& ctx) : super(server_, ctx) {}
};
#endif

template<typename Socket, typename Pool = st_ssl_object_pool<Socket>, typename Server = i_server>
class st_ssl_server_base : public st_server_base<Socket, Pool, Server>
{
protected:
	typedef st_server_base<Socket, Pool, Server> super;

public:
	using super::TIMER_BEGIN;
	using super::TIMER_END;

	st_ssl_server_base(st_service_pump& service_pump_, boost::asio::ssl::context::method m) : super(service_pump_, m) {}

protected:
	virtual void on_handshake(const boost::system::error_code& ec, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		if (!ec)
			client_ptr->show_info("handshake with", "success.");
		else
		{
			std::string error_info = "failed: ";
			error_info += ec.message().data();
			client_ptr->show_info("handshake with", error_info.data());
		}
	}

	virtual void start_next_accept()
	{
		auto client_ptr = ST_THIS create_object(*this);
		ST_THIS acceptor.async_accept(client_ptr->lowest_layer(), [client_ptr, this](const boost::system::error_code& ec) {ST_THIS accept_handler(ec, client_ptr);});
	}

private:
	void accept_handler(const boost::system::error_code& ec, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		if (!ec)
		{
			if (ST_THIS on_accept(client_ptr))
				client_ptr->next_layer().async_handshake(boost::asio::ssl::stream_base::server,
					[client_ptr, this](const boost::system::error_code& ec) {ST_THIS handshake_handler(ec, client_ptr);});

			start_next_accept();
		}
		else
			ST_THIS stop_listen();
	}

	void handshake_handler(const boost::system::error_code& ec, typename st_ssl_server_base::object_ctype& client_ptr)
	{
		on_handshake(ec, client_ptr);
		if (!ec && ST_THIS add_client(client_ptr))
			client_ptr->start();
	}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SSL_H_ */

/*
 * ssl.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support ssl (based on boost::asio::ssl)
 */

#ifndef ST_ASIO_SSL_H_
#define ST_ASIO_SSL_H_

#include <boost/asio/ssl.hpp>

#include "../client.h"
#include "../server.h"
#include "../client_socket.h"
#include "../server_socket.h"
#include "../../object_pool.h"

namespace st_asio_wrapper { namespace ssl {

template<typename Socket> class socket : public Socket
{
public:
	template<typename Arg> socket(Arg& arg, boost::asio::ssl::context& ctx_) : Socket(arg, ctx_), ctx(ctx_) {}

public:
	virtual void reset() {ST_THIS reset_next_layer(ctx); Socket::reset();}

	boost::asio::ssl::context& get_context() {return ctx;}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		shutdown_ssl(true);
		Socket::on_recv_error(ec);
	}

	virtual void on_handshake(const boost::system::error_code& ec)
	{
		if (!ec)
			unified_out::info_out(ST_ASIO_LLF " handshake success.", ST_THIS id());
		else
			unified_out::error_out(ST_ASIO_LLF " handshake failed: %s", ST_THIS id(), ec.message().data());
	}

	void shutdown_ssl(bool sync = true)
	{
		if (!ST_THIS is_ready())
			return;

		ST_THIS status = Socket::GRACEFUL_SHUTTING_DOWN;
		if (!sync)
		{
			ST_THIS show_info("ssl link:", "been shutting down.");
			ST_THIS next_layer().async_shutdown(ST_THIS make_handler_error(boost::bind(&socket::shutdown_handler, this, boost::asio::placeholders::error)));
		}
		else
		{
			ST_THIS show_info("ssl link:", "been shut down.");

			boost::system::error_code ec;
			ST_THIS next_layer().shutdown(ec);
			if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
				unified_out::info_out(ST_ASIO_LLF " shutdown ssl link failed: %s", ST_THIS id(), ec.message().data());
		}
	}

private:
	void shutdown_handler(const boost::system::error_code& ec)
	{
		if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
			unified_out::info_out(ST_ASIO_LLF " async shutdown ssl link failed (maybe intentionally because of reusing)", ST_THIS id());
	}

private:
	boost::asio::ssl::context& ctx;
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public socket<tcp::client_socket_base<Packer, Unpacker, Matrix, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket<tcp::client_socket_base<Packer, Unpacker, Matrix, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer> > super;

public:
	client_socket_base(boost::asio::io_context& io_context_, boost::asio::ssl::context& ctx_) : super(io_context_, ctx_) {}
	client_socket_base(Matrix& matrix_, boost::asio::ssl::context& ctx_) : super(matrix_, ctx_) {}

	virtual const char* type_name() const {return "SSL (client endpoint)";}
	virtual int type_id() const {return 3;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (ST_THIS is_ready())
		{
			ST_THIS set_reconnect(reconnect);
			shutdown_ssl(sync);
		}
		else
			super::force_shutdown(reconnect);
	}

protected:
	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", ST_THIS id()); ST_THIS unpacker()->dump_left_data(); force_shutdown(ST_THIS is_reconnect());}
	virtual void after_close()
	{
		if (ST_THIS is_reconnect())
			ST_THIS reset_next_layer(ST_THIS get_context());

		super::after_close();
	}

private:
	virtual void connect_handler(const boost::system::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (!ec)
		{
			ST_THIS status = super::HANDSHAKING;
			ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::client,
				ST_THIS make_handler_error(boost::bind(&client_socket_base::handle_handshake, this, boost::asio::placeholders::error)));
		}
		else
			super::connect_handler(ec);
	}

	void handle_handshake(const boost::system::error_code& ec)
	{
		ST_THIS on_handshake(ec);

		if (!ec)
			super::connect_handler(ec); //return to tcp::client_socket_base::connect_handler
		else
			ST_THIS force_shutdown();
	}

	using super::shutdown_ssl;
};

template<typename Object>
class object_pool : public st_asio_wrapper::object_pool<Object>
{
private:
	typedef st_asio_wrapper::object_pool<Object> super;

public:
	object_pool(service_pump& service_pump_, const boost::asio::ssl::context::method& m) : super(service_pump_), ctx(m) {}
	boost::asio::ssl::context& context() {return ctx;}

protected:
	template<typename Arg> typename object_pool::object_type create_object(Arg& arg) {return super::create_object(arg, boost::ref(ctx));}

private:
	boost::asio::ssl::context ctx;
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class server_socket_base : public socket<tcp::server_socket_base<Packer, Unpacker, Server, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket<tcp::server_socket_base<Packer, Unpacker, Server, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>, InQueue, InContainer, OutQueue, OutContainer> > super;

public:
	server_socket_base(Server& server_, boost::asio::ssl::context& ctx_) : super(server_, ctx_) {}

	virtual const char* type_name() const {return "SSL (server endpoint)";}
	virtual int type_id() const {return 4;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown(bool sync = false) {if (ST_THIS is_ready()) shutdown_ssl(sync); else super::force_shutdown();}

protected:
	virtual bool do_start() //intercept tcp::server_socket_base::do_start (to add handshake)
	{
		ST_THIS status = super::HANDSHAKING;
		ST_THIS next_layer().async_handshake(boost::asio::ssl::stream_base::server,
			ST_THIS make_handler_error(boost::bind(&server_socket_base::handle_handshake, this, boost::asio::placeholders::error)));
		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", ST_THIS id()); ST_THIS unpacker()->dump_left_data(); ST_THIS force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec)
	{
		ST_THIS on_handshake(ec);

		if (!ec)
			super::do_start(); //return to tcp::server_socket_base::do_start
		else
			ST_THIS get_server().del_socket(ST_THIS shared_from_this());
	}

	using super::shutdown_ssl;
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = tcp::i_server> class server_base : public tcp::server_base<Socket, Pool, Server>
{
public:
	server_base(service_pump& service_pump_, const boost::asio::ssl::context::method& m) : tcp::server_base<Socket, Pool, Server>(service_pump_, m) {}
};
template<typename Socket> class single_client_base : public tcp::single_client_base<Socket>
{
private:
	typedef tcp::single_client_base<Socket> super;

public:
	single_client_base(service_pump& service_pump_, boost::asio::ssl::context& ctx_) : super(service_pump_, ctx_) {}

protected:
	virtual bool init() {if (0 == ST_THIS get_io_context_refs()) ST_THIS reset_next_layer(ST_THIS get_context()); return super::init();}
};
template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> class multi_client_base : public tcp::multi_client_base<Socket, Pool, Matrix>
{
public:
	multi_client_base(service_pump& service_pump_, const boost::asio::ssl::context::method& m) : tcp::multi_client_base<Socket, Pool, Matrix>(service_pump_, m) {}
};

}} //namespace

#endif /* ST_ASIO_SSL_H_ */

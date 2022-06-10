/*
 * websocket/ssh.h
 *
 *  Created on: 2022-6-9
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support ssl websocket (based on boost::beast)
 */

#ifndef ST_ASIO_SSL_WEBSOCKET_H_
#define ST_ASIO_SSL_WEBSOCKET_H_

#include <boost/beast/core.hpp>
#if BOOST_VERSION >= 107000
#include <boost/beast/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
namespace boost {namespace beast {template<typename Stream> using ssl_stream = boost::asio::ssl::stream<Stream>;}}
#endif
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "../websocket.h"

namespace st_asio_wrapper { namespace websocket { namespace ssl {


template<typename Stream> class lowest_layer_getter : public Stream
{
public:
#if BOOST_VERSION >= 107000
	typedef typename Stream::next_layer_type::next_layer_type::socket_type lowest_layer_type;
	lowest_layer_type& lowest_layer() {return this->next_layer().next_layer().socket();}
	const lowest_layer_type& lowest_layer() const {return this->next_layer().next_layer().socket();}
#endif

public:
	template<class... Args> explicit lowest_layer_getter(Args&&... args) : Stream(std::forward<Args>(args)...) {}
};

template<typename Socket> class socket : public Socket
{
public:
	template<typename Arg> socket(Arg& arg, boost::asio::ssl::context& ctx_) : Socket(arg, ctx_), ctx(ctx_) {}

public:
	virtual void reset() {this->reset_next_layer(ctx); Socket::reset();}
	boost::asio::ssl::context& get_context() {return ctx;}

protected:
	virtual void on_handshake(const boost::system::error_code& ec) {show_handshake(ec, "websocket");}
	virtual void on_ssl_handshake(const boost::system::error_code& ec) {show_handshake(ec, "ssl");}

	void show_handshake(const boost::system::error_code& ec, const char* type)
	{
		assert(nullptr != type);

		if (!ec)
			unified_out::info_out(ST_ASIO_LLF " %s handshake success.", this->id(), type);
		else
			unified_out::error_out(ST_ASIO_LLF " %s handshake failed: %s", this->id(), type, ec.message().data());
	}

private:
	boost::asio::ssl::context& ctx;
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public socket<websocket::client_socket_base<Packer, Unpacker, Matrix, boost::beast::ssl_stream<boost::beast::tcp_stream>, lowest_layer_getter, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<websocket::client_socket_base<Packer, Unpacker, Matrix, boost::beast::ssl_stream<boost::beast::tcp_stream>, lowest_layer_getter, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	client_socket_base(boost::asio::io_context& io_context_, boost::asio::ssl::context& ctx_) : super(io_context_, ctx_) {}
	client_socket_base(Matrix& matrix_, boost::asio::ssl::context& ctx_) : super(matrix_, ctx_) {}

	virtual const char* type_name() const {return "ssl websocket (client endpoint)";}
	virtual int type_id() const {return 9;}

protected:
	virtual void after_close()
	{
		this->reset_next_layer(this->get_context());
		super::after_close();
	}

private:
	virtual void connect_handler(const boost::system::error_code& ec) //intercept websocket::client_socket_base::connect_handler
	{
		if (ec)
			return super::connect_handler(ec);

		this->status = super::HANDSHAKING;

#if BOOST_VERSION >= 107000
		// Set a timeout on the operation
		//boost::beast::get_lowest_layer(this->next_layer()).expires_after(std::chrono::seconds(30));
#endif
		// Set SNI Hostname (many hosts need this to handshake successfully)
		if (!SSL_set_tlsext_host_name(this->next_layer().next_layer().native_handle(), this->get_server_addr().address().to_string().data()))
			return super::connect_handler(boost::beast::error_code(static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()));

		// Perform the SSL handshake
		this->next_layer().next_layer().async_handshake(boost::asio::ssl::stream_base::client, [this](const boost::system::error_code& ec) {
			this->on_ssl_handshake(ec);
			ec ? this->force_shutdown() : super::connect_handler(ec); //return to websocket::client_socket_base::connect_handler
		});
	}
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class server_socket_base : public socket<websocket::server_socket_base<Packer, Unpacker, Server, boost::beast::ssl_stream<boost::beast::tcp_stream>, lowest_layer_getter, InQueue, InContainer, OutQueue, OutContainer>>
{
private:
	typedef socket<websocket::server_socket_base<Packer, Unpacker, Server, boost::beast::ssl_stream<boost::beast::tcp_stream>, lowest_layer_getter, InQueue, InContainer, OutQueue, OutContainer>> super;

public:
	server_socket_base(Server& server_, boost::asio::ssl::context& ctx_) : super(server_, ctx_) {}

	virtual const char* type_name() const {return "ssl websocket (server endpoint)";}
	virtual int type_id() const {return 10;}

protected:
	virtual bool do_start() //intercept websocket::server_socket_base::do_start (to add handshake)
	{
		this->status = super::HANDSHAKING;

#if BOOST_VERSION >= 107000
		// Set the timeout.
		//beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
#endif
		// Perform the SSL handshake
		this->next_layer().next_layer().async_handshake(boost::asio::ssl::stream_base::server, [this](const boost::system::error_code& ec) {
			this->on_ssl_handshake(ec);
			ec ? this->get_server().del_socket(this->shared_from_this()) : super::do_start(); //return to websocket::server_socket_base::do_start
		});

		return true;
	}
};

}}}

#endif /* ST_ASIO_SSL_WEBSOCKET_H_ */

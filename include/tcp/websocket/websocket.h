/*
 * websocket.h
 *
 *  Created on: 2022-5-27
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * make st_asio_wrapper support websocket (based on boost::beast)
 */

#ifndef ST_ASIO_WEBSOCKET_H_
#define ST_ASIO_WEBSOCKET_H_

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#if BOOST_VERSION < 107000
namespace boost {namespace beast {using tcp_stream = boost::asio::ip::tcp::socket;}}
#endif

#include "../client.h"
#include "../server.h"
#include "../client_socket.h"
#include "../server_socket.h"
#include "../../object_pool.h"

namespace st_asio_wrapper { namespace websocket {

template<typename Stream> class lowest_layer_getter : public Stream
{
public:
#if BOOST_VERSION >= 107000
	typedef typename Stream::next_layer_type::socket_type lowest_layer_type;
	lowest_layer_type& lowest_layer() {return this->next_layer().socket();}
	const lowest_layer_type& lowest_layer() const {return this->next_layer().socket();}
#endif

public:
	template<class... Args> explicit lowest_layer_getter(Args&&... args) : Stream(std::forward<Args>(args)...) {}
};

template<typename NextLayer, template<typename> class LowestLayerGetter = lowest_layer_getter>
class stream : public LowestLayerGetter<boost::beast::websocket::stream<NextLayer>>
{
private:
	typedef LowestLayerGetter<boost::beast::websocket::stream<NextLayer>> super;

public:
	typedef boost::function<void(const boost::system::error_code& ec, size_t bytes_transferred)> ReadWriteCallBack;

public:
	template<class... Args> explicit stream(Args&&... args) : super(std::forward<Args>(args)...) {first_init();}

	void async_read(const ReadWriteCallBack& call_back) {super::async_read(recv_buff, call_back);}
	template<typename OutMsgType> bool parse_msg(list<OutMsgType>& msg_can)
	{
#if BOOST_VERSION < 107000
		bool re = this->is_message_done() ? (msg_can.emplace_back((const char*) recv_buff.data().data(), recv_buff.size()), true) : false;
		recv_buff.consume(-1);
#else
		bool re = this->is_message_done() ? (msg_can.emplace_back((const char*) recv_buff.cdata().data(), recv_buff.size()), true) : false;
		recv_buff.clear();
#endif

		return re;
	}
	template<typename Buffer> void async_write(const Buffer& buff, const ReadWriteCallBack& call_back) {super::async_write(buff, call_back);}

protected:
	//helper function, just call it in constructor
	void first_init() {this->binary(0 != ST_ASIO_WEBSOCKET_BINARY);}

private:
	boost::beast::flat_buffer recv_buff;
};

template<typename Socket, typename OutMsgType> class reader_writer : public Socket
{
public:
	reader_writer(boost::asio::io_context& io_context_) : Socket(io_context_) {}
	template<typename Arg> reader_writer(boost::asio::io_context& io_context_, Arg& arg) : Socket(io_context_, arg) {}

protected:
	template<typename CallBack> bool async_read(const CallBack& call_back) {this->next_layer().async_read(call_back); return true;}
	bool parse_msg(size_t bytes_transferred, list<OutMsgType>& msg_can) {return this->next_layer().parse_msg(msg_can);}

	size_t batch_msg_send_size() const {return 0;}
	template<typename Buffer, typename CallBack> void async_write(const Buffer& msg_can, const CallBack& call_back) {this->next_layer().async_write(msg_can, call_back);}
};

template<typename Socket> class socket : public Socket
{
public:
	template<typename Arg> socket(Arg& arg) : Socket(arg) {}
	template<typename Arg1, typename Arg2> socket(Arg1& arg1, Arg2& arg2) : Socket(arg1, arg2) {}

protected:
	virtual void on_handshake(const boost::system::error_code& ec)
	{
		if (!ec)
			this->show_info(nullptr, "handshake success.");
		else
			this->show_info(ec, nullptr, "handshake failed");
	}

	void shutdown_websocket()
	{
		this->status = Socket::GRACEFUL_SHUTTING_DOWN;
		this->do_something_in_strand([this]() {
			this->show_info("websocket link:", "been shutting down.");
			this->start_graceful_shutdown_monitoring();
			this->next_layer().async_close(boost::beast::websocket::close_code::normal, this->make_handler_error([this](const boost::system::error_code& ec) {
				this->stop_graceful_shutdown_monitoring();
				if (ec)
					this->show_info(ec, "websocket link", "async shutdown failed");
			}));
		});
	}
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::beast::tcp_stream, template<typename> class LowestLayerGetter = lowest_layer_getter,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class client_socket_base : public socket<tcp::client_socket_base<Packer, Unpacker, Matrix, stream<Socket, LowestLayerGetter>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>>
{
private:
	typedef socket<tcp::client_socket_base<Packer, Unpacker, Matrix, stream<Socket, LowestLayerGetter>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>> super;

public:
	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {}
	template<typename Arg> client_socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {}

	client_socket_base(Matrix& matrix_) : super(matrix_) {}
	template<typename Arg> client_socket_base(Matrix& matrix_, Arg& arg) : super(matrix_, arg) {}

	virtual const char* type_name() const {return "websocket (client endpoint)";}
	virtual int type_id() const {return 7;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false)
	{
		if (this->is_ready())
		{
			this->set_reconnect(reconnect);
			shutdown_websocket();
		}
		else
			super::force_shutdown(reconnect);
	}

protected:
	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown(this->is_reconnect());}

	virtual void connect_handler(const boost::system::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (ec)
			return super::connect_handler(ec);

		this->status = super::HANDSHAKING;

#if BOOST_VERSION >= 107000
		// Turn off the timeout on the tcp_stream, because
		// the websocket stream has its own timeout system.
		boost::beast::get_lowest_layer(this->next_layer()).expires_never();

		// Set suggested timeout settings for the websocket
		this->next_layer().set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

		// Set a decorator to change the User-Agent of the handshake
		this->next_layer().set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
			req.set(boost::beast::http::field::user_agent, "st_asio_wrapper websocket client based on " BOOST_BEAST_VERSION_STRING);
		}));
#endif
		// Provide the value of the Host HTTP header during the WebSocket handshake.
		// See https://tools.ietf.org/html/rfc7230#section-5.4
		// Perform the websocket handshake
		this->next_layer().async_handshake(this->endpoint_to_string(this->get_server_addr()), "/",
			this->make_handler_error(boost::bind(&client_socket_base::handle_handshake, this, boost::asio::placeholders::error)));
	}

private:
	void handle_handshake(const boost::system::error_code& ec)
	{
		this->on_handshake(ec);
		ec ? this->force_shutdown() : super::connect_handler(ec); //return to tcp::client_socket_base::connect_handler
	}

	using super::shutdown_websocket;
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server, typename Socket = boost::beast::tcp_stream, template<typename> class LowestLayerGetter = lowest_layer_getter,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class server_socket_base : public socket<tcp::server_socket_base<Packer, Unpacker, Server, stream<Socket, LowestLayerGetter>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>>
{
private:
	typedef socket<tcp::server_socket_base<Packer, Unpacker, Server, stream<Socket, LowestLayerGetter>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>> super;

public:
	server_socket_base(Server& server_) : super(server_) {}
	template<typename Arg> server_socket_base(Server& server_, Arg& arg) : super(server_, arg) {}

	virtual const char* type_name() const {return "websocket (server endpoint)";}
	virtual int type_id() const {return 8;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown() {if (this->is_ready()) shutdown_websocket(); else super::force_shutdown();}

protected:
	virtual bool do_start() //intercept tcp::server_socket_base::do_start (to add handshake)
	{
		this->status = super::HANDSHAKING;

#if BOOST_VERSION >= 107000
		// Set suggested timeout settings for the websocket
		this->next_layer().set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

		// Set a decorator to change the Server of the handshake
		this->next_layer().set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
			res.set(boost::beast::http::field::server, "st_asio_wrapper websocket server based on " BOOST_BEAST_VERSION_STRING);
		}));
#endif
		// Accept the websocket handshake
		this->next_layer().async_accept(this->make_handler_error(boost::bind(&server_socket_base::handle_handshake, this, boost::asio::placeholders::error)));
		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); this->force_shutdown();}

private:
	void handle_handshake(const boost::system::error_code& ec)
	{
		this->on_handshake(ec);
		ec ? this->get_server().del_socket(this->shared_from_this()) : super::do_start(); //return to tcp::server_socket_base::do_start
	}

	using super::shutdown_websocket;
};

}} //namespace

#endif /* ST_ASIO_WEBSOCKET_H_ */

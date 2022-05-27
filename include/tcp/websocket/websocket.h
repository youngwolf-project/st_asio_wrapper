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

#include "../client.h"
#include "../server.h"
#include "../client_socket.h"
#include "../server_socket.h"
#include "../../object_pool.h"

namespace st_asio_wrapper { namespace websocket {

template<class NextLayer>
class stream : public boost::beast::websocket::stream<NextLayer>
{
private:
	typedef boost::beast::websocket::stream<NextLayer> super;

public:
	typedef boost::function<void(const boost::system::error_code& ec, size_t bytes_transferred)> ReadWriteCallBack;

public:
	stream(boost::asio::io_context& io_context_) : super(io_context_) {}
	template<typename Arg> stream(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {}
#if BOOST_ASIO_VERSION >= 101300
	stream(const boost::asio::any_io_executor& executor) : super(executor) {}
	template<typename Arg> stream(const boost::asio::any_io_executor& executor, Arg& arg) : super(executor, arg) {}
#endif

	typedef typename super::next_layer_type::socket_type lowest_layer_type;
	lowest_layer_type& lowest_layer() {return this->next_layer().socket();}
	const lowest_layer_type& lowest_layer() const {return this->next_layer().socket();}

	void async_read(const ReadWriteCallBack& call_back) {super::async_read(recv_buff, call_back);}
	template<typename OutMsgType> bool parse_msg(list<OutMsgType>& msg_can)
	{
		bool re = this->is_message_done() ? (msg_can.emplace_back((const char*) recv_buff.cdata().data(), recv_buff.size()), true) : false;
		recv_buff.clear();

		return re;
	}
	template<typename Buffer> void async_write(const Buffer& buff, const ReadWriteCallBack& call_back) {super::async_write(buff, call_back);}

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

public:
	virtual void reset() {this->reset_next_layer(); Socket::reset();}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		shutdown_websocket(true);
		Socket::on_recv_error(ec);
	}

	virtual void on_handshake(const boost::system::error_code& ec)
	{
		if (!ec)
			unified_out::info_out(ST_ASIO_LLF " handshake success.", this->id());
		else
			unified_out::error_out(ST_ASIO_LLF " handshake failed: %s", this->id(), ec.message().data());
	}

	void shutdown_websocket(bool sync = true)
	{
		if (!this->is_ready())
			return;

		this->status = Socket::GRACEFUL_SHUTTING_DOWN;
		if (!sync)
		{
			this->show_info("websocket link:", "been shutting down.");
			this->next_layer().async_close(boost::beast::websocket::close_code::normal,
				this->make_handler_error(boost::bind(&socket::shutdown_handler, this, boost::asio::placeholders::error)));
		}
		else
		{
			this->show_info("websocket link:", "been shut down.");

			boost::system::error_code ec;
			this->next_layer().close(boost::beast::websocket::close_code::normal, ec);
			if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
				unified_out::info_out(ST_ASIO_LLF " shutdown websocket link failed: %s", this->id(), ec.message().data());
		}
	}

private:
	void shutdown_handler(const boost::system::error_code& ec)
	{
		if (ec && boost::asio::error::eof != ec) //the endpoint who initiated a shutdown operation will get error eof.
			unified_out::info_out(ST_ASIO_LLF " async shutdown websocket link failed (maybe intentionally because of reusing)", this->id());
	}
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class client_socket_base : public socket<tcp::client_socket_base<Packer, Unpacker, Matrix, stream<boost::beast::tcp_stream>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter> >
{
private:
	typedef socket<tcp::client_socket_base<Packer, Unpacker, Matrix, stream<boost::beast::tcp_stream>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter> > super;

public:
	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {}
	template<typename Arg> client_socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {}

	client_socket_base(Matrix& matrix_) : super(matrix_) {}
	template<typename Arg> client_socket_base(Matrix& matrix_, Arg& arg) : super(matrix_, arg) {}

	virtual const char* type_name() const {return "sebsocket (client endpoint)";}
	virtual int type_id() const {return 5;}

	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false) {graceful_shutdown(reconnect);}
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (this->is_ready())
		{
			this->set_reconnect(reconnect);
			shutdown_websocket(sync);
		}
		else
			super::force_shutdown(reconnect);
	}

protected:
	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); force_shutdown(this->is_reconnect());}
	virtual void after_close()
	{
		if (this->is_reconnect())
			this->reset_next_layer();

		super::after_close();
	}

private:
	virtual void connect_handler(const boost::system::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (!ec)
		{
			this->status = super::HANDSHAKING;

			// Set suggested timeout settings for the websocket
			this->next_layer().set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

			// Set a decorator to change the User-Agent of the handshake
			this->next_layer().set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
				req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " ascs websocket client");
			}));

			// Update the host_ string. This will provide the value of the
			// Host HTTP header during the WebSocket handshake.
			// See https://tools.ietf.org/html/rfc7230#section-5.4
			// Perform the websocket handshake
			this->next_layer().async_handshake(this->endpoint_to_string(this->get_server_addr()), "/", [this](const boost::system::error_code& ec) {
				this->on_handshake(ec);

				if (!ec)
					super::connect_handler(ec); //return to tcp::client_socket_base::connect_handler
				else
					this->force_shutdown();
			});
		}
		else
			super::connect_handler(ec);
	}

	using super::shutdown_websocket;
};

template<typename Packer, typename Unpacker, typename Server = tcp::i_server,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class server_socket_base : public socket<tcp::server_socket_base<Packer, Unpacker, Server, stream<boost::beast::tcp_stream>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter> >
{
private:
	typedef socket<tcp::server_socket_base<Packer, Unpacker, Server, stream<boost::beast::tcp_stream>, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter> > super;

public:
	server_socket_base(Server& server_) : super(server_) {}
	template<typename Arg> server_socket_base(Server& server_, Arg& arg) : super(server_, arg) {}

	virtual const char* type_name() const {return "sebsocket (server endpoint)";}
	virtual int type_id() const {return 6;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {graceful_shutdown();} //must with async mode (the default value), because server_base::uninit will call this function
	void graceful_shutdown(bool sync = false) {if (this->is_ready()) shutdown_websocket(sync); else super::force_shutdown();}

protected:
	virtual bool do_start() //intercept tcp::server_socket_base::do_start (to add handshake)
	{
		this->status = super::HANDSHAKING;

		// Set suggested timeout settings for the websocket
		this->next_layer().set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

		// Set a decorator to change the Server of the handshake
		this->next_layer().set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
			res.set(boost::beast::http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " ascs websocket server");
		}));

		// Accept the websocket handshake
		this->next_layer().async_accept([this](const boost::system::error_code& ec) {
			this->on_handshake(ec);

			if (!ec)
				super::do_start(); //return to tcp::server_socket_base::do_start
			else
				this->get_server().del_socket(this->shared_from_this());
		});

		return true;
	}

	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", this->id()); this->unpacker()->dump_left_data(); this->force_shutdown();}

	using super::shutdown_websocket;
};

}} //namespace

#endif /* ST_ASIO_WEBSOCKET_H_ */

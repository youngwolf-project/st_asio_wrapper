/*
 * server_socket.h
 *
 *  Created on: 2013-4-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_SERVER_SOCKET_H_
#define ST_ASIO_SERVER_SOCKET_H_

#include "socket.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class generic_server_socket : public socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>,
	public boost::enable_shared_from_this<generic_server_socket<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	generic_server_socket(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg> generic_server_socket(Server& server_, Arg& arg) : super(server_.get_service_pump(), arg), server(server_) {}
	~generic_server_socket() {ST_THIS clear_io_context_refs();}

	virtual const char* type_name() const {return "TCP (server endpoint)";}
	virtual int type_id() const {return 2;}

	virtual void take_over(boost::shared_ptr<generic_server_socket> socket_ptr) {} //restore this socket from socket_ptr

	void disconnect() {force_shutdown();}
	void force_shutdown()
	{
		if (super::FORCE_SHUTTING_DOWN != ST_THIS status)
			ST_THIS show_info("server link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool sync = false)
	{
		if (ST_THIS is_broken())
			return force_shutdown();
		else if (!ST_THIS is_shutting_down())
			ST_THIS show_info("server link:", "being shut down gracefully.");

		super::graceful_shutdown(sync);
	}

protected:
	Server& get_server() {return server;}
	const Server& get_server() const {return server;}

	virtual void on_unpack_error() {unified_out::error_out(ST_ASIO_LLF " can not unpack msg.", ST_THIS id()); ST_THIS unpacker()->dump_left_data(); force_shutdown();}
	//do not forget to force_shutdown this socket(in del_socket(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		ST_THIS show_info(ec, "server link:", "broken/been shut down");

#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
		force_shutdown();
#else
		server.del_socket(ST_THIS shared_from_this());
		ST_THIS status = super::BROKEN;
#endif
	}

	virtual void on_async_shutdown_error() {force_shutdown();}
	virtual bool on_heartbeat_error() {ST_THIS show_info("server link:", "broke unexpectedly."); force_shutdown(); return false;}
	virtual void on_close()
	{
		ST_THIS clear_io_context_refs();
		super::on_close();
	}

private:
	//call following two functions (via timer object's add_io_context_refs, sub_io_context_refs or clear_io_context_refs) in:
	// 1. on_xxxx callbacks on this object;
	// 2. use this->post or this->set_timer to emit an async event, then in the callback.
	//otherwise, you must protect them to not be called with reset and on_close simultaneously
	virtual void attach_io_context(boost::asio::io_context& io_context_, unsigned refs) {server.get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(boost::asio::io_context& io_context_, unsigned refs) {server.get_service_pump().return_io_context(io_context_, refs);}

private:
	Server& server;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class server_socket_base : public generic_server_socket<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef generic_server_socket<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	server_socket_base(Server& server_) : super(server_) {}
	template<typename Arg> server_socket_base(Server& server_, Arg& arg) : super(server_, arg) {}
};

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template <typename Packer, typename Unpacker, typename Server = i_server,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class unix_server_socket_base : public generic_server_socket<Packer, Unpacker, Server, boost::asio::local::stream_protocol::socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef generic_server_socket<Packer, Unpacker, Server, boost::asio::local::stream_protocol::socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	unix_server_socket_base(Server& server_) : super(server_) {}
};
#endif

}} //namespace

#endif /* ST_ASIO_SERVER_SOCKET_H_ */

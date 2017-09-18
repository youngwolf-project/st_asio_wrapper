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
#include "../container.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class server_socket_base : public socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>,
	public boost::enable_shared_from_this<server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer> >
{
private:
	typedef socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	server_socket_base(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg>
	server_socket_base(Server& server_, Arg& arg) : super(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function
	virtual void reset() {super::reset();}
	virtual void take_over(boost::shared_ptr<server_socket_base> socket_ptr) {} //restore this socket from socket_ptr

	void disconnect() {force_shutdown();}
	void force_shutdown()
	{
		if (super::FORCE_SHUTTING_DOWN != ST_THIS status)
			show_info("server link:", "been shut down.");

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
			show_info("server link:", "being shut down gracefully.");

		super::graceful_shutdown(sync);
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		BOOST_AUTO(ep, ST_THIS lowest_layer().remote_endpoint(ec));
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().data(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		BOOST_AUTO(ep, ST_THIS lowest_layer().remote_endpoint(ec2));
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().data(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); force_shutdown();}
	//do not forget to force_shutdown this socket(in del_socket(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		show_info("server link:", "broken/been shut down", ec);

#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
		force_shutdown();
#else
		ST_THIS status = super::BROKEN;
		server.del_socket(ST_THIS shared_from_this());
#endif
	}

	virtual void on_async_shutdown_error() {force_shutdown();}
	virtual bool on_heartbeat_error() {show_info("server link:", "broke unexpectedly."); force_shutdown(); return false;}

protected:
	Server& server;
};

}} //namespace

#endif /* ST_ASIO_SERVER_SOCKET_H_ */

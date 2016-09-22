/*
 * st_asio_wrapper_server_socket.h
 *
 *  Created on: 2013-4-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVER_SOCKET_H_
#define ST_ASIO_WRAPPER_SERVER_SOCKET_H_

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_tcp_socket.h"

namespace st_asio_wrapper
{

class i_server
{
public:
	virtual st_service_pump& get_service_pump() = 0;
	virtual const st_service_pump& get_service_pump() const = 0;
	virtual bool del_client(const boost::shared_ptr<st_timer>& client_ptr) = 0;
};

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket>
class st_server_socket_base : public st_tcp_socket_base<Socket, Packer, Unpacker>, public boost::enable_shared_from_this<st_server_socket_base<Packer, Unpacker, Server, Socket>>
{
protected:
	typedef st_tcp_socket_base<Socket, Packer, Unpacker> super;

public:
	static const unsigned char TIMER_BEGIN = super::TIMER_END;
	static const unsigned char TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const unsigned char TIMER_END = TIMER_BEGIN + 10;

	st_server_socket_base(Server& server_) : super(server_.get_service_pump()), server(server_) {}
	template<typename Arg>
	st_server_socket_base(Server& server_, Arg& arg) : super(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//please note, when reuse this socket, st_object_pool will invoke reset(), child must re-write it to initialize all member variables,
	//and then do not forget to invoke st_server_socket_base::reset() to initialize father's member variables
	virtual void reset() {super::reset();}

	void disconnect() {force_shutdown();}
	void force_shutdown()
	{
		if (1 != ST_THIS shutdown_state)
			show_info("server link:", "been shut down.");

		super::force_shutdown();
	}

	void graceful_shutdown(bool sync = true)
	{
		if (!ST_THIS is_shutting_down())
			show_info("server link:", "being shut down gracefully.");

		if (super::graceful_shutdown(sync))
			ST_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](unsigned char id)->bool {return ST_THIS async_shutdown_handler(id, ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().remote_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		auto ep = ST_THIS lowest_layer().remote_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().c_str(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS stopped())
		{
			ST_THIS do_recv_msg();
			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); ST_THIS force_shutdown();}
	//do not forget to force_shutdown this socket(in del_client(), there's a force_shutdown() invocation)
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		ST_THIS show_info("server link:", "broken/been shut down", ec);

#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
		ST_THIS force_shutdown();
#else
		server.del_client(boost::dynamic_pointer_cast<st_timer>(ST_THIS shared_from_this()));
#endif

		ST_THIS shutdown_state = 0;
	}

private:
	bool async_shutdown_handler(unsigned char id, ssize_t loop_num)
	{
		assert(TIMER_ASYNC_SHUTDOWN == id);

		if (2 == ST_THIS shutdown_state)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS update_timer_info(id, 10, [loop_num, this](unsigned char id)->bool {return ST_THIS async_shutdown_handler(id, loop_num);});
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful shutdown within %d seconds", ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION);
				force_shutdown();
			}
		}

		return false;
	}

protected:
	Server& server;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_SOCKET_H_ */

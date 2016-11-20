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

#include "st_asio_wrapper_tcp_socket.h"
#include "st_asio_wrapper_container.h"

namespace st_asio_wrapper
{

template<typename Packer, typename Unpacker, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_server_socket_base : public st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>,
	public boost::enable_shared_from_this<st_server_socket_base<Packer, Unpacker, Server, Socket, InQueue, InContainer, OutQueue, OutContainer>>
{
protected:
	typedef st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const st_timer::tid TIMER_BEGIN = super::TIMER_END;
	static const st_timer::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN;
	static const st_timer::tid TIMER_END = TIMER_BEGIN + 10;

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
		if (super::FORCE != ST_THIS shutdown_state)
			show_info("server link:", "been shut down.");

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool sync = false)
	{
		if (!ST_THIS is_shutting_down())
			show_info("server link:", "being shut down gracefully.");

		if (super::graceful_shutdown(sync))
			ST_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](st_timer::tid id)->bool {return ST_THIS async_shutdown_handler(id, ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().remote_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().data(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		auto ep = ST_THIS lowest_layer().remote_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().data(), ep.port(), tail, ec.value(), ec.message().data());
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
		ST_THIS shutdown_state = super::NONE;
	}

private:
	bool async_shutdown_handler(st_timer::tid id, size_t loop_num)
	{
		assert(TIMER_ASYNC_SHUTDOWN == id);

		if (super::GRACEFUL == ST_THIS shutdown_state)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS update_timer_info(id, 10, [loop_num, this](st_timer::tid id)->bool {return ST_THIS async_shutdown_handler(id, loop_num);});
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

/*
 * st_asio_wrapper_connector.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_WRAPPER_CONNECTOR_H_
#define ST_ASIO_WRAPPER_CONNECTOR_H_

#include "st_asio_wrapper_tcp_socket.h"
#include "st_asio_wrapper_container.h"

#ifndef ST_ASIO_SERVER_IP
#define ST_ASIO_SERVER_IP			"127.0.0.1"
#endif
#ifndef ST_ASIO_SERVER_PORT
#define ST_ASIO_SERVER_PORT			5050
#endif
static_assert(ST_ASIO_SERVER_PORT > 0, "server port must be bigger than zero.");
#ifndef ST_ASIO_RECONNECT_INTERVAL
#define ST_ASIO_RECONNECT_INTERVAL	500 //millisecond(s), negative number means don't reconnect the server
#endif

namespace st_asio_wrapper
{

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ip::tcp::socket,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class st_connector_base : public st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer>
{
protected:
	typedef st_tcp_socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const st_timer::tid TIMER_BEGIN = super::TIMER_END;
	static const st_timer::tid TIMER_CONNECT = TIMER_BEGIN;
	static const st_timer::tid TIMER_ASYNC_SHUTDOWN = TIMER_BEGIN + 1;
	static const st_timer::tid TIMER_HEARTBEAT_CHECK = TIMER_BEGIN + 2;
	static const st_timer::tid TIMER_END = TIMER_BEGIN + 10;

	st_connector_base(boost::asio::io_service& io_service_) : super(io_service_), connected(false), reconnecting(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}
	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) : super(io_service_, arg), connected(false), reconnecting(true) {set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this st_connector_base when invoke it
	//notice, when reusing this st_connector_base, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_connector_base::reset() to initialize father's member variables
	virtual void reset() {connected = false; reconnecting = true; super::reset();}
	virtual bool obsoleted() {return !reconnecting && super::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		boost::system::error_code ec;
		auto addr = boost::asio::ip::address::from_string(ip, ec);
		if (ec)
			return false;

		server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	bool is_connected() const {return connected;}

	//if the connection is broken unexpectedly, st_connector will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		if (super::FORCE != ST_THIS shutdown_state)
		{
			show_info("client link:", "been shut down.");
			reconnecting = reconnect;
			connected = false;
		}

		super::force_shutdown();
	}

	//sync must be false if you call graceful_shutdown in on_msg
	//furthermore, you're recommended to call this function with sync equal to false in all service threads,
	//all callbacks will be called in service threads.
	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false, bool sync = true)
	{
		if (ST_THIS is_shutting_down())
			return;
		else if (!is_connected())
			return force_shutdown(reconnect);

		show_info("client link:", "being shut down gracefully.");
		reconnecting = reconnect;
		connected = false;

		if (super::graceful_shutdown(sync))
			ST_THIS set_timer(TIMER_ASYNC_SHUTDOWN, 10, [this](st_timer::tid id)->bool {return ST_THIS async_shutdown_handler(id, ST_ASIO_GRACEFUL_SHUTDOWN_MAX_DURATION * 100);});
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().data(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		auto ep = ST_THIS lowest_layer().local_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().data(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS stopped())
		{
			if (reconnecting && !is_connected())
				ST_THIS lowest_layer().async_connect(server_addr, ST_THIS make_handler_error([this](const boost::system::error_code& ec) {ST_THIS connect_handler(ec);}));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	//after how much time(ms), st_connector will try to reconnect to the server, negative means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_closable() {return !reconnecting;}
	virtual bool is_send_allowed() {return is_connected() && super::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_shutdown();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		show_info("client link:", "broken/been shut down", ec);

		force_shutdown(ST_THIS is_shutting_down() ? reconnecting : prepare_reconnect(ec) >= 0);
		ST_THIS shutdown_state = super::NONE;

		if (reconnecting)
			ST_THIS start();
	}

	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if ((boost::asio::error::operation_aborted != ec || reconnecting) && !ST_THIS stopped())
		{
#ifdef _WIN32
			if (boost::asio::error::connection_refused != ec && boost::asio::error::network_unreachable != ec && boost::asio::error::timed_out != ec)
#endif
			{
				boost::system::error_code ec;
				ST_THIS lowest_layer().close(ec);
			}

			auto delay = prepare_reconnect(ec);
			if (delay >= 0)
			{
				ST_THIS set_timer(TIMER_CONNECT, delay, [this](st_timer::tid id)->bool {ST_THIS do_start(); return false;});
				return true;
			}
		}

		return false;
	}

	//unit is second
	//if macro ST_ASIO_HEARTBEAT_INTERVAL is bigger than zero, st_connector_base will start a timer to call this automatically with interval equal to ST_ASIO_HEARTBEAT_INTERVAL.
	//otherwise, you can call check_heartbeat with you own logic, but you still need to define a valid ST_ASIO_HEARTBEAT_MAX_ABSENCE macro, please note.
	bool check_heartbeat(int interval)
	{
		assert(interval > 0);

		auto now = time(nullptr);
		if (now - ST_THIS last_interact_time >= interval) //client send heartbeat on its own initiative
			ST_THIS send_heartbeat('c');

		if (ST_THIS clean_heartbeat() > 0)
			ST_THIS last_interact_time = now;
		else if (now - ST_THIS last_interact_time >= interval * ST_ASIO_HEARTBEAT_MAX_ABSENCE)
		{
			show_info("client link:", "broke unexpectedly.");
			force_shutdown(ST_THIS is_shutting_down() ? reconnecting : prepare_reconnect(boost::system::error_code(boost::asio::error::network_down)) >= 0);
		}

		return ST_THIS started(); //always keep this timer
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
				force_shutdown(reconnecting);
			}
		}

		return false;
	}

	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			connected = reconnecting = true;
			ST_THIS reset_state();
			on_connect();
			ST_THIS last_interact_time = time(nullptr);
			if (ST_ASIO_HEARTBEAT_INTERVAL > 0)
				ST_THIS set_timer(TIMER_HEARTBEAT_CHECK, ST_ASIO_HEARTBEAT_INTERVAL * 1000, [this](st_timer::tid id)->bool {return ST_THIS check_heartbeat(ST_ASIO_HEARTBEAT_INTERVAL);});
			ST_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
		else
			prepare_next_reconnect(ec);
	}

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

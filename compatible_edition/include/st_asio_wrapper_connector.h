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

#ifndef ST_ASIO_SERVER_IP
#define ST_ASIO_SERVER_IP			"127.0.0.1"
#endif
#ifndef ST_ASIO_SERVER_PORT
#define ST_ASIO_SERVER_PORT			5050
#endif
#ifndef ST_ASIO_RECONNECT_INTERVAL
#define ST_ASIO_RECONNECT_INTERVAL	500 //millisecond(s)
#endif

namespace st_asio_wrapper
{

template <typename Packer = ST_ASIO_DEFAULT_PACKER, typename Unpacker = ST_ASIO_DEFAULT_UNPACKER, typename Socket = boost::asio::ip::tcp::socket>
class st_connector_base : public st_tcp_socket_base<Socket, Packer, Unpacker>
{
public:
	static const unsigned char TIMER_BEGIN = st_tcp_socket_base<Socket, Packer, Unpacker>::TIMER_END;
	static const unsigned char TIMER_CONNECT = TIMER_BEGIN;
	static const unsigned char TIMER_ASYNC_CLOSE = TIMER_BEGIN + 1;
	static const unsigned char TIMER_END = TIMER_BEGIN + 10;

	st_connector_base(boost::asio::io_service& io_service_) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_), connected(false), reconnecting(true)
		{set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_, arg), connected(false), reconnecting(true)
		{set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this st_connector_base when invoke it
	//notice, when reusing this st_connector_base, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_connector_base::reset() to initialize father's member variables
	virtual void reset() {connected = false; reconnecting = true; st_tcp_socket_base<Socket, Packer, Unpacker>::reset();}
	virtual bool obsoleted() {return !reconnecting && st_tcp_socket_base<Socket, Packer, Unpacker>::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP)
	{
		boost::system::error_code ec;
		BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
		if (ec)
			return false;

		server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	bool is_connected() const {return connected;}

	//if the connection is broken unexpectedly, st_connector will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_close(reconnect);}
	void force_close(bool reconnect = false)
	{
		if (1 != ST_THIS close_state)
		{
			show_info("client link:", "been closed.");
			reconnecting = reconnect;
			connected = false;
		}

		st_tcp_socket_base<Socket, Packer, Unpacker>::force_close();
	}

	//sync must be false if you call graceful_close in on_msg
	void graceful_close(bool reconnect = false, bool sync = true)
	{
		if (ST_THIS is_closing())
			return;
		else if (!is_connected())
			return force_close(reconnect);

		show_info("client link:", "been closing gracefully.");
		reconnecting = reconnect;
		connected = false;

		if (st_tcp_socket_base<Socket, Packer, Unpacker>::graceful_close(sync))
			ST_THIS set_timer(TIMER_ASYNC_CLOSE, 10, boost::bind(&st_connector_base::async_close_handler, this, _1, ST_ASIO_GRACEFUL_CLOSE_MAX_DURATION * 100));
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		BOOST_AUTO(ep, ST_THIS lowest_layer().local_endpoint(ec));
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		BOOST_AUTO(ep, ST_THIS lowest_layer().local_endpoint(ec2));
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().c_str(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS stopped())
		{
			if (reconnecting && !is_connected())
				ST_THIS lowest_layer().async_connect(server_addr, ST_THIS make_handler_error(boost::bind(&st_connector_base::connect_handler, this, boost::asio::placeholders::error)));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	//after how much time(ms), st_connector will try to reconnect to the server, negative means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_send_allowed() const {return is_connected() && st_tcp_socket_base<Socket, Packer, Unpacker>::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_close();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		show_info("client link:", "broken/closed", ec);

		force_close(ST_THIS is_closing() ? reconnecting : prepare_reconnect(ec) >= 0);
		ST_THIS close_state = 0;

		if (reconnecting)
			ST_THIS start();
	}

	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if ((boost::asio::error::operation_aborted != ec || reconnecting) && !ST_THIS stopped())
		{
			if (boost::asio::error::connection_refused != ec && boost::asio::error::timed_out != ec)
			{
				boost::system::error_code ec;
				ST_THIS lowest_layer().close(ec);
			}

			int delay = prepare_reconnect(ec);
			if (delay >= 0)
			{
				ST_THIS set_timer(TIMER_CONNECT, delay, boost::bind(&st_connector_base::reconnect_handler, this, _1));
				return true;
			}
		}

		return false;
	}

private:
	bool reconnect_handler(unsigned char id)
	{
		assert(TIMER_CONNECT == id);

		do_start();
		return false;
	}

	bool async_close_handler(unsigned char id, ssize_t loop_num)
	{
		assert(TIMER_ASYNC_CLOSE == id);

		if (2 == ST_THIS close_state)
		{
			--loop_num;
			if (loop_num > 0)
			{
				ST_THIS update_timer_info(id, 10, boost::bind(&st_connector_base::async_close_handler, this, _1, loop_num));
				return true;
			}
			else
			{
				unified_out::info_out("failed to graceful close within %d seconds", ST_ASIO_GRACEFUL_CLOSE_MAX_DURATION);
				force_close(reconnecting);
			}
		}

		return false;
	}

	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			connected = reconnecting = true;
			on_connect();
			ST_THIS reset_state();
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
typedef st_connector_base<> st_connector;

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

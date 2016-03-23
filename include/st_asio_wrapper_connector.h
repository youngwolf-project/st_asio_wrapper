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

#ifndef SERVER_IP
#define SERVER_IP				"127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT				5050
#endif
#ifndef RE_CONNECT_INTERVAL
#define RE_CONNECT_INTERVAL		500 //millisecond(s)
#endif

namespace st_asio_wrapper
{

template <typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UNPACKER, typename Socket = boost::asio::ip::tcp::socket>
class st_connector_base : public st_tcp_socket_base<Socket, Packer, Unpacker>
{
public:
	st_connector_base(boost::asio::io_service& io_service_) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_), connected(false), reconnecting(true)
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_, arg), connected(false), reconnecting(true)
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this st_connector_base when invoke it
	//notice, when reusing this st_connector_base, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_connector_base::reset() to initialize father's member variables
	virtual void reset() {connected = false; reconnecting = true; st_tcp_socket_base<Socket, Packer, Unpacker>::reset(); ST_THIS close_state = 0;}
	virtual bool obsoleted() {return !reconnecting && st_tcp_socket_base<Socket, Packer, Unpacker>::obsoleted();}

	bool set_server_addr(unsigned short port, const std::string& ip = SERVER_IP)
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
			ST_THIS set_timer(11, 10, reinterpret_cast<const void*>((ssize_t) (GRACEFUL_CLOSE_MAX_DURATION * 100)));
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		auto ep = ST_THIS lowest_layer().local_endpoint(ec);
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		auto ep = ST_THIS lowest_layer().local_endpoint(ec2);
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().c_str(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS get_io_service().stopped())
		{
			if (!is_connected())
				ST_THIS lowest_layer().async_connect(server_addr, boost::bind(&st_connector_base::connect_handler, this, boost::asio::placeholders::error));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	//after how much time(ms), st_connector will try to reconnect to the server, negative means give up.
	virtual int prepare_re_connect(const boost::system::error_code& ec) {return RE_CONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_send_allowed() const {return is_connected() && st_tcp_socket_base<Socket, Packer, Unpacker>::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_close();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		show_info("client link:", "broken/closed", ec);

		force_close(ST_THIS is_closing() ? reconnecting : prepare_re_connect(ec) >= 0);
		ST_THIS close_state = 0;

		if (reconnecting)
			ST_THIS start();
	}

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
		case 10:
			do_start();
			break;
		case 11:
			if (2 == ST_THIS close_state)
			{
				auto loop_num = reinterpret_cast<ssize_t>(user_data);
				--loop_num;

				if (loop_num > 0)
				{
					ST_THIS update_timer_info(id, 10, reinterpret_cast<const void*>(loop_num));
					return true;
				}
				else
				{
					unified_out::info_out("failed to graceful close within %d seconds", GRACEFUL_CLOSE_MAX_DURATION);
					force_close(reconnecting);
				}
			}
			break;
		case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: //reserved
			break;
		default:
			return st_tcp_socket_base<Socket, Packer, Unpacker>::on_timer(id, user_data);
			break;
		}

		return false;
	}

	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			connected = reconnecting = true;
			on_connect();
			ST_THIS send_msg(); //send buffer may have msgs, send them
			do_start();
		}
		else if ((boost::asio::error::operation_aborted != ec || reconnecting) && !ST_THIS get_io_service().stopped())
		{
			auto delay = prepare_re_connect(ec);
			if (delay >= 0)
				ST_THIS set_timer(10, delay, nullptr);

			if (boost::asio::error::connection_refused != ec && boost::asio::error::timed_out != ec)
			{
				boost::system::error_code ec;
				ST_THIS lowest_layer().close(ec);
			}
		}
	}

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
};
typedef st_connector_base<> st_connector;

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

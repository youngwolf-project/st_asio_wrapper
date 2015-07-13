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

#ifdef RE_CONNECT_CONTROL
#define RE_CONNECT_CHECK	prepare_re_connect()
#else
#define RE_CONNECT_CHECK	true
#endif

namespace st_asio_wrapper
{

template <typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UNPACKER, typename Socket = boost::asio::ip::tcp::socket>
class st_connector_base : public st_tcp_socket_base<Socket, Packer, Unpacker>
{
public:
	st_connector_base(boost::asio::io_service& io_service_) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_), connected(false), reconnecting(true)
#ifdef RE_CONNECT_CONTROL
		, re_connect_times(-1)
#endif
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) : st_tcp_socket_base<Socket, Packer, Unpacker>(io_service_, arg), connected(false), reconnecting(true)
#ifdef RE_CONNECT_CONTROL
		, re_connect_times(-1)
#endif
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this st_connector_base when invoke it
	//notice, when reusing this st_connector_base, st_object_pool will invoke reset(), child must re-write this to initialize
	//all member variables, and then do not forget to invoke st_connector_base::reset() to initialize father's member variables
	virtual void reset() {connected = false; reconnecting = true; st_tcp_socket_base<Socket, Packer, Unpacker>::reset();}

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

#ifdef RE_CONNECT_CONTROL
	void set_re_connect_times(size_t times) {re_connect_times = times;}
#endif
	bool is_connected() const {return connected;}

	//the following three functions can only be used when the connection is OK and you want to reconnect to the server.
	//if the connection is broken unexpectedly, st_connector will try to reconnect to the server automatically.
	void disconnect(bool reconnect = false) {force_close(reconnect);}
	void force_close(bool reconnect = false) {reconnecting = reconnect; connected = false; st_tcp_socket_base<Socket, Packer, Unpacker>::force_close();}
	void graceful_close(bool reconnect = false)
	{
		if (!is_connected())
			force_close(reconnect);
		else
		{
			reconnecting = reconnect;
			connected = false;
			st_tcp_socket_base<Socket, Packer, Unpacker>::graceful_close();
		}
	}

protected:
	virtual bool do_start() //connect or receive
	{
		if (!ST_THIS get_io_service().stopped())
		{
			if (reconnecting && !is_connected())
				ST_THIS lowest_layer().async_connect(server_addr, boost::bind(&st_connector_base::connect_handler, this, boost::asio::placeholders::error));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_send_allowed() const {return is_connected() && st_tcp_socket_base<Socket, Packer, Unpacker>::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_close();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		unified_out::error_out("connection closed.");

		auto reconnect = reconnecting;
		if (ST_THIS is_closing())
			force_close(reconnecting);
		else
		{
			force_close(reconnecting);
			if (reconnect && (!ec || boost::asio::error::operation_aborted == ec || !RE_CONNECT_CHECK))
				reconnect = false;
		}

		if (reconnect)
			do_start();
	}

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
		case 10:
			do_start();
			break;
		case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: //reserved
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
			ST_THIS send_msg(); //send msg buffer may have msgs, send them
			do_start();
		}
		else if ((boost::asio::error::operation_aborted != ec || reconnecting) && RE_CONNECT_CHECK && !ST_THIS get_io_service().stopped())
			ST_THIS set_timer(10, RE_CONNECT_INTERVAL, nullptr);
	}

#ifdef RE_CONNECT_CONTROL
	bool prepare_re_connect()
	{
		if (0 == re_connect_times)
		{
			unified_out::info_out("re-connecting abandoned!");
			return false;
		}

		--re_connect_times;
		return true;
	}
#endif

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
#ifdef RE_CONNECT_CONTROL
	size_t re_connect_times;
#endif
};
typedef st_connector_base<> st_connector;

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

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

template <typename Socket = boost::asio::ip::tcp::socket>
class st_connector_base : public st_tcp_socket_base<Socket>
{
public:
	st_connector_base(boost::asio::io_service& io_service_) :
		st_tcp_socket_base<Socket>(io_service_), connected(false), reconnecting(false)
#ifdef RE_CONNECT_CONTROL
		, re_connect_times(-1)
#endif
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	template<typename Arg>
	st_connector_base(boost::asio::io_service& io_service_, Arg& arg) :
		st_tcp_socket_base<Socket>(io_service_, arg), connected(false), reconnecting(false)
#ifdef RE_CONNECT_CONTROL
		, re_connect_times(-1)
#endif
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	//reset all, be ensure that there's no any operations performed on this st_connector_base when invoke it
	//notice, when resue this st_connector_base, st_object_pool will invoke reset(), child must re-write this to init
	//all member variables, and then do not forget to invoke st_connector_base::reset() to init father's
	//member variables
	virtual void reset() {connected = false; reconnecting = false; st_tcp_socket_base<Socket>::reset();}

	void set_server_addr(unsigned short port, const std::string& ip)
	{
		boost::system::error_code ec;
		server_addr = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip, ec), port); assert(!ec);
		boost::asio::ip::tcp::resolver resolver(ST_THIS get_io_service());
		server_addr_iter = resolver.resolve(server_addr);
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

#ifdef RE_CONNECT_CONTROL
	void set_re_connect_times(size_t times) {re_connect_times = times;}
#endif
	bool is_connected() const {return connected;}

	void disconnect(bool reconnect = false) {force_close(reconnect);}
	void force_close(bool reconnect = false)
		{reconnecting = reconnect; connected = false; st_tcp_socket_base<Socket>::force_close();}
	void graceful_close(bool reconnect = false)
	{
		if (!is_connected())
			force_close(reconnect);
		else
		{
			reconnecting = reconnect;
			connected = false;
			st_tcp_socket_base<Socket>::graceful_close();
		}
	}

protected:
	virtual bool do_start() //connect or recv
	{
		if (!ST_THIS get_io_service().stopped())
		{
			if (!is_connected())
				boost::asio::async_connect(ST_THIS lowest_layer(), server_addr_iter,
					boost::bind(&st_connector_base::connect_handler, this, boost::asio::placeholders::error));
			else
				ST_THIS do_recv_msg();

			return true;
		}

		return false;
	}

	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_send_allowed() const {return is_connected() && st_tcp_socket_base<Socket>::is_send_allowed();}
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_close();}
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		unified_out::error_out("connection closed.");

		if (ST_THIS is_closing())
			force_close(reconnecting);
		else
		{
			force_close(reconnecting);
			if (ec && boost::asio::error::operation_aborted != ec && RE_CONNECT_CHECK)
				reconnecting = true;
		}

		if (reconnecting)
		{
			reconnecting = false;
			do_start();
		}
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
			return st_tcp_socket_base<Socket>::on_timer(id, user_data);
			break;
		}

		return false;
	}

	void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec)
		{
			connected = true;
			on_connect();
			ST_THIS send_msg(); //send msg buffer may have msgs, send them
			do_start();
		}
		else if (boost::asio::error::operation_aborted != ec && RE_CONNECT_CHECK && !ST_THIS get_io_service().stopped())
			ST_THIS set_timer(10, RE_CONNECT_INTERVAL, nullptr);
	}

#ifdef RE_CONNECT_CONTROL
	bool prepare_re_connect() {return 0 == re_connect_times ? false : (--re_connect_times, true);}
#endif

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
#ifdef RE_CONNECT_CONTROL
	size_t re_connect_times;
#endif

	boost::asio::ip::tcp::resolver::iterator server_addr_iter;
};
typedef st_connector_base<> st_connector;

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

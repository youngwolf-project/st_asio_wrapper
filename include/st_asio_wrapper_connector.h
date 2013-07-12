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

#include "st_asio_wrapper_socket.h"

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

class st_connector : public st_socket
{
public:
	st_connector(io_service& io_service_) : st_socket(io_service_), connected(false), reconnecting(false)
#ifdef RE_CONNECT_CONTROL
		, re_connect_times(-1)
#endif
		{set_server_addr(SERVER_PORT, SERVER_IP);}

	void set_server_addr(unsigned short port, const std::string& ip)
	{
		error_code ec;
		server_addr = tcp::endpoint(address::from_string(ip, ec), port); assert(!ec);
	}
#ifdef RE_CONNECT_CONTROL
	void set_re_connect_times(size_t times) {re_connect_times = times;}
#endif
	bool is_connected() const {return connected;}
	virtual void reset() {st_socket::reset();}
	virtual void start() //connect or recv
	{
		if (!get_io_service().stopped())
		{
			if (!is_connected())
				async_connect(server_addr, boost::bind(&st_connector::connect_handler, this, placeholders::error));
			else
				do_recv_msg();
		}
	}

	void disconnect(bool reconnect = false) {force_close(reconnect);}
	void force_close(bool reconnect = false) {reconnecting = reconnect; connected = false; st_socket::force_close();}
	void graceful_close(bool reconnect = false)
	{
		if (!is_connected())
			force_close(reconnect);
		else
		{
			reconnecting = reconnect;
			connected = false;
			st_socket::graceful_close();
		}
	}

protected:
	virtual void on_connect() {unified_out::info_out("connecting success.");}
	virtual bool is_send_allowed() {return is_connected() && st_socket::is_send_allowed();} //can send data or not
	virtual void on_unpack_error() {unified_out::info_out("can not unpack msg."); force_close();}
	virtual void on_recv_error(const error_code& ec)
	{
		unified_out::error_out("connection closed.");

		if (is_closing())
			force_close(reconnecting);
		else
		{
			force_close(reconnecting);
			if (ec && error::operation_aborted != ec && RE_CONNECT_CHECK)
				reconnecting = true;
		}

		if (reconnecting)
		{
			reconnecting = false;
			start();
		}
	}

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
		case 10:
			start();
			break;
		case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: //reserved
			break;
		default:
			return st_socket::on_timer(id, user_data);
			break;
		}

		return false;
	}

	void connect_handler(const error_code& ec)
	{
		if (!ec)
		{
			connected = true;
			on_connect();
			send_msg(); //send msg buffer may have msgs, send them
			start();
		}
		else if (error::operation_aborted != ec && RE_CONNECT_CHECK && !get_io_service().stopped())
			set_timer(10, RE_CONNECT_INTERVAL, nullptr);
	}

#ifdef RE_CONNECT_CONTROL
	bool prepare_re_connect() {return 0 == re_connect_times ? false : (--re_connect_times, true);}
#endif

protected:
	tcp::endpoint server_addr;
	bool connected;
	bool reconnecting;
#ifdef RE_CONNECT_CONTROL
	size_t re_connect_times;
#endif
};

} //namespace

#endif /* ST_ASIO_WRAPPER_CONNECTOR_H_ */

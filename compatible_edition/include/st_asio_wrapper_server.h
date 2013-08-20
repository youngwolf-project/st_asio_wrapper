/*
 * st_asio_wrapper_server.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVER_H_
#define ST_ASIO_WRAPPER_SERVER_H_

#include "st_asio_wrapper_server_socket.h"
#include "st_asio_wrapper_object_pool.h"

#ifndef SERVER_PORT
#define SERVER_PORT					5051
#endif

#ifndef ASYNC_ACCEPT_NUM
#define ASYNC_ACCEPT_NUM			1 //how many async_accept delivery concurrently
#endif

//in set_server_addr, if the ip is empty, TCP_DEFAULT_IP_VERSION will define the ip version,
//or, the ip version will be deduced by the ip address.
//tcp::v4() means ipv4 and tcp::v6() means ipv6.
#ifndef TCP_DEFAULT_IP_VERSION
#define TCP_DEFAULT_IP_VERSION tcp::v4()
#endif

namespace st_asio_wrapper
{

template<typename Socket = st_server_socket, typename Server = i_server>
class st_server_base : public Server, public st_object_pool<Socket>
{
public:
	st_server_base(st_service_pump& service_pump_) : st_object_pool<Socket>(service_pump_),
		acceptor(service_pump_) {set_server_addr(SERVER_PORT);}

	void set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = tcp::endpoint(TCP_DEFAULT_IP_VERSION, port);
		else
		{
			error_code ec;
			server_addr = tcp::endpoint(address::from_string(ip, ec), port); assert(!ec);
		}
	}

	virtual void init()
	{
		error_code ec;
		acceptor.open(server_addr.protocol(), ec); assert(!ec);
#ifndef NOT_REUSE_ADDRESS
		acceptor.set_option(tcp::acceptor::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("bind failed."); return;}
		acceptor.listen(tcp::acceptor::max_connections, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("listen failed."); return;}

		st_object_pool<Socket>::start();

		for (int i = 0; i < ASYNC_ACCEPT_NUM; ++i)
			start_next_accept();
	}
	virtual void uninit() {st_object_pool<Socket>::stop(); stop_listen(); close_all_client();}

	void stop_listen() {error_code ec; acceptor.cancel(ec); acceptor.close(ec);}
	bool is_listening() const {return acceptor.is_open();}

	//implement i_server's pure virtual functions
	virtual st_service_pump& get_service_pump() {return st_object_pool<Socket>::get_service_pump();}
	virtual const st_service_pump& get_service_pump() const {return st_object_pool<Socket>::get_service_pump();}
	virtual void del_client(const boost::shared_ptr<st_tcp_socket>& client_ptr)
	{
		if (ST_THIS del_object(dynamic_pointer_cast<Socket>(client_ptr)))
		{
			client_ptr->show_info("client:", "quit.");
			client_ptr->force_close();
		}
	}

	void close_all_client()
	{
		//do not use graceful_close() as client endpoint do,
		//because in this function, object_can_mutex has been locked,
		//graceful_close will wait until on_recv_error() been invoked,
		//in on_recv_error(), we need to lock object_can_mutex too(in del_object()), which made dead lock
		mutex::scoped_lock lock(ST_THIS object_can_mutex);
		for (BOOST_AUTO(iter, ST_THIS object_can.begin()); iter != ST_THIS object_can.end(); ++iter)
		{
			(*iter)->show_info("client:", "been closed.");
			(*iter)->force_close();
		}
	}

	boost::shared_ptr<Socket> create_client()
	{
		BOOST_AUTO(client_ptr, ST_THIS reuse_object());
		return client_ptr ? client_ptr : boost::make_shared<Socket>(boost::ref(*this));
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	virtual bool on_accept(const boost::shared_ptr<Socket>& client_ptr) {return true;}

protected:
	void start_next_accept()
	{
		BOOST_AUTO(client_ptr, create_client());
		acceptor.async_accept(*client_ptr, boost::bind(&st_server_base::accept_handler, this,
			placeholders::error, client_ptr));
	}

	bool add_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		if (st_object_pool<Socket>::add_object(client_ptr))
		{
			client_ptr->show_info("client:", "arrive.");
			return true;
		}

		return false;
	}

	void accept_handler(const error_code& ec, const boost::shared_ptr<Socket>& client_ptr)
	{
		if (!ec)
		{
			if (on_accept(client_ptr) && add_client(client_ptr))
				client_ptr->start();
			start_next_accept();
		}
		else
			stop_listen();
	}

protected:
	tcp::endpoint server_addr;
	tcp::acceptor acceptor;
};
typedef st_server_base<> st_server;

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_H_ */

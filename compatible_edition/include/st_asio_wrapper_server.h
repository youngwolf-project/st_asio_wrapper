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

#include "st_asio_wrapper_object_pool.h"

#ifndef ST_ASIO_SERVER_PORT
#define ST_ASIO_SERVER_PORT			5051
#elif ST_ASIO_SERVER_PORT <= 0
	#error server port must be bigger than zero.
#endif

#ifndef ST_ASIO_ASYNC_ACCEPT_NUM
#define ST_ASIO_ASYNC_ACCEPT_NUM	16 //how many async_accept delivery concurrently
#elif ST_ASIO_SERVER_PORT <= 0
	#error async accept number must be bigger than zero.
#endif

//in set_server_addr, if the IP is empty, ST_ASIO_TCP_DEFAULT_IP_VERSION will define the IP version, or the IP version will be deduced by the IP address.
//boost::asio::ip::tcp::v4() means ipv4 and boost::asio::ip::tcp::v6() means ipv6.
#ifndef ST_ASIO_TCP_DEFAULT_IP_VERSION
#define ST_ASIO_TCP_DEFAULT_IP_VERSION boost::asio::ip::tcp::v4()
#endif

namespace st_asio_wrapper
{

template<typename Socket, typename Pool = st_object_pool<Socket>, typename Server = i_server>
class st_server_base : public Server, public Pool
{
public:
	st_server_base(st_service_pump& service_pump_) : Pool(service_pump_), acceptor(service_pump_) {set_server_addr(ST_ASIO_SERVER_PORT);}
	template<typename Arg>
	st_server_base(st_service_pump& service_pump_, const Arg& arg) : Pool(service_pump_, arg), acceptor(service_pump_) {set_server_addr(ST_ASIO_SERVER_PORT);}

	bool set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = boost::asio::ip::tcp::endpoint(ST_ASIO_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
			if (ec)
				return false;

			server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_server_addr() const {return server_addr;}

	void stop_listen() {boost::system::error_code ec; acceptor.cancel(ec); acceptor.close(ec);}
	bool is_listening() const {return acceptor.is_open();}

	//implement i_server's pure virtual functions
	virtual st_service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const st_service_pump& get_service_pump() const {return Pool::get_service_pump();}
	virtual bool del_socket(const boost::shared_ptr<st_object>& socket_ptr)
	{
		BOOST_AUTO(raw_socket_ptr, boost::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		raw_socket_ptr->force_shutdown();
		return ST_THIS del_object(raw_socket_ptr);
	}
	//restore the invalid socket whose id is equal to id, if successful, socket_ptr's take_over function will be invoked,
	//you can restore the invalid socket to socket_ptr, everything is restorable except socket::next_layer_ (on the other
	//hand, restore socket::next_layer_ doesn't make any sense).
	virtual bool restore_socket(const boost::shared_ptr<st_object>& socket_ptr, boost::uint_fast64_t id)
	{
		BOOST_AUTO(raw_socket_ptr, boost::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		BOOST_AUTO(this_id, raw_socket_ptr->id());
		BOOST_AUTO(old_socket_ptr, ST_THIS change_object_id(raw_socket_ptr, id));
		if (old_socket_ptr)
		{
			assert(raw_socket_ptr->id() == old_socket_ptr->id());

			unified_out::info_out("object id " ST_ASIO_LLF " been reused, id " ST_ASIO_LLF " been discarded.", raw_socket_ptr->id(), this_id);
			raw_socket_ptr->take_over(old_socket_ptr);

			return true;
		}

		return false;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means putting the msg into st_socket's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//send message with sync mode
	TCP_BROADCAST_MSG(sync_broadcast_msg, sync_send_msg)
	TCP_BROADCAST_MSG(sync_broadcast_native_msg, sync_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function.
	void disconnect(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {ST_THIS do_something_to_all(boost::bind(&Socket::disconnect, _1));}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {ST_THIS do_something_to_all(boost::bind(&Socket::force_shutdown, _1));}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr, bool sync = false) {ST_THIS del_object(socket_ptr); socket_ptr->graceful_shutdown(sync);}
	void graceful_shutdown() {ST_THIS do_something_to_all(boost::bind(&Socket::graceful_shutdown, _1, false));} //parameter sync must be false (the default value), or dead lock will occur.

protected:
	virtual bool init()
	{
		boost::system::error_code ec;
		if (!acceptor.is_open()) {acceptor.open(server_addr.protocol(), ec); assert(!ec);} //user maybe has opened this acceptor (to set options for example)
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
		acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("bind failed."); return false;}
		acceptor.listen(boost::asio::ip::tcp::acceptor::max_connections, ec); assert(!ec);
		if (ec) {get_service_pump().stop(); unified_out::error_out("listen failed."); return false;}

		ST_THIS start();

		for (int i = 0; i < ST_ASIO_ASYNC_ACCEPT_NUM; ++i)
			start_next_accept();

		return true;
	}
	virtual void uninit() {ST_THIS stop(); stop_listen(); force_shutdown();}
	virtual bool on_accept(typename Pool::object_ctype& socket_ptr) {return true;}

	//if you want to ignore this error and continue to accept new connections immediately, return true in this virtual function;
	//if you want to ignore this error and continue to accept new connections after a specific delay, start a timer immediately and return false (don't call stop_listen()),
	// when the timer ends up, call start_next_accept() in the callback function.
	//otherwise, don't rewrite this virtual function or call st_server_base::on_accept_error() directly after your code.
	virtual bool on_accept_error(const boost::system::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (boost::asio::error::operation_aborted != ec)
		{
			unified_out::error_out("failed to accept new connection because of %s, will stop listening.", ec.message().data());
			stop_listen();
		}

		return false;
	}

	virtual void start_next_accept()
	{
		typename Pool::object_type socket_ptr = ST_THIS create_object(boost::ref(*this));
		acceptor.async_accept(socket_ptr->lowest_layer(), boost::bind(&st_server_base::accept_handler, this, boost::asio::placeholders::error, socket_ptr));
	}

protected:
	bool add_socket(typename Pool::object_ctype& socket_ptr)
	{
		if (ST_THIS add_object(socket_ptr))
		{
			socket_ptr->show_info("client:", "arrive.");
			return true;
		}

		socket_ptr->show_info("client:", "been refused because of too many clients.");
		socket_ptr->force_shutdown();
		return false;
	}

	void accept_handler(const boost::system::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (!ec)
		{
			if (on_accept(socket_ptr) && add_socket(socket_ptr))
				socket_ptr->start();

			start_next_accept();
		}
		else if (on_accept_error(ec, socket_ptr))
			start_next_accept();
	}

protected:
	boost::asio::ip::tcp::endpoint server_addr;
	boost::asio::ip::tcp::acceptor acceptor;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_H_ */

/*
 * server.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_SERVER_H_
#define ST_ASIO_SERVER_H_

#include "../object_pool.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Socket, typename Family = boost::asio::ip::tcp, typename Pool = object_pool<Socket>, typename Server = i_server>
class generic_server : public Server, public Pool
{
protected:
	generic_server(service_pump& service_pump_) : Pool(service_pump_), acceptor(service_pump_) {}
	template<typename Arg> generic_server(service_pump& service_pump_, const Arg& arg) : Pool(service_pump_, arg), acceptor(service_pump_) {}

public:
	bool set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = typename Family::endpoint(ST_ASIO_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
			BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec));
#else
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec));
#endif
			if (ec)
				return false;

			server_addr = boost::asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}
	bool set_server_addr(const std::string& file_name) {server_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_server_addr() const {return server_addr;}

	bool start_listen()
	{
		boost::lock_guard<boost::mutex> lock(mutex);
		if (is_listening())
			return false;

		boost::system::error_code ec;
		if (!acceptor.is_open()) {acceptor.open(server_addr.protocol(), ec); assert(!ec);} //user maybe has opened this acceptor (to set options for example)
#ifndef ST_ASIO_NOT_REUSE_ADDRESS
		acceptor.set_option(typename Family::acceptor::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {unified_out::error_out("bind failed."); return false;}

		int num = async_accept_num();
		assert(num > 0);
		if (num <= 0)
			num = 16;

		boost::container::list<typename Pool::object_type> sockets;
		unified_out::info_out("begin to pre-create %d server socket...", num);
		while (--num >= 0)
		{
			BOOST_AUTO(socket_ptr, create_object());
			if (!socket_ptr)
				break;

			sockets.push_back(socket_ptr);
		}
		if (num >= 0)
			unified_out::info_out("finished pre-creating server sockets, but failed %d time(s).", num + 1);
		else
			unified_out::info_out("finished pre-creating server sockets.");

#if BOOST_ASIO_VERSION >= 101100
		acceptor.listen(Family::acceptor::max_listen_connections, ec); assert(!ec);
#else
		acceptor.listen(Family::acceptor::max_connections, ec); assert(!ec);
#endif
		if (ec) {unified_out::error_out("listen failed."); return false;}

		st_asio_wrapper::do_something_to_all(sockets, boost::bind(&generic_server::do_async_accept, this, boost::placeholders::_1));
		return true;
	}
	bool is_listening() const {return acceptor.is_open();}
	void stop_listen() {boost::lock_guard<boost::mutex> lock(mutex); boost::system::error_code ec; acceptor.cancel(ec); acceptor.close(ec);}

	typename Family::acceptor& next_layer() {return acceptor;}
	const typename Family::acceptor& next_layer() const {return acceptor;}

	//implement i_server's pure virtual functions
	virtual bool started() const {return ST_THIS service_started();}
	virtual service_pump& get_service_pump() {return Pool::get_service_pump();}
	virtual const service_pump& get_service_pump() const {return Pool::get_service_pump();}

	virtual bool socket_exist(boost::uint_fast64_t id) {return ST_THIS exist(id);}
	virtual boost::shared_ptr<tracked_executor> find_socket(boost::uint_fast64_t id) {return ST_THIS find(id);}
	virtual bool del_socket(boost::uint_fast64_t id) {unified_out::error_out("use bool del_socket(const boost::shared_ptr<tracked_executor>&) please."); assert(false); return false;}

	virtual bool del_socket(const boost::shared_ptr<tracked_executor>& socket_ptr)
	{
		BOOST_AUTO(raw_socket_ptr, boost::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		raw_socket_ptr->force_shutdown();
		return ST_THIS del_object(raw_socket_ptr);
	}
	//restore the invalid socket whose id is equal to 'id', if successful, socket_ptr's take_over function will be invoked,
	//you can restore the invalid socket to socket_ptr, everything can be restored except socket::next_layer_ (on the other
	//hand, restore socket::next_layer_ doesn't make any sense).
	virtual bool restore_socket(const boost::shared_ptr<tracked_executor>& socket_ptr, boost::uint_fast64_t id, bool init)
	{
		BOOST_AUTO(raw_socket_ptr, boost::dynamic_pointer_cast<Socket>(socket_ptr));
		if (!raw_socket_ptr)
			return false;

		BOOST_AUTO(this_id, raw_socket_ptr->id());
		if (!init)
		{
			BOOST_AUTO(old_socket_ptr, ST_THIS change_object_id(raw_socket_ptr, id));
			if (old_socket_ptr)
			{
				assert(raw_socket_ptr->id() == old_socket_ptr->id());

				unified_out::info_out("object id " ST_ASIO_LLF " been reused, id " ST_ASIO_LLF " been discarded.", raw_socket_ptr->id(), this_id);
				raw_socket_ptr->take_over(old_socket_ptr);

				return true;
			}
		}
		else if (ST_THIS init_object_id(raw_socket_ptr, id))
		{
			unified_out::info_out("object id " ST_ASIO_LLF " been set to " ST_ASIO_LLF, this_id, raw_socket_ptr->id());
			return true;
		}

		return false;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means putting the msg into tcp::socket_base's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

	//functions with a socket_ptr parameter will remove the link from object pool first, then call corresponding function.
	void disconnect(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->disconnect();}
	void disconnect() {ST_THIS do_something_to_all(boost::bind(&Socket::disconnect, boost::placeholders::_1));}
	void force_shutdown(typename Pool::object_ctype& socket_ptr) {ST_THIS del_object(socket_ptr); socket_ptr->force_shutdown();}
	void force_shutdown() {ST_THIS do_something_to_all(boost::bind(&Socket::force_shutdown, boost::placeholders::_1));}
	void graceful_shutdown(typename Pool::object_ctype& socket_ptr, bool sync = false) {ST_THIS del_object(socket_ptr); socket_ptr->graceful_shutdown(sync);}
	void graceful_shutdown() {ST_THIS do_something_to_all(boost::bind(&Socket::graceful_shutdown, boost::placeholders::_1, false));}
	//for the last function, parameter sync must be false (the default value), or dead lock will occur.

protected:
	virtual int async_accept_num() {return ST_ASIO_ASYNC_ACCEPT_NUM;}
	virtual bool init() {return start_listen() ? (ST_THIS start(), true) : false;}
	virtual void uninit() {ST_THIS stop(); stop_listen(); force_shutdown();} //if you wanna graceful shutdown, call graceful_shutdown before service_pump::stop_service invocation.

	virtual bool on_accept(typename Pool::object_ctype& socket_ptr) {return true;}
	virtual void start_next_accept() {boost::lock_guard<boost::mutex> lock(mutex); do_async_accept(create_object());}

	//if you want to ignore this error and continue to accept new connections immediately, return true in this virtual function;
	//if you want to ignore this error and continue to accept new connections after a specific delay, start a timer immediately and return false
	// (don't call stop_listen()), after the timer exhausts, call start_next_accept() in the callback function.
	//otherwise, don't rewrite this virtual function or call generic_server::on_accept_error() directly after your code.
	virtual bool on_accept_error(const boost::system::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (boost::asio::error::operation_aborted != ec)
		{
			unified_out::error_out("failed to accept new connection because of %s, will stop listening.", ec.message().data());
			stop_listen();
		}

		return false;
	}

protected:
	typename Pool::object_type create_object() {return Pool::create_object(boost::ref(*this));}
	template<typename Arg> typename Pool::object_type create_object(Arg& arg) {return Pool::create_object(boost::ref(*this), arg);}

	bool add_socket(typename Pool::object_ctype& socket_ptr)
	{
		if (ST_THIS add_object(socket_ptr))
		{
			socket_ptr->show_info("client:", "arrive.");
			if (get_service_pump().is_service_started()) //service already started
				socket_ptr->start();

			return true;
		}

		socket_ptr->show_info("client:", "been refused because of too many clients or id conflict.");
		return false;
	}

private:
	void accept_handler(const boost::system::error_code& ec, typename Pool::object_ctype& socket_ptr)
	{
		if (!ec)
		{
			if (on_accept(socket_ptr))
				add_socket(socket_ptr);

			if (is_listening())
				start_next_accept();
		}
		else if (on_accept_error(ec, socket_ptr))
			start_next_accept();
	}

	void do_async_accept(typename Pool::object_ctype& socket_ptr)
		{if (socket_ptr) acceptor.async_accept(socket_ptr->lowest_layer(), boost::bind(&generic_server::accept_handler, this, boost::asio::placeholders::error, socket_ptr));}

private:
	typename Family::endpoint server_addr;
	typename Family::acceptor acceptor;
	boost::mutex mutex;
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class server_base : public generic_server<Socket, boost::asio::ip::tcp, Pool, Server>
{
private:
	typedef generic_server<Socket, boost::asio::ip::tcp, Pool, Server> super;

public:
	server_base(service_pump& service_pump_) : super(service_pump_) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT);}
	template<typename Arg> server_base(service_pump& service_pump_, const Arg& arg) : super(service_pump_, arg) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT);}
};

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template<typename Socket, typename Pool = object_pool<Socket>, typename Server = i_server>
class unix_server_base : public generic_server<Socket, boost::asio::local::stream_protocol, Pool, Server>
{
private:
	typedef generic_server<Socket, boost::asio::local::stream_protocol, Pool, Server> super;

public:
	unix_server_base(service_pump& service_pump_) : super(service_pump_) {ST_THIS set_server_addr("./st-unix-socket");}
};
#endif

}} //namespace

#endif /* ST_ASIO_SERVER_H_ */

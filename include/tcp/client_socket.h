/*
 * client_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at client endpoint
 */

#ifndef ST_ASIO_CLIENT_SOCKET_H_
#define ST_ASIO_CLIENT_SOCKET_H_

#include "socket.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Socket, typename Matrix = i_matrix, typename Family = boost::asio::ip::tcp> class generic_client_socket : public Socket
{
private:
	typedef Socket super;

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_CONNECT_DELAY = TIMER_BEGIN;
	static const typename super::tid TIMER_CONNECT = TIMER_BEGIN + 1;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

	static bool set_addr(boost::asio::ip::tcp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = boost::asio::ip::tcp::endpoint(ST_ASIO_TCP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
			BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec)); assert(!ec);
#else
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec)); assert(!ec);
#endif
			if (ec)
			{
				unified_out::error_out("invalid IP address %s.", ip.data());
				return false;
			}

			endpoint = boost::asio::ip::tcp::endpoint(addr, port);
		}

		return true;
	}

protected:
	generic_client_socket(boost::asio::io_context& io_context_) : super(io_context_) {first_init();}
	template<typename Arg> generic_client_socket(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {first_init();}

	generic_client_socket(Matrix& matrix_) : super(matrix_.get_service_pump()) {first_init(&matrix_);}
	template<typename Arg> generic_client_socket(Matrix& matrix_, Arg& arg) : super(matrix_.get_service_pump(), arg) {first_init(&matrix_);}

	~generic_client_socket() {ST_THIS clear_io_context_refs();}

public:
	virtual const char* type_name() const {return "TCP (client endpoint)";}
	virtual int type_id() const {return 1;}

	virtual bool obsoleted() {return !need_reconnect && super::obsoleted();}
	virtual void reset() {need_reconnect = ST_ASIO_RECONNECT; super::reset();}

#ifdef _MSC_VER
	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP) {return set_addr(server_addr, port, ip.empty() ? ST_ASIO_SERVER_IP : ip);}
#else
	bool set_server_addr(unsigned short port, const std::string& ip = ST_ASIO_SERVER_IP) {return set_addr(server_addr, port, ip);}
#endif
	bool set_server_addr(const std::string& file_name) {server_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_server_addr() const {return server_addr;}

	//if you don't want to reconnect to the server after link broken, define macro ST_ASIO_RECONNECT as false, call set_reconnect(false) in on_connect()
	// or rewrite after_close() virtual function and do nothing in it.
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	//disconnect(bool), force_shutdown(bool) and graceful_shutdown(bool) can overwrite reconnecting behavior, please note.
	//reset() virtual function will set reconnecting behavior according to macro ST_ASIO_RECONNECT, please note.
	//if prepare_reconnect returns negative value, reconnecting will be closed, please note.
	void set_reconnect(bool reconnect) {need_reconnect = reconnect;}
	bool is_reconnect() const {return need_reconnect;}

	//if the connection is broken unexpectedly, generic_client_socket will try to reconnect to the server automatically (if need_reconnect is true).
	//these functions are not thread safe, please note.
	void disconnect(bool reconnect = false) {force_shutdown(reconnect);}
	void force_shutdown(bool reconnect = false)
	{
		need_reconnect = reconnect;

		if (reconnect && ST_THIS is_broken() && !ST_THIS started())
			return ST_THIS start();
		else if (super::FORCE_SHUTTING_DOWN != ST_THIS status)
			ST_THIS show_info("client link:", "been shut down.");

		super::force_shutdown();
	}

	//this function is not thread safe, please note.
	void graceful_shutdown(bool reconnect = false)
	{
		need_reconnect = reconnect;

		if (reconnect && ST_THIS is_broken() && !ST_THIS started())
			return ST_THIS start();
		else if (ST_THIS is_broken())
			return force_shutdown(reconnect);
		else if (!ST_THIS is_shutting_down())
			ST_THIS show_info("client link:", "being shut down gracefully.");

		super::graceful_shutdown();
	}

protected:
	//helper function, just call it in constructor
	void first_init(Matrix* matrix_ = NULL) {need_reconnect = ST_ASIO_RECONNECT; matrix = matrix_;}

	Matrix* get_matrix() {return matrix;}
	const Matrix* get_matrix() const {return matrix;}

	virtual bool do_start() //connect
	{
		assert(!ST_THIS is_connected());
		return bind() && ST_THIS set_timer(TIMER_CONNECT, 50, (boost::lambda::bind(&generic_client_socket::connect, this, true), false));
	}

	virtual void connect_handler(const boost::system::error_code& ec)
	{
		if (!ec) //already started, so cannot call start()
			super::do_start();
		else
			ST_THIS set_timer(TIMER_CONNECT_DELAY, 50, boost::lambda::bind(&generic_client_socket::check_and_reconnect, this, ec));
			//prepare_next_reconnect(ec);
			//do not call prepare_next_reconnect directly at here, otherwise, there's a posibility that the next set_timer (reconnecting failed immediately)
			// be called concurrently with the callback of this timer.
			//for the same timer in the same timer object, any manipulations are not thread safe ---st_asio_wrapper::timer
	}

	//after how much time (ms), generic_client_socket will try to reconnect the server, negative value means give up.
	virtual int prepare_reconnect(const boost::system::error_code& ec) {return ST_ASIO_RECONNECT_INTERVAL;}
	virtual void on_connect() {unified_out::info_out(ST_ASIO_LLF " connecting success.", ST_THIS id());}
	virtual void on_unpack_error() {unified_out::info_out(ST_ASIO_LLF " can not unpack msg.", ST_THIS id()); ST_THIS unpacker()->dump_left_data(); force_shutdown(need_reconnect);}
	virtual void on_recv_error(const boost::system::error_code& ec) {ST_THIS show_info(ec, "client link:", "broken/been shut down"); force_shutdown(need_reconnect);}
	virtual void on_async_shutdown_error() {force_shutdown(need_reconnect);}
	virtual bool on_heartbeat_error() {ST_THIS show_info("client link:", "broke unexpectedly."); force_shutdown(need_reconnect); return false;}

	virtual void on_close()
	{
		if (!need_reconnect)
		{
			ST_THIS clear_io_context_refs();
#ifndef ST_ASIO_CLEAR_OBJECT_INTERVAL
			if (NULL != matrix)
				matrix->del_socket(ST_THIS id());
#endif
		}

		super::on_close();
	}
	//reconnect at here rather than in on_recv_error to make sure no async invocations performed on this socket before reconnecting.
	//if you don't want to reconnect the server after link broken, rewrite this virtual function and do nothing in it or call set_reconnect(false).
	//if you want to control the retry times and delay time after reconnecting failed, rewrite prepare_reconnect virtual function.
	virtual void after_close() {if (need_reconnect) ST_THIS start();}

	virtual bool bind() {return true;}

private:
	//call following two functions (via timer object's add_io_context_refs, sub_io_context_refs or clear_io_context_refs) in:
	// 1. on_xxxx callbacks on this object;
	// 2. use this->post or this->set_timer to emit an async event, then in the callback.
	//otherwise, you must protect them to not be called with reset and on_close simultaneously
	//actually, you're recommended to not use them, use add_socket instead.
	virtual void attach_io_context(boost::asio::io_context& io_context_, unsigned refs) {if (NULL != matrix) matrix->get_service_pump().assign_io_context(io_context_, refs);}
	virtual void detach_io_context(boost::asio::io_context& io_context_, unsigned refs) {if (NULL != matrix) matrix->get_service_pump().return_io_context(io_context_, refs);}

	bool connect(bool first)
	{
		if (!first && !bind())
			return false;

		ST_THIS lowest_layer().async_connect(server_addr, ST_THIS make_handler_error(boost::bind(&generic_client_socket::connect_handler, this, boost::asio::placeholders::error)));
		return true;
	}

	bool check_and_reconnect(const boost::system::error_code& ec) {return ST_THIS is_timer(TIMER_CONNECT) ? true : (prepare_next_reconnect(ec), false);}
	bool prepare_next_reconnect(const boost::system::error_code& ec)
	{
		if (need_reconnect && ST_THIS started() && !ST_THIS stopped())
		{
#ifdef _WIN32
			if (boost::asio::error::connection_refused != ec && boost::asio::error::network_unreachable != ec && boost::asio::error::timed_out != ec)
#endif
			{
				boost::system::error_code ec;
				ST_THIS lowest_layer().close(ec);
			}

			int delay = prepare_reconnect(ec);
			if (delay < 0)
				need_reconnect = false;
			else if (ST_THIS set_timer(TIMER_CONNECT, delay, (boost::lambda::bind(&generic_client_socket::connect, this, false), false)))
				return true;
		}

		unified_out::info_out(ST_ASIO_LLF " reconnecting abandoned.", ST_THIS id());
		super::force_shutdown();
		return false;
	}

private:
	bool need_reconnect;
	typename Family::endpoint server_addr;

	Matrix* matrix;
};

template<typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class client_socket_base : public generic_client_socket<socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix>
{
private:
	typedef generic_client_socket<socket_base<Socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix> super;

public:
	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}
	template<typename Arg>
	client_socket_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	client_socket_base(Matrix& matrix_) : super(matrix_) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}
	template<typename Arg> client_socket_base(Matrix& matrix_, Arg& arg) : super(matrix_, arg) {ST_THIS set_server_addr(ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return super::set_addr(local_addr, port, ip);}
	const boost::asio::ip::tcp::endpoint& get_local_addr() const {return local_addr;}

protected:
	virtual bool bind()
	{
		if (0 != local_addr.port() || !local_addr.address().is_unspecified())
		{
			BOOST_AUTO(&lowest_object, ST_THIS lowest_layer());

			boost::system::error_code ec;
			if (!lowest_object.is_open()) //user maybe has opened this socket (to set options for example)
			{
				lowest_object.open(local_addr.protocol(), ec); assert(!ec);
				if (ec)
				{
					unified_out::error_out("cannot create socket: %s", ec.message().data());
					return false;
				}
			}

			lowest_object.bind(local_addr, ec);
			if (ec && boost::asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		return true;
	}

private:
	boost::asio::ip::tcp::endpoint local_addr;
};

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class unix_client_socket_base : public generic_client_socket<socket_base<boost::asio::local::stream_protocol::socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix, boost::asio::local::stream_protocol>
{
private:
	typedef generic_client_socket<socket_base<boost::asio::local::stream_protocol::socket, Packer, Unpacker, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>, Matrix, boost::asio::local::stream_protocol> super;

public:
	unix_client_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {ST_THIS set_server_addr("./st-unix-socket");}
	unix_client_socket_base(Matrix& matrix_) : super(matrix_) {ST_THIS set_server_addr("./st-unix-socket");}
};
#endif

}} //namespace

#endif /* ST_ASIO_CLIENT_SOCKET_H_ */

/*
 * tcp.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * TCP related conveniences.
 */

#ifndef ST_ASIO_EXT_TCP_H_
#define ST_ASIO_EXT_TCP_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/client_socket.h"
#include "../tcp/proxy/socks.h"
#include "../tcp/client.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"
#include "../single_service_pump.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer<>
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER st_asio_wrapper::ext::unpacker<>
#endif

namespace st_asio_wrapper { namespace ext { namespace tcp {

typedef st_asio_wrapper::tcp::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
template<typename Matrix = i_matrix>
class client_socket2 : public st_asio_wrapper::tcp::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
{
private:
	typedef st_asio_wrapper::tcp::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

public:
	client_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
	client_socket2(Matrix& matrix_) : super(matrix_) {}
};
typedef client_socket connector;
typedef st_asio_wrapper::tcp::single_client_base<client_socket> single_client;
typedef st_asio_wrapper::tcp::multi_client_base<client_socket> multi_client;
template<typename Socket, typename Matrix = i_matrix>
class multi_client2 : public st_asio_wrapper::tcp::multi_client_base<Socket, object_pool<Socket>, Matrix>
{
private:
	typedef st_asio_wrapper::tcp::multi_client_base<Socket, object_pool<Socket>, Matrix> super;

public:
	multi_client2(service_pump& service_pump_) : super(service_pump_) {}
};
typedef multi_client client;

typedef st_asio_wrapper::tcp::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
template <typename Server = st_asio_wrapper::tcp::i_server>
class server_socket2 : public st_asio_wrapper::tcp::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server>
{
private:
	typedef st_asio_wrapper::tcp::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server> super;

public:
	server_socket2(Server& server_) : super(server_) {}
	template<typename Arg> server_socket2(Server& server_, Arg& arg) : super(server_, arg) {}
};
typedef st_asio_wrapper::tcp::server_base<server_socket> server;
template<typename Socket, typename Server = st_asio_wrapper::tcp::i_server>
class server2 : public st_asio_wrapper::tcp::server_base<Socket, object_pool<Socket>, Server>
{
private:
	typedef st_asio_wrapper::tcp::server_base<Socket, object_pool<Socket>, Server> super;

public:
	server2(service_pump& service_pump_) : super(service_pump_) {}
};

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
typedef st_asio_wrapper::tcp::unix_client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> unix_client_socket;
template<typename Matrix = i_matrix>
class unix_client_socket2 : public st_asio_wrapper::tcp::unix_client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
{
private:
	typedef st_asio_wrapper::tcp::unix_client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

public:
	unix_client_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
	unix_client_socket2(Matrix& matrix_) : super(matrix_) {}
};
typedef st_asio_wrapper::tcp::single_client_base<unix_client_socket> unix_single_client;
typedef st_asio_wrapper::tcp::multi_client_base<unix_client_socket> unix_multi_client;
//typedef multi_client2 unix_multi_client2; //multi_client2 can be used for unix socket too, but we cannot typedef it.

typedef st_asio_wrapper::tcp::unix_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> unix_server_socket;
template<typename Server = st_asio_wrapper::tcp::i_server>
class unix_server_socket2 : public st_asio_wrapper::tcp::unix_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server>
{
private:
	typedef st_asio_wrapper::tcp::unix_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server> super;

public:
	unix_server_socket2(Server& server_) : super(server_) {}
	template<typename Arg> unix_server_socket2(Server& server_, Arg& arg) : super(server_, arg) {}
};
typedef st_asio_wrapper::tcp::unix_server_base<unix_server_socket> unix_server;
template<typename Socket, typename Server = st_asio_wrapper::tcp::i_server>
class unix_server2 : public st_asio_wrapper::tcp::unix_server_base<Socket, object_pool<Socket>, Server>
{
private:
	typedef st_asio_wrapper::tcp::unix_server_base<Socket, object_pool<Socket>, Server> super;

public:
	unix_server2(service_pump& service_pump_) : super(service_pump_) {}
};
#endif

namespace proxy {

namespace socks4 {
	typedef st_asio_wrapper::tcp::proxy::socks4::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
	template<typename Matrix = i_matrix>
	class client_socket2 : public st_asio_wrapper::tcp::proxy::socks4::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
	{
	private:
		typedef st_asio_wrapper::tcp::proxy::socks4::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

	public:
		client_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
		client_socket2(Matrix& matrix_) : super(matrix_) {}
	};
	typedef client_socket connector;
	typedef st_asio_wrapper::tcp::single_client_base<client_socket> single_client;
	typedef st_asio_wrapper::tcp::multi_client_base<client_socket> multi_client;
	//typedef st_asio_wrapper::ext::tcp::multi_client2 multi_client2; //multi_client2 can be used for socks4 too, but we cannot typedef it.
	typedef multi_client client;
}

namespace socks5 {
	typedef st_asio_wrapper::tcp::proxy::socks5::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
	template<typename Matrix = i_matrix>
	class client_socket2 : public st_asio_wrapper::tcp::proxy::socks5::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
	{
	private:
		typedef st_asio_wrapper::tcp::proxy::socks5::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

	public:
		client_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
		client_socket2(Matrix& matrix_) : super(matrix_) {}
	};
	typedef client_socket connector;
	typedef st_asio_wrapper::tcp::single_client_base<client_socket> single_client;
	typedef st_asio_wrapper::tcp::multi_client_base<client_socket> multi_client;
	//typedef st_asio_wrapper::ext::tcp::multi_client2 multi_client2; //multi_client2 can be used for socks5 too, but we cannot typedef it.
	typedef multi_client client;
}

}

}}} //namespace

#endif /* ST_ASIO_EXT_TCP_H_ */

/*
 * websocket.h
 *
 *  Created on: 2022-5-27
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * websocket related conveniences.
 */

#ifndef ST_ASIO_EXT_WEBSOCKET_H_
#define ST_ASIO_EXT_WEBSOCKET_H_

#include "packer.h"
#include "../tcp/websocket/websocket.h"
#include "../single_service_pump.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer<>
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER st_asio_wrapper::dummy_unpacker<std::string>
#endif

namespace st_asio_wrapper { namespace ext { namespace websocket {

typedef st_asio_wrapper::websocket::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
template<typename Matrix = i_matrix>
class client_socket2 : public st_asio_wrapper::websocket::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
{
private:
	typedef st_asio_wrapper::websocket::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

public:
	client_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
	template<typename Arg> client_socket2(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {}

	client_socket2(Matrix& matrix_) : super(matrix_) {}
	template<typename Arg> client_socket2(Matrix& matrix_, Arg& arg) : super(matrix_, arg) {}
};
typedef client_socket connector;
typedef st_asio_wrapper::websocket::single_client_base<client_socket> single_client;
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

typedef st_asio_wrapper::websocket::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
template<typename Server = st_asio_wrapper::tcp::i_server>
class server_socket2 : public st_asio_wrapper::websocket::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server>
{
private:
	typedef st_asio_wrapper::websocket::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server> super;

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

}}} //namespace

#endif /* ST_ASIO_EXT_WEBSOCKET_H_ */

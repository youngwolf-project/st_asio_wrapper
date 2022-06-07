/*
 * ssl.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * ssl related conveniences.
 */

#ifndef ST_ASIO_EXT_SSL_H_
#define ST_ASIO_EXT_SSL_H_

#include "packer.h"
#include "unpacker.h"
#include "../tcp/ssl/ssl.h"
#include "../single_service_pump.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer<>
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER st_asio_wrapper::ext::unpacker<>
#endif

namespace st_asio_wrapper { namespace ext { namespace ssl {

typedef st_asio_wrapper::ssl::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
template<typename Matrix = i_matrix>
class client_socket2 : public st_asio_wrapper::ssl::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>
{
private:
	typedef st_asio_wrapper::ssl::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix> super;

public:
	client_socket2(boost::asio::io_context& io_context_, boost::asio::ssl::context& ctx_) : super(io_context_, ctx_) {}
	client_socket2(Matrix& matrix_, boost::asio::ssl::context& ctx_) : super(matrix_, ctx_) {}
};
typedef client_socket connector;
typedef st_asio_wrapper::ssl::single_client_base<client_socket> single_client;
typedef st_asio_wrapper::ssl::multi_client_base<client_socket> multi_client;
template<typename Socket, typename Matrix = i_matrix>
class multi_client2 : public st_asio_wrapper::ssl::multi_client_base<Socket, st_asio_wrapper::ssl::object_pool<Socket>, Matrix>
{
private:
	typedef st_asio_wrapper::ssl::multi_client_base<Socket, st_asio_wrapper::ssl::object_pool<Socket>, Matrix> super;

public:
	multi_client2(service_pump& service_pump_, const boost::asio::ssl::context::method& m) : super(service_pump_, m) {}
};
typedef multi_client client;

typedef st_asio_wrapper::ssl::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
template<typename Server = st_asio_wrapper::tcp::i_server>
class server_socket2 : public st_asio_wrapper::ssl::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server>
{
private:
	typedef st_asio_wrapper::ssl::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server> super;

public:
	server_socket2(Server& server_, boost::asio::ssl::context& ctx) : super(server_, ctx) {}
};
typedef st_asio_wrapper::ssl::server_base<server_socket> server;
template<typename Socket, typename Server = st_asio_wrapper::tcp::i_server>
class server2 : public st_asio_wrapper::ssl::server_base<Socket, st_asio_wrapper::ssl::object_pool<Socket>, Server>
{
private:
	typedef st_asio_wrapper::ssl::server_base<Socket, st_asio_wrapper::ssl::object_pool<Socket>, Server> super;

public:
	server2(service_pump& service_pump_, const boost::asio::ssl::context::method& m) : super(service_pump_, m) {}
};

}}} //namespace

#endif /* ST_ASIO_EXT_SSL_H_ */

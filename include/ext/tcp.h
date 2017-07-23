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
#include "../tcp/client.h"
#include "../tcp/server_socket.h"
#include "../tcp/server.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER st_asio_wrapper::ext::unpacker
#endif

namespace st_asio_wrapper { namespace ext { namespace tcp {

typedef st_asio_wrapper::tcp::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
typedef client_socket connector;
typedef st_asio_wrapper::tcp::single_client_base<client_socket> single_client;
typedef st_asio_wrapper::tcp::multi_client_base<client_socket> multi_client;
typedef multi_client client;

typedef st_asio_wrapper::tcp::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
typedef st_asio_wrapper::tcp::server_base<server_socket> server;

}}} //namespace

#endif /* ST_ASIO_EXT_TCP_H_ */

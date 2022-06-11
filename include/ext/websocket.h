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
template<typename Matrix = i_matrix> using client_socket2 = st_asio_wrapper::websocket::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Matrix>;
typedef client_socket connector;
typedef st_asio_wrapper::tcp::single_client_base<client_socket> single_client;
typedef st_asio_wrapper::tcp::multi_client_base<client_socket> multi_client;
template<typename Socket, typename Matrix = i_matrix> using multi_client2 = st_asio_wrapper::tcp::multi_client_base<Socket, object_pool<Socket>, Matrix>;
typedef multi_client client;

typedef st_asio_wrapper::websocket::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
template<typename Server = st_asio_wrapper::tcp::i_server> using server_socket2 = st_asio_wrapper::websocket::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, Server>;
typedef st_asio_wrapper::tcp::server_base<server_socket> server;
template<typename Socket, typename Server = st_asio_wrapper::tcp::i_server> using server2 = st_asio_wrapper::tcp::server_base<Socket, object_pool<Socket>, Server>;

}}} //namespace

#endif /* ST_ASIO_EXT_WEBSOCKET_H_ */

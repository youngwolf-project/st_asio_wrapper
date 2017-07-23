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

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER st_asio_wrapper::ext::unpacker
#endif

namespace st_asio_wrapper { namespace ext { namespace ssl {

typedef st_asio_wrapper::ssl::client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> client_socket;
typedef client_socket connector;
typedef st_asio_wrapper::ssl::single_client_base<client_socket> single_client;
typedef st_asio_wrapper::ssl::multi_client_base<client_socket> multi_client;
typedef multi_client client;

typedef st_asio_wrapper::ssl::server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> server_socket;
typedef st_asio_wrapper::ssl::server_base<server_socket> server;

}}} //namespace

#endif /* ST_ASIO_EXT_SSL_H_ */

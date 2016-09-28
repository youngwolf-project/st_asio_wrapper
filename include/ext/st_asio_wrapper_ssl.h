/*
 * st_asio_wrapper_ssl.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * ssl related conveniences.
 */

#ifndef ST_ASIO_WRAPPER_EXT_SSL_H_
#define ST_ASIO_WRAPPER_EXT_SSL_H_

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"
#include "../st_asio_wrapper_ssl.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER unpacker
#endif

namespace st_asio_wrapper { namespace ext {

typedef st_ssl_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_ssl_server_socket;
typedef st_ssl_server_base<st_ssl_server_socket> st_ssl_server;

typedef st_ssl_connector_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_ssl_connector;
typedef st_sclient<st_ssl_connector> st_ssl_tcp_sclient;
typedef st_tcp_client_base<st_ssl_connector, st_ssl_object_pool<st_ssl_connector> > st_ssl_tcp_client;

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_SSL_H_ */

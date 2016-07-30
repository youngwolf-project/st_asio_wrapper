/*
 * st_asio_wrapper_ext.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * extensional, replaceable and indispensable components.
 */

#ifndef ST_ASIO_WRAPPER_EXT_H_
#define ST_ASIO_WRAPPER_EXT_H_

#include "../st_asio_wrapper_base.h"
#include "../st_asio_wrapper_client.h"
#include "../st_asio_wrapper_tcp_client.h"
#include "../st_asio_wrapper_udp_client.h"
#include "../st_asio_wrapper_server.h"

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER unpacker
#endif

#ifndef ST_ASIO_DEFAULT_UDP_UNPACKER
#define ST_ASIO_DEFAULT_UDP_UNPACKER udp_unpacker
#endif

namespace st_asio_wrapper { namespace ext {

typedef st_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_server_socket;
typedef st_server_base<st_server_socket> st_server;

typedef st_connector_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_connector;
typedef st_sclient<st_connector> st_tcp_sclient;
typedef st_tcp_client_base<st_connector> st_tcp_client;

typedef st_udp_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER> st_udp_socket;
typedef st_sclient<st_udp_socket> st_udp_sclient;
typedef st_udp_client_base<st_udp_socket> st_udp_client;

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_H_ */

/*
 * st_asio_wrapper_udp.h
 *
 *  Created on: 2016-9-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * udp related conveniences.
 */

#ifndef ST_ASIO_WRAPPER_EXT_UDP_H_
#define ST_ASIO_WRAPPER_EXT_UDP_H_

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"
#include "../st_asio_wrapper_udp_socket.h"
#include "../st_asio_wrapper_udp_client.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER packer
#endif

#ifndef ST_ASIO_DEFAULT_UDP_UNPACKER
#define ST_ASIO_DEFAULT_UDP_UNPACKER udp_unpacker
#endif

namespace st_asio_wrapper { namespace ext {

typedef st_udp_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER> st_udp_socket;
typedef st_udp_sclient_base<st_udp_socket> st_udp_sclient;
typedef st_udp_client_base<st_udp_socket> st_udp_client;

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_UDP_H_ */

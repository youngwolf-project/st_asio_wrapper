/*
 * st_asio_wrapper_server.h
 *
 *  Created on: 2016-9-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * server related conveniences.
 */

#ifndef ST_ASIO_WRAPPER_EXT_SERVER_H_
#define ST_ASIO_WRAPPER_EXT_SERVER_H_

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"
#include "../st_asio_wrapper_server_socket.h"
#include "../st_asio_wrapper_server.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER unpacker
#endif

namespace st_asio_wrapper { namespace ext {

typedef st_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_server_socket;
typedef st_server_base<st_server_socket> st_server;

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_SERVER_H_ */

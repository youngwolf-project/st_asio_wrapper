/*
 * st_asio_wrapper_client.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * client related conveniences.
 */

#ifndef ST_ASIO_WRAPPER_EXT_CLIENT_H_
#define ST_ASIO_WRAPPER_EXT_CLIENT_H_

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"
#include "../st_asio_wrapper_connector.h"
#include "../st_asio_wrapper_tcp_client.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER packer
#endif

#ifndef ST_ASIO_DEFAULT_UNPACKER
#define ST_ASIO_DEFAULT_UNPACKER unpacker
#endif

namespace st_asio_wrapper { namespace ext {

typedef st_connector_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> st_connector;
typedef st_tcp_sclient_base<st_connector> st_tcp_sclient;
typedef st_tcp_client_base<st_connector> st_tcp_client;

}} //namespace

#endif /* ST_ASIO_WRAPPER_EXT_CLIENT_H_ */

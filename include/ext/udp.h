/*
 * udp.h
 *
 *  Created on: 2016-9-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * udp related conveniences.
 */

#ifndef ST_ASIO_EXT_UDP_H_
#define ST_ASIO_EXT_UDP_H_

#include "packer.h"
#include "unpacker.h"
#include "../udp/socket.h"
#include "../udp/socket_service.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer
#endif

#ifndef ST_ASIO_DEFAULT_UDP_UNPACKER
#define ST_ASIO_DEFAULT_UDP_UNPACKER st_asio_wrapper::ext::udp_unpacker
#endif

namespace st_asio_wrapper { namespace ext { namespace udp {

typedef st_asio_wrapper::udp::socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER> socket;
typedef st_asio_wrapper::udp::single_service_base<socket> single_service;
typedef st_asio_wrapper::udp::multi_service_base<socket> multi_service;
typedef multi_service service;

}}} //namespace

#endif /* ST_ASIO_EXT_UDP_H_ */

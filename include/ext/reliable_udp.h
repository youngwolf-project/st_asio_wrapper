/*
 * reliable_udp.h
 *
 *  Created on: 2021-9-3
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * reliable udp related conveniences.
 */

#ifndef ST_ASIO_EXT_RELIABLE_UDP_H_
#define ST_ASIO_EXT_RELIABLE_UDP_H_

#include "packer.h"
#include "unpacker.h"
#include "../udp/reliable_socket.h"
#include "../udp/socket_service.h"
#include "../single_service_pump.h"

#ifndef ST_ASIO_DEFAULT_PACKER
#define ST_ASIO_DEFAULT_PACKER st_asio_wrapper::ext::packer<>
#endif

#ifndef ST_ASIO_DEFAULT_UDP_UNPACKER
#define ST_ASIO_DEFAULT_UDP_UNPACKER st_asio_wrapper::ext::udp_unpacker
#endif

namespace st_asio_wrapper { namespace ext { namespace udp {

typedef st_asio_wrapper::udp::reliable_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER> reliable_socket;
typedef st_asio_wrapper::udp::single_socket_service_base<reliable_socket> single_reliable_socket_service;
typedef st_asio_wrapper::udp::multi_socket_service_base<reliable_socket> multi_reliable_socket_service;

}}} //namespace

#endif /* ST_ASIO_EXT_RELIABLE_UDP_H_ */

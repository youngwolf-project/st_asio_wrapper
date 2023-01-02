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
template<typename Matrix = i_matrix>
class reliable_socket2 : public st_asio_wrapper::udp::reliable_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER, Matrix>
{
private:
	typedef st_asio_wrapper::udp::reliable_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UDP_UNPACKER, Matrix> super;

public:
	reliable_socket2(boost::asio::io_context& io_context_) : super(io_context_) {}
	reliable_socket2(Matrix& matrix_) : super(matrix_) {}
};
typedef st_asio_wrapper::udp::single_socket_service_base<reliable_socket> single_reliable_socket_service;
typedef st_asio_wrapper::udp::multi_socket_service_base<reliable_socket> multi_reliable_socket_service;
template<typename Socket, typename Matrix = i_matrix>
class multi_reliable_socket_service2 : public st_asio_wrapper::udp::multi_socket_service_base<Socket, object_pool<Socket>, Matrix>
{
private:
	typedef st_asio_wrapper::udp::multi_socket_service_base<Socket, object_pool<Socket>, Matrix> super;

public:
	multi_reliable_socket_service2(service_pump& service_pump_) : super(service_pump_) {}
};
typedef multi_reliable_socket_service reliable_socket_service;

}}} //namespace

#endif /* ST_ASIO_EXT_RELIABLE_UDP_H_ */

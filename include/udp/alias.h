/*
* alias.h
*
*  Created on: 2017-7-17
*      Author: youngwolf
*		email: mail2tao@163.com
*		QQ: 676218192
*		Community on QQ: 198941541
*
* some alias, they are deprecated.
*/

#ifndef ST_ASIO_UDP_ALIAS_H_
#define ST_ASIO_UDP_ALIAS_H_

#include "socket_service.h"

namespace st_asio_wrapper { namespace udp {

template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> class service_base : public multi_socket_service_base<Socket, Pool, Matrix>
{
public:
	service_base(service_pump& service_pump_) : multi_socket_service_base<Socket, Pool, Matrix>(service_pump_) {}
};

}} //namespace

#endif /* ST_ASIO_UDP_ALIAS_H_ */

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

template<typename Socket, typename Pool = object_pool<Socket> > class service_base : public multi_service_base<Socket, Pool>
{
public:
	service_base(service_pump& service_pump_) : multi_service_base<Socket, Pool>(service_pump_) {}
};

}} //namespace

#endif /* ST_ASIO_UDP_ALIAS_H_ */

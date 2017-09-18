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

#ifndef ST_ASIO_SSL_ALIAS_H_
#define ST_ASIO_SSL_ALIAS_H_

#include "ssl.h"

namespace st_asio_wrapper { namespace ssl {

template <typename Packer, typename Unpacker, typename Socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>,
	template<typename, typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename, typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class connector_base : public client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef client_socket_base<Packer, Unpacker, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	connector_base(boost::asio::io_context& io_context_, boost::asio::ssl::context& ctx) : super(io_context_, ctx) {}
};

template<typename Socket, typename Pool = object_pool<Socket> > class client_base : public multi_client_base<Socket, Pool>
{
public:
	client_base(service_pump& service_pump_, boost::asio::ssl::context::method m) : multi_client_base<Socket, Pool>(service_pump_, m) {}
};

}} //namespace

#endif /* ST_ASIO_SSL_ALIAS_H_ */

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

#ifndef ST_ASIO_TCP_ALIAS_H_
#define ST_ASIO_TCP_ALIAS_H_

#include "client_socket.h"
#include "client.h"

namespace st_asio_wrapper { namespace tcp {

template<typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER,
	template<typename, typename> class ReaderWriter = reader_writer>
class connector_base : public client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter>
{
private:
	typedef client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer, ReaderWriter> super;

public:
	connector_base(boost::asio::io_context& io_context_) : super(io_context_) {}
	template<typename Arg> connector_base(boost::asio::io_context& io_context_, Arg& arg) : super(io_context_, arg) {}

	connector_base(Matrix& matrix_) : super(matrix_) {}
	template<typename Arg> connector_base(Matrix& matrix_, Arg& arg) : super(matrix_, arg) {}
};

template<typename Socket, typename Pool = object_pool<Socket>, typename Matrix = i_matrix> class client_base : public multi_client_base<Socket, Pool, Matrix>
{
public:
	client_base(service_pump& service_pump_) : multi_client_base<Socket, Pool, Matrix>(service_pump_) {}
};

}} //namespace

#endif /* ST_ASIO_TCP_ALIAS_H_ */

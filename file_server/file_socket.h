
#ifndef FILE_SOCKET_H_
#define FILE_SOCKET_H_

#include "../include/st_asio_wrapper_server_socket.h"
using namespace st_asio_wrapper;
#include "packer_unpacker.h"

#define st_server_socket_customized(MsgType, Packer, Unpacker) st_asio_wrapper::st_server_socket_base<MsgType, boost::asio::ip::tcp::socket, st_asio_wrapper::i_server, Packer, Unpacker>
class file_socket : public base_socket, public st_server_socket_customized(file_buffer, command_packer, command_unpacker)
{
public:
	file_socket(i_server& server_);
	virtual ~file_socket();

public:
	//because we don't use objects pool(we don't define REUSE_OBJECT), so, this virtual function will
	//not be invoked, and can be omitted, but we keep it for possibly future using
	virtual void reset();

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//we can handle the msg very fast, so we don't use the recv buffer
	virtual bool on_msg(file_buffer& msg);
#endif
	virtual bool on_msg_handle(file_buffer& msg, bool link_down);
	//msg handling end

#ifdef WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(file_buffer& msg);
#endif

private:
	void trans_end();
	void handle_msg(const file_buffer& str);
};

#endif //#ifndef FILE_SOCKET_H_

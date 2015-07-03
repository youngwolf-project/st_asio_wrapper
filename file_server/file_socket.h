
#ifndef FILE_SOCKET_H_
#define FILE_SOCKET_H_

#include "../include/st_asio_wrapper_server_socket.h"
using namespace st_asio_wrapper;
#include "packer_unpacker.h"

class file_socket : public base_socket, public st_server_socket
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
	virtual bool on_msg(msg_type& msg);
#endif
	virtual bool on_msg_handle(msg_type& msg, bool link_down);
	//msg handling end

#ifdef WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(msg_type& msg);
#endif

private:
	void trans_end();
	void handle_msg(const msg_type& msg);
};

#endif //#ifndef FILE_SOCKET_H_

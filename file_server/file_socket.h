
#ifndef FILE_SOCKET_H_
#define FILE_SOCKET_H_

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#include "file_buffer.h"

class file_socket : public base_socket, public server_socket
{
public:
	file_socket(i_server& server_);
	virtual ~file_socket();

public:
	//because we don't use objects pool(we don't defined ST_ASIO_REUSE_OBJECT), so this virtual function will
	//not be invoked, and can be omitted, but we keep it for the possibility of using it in the future
	virtual void reset();
	virtual void take_over(boost::shared_ptr<generic_server_socket<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER> > socket_ptr); //move socket_ptr into this socket
//	virtual void take_over(boost::shared_ptr<file_socket> socket_ptr); //this works too, but brings warnings with -Woverloaded-virtual option.

protected:
	//msg handling
	virtual bool on_msg_handle(out_msg_type& msg);
	//msg handling end

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg);
#endif

private:
	void trans_end();
	void handle_msg(out_msg_ctype& msg);
};

#endif //#ifndef FILE_SOCKET_H_

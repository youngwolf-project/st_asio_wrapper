#ifndef _SERVER_H_
#define _SERVER_H_

class my_server_socket : public server_socket
{
public:
	my_server_socket(i_server& server_) : server_socket(server_)
	{
		boost::dynamic_pointer_cast<prefix_suffix_packer>(packer())->prefix_suffix("", "\n");
		boost::dynamic_pointer_cast<prefix_suffix_unpacker>(unpacker())->prefix_suffix("", "\n");
	}

protected:
	//msg handling
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg + " (from the server)");}
	//msg handling end
};
typedef server_base<my_server_socket> my_server;

#endif //#define _SERVER_H_

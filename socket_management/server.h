#ifndef _SERVER_H_
#define _SERVER_H_

class my_server_socket : public server_socket
{
public:
	my_server_socket(i_server& server_) : server_socket(server_)
	{
#if 3 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_PACKER>(packer())->prefix_suffix("", "\n");
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("", "\n");
#endif
	}

protected:
	//msg handling
#if 1 == PACKER_UNPACKER_TYPE
	//unpacker2 uses unique_buffer or shared_buffer as its message type
	virtual bool on_msg_handle(out_msg_type& msg)
	{
		BOOST_AUTO(raw_msg, new string_buffer());
		raw_msg->assign(" (from the server)");

		ST_ASIO_DEFAULT_PACKER::msg_type msg2(raw_msg);
		return send_msg(msg, msg2); //new feature introduced in 2.2.0
	}
#elif 2 == PACKER_UNPACKER_TYPE
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg);}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {std::string msg2(" (from the server)"); return send_msg(msg, msg2);} //new feature introduced in 2.2.0
#endif
	//msg handling end
};
typedef server_base<my_server_socket> my_server;

#endif //#define _SERVER_H_

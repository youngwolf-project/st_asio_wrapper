
#include <iostream>

#include "../include/ext/tcp.h"
#include "../include/ext/callbacks.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;

#define QUIT_COMMAND	"quit"

//this callback can be invoked concurrently for different sockets
template<typename Socket, typename Msg> bool cb_handle_msg(Socket* socket, Msg& msg) {printf("handle msg: %s\n", msg.data()); return true;}

//this callback can be invoked concurrently for different object pools, callback can be a raw function
template<typename Pool, typename Object, typename Socket, typename Msg> void cb_object_create(Pool*, Object& object) {object->register_on_msg_handle(&cb_handle_msg<Socket, Msg>);}

typedef ext::tcp::client_socket orig_client_socket;
typedef callbacks::c_socket<orig_client_socket> cb_client_socket;
typedef ext::tcp::server_socket orig_server_socket;
typedef callbacks::s_socket<orig_server_socket> cb_server_socket;
int main()
{
	service_pump sp;

	single_client_base<cb_client_socket> c(sp);
	c.register_on_msg_handle(boost::lambda::bind(&cb_handle_msg<orig_client_socket, typename orig_client_socket::out_msg_type>, boost::lambda::_1, boost::lambda::_2)); //callback can be a lambda

	server_base<cb_server_socket, callbacks::object_pool<object_pool<cb_server_socket> > > s(sp);
	BOOST_AUTO(raw_cb_handle_msg, (&cb_handle_msg<orig_server_socket, typename orig_server_socket::out_msg_type>));
	s.register_on_create(boost::bind((void (cb_server_socket::*)(BOOST_TYPEOF(raw_cb_handle_msg), bool)) &cb_server_socket::register_on_msg_handle, //callback can be an function object
		boost::placeholders::_2, raw_cb_handle_msg, false)); //callback can be a raw function, cannot omit default parameters here

	multi_client_base<cb_client_socket, callbacks::object_pool<object_pool<cb_client_socket> > > mc(sp);
	//with helper function 'cb_object_create', it's easier to approach the same result as we do above for server side
	//callback can be a raw function
	mc.register_on_create(&cb_object_create<object_pool<cb_client_socket>, typename object_pool<cb_client_socket>::object_ctype, orig_client_socket, typename orig_client_socket::out_msg_type>);
	mc.add_socket();
	mc.add_socket();

	sp.start_service();
	while (sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else
		{
			c.send_msg(str + " (from single client)");
			s.broadcast_msg(str + " (from server)");
			mc.broadcast_msg(str + " (from multi client)");
		}
	}
}


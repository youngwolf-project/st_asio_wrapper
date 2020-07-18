
#include <iostream>

//configuration
#define ST_ASIO_REUSE_OBJECT		//use objects pool
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT				"quit"
#define UNIX_SOCKET_NAME	"unix-socket"

int main(int argc, const char* argv[])
{
	service_pump sp;

	unix_server server(sp);
	unix_single_client client(sp);
	//unix_multi_client client(sp);

	unlink(UNIX_SOCKET_NAME);
	server.set_server_addr(UNIX_SOCKET_NAME);
	client.set_server_addr(UNIX_SOCKET_NAME);
	//BOOST_AUTO(socket_ptr, client.create_object());
	//socket_ptr->set_server_addr(UNIX_SOCKET_NAME);
	//client.add_socket(socket_ptr);

	//demonstrate how to set/get single client's id, and how to set/get i_service's id.
	//we need specific conversion for i_service because it has the same function name as single client does.
	client.id(10000);
	std::cout << client.id() << " : " << ((service_pump::i_service&) client).id() << std::endl;

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT == str)
			sp.stop_service();
		else
		{
			client.send_msg("client says: " + str);
			//client.broadcast_msg("client says: " + str);
			server.broadcast_msg("server says: " + str);
		}
	}
	unlink(UNIX_SOCKET_NAME);

	return 0;
}

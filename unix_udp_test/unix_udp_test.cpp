
#include <iostream>

//configuration
//configuration

#include "../include/ext/udp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext::udp;

#define QUIT_COMMAND		"quit"
#define RESTART_COMMAND		"restart"
#define UNIX_SOCKET_NAME_1	"unix-socket-1"
#define UNIX_SOCKET_NAME_2	"unix-socket-2"

int main(int argc, const char* argv[])
{
	puts("type " QUIT_COMMAND " to end.");

	service_pump sp;

	unix_single_socket_service uu1(sp);
	unix_single_socket_service uu2(sp);
	//unix_multi_socket_service uu2(sp);

	unlink(UNIX_SOCKET_NAME_1);
	unlink(UNIX_SOCKET_NAME_2);
	uu1.set_local_addr(UNIX_SOCKET_NAME_1);
	uu1.set_peer_addr(UNIX_SOCKET_NAME_2);
	uu2.set_local_addr(UNIX_SOCKET_NAME_2);
	uu2.set_peer_addr(UNIX_SOCKET_NAME_1);
	//BOOST_AUTO(socket_ptr, uu2.create_object());
	//socket_ptr->set_local_addr(UNIX_SOCKET_NAME_2);
	//socket_ptr->set_peer_addr(UNIX_SOCKET_NAME_1);
	//uu2.add_socket(socket_ptr);

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else
		{
			uu1.send_native_msg("uu1 -> uu2: " + str);
			uu2.send_native_msg("uu2 -> uu1: " + str);
			//uu2.at(0)->send_native_msg("uu2 -> uu1: " + str);
		}
	}
	unlink(UNIX_SOCKET_NAME_1);
	unlink(UNIX_SOCKET_NAME_2);

	return 0;
}

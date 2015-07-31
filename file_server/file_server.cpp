
//configuration
#define SERVER_PORT		5050
#define AUTO_CLEAR_CLOSED_SOCKET //auto clear closed clients
#define ENHANCED_STABILITY
#define WANT_MSG_SEND_NOTIFY
#define REPLACEABLE_BUFFER
//configuration

#include "../include/st_asio_wrapper_server.h"
using namespace st_asio_wrapper;
#include "file_socket.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"

int main(int argc, const char* argv[])
{
	puts("this is a file transfer server.");
	printf("usage: file_server [<port=%d> [ip=0.0.0.0]]\n", SERVER_PORT);
	puts("type " QUIT_COMMAND " to end.");

	st_service_pump service_pump;
	st_server_base<file_socket> file_server_(service_pump);

	if (argc > 2)
		file_server_.set_server_addr(atoi(argv[1]), argv[2]);
	else if (argc > 1)
		file_server_.set_server_addr(atoi(argv[1]));

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str == LIST_ALL_CLIENT)
			file_server_.list_all_object();
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef AUTO_CLEAR_CLOSED_SOCKET
#undef ENHANCED_STABILITY
#undef WANT_MSG_SEND_NOTIFY
#undef REPLACEABLE_BUFFER
//restore configuration

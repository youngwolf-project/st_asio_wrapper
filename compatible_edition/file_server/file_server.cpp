
//configuration
#define ST_ASIO_SERVER_PORT		5051
#define ST_ASIO_CLEAR_OBJECT_INTERVAL	60
#define ST_ASIO_ENHANCED_STABILITY
#define ST_ASIO_WANT_MSG_SEND_NOTIFY
#define ST_ASIO_DEFAULT_PACKER	replaceable_packer
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
	printf("usage: file_server [<port=%d> [ip=0.0.0.0]]\n", ST_ASIO_SERVER_PORT);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
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
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (RESTART_COMMAND == str)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (LIST_ALL_CLIENT == str)
			file_server_.list_all_object();
	}

	return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_CLEAR_OBJECT_INTERVAL
#undef ST_ASIO_ENHANCED_STABILITY
#undef ST_ASIO_WANT_MSG_SEND_NOTIFY
#undef ST_ASIO_DEFAULT_PACKER
//restore configuration

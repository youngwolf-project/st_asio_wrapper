
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		5051
#define ST_ASIO_RESTORE_OBJECT
#define ST_ASIO_ENHANCED_STABILITY
#define ST_ASIO_WANT_MSG_SEND_NOTIFY
#define ST_ASIO_DEFAULT_PACKER	replaceable_packer<>
#define ST_ASIO_RECV_BUFFER_TYPE std::vector<boost::asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
#define ST_ASIO_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to st_asio_wrapper
//configuration

#include "file_socket.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"

int main(int argc, const char* argv[])
{
	puts("this is a file transfer server.");
	printf("usage: %s [<port=%d> [ip=0.0.0.0]]\n", argv[0], ST_ASIO_SERVER_PORT);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	server_base<file_socket> file_server_(sp);

	if (argc > 2)
		file_server_.set_server_addr(atoi(argv[1]), argv[2]);
	else if (argc > 1)
		file_server_.set_server_addr(atoi(argv[1]));

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else if (LIST_ALL_CLIENT == str)
			file_server_.list_all_object();
	}

	return 0;
}

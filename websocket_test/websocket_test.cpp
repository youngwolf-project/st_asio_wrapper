
#include <iostream>

//configuration
//#define ST_ASIO_SYNC_RECV
//#define ST_ASIO_SYNC_SEND
//configuration

#include "../include/ext/websocket.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext::websocket;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"

int main(int argc, const char* argv[])
{
	puts("Demonstrate how to use websocket with st_asio_wrapper.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;

	server server_(sp);
	server_.set_start_object_id(1000);

	multi_client client_(sp);
	client_.add_socket();
	client_.add_socket();

	sp.start_service(2);
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();

			//add all clients back
			client_.add_socket();
			client_.add_socket();
			sp.start_service();
		}
		else if (RECONNECT == str)
			//client_.force_shutdown(true);
			client_.graceful_shutdown(true);
		else
			//client_.broadcast_native_msg(str);
			server_.broadcast_native_msg(str);
	}

	return 0;
}

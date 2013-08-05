
//configuration
#define SERVER_PORT		9527
#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
//configuration

#include "../include/st_asio_wrapper_tcp_client.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT_COMMAND "reconnect"
#define SUSPEND_COMMAND	"suspend"
#define RESUME_COMMAND	"resume"

int main() {
	std::string str;
	st_service_pump service_pump;
	st_tcp_sclient client(service_pump);
	//there is no corresponding echo client demo as server endpoint
	//because echo server with echo client made dead loop, and occupy almost all the network resource

	client.set_server_addr(SERVER_PORT + 100, SERVER_IP);
//	client.set_server_addr(SERVER_PORT, "::1"); //ipv6
//	client.set_server_addr(SERVER_PORT, "127.0.0.1"); //ipv4

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::cin >> str;
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str == RECONNECT_COMMAND)
			client.graceful_close(true);
		//the following two commands demonstrate how to suspend msg dispatching, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			client.suspend_dispatch_msg(true);
		else if (str == RESUME_COMMAND)
			client.suspend_dispatch_msg(false);
		else
			client.send_msg(str);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
//restore configuration

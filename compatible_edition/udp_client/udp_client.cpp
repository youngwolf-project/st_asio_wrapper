
#include "../include/st_asio_wrapper_udp_client.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

int main(int argc, const char* argv[]) {
	puts("usage: udp_client <my port> <peer port> <peer ip>");

	if (argc < 4)
		return 1;

	unsigned short local_port = (unsigned short) atoi(argv[1]);
	error_code ec;
	udp::endpoint peer_addr = udp::endpoint(address::from_string(argv[3], ec), (unsigned short) atoi(argv[2]));
	assert(!ec);

	std::string str;
	st_service_pump service_pump;
	st_sudp_client client(service_pump);
	client.set_local_addr(local_port);

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
		else
			client.send_native_msg(peer_addr, str);
	}

	return 0;
}

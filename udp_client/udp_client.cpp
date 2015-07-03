
//configuration
//#define REPLACEABLE_BUFFER
//configuration

#include "../include/st_asio_wrapper_udp_client.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

int main(int argc, const char* argv[]) {
	puts("usage: udp_client <my port> <peer port> [peer ip=127.0.0.1]");

	if (argc < 3)
		return 1;

	auto local_port = (unsigned short) atoi(argv[1]);
	boost::system::error_code ec;
	auto peer_addr = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(argc >= 4 ? argv[3] : "127.0.0.1", ec), (unsigned short) atoi(argv[2]));
	assert(!ec);

	std::string str;
	st_service_pump service_pump;
	st_udp_sclient client(service_pump);
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
			client.safe_send_native_msg(peer_addr, str);
	}

	return 0;
}

//restore configuration
//#undef REPLACEABLE_BUFFER
//restore configuration

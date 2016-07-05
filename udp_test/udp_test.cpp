
//configuration
//#define ST_ASIO_DEFAULT_PACKER replaceable_packer
//#define ST_ASIO_DEFAULT_UDP_UNPACKER replaceable_udp_unpacker
//configuration

#include "../include/st_asio_wrapper_udp_client.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

int main(int argc, const char* argv[])
{
	puts("usage: udp_test <my port> <peer port> [peer ip=127.0.0.1]");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else if (argc < 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

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
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (RESTART_COMMAND == str)
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
#undef ST_ASIO_DEFAULT_PACKER
#undef ST_ASIO_DEFAULT_UNPACKER
//restore configuration

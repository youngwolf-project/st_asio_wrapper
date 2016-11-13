
#include <iostream>

//configuration
#define ST_ASIO_DELAY_CLOSE		1 //this demo not used object pool and doesn't need life cycle management,
								  //so, define this to avoid hooks for async call (and slightly improve efficiency),
								  //any value which is bigger than zero is okay.
//#define ST_ASIO_DEFAULT_PACKER replaceable_packer<>
//#define ST_ASIO_DEFAULT_UDP_UNPACKER replaceable_udp_unpacker<>
//configuration

#include "../include/ext/st_asio_wrapper_udp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

int main(int argc, const char* argv[])
{
	printf("usage: %s <my port> <peer port> [peer ip=127.0.0.1]\n", argv[0]);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else if (argc < 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

	BOOST_AUTO(local_port, (unsigned short) atoi(argv[1]));
	boost::system::error_code ec;
	BOOST_AUTO(peer_addr, boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(argc >= 4 ? argv[3] : "127.0.0.1", ec), (unsigned short) atoi(argv[2])));
	assert(!ec);

	std::string str;
	st_service_pump sp;
	st_udp_sclient client(sp);
	client.set_local_addr(local_port);

	sp.start_service();
	while(sp.is_running())
	{
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else
			client.safe_send_native_msg(peer_addr, str);
	}

	return 0;
}

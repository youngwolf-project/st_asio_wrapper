
#include <iostream>

//configuration
#define ST_ASIO_DELAY_CLOSE		1 //this demo not used object pool and doesn't need life cycle management,
								  //so, define this to avoid hooks for async call (and slightly improve efficiency),
								  //any value which is bigger than zero is okay.
//#define ST_ASIO_DEFAULT_PACKER replaceable_packer<>
//#define ST_ASIO_DEFAULT_UDP_UNPACKER replaceable_udp_unpacker<>
#define ST_ASIO_HEARTBEAT_INTERVAL 5 //neither udp_unpacker nor replaceable_udp_unpacker support heartbeat message,
									 //so heartbeat will be treated as normal message.
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

	st_service_pump sp;
	st_udp_sclient client(sp);
	client.set_local_addr((unsigned short) atoi(argv[1])); //for multicast, do not bind to a specific IP, just port is enough
	client.set_peer_addr((unsigned short) atoi(argv[2]), argc >= 4 ? argv[3] : "127.0.0.1");

	sp.start_service();
	//for multicast, join it after start_service():
//	client.lowest_layer().set_option(boost::asio::ip::multicast::join_group(boost::asio::ip::address::from_string("x.x.x.x")));

	//if you must join it before start_service():
//	client.lowest_layer().open(ST_ASIO_UDP_DEFAULT_IP_VERSION);
//	client.lowest_layer().set_option(boost::asio::ip::multicast::join_group(boost::asio::ip::address::from_string("x.x.x.x")));
//	sp.start_service();
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
			client.direct_sync_send_msg(str); //to send to different endpoints, use overloads that take a const boost::asio::ip::udp::endpoint& parameter
//			client.sync_send_native_msg(str); //to send to different endpoints, use overloads that take a const boost::asio::ip::udp::endpoint& parameter
//			client.safe_send_native_msg(str); //to send to different endpoints, use overloads that take a const boost::asio::ip::udp::endpoint& parameter
	}

	return 0;
}

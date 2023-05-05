
#include <iostream>

//configuration
#define ST_ASIO_NOT_REUSE_ADDRESS
#define ST_ASIO_SYNC_RECV
#define ST_ASIO_SYNC_SEND
#define ST_ASIO_PASSIVE_RECV //if you annotate this definition, this demo will use mix model to receive messages, which means
							 //some messages will be dispatched via on_msg_handle(), some messages will be returned via sync_recv_msg(),
							 //type more than one messages (separate them by space) in one line with ENTER key to send them,
							 //you will see them cross together on the receiver's screen.
							 //with this macro, if heartbeat not applied, macro ST_ASIO_AVOID_AUTO_STOP_SERVICE must be defined to avoid the service_pump run out.
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
//#define ST_ASIO_UDP_CONNECT_MODE true
//#define ST_ASIO_HEARTBEAT_INTERVAL 5 //neither udp_unpacker nor udp_unpacker2 support heartbeat message, so heartbeat will be treated as normal message.
//#define ST_ASIO_DEFAULT_UDP_UNPACKER udp_unpacker2<>
//configuration

#include "../include/ext/reliable_udp.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"

void sync_recv_thread(ext::udp::reliable_socket& socket)
{
	list<ext::udp::reliable_socket::out_msg_type> msg_can;
	sync_call_result re = SUCCESS;
	do
	{
		re = socket.sync_recv_msg(msg_can, 50); //st_asio_wrapper will not maintain messages in msg_can anymore after sync_recv_msg return, please note.
		if (SUCCESS == re)
		{
			for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
				printf("sync recv(" ST_ASIO_SF ") : %s\n", iter->size(), iter->data());
			msg_can.clear(); //sync_recv_msg just append new message(s) to msg_can, please note.
		}
	} while (SUCCESS == re || TIMEOUT == re);
	puts("sync recv end.");
}

//because st_asio_wrapper is header only, it cannot provide the implementation of below global function, but kcp needs it,
//you're supposed to provide it and call reliable_socket_base::output directly in it, like:
int output(const char* buf, int len, ikcpcb* kcp, void* user) {return ((ext::udp::single_reliable_socket_service*) user)->output(buf, len);}

int main(int argc, const char* argv[])
{
	printf("usage: %s [-n normal UDP, otherwise reliable UDP] <my port> <peer port> [peer ip=127.0.0.1]\n", argv[0]);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;

	int index = 0;
	if (argc >= 2 && 0 == strcmp(argv[1], "-n"))
		index = 1;

	if (argc < index + 3)
		return 1;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	ext::udp::single_reliable_socket_service service(sp);
	service.set_local_addr((unsigned short) atoi(argv[index + 1])); //for multicast, do not bind to a specific IP, just port is enough
	service.set_peer_addr((unsigned short) atoi(argv[index + 2]), argc >= index + 4 ? argv[index + 3] : "127.0.0.1");

	if (0 == index)
	{
		service.set_connect_mode();

		//reliable_socket cannot become reliable without below statement, instead, it downgrade to normal UDP socket
		service.create_kcpcb(0, (void*) &service);
		//without below statement, your application will core dump
		ikcp_setoutput(service.get_kcpcb(), &output);
	}

	sp.start_service();
	//for broadcast
//	service.lowest_layer().set_option(boost::asio::socket_base::broadcast(true)); //usage: ./udp_test 5000 5000 "255.255.255.255"
	//for multicast, join it after start_service():
//	service.lowest_layer().set_option(boost::asio::ip::multicast::join_group(boost::asio::ip::make_address("x.x.x.x"))); // >= asio 1.11

	//if you must join it before start_service():
//	service.lowest_layer().open(ST_ASIO_UDP_DEFAULT_IP_VERSION);
//	service.lowest_layer().set_option(boost::asio::ip::multicast::join_group(boost::asio::ip::address::from_string("x.x.x.x"))); // < asio 1.11
//	sp.start_service();

	//demonstrate how to change local address if the binding was failed.
	if (!service.service_started())
	{
		service.set_local_addr(6666);
		sp.start_service(&service);
	}

	boost::thread t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(service)));
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			sp.stop_service();
			t.join();
		}
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			t.join();

			sp.start_service();
			t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(service)));
		}
		else
			service.sync_safe_send_native_msg(str); //to send to different endpoints, use overloads that take a const boost::asio::ip::udp::endpoint& parameter
	}

	return 0;
}

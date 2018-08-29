
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9528
#define ST_ASIO_DELAY_CLOSE		1 //this demo not used object pool and doesn't need life cycle management,
								  //so, define this to avoid hooks for async call (and slightly improve efficiency),
								  //any value which is bigger than zero is okay.
#define ST_ASIO_SYNC_RECV
//#define ST_ASIO_PASSIVE_RECV //because we not defined this macro, this demo will use mix model to receive messages, which means
							   //some messages will be dispatched via on_msg_handle(), some messages will be returned via sync_recv_msg(),
							   //if the server send messages quickly enough, you will see them cross together.
#define ST_ASIO_DISPATCH_BATCH_MSG
#define ST_ASIO_ALIGNED_TIMER
#define ST_ASIO_CUSTOM_LOG
#define ST_ASIO_DEFAULT_UNPACKER non_copy_unpacker
//#define ST_ASIO_DEFAULT_UNPACKER stream_unpacker

//the following three macros demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg will consume huge memory, for example, if we want to support 1M msg size, because every tcp::socket has a
//private unpacker which has a fixed buffer with at lest 1M size, so just for unpackers, 1K tcp::socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every tcp::socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//generally speaking, if there are 1K clients connected to the server, the server can consume
//1G(occupied by unpackers) + 2G(occupied by msg buffer) * 1K = 2049G memory theoretically.
//please note that the server also need to define at least ST_ASIO_HUGE_MSG and ST_ASIO_MSG_BUFFER_SIZE macros too.

//#define ST_ASIO_HUGE_MSG
//#define ST_ASIO_MSG_BUFFER_SIZE (1024 * 1024)
//#define ST_ASIO_MAX_MSG_NUM 8 //reduce msg buffer size to reduce memory occupation
#define ST_ASIO_HEARTBEAT_INTERVAL 5 //if use stream_unpacker, heartbeat messages will be treated as normal messages,
									 //because stream_unpacker doesn't support heartbeat
//configuration

//demonstrate how to use custom log system:
//use your own code to replace the following all_out_helper2 macros, then you can record logs according to your wishes.
//custom log should be defined(or included) before including any st_asio_wrapper header files except base.h
//notice: please don't forget to define the ST_ASIO_CUSTOM_LOG macro.
#include "../include/base.h"
using namespace st_asio_wrapper;

class unified_out
{
public:
	static void fatal_out(const char* fmt, ...) {all_out_helper2("fatal");}
	static void error_out(const char* fmt, ...) {all_out_helper2("error");}
	static void warning_out(const char* fmt, ...) {all_out_helper2("warning");}
	static void info_out(const char* fmt, ...) {all_out_helper2("info");}
	static void debug_out(const char* fmt, ...) {all_out_helper2("debug");}
};

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"

void sync_recv_thread(single_client& client)
{
	ST_ASIO_DEFAULT_UNPACKER::container_type msg_can;
	while (client.sync_recv_msg(msg_can)) //st_asio_wrapper will not maintain messages in msg_can anymore after sync_recv_msg return, please note.
	{
		for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
			printf("sync recv(" ST_ASIO_SF ") : %s\n", iter->size(), iter->data());
		msg_can.clear(); //sync_recv_msg just append new messages(s) to msg_can, please note.
	}
	puts("sync recv end.");
}

int main(int argc, const char* argv[])
{
	printf("usage: %s [<port=%d> [ip=%s]]\n", argv[0], ST_ASIO_SERVER_PORT + 100, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	single_client client(sp);

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	if (argc > 2)
		client.set_server_addr(atoi(argv[1]), argv[2]);
	else if (argc > 1)
		client.set_server_addr(atoi(argv[1]), ST_ASIO_SERVER_IP);
	else
		client.set_server_addr(ST_ASIO_SERVER_PORT + 100, ST_ASIO_SERVER_IP);

	sp.start_service();
	boost::this_thread::sleep_for(boost::chrono::milliseconds(500)); //to be more efficiently, start the worker thread in tcp::socket_base::on_connect().
	boost::thread t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(client)));
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

			t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(client)));
			sp.start_service();
		}
		else if (RECONNECT == str)
			client.graceful_shutdown(true);
		else
			client.safe_send_msg(str, false);
	}

	return 0;
}

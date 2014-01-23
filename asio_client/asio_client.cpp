
//configuration
#define SERVER_PORT		9527
#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define CUSTOM_LOG

//the following three macro demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg consume huge memory, for example, if we support 1M msg size, because every st_tcp_socket has a
//private unpacker which has a buffer at lest 1M size, so 1K st_tcp_socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every st_tcp_socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//#define HUGE_MSG
//#define MAX_MSG_LEN (1024 * 1024)
//#define MAX_MSG_NUM 8 //reduce buffer size to reduce memory occupation
//configuration

//demonstrate how to use custom log system(two methods):
//notice: please don't forget to define the CUSTOM_LOG macro.
//use your own code to replace all_out_helper2 macro.
//custom log should be defined(or included) before including any st_asio_wrapper header files except
//st_asio_wrapper_base.h
#include "../include/st_asio_wrapper_base.h"
using namespace st_asio_wrapper;
//one:
///*
namespace unified_out
{
void fatal_out(const char* fmt, ...) {all_out_helper2;}
void error_out(const char* fmt, ...) {all_out_helper2;}
void warning_out(const char* fmt, ...) {all_out_helper2;}
void info_out(const char* fmt, ...) {all_out_helper2;}
void debug_out(const char* fmt, ...) {all_out_helper2;}
}
//*/
//two:
/*
class unified_out
{
public:
	static void fatal_out(const char* fmt, ...) {all_out_helper2;}
	static void error_out(const char* fmt, ...) {all_out_helper2;}
	static void warning_out(const char* fmt, ...) {all_out_helper2;}
	static void info_out(const char* fmt, ...) {all_out_helper2;}
	static void debug_out(const char* fmt, ...) {all_out_helper2;}
};
*/
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
		//the following two commands demonstrate how to suspend msg sending, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			client.suspend_send_msg(true);
		else if (str == RESUME_COMMAND)
			client.suspend_send_msg(false);
		else
			client.safe_send_msg(str);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer

//#undef HUGE_MSG
//#undef MAX_MSG_LEN
//#undef MAX_MSG_NUM
//restore configuration

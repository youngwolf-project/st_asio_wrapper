
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_REUSE_OBJECT //use objects pool
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
#define ST_ASIO_DEFAULT_UNPACKER stream_unpacker
#define ST_ASIO_MSG_BUFFER_SIZE 65536
//configuration

#include "../include/ext/st_asio_wrapper_net.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"

class echo_socket : public st_server_socket
{
public:
	echo_socket(i_server& server_) : st_server_socket(server_) {}

protected:
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {return direct_post_msg(msg);}
#endif
	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {return direct_post_msg(msg);}
};

class echo_server : public st_server_base<echo_socket>
{
public:
	echo_server(st_service_pump& service_pump_) : st_server_base(service_pump_) {}

	echo_socket::statistic get_statistic()
	{
		echo_socket::statistic stat;
		boost::shared_lock<boost::shared_mutex> lock(ST_THIS object_can_mutex);
		for (BOOST_AUTO(iter, ST_THIS object_can.begin()); iter != ST_THIS object_can.end(); ++iter)
			stat += (*iter)->get_statistic();

		return stat;
	}
};

int main(int argc, const char* argv[])
{
	printf("usage: pingpong_server [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", ST_ASIO_SERVER_PORT);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	st_service_pump service_pump;
	echo_server echo_server_(service_pump);

	if (argc > 3)
		echo_server_.set_server_addr(atoi(argv[2]), argv[3]);
	else if (argc > 2)
		echo_server_.set_server_addr(atoi(argv[2]));

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));

	service_pump.start_service(thread_num);
	while(service_pump.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts(echo_server_.get_statistic().to_string().data());
		}
	}

	return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_REUSE_OBJECT
#undef ST_ASIO_FREE_OBJECT_INTERVAL
#undef ST_ASIO_DEFAULT_UNPACKER
#undef ST_ASIO_MSG_BUFFER_SIZE
//restore configuration

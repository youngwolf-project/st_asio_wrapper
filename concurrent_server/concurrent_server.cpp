
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_MAX_OBJECT_NUM	102400
#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
#define ST_ASIO_MSG_BUFFER_SIZE	1024
#define ST_ASIO_INPUT_QUEUE		non_lock_queue //we will never operate sending buffer concurrently, so need no locks
#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"
#define INCREASE_THREAD	"increase_thread"
#define DECREASE_THREAD	"decrease_thread"

class echo_socket : public server_socket
{
public:
	echo_socket(i_server& server_) : server_socket(server_) {unpacker()->stripped(false);}

protected:
	//msg handling: send the original msg back(echo server)
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER //this virtual function doesn't exist if ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER been defined
	virtual bool on_msg(out_msg_type& msg) {direct_send_msg(msg, true); return true;}
#endif
	virtual bool on_msg_handle(out_msg_type& msg) {direct_send_msg(msg, true); return true;}
	//msg handling end
};

class echo_server : public server_base<echo_socket>
{
public:
	echo_server(service_pump& service_pump_) : server_base<echo_socket>(service_pump_) {}

	statistic get_statistic()
	{
		statistic stat;
		do_something_to_all(stat += boost::lambda::bind(&echo_socket::get_statistic, *boost::lambda::_1));

		return stat;
	}

protected:
	virtual bool on_accept(object_ctype& socket_ptr) {boost::asio::ip::tcp::no_delay option(true); socket_ptr->lowest_layer().set_option(option); return true;}
};

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", argv[0], ST_ASIO_SERVER_PORT);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	echo_server echo_server_(sp);

	if (argc > 3)
		echo_server_.set_server_addr(atoi(argv[2]), argv[3]);
	else if (argc > 2)
		echo_server_.set_server_addr(atoi(argv[2]));

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts("");
			puts(echo_server_.get_statistic().to_string().data());
		}
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

	return 0;
}

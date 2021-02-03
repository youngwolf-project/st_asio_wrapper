
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_MAX_OBJECT_NUM	102400
#define ST_ASIO_ASYNC_ACCEPT_NUM 1024 //pre-create 1024 server socket, this is very useful if creating server socket is very expensive
#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
#define ST_ASIO_MSG_BUFFER_SIZE	1024
#define ST_ASIO_SYNC_DISPATCH
#ifdef ST_ASIO_SYNC_DISPATCH
	#define ST_ASIO_INPUT_QUEUE	non_lock_queue
	#define ST_ASIO_OUTPUT_QUEUE non_lock_queue
#endif
#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

class echo_socket : public server_socket
{
public:
	echo_socket(i_server& server_) : server_socket(server_) {unpacker()->stripped(false);}
	//other heavy things can be done in the constructor too, because we pre-created ST_ASIO_ASYNC_ACCEPT_NUM echo_socket objects

protected:
	//msg handling
#ifdef ST_ASIO_SYNC_DISPATCH
	virtual size_t on_msg(list<out_msg_type>& msg_can)
	{
		for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter) direct_send_msg(*iter, true);
		msg_can.clear();

		return 1;
	}
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		assert(!msg_can.empty());
		out_container_type tmp_can;
		msg_can.swap(tmp_can);

		for (BOOST_AUTO(iter, tmp_can.begin()); iter != tmp_can.end(); ++iter) direct_send_msg(*iter, true);
		return tmp_can.size();
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {direct_send_msg(msg, true); return true;}
#endif
	//msg handling end
};

class echo_server : public server_base<echo_socket>
{
public:
	echo_server(service_pump& service_pump_) : server_base<echo_socket>(service_pump_) {}

protected:
	virtual bool on_accept(object_ctype& socket_ptr) {boost::asio::ip::tcp::no_delay option(true); socket_ptr->lowest_layer().set_option(option); return true;}
	virtual void start_next_accept()
	{
		//after we accepted ST_ASIO_ASYNC_ACCEPT_NUM - 10 connections, we start to create new echo_socket objects (one echo_socket per one accepting)
		if (size() + 10 >= ST_ASIO_ASYNC_ACCEPT_NUM) //only left 10 async accepting operations
			server_base<echo_socket>::start_next_accept();
		else
			puts("stopped one async accepting.");
	}
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
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (STATISTIC == str)
		{
			printf("link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts(echo_server_.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			echo_server_.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			echo_server_.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

	return 0;
}

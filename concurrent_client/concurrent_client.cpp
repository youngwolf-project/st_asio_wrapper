
#include <iostream>
#include <boost/timer/timer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_MAX_OBJECT_NUM	102400
#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve performance)
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
//using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

class echo_socket : public client_socket
{
public:
	echo_socket(i_matrix& matrix_) : client_socket(matrix_), max_delay(1.f), msg_len(ST_ASIO_MSG_BUFFER_SIZE - ST_ASIO_HEAD_LEN) {unpacker()->stripped(false);}
	void begin(float max_delay_, size_t msg_len_) {max_delay = max_delay_; msg_len = msg_len_;}

protected:
	bool check_delay(bool restart_timer)
	{
		boost::lock_guard<boost::mutex> lock(mutex);
		if (is_connected() && (double) last_send_time.elapsed().wall / 1000000000 > max_delay)
		{
			force_shutdown();
			return false;
		}
		else if (restart_timer)
		{
			last_send_time.stop();
			last_send_time.start();
		}

		return true;
	}

	virtual void on_connect()
	{
		boost::asio::ip::tcp::no_delay option(true);
		lowest_layer().set_option(option);

		char* buff = new char[msg_len];
		memset(buff, '$', msg_len); //what should we send?

		last_send_time.stop();
		last_send_time.start();
		send_msg(buff, msg_len, true);

		delete[] buff;
		set_timer(TIMER_END, 5000, boost::lambda::if_then_else_return(boost::lambda::bind(&echo_socket::check_delay, this, false), true, false));

		client_socket::on_connect();
	}

	//msg handling
#ifdef ST_ASIO_SYNC_DISPATCH
	virtual size_t on_msg(list<out_msg_type>& msg_can)
	{
		st_asio_wrapper::do_something_to_all(msg_can, boost::bind(&echo_socket::handle_msg, this, boost::placeholders::_1));
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

		st_asio_wrapper::do_something_to_all(tmp_can, boost::bind(&echo_socket::handle_msg, this, boost::placeholders::_1));
		return tmp_can.size();
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//msg handling end

private:
	void handle_msg(out_msg_type& msg) {if (check_delay(true)) direct_send_msg(msg, true);}

private:
	float max_delay;
	size_t msg_len;

	boost::timer::cpu_timer last_send_time;
	boost::mutex mutex;
};

class echo_client : public st_asio_wrapper::tcp::multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}
	void begin(float max_delay, size_t msg_len) {do_something_to_all(boost::bind(&echo_socket::begin, boost::placeholders::_1, max_delay, msg_len));}
};

int main(int argc, const char* argv[])
{
	printf("usage: %s [<message size=" ST_ASIO_SF "> [<max delay=%f (seconds)> [<service thread number=1> [<port=%d> [<ip=%s> [link num=16]]]]]]\n",
		argv[0], ST_ASIO_MSG_BUFFER_SIZE - ST_ASIO_HEAD_LEN, 1.f, ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	///////////////////////////////////////////////////////////
	size_t link_num = 16;
	if (argc > 6)
		link_num = std::min(ST_ASIO_MAX_OBJECT_NUM, std::max(atoi(argv[6]), 1));

	printf("exec: concurrent_client with " ST_ASIO_SF " links\n", link_num);
	///////////////////////////////////////////////////////////

	service_pump sp;
	echo_client client(sp);

//	argv[5] = "::1" //ipv6
//	argv[5] = "127.0.0.1" //ipv4
	std::string ip = argc > 5 ? argv[5] : ST_ASIO_SERVER_IP;
	unsigned short port = argc > 4 ? atoi(argv[4]) : ST_ASIO_SERVER_PORT;

	int thread_num = 1;
	if (argc > 3)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[3])));
	//add one thread will seriously impact IO throughput when doing performance benchmark, this is because the business logic is very simple (send original messages back,
	//or just add up total message size), under this scenario, just one service thread without receiving buffer will obtain the best IO throughput.
	//the server has such behavior too.

	for (size_t i = 0; i < link_num; ++i)
		client.add_socket(port, ip);

	float max_delay = 1.f;
	if (argc > 2)
		max_delay = std::max(.1f, (float) atof(argv[2]));

	size_t msg_len = ST_ASIO_MSG_BUFFER_SIZE - ST_ASIO_HEAD_LEN;
	if (argc > 1)
		msg_len = std::max((size_t) 1, std::min(msg_len, (size_t) atoi(argv[1])));
	client.begin(max_delay, msg_len);

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
			printf("link #: " ST_ASIO_SF ", valid links: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n\n", client.size(), client.valid_size(), client.invalid_object_size());
			puts(client.get_statistic().to_string().data());
		}
		else if (STATUS == str)
			client.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			client.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
	}

    return 0;
}

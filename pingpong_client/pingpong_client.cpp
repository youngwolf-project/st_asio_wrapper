
#include <iostream>
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
//#define ST_ASIO_WANT_MSG_SEND_NOTIFY
#define ST_ASIO_MSG_BUFFER_SIZE	65536
#define ST_ASIO_INPUT_QUEUE non_lock_queue //we will never operate sending buffer concurrently, so need no locks.
#define ST_ASIO_DEFAULT_UNPACKER stream_unpacker //non-protocol
#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"
#define INCREASE_THREAD	"increase_thread"
#define DECREASE_THREAD	"decrease_thread"

boost::timer::cpu_timer begin_time;
#if BOOST_VERSION >= 105300
boost::atomic_ushort completed_session_num;
#else
atomic<unsigned short> completed_session_num;
#endif

//about congestion control
//
//in 1.3, congestion control has been removed (no post_msg nor post_native_msg anymore), this is because
//without known the business (or logic), framework cannot always do congestion control properly.
//now, users should take the responsibility to do congestion control, there're two ways:
//
//1. for receiver, if you cannot handle msgs timely, which means the bottleneck is in your business,
//    you should open/close congestion control intermittently;
//   for sender, send msgs in on_msg_send() or use sending buffer limitation (like safe_send_msg(..., false)),
//    but must not in service threads, please note.
//
//2. for sender, if responses are available (like pingpong test), send msgs in on_msg()/on_msg_handle(),
//    but this will reduce IO throughput because SOCKET's sliding window is not fully used, pleae note.
//
//pingpong_client will choose method #1 if defined ST_ASIO_WANT_MSG_SEND_NOTIFY, otherwise #2
//BTW, if pingpong_client chose method #2, then pingpong_server can work properly without any congestion control,
//which means pingpong_server can send msgs back with can_overflow parameter equal to true, and memory occupation
//will be under control.

class echo_socket : public client_socket
{
public:
	echo_socket(boost::asio::io_context& io_context_) : client_socket(io_context_) {}

	void begin(size_t msg_num, const char* msg, size_t msg_len)
	{
		total_bytes = msg_len;
		total_bytes *= msg_num;
		send_bytes = recv_bytes = 0;

		send_native_msg(msg, msg_len);
	}

protected:
	virtual void on_connect() {boost::asio::ip::tcp::no_delay option(true); lowest_layer().set_option(option); client_socket::on_connect();}

	//msg handling
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	//congestion control, method #1, the peer needs its own congestion control too.
	virtual void on_msg_send(in_msg_type& msg)
	{
		send_bytes += msg.size();
		if (send_bytes < total_bytes)
			direct_send_msg(msg, true);
	}

private:
	void handle_msg(out_msg_ctype& msg)
	{
		recv_bytes += msg.size();
		if (recv_bytes >= total_bytes && 0 == --completed_session_num)
			begin_time.stop();
	}
#else
private:
	//congestion control, method #2, the peer totally doesn't have to consider congestion control.
	void handle_msg(out_msg_type& msg)
	{
		if (0 == total_bytes)
			return;

		recv_bytes += msg.size();
		if (recv_bytes >= total_bytes)
		{
			total_bytes = 0;
			if (0 == --completed_session_num)
				begin_time.stop();
		}
		else
			direct_send_msg(msg, true);
	}
#endif

private:
	boost::uint64_t total_bytes, send_bytes, recv_bytes;
};

class echo_client : public multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}

	void begin(size_t msg_num, const char* msg, size_t msg_len) {do_something_to_all(boost::bind(&echo_socket::begin, _1, msg_num, msg, msg_len));}
};

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<port=%d> [<ip=%s> [link num=16]]]]\n", argv[0], ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	///////////////////////////////////////////////////////////
	size_t link_num = 16;
	if (argc > 4)
		link_num = std::min(ST_ASIO_MAX_OBJECT_NUM, std::max(atoi(argv[4]), 1));

	printf("exec: echo_client with " ST_ASIO_SF " links\n", link_num);
	///////////////////////////////////////////////////////////

	service_pump sp;
	echo_client client(sp);

//	argv[3] = "::1" //ipv6
//	argv[3] = "127.0.0.1" //ipv4
	std::string ip = argc > 3 ? argv[3] : ST_ASIO_SERVER_IP;
	unsigned short port = argc > 2 ? atoi(argv[2]) : ST_ASIO_SERVER_PORT;

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));
	//add one thread will seriously impact IO throughput when doing performance benchmark, this is because the business logic is very simple (send original messages back,
	//or just add up total message size), under this scenario, just one service thread without receiving buffer will obtain the best IO throughput.
	//the server has such behavior too.

	for (size_t i = 0; i < link_num; ++i)
		client.add_socket(port, ip);

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ST_ASIO_SF ", valid links: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", client.size(), client.valid_size(), client.invalid_object_size());
			puts("");
			puts(client.get_statistic().to_string().data());
		}
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
		else if (!str.empty())
		{
			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			char msg_fill = '0';

			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > tok(str, sep);
			BOOST_AUTO(iter, tok.begin());
			if (iter != tok.end()) msg_num = std::max((size_t) atoi(iter++->data()), (size_t) 1);
			if (iter != tok.end()) msg_len = std::min((size_t) ST_ASIO_MSG_BUFFER_SIZE, std::max((size_t) atoi(iter++->data()), (size_t) 1));
			if (iter != tok.end()) msg_fill = *iter++->data();

			printf("test parameters after adjustment: " ST_ASIO_SF " " ST_ASIO_SF " %c\n", msg_num, msg_len, msg_fill);
			puts("performance test begin, this application will have no response during the test!");

			completed_session_num = (unsigned short) link_num;
			char* init_msg = new char[msg_len];
			memset(init_msg, msg_fill, msg_len);
			client.begin(msg_num, init_msg, msg_len);
			begin_time.start();

			while (0 != completed_session_num)
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));

			boost::uint64_t total_msg_bytes = link_num; total_msg_bytes *= msg_len; total_msg_bytes *= msg_num;
			double used_time = (double) begin_time.elapsed().wall / 1000000000;
			printf("finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n", used_time, link_num * msg_num / used_time, total_msg_bytes / used_time / 1024 / 1024);

			delete[] init_msg;
		}
	}

    return 0;
}

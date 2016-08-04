
#include <iostream>
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_REUSE_OBJECT //use objects pool
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
//#define ST_ASIO_WANT_MSG_SEND_NOTIFY
#define ST_ASIO_MSG_BUFFER_SIZE 65536

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	2
//1-stream unpacker (non-protocol)
//2-pooled_stream_packer and pooled_stream_unpacker (non-protocol)

#if 1 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_UNPACKER stream_unpacker
#elif 2 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER pooled_stream_packer
#define ST_ASIO_DEFAULT_UNPACKER pooled_stream_unpacker
#endif
//configuration

#include "../include/ext/st_asio_wrapper_net.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#if defined(_MSC_VER) || defined(__i386__)
#define uint64_format "%llu"
#else // defined(__GNUC__) && defined(__x86_64__)
#define uint64_format "%lu"
#endif

#define QUIT_COMMAND	"quit"
#define LIST_STATUS		"status"

boost::timer::cpu_timer begin_time;
#if BOOST_VERSION >= 105300
boost::atomic_ushort completed_session_num;
#else
st_atomic<unsigned short> completed_session_num;
#endif
#if 2 == PACKER_UNPACKER_TYPE
memory_pool pool;
#endif

class echo_socket : public st_connector
{
public:
	echo_socket(boost::asio::io_service& io_service_) : st_connector(io_service_)
	{
#if 2 == PACKER_UNPACKER_TYPE
		dynamic_cast<ST_ASIO_DEFAULT_PACKER*>(&*inner_packer())->mem_pool(pool);
		dynamic_cast<ST_ASIO_DEFAULT_UNPACKER*>(&*inner_unpacker())->mem_pool(pool);
#endif
	}

	void begin(size_t msg_num, const char* msg, size_t msg_len)
	{
		total_bytes = msg_len;
		total_bytes *= msg_num;
		send_bytes = recv_bytes = 0;

		send_native_msg(msg, msg_len);
	}

protected:
	//msg handling
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	virtual bool on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {handle_msg(msg); return true;}

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg)
	{
		send_bytes += msg.size();
		if (send_bytes < total_bytes)
			direct_send_msg(std::move(msg));
	}
#endif

private:
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	void handle_msg(out_msg_ctype& msg)
	{
		recv_bytes += msg.size();
		if (recv_bytes >= total_bytes && 0 == --completed_session_num)
			begin_time.stop();
	}
#else
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
			direct_send_msg(std::move(msg));
	}
#endif

private:
	uint64_t total_bytes, send_bytes, recv_bytes;
};

class echo_client : public st_tcp_client_base<echo_socket>
{
public:
	echo_client(st_service_pump& service_pump_) : st_tcp_client_base<echo_socket>(service_pump_) {}

	echo_socket::statistic get_statistic()
	{
		echo_socket::statistic stat;
		do_something_to_all([&stat](object_ctype& item) {stat += item->get_statistic(); });

		return stat;
	}

	void begin(size_t msg_num, const char* msg, size_t msg_len) {do_something_to_all([=](object_ctype& item) {item->begin(msg_num, msg, msg_len);});}
};

int main(int argc, const char* argv[])
{
	printf("usage: pingpong_client [<service thread number=1> [<port=%d> [<ip=%s> [link num=16]]]]\n", ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
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

	st_service_pump service_pump;
	echo_client client(service_pump);

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	std::string ip = argc > 3 ? argv[3] : ST_ASIO_SERVER_IP;
	unsigned short port = argc > 2 ? atoi(argv[2]) : ST_ASIO_SERVER_PORT;

	auto thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));
#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
	if (1 == thread_num)
		++thread_num;
#endif

	for (size_t i = 0; i < link_num; ++i)
		client.add_client(port, ip);

	service_pump.start_service(thread_num);
	while(service_pump.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (LIST_STATUS == str)
		{
			printf("link #: " ST_ASIO_SF ", valid links: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", client.size(), client.valid_size(), client.invalid_object_size());
#if 2 == PACKER_UNPACKER_TYPE
			puts("");
			puts(pool.get_statistic().data());
#endif
			puts("");
			puts(client.get_statistic().to_string().data());
		}
		else if (!str.empty())
		{
			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			auto msg_fill = '0';

			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char>> tok(str, sep);
			auto iter = std::begin(tok);
			if (iter != std::end(tok)) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);
			if (iter != std::end(tok)) msg_len = std::min((size_t) ST_ASIO_MSG_BUFFER_SIZE, std::max((size_t) atoi(iter++->data()), (size_t) 1));
			if (iter != std::end(tok)) msg_fill = *iter++->data();

			printf("test parameters after adjustment: " ST_ASIO_SF " " ST_ASIO_SF " %c\n", msg_num, msg_len, msg_fill);
			puts("performance test begin, this application will have no response during the test!");

			completed_session_num = (unsigned short) link_num;
			auto init_msg = new char[msg_len];
			memset(init_msg, msg_fill, msg_len);
			client.begin(msg_num, init_msg, msg_len);
			begin_time.start();

			while (0 != completed_session_num)
				boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));

			uint64_t total_msg_bytes = link_num; total_msg_bytes *= msg_len; total_msg_bytes *= msg_num;
			auto used_time = (double) (begin_time.elapsed().wall / 1000000) / 1000;
			printf("\r100%%\ntime spent statistics: %f seconds.\n", used_time);
			printf("speed: %f(*2) MBps.\n", total_msg_bytes / used_time / 1024 / 1024);

			delete[] init_msg;
		}
	}

    return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_REUSE_OBJECT
#undef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
#undef ST_ASIO_WANT_MSG_SEND_NOTIFY
#undef ST_ASIO_DEFAULT_PACKER
#undef ST_ASIO_DEFAULT_UNPACKER
#undef ST_ASIO_MSG_BUFFER_SIZE
//restore configuration

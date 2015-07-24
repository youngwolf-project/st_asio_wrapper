
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

//configuration
#define SERVER_PORT		9527
//#define REUSE_OBJECT //use objects pool
//#define AUTO_CLEAR_CLOSED_SOCKET
//#define CLEAR_CLOSED_SOCKET_INTERVAL	1

//the following three macro demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg consume huge memory, for example, if we support 1M msg size, because every st_tcp_socket has a
//private unpacker which has a buffer at lest 1M size, so 1K st_tcp_socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every st_tcp_socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//#define HUGE_MSG
//#define MAX_MSG_LEN (1024 * 1024)
//#define MAX_MSG_NUM 8 //reduce buffer size to reduce memory occupation
//configuration

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	1
//1-default packer and unpacker, head(length) + body
//2-fixed length unpacker
//3-prefix and suffix packer and unpacker

#if 1 == PACKER_UNPACKER_TYPE
//#define REPLACEABLE_BUFFER
#elif 2 == PACKER_UNPACKER_TYPE
#define DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define DEFAULT_PACKER prefix_suffix_packer
#define DEFAULT_UNPACKER prefix_suffix_unpacker
#endif

#include "../include/st_asio_wrapper_tcp_client.h"
using namespace st_asio_wrapper;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"
#define LIST_STATUS		"status"
#define SUSPEND_COMMAND	"suspend"
#define RESUME_COMMAND	"resume"

static bool check_msg;

///////////////////////////////////////////////////
//msg sending interface
#define TCP_RANDOM_SEND_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	auto index = (size_t) ((uint64_t) rand() * (size() - 1) / RAND_MAX); \
	at(index)->SEND_FUNNAME(pstr, len, num, can_overflow); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//msg sending interface
///////////////////////////////////////////////////

class test_socket : public st_connector
{
public:
	test_socket(boost::asio::io_service& io_service_) : st_connector(io_service_), recv_bytes(0), recv_index(0)
	{
#if 2 == PACKER_UNPACKER_TYPE
		dynamic_cast<fixed_length_unpacker*>(&*inner_unpacker())->fixed_length(1024);
#elif 3 == PACKER_UNPACKER_TYPE
		dynamic_cast<prefix_suffix_packer*>(&*inner_packer())->prefix_suffix("begin", "end");
		dynamic_cast<prefix_suffix_unpacker*>(&*inner_unpacker())->prefix_suffix("begin", "end");
#endif
	}

	uint64_t get_recv_bytes() const {return recv_bytes;}
	operator uint64_t() const {return recv_bytes;}

	void restart() {recv_bytes = recv_index = 0;}

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER //not force to use msg recv buffer(so on_msg() will make the decision)
	//we can handle the msg very fast, so we don't use the recv buffer(return false)
	virtual bool on_msg(msg_type& msg) {handle_msg(msg); return true;}
#endif
	//we should handle the msg in on_msg_handle for time-consuming task like this:
	virtual bool on_msg_handle(msg_type& msg, bool link_down) {handle_msg(msg); return true;}
	//msg handling end

private:
	void handle_msg(msg_ctype& msg)
	{
		recv_bytes += msg.size();
		if (check_msg && (msg.size() < sizeof(size_t) || recv_index != *(size_t*) msg.data()))
			printf("check msg error: " size_t_format ".\n", recv_index);
		++recv_index;
	}

private:
	uint64_t recv_bytes;
	size_t recv_index;
};

class test_client : public st_tcp_client_base<test_socket>
{
public:
	test_client(st_service_pump& service_pump_) : st_tcp_client_base<test_socket>(service_pump_) {}

	void restart() {do_something_to_all([](object_ctype& item) {item->restart();});}
	uint64_t get_total_recv_bytes()
	{
		uint64_t total_recv_bytes = 0;
		do_something_to_all([&total_recv_bytes](object_ctype& item) {total_recv_bytes += *item;});
//		do_something_to_all([&total_recv_bytes](object_ctype& item) {total_recv_bytes += item->get_recv_bytes();});

		return total_recv_bytes;
	}

	void close_some_client(size_t n)
	{
		//close some clients
		//method #1
//		do_something_to_one([&n](object_ctype& item) {return n-- > 0 ? item->graceful_close(), false : true;});
		//notice: this method need to define AUTO_CLEAR_CLOSED_SOCKET and CLEAR_CLOSED_SOCKET_INTERVAL macro, because it just closed the st_socket,
		//not really removed them from object pool, this will cause test_client still send data to them, and wait responses from them.
		//for this scenario, the smaller CLEAR_CLOSED_SOCKET_INTERVAL is, the better experience you will get, so set it to 1 second.

		//method #2
		while (n-- > 0)
			graceful_close(at(0));
		//notice: this method directly remove the client from object pool (and insert into list temp_object_can), and close the st_socket.
		//clients in list temp_object_can will be reused if new clients needed (REUSE_OBJECT macro been defined), or be truly freed from memory
		//CLOSED_SOCKET_MAX_DURATION seconds later (but check interval is SOCKET_FREE_INTERVAL seconds, so the maximum delay is CLOSED_SOCKET_MAX_DURATION + SOCKET_FREE_INTERVAL).
		//this is a equivalence of calling i_server::del_client in st_server_socket_base::on_recv_error (see st_server_socket_base for more details).
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket's send buffer
	TCP_RANDOM_SEND_MSG(safe_random_send_msg, safe_send_msg)
	TCP_RANDOM_SEND_MSG(safe_random_send_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////
};

int main(int argc, const char* argv[])
{
	///////////////////////////////////////////////////////////
	printf("usage: test_client [<port=%d> [<ip=%s> [link num=1]]]\n", SERVER_PORT, SERVER_IP);

	size_t link_num = 16;
	if (argc > 3)
		link_num = std::min(MAX_OBJECT_NUM, std::max(atoi(argv[3]), 1));

	printf("exec: test_client " size_t_format "\n", link_num);
	///////////////////////////////////////////////////////////

	st_service_pump service_pump;
	test_client client(service_pump);
	for (size_t i = 0; i < link_num; ++i)
		client.add_client();

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	if (argc > 2)
		client.do_something_to_all([argv](test_client::object_ctype& item) {item->set_server_addr(atoi(argv[1]), argv[2]);});
	else if (argc > 1)
		client.do_something_to_all([argv](test_client::object_ctype& item) {item->set_server_addr(atoi(argv[1]), SERVER_IP);});

	int min_thread_num = 1;
#ifdef AUTO_CLEAR_CLOSED_SOCKET
	++min_thread_num;
#endif

	service_pump.start_service(min_thread_num);
	while(service_pump.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service(min_thread_num);
		}
		else if (str == LIST_STATUS)
			printf("valid links: " size_t_format ", closed links: " size_t_format "\n", client.valid_size(), client.closed_object_size());
		//the following two commands demonstrate how to suspend msg dispatching, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			client.do_something_to_all([](test_client::object_ctype& item) {item->suspend_dispatch_msg(true);});
		else if (str == RESUME_COMMAND)
			client.do_something_to_all([](test_client::object_ctype& item) {item->suspend_dispatch_msg(false);});
		else if (str == LIST_ALL_CLIENT)
			client.list_all_object();
		else if (!str.empty())
		{
			if ('+' == str[0] || '-' == str[0])
			{
				auto n = (size_t) atoi(std::next(str.data()));
				if (0 == n)
					n = 1;

				if ('+' == str[0])
					for (; n > 0 && client.add_client(); ++link_num, --n);
				else
				{
					if (n > client.size())
						n = client.size();
					link_num -= n;

					client.close_some_client(n);
				}

				continue;
			}

			if (client.size() != link_num)
			{
				puts("some closed links have not been cleared, did you defined AUTO_CLEAR_CLOSED_SOCKET macro?");
				continue;
			}

			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			char msg_fill = '0';
			char model = 0; //0 broadcast, 1 randomly pick one link per msg

			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char>> tok(str, sep);
			auto iter = std::begin(tok);
			if (iter != std::end(tok)) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);

			auto native = false;
#if 1 == PACKER_UNPACKER_TYPE
			if (iter != std::end(tok)) msg_len = std::min(packer::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#elif 2 == PACKER_UNPACKER_TYPE
			if (iter != std::end(tok)) ++iter;
			msg_len = 1024; //we hard code this because we fixedly initialized the length of fixed_length_unpacker to 1024
			native = true; //we don't have fixed_length_packer, so use packer instead, but need to pack msgs with native manner.
#elif 3 == PACKER_UNPACKER_TYPE
			if (iter != std::end(tok)) msg_len = std::min((size_t) MSG_BUFFER_SIZE,
				std::max((size_t) atoi(iter++->data()), sizeof(size_t)));
#endif
			if (iter != std::end(tok)) msg_fill = *iter++->data();
			if (iter != std::end(tok)) model = *iter++->data() - '0';

			unsigned percent = 0;
			uint64_t total_msg_bytes;
			switch (model)
			{
			case 0:
				check_msg = true;
				total_msg_bytes = msg_num * link_num; break;
			case 1:
				check_msg = false;
				srand(time(nullptr));
				total_msg_bytes = msg_num; break;
			default:
				total_msg_bytes = 0; break;
			}

			if (total_msg_bytes > 0)
			{
				printf("test parameters after adjustment: " size_t_format " " size_t_format " %c %d\n", msg_num, msg_len, msg_fill, model);
				puts("performance test begin, this application will have no response during the test!");

				client.restart();

				total_msg_bytes *= msg_len;
				boost::timer::cpu_timer begin_time;
				auto buff = new char[msg_len];
				memset(buff, msg_fill, msg_len);
				uint64_t send_bytes = 0;
				for (size_t i = 0; i < msg_num; ++i)
				{
					memcpy(buff, &i, sizeof(size_t)); //seq

					switch (model)
					{
					case 0:
						native ? client.safe_broadcast_native_msg(buff, msg_len) : client.safe_broadcast_msg(buff, msg_len);
						send_bytes += link_num * msg_len;
						break;
					case 1:
						native ? client.safe_random_send_native_msg(buff, msg_len) : client.safe_random_send_msg(buff, msg_len);
						send_bytes += msg_len;
						break;
					default:
						break;
					}

					auto new_percent = (unsigned) (100 * send_bytes / total_msg_bytes);
					if (percent != new_percent)
					{
						percent = new_percent;
						printf("\r%u%%", percent);
						fflush(stdout);
					}
				}
				delete[] buff;

				while(client.get_total_recv_bytes() != total_msg_bytes)
					boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));

				auto used_time = (double) (begin_time.elapsed().wall / 1000000) / 1000;
				printf("\r100%%\ntime spent statistics: %.1f seconds.\n", used_time);
				printf("speed: %.0f(*2)kB/s.\n", total_msg_bytes / used_time / 1024);
			} // if (total_data_num > 0)
		}
	}

    return 0;
}

//restore configuration
#undef SERVER_PORT
//#undef REUSE_OBJECT
//#undef AUTO_CLEAR_CLOSED_SOCKET
//#undef CLEAR_CLOSED_SOCKET_INTERVAL
//#undef REPLACEABLE_BUFFER

//#undef HUGE_MSG
//#undef MAX_MSG_LEN
//#undef MAX_MSG_NUM
//restore configuration

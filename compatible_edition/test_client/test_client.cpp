
#include <boost/tokenizer.hpp>
#include <boost/lambda/lambda.hpp>

//configuration
#define SERVER_PORT		9528
//#define REUSE_OBJECT //use objects pool

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
//2-fixed length packer and unpacker
//3-prefix and suffix packer and unpacker

#if 2 == PACKER_UNPACKER_TYPE
#define DEFAULT_PACKER	fixed_legnth_packer
#define DEFAULT_UNPACKER fixed_length_unpacker
#endif
#if 3 == PACKER_UNPACKER_TYPE
#define DEFAULT_PACKER prefix_suffix_packer
#define DEFAULT_UNPACKER prefix_suffix_unpacker
#endif

#include "../include/st_asio_wrapper_tcp_client.h"
using namespace st_asio_wrapper;

#ifdef _MSC_VER
#define atoll _atoi64
#endif

#if defined(_WIN64) || 64 == __WORDSIZE
#define uint64_t_format "%lu"
#else
#define uint64_t_format "%llu"
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
	size_t index = (size_t) ((boost::uint64_t) rand() * (object_can.size() - 1) / RAND_MAX); \
	boost::mutex::scoped_lock lock(object_can_mutex); \
	BOOST_AUTO(iter, object_can.begin()); \
	std::advance(iter, index); \
	(*iter)->SEND_FUNNAME(pstr, len, num, can_overflow); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//msg sending interface
///////////////////////////////////////////////////

SHARED_OBJECT(test_socket, st_connector)
{
public:
	test_socket(boost::asio::io_service& io_service_) : st_connector(io_service_), recv_bytes(0), recv_index(0)
	{
#if PACKER_UNPACKER_TYPE == 2
		dynamic_cast<fixed_length_unpacker*>(&*inner_unpacker())->fixed_length(1024);
#endif
#if PACKER_UNPACKER_TYPE == 3
		dynamic_cast<prefix_suffix_unpacker*>(&*inner_unpacker())->prefix_suffix("begin", "end");
		dynamic_cast<prefix_suffix_packer*>(&*inner_packer())->prefix_suffix("begin", "end");
#endif
	}

	boost::uint64_t get_recv_bytes() const {return recv_bytes;}
	operator boost::uint64_t() const {return recv_bytes;}
	//workaround for boost::lambda
	//how to invoke get_recv_bytes() directly, please tell me if somebody knows!

	void restart() {recv_bytes = recv_index = 0;}

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER //not force to use msg recv buffer(so on_msg() will make the decision)
	//we can handle the msg very fast, so we don't use the recv buffer(return false)
	virtual bool on_msg(std::string& msg) {handle_msg(msg); return true;}
#endif
	//we should handle the msg in on_msg_handle for time-consuming task like this:
	virtual bool on_msg_handle(std::string& msg, bool link_down) {handle_msg(msg); return true;}
	//msg handling end

private:
	void handle_msg(const std::string& msg)
	{
		recv_bytes += msg.size();
		if (::check_msg && (msg.size() < sizeof(size_t) || recv_index != *(size_t*) msg.data()))
			printf("check msg error: " size_t_format ".\n", recv_index);
		++recv_index;
	}

private:
	boost::uint64_t recv_bytes;
	size_t recv_index;
};

class test_client : public st_tcp_client_base<test_socket>
{
public:
	test_client(st_service_pump& service_pump_) : st_tcp_client_base<test_socket>(service_pump_) {}

	void restart() {do_something_to_all(boost::mem_fn(&test_socket::restart));}
	boost::uint64_t get_total_recv_bytes()
	{
		boost::uint64_t total_recv_bytes = 0;
		do_something_to_all(boost::ref(total_recv_bytes) += boost::lambda::ret<boost::uint64_t>(*boost::lambda::_1));
		//how to invoke the test_socket::get_recv_bytes() directly, please tell me if somebody knows!

		return total_recv_bytes;
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
	puts("usage: test_client [link num=16]");

	size_t link_num = 16;
	if (argc > 1) link_num = std::min((size_t) MAX_OBJECT_NUM, std::max((size_t) atoi(argv[1]), (size_t) 1));

	printf("exec: test_client " size_t_format "\n", link_num);
	///////////////////////////////////////////////////////////

	std::string str;
	st_service_pump service_pump;
	test_client client(service_pump);
	for (size_t i = 0; i < link_num; ++i)
		client.add_client();
//	client.do_something_to_all(boost::bind(&test_socket::set_server_addr, _1, SERVER_PORT, "::1")); //ipv6
//	client.do_something_to_all(boost::bind(&test_socket::set_server_addr, _1, SERVER_PORT, "127.0.0.1")); //ipv4

	service_pump.start_service(1);
	while(service_pump.is_running())
	{
		std::getline(std::cin, str);
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service(1);
		}
		else if (str == LIST_STATUS)
		{
			printf("valid links: " size_t_format ", closed links: " size_t_format "\n",
				client.valid_size(), client.closed_object_size());
		}
		//the following two commands demonstrate how to suspend msg dispatching, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			client.do_something_to_all(boost::bind(&test_socket::suspend_dispatch_msg, _1, true));
		else if (str == RESUME_COMMAND)
			client.do_something_to_all(boost::bind(&test_socket::suspend_dispatch_msg, _1, false));
		else if (str == LIST_ALL_CLIENT)
			client.list_all_object();
		else if (!str.empty())
		{
			if ('+' == str[0] || '-' == str[0])
			{
				size_t n = (size_t) atoi(str.data() + 1);
				if (0 == n)
					n = 1;

				if ('+' == str[0])
					for (; n > 0 && client.add_client(); ++link_num, --n);
				else
				{
					if (n > client.size())
						n = client.size();
					link_num -= n;

					while (n-- > 0)
						client.graceful_close(client.at(0));
				}

				continue;
			}

			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			char msg_fill = '0';
			char model = 0; //0 broadcast, 1 randomly pick one link per msg

			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > tok(str, sep);
			BOOST_AUTO(iter, tok.begin());
			if (iter != tok.end()) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);
#if 1 == PACKER_UNPACKER_TYPE
			if (iter != tok.end()) msg_len = std::min(packer::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#endif
#if 2 == PACKER_UNPACKER_TYPE
			if (iter != tok.end()) ++iter;
			msg_len = 1024;
#endif
#if 3 == PACKER_UNPACKER_TYPE
			if (iter != tok.end()) msg_len = std::min((size_t) MAX_MSG_LEN,
				std::max((size_t) atoi(iter++->data()), sizeof(size_t)));
#endif
			if (iter != tok.end()) msg_fill = *iter++->data();
			if (iter != tok.end()) model = *iter++->data() - '0';

			unsigned percent = 0;
			boost::uint64_t total_msg_bytes;
			switch (model)
			{
			case 0:
				check_msg = true;
				total_msg_bytes = msg_num * link_num; break;
			case 1:
				check_msg = false;
				srand(time(NULL));
				total_msg_bytes = msg_num; break;
			default:
				total_msg_bytes = 0; break;
			}

			if (total_msg_bytes > 0)
			{
				printf("test parameters after adjustment: " size_t_format " " size_t_format " %c %d\n",
					msg_num, msg_len, msg_fill, model);
				puts("performance test begin, this application will have no response during the test!");

				client.restart();

				total_msg_bytes *= msg_len;
				int begin_time = boost::get_system_time().time_of_day().total_seconds();
				char* buff = new char[msg_len];
				memset(buff, msg_fill, msg_len);
				boost::uint64_t send_bytes = 0;
				for (size_t i = 0; i < msg_num; ++i)
				{
					memcpy(buff, &i, sizeof(size_t)); //seq

					switch (model)
					{
					case 0:
						client.safe_broadcast_msg(buff, msg_len); send_bytes += link_num * msg_len; break;
					case 1:
						client.safe_random_send_msg(buff, msg_len); send_bytes += msg_len; break;
					default: break;
					}

					unsigned new_percent = (unsigned) (100 * send_bytes / total_msg_bytes);
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

				int used_time = boost::get_system_time().time_of_day().total_seconds() - begin_time;
				printf("\r100%%\ntime spent statistics: %d hours, %d minutes, %d seconds.\n",
					used_time / 60 / 60, (used_time / 60) % 60, used_time % 60);
				if (used_time > 0)
					printf("speed: " uint64_t_format "(*2)kB/s.\n", total_msg_bytes / 1024 / used_time);
			} // if (total_data_num > 0)
		}
	}

    return 0;
}

//restore configuration
#undef SERVER_PORT
//#undef REUSE_OBJECT

//#undef HUGE_MSG
//#undef MAX_MSG_LEN
//#undef MAX_MSG_NUM
//restore configuration

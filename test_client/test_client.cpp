
#include <boost/tokenizer.hpp>

//configuration
#define SERVER_PORT		9527
//configuration

#include "../include/st_asio_wrapper_client.h"
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

static bool check_msg;

///////////////////////////////////////////////////
//msg sending interface
#define RANDOM_SEND_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	auto index = (size_t) ((uint64_t) rand() * (client_can.size() - 1) / RAND_MAX); \
	auto iter = std::begin(client_can); \
	std::advance(iter, index); \
	SEND_AND_WAIT(*iter, SEND_FUNNAME); \
} \
SEND_MSG_CALL_SWITCH(FUNNAME, void)
//msg sending interface
///////////////////////////////////////////////////

class test_socket : public st_connector
{
public:
	test_socket(st_service_pump& service_pump_) : st_connector(service_pump_), recv_bytes(0), recv_index(0) {}

	uint64_t get_recv_bytes() const {return recv_bytes;}
	//reset all, be ensure that there's no any operations performed on this st_socket when invoke it
	virtual void reset() {recv_bytes = recv_index = 0; st_connector::reset();}

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER //not force to use msg recv buffer(so on_msg() will make the decision)
	//we can handle the msg very fast, so we don't use the recv buffer(return false)
	virtual bool on_msg(msg_type& msg) {handle_msg(msg); return false;}
#endif
	//we should handle the msg in on_msg_handle for time-consuming task like this:
	virtual void on_msg_handle(msg_type& msg) {handle_msg(msg);}
	//msg handling end

private:
	void handle_msg(const std::string& str)
	{
		recv_bytes += str.size();
		if (::check_msg && (str.size() < sizeof(size_t) || recv_index != *(size_t*) str.data()))
			printf("check msg error: " size_t_format ".\n", recv_index);
		++recv_index;
	}

private:
	uint64_t recv_bytes;
	size_t recv_index;
};

class test_client : public st_client_base<test_socket>
{
public:
	test_client(st_service_pump& service_pump_) : st_client_base<test_socket>(service_pump_) {}

	void reset() {do_something_to_all(boost::mem_fn(&test_socket::reset));}
	uint64_t get_total_recv_bytes() const
	{
		uint64_t total_recv_bytes = 0;
		do_something_to_all([&](decltype(*std::begin(client_can))& item) {
			total_recv_bytes += item->get_recv_bytes();
		});

		return total_recv_bytes;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//if can_overflow is false, all of the following four functions will block when there's not
	//enough send buffers
	RANDOM_SEND_MSG(random_send_msg, send_msg)
	RANDOM_SEND_MSG(random_send_native_msg, send_native_msg)
	SAFE_BROADCAST_MSG(broadcast_msg, send_msg)
	SAFE_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////
};

int main(int argc, const char* argv[])
{
	///////////////////////////////////////////////////////////
	puts("usage: test_client <link num>");

	size_t link_num = 16;
	if (argc > 1) link_num = std::min((size_t) 4096, std::max((size_t) atoi(argv[1]), (size_t) 1));

	printf("exec: test_client " size_t_format "\n", link_num);
	///////////////////////////////////////////////////////////

	std::string str;
	st_service_pump service_pump;
	test_client client(service_pump);
	for (size_t i = 0; i < link_num; ++i)
		client.add_client();
//	client.do_something_to_all(boost::bind(&test_socket::set_server_addr, _1, SERVER_PORT, "::1")); //ipv6
//	client.do_something_to_all(boost::bind(&test_socket::set_server_addr, _1, SERVER_PORT, "127.0.0.1")); //ipv4

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::getline(std::cin, str);
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (!str.empty())
		{
			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			char msg_fill = '0';
			char model = 0; //0 broadcast, 1 randomly pick one link per msg

			char_separator<char> sep(" \t");
			tokenizer<char_separator<char>> tok(str, sep);
			auto iter = std::begin(tok);
			if (iter != std::end(tok)) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);
			if (iter != std::end(tok)) msg_len = std::min(i_packer::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
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
				printf("test parameters after adjustment: " size_t_format " " size_t_format " %c %d\n",
					msg_num, msg_len, msg_fill, model);
				puts("performance test begin, this application will have no response during the test!");

				client.reset();

				total_msg_bytes *= msg_len;
				auto begin_time = get_system_time().time_of_day().total_seconds();
				auto buff = new char[msg_len];
				memset(buff, msg_fill, msg_len);
				uint64_t send_bytes = 0;
				for (size_t i = 0; i < msg_num; ++i)
				{
					memcpy(buff, &i, sizeof(size_t)); //seq

					switch (model)
					{
					case 0:
						client.broadcast_msg(buff, msg_len); send_bytes += link_num * msg_len; break;
					case 1:
						client.random_send_msg(buff, msg_len); send_bytes += msg_len; break;
					default: break;
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
					this_thread::sleep(get_system_time() + posix_time::milliseconds(50));

				auto used_time = get_system_time().time_of_day().total_seconds() - begin_time;
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
//restore configuration

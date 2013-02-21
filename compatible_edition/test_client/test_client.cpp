
#include <boost/tokenizer.hpp>

//configuration
#define SERVER_PORT		9528
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

///////////////////////////////////////////////////
//msg sending interface
#define SEND_AND_WAIT(SOCKET, SEND_FUNNAME) \
while (!(SOCKET)->SEND_FUNNAME(pstr, len, num, can_overflow)) \
	this_thread::sleep(get_system_time() + posix_time::milliseconds(50))

#define RANDOM_SEND_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	size_t index = (size_t) ((uint64_t) rand() * (client_can.size() - 1) / RAND_MAX); \
	BOOST_AUTO(iter, client_can.begin()); \
	std::advance(iter, index); \
	SEND_AND_WAIT(*iter, SEND_FUNNAME); \
} \
SEND_MSG_CALL_SWITCH(FUNNAME, void)

#define MY_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	for (BOOST_AUTO(iter, client_can.begin()); iter != client_can.end(); ++iter) \
		SEND_AND_WAIT(*iter, SEND_FUNNAME); \
} \
SEND_MSG_CALL_SWITCH(FUNNAME, void)
//msg sending interface
///////////////////////////////////////////////////

static bool check_msg;
class test_client : public st_client
{
public:
	class test_socket : public st_connector
	{
	public:
		test_socket(test_client& test_client_) : st_connector(test_client_.get_service_pump()),
			recv_bytes(0), recv_index(0) {}

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

public:
	test_client(st_service_pump& service_pump) : st_client(service_pump) {}

	void reset() {do_something_to_all(boost::mem_fn(&st_connector::reset));}
	uint64_t get_total_recv_bytes() const
	{
		uint64_t total_recv_bytes = 0;
		for (BOOST_AUTO(iter, client_can.begin()); iter != client_can.end(); ++iter)
			//this cause compiler error in boost_1.52(maybe include lower versions), it's a bug which have been resolved
			//in boost_1.53
			//total_recv_bytes += dynamic_cast<test_socket*>(iter->get())->get_recv_bytes();
			total_recv_bytes += dynamic_cast<test_socket*>((*iter).get())->get_recv_bytes(); //bypass above bug

		return total_recv_bytes;
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//if can_overflow is false, all of the following four functions will block when there's not
	//enough send buffers
	RANDOM_SEND_MSG(random_send_msg, send_msg)
	RANDOM_SEND_MSG(random_send_native_msg, send_native_msg)
	MY_BROADCAST_MSG(broadcast_msg, send_msg)
	MY_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
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
	{
		BOOST_AUTO(client_ptr, boost::make_shared<test_client::test_socket>(boost::ref(client)));
//		client_ptr->set_server_addr(SERVER_PORT, "::1"); //ipv6
//		client_ptr->set_server_addr(SERVER_PORT, "127.0.0.1"); //ipv4
		client.add_client(client_ptr);
	}

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
			tokenizer<char_separator<char> > tok(str, sep);
			BOOST_AUTO(iter, tok.begin());
			if (iter != tok.end()) msg_num = std::max((size_t) atoll(iter++->data()), (size_t) 1);
			if (iter != tok.end()) msg_len = std::min(i_packer::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
			if (iter != tok.end()) msg_fill = *iter++->data();
			if (iter != tok.end()) model = *iter++->data() - '0';

			unsigned percent = 0;
			uint64_t total_msg_bytes;
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

				client.reset();

				total_msg_bytes *= msg_len;
				int begin_time = get_system_time().time_of_day().total_seconds();
				char* buff = new char[msg_len];
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
					this_thread::sleep(get_system_time() + posix_time::milliseconds(50));

				int used_time = get_system_time().time_of_day().total_seconds() - begin_time;
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

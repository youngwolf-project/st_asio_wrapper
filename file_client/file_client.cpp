
#include <iostream>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		5051
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
//#define ST_ASIO_INPUT_QUEUE non_lock_queue
//we cannot use non_lock_queue, because we also send messages (talking messages) out of st_asio_wrapper::socket::on_msg_send().
#define ST_ASIO_DEFAULT_UNPACKER replaceable_unpacker<>
#define ST_ASIO_RECV_BUFFER_TYPE std::vector<boost::asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
#define ST_ASIO_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to st_asio_wrapper
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define REQUEST_FILE	"get"

int link_num = 1;
fl_type file_size;
#if BOOST_VERSION >= 105300
boost::atomic_int_fast64_t received_size;
#else
atomic<boost::int_fast64_t> received_size;
#endif

int main(int argc, const char* argv[])
{
	puts("this is a file transfer client.");
	printf("usage: %s [<port=%d> [<ip=%s> [link num=1]]]\n", argv[0], ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	file_client client(sp);

	if (argc > 3)
		link_num = std::min(256, std::max(atoi(argv[3]), 1));

	for (int i = 0; i < link_num; ++i)
	{
//		argv[2] = "::1" //ipv6
//		argv[2] = "127.0.0.1" //ipv4
		if (argc > 2)
			client.add_socket(atoi(argv[1]), argv[2])->set_index(i);
		else if (argc > 1)
			client.add_socket(atoi(argv[1]))->set_index(i);
		else
			client.add_socket()->set_index(i);
	}

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service();
		}
		else if (str.size() > sizeof(REQUEST_FILE) && !strncmp(REQUEST_FILE, str.data(), sizeof(REQUEST_FILE) - 1) && isspace(str[sizeof(REQUEST_FILE) - 1]))
		{
			str.erase(0, sizeof(REQUEST_FILE));
			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > tok(str, sep);
			for (BOOST_AUTO(iter, tok.begin()); iter != tok.end(); ++iter)
			{
				file_size = -1;
				received_size = 0;

				if (client.get_file(*iter))
					while (client.is_transferring())
						boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
			}
		}
		else
			client.at(0)->talk(str);
	}

	return 0;
}

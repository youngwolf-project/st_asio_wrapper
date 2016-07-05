
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		5051
#define ST_ASIO_DEFAULT_UNPACKER replaceable_unpacker
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define REQUEST_FILE	"get"

boost::atomic_ushort completed_client_num;
int link_num = 1;
fl_type file_size;

int main(int argc, const char* argv[])
{
	puts("this is a file transfer client.");
	printf("usage: file_client [<port=%d> [<ip=%s> [link num=1]]]\n", ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	st_service_pump service_pump;
	file_client client(service_pump);

	if (argc > 3)
		link_num = std::min(256, std::max(atoi(argv[3]), 1));

	for (int i = 0; i < link_num; ++i)
	{
//		argv[2] = "::1" //ipv6
//		argv[2] = "127.0.0.1" //ipv4
		if (argc > 2)
			client.add_client(atoi(argv[1]), argv[2])->set_index(i);
		else if (argc > 1)
			client.add_client(atoi(argv[1]))->set_index(i);
		else
			client.add_client()->set_index(i);
	}

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (RESTART_COMMAND == str)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str.size() > sizeof(REQUEST_FILE) && !strncmp(REQUEST_FILE, str.data(), sizeof(REQUEST_FILE) - 1) && isspace(str[sizeof(REQUEST_FILE) - 1]))
		{
			str.erase(0, sizeof(REQUEST_FILE));
			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > tok(str, sep);
			for (BOOST_AUTO(iter, tok.begin()); iter != tok.end(); ++iter)
			{
				completed_client_num = 0;
				file_size = 0;
				boost::timer::cpu_timer begin_time;

				printf("transfer %s begin.\n", iter->data());
				if (client.find(0)->get_file(*iter))
				{
					client.do_something_to_all(boost::bind(&file_socket::get_file, _1, boost::cref(*iter)));
					client.start();

					while (completed_client_num != (unsigned short) link_num)
						boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));

					client.stop(*iter);
				}
				else
					printf("transfer %s failed!\n", iter->data());
			}
		}
		else
			client.at(0)->talk(str);
	}

	return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_DEFAULT_UNPACKER
//restore configuration

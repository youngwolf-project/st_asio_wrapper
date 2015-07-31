
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

//configuration
#define SERVER_PORT		5051
#define REPLACEABLE_BUFFER
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define REQUEST_FILE	"get"

boost::atomic_ushort completed_client_num;
int link_num = 1;
__off64_t file_size;

int main(int argc, const char* argv[])
{
	puts("this is a file transfer client.");
	printf("usage: file_client [<port=%d> [<ip=%s> [link num=1]]]\n", SERVER_PORT, SERVER_IP);
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
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
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

					unsigned percent = -1;
					while (completed_client_num != (unsigned short) link_num)
					{
						boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50));
						if (file_size > 0)
						{
							__off64_t total_rest_size = client.get_total_rest_size();
							if (total_rest_size > 0)
							{
								unsigned new_percent = (unsigned) ((file_size - total_rest_size) * 100 / file_size);
								if (percent != new_percent)
								{
									percent = new_percent;
									printf("\r%u%%", percent);
									fflush(stdout);
								}
							}
						}
					}

					double used_time = (double) (begin_time.elapsed().wall / 1000000) / 1000;
					printf("\r100%%\ntransfer %s end, speed: %.0f kB/s.\n", iter->data(), file_size / used_time / 1024);
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
#undef SERVER_PORT
#undef REPLACEABLE_BUFFER
//restore configuration

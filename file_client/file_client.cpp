
#include <boost/tokenizer.hpp>

//configuration
#define SERVER_PORT		5050
#define FORCE_TO_USE_MSG_RECV_BUFFER
#define MAX_MSG_LEN		(HEAD_LEN + 1 + 4096)
	//read 4096 bytes from disk file one time will gain the best I/O performance
	//HEAD_LEN is used by the default packer
	//1 is the head length(see the protocol in file_client.h)
//configuration

#include "file_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define REQUEST_FILE	"get"

atomic_ushort completed_client_num;
int link_num = 1;
__off64_t file_size;

int main(int argc, const char* argv[])
{
	puts("usage: asio_client [link num=1]");
	if (argc > 1)
		link_num = std::min(256, std::max(atoi(argv[1]), 1));

	puts("\nthis is a file client.");
	puts("type quit to end this client.");

	std::string str;
	st_service_pump service_pump;
	file_client client(service_pump);
	for (auto i = 0; i < link_num; ++i)
	{
		auto client_ptr = client.create_client();
//		client_ptr->set_server_addr(SERVER_PORT, "::1"); //ipv6
//		client_ptr->set_server_addr(SERVER_PORT, "127.0.0.1"); //ipv4
		client_ptr->set_index(i);
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
		else if (str.size() > sizeof(REQUEST_FILE) &&
			!strncmp(REQUEST_FILE, str.data(), sizeof(REQUEST_FILE) - 1) &&
			isspace(str[sizeof(REQUEST_FILE) - 1]))
		{
			str.erase(0, sizeof(REQUEST_FILE));
			char_separator<char> sep(" \t");
			tokenizer<char_separator<char>> tok(str, sep);
			do_something_to_all(tok, [&](decltype(*std::begin(tok))& item) {
				completed_client_num.store(0);
				file_size = 0;
				auto begin_time = get_system_time().time_of_day().total_seconds();
				if (client.at(0)->get_file(item))
				{
					for (auto i = 1; i < link_num; ++i)
						client.at(i)->get_file(item);

					printf("transfer %s begin.\n", item.data());
					unsigned percent = -1;
					while (completed_client_num.load() != (unsigned short) link_num)
					{
						this_thread::sleep(get_system_time() + posix_time::milliseconds(50));
						if (file_size > 0)
						{
							auto total_rest_size = client.get_total_rest_size();
							if (total_rest_size > 0)
							{
								auto new_percent =
									(unsigned) ((file_size - total_rest_size) * 100 / file_size);
								if (percent != new_percent)
								{
									percent = new_percent;
									printf("\r%u%%", percent);
									fflush(stdout);
								}
							}
						}
					}
					auto used_time = get_system_time().time_of_day().total_seconds() - begin_time;
					if (used_time > 0)
						printf("\r100%%\ntransfer %s end, speed: " __off64_t_format "kB/s.\n",
							item.data(), file_size / 1024 / used_time);
					else
						printf("\r100%%\ntransfer %s end.\n", item.data());
				}
			});
		}
		else
			client.at(0)->talk(str);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef FORCE_TO_USE_MSG_RECV_BUFFER
#undef MAX_MSG_LEN
//restore configuration

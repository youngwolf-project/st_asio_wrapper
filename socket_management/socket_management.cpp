
#include <iostream>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_REUSE_OBJECT		//use objects pool
#define ST_ASIO_HEARTBEAT_INTERVAL	5
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
#define ST_ASIO_RECONNECT			false
#define ST_ASIO_DEFAULT_PACKER		prefix_suffix_packer
#define ST_ASIO_DEFAULT_UNPACKER	prefix_suffix_unpacker
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#include "server.h"
#include "client.h"

int main(int argc, const char* argv[])
{
	service_pump sp;
	my_server server(sp);
	my_client client(sp);

	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if ("quit" == str)
			sp.stop_service();
		else
		{
			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > tok(str, sep);
			BOOST_AUTO(iter, tok.begin());
			if (iter == tok.end())
				continue;

			if ("add" == *iter)
			{
				++iter;
				if (iter != tok.end())
					client.add_link(*iter);
			}
			else if ("del" == *iter)
			{
				++iter;
				if (iter != tok.end())
					client.shutdown_link(*iter);
			}
			else
			{
				std::string name = *iter++;
				for (; iter != tok.end(); ++iter)
					client.send_msg(name, *iter);
			}
		}
	}

    return 0;
}


#include <iostream>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_REUSE_OBJECT		//use objects pool
#define ST_ASIO_HEARTBEAT_INTERVAL	5
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
#define ST_ASIO_RECONNECT			false
#define ST_ASIO_SHARED_MUTEX_TYPE	boost::shared_mutex	//we search objects frequently, defining this can promote performance, otherwise,
#define ST_ASIO_SHARED_LOCK_TYPE	boost::shared_lock	//you should not define these two macro and st_asio_wrapper will use boost::mutex instead.

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	0
//0-default packer and unpacker, head(length) + body
//1-replaceable packer and unpacker, head(length) + body
//2-fixed length packer and unpacker
//3-prefix and/or suffix packer and unpacker

#if 1 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER replaceable_packer<>
#define ST_ASIO_DEFAULT_UNPACKER replaceable_unpacker<>
#elif 2 == PACKER_UNPACKER_TYPE
#undef ST_ASIO_HEARTBEAT_INTERVAL
#define ST_ASIO_HEARTBEAT_INTERVAL	0 //not support heartbeat
#define ST_ASIO_DEFAULT_PACKER fixed_length_packer
#define ST_ASIO_DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER prefix_suffix_packer
#define ST_ASIO_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif
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
				for (++iter; iter != tok.end(); ++iter)
					client.add_link(*iter);
			else if ("del" == *iter)
				for (++iter; iter != tok.end(); ++iter)
					client.shutdown_link(*iter);
			else
			{
				std::string name = *iter++;
				for (; iter != tok.end(); ++iter)
#if 2 == PACKER_UNPACKER_TYPE
				{
					std::string msg(1024, '$'); //the default fixed length is 1024
					client.send_msg(name, msg);
				}
#else
					client.send_msg(name, *iter);
#endif
			}
		}
	}

    return 0;
}

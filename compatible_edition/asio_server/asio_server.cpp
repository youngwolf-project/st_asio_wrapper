
//configuration
#define SERVER_PORT		9528
#define REUSE_OBJECT //use objects pool
//#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ENHANCED_STABILITY

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

//the following three macro demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg consume huge memory, for example, if we support 1M msg size, because every st_tcp_socket has a
//private unpacker which has a buffer at lest 1M size, so 1K st_tcp_socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every st_tcp_socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//#define HUGE_MSG
//#define MAX_MSG_LEN (1024 * 1024)
//#define MAX_MSG_NUM 8 //reduce buffer size to reduce memory occupation
//configuration

#include "../include/st_asio_wrapper_server.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"
#define LIST_STATUS		"status"
#define SUSPEND_COMMAND	"suspend"
#define RESUME_COMMAND	"resume"

//demonstrates how to use custom packer
//in the default behavior, each st_tcp_socket has their own packer, and cause memory waste
//at here, we make each echo_socket use the same global packer for memory saving
//notice: do not do this for unpacker, because unpacker has member variables and can't share each other
#if 1 == PACKER_UNPACKER_TYPE
BOOST_AUTO(global_packer, boost::make_shared<packer>());
#endif
#if 2 == PACKER_UNPACKER_TYPE
BOOST_AUTO(global_packer, boost::make_shared<fixed_legnth_packer>());
#endif
#if 3 == PACKER_UNPACKER_TYPE
BOOST_AUTO(global_packer, boost::make_shared<prefix_suffix_packer>());
#endif

//demonstrates how to control the type of st_server_socket_base::server from template parameters
class i_echo_server : public i_server
{
public:
	virtual void test() = 0;
};

class echo_socket : public st_server_socket_base<std::string, boost::asio::ip::tcp::socket, i_echo_server>
{
public:
	echo_socket(i_echo_server& server_) : st_server_socket_base(server_)
	{
		inner_packer(global_packer);
#if 2 == PACKER_UNPACKER_TYPE
		dynamic_cast<fixed_length_unpacker*>(&*inner_unpacker())->fixed_length(1024);
#endif
#if 3 == PACKER_UNPACKER_TYPE
		dynamic_cast<prefix_suffix_unpacker*>(&*inner_unpacker())->prefix_suffix("begin", "end");
		dynamic_cast<prefix_suffix_packer*>(&*inner_packer())->prefix_suffix("begin", "end");
#endif
	}

public:
	//because we use objects pool(REUSE_OBJECT been defined), so, strictly speaking, this virtual
	//function must be rewrote, but we don't have member variables to initialize but invoke father's
	//reset() directly, so, it can be omitted, but we keep it for possibly future using
	virtual void reset() {st_server_socket_base::reset();}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		//the type of st_server_socket_base::server now can be controlled by derived class(echo_socket),
		//which is actually i_echo_server, so, we can invoke i_echo_server::test virtual function.
		server.test();
		st_server_socket_base::on_recv_error(ec);
	}

	//msg handling: send the original msg back(echo server)
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//this virtual function doesn't exists if FORCE_TO_USE_MSG_RECV_BUFFER been defined
	virtual bool on_msg(std::string& msg) {post_msg(msg); return true;}
#endif
	//we should handle the msg in on_msg_handle for time-consuming task like this:
	virtual bool on_msg_handle(std::string& msg, bool link_down) {return send_msg(msg);}
	//please remember that we have defined FORCE_TO_USE_MSG_RECV_BUFFER, so, st_tcp_socket will directly
	//use the msg recv buffer, and we need not rewrite on_msg(), which doesn't exists any more
	//msg handling end
};

class echo_server : public st_server_base<echo_socket, st_object_pool<echo_socket>, i_echo_server>
{
public:
	echo_server(st_service_pump& service_pump_) : st_server_base(service_pump_) {}

	//from i_echo_server, pure virtual function, we must implement it.
	virtual void test() {/*puts("in echo_server::test()");*/}
};

int main() {
	puts("type quit to end these two servers.");

	std::string str;
	st_service_pump service_pump;
	st_server server_(service_pump); //only need a simple server? you can directly use st_server
	server_.set_server_addr(SERVER_PORT + 100);
	echo_server echo_server_(service_pump); //echo server

	service_pump.start_service(1);
	while(service_pump.is_running())
	{
		std::cin >> str;
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service(1);
		}
		else if (str == LIST_STATUS)
		{
			printf("normal server:\nvalid links: " size_t_format ", closed links: " size_t_format "\n",
				server_.size(), server_.closed_object_size());

			printf("echo server:\nvalid links: " size_t_format ", closed links: " size_t_format "\n",
				echo_server_.size(), echo_server_.closed_object_size());
		}
		//the following two commands demonstrate how to suspend msg dispatching, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			echo_server_.do_something_to_all(boost::bind(&echo_socket::suspend_dispatch_msg, _1, true));
		else if (str == RESUME_COMMAND)
			echo_server_.do_something_to_all(boost::bind(&echo_socket::suspend_dispatch_msg, _1, false));
		else if (str == LIST_ALL_CLIENT)
		{
			puts("clients from normal server:");
			server_.list_all_object();
			puts("clients from echo server:");
			echo_server_.list_all_object();
		}
		else
			server_.broadcast_msg(str.data(), str.size() + 1);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef REUSE_OBJECT //use objects pool
//#undef FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#undef ENHANCED_STABILITY

//#undef HUGE_MSG
//#undef MAX_MSG_LEN
//#undef MAX_MSG_NUM
//restore configuration

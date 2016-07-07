
//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_FREE_OBJECT_INTERVAL	60
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ST_ASIO_ENHANCED_STABILITY

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	1
//1-default packer and unpacker, head(length) + body
//2-fixed length unpacker
//3-prefix and suffix packer and unpacker

#if 1 == PACKER_UNPACKER_TYPE
//#define ST_ASIO_DEFAULT_PACKER replaceable_packer
//#define ST_ASIO_DEFAULT_UNPACKER replaceable_unpacker
#elif 2 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER prefix_suffix_packer
#define ST_ASIO_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif
//configuration

#include "../include/st_asio_wrapper_server.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"
#define LIST_STATUS		"status"
#define SUSPEND_COMMAND	"suspend"
#define RESUME_COMMAND	"resume"

//demonstrate how to use custom packer
//under the default behavior, each st_tcp_socket has their own packer, and cause memory waste
//at here, we make each echo_socket use the same global packer for memory saving
//notice: do not do this for unpacker, because unpacker has member variables and can't share each other
#if 1 == PACKER_UNPACKER_TYPE || 2 == PACKER_UNPACKER_TYPE
auto global_packer(boost::make_shared<ST_ASIO_DEFAULT_PACKER>());
#elif 3 == PACKER_UNPACKER_TYPE
auto global_packer(boost::make_shared<prefix_suffix_packer>());
#endif

//demonstrate how to control the type of st_server_socket_base::server from template parameter
class i_echo_server : public i_server
{
public:
	virtual void test() = 0;
};

class echo_socket : public st_server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, i_echo_server>
{
public:
	echo_socket(i_echo_server& server_) : st_server_socket_base(server_)
	{
		inner_packer(global_packer);

#if 2 == PACKER_UNPACKER_TYPE
		dynamic_cast<fixed_length_unpacker*>(&*inner_unpacker())->fixed_length(1024);
#elif 3 == PACKER_UNPACKER_TYPE
		dynamic_cast<prefix_suffix_packer*>(&*inner_packer())->prefix_suffix("begin", "end");
		dynamic_cast<prefix_suffix_unpacker*>(&*inner_unpacker())->prefix_suffix("begin", "end");
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
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	//this virtual function doesn't exists if ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER been defined
	virtual bool on_msg(out_msg_type& msg)
	{
	#if 2 == PACKER_UNPACKER_TYPE
		//we don't have fixed_length_packer, so use packer instead, but need to pack msgs with native manner.
		return post_native_msg(msg.data(), msg.size());
	#else
		return post_msg(msg.data(), msg.size());
	#endif
	}
#endif
	//we should handle msg in on_msg_handle for time-consuming task like this:
	virtual bool on_msg_handle(out_msg_type& msg, bool link_down)
	{
	#if 2 == PACKER_UNPACKER_TYPE
		//we don't have fixed_length_packer, so use packer instead, but need to pack msgs with native manner.
		return send_native_msg(msg.data(), msg.size());
	#else
		return send_msg(msg.data(), msg.size());
	#endif
	}
	//please remember that we have defined ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER, so, st_tcp_socket will directly
	//use msg recv buffer, and we need not rewrite on_msg(), which doesn't exists any more
	//msg handling end
};

class echo_server : public st_server_base<echo_socket, st_object_pool<echo_socket>, i_echo_server>
{
public:
	echo_server(st_service_pump& service_pump_) : st_server_base(service_pump_) {}

	boost::posix_time::time_duration recv_idle_time()
	{
		boost::posix_time::time_duration time_recv_idle;
		ST_THIS do_something_to_all([&time_recv_idle](object_ctype& item) {time_recv_idle += item->recv_idle_time();});

		return time_recv_idle;
	}

	//from i_echo_server, pure virtual function, we must implement it.
	virtual void test() {/*puts("in echo_server::test()");*/}
};

#if defined(_MSC_VER) || defined(__i386__)
#define fractional_seconds_format "%lld"
#else // defined(__GNUC__) && defined(__x86_64__)
#define fractional_seconds_format "%ld"
#endif

int main(int argc, const char* argv[])
{
	printf("usage: asio_server [<port=%d> [ip=0.0.0.0]]\n", ST_ASIO_SERVER_PORT);
	puts("normal server's port will be 100 larger.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	st_service_pump service_pump;
	//only need a simple server? you can directly use st_server or st_server_base.
	//because we use st_server_socket_base directly, so this server cannot support fixed_length_unpacker and prefix_suffix_packer/prefix_suffix_unpacker,
	//the reason is these packer and unpacker need additional initializations that st_server_socket_base not implemented, see echo_socket's constructor for more details.
	typedef st_server_socket_base<packer, unpacker> normal_server_socket;
	st_server_base<normal_server_socket> server_(service_pump);
	echo_server echo_server_(service_pump); //echo server

	if (argc > 2)
	{
		server_.set_server_addr(atoi(argv[1]) + 100, argv[2]);
		echo_server_.set_server_addr(atoi(argv[1]), argv[2]);
	}
	else if (argc > 1)
	{
		server_.set_server_addr(atoi(argv[1]) + 100);
		echo_server_.set_server_addr(atoi(argv[1]));
	}
	else
		server_.set_server_addr(ST_ASIO_SERVER_PORT + 100);

	service_pump.start_service(1);
	while(service_pump.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			service_pump.stop_service();
		else if (RESTART_COMMAND == str)
		{
			service_pump.stop_service();
			service_pump.start_service(1);
		}
		else if (LIST_STATUS == str)
		{
			printf("normal server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", server_.size(), server_.invalid_object_size());
			printf("echo server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", echo_server_.size(), echo_server_.invalid_object_size());
			boost::posix_time::time_duration time_recv_idle = echo_server_.recv_idle_time();
			printf("total recv idle time(echo server): %d." fractional_seconds_format " second(s)\n", time_recv_idle.total_seconds(), time_recv_idle.fractional_seconds());
		}
		//the following two commands demonstrate how to suspend msg dispatching, no matter recv buffer been used or not
		else if (SUSPEND_COMMAND == str)
			echo_server_.do_something_to_all([](echo_server::object_ctype& item) {item->suspend_dispatch_msg(true);});
		else if (RESUME_COMMAND == str)
			echo_server_.do_something_to_all([](echo_server::object_ctype& item) {item->suspend_dispatch_msg(false);});
		else if (LIST_ALL_CLIENT == str)
		{
			puts("clients from normal server:");
			server_.list_all_object();
			puts("clients from echo server:");
			echo_server_.list_all_object();
		}
		else
		{
			//broadcast series functions call pack_msg for each client respectively, because clients may used different protocols(so different type of packers, of course)
//			server_.broadcast_msg(str.data(), str.size() + 1);
			//send \0 character too, because asio_client used inflexible_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.

			//if all clients used the same protocol, we can pack msg one time, and send it repeatedly like this:
			packer p;
			auto msg = p.pack_msg(str.data(), str.size() + 1);
			//send \0 character too, because asio_client used inflexible_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
			if (!msg.empty())
				server_.do_something_to_all([&msg](st_server_base<normal_server_socket>::object_ctype& item) {item->direct_send_msg(msg);});
		}
	}

	return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_REUSE_OBJECT
#undef ST_ASIO_FREE_OBJECT_INTERVAL
#undef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
#undef ST_ASIO_ENHANCED_STABILITY
#undef ST_ASIO_DEFAULT_PACKER
#undef ST_ASIO_DEFAULT_UNPACKER
//restore configuration

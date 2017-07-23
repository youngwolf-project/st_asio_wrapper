
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9528
#define ST_ASIO_REUSE_OBJECT //use objects pool
//#define ST_ASIO_FREE_OBJECT_INTERVAL 60 //it's useless if ST_ASIO_REUSE_OBJECT macro been defined
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ST_ASIO_ENHANCED_STABILITY
#define ST_ASIO_FULL_STATISTIC //full statistic will slightly impact efficiency
//#define ST_ASIO_USE_STEADY_TIMER
//#define ST_ASIO_USE_SYSTEM_TIMER
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//#define ST_ASIO_MAX_MSG_NUM		16
//if there's a huge number of links, please reduce messge buffer via ST_ASIO_MAX_MSG_NUM macro.
//please think about if we have 512 links, how much memory we can accupy at most with default ST_ASIO_MAX_MSG_NUM?
//it's 2 * 1024 * 1024 * 512 = 1G

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
#undef ST_ASIO_HEARTBEAT_INTERVAL
#define ST_ASIO_HEARTBEAT_INTERVAL	0 //not support heartbeat
#define ST_ASIO_DEFAULT_PACKER prefix_suffix_packer
#define ST_ASIO_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"
#define LIST_STATUS		"status"
#define INCREASE_THREAD	"increase_thread"
#define DECREASE_THREAD	"decrease_thread"

//demonstrate how to use custom packer
//under the default behavior, each tcp::socket has their own packer, and cause memory waste
//at here, we make each echo_socket use the same global packer for memory saving
//notice: do not do this for unpacker, because unpacker has member variables and can't share each other
BOOST_AUTO(global_packer, boost::make_shared<ST_ASIO_DEFAULT_PACKER>());

//about congestion control
//
//in 1.3, congestion control has been removed (no post_msg nor post_native_msg anymore), this is because
//without known the business (or logic), framework cannot always do congestion control properly.
//now, users should take the responsibility to do congestion control, there're two ways:
//
//1. for receiver, if you cannot handle msgs timely, which means the bottleneck is in your business,
//    you should open/close congestion control intermittently;
//   for sender, send msgs in on_msg_send() or use sending buffer limitation (like safe_send_msg(..., false)),
//    but must not in service threads, please note.
//
//2. for sender, if responses are available (like pingpong test), send msgs in on_msg()/on_msg_handle(),
//    but this will reduce IO throughput because SOCKET's sliding window is not fully used, pleae note.
//
//asio_server chose method #1

//demonstrate how to control the type of tcp::server_socket_base::server from template parameter
class i_echo_server : public i_server
{
public:
	virtual void test() = 0;
};

typedef server_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, i_echo_server> echo_socket_base;
class echo_socket : public echo_socket_base
{
public:
	echo_socket(i_echo_server& server_) : echo_socket_base(server_)
	{
		packer(global_packer);

#if 2 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->fixed_length(1024);
#elif 3 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("begin", "end");
#endif
	}

public:
	//because we use objects pool(REUSE_OBJECT been defined), so, strictly speaking, this virtual
	//function must be rewrote, but we don't have member variables to initialize but invoke father's
	//reset() directly, so, it can be omitted, but we keep it for possibly future using
	virtual void reset() {echo_socket_base::reset();}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		//the type of tcp::server_socket_base::server now can be controlled by derived class(echo_socket),
		//which is actually i_echo_server, so, we can invoke i_echo_server::test virtual function.
		server.test();
		echo_socket_base::on_recv_error(ec);
	}

	//msg handling: send the original msg back(echo server)
	//congestion control, method #1, the peer needs its own congestion control too.
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	//this virtual function doesn't exists if ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER been defined
	virtual bool on_msg(out_msg_type& msg)
	{
		bool re = send_msg(msg.data(), msg.size());
		if (!re)
		{
			//cannot handle (send it back) this msg timely, begin congestion control
			//'msg' will be put into receiving buffer, and be dispatched via on_msg_handle() in the future
			congestion_control(true);
			//unified_out::warning_out("open congestion control."); //too many prompts will affect efficiency
		}

		return re;
	}

	virtual bool on_msg_handle(out_msg_type& msg)
	{
		bool re = send_msg(msg.data(), msg.size());
		if (re)
		{
			//successfully handled the only one msg in receiving buffer, end congestion control
			//subsequent msgs will be dispatched via on_msg() again.
			congestion_control(false);
			//unified_out::warning_out("close congestion control."); //too many prompts will affect efficiency
		}

		return re;
	}
#else
	//if we used receiving buffer, congestion control will become much simpler, like this:
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg.data(), msg.size());}
#endif
	//msg handling end
};

typedef server_base<echo_socket, object_pool<echo_socket>, i_echo_server> echo_server_base;
class echo_server : public echo_server_base
{
public:
	echo_server(service_pump& service_pump_) : echo_server_base(service_pump_) {}

	statistic get_statistic()
	{
		statistic stat;
		do_something_to_all(stat += boost::lambda::bind(&echo_socket::get_statistic, *boost::lambda::_1));

		return stat;
	}

	//from i_echo_server, pure virtual function, we must implement it.
	virtual void test() {/*puts("in echo_server::test()");*/}
};

#if ST_ASIO_HEARTBEAT_INTERVAL > 0
typedef server_socket_base<packer, unpacker> normal_socket;
#else
//demonstrate how to open heartbeat function without defining macro ST_ASIO_HEARTBEAT_INTERVAL
typedef server_socket_base<packer, unpacker> normal_socket_base;
class normal_socket : public normal_socket_base
{
public:
	normal_socket(i_server& server_) : normal_socket_base(server_) {}

protected:
	virtual bool do_start()
	{
		//demo client needs heartbeat (macro ST_ASIO_HEARTBEAT_INTERVAL been defined), pleae note that the interval (here is 5) must be equal to
		//macro ST_ASIO_HEARTBEAT_INTERVAL defined in demo client, and macro ST_ASIO_HEARTBEAT_MAX_ABSENCE must has the same value as demo client's.
		start_heartbeat(5);

		return normal_socket_base::do_start();
	}
};
#endif

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", argv[0], ST_ASIO_SERVER_PORT);
	puts("normal server's port will be 100 larger.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;
	//only need a simple server? you can directly use server or tcp::server_base, because of normal_socket,
	//this server cannot support fixed_length_packer/fixed_length_unpacker and prefix_suffix_packer/prefix_suffix_unpacker,
	//the reason is these packer and unpacker need additional initializations that normal_socket not implemented,
	//see echo_socket's constructor for more details.
	server_base<normal_socket> server_(sp);
	echo_server echo_server_(sp); //echo server

	if (argc > 3)
	{
		server_.set_server_addr(atoi(argv[2]) + 100, argv[3]);
		echo_server_.set_server_addr(atoi(argv[2]), argv[3]);
	}
	else if (argc > 2)
	{
		server_.set_server_addr(atoi(argv[2]) + 100);
		echo_server_.set_server_addr(atoi(argv[2]));
	}
	else
		server_.set_server_addr(ST_ASIO_SERVER_PORT + 100);

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));

#if 3 == PACKER_UNPACKER_TYPE
	global_packer->prefix_suffix("begin", "end");
#endif

	sp.start_service(thread_num);
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service(thread_num);
		}
		else if (LIST_STATUS == str)
		{
			printf("normal server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", server_.size(), server_.invalid_object_size());
			printf("echo server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", echo_server_.size(), echo_server_.invalid_object_size());
			puts("");
			puts(echo_server_.get_statistic().to_string().data());
		}
		else if (LIST_ALL_CLIENT == str)
		{
			puts("clients from normal server:");
			server_.list_all_object();
			puts("clients from echo server:");
			echo_server_.list_all_object();
		}
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
		else
		{
//			/*
			//broadcast series functions call pack_msg for each client respectively, because clients may used different protocols(so different type of packers, of course)
			server_.sync_broadcast_msg(str.data(), str.size() + 1);
			//send \0 character too, because demo client used basic_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
//			*/
			/*
			//broadcast series functions call pack_msg for each client respectively, because clients may used different protocols(so different type of packers, of course)
			server_.broadcast_msg(str.data(), str.size() + 1);
			//send \0 character too, because demo client used basic_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
			*/
			/*
			//if all clients used the same protocol, we can pack msg one time, and send it repeatedly like this:
			packer p;
			packer::msg_type msg;
			//send \0 character too, because demo client used basic_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
			if (p.pack_msg(msg, str.data(), str.size() + 1))
				server_.do_something_to_all(boost::bind((bool (normal_server_socket::*)(packer::msg_ctype&, bool)) &normal_server_socket::direct_send_msg, _1, boost::cref(msg), false));
			*/
			/*
			//if demo client is using stream_unpacker
			if (!str.empty())
				server_.do_something_to_all(boost::bind((bool (normal_server_socket::*)(packer::msg_ctype&, bool)) &normal_server_socket::direct_send_msg, _1, boost::cref(str), false));
			*/
		}
	}

	return 0;
}

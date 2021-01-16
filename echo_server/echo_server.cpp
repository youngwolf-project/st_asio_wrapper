
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9528
#define ST_ASIO_REUSE_OBJECT //use objects pool
//#define ST_ASIO_FREE_OBJECT_INTERVAL 60 //it's useless if ST_ASIO_REUSE_OBJECT macro been defined
//#define ST_ASIO_SYNC_DISPATCH //do not open this feature, see below for more details
#define ST_ASIO_DISPATCH_BATCH_MSG
//#define ST_ASIO_FULL_STATISTIC //full statistic will slightly impact efficiency
#define ST_ASIO_USE_STEADY_TIMER
//#define ST_ASIO_USE_SYSTEM_TIMER
#define ST_ASIO_ALIGNED_TIMER
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//#define ST_ASIO_MAX_SEND_BUF	65536
//#define ST_ASIO_MAX_RECV_BUF	65536
//if there's a huge number of links, please reduce messge buffer via ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF macro.
//please think about if we have 512 links, how much memory we can accupy at most with default ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF?
//it's 2 * 1M * 512 = 1G

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	0
//0-default packer and unpacker, head(length) + body
//1-packer2 and unpacker2, head(length) + body
//2-fixed length packer and unpacker
//3-prefix and/or suffix packer and unpacker

#if 0 == PACKER_UNPACKER_TYPE
#define ST_ASIO_HUGE_MSG
#define ST_ASIO_MSG_BUFFER_SIZE 1000000
#define ST_ASIO_MAX_SEND_BUF (10 * ST_ASIO_MSG_BUFFER_SIZE)
#define ST_ASIO_MAX_RECV_BUF (10 * ST_ASIO_MSG_BUFFER_SIZE)
#define ST_ASIO_DEFAULT_UNPACKER flexible_unpacker<std::string>
//this unpacker only pre-allocated a buffer of 4000 bytes, but it can parse messages up to ST_ASIO_MSG_BUFFER_SIZE (here is 1000000) bytes,
//it works as the default unpacker for messages <= 4000, otherwise, it works as non_copy_unpacker
#elif 1 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER packer2<unique_buffer<basic_buffer>, basic_buffer, ext::packer<basic_buffer> >
//sometime, the default packer brings name conflict with the socket's packer member function, prefix namespace can resolve this conflict.
#define ST_ASIO_DEFAULT_UNPACKER unpacker2<unique_buffer<basic_buffer>, basic_buffer, flexible_unpacker<> >
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

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

//demonstrate how to use custom packer
//under the default behavior, each tcp::socket has their own packer, and cause memory waste
//at here, we make each echo_socket use the same global packer for memory saving
//notice: do not do this for unpacker, because unpacker has member variables and can't share each other
boost::shared_ptr<ST_ASIO_DEFAULT_PACKER> global_packer = boost::make_shared<ST_ASIO_DEFAULT_PACKER>();

//demonstrate how to control the type of tcp::server_socket_base::server from template parameter
class i_echo_server : public i_server
{
public:
	virtual void test() = 0;
};

class echo_socket : public server_socket2<i_echo_server>
{
private:
	typedef server_socket2<i_echo_server> super;

public:
	echo_socket(i_echo_server& server_) : super(server_)
	{
		packer(global_packer);

#if 3 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("begin", "end");
#endif
	}

public:
	//because we use objects pool(REUSE_OBJECT been defined), so, strictly speaking, this virtual
	//function must be rewrote, but we don't have member variables to initialize but invoke father's
	//reset() directly, so, it can be omitted, but we keep it for possibly future using
	virtual void reset() {super::reset();}

protected:
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		//the type of tcp::server_socket_base::server now can be controlled by derived class(echo_socket),
		//which is actually i_echo_server, so, we can invoke i_echo_server::test virtual function.
		get_server().test();
		super::on_recv_error(ec);
	}

	//msg handling: send the original msg back(echo server)
#ifdef ST_ASIO_SYNC_DISPATCH //do not open this feature
	//do not hold msg_can for further using, return from on_msg as quickly as possible
	//access msg_can freely within this callback, it's always thread safe.
	virtual size_t on_msg(list<out_msg_type>& msg_can)
	{
		if (!is_send_buffer_available())
			return 0;
		//here if we cannot handle all messages in msg_can, do not use sync message dispatching except we can bear message disordering,
		//this is because on_msg_handle can be invoked concurrently with the next on_msg (new messages arrived) and then disorder messages.
		//and do not try to handle all messages here (just for echo_server's business logic) because:
		//1. we can not use safe_send_msg as i said many times, we should not block service threads.
		//2. if we use true can_overflow to call send_msg, then buffer usage will be out of control, we should not take this risk.

		//following statement can avoid one memory replication if the type of out_msg_type and in_msg_type are identical.
		for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter) send_msg(*iter, true);
		msg_can.clear();

		return 1;
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number)
		//if we handled nothing, return a positive number is also okey but will very slightly impact performance (if msg_can is not empty), return 0 is suggested
	}
#endif

#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	//do not hold msg_can for further using, access msg_can and return from on_msg_handle as quickly as possible
	//can only access msg_can via functions that marked as 'thread safe', if you used non-lock queue, its your responsibility to guarantee
	// that new messages will not come until we returned from this callback (for example, pingpong test).
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		if (!is_send_buffer_available())
			return 0;

		out_container_type tmp_can;
		msg_can.move_items_out(tmp_can, 10); //don't be too greedy, here is in a service thread, we should not block this thread for a long time

#if 2 == PACKER_UNPACKER_TYPE //the type of out_msg_type and in_msg_type are not identical
		for (BOOST_AUTO(iter, tmp_can.end()); iter != tmp_can.end(); ++iter)
			send_msg(*iter, true);
#else
		//following statement can avoid one memory replication if the type of out_msg_type and in_msg_type are identical.
		for (BOOST_AUTO(iter, tmp_can.begin()); iter != tmp_can.end(); ++iter) send_msg(*iter, true);
#endif
		return tmp_can.size();
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number), else, return 0
		//if we handled nothing, but want to re-dispatch messages immediately, return 1
	}
#else
	//following statement can avoid one memory replication if the type of out_msg_type and in_msg_type are identical.
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg);}
#endif
	//msg handling end
};

typedef server_base<echo_socket, object_pool<echo_socket>, i_echo_server> echo_server_base;
class echo_server : public echo_server_base
{
public:
	echo_server(service_pump& service_pump_) : echo_server_base(service_pump_) {}

protected:
	//from i_echo_server, pure virtual function, we must implement it.
	virtual void test() {/*puts("in echo_server::test()");*/}
};

#if ST_ASIO_HEARTBEAT_INTERVAL > 0
typedef server_socket_base<packer<>, unpacker<> > normal_socket;
#else
//demonstrate how to open heartbeat function without defining macro ST_ASIO_HEARTBEAT_INTERVAL
typedef server_socket_base<packer<>, unpacker<> > normal_socket_base;
class normal_socket : public normal_socket_base
{
public:
	normal_socket(i_server& server_) : normal_socket_base(server_) {}

protected:
	//demo client needs heartbeat (macro ST_ASIO_HEARTBEAT_INTERVAL been defined), please note that the interval (here is 5) must be equal to
	//macro ST_ASIO_HEARTBEAT_INTERVAL defined in demo client, and macro ST_ASIO_HEARTBEAT_MAX_ABSENCE must has the same value as demo client's.
	virtual void on_connect() {start_heartbeat(5);}
};
#endif

//demonstrate how to accept just one client at server endpoint
typedef server_base<normal_socket> normal_server_base;
class normal_server : public normal_server_base
{
public:
	normal_server(service_pump& service_pump_) : normal_server_base(service_pump_) {}

protected:
	virtual int async_accept_num() {return 1;}
	virtual bool on_accept(object_ctype& socket_ptr) {stop_listen(); return true;}
};

typedef server_socket_base<packer<>, unpacker<> > short_socket_base;
class short_connection : public short_socket_base
{
public:
	short_connection(i_server& server_) : short_socket_base(server_) {}

protected:
	//msg handling
#ifdef ST_ASIO_SYNC_DISPATCH
	//do not hold msg_can for further using, return from on_msg as quickly as possible
	//access msg_can freely within this callback, it's always thread safe.
	virtual size_t on_msg(list<out_msg_type>& msg_can) {bool re = short_socket_base::on_msg(msg_can); force_shutdown(); return re;}
#endif

#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	//do not hold msg_can for further using, access msg_can and return from on_msg_handle as quickly as possible
	//can only access msg_can via functions that marked as 'thread safe', if you used non-lock queue, its your responsibility to guarantee
	// that new messages will not come until we returned from this callback (for example, pingpong test).
	virtual size_t on_msg_handle(out_queue_type& msg_can) {size_t re = short_socket_base::on_msg_handle(msg_can); force_shutdown(); return re;}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {bool re = short_socket_base::on_msg_handle(msg); force_shutdown(); return re;}
#endif
	//msg handling end
};
/*
class test_aci_ref : public tracked_executor
{
public:
	test_aci_ref(boost::asio::io_context& io_context_) : tracked_executor(io_context_) {}
	void start() {post(boost::bind(&test_aci_ref::handler, this)); printf("after post invocation, the aci ref is %ld\n", get_aci_ref());}

private:
	void handler() {printf("in post handler, the aci ref is %ld, is last async call is %d\n", get_aci_ref(), is_last_async_call());}
};

class my_timer : public timer<tracked_executor>
{
public:
    my_timer(boost::asio::io_context& io_context_) :timer<tracked_executor>(io_context_) {}
    void start() {set_timer(0, 10, boost::bind(&my_timer::handler, this, boost::placeholders::_1)); printf("after set_timer invocation, the aci ref is %ld\n", get_aci_ref());}

private:
    bool handler(tid id)
        {printf("in timer handler, the aci ref is %ld, is last async call is %d\n", get_aci_ref(), is_last_async_call()); return false;}
};
*/
int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=1> [<port=%d> [ip=0.0.0.0]]]\n", argv[0], ST_ASIO_SERVER_PORT);
	puts("normal server's port will be 100 larger.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");
/*
	printf("the ST_ASIO_MIN_ACI_REF is %d\n", ST_ASIO_MIN_ACI_REF);
	boost::asio::io_context context;
	test_aci_ref t(context);
	t.start();
	my_timer tt(context);
	tt.start();
	context.run();
*/
	service_pump sp;
	echo_server echo_server_(sp); //echo server

	//demonstrate how to use singel_service
	//because of normal_socket, this server cannot support fixed_length_packer/fixed_length_unpacker and prefix_suffix_packer/prefix_suffix_unpacker,
	//the reason is these packer and unpacker need additional initializations that normal_socket not implemented, see echo_socket's constructor for more details.
	single_service_pump<normal_server> normal_server_;
	single_service_pump<server_base<short_connection> > short_server;

	unsigned short port = ST_ASIO_SERVER_PORT;
	std::string ip;
	if (argc > 2)
		port = (unsigned short) atoi(argv[2]);
	if (argc > 3)
		ip = argv[3];

	normal_server_.set_server_addr(port + 100, ip);
	short_server.set_server_addr(port + 200, ip);
	echo_server_.set_server_addr(port, ip);

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));

#if 3 == PACKER_UNPACKER_TYPE
	global_packer->prefix_suffix("begin", "end");
#endif

	sp.start_service(thread_num);
	normal_server_.start_service(1);
	short_server.start_service(1);
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
		{
			sp.stop_service();
			normal_server_.stop_service();
			short_server.stop_service();
		}
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service(thread_num);
		}
		else if (STATISTIC == str)
		{
			printf("normal server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", normal_server_.size(), normal_server_.invalid_object_size());
			printf("echo server, link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n\n", echo_server_.size(), echo_server_.invalid_object_size());
			static statistic last_stat;
			statistic this_stat = echo_server_.get_statistic();
			puts((this_stat - last_stat).to_string().data());
			last_stat = this_stat;
		}
		else if (STATUS == str)
		{
			normal_server_.list_all_status();
			echo_server_.list_all_status();
		}
		else if (LIST_ALL_CLIENT == str)
		{
			puts("clients from normal server:");
			normal_server_.list_all_object();
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
			normal_server_.broadcast_msg(str.data(), str.size() + 1);
			//send \0 character too, because demo client used basic_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
//			*/
			/*
			//if all clients used the same protocol, we can pack msg one time, and send it repeatedly like this:
			packer p;
			packer::msg_type msg;
			//send \0 character too, because demo client used basic_buffer as its msg type, it will not append \0 character automatically as std::string does,
			//so need \0 character when printing it.
			if (p.pack_msg(msg, str.data(), str.size() + 1))
				((normal_server&) normal_server_).do_something_to_all(boost::bind((bool (normal_socket::*)(packer::msg_ctype&, bool, bool)) &normal_socket::direct_send_msg,
					boost::placeholders::_1, boost::cref(msg), false, false));
			*/
			/*
			//if demo client is using stream_unpacker
			((normal_server&) normal_server_).do_something_to_all(boost::bind((bool (normal_socket::*)(packer::msg_ctype&, bool, bool)) &normal_socket::direct_send_msg,
				boost::placeholders::_1, boost::cref(str), false, false));
			//or
			normal_server_.broadcast_native_msg(str);
			*/
		}
	}

	return 0;
}

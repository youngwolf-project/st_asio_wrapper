
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9528
#define ST_ASIO_SYNC_SEND
#define ST_ASIO_SYNC_RECV
//#define ST_ASIO_PASSIVE_RECV //because we not defined this macro, this demo will use mix model to receive messages, which means
							   //some messages will be dispatched via on_msg_handle(), some messages will be returned via sync_recv_msg(),
							   //if the server send messages quickly enough, you will see them cross together.
#define ST_ASIO_HIDE_WARNINGS
#define ST_ASIO_ALIGNED_TIMER
#define ST_ASIO_CUSTOM_LOG
#define ST_ASIO_DEFAULT_UNPACKER non_copy_unpacker
//#define ST_ASIO_DEFAULT_UNPACKER stream_unpacker

//the following two macros demonstrate how to support huge msg(exceed 65535 - 2).
#define ST_ASIO_HUGE_MSG
#define ST_ASIO_MSG_BUFFER_SIZE	1000000
#define ST_ASIO_HEARTBEAT_INTERVAL 5 //if use stream_unpacker, heartbeat messages will be treated as normal messages,
									 //because stream_unpacker doesn't support heartbeat
//configuration

//demonstrate how to use custom log system:
//use your own code to replace the following all_out_helper2 macros, then you can record logs according to your wishes.
//custom log should be defined(or included) before including any st_asio_wrapper header files except base.h
//notice: please don't forget to define the ST_ASIO_CUSTOM_LOG macro.
#include "../include/base.h"
using namespace st_asio_wrapper;

class unified_out
{
public:
	static void fatal_out(const char* fmt, ...) {all_out_helper2("fatal");}
	static void error_out(const char* fmt, ...) {all_out_helper2("error");}
	static void warning_out(const char* fmt, ...) {all_out_helper2("warning");}
	static void info_out(const char* fmt, ...) {all_out_helper2("info");}
	static void debug_out(const char* fmt, ...) {all_out_helper2("debug");}
};

#include "../include/ext/tcp.h"
#include "../include/ext/callbacks.h"
using namespace st_asio_wrapper;
//using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;
using namespace st_asio_wrapper::ext::tcp::proxy;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"
#define STATISTIC		"statistic"

//we only want close reconnecting mechanism on these sockets, so it cannot be done by defining macro ST_ASIO_RECONNECT to false
///*
//method 1
class short_client : public st_asio_wrapper::tcp::multi_client_base<callbacks::c_socket<socks4::client_socket> >
{
public:
	short_client(service_pump& service_pump_) : multi_client_base<callbacks::c_socket<socks4::client_socket> >(service_pump_) {set_server_addr(ST_ASIO_SERVER_PORT);}

	void set_server_addr(unsigned short _port, const std::string& _ip = ST_ASIO_SERVER_IP) {port = _port; ip = _ip;}
	bool send_msg(const std::string& msg) {return send_msg(msg, port, ip);}
	bool send_msg(std::string& msg) {return send_msg(msg, port, ip);}
	bool send_msg(const std::string& msg, unsigned short port, const std::string& ip) {std::string unused(msg); return send_msg(unused, port, ip);}
	bool send_msg(std::string& msg, unsigned short port, const std::string& ip)
	{
		BOOST_AUTO(socket_ptr, add_socket(port, ip));
		if (!socket_ptr)
			return false;

		//register event callback from outside of the socket, it also can be done from inside of the socket, see echo_server for more details
		socket_ptr->register_on_connect(boost::bind(&socks4::client_socket::set_reconnect, boost::placeholders::_1, false), true); //close reconnection mechanism

		//without following setting, socks4::client_socket will be downgraded to normal client_socket
		//socket_ptr->set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK4 server address
		return socket_ptr->send_msg(msg);
	}

private:
	unsigned short port;
	std::string ip;
};
//*/
//method 2
/*
typedef multi_client_base<socks4::client_socket, callbacks::object_pool<object_pool<socks4::client_socket> > > short_client_base;
class short_client : public short_client_base
{
public:
	short_client(service_pump& service_pump_) : short_client_base(service_pump_) {set_server_addr(ST_ASIO_SERVER_PORT);}

	void set_server_addr(unsigned short _port, const std::string& _ip = ST_ASIO_SERVER_IP) {port = _port; ip = _ip;}
	bool send_msg(const std::string& msg) {return send_msg(msg, port, ip);}
	bool send_msg(std::string& msg) {return send_msg(msg, port, ip);}
	bool send_msg(const std::string& msg, unsigned short port, const std::string& ip) {std::string unused(msg); return send_msg(unused, port, ip);}
	bool send_msg(std::string& msg, unsigned short port, const std::string& ip)
	{
		BOOST_AUTO(socket_ptr, add_socket(port, ip));
		if (!socket_ptr)
			return false;

		//we now can not call register_on_connect on socket_ptr, since socket_ptr is not wrapped by callbacks::c_socket
		//register event callback from outside of the socket, it also can be done from inside of the socket, see echo_server for more details
		//socket_ptr->register_on_connect(boost::bind(&socks4::client_socket::set_reconnect, boost::placeholders::_1, false), true); //close reconnection mechanism

		//without following setting, socks4::client_socket will be downgraded to normal client_socket
		//socket_ptr->set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK4 server address
		return socket_ptr->send_msg(msg);
	}

private:
	unsigned short port;
	std::string ip;
};
void on_create(object_pool<socks4::client_socket>* op, object_pool<socks4::client_socket>::object_ctype& socket_ptr) {socket_ptr->set_reconnect(false);}
*/

void sync_recv_thread(client_socket& client)
{
	ST_ASIO_DEFAULT_UNPACKER::container_type msg_can;
	sync_call_result re = SUCCESS;
	do
	{
		re = client.sync_recv_msg(msg_can, 50); //st_asio_wrapper will not maintain messages in msg_can anymore after sync_recv_msg return, please note.
		if (SUCCESS == re)
		{
			for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
				printf("sync recv(" ST_ASIO_SF ") : %s\n", iter->size(), iter->data());
			msg_can.clear(); //sync_recv_msg just append new message(s) to msg_can, please note.
		}
	} while (SUCCESS == re || TIMEOUT == re);
	puts("sync recv end.");
}

int main(int argc, const char* argv[])
{
	printf("usage: %s [<port=%d> [ip=%s]]\n", argv[0], ST_ASIO_SERVER_PORT + 100, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	//demonstrate how to use single_service_pump
	single_service_pump<socks5::single_client> client;
	//singel_service_pump also is a service_pump, this let us to control client2 via client
	short_client client2(client); //without single_client, we need to define ST_ASIO_AVOID_AUTO_STOP_SERVICE macro to forbid service_pump stopping services automatically
	//method 2
	/*
	//close reconnection mechanism, 2 approaches
	client2.register_on_create(boost::bind(on_create, boost::placeholders::_1, boost::placeholders::_2));
	client2.register_on_create(on_create);
	*/
	//method 2

//	argv[2] = "::1" //ipv6
//	argv[2] = "127.0.0.1" //ipv4
	unsigned short port = ST_ASIO_SERVER_PORT + 100;
	std::string ip = ST_ASIO_SERVER_IP;
	if (argc > 1)
		port = (unsigned short) atoi(argv[1]);
	if (argc > 2)
		ip = argv[2];

	client.set_server_addr(port, ip);
	//without following setting, socks5::single_client will be downgraded to normal single_client
	//client.set_target_addr(9527, "172.27.0.14"); //target server address, original server address becomes SOCK5 server address
	//client.set_auth("st_asio_wrapper", "st_asio_wrapper"); //can be omitted if the SOCKS5 server support non-auth
	client2.set_server_addr(port + 100, ip);

	client.start_service();
	boost::thread t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(client)));
	while(client.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			client.stop_service();
			t.join();
		}
		else if (RESTART_COMMAND == str)
		{
			client.stop_service();
			t.join();

			t = boost::thread(boost::bind(&sync_recv_thread, boost::ref(client)));
			client.start_service();
		}
		else if (RECONNECT == str)
			client.graceful_shutdown(true);
		else if (STATISTIC == str)
			puts(client.get_statistic().to_string().data());
		else
		{
			std::string tmp_str = str + " (from short client)"; //make a backup of str first, because it will be moved into client

			//each of following 4 tests is exclusive from other 3, because str will be moved into client (to reduce one memory replication)
			//to avoid this, call other overloads that accept const references.
			//we also have another 4 tests that send native messages (if the server used stream_unpacker) not listed, you can try to complete them.
			//test #1
			std::string msg2(" (from normal client)");
			sync_call_result re = client.sync_safe_send_msg(str, msg2, 100); //new feature introduced in 2.2.0
			if (SUCCESS != re)
				printf("sync send result: %d\n", re);
			//test #2
			//client.sync_send_msg(str, msg2, 100); //new feature introduced in 2.2.0
			//test #3
			//client.safe_send_msg(str, msg2); //new feature introduced in 2.2.0
			//test #4
			//client.send_msg(str, msg2); //new feature introduced in 2.2.0

			client2.send_msg(tmp_str);
		}
	}

	return 0;
}

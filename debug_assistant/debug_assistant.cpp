
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT	9527
#define ST_ASIO_REUSE_OBJECT	//use objects pool
#define ST_ASIO_MAX_OBJECT_NUM	1024
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
//configuration

#include "../include/ext/tcp.h"
#include "../include/ext/udp.h"
#include "../include/ext/ssl.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
//sing namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define STATUS			"status"

template<typename ObjectPool>
class timed_object_pool : public ObjectPool
{
private:
	typedef typename ObjectPool::tid tid; //old gcc needs this

public:
	template<typename Arg> timed_object_pool(Arg& arg) : ObjectPool(arg) {}
	template<typename Arg1, typename Arg2> timed_object_pool(Arg1& arg1, Arg2& arg2) : ObjectPool(arg1, arg2) {}

protected:
	~timed_object_pool() {}

	void start()
	{
		ObjectPool::start();
		ST_THIS set_timer(ObjectPool::TIMER_END, 1000 * 60, boost::bind(&timed_object_pool::timer_handler, this, boost::placeholders::_1));
	}

private:
	bool timeout_check(typename ObjectPool::object_ctype& object_ptr, const time_t& now)
	{
		if (object_ptr->get_statistic().last_recv_time + 10 * 60 >= now)
			return false;

		object_ptr->force_shutdown();
		return true;
	}

	bool timer_handler(tid id)
	{
		time_t now = time(NULL);
		ST_THIS do_something_to_all(boost::bind(&timed_object_pool::timeout_check, this, boost::placeholders::_1, boost::ref(now)));
		return true;
	}
};

class echo_socket : public ext::tcp::server_socket
{
public:
	template<typename Arg> echo_socket(Arg& arg) : ext::tcp::server_socket(arg) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg);}
	//msg handling end
};

class echo_stream_socket : public server_socket_base<dummy_packer<std::string>, stream_unpacker>
{
public:
	template<typename Arg> echo_stream_socket(Arg& arg) : server_socket_base(arg) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return send_native_msg(msg);}
	//msg handling end
};

class echo_ssl_socket : public ext::ssl::server_socket
{
public:
	template<typename Arg1, typename Arg2> echo_ssl_socket(Arg1& arg1, Arg2& arg2) : ext::ssl::server_socket(arg1, arg2) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return send_msg(msg);}
	//msg handling end
};

class single_udp_service : public ext::udp::single_socket_service
{
public:
	template<typename Arg> single_udp_service(Arg& arg) : ext::udp::single_socket_service(arg) {}

protected:
	//msg handling: send the original msg back(echo server)
	virtual bool on_msg_handle(out_msg_type& msg) {return direct_send_msg(msg);} //packer and unpacker have the same type of message
	//virtual bool on_msg_handle(out_msg_type& msg) {return send_native_msg(msg.peer_addr, msg);} //packer and unpacker have different types of message
	//msg handling end
};

#ifndef _WIN32
void run_daemon(boost::asio::signal_set& signal_receiver, const boost::system::error_code& ec, int signal_number,
	service_pump& sp,
	server_base<echo_socket, timed_object_pool<object_pool<echo_socket> > >& echo_server,
	server_base<echo_stream_socket, timed_object_pool<object_pool<echo_stream_socket> > >& echo_stream_server)
{
	if (!ec)
	{
		if (SIGUSR1 == signal_number)
		{
			echo_server.list_all_status();
			echo_stream_server.list_all_status();
		}
		else
			return sp.end_service();
	}

	signal_receiver.async_wait(boost::bind(run_daemon, boost::ref(signal_receiver), boost::placeholders::_1, boost::placeholders::_2, boost::ref(sp), boost::ref(echo_server), boost::ref(echo_stream_server)));
}
#endif

int main(int argc, const char* argv[])
{
	bool daemon = false;
	if (argc >= 2 && 0 == strcmp(argv[1], "-d"))
	{
#ifdef _WIN32
		puts("on windows, -d is not supported!");
		return 1;
#endif

		//setbuf(stdout, NULL);
		setvbuf(stdout, NULL, _IOLBF, 0);
		daemon = true;
	}

	puts("echo server with length (2 bytes big endian) + body protocol: 9527\n"
		"echo server with non-protocol: 9528\n"
		"echo server udp: 9528\n"
		"echo ssl server with length (2 bytes big endian) + body protocol: 9529\n"
		"type " QUIT_COMMAND " to end.");

	service_pump sp;
#ifndef ASCS_DECREASE_THREAD_AT_RUNTIME
	//if you want to decrease service thread at runtime, then you cannot use multiple io_context, if somebody indeed needs it, please let me know.
	//with multiple io_context, the number of service thread must be bigger than or equal to the number of io_context, please note.
	//with multiple io_context, please also define macro ASCS_AVOID_AUTO_STOP_SERVICE.
	sp.set_io_context_num(8);
#endif
	server_base<echo_socket, timed_object_pool<object_pool<echo_socket> > > echo_server(sp);
	server_base<echo_stream_socket, timed_object_pool<object_pool<echo_stream_socket> > > echo_stream_server(sp);
	single_udp_service udp_service(sp);
	st_asio_wrapper::ssl::server_base<echo_ssl_socket, timed_object_pool<st_asio_wrapper::ssl::object_pool<echo_ssl_socket> > > echo_ssl_server(sp, boost::asio::ssl::context::sslv23_server);
	echo_ssl_server.context().set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	echo_ssl_server.context().set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	echo_ssl_server.context().load_verify_file("client_certs/server.crt");
	echo_ssl_server.context().use_certificate_chain_file("certs/server.crt");
	echo_ssl_server.context().use_private_key_file("certs/server.key", boost::asio::ssl::context::pem);
	echo_ssl_server.context().use_tmp_dh_file("certs/dh2048.pem");

	echo_stream_server.set_server_addr(9528);
	udp_service.set_local_addr(9528);
	echo_ssl_server.set_server_addr(9529);

#ifndef _WIN32
	if (daemon)
	{
		boost::asio::signal_set signal_receiver(sp.assign_io_context(), SIGINT, SIGTERM, SIGUSR1);
		signal_receiver.async_wait(boost::bind(run_daemon, boost::ref(signal_receiver), boost::placeholders::_1, boost::placeholders::_2, boost::ref(sp), boost::ref(echo_server), boost::ref(echo_stream_server)));

		sp.run_service();
		return 0;
	}
#else
	(void) daemon;
#endif

	sp.start_service();
	while (sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (STATUS == str)
		{
			echo_server.list_all_status();
			echo_stream_server.list_all_status();
			echo_ssl_server.list_all_status();
		}
		else
		{
		}
	}

	return 0;
}

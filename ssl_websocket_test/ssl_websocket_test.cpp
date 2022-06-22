
#include <iostream>

//configuration
//#define ST_ASIO_SYNC_RECV
//#define ST_ASIO_SYNC_SEND
//configuration

#include "../include/ext/ssl_websocket.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext::websocket::ssl;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"

int main(int argc, const char* argv[])
{
	puts("Demonstrate how to use ssl websocket with st_asio_wrapper.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	service_pump sp;

	server server_(sp, boost::asio::ssl::context::sslv23_server);
	server_.set_start_object_id(1000);

	server_.context().set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	server_.context().set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	server_.context().load_verify_file("client_certs/server.crt");
	server_.context().use_certificate_chain_file("certs/server.crt");
	server_.context().use_private_key_file("certs/server.key", boost::asio::ssl::context::pem);
	server_.context().use_tmp_dh_file("certs/dh2048.pem");

	multi_client client_(sp, boost::asio::ssl::context::sslv23_client);
	client_.context().set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	client_.context().set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	client_.context().load_verify_file("certs/server.crt");
	client_.context().use_certificate_chain_file("client_certs/server.crt");
	client_.context().use_private_key_file("client_certs/server.key", boost::asio::ssl::context::pem);
	client_.context().use_tmp_dh_file("client_certs/dh2048.pem");

	//please config the ssl context before creating any clients.
	client_.add_socket();
	client_.add_socket();

	sp.start_service(2);
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();

			//add all clients back
			client_.add_socket();
			client_.add_socket();
			sp.start_service();
		}
		else if (RECONNECT == str)
			//client_.force_shutdown(true);
			client_.graceful_shutdown(true);
		else
			//client_.broadcast_native_msg(str);
			server_.broadcast_native_msg(str);
	}

	return 0;
}

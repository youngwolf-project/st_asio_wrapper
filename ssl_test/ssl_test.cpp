
#include <iostream>

//configuration
#define ST_ASIO_SERVER_PORT		9527
#define ST_ASIO_REUSE_OBJECT //use objects pool
//#define ST_ASIO_DEFAULT_PACKER packer2<unique_buffer<std::string>, std::string>
//#define ST_ASIO_DEFAULT_UNPACKER unpacker2<>
#define ST_ASIO_HEARTBEAT_INTERVAL 5 //SSL has supported heartbeat because we used user data instead of OOB to implement
									 //heartbeat since 1.4.0
//configuration

#include "../include/ext/ssl.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext::ssl;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT		"reconnect"
#define SHOW_ALL_LINKS	"show_all_links"
#define SHUTDOWN_LINK	"shutdown"

int main(int argc, const char* argv[])
{
	puts("Directories 'certs' and 'client_certs' must available in current directory.");
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

///*
	//method #1
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
//*/
/*
	//method #2
	//to use single_client, we must construct ssl context first.
	boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
	ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	ctx.set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ctx.load_verify_file("certs/server.crt");
	ctx.use_certificate_chain_file("client_certs/server.crt");
	ctx.use_private_key_file("client_certs/server.key", boost::asio::ssl::context::pem);
	ctx.use_tmp_dh_file("client_certs/dh2048.pem");

	single_client client_(sp, ctx);
*/
	sp.start_service();
	while(sp.is_running())
	{
		std::string str;
		std::cin >> str;
		if (str.empty())
			;
		else if (QUIT_COMMAND == str)
		{
			sp.stop_service(&client_);
			sp.stop_service();
		}
		else if (SHOW_ALL_LINKS == str)
		{
			puts("server:");
			printf("link #: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", server_.size(), server_.invalid_object_size());
			server_.list_all_object();

			//if you used single_client, comment out following codes.
			puts("\nclient:");
			printf("link #: " ST_ASIO_SF ", valid links: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n", client_.size(), client_.valid_size(), client_.invalid_object_size());
			client_.list_all_object();
		}
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service(&client_);
			sp.stop_service();

			//add all clients back
			client_.add_socket();
			client_.add_socket();
			sp.start_service();
		}
		else if (RECONNECT == str)
//			server_.graceful_shutdown();
			client_.graceful_shutdown(true);
		else if (SHUTDOWN_LINK == str)
			//async shutdown, client will reconnect to the server
//			server_.at(0)->graceful_shutdown();
//			server_.at(0)->force_shutdown();

			//sync shutdown, client will reconnect to the server
//			server_.at(0)->graceful_shutdown(true);

			//sync shutdown and reconnect to the server
			client_.at(0)->graceful_shutdown(true);
//			client_.at(0)->force_shutdown(true);
//			client_.graceful_shutdown(true); //if you used single_client
//			client_.force_shutdown(true); //if you used single_client

			//async shutdown and reconnect to the server
//			client_.at(0)->graceful_shutdown(true, false);
//			client_.graceful_shutdown(true, false); //if you used single_client

			//sync shutdown and not reconnect to the server
//			client_.at(0)->graceful_shutdown();
//			client_.at(0)->force_shutdown();
//			client_.graceful_shutdown(client_.at(0));
//			client_.force_shutdown(client_.at(0));
//			client_.graceful_shutdown(); //if you used single_client
//			client_.force_shutdown(); //if you used single_client

			//async shutdown and not reconnect to the server
//			client_.at(0)->graceful_shutdown(false, false);
//			client_.graceful_shutdown(client_.at(0), false);
//			client_.graceful_shutdown(false, false); //if you used single_client
		else
			server_.broadcast_msg(str);
	}

	return 0;
}


//configuration
#define ST_ASIO_SERVER_PORT		9527
//#define ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ST_ASIO_ENHANCED_STABILITY
//#define ST_ASIO_DEFAULT_PACKER replaceable_packer
//#define ST_ASIO_DEFAULT_UNPACKER replaceable_unpacker
//configuration

#include "../include/st_asio_wrapper_ssl.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT_COMMAND "reconnect"

int main(int argc, const char* argv[])
{
	puts("Directories certs and client_certs must be in the current directory.");
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	st_service_pump service_pump;

	st_ssl_server server_(service_pump, boost::asio::ssl::context::sslv23_server);
	server_.ssl_context().set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	server_.ssl_context().set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	server_.ssl_context().load_verify_file("client_certs/server.crt");
	server_.ssl_context().use_certificate_chain_file("certs/server.crt");
	server_.ssl_context().use_private_key_file("certs/server.key", boost::asio::ssl::context::pem);
	server_.ssl_context().use_tmp_dh_file("certs/dh1024.pem");

///*
	//method #1
	st_ssl_tcp_client ssl_client(service_pump, boost::asio::ssl::context::sslv23_client);
	ssl_client.ssl_context().set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	ssl_client.ssl_context().set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ssl_client.ssl_context().load_verify_file("certs/server.crt");
	ssl_client.ssl_context().use_certificate_chain_file("client_certs/server.crt");
	ssl_client.ssl_context().use_private_key_file("client_certs/server.key", boost::asio::ssl::context::pem);
	ssl_client.ssl_context().use_tmp_dh_file("client_certs/dh1024.pem");

	//please config the ssl context before creating any clients.
	ssl_client.add_client();
	ssl_client.add_client();
//*/
/*
	//method #2
	//to use st_ssl_tcp_sclient, we must construct ssl context first.
	boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
	ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	ctx.set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ctx.load_verify_file("certs/server.crt");
	ctx.use_certificate_chain_file("client_certs/server.crt");
	ctx.use_private_key_file("client_certs/server.key", boost::asio::ssl::context::pem);
	ctx.use_tmp_dh_file("client_certs/dh1024.pem");

	st_ssl_tcp_sclient ssl_sclient(service_pump, ctx);
#define ssl_client ssl_sclient
*/
	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::string str;
		std::cin >> str;
		if (QUIT_COMMAND == str)
		{
			service_pump.stop_service(&ssl_client);
			sleep(1);
			service_pump.stop_service();
		}
		else if (RESTART_COMMAND == str || RECONNECT_COMMAND == str)
			puts("I still not find a way to reuse a boost::asio::ssl::stream,\n"
				"it can reconnect to the server, but can not re-handshake with the server,\n"
				"if somebody knows how to fix this defect, please tell me, thanks in advance.");
		/*
		else if (RESTART_COMMAND == str)
		{
			service_pump.stop_service(&ssl_client);
			sleep(1);
			service_pump.stop_service();

			service_pump.start_service();
		}
		else if (RECONNECT_COMMAND == str)
			ssl_client.graceful_close(true);
		*/
		else
			server_.broadcast_msg(str);
	}

	return 0;
}

//restore configuration
#undef ST_ASIO_SERVER_PORT
#undef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
#undef ST_ASIO_ENHANCED_STABILITY
#undef ST_ASIO_DEFAULT_PACKER
#undef ST_ASIO_DEFAULT_UNPACKER
//restore configuration

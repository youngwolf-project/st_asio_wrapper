
//configuration
#define SERVER_PORT		9527
#define REUSE_OBJECT //use objects pool
//#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ENHANCED_STABILITY
#define DEFAULT_SSL_METHOD	boost::asio::ssl::context::sslv23
//configuration

#include "../include/st_asio_wrapper_ssl.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT_COMMAND "reconnect"

int main() {
	puts("type quit to end.");

	std::string str;
	st_service_pump service_pump;

	st_ssl_server server_(service_pump);
	server_.ssl_context().set_options(boost::asio::ssl::context::default_workarounds |
		boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	server_.ssl_context().set_verify_mode(boost::asio::ssl::context::verify_peer |
		boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	server_.ssl_context().load_verify_file("client_certs/server.crt");
	server_.ssl_context().use_certificate_chain_file("certs/server.crt");
	server_.ssl_context().use_private_key_file("certs/server.key", boost::asio::ssl::context::pem);
	server_.ssl_context().use_tmp_dh_file("certs/dh512.pem");

	const std::string cert_folder = "client_certs";
/*
	st_ssl_tcp_client ssl_client(service_pump, boost::asio::ssl::context::sslv23);
	ssl_client.ssl_context().set_options(boost::asio::ssl::context::default_workarounds |
		boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	ssl_client.ssl_context().set_verify_mode(boost::asio::ssl::context::verify_peer |
		boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ssl_client.ssl_context().load_verify_file("certs/server.crt");
	ssl_client.ssl_context().use_certificate_chain_file(cert_folder + "/server.crt");
	ssl_client.ssl_context().use_private_key_file(cert_folder + "/server.key", boost::asio::ssl::context::pem);
	ssl_client.ssl_context().use_tmp_dh_file(cert_folder + "/dh512.pem");

	//please configurate the ssl context before creating any clients.
	ssl_client.add_client(SERVER_PORT, SERVER_IP);
*/
///*
	//to use st_ssl_tcp_sclient, we must construct ssl context first.
	boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
	ctx.set_options(boost::asio::ssl::context::default_workarounds |
		boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
	ctx.set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ctx.load_verify_file("certs/server.crt");
	ctx.use_certificate_chain_file(cert_folder + "/server.crt");
	ctx.use_private_key_file(cert_folder + "/server.key", boost::asio::ssl::context::pem);
	ctx.use_tmp_dh_file(cert_folder + "/dh512.pem");

	st_ssl_tcp_sclient ssl_sclient(service_pump, ctx);
	ssl_sclient.set_server_addr(SERVER_PORT, SERVER_IP);
//*/
	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::cin >> str;
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str == RECONNECT_COMMAND)
			ssl_sclient.graceful_close(true);
			//ssl_client.at(0)->graceful_close(true);
		else
			server_.broadcast_msg(str);
	}

	return 0;
}

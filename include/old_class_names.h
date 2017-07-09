/*
 * old_class_names.h
 *
 *  Created on: 2016-7-30
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * be compatible with old editions.
 */

#ifndef ST_ASIO_OLD_CLASS_NAMES_H_
#define ST_ASIO_OLD_CLASS_NAMES_H_

//common
#define st_object object
#define st_timer timer
#define st_object_pool object_pool
#define st_service_pump service_pump

#define st_socket socket
#define st_tcp_socket_base socket_base //from st_asio_wrapper to st_asio_wrapper::tcp

#define st_sclient single_socket_service
#define st_client multi_socket_service
//common

//tcp client
#define st_client_socket_base client_socket_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_client_socket client_socket //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
#define st_connector_base connector_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_connector connector //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
#define st_tcp_sclient_base single_client_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_tcp_sclient single_client //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
#define st_tcp_client_base client_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_tcp_client client //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
//tcp client

//tcp server
#define st_server_socket_base server_socket_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_server_socket server_socket //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
#define st_server_base server_base //from st_asio_wrapper to st_asio_wrapper::tcp
#define st_server server //from st_asio_wrapper::ext to st_asio_wrapper::ext::tcp
//tcp server

//ssl
#define st_ssl_client_socket_base client_socket_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_client_socket client_socket //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl
#define st_ssl_connector_base connector_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_connector connector //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl
#define st_ssl_tcp_sclient_base single_client_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_tcp_sclient single_client //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl
#define st_ssl_tcp_client_base client_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_tcp_client client //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl

#define st_ssl_server_socket_base server_socket_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_server_socket server_socket //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl
#define st_ssl_server_base server_base //from st_asio_wrapper to st_asio_wrapper::ssl
#define st_ssl_server server //from st_asio_wrapper::ext to st_asio_wrapper::ext::ssl
//ssl

//udp
#define st_udp_socket_base socket_base //from st_asio_wrapper to st_asio_wrapper::udp
#define st_udp_socket socket //from st_asio_wrapper::ext to st_asio_wrapper::ext::udp
#define st_udp_sclient_base single_service_base //from st_asio_wrapper to st_asio_wrapper::udp
#define st_udp_sclient single_service //from st_asio_wrapper::ext to st_asio_wrapper::ext::udp
#define st_udp_client_base service_base //from st_asio_wrapper to st_asio_wrapper::udp
#define st_udp_client service //from st_asio_wrapper::ext to st_asio_wrapper::ext::udp
//udp

#endif /* ST_ASIO_OLD_CLASS_NAMES_H_ */


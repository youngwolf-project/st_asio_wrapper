/*
 * socks.h
 *
 *  Created on: 2020-8-25
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * SOCKS4/5 proxy support (CONNECT only).
 */

#ifndef ST_ASIO_PROXY_SOCKS_H_
#define ST_ASIO_PROXY_SOCKS_H_

#include "../client_socket.h"

namespace st_asio_wrapper { namespace tcp { namespace proxy {

namespace socks4 {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public st_asio_wrapper::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef st_asio_wrapper::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_), req_len(0) {}
	client_socket_base(Matrix& matrix_) : super(matrix_), req_len(0) {}

	virtual const char* type_name() const {return "SOCKS4 (client endpoint)";}
	virtual int type_id() const {return 5;}

	bool set_target_addr(unsigned short port, const std::string& ip)
	{
		req_len = 0;
		if (0 == port || ip.empty() || !super::set_addr(target_addr, port, ip) || !target_addr.address().is_v4())
			return false;

		buff[0] = 4;
		buff[1] = 1;
		*((unsigned short*) boost::next(buff, 2)) = htons(target_addr.port());
		memcpy(boost::next(buff, 4), target_addr.address().to_v4().to_bytes().data(), 4);
		memcpy(boost::next(buff, 8), "st_asio", sizeof("st_asio"));
		req_len = 8 + sizeof("st_asio");

		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_target_addr() const {return target_addr;}

private:
	virtual void connect_handler(const boost::system::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (ec || 0 == req_len)
			return super::connect_handler(ec);

		unified_out::info_out("connected to the proxy server, begin to negotiate with it.");
		ST_THIS status = super::HANDSHAKING;
		boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(buff, req_len),
			ST_THIS make_handler_error_size(boost::bind(&client_socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec || req_len != bytes_transferred)
		{
			unified_out::error_out(ST_ASIO_LLF " socks4 write error", ST_THIS id());
			ST_THIS force_shutdown(false);
		}
		else
			boost::asio::async_read(ST_THIS next_layer(), boost::asio::buffer(buff, 8),
				ST_THIS make_handler_error_size(boost::bind(&client_socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec || 8 != bytes_transferred)
		{
			unified_out::error_out(ST_ASIO_LLF " socks4 read error", ST_THIS id());
			ST_THIS force_shutdown(false);
		}
		else if (90 != buff[1])
		{
			unified_out::info_out(ST_ASIO_LLF " socks4 server error: %d", ST_THIS id(), (int) (unsigned char) buff[1]);
			ST_THIS force_shutdown(false);
		}
		else
			super::connect_handler(ec);
	}

private:
	char buff[16];
	size_t req_len;

	boost::asio::ip::tcp::endpoint target_addr;
};

}

namespace socks5 {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::tcp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class client_socket_base : public st_asio_wrapper::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef st_asio_wrapper::tcp::client_socket_base<Packer, Unpacker, Matrix, Socket, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	client_socket_base(boost::asio::io_context& io_context_) : super(io_context_), req_len(0), res_len(0), step(-1), target_port(0) {}
	client_socket_base(Matrix& matrix_) : super(matrix_), req_len(0), res_len(0), step(-1), target_port(0) {}

	virtual const char* type_name() const {return "SOCKS5 (client endpoint)";}
	virtual int type_id() const {return 6;}

	bool set_target_addr(unsigned short port, const std::string& ip)
	{
		step = -1;
		if (0 == port || ip.empty())
			return false;
		else if (!super::set_addr(target_addr, port, ip))
		{
			if (ip.size() > sizeof(buff) - 7)
			{
				unified_out::error_out("too long (must <= " ST_ASIO_SF ") target domain %s", sizeof(buff) - 7, ip.data());
				return false;
			}

			target_domain = ip;
			target_port = port;
		}
		else if (!target_addr.address().is_v4() && !target_addr.address().is_v6())
			return false;

		step = 0;
		return true;
	}
	const boost::asio::ip::tcp::endpoint& get_target_addr() const {return target_addr;}

	bool set_auth(const std::string& usr, const std::string& pwd)
	{
		if (usr.empty() || pwd.empty() || usr.size() + pwd.size() > 60)
		{
			unified_out::error_out("usr and pwd must not be empty and the sum of them must <= 60 characters.");
			return false;
		}

		username = usr;
		password = pwd;
		return true;
	}

private:
	virtual void connect_handler(const boost::system::error_code& ec) //intercept tcp::client_socket_base::connect_handler
	{
		if (ec || -1 == step)
			return super::connect_handler(ec);

		unified_out::info_out("connected to the proxy server, begin to negotiate with it.");
		ST_THIS status = super::HANDSHAKING;
		step = 0;
		send_method();
	}

	void send_method()
	{
		res_len = 0;

		buff[0] = 5;
		if (username.empty() && password.empty())
		{
			buff[1] = 1;
			req_len = 3;
		}
		else
		{
			buff[1] = 2;
			buff[3] = 2;
			req_len = 4;
		}
		buff[2] = 0;

		boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(buff, req_len),
			ST_THIS make_handler_error_size(boost::bind(&client_socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	void send_auth()
	{
		res_len = 0;

		buff[0] = 1;
		buff[1] = (char) username.size();
		memcpy(boost::next(buff, 2), username.data(), username.size());
		buff[2 + username.size()] = (char) password.size();
		memcpy(boost::next(buff, 3 + username.size()), password.data(), password.size());
		req_len = 1 + 1 + username.size() + 1 + password.size();

		boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(buff, req_len),
			ST_THIS make_handler_error_size(boost::bind(&client_socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	void send_request()
	{
		res_len = 0;

		buff[0] = 5;
		buff[1] = 1;
		buff[2] = 0;
		if (!target_domain.empty())
		{
			buff[3] = 3;
			buff[4] = (char) std::min<size_t>(target_domain.size(), sizeof(buff) - 7);
			memcpy(boost::next(buff, 5), target_domain.data(), (size_t) buff[4]);
			*((unsigned short*) boost::next(buff, 5 + buff[4])) = htons(target_port);
			req_len = 7 + buff[4];
		}
		else if (target_addr.address().is_v4())
		{
			buff[3] = 1;
			memcpy(boost::next(buff, 4), target_addr.address().to_v4().to_bytes().data(), 4);
			*((unsigned short*) boost::next(buff, 8)) = htons(target_addr.port());
			req_len = 10;
		}
		else //ipv6
		{
			buff[3] = 4;
			memcpy(boost::next(buff, 4), target_addr.address().to_v6().to_bytes().data(), 16);
			*((unsigned short*) boost::next(buff, 20)) = htons(target_addr.port());
			req_len = 22;
		}

		boost::asio::async_write(ST_THIS next_layer(), boost::asio::buffer(buff, req_len),
			ST_THIS make_handler_error_size(boost::bind(&client_socket_base::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec || req_len != bytes_transferred)
		{
			unified_out::error_out(ST_ASIO_LLF " socks5 write error", ST_THIS id());
			ST_THIS force_shutdown(false);
		}
		else
		{
			++step;
			ST_THIS next_layer().async_read_some(boost::asio::buffer(buff, sizeof(buff)),
				ST_THIS make_handler_error_size(boost::bind(&client_socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
		}
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		res_len += bytes_transferred;
		if (ec)
		{
			unified_out::error_out(ST_ASIO_LLF " socks5 read error", ST_THIS id());
			ST_THIS force_shutdown(false);
		}
		else
		{
			bool succ = true;
			bool continue_read = false;
			if (1 == step) //parse method
			{
				if (res_len < 2)
					continue_read = true;
				else if (res_len > 2)
				{
					unified_out::info_out(ST_ASIO_LLF " socks5 server error", ST_THIS id());
					succ = false;
				}
				else if (0 == buff[1])
				{
					++step; //skip auth step
					return send_request();
				}
				else if (2 == buff[1])
					return send_auth();
				else
				{
					unified_out::error_out(ST_ASIO_LLF " unsupported socks5 auth %d", ST_THIS id(), (int) (unsigned char) buff[1]);
					succ = false;
				}
			}
			else if (2 == step) //parse auth
			{
				if (res_len < 2)
					continue_read = true;
				else if (res_len > 2)
				{
					unified_out::info_out(ST_ASIO_LLF " socks5 server error", ST_THIS id());
					succ = false;
				}
				else if (0 == buff[1])
					return send_request();
				else
				{
					unified_out::error_out(ST_ASIO_LLF " socks5 auth error", ST_THIS id());
					succ = false;
				}
			}
			else if (3 == step) //parse request
			{
				size_t len = 6;
				if (res_len < len)
					continue_read = true;
				else if (0 == buff[1])
				{
					if (1 == buff[3])
						len += 4;
					else if (3 == buff[3])
						len += 1 + buff[4];
					else if (4 == buff[3])
						len += 16;

					if (res_len < len)
						continue_read = true;
					else if (res_len > len)
					{
						unified_out::info_out(ST_ASIO_LLF " socks5 server error", ST_THIS id());
						succ = false;
					}
				}
				else
				{
					unified_out::info_out(ST_ASIO_LLF " socks5 server error", ST_THIS id());
					succ = false;
				}
			}
			else
			{
				unified_out::info_out(ST_ASIO_LLF " socks5 client error", ST_THIS id());
				succ = false;
			}

			if (!succ)
				ST_THIS force_shutdown(false);
			else if (!continue_read)
				super::connect_handler(ec);
			else if (res_len >= sizeof(buff))
			{
				unified_out::info_out(ST_ASIO_LLF " socks5 server error", ST_THIS id());
				ST_THIS force_shutdown(false);
			}
			else
				ST_THIS next_layer().async_read_some(boost::asio::buffer(boost::next(buff, res_len), sizeof(buff) - res_len),
					ST_THIS make_handler_error_size(boost::bind(&client_socket_base::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
		}
	}

private:
	char buff[64];
	size_t req_len, res_len;
	int step;

	boost::asio::ip::tcp::endpoint target_addr;
	std::string target_domain;
	unsigned short target_port;
	std::string username, password;
};

}

}}} //namespace

#endif /* ST_ASIO_PROXY_SOCKS_H_ */

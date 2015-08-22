
#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "../file_server/packer_unpacker.h"
#include "../include/st_asio_wrapper_tcp_client.h"
using namespace st_asio_wrapper;

extern boost::atomic_ushort completed_client_num;
extern int link_num;
extern __off64_t file_size;

class file_socket : public base_socket, public st_connector
{
public:
	file_socket(boost::asio::io_service& io_service_) : st_connector(io_service_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {clear(); st_connector::reset();}

	void set_index(int index_) {index = index_;}
	__off64_t get_rest_size() const
	{
		BOOST_AUTO(unpacker, boost::dynamic_pointer_cast<const data_unpacker>(inner_unpacker()));
		return NULL == unpacker ? 0 : unpacker->get_rest_size();
	}
	operator __off64_t() const {return get_rest_size();}

	bool get_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		if (TRANS_IDLE != state)
			return false;
		else if (NULL == file)
		{
			if (0 == id())
				file = fopen(file_name.data(), "w+b");
			else
				file = fopen(file_name.data(), "r+b");

			if (NULL == file)
			{
				printf("can't create file %s.\n", file_name.data());
				return false;
			}
			else if (0 == id())
				return true;
		}

		std::string order("\0", ORDER_LEN);
		order += file_name;

		state = TRANS_PREPARE;
		send_msg(order, true);

		return true;
	}

	void talk(const std::string& str)
	{
		if (TRANS_IDLE == state && !str.empty())
		{
			std::string order("\2", ORDER_LEN);
			order += str;
			send_msg(order, true);
		}
	}

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//we can handle msg very fast, so we don't use recv buffer
	virtual bool on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//we will change unpacker at runtime, this operation must be done in on_msg, do not to it in on_msg_handle
	//virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {handle_msg(msg); return true;}
	//msg handling end

private:
	void clear()
	{
		state = TRANS_IDLE;
		if (NULL != file)
		{
			fclose(file);
			file = NULL;
		}

		inner_unpacker(boost::make_shared<DEFAULT_UNPACKER>());
	}
	void trans_end() {clear(); ++completed_client_num;}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());
			trans_end();
			return;
		}
		else if (msg.size() <= ORDER_LEN)
		{
			printf("wrong order length: " size_t_format ".\n", msg.size());
			return;
		}

		switch (*msg.data())
		{
		case 0:
			if (ORDER_LEN + DATA_LEN == msg.size() && NULL != file && TRANS_PREPARE == state)
			{
				__off64_t length = *(__off64_t*) boost::next(msg.data(), ORDER_LEN);
				if (-1 == length)
				{
					if (0 == index)
						puts("get file failed!");
					trans_end();
				}
				else
				{
					if (0 == index)
						file_size = length;

					__off64_t my_length = length / link_num;
					__off64_t offset = my_length * index;

					if (link_num - 1 == index)
						my_length = length - offset;
					if (my_length > 0)
					{
						char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
						*buffer = 1; //head
						*(__off64_t*) boost::next(buffer, ORDER_LEN) = offset;
						*(__off64_t*) boost::next(buffer, ORDER_LEN + OFFSET_LEN) = my_length;

						state = TRANS_BUSY;
						send_msg(buffer, sizeof(buffer), true);

						fseeko64(file, offset, SEEK_SET);
						inner_unpacker(boost::make_shared<data_unpacker>(file, my_length));
					}
					else
						trans_end();
				}
			}
			break;
		case 2:
			if (0 == index)
				printf("server says: %s\n", boost::next(msg.data(), ORDER_LEN));
			break;
		default:
			break;
		}
	}

private:
	int index;
};

class file_client : public st_tcp_client_base<file_socket>
{
public:
	file_client(st_service_pump& service_pump_) : st_tcp_client_base<file_socket>(service_pump_) {}

	__off64_t get_total_rest_size()
	{
		__off64_t total_rest_size = 0;
		do_something_to_all(boost::ref(total_rest_size) += *boost::lambda::_1);
//		do_something_to_all(boost::ref(total_rest_size) += boost::lambda::bind(&file_socket::get_rest_size, &*boost::lambda::_1));

		return total_rest_size;
	}
};

#endif //#ifndef FILE_CLIENT_H_

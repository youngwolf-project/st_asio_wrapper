
#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <boost/atomic.hpp>

#include "../include/st_asio_wrapper_client.h"
using namespace st_asio_wrapper;

extern atomic_ushort completed_client_num;
extern int link_num;
extern __off64_t file_size;

/*
protocol:
head(1 byte) + body

if head equal:
0: body is a filename
	request the file length, client->server->client
	return: same head + file length(8 bytes)
1: body is file offset(8 bytes) + data length(8 bytes)
	request the file content, client->server->client
	return: same head + file content, repeat until all data requested by client been sent
2: body is talk content
	talk, client->server or server->client
	return: n/a
*/

#ifdef _MSC_VER
#define __off64_t __int64
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
#endif

#define ORDER_LEN	sizeof(char)
#define OFFSET_LEN	sizeof(__off64_t)
#define DATA_LEN	sizeof(__off64_t)

#if defined(_WIN64) || 64 == __WORDSIZE
#define __off64_t_format "%ld"
#else
#define __off64_t_format "%lld"
#endif

class file_client : public st_client
{
public:
	class file_socket : public st_connector
	{
	public:
		file_socket(file_client& file_client_) : st_connector(file_client_.get_service_pump()),
			state(TRANS_IDLE), index(-1), file(NULL), my_length(0) {}
		virtual ~file_socket() {clear();}

		//reset all, be ensure that there's no any operations performed on this st_socket when invoke it
		virtual void reset() {clear(); st_connector::reset();}

		void set_index(int index_) {index = index_;}
		__off64_t get_rest_size() {return my_length;}
		bool get_file(const std::string& file_name)
		{
			if (TRANS_IDLE == state && !file_name.empty())
			{
				if (0 == index)
					file = fopen(file_name.data(), "w+b");
				else
					file = fopen(file_name.data(), "r+b");
				if (NULL != file)
				{
					std::string order("\0", ORDER_LEN);
					order += file_name;

					state = TRANS_PREPARE;
					send_msg(order);

					return true;
				}
				else if (0 == index)
					printf("can't create file %s.\n", file_name.data());
			}

			return false;
		}

		void talk(const std::string& str)
		{
			if (!str.empty())
			{
				std::string order("\2", ORDER_LEN);
				order += str;
				send_msg(order);
			}
		}

	protected:
		//msg handling
	#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
		//we can handle the msg very fast, so we don't use the recv buffer
		virtual bool on_msg(msg_type& msg) {handle_msg(msg); return false;}
	#endif
		virtual void on_msg_handle(msg_type& msg) {handle_msg(msg);}
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
			my_length = 0;
		}
		void trans_end() {clear(); ++completed_client_num;}

		void handle_msg(const std::string& str)
		{
			if (str.size() <= ORDER_LEN)
			{
				printf("wrong order length: " size_t_format ".\n", str.size());
				return;
			}

			switch (*str.data())
			{
			case 0:
				if (ORDER_LEN + DATA_LEN == str.size() && NULL != file && TRANS_PREPARE == state)
				{
					__off64_t length = *(__off64_t*) (str.data() + ORDER_LEN);
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

						my_length = length / link_num;
						__off64_t offset = my_length * index;

						if (link_num - 1 == index)
							my_length = length - offset;
						if (my_length > 0)
						{
							fseeko64(file, offset, SEEK_SET);

							char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
							*buffer = 1; //head
							*(__off64_t*) (buffer + ORDER_LEN) = offset;
							*(__off64_t*) (buffer + ORDER_LEN + OFFSET_LEN) = my_length;

							state = TRANS_BUSY;
							send_msg(buffer, sizeof(buffer));
						}
						else
							trans_end();
					}
				}
				break;
			case 1:
				if (NULL != file && TRANS_BUSY == state)
				{
					size_t data_len = str.size() - ORDER_LEN;
					if (data_len != fwrite(str.data() + ORDER_LEN, 1, data_len, file))
					{
						printf("fwrite(" size_t_format ") error!\n", data_len);
						trans_end();
					}
					else
					{
						my_length -= data_len;
						if (my_length <= 0)
						{
							if (my_length < 0)
								printf("error: my_length(" __off64_t_format ") < 0!\n", my_length);
							trans_end();
						}
					}
				}
				break;
			case 2:
				if (0 == index)
					printf("server: %s\n", str.data() + ORDER_LEN);
				break;
			default:
				break;
			}
		}

	private:
		enum TRANS_STATE {TRANS_IDLE, TRANS_PREPARE, TRANS_BUSY};
		TRANS_STATE state;
		int index;

		FILE* file;
		__off64_t my_length;
	};

public:
	file_client(st_service_pump& service_pump) : st_client(service_pump) {}

	boost::shared_ptr<st_connector> get_client(size_t index)
	{
		assert(index < client_can.size());
		return *boost::next(client_can.begin(), index);
	}

	boost::shared_ptr<const st_connector> get_client(size_t index) const
	{
		assert(index < client_can.size());
		return *boost::next(client_can.begin(), index);
	}

	__off64_t get_total_rest_size() const
	{
		__off64_t total_rest_size = 0;
		for (BOOST_AUTO(iter, client_can.begin()); iter != client_can.end(); ++iter)
			total_rest_size += dynamic_cast<file_client::file_socket*>(iter->get())->get_rest_size();

		return total_rest_size;
	}
};

#endif //#ifndef FILE_CLIENT_H_

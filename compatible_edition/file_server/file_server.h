
#ifndef FILE_SERVER_H_
#define FILE_SERVER_H_

#include "../include/st_asio_wrapper_server.h"
using namespace st_asio_wrapper;

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

class file_socket : public st_server_socket
{
public:
	file_socket(i_server& server_) : st_server_socket(server_), state(TRANS_IDLE), file(NULL) {}
	virtual ~file_socket() {trans_end();}

public:
	//because we don't use objects pool(we don't define REUSE_OBJECT), so, this virtual function will
	//not be invoked, and can be omitted, but we keep it for possibly future using
	virtual void reset() {trans_end(); st_server_socket::reset();}

protected:
	//msg handling
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//we can handle the msg very fast, so we don't use the recv buffer
	virtual bool on_msg(std::string& msg) {handle_msg(msg); return true;}
#endif
	virtual bool on_msg_handle(std::string& msg, bool link_down) {handle_msg(msg); return true;}
	//msg handling end

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch (id)
		{
		case 10:
			if (!is_send_buffer_available()) //continue this timer
				return true;

			get_io_service().post(boost::bind(&file_socket::read_file_handler, this,
				*static_cast<const __off64_t*>(user_data)));
			delete static_cast<const __off64_t*>(user_data); //free this memory, because we newed it in set_timer.
			break;
		default:
			return st_server_socket::on_timer(id, user_data);
			break;
		}

		return false;
	}

private:
	void trans_end()
	{
		state = TRANS_IDLE;
		if (NULL != file)
		{
			fclose(file);
			file = NULL;
		}
	}

	void read_file_handler(__off64_t length)
	{
		if (NULL != file && length > 0)
		{
			//network IO is slower than disk IO, wait for a moment
			//we should avoid invoke blocking functions such as safe_send_msg in service threads,
			//unless you are sure that the effects on the throughput caused by blocking functions is acceptable,
			//because blocking here means one service thread suspended
			//more details about service thread, please refer to class st_service_pump
			if (!is_send_buffer_available())
				set_timer(10, 50, new __off64_t(length));
			else
			{
				size_t read_size = (size_t) std::min(
					(__off64_t) packer::get_max_msg_size(), (__off64_t) ORDER_LEN + length);
				read_size -= ORDER_LEN;

				char* buffer = new char[ORDER_LEN + read_size];
				*buffer = 1; //head
				if (read_size != fread(buffer + ORDER_LEN, 1, read_size, file))
				{
					printf("fread(" size_t_format ") error!\n", read_size);
					trans_end();
				}
				else
				{
					send_msg(buffer, read_size + ORDER_LEN, true);
					length -= read_size;
					if (length > 0)
						get_io_service().post(boost::bind(&file_socket::read_file_handler, this, length));
					else
					{
						assert(0 == length);
						trans_end();
					}
				}
				delete[] buffer;
			}
		}
	}

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
			if (TRANS_IDLE == state)
			{
				trans_end();

				char buffer[ORDER_LEN + DATA_LEN];
				*buffer = 0; //head

				file = fopen(str.data() + ORDER_LEN, "rb");
				if (NULL != file)
				{
					fseeko64(file, 0, SEEK_END);
					__off64_t length = ftello64(file);
					memcpy(buffer + ORDER_LEN, &length, DATA_LEN);
					state = TRANS_PREPARE;
				}
				else
				{
					*(__off64_t*) (buffer + ORDER_LEN) = -1;
					printf("can't not open file %s!\n", str.data() + ORDER_LEN);
				}

				send_msg(buffer, sizeof(buffer), true);
			}
			break;
		case 1:
			if (TRANS_PREPARE == state && NULL != file &&
				ORDER_LEN + OFFSET_LEN + DATA_LEN == str.size())
			{
				__off64_t offset = *(__off64_t*) (str.data() + ORDER_LEN);
				__off64_t length = *(__off64_t*) (str.data() + ORDER_LEN + OFFSET_LEN);
				if (offset >= 0 && length > 0 && offset + length <= ftello64(file))
				{
					fseeko64(file, offset, SEEK_SET);
					state = TRANS_BUSY;
					get_io_service().post(boost::bind(&file_socket::read_file_handler, this, length));
				}
			}
			break;
		case 2:
				printf("client: %s\n", str.data() + ORDER_LEN);
			break;
		default:
			break;
		}
	}

private:
	enum TRANS_STATE {TRANS_IDLE, TRANS_PREPARE, TRANS_BUSY};
	TRANS_STATE state;
	FILE* file;
};

class file_server : public st_server_base<file_socket>
{
public:
	file_server(st_service_pump& service_pump_) : st_server_base<file_socket>(service_pump_) {}

	void talk(const std::string& str)
	{
		if (!str.empty())
		{
			std::string order("\2", ORDER_LEN);
			order += str;
			broadcast_msg(order, true);
		}
	}
};

#undef HOW_USE_MSG_RECV_BUFFER

#endif //#ifndef FILE_SERVER_H_

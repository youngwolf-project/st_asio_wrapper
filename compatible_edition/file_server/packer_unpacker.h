#ifndef PACKER_UNPACKER_H_
#define PACKER_UNPACKER_H_

#include "../include/st_asio_wrapper_packer.h"
#include "../include/st_asio_wrapper_unpacker.h"
using namespace st_asio_wrapper;

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

/*
protocol:
head(1 byte) + body

if head equal to:
0: body is a filename
	request the file length, client->server->client
	return: same head + file length(8 bytes)
1: body is file offset(8 bytes) + data length(8 bytes)
	request the file content, client->server->client
	return: file content(no-protocol), repeat until all data requested by client been sent(client only need to request one time)
2: body is talk content
	talk, client->server. please note that server cannot talk to client, this is because server never knows whether it is transmitting a file or not.
	return: n/a
*/

class file_buffer : public i_buffer
{
public:
	file_buffer(FILE* file, __off64_t data_len) : _file(file), _data_len(data_len)
	{
		assert(NULL != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(NULL != buffer);

		read();
	}
	~file_buffer() {delete[] buffer;}

public:
	virtual bool empty() const {return 0 == buffer_len;}
	virtual size_t size() const {return buffer_len;}
	virtual const char* data() const {return buffer;}

	void read()
	{
		if (0 == _data_len)
			buffer_len = 0;
		else
		{
			buffer_len = _data_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) _data_len;
			_data_len -= buffer_len;
			if (buffer_len != fread(buffer, 1, buffer_len, _file))
			{
				printf("fread(" size_t_format ") error!\n", buffer_len);
				buffer_len = 0;
			}
		}
	}

protected:
	FILE* _file;
	char* buffer;
	size_t buffer_len;

	__off64_t _data_len;
};

class data_unpacker : public i_unpacker<replaceable_buffer>
{
public:
	data_unpacker(FILE* file, __off64_t data_len)  : _file(file), _data_len(data_len)
	{
		assert(NULL != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(NULL != buffer);
	}
	~data_unpacker() {delete[] buffer;}

	__off64_t get_rest_size() const {return _data_len;}

	virtual void reset_state() {_file = NULL; delete[] buffer; buffer = NULL; _data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(_data_len >= (__off64_t) bytes_transferred && bytes_transferred > 0);
		_data_len -= bytes_transferred;

		if (bytes_transferred != fwrite(buffer, 1, bytes_transferred, _file))
		{
			printf("fwrite(" size_t_format ") error!\n", bytes_transferred);
			return false;
		}

		if (0 == _data_len)
			msg_can.resize(msg_can.size() + 1);

		return true;
	}

	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) {return ec ? 0 : boost::asio::detail::default_max_transfer_size;}
	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		size_t buffer_len = _data_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) _data_len;
		return boost::asio::buffer(buffer, buffer_len);
	}

protected:
	FILE* _file;
	char* buffer;

	__off64_t _data_len;
};

class base_socket
{
public:
	base_socket() : state(TRANS_IDLE), file(NULL)  {}

protected:
	enum TRANS_STATE {TRANS_IDLE, TRANS_PREPARE, TRANS_BUSY};
	TRANS_STATE state;
	FILE* file;
};

#endif // PACKER_UNPACKER_H_

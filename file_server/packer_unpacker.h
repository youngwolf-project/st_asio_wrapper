#ifndef PACKER_UNPACKER_H_
#define PACKER_UNPACKER_H_

#include "../include/st_asio_wrapper_base.h"
using namespace st_asio_wrapper;

#ifdef _MSC_VER
#define fseeko _fseeki64
#define ftello _ftelli64
#define fl_type __int64
#else
#define fl_type off_t
#endif

#define ORDER_LEN	sizeof(char)
#define OFFSET_LEN	sizeof(fl_type)
#define DATA_LEN	OFFSET_LEN

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
	file_buffer(FILE* file, fl_type data_len) : _file(file), _data_len(data_len)
	{
		assert(nullptr != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);

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
				printf("fread(" ST_ASIO_SF ") error!\n", buffer_len);
				buffer_len = 0;
			}
		}
	}

protected:
	FILE* _file;
	char* buffer;
	size_t buffer_len;

	fl_type _data_len;
};

class data_unpacker : public i_unpacker<replaceable_buffer>
{
public:
	data_unpacker(FILE* file, fl_type data_len)  : _file(file), _data_len(data_len)
	{
		assert(nullptr != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(nullptr != buffer);
	}
	~data_unpacker() {delete[] buffer;}

	fl_type get_rest_size() const {return _data_len;}

	virtual void reset_state() {_file = nullptr; delete[] buffer; buffer = nullptr; _data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(_data_len >= (fl_type) bytes_transferred && bytes_transferred > 0);
		_data_len -= bytes_transferred;

		if (bytes_transferred != fwrite(buffer, 1, bytes_transferred, _file))
		{
			printf("fwrite(" ST_ASIO_SF ") error!\n", bytes_transferred);
			return false;
		}

		if (0 == _data_len)
			msg_can.emplace_back();

		return true;
	}

	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) {return ec ? 0 : boost::asio::detail::default_max_transfer_size;}
	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		auto buffer_len = _data_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) _data_len;
		return boost::asio::buffer(buffer, buffer_len);
	}

protected:
	FILE* _file;
	char* buffer;

	fl_type _data_len;
};

class base_socket
{
public:
	base_socket() : state(TRANS_IDLE), file(nullptr)  {}

protected:
	enum TRANS_STATE {TRANS_IDLE, TRANS_PREPARE, TRANS_BUSY};
	TRANS_STATE state;
	FILE* file;
};

#endif // PACKER_UNPACKER_H_

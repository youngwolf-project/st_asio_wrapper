#ifndef FILE_BUFFER_H_
#define FILE_BUFFER_H_

#include "../include/st_asio_wrapper_base.h"
using namespace st_asio_wrapper;

#include "common.h"

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

#endif //FILE_BUFFER_H_

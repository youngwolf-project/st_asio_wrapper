#ifndef FILE_BUFFER_H_
#define FILE_BUFFER_H_

#include "common.h"

class file_buffer : public i_buffer, public boost::noncopyable
{
public:
	file_buffer(FILE* file, fl_type total_len_, atomic_size* transmit_size_ = NULL) : _file(file), total_len(total_len_), transmit_size(transmit_size_)
	{
		assert(NULL != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(NULL != buffer);

		read();
	}
	~file_buffer() {delete[] buffer;}

public:
	virtual bool empty() const {return 0 == data_len;}
	virtual size_t size() const {return data_len;}
	virtual const char* data() const {return buffer;}

	void read()
	{
		if (0 == total_len)
			data_len = 0;
		else
		{
			data_len = total_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) total_len;
			total_len -= data_len;
			if (data_len != fread(buffer, 1, data_len, _file))
			{
				printf("fread(" ST_ASIO_SF ") error!\n", data_len);
				data_len = 0;
			}
			else if (NULL != transmit_size)
				*transmit_size += data_len;
		}
	}

protected:
	FILE* _file;
	char* buffer;
	size_t data_len;

	fl_type total_len;
	atomic_size* transmit_size;
};

#endif //FILE_BUFFER_H_

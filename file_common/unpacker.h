#ifndef UNPACKER_H_
#define UNPACKER_H_

#include "common.h"

class file_unpacker : public i_unpacker<std::string>, public boost::noncopyable
{
public:
	file_unpacker(FILE* file, fl_type total_len_, atomic_size* transmit_size_ = NULL)  : _file(file), total_len(total_len_), transmit_size(transmit_size_)
	{
		assert(NULL != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(NULL != buffer);
	}
	~file_unpacker() {delete[] buffer;}

	bool is_finished() const {return 0 == total_len;}

	virtual void reset() {_file = NULL; delete[] buffer; buffer = NULL; total_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(total_len >= (fl_type) bytes_transferred && bytes_transferred > 0);

		total_len -= bytes_transferred;
		if (NULL != transmit_size)
			*transmit_size += bytes_transferred;

		if (bytes_transferred == fwrite(buffer, 1, bytes_transferred, _file))
			return true;

		printf("fwrite(" ST_ASIO_SF ") error!\n", bytes_transferred);
		return false;
	}

	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) {return ec ? 0 : boost::asio::detail::default_max_transfer_size;}
	virtual buffer_type prepare_next_recv()
	{
		size_t data_len = total_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) total_len;
#ifdef ST_ASIO_SCATTERED_RECV_BUFFER
		return buffer_type(1, boost::asio::buffer(buffer, data_len));
#else
		return boost::asio::buffer(buffer, data_len);
#endif
	}

protected:
	FILE* _file;
	char* buffer;

	fl_type total_len;
	atomic_size* transmit_size;
};

#endif //UNPACKER_H_

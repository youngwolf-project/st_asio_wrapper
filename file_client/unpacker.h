#ifndef UNPACKER_H_
#define UNPACKER_H_

#include "../include/base.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;

#include "../file_server/common.h"

#if BOOST_VERSION >= 105300
extern boost::atomic_int_fast64_t received_size;
#else
extern atomic<boost::int_fast64_t> received_size;
#endif

class data_unpacker : public i_unpacker<replaceable_buffer>
{
public:
	data_unpacker(FILE* file, fl_type data_len)  : _file(file), _data_len(data_len)
	{
		assert(NULL != _file);

		buffer = new char[boost::asio::detail::default_max_transfer_size];
		assert(NULL != buffer);
	}
	~data_unpacker() {delete[] buffer;}

	virtual void reset() {_file = NULL; delete[] buffer; buffer = NULL; _data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can)
	{
		assert(_data_len >= (fl_type) bytes_transferred && bytes_transferred > 0);
		_data_len -= bytes_transferred;
		received_size += bytes_transferred;

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
	virtual buffer_type prepare_next_recv()
	{
		size_t buffer_len = _data_len > boost::asio::detail::default_max_transfer_size ? boost::asio::detail::default_max_transfer_size : (size_t) _data_len;
#ifdef ST_ASIO_SCATTERED_RECV_BUFFER
		return buffer_type(1, boost::asio::buffer(buffer, buffer_len));
#else
		return boost::asio::buffer(buffer, buffer_len);
#endif
	}

protected:
	FILE* _file;
	char* buffer;

	fl_type _data_len;
};

#endif //UNPACKER_H_

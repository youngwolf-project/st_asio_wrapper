
//configuration
#define ST_ASIO_SERVER_PORT		5051
#define ST_ASIO_RESTORE_OBJECT
#define ST_ASIO_ENHANCED_STABILITY
#define ST_ASIO_WANT_MSG_SEND_NOTIFY
#define ST_ASIO_INPUT_QUEUE non_lock_queue
//file_server / file_client is a responsive system, before file_server send each message (except talking message,
//but file_server only receive talking message, not send talking message proactively), the previous message has been
//sent to file_client, so sending buffer will always be empty, which means we will never operate sending buffer concurrently,
//so need no locks.
#define ST_ASIO_DEFAULT_PACKER	replaceable_packer<>
#define ST_ASIO_RECV_BUFFER_TYPE std::vector<boost::asio::mutable_buffer> //scatter-gather buffer, it's very useful under certain situations (for example, ring buffer).
#define ST_ASIO_SCATTERED_RECV_BUFFER //used by unpackers, not belongs to st_asio_wrapper
//configuration

#include "file_socket.h"

file_socket::file_socket(i_server& server_) : server_socket(server_) {}
file_socket::~file_socket() {trans_end();}

void file_socket::reset() {trans_end(); server_socket::reset();}
void file_socket::take_over(boost::shared_ptr<server_socket> socket_ptr)
{
	BOOST_AUTO(raw_socket_ptr, boost::dynamic_pointer_cast<file_socket>(socket_ptr)); //socket_ptr actually is a pointer of file_socket
	printf("restore user data from invalid object (" ST_ASIO_LLF ").\n", raw_socket_ptr->id());
}
//this works too, but brings warnings with -Woverloaded-virtual option.
//void file_socket::take_over(boost::shared_ptr<file_socket> socket_ptr) {printf("restore user data from invalid object (" ST_ASIO_LLF ").\n", socket_ptr->id());}

//msg handling
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
//we can handle msg very fast, so we don't use recv buffer
bool file_socket::on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
bool file_socket::on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
//msg handling end

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
void file_socket::on_msg_send(in_msg_type& msg)
{
	BOOST_AUTO(buffer, dynamic_cast<file_buffer*>(msg.raw_buffer()));
	if (NULL != buffer)
	{
		buffer->read();
		if (buffer->empty())
			trans_end();
		else
			direct_send_msg(msg, true);
	}
}
#endif

void file_socket::trans_end()
{
	state = TRANS_IDLE;
	if (NULL != file)
	{
		fclose(file);
		file = NULL;
	}
}

void file_socket::handle_msg(out_msg_ctype& msg)
{
	if (msg.size() <= ORDER_LEN)
	{
		printf("wrong order length: " ST_ASIO_SF ".\n", msg.size());
		return;
	}

	switch (*msg.data())
	{
	case 0:
		if (TRANS_IDLE == state)
		{
			trans_end();

			char buffer[ORDER_LEN + DATA_LEN];
			*buffer = 0; //head

			file = fopen(boost::next(msg.data(), ORDER_LEN), "rb");
			if (NULL != file)
			{
				fseeko(file, 0, SEEK_END);
				fl_type length = ftello(file);
				memcpy(boost::next(buffer, ORDER_LEN), &length, DATA_LEN);
				state = TRANS_PREPARE;
			}
			else
			{
				memset(boost::next(buffer, ORDER_LEN), -1, DATA_LEN);
				printf("can't not open file %s!\n", boost::next(msg.data(), ORDER_LEN));
			}

			send_msg(buffer, sizeof(buffer), true);
		}
		break;
	case 1:
		if (TRANS_PREPARE == state && NULL != file && ORDER_LEN + OFFSET_LEN + DATA_LEN == msg.size())
		{
			fl_type offset;
			memcpy(&offset, boost::next(msg.data(), ORDER_LEN), OFFSET_LEN);
			fl_type length;
			memcpy(&length, boost::next(msg.data(), ORDER_LEN + OFFSET_LEN), DATA_LEN);
			if (offset >= 0 && length > 0 && offset + length <= ftello(file))
			{
				state = TRANS_BUSY;
				fseeko(file, offset, SEEK_SET);
				in_msg_type msg(new file_buffer(file, length));
				direct_send_msg(msg, true);
			}
		}
		break;
	case 2:
		printf("client says: %s\n", boost::next(msg.data(), ORDER_LEN));
		break;
	case 3:
		if (ORDER_LEN + sizeof(boost::uint_fast64_t) == msg.size())
		{
			boost::uint_fast64_t id;
			memcpy(&id, boost::next(msg.data(), ORDER_LEN), sizeof(boost::uint_fast64_t));
			get_server().restore_socket(ST_THIS shared_from_this(), id);
		}
	default:
		break;
	}
}

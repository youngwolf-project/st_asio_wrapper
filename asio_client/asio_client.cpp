
//configuration
#define SERVER_PORT		9527
#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define CUSTOM_LOG
#define DEFAULT_PACKER	my_packer
#define DEFAULT_UNPACKER my_unpacker

//the following three macro demonstrate how to support huge msg(exceed 65535 - 2).
//huge msg consume huge memory, for example, if we support 1M msg size, because every st_tcp_socket has a
//private unpacker which has a buffer at lest 1M size, so 1K st_tcp_socket will consume 1G memory.
//if we consider the send buffer and recv buffer, the buffer's default max size is 1K, so, every st_tcp_socket
//can consume 2G(2 * 1M * 1K) memory when performance testing(both send buffer and recv buffer are full).
//#define HUGE_MSG
//#define MAX_MSG_LEN (1024 * 1024)
//#define MAX_MSG_NUM 8 //reduce buffer size to reduce memory occupation
//configuration

//demonstrate how to use custom log system:
//notice: please don't forget to define the CUSTOM_LOG macro.
//use your own code to replace all_out_helper2 macro.
//custom log should be defined(or included) before including any st_asio_wrapper header files except
//st_asio_wrapper_base.h
#include "../include/st_asio_wrapper_base.h"
using namespace st_asio_wrapper;

class unified_out
{
public:
	static void fatal_out(const char* fmt, ...) {all_out_helper2;}
	static void error_out(const char* fmt, ...) {all_out_helper2;}
	static void warning_out(const char* fmt, ...) {all_out_helper2;}
	static void info_out(const char* fmt, ...) {all_out_helper2;}
	static void debug_out(const char* fmt, ...) {all_out_helper2;}
};

//demonstrate how to use custom msg buffer(default buffer is std::string)
#include "../include/st_asio_wrapper_packer.h"
#include "../include/st_asio_wrapper_unpacker.h"

//this buffer is more efficient than std::string if the memory is already allocated,
//because the replication been saved. for example, you are sending memory-mapped files.
class my_msg_buffer
{
public:
	//the following two functions are needed by st_asio_wrapper
	my_msg_buffer() {detach();}
	my_msg_buffer(const char* _buff, size_t _len) {detach(); assign(_buff, _len);}

	my_msg_buffer(const my_msg_buffer& other) {detach(); assign(other);}
	my_msg_buffer(my_msg_buffer&& other) {detach(); do_attach(other.buff, other.len); other.detach();}
	~my_msg_buffer() {free();}

	void operator=(my_msg_buffer& other) {attach(other.buff, other.len); other.detach();}
	void assign(const my_msg_buffer& other) {if (!other.empty()) assign(other.buff, other.len); else free();}
	void assign(const char* _buff, size_t _len)
	{
		assert(_len > 0 && nullptr != _buff);
		auto _buff_ = new char[_len];
		memcpy(_buff_, _buff, _len);

		attach(_buff_, _len);
	}
	void attach(const char* _buff, size_t _len)
	{
		free();
		do_attach(_buff, _len);
	}

	//the following four functions are needed by st_asio_wrapper
	bool empty() const {return 0 == len || nullptr == buff;}
	const char* data() const {return buff;}
	size_t size() const {return len;}
	void swap(my_msg_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len);}

protected:
	void do_attach(const char* _buff, size_t _len) {buff = _buff; len = _len;}
	void detach() {buff = nullptr; len = 0;}
	void free() {delete buff; detach();}

protected:
	const char* buff;
	size_t len;
};

class my_packer : public i_packer<my_msg_buffer>
{
public:
	static size_t get_max_msg_size() {return MAX_MSG_LEN - HEAD_LEN;}
	virtual my_msg_buffer pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		my_msg_buffer str;
		if (nullptr != pstr && nullptr != len)
		{
			size_t total_len = native ? 0 : HEAD_LEN;
			auto last_total_len = total_len;
			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
				{
					total_len += len[i];
					if (last_total_len > total_len || total_len > MAX_MSG_LEN) //overflow
					{
						unified_out::error_out("pack msg error: length exceeds the MAX_MSG_LEN!");
						return str;
					}
					last_total_len = total_len;
				}

				if (total_len > (native ? 0 : HEAD_LEN))
				{
					char* buff = nullptr;
					size_t pos = 0;
					if (!native)
					{
						auto head_len = (HEAD_TYPE) total_len;
						if (total_len != head_len)
						{
							unified_out::error_out("pack msg error: length exceeds the header's range!");
							return str;
						}

						head_len = HEAD_H2N(head_len);
						buff = new char[total_len];
						memcpy(buff, (const char*) &head_len, HEAD_LEN);
						pos = HEAD_LEN;
					}
					else
						buff = new char[total_len];

					for (size_t i = 0; i < num; ++i)
						if (nullptr != pstr[i])
						{
							memcpy(buff + pos, pstr[i], len[i]);
							pos += len[i];
						}

					str.attach(buff, total_len);
				}
		} //if (nullptr != pstr && nullptr != len)

		return str;
	}
};

class my_unpacker : public i_unpacker<my_msg_buffer>
{
public:
	my_unpacker() {reset_unpacker_state();}
	size_t used_buffer_size() const {return cur_data_len;} //how many data have been received
	size_t current_msg_length() const {return cur_msg_len;} //current msg's total length, -1 means don't know

public:
	virtual void reset_unpacker_state() {cur_msg_len = -1; cur_data_len = 0;}
	virtual bool parse_msg(size_t bytes_transferred, boost::container::list<my_msg_buffer>& msg_can)
	{
		//length + msg
		cur_data_len += bytes_transferred;

		auto pnext = raw_buff.begin();
		auto unpack_ok = true;
		while (unpack_ok) //considering stick package problem, we need a loop
			if ((size_t) -1 != cur_msg_len)
			{
				//cur_msg_len now can be assigned in the completion_condition function, or in the following 'else if',
				//so, we must verify cur_msg_len at the very beginning of using it, not at the assignment as we do
				//before, please pay special attention
				if (cur_msg_len > MAX_MSG_LEN || cur_msg_len <= HEAD_LEN)
					unpack_ok = false;
				else if (cur_data_len >= cur_msg_len) //one msg received
				{
					msg_can.resize(msg_can.size() + 1);
					msg_can.back().assign(pnext + HEAD_LEN, cur_msg_len - HEAD_LEN);
					cur_data_len -= cur_msg_len;
					std::advance(pnext, cur_msg_len);
					cur_msg_len = -1;
				}
				else
					break;
			}
			else if (cur_data_len >= HEAD_LEN) //the msg's head been received, stick package found
				cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) pnext);
			else
				break;

			if (pnext == raw_buff.begin()) //we should have at lest got one msg.
				unpack_ok = false;
			else if (unpack_ok && cur_data_len > 0)
				memcpy(raw_buff.begin(), pnext, cur_data_len); //left behind unparsed msg

			//when unpack failed, some successfully parsed msgs may still returned via msg_can(stick package), please note.
			if (!unpack_ok)
				reset_unpacker_state();

			return unpack_ok;
	}

	//a return value of 0 indicates that the read operation is complete. a non-zero value indicates the maximum number
	//of bytes to be read on the next call to the stream's async_read_some function. ---boost::asio::async_read
	//read as many as possible to reduce asynchronous call-back(st_tcp_socket_base::recv_handler), and don't forget to handle
	//stick package carefully in parse_msg function.
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (ec)
			return 0;

		auto data_len = cur_data_len + bytes_transferred;
		assert(data_len <= MAX_MSG_LEN);

		if ((size_t) -1 == cur_msg_len)
		{
			if (data_len >= HEAD_LEN) //the msg's head been received
			{
				cur_msg_len = HEAD_N2H(*(HEAD_TYPE*) raw_buff.begin());
				if (cur_msg_len > MAX_MSG_LEN || cur_msg_len <= HEAD_LEN) //invalid msg, stop reading
					return 0;
			}
			else
				return MAX_MSG_LEN - data_len; //read as many as possible
		}

		return data_len >= cur_msg_len ? 0 : MAX_MSG_LEN - data_len;
		//read as many as possible except that we have already got a entire msg
	}

	virtual boost::asio::mutable_buffers_1 prepare_next_recv()
	{
		assert(cur_data_len < MAX_MSG_LEN);
		return boost::asio::buffer(boost::asio::buffer(raw_buff) + cur_data_len);
	}

private:
	boost::array<char, MAX_MSG_LEN> raw_buff;
	size_t cur_msg_len; //-1 means head has not received, so, doesn't know the whole msg length.
	size_t cur_data_len; //include head
};

#include "../include/st_asio_wrapper_tcp_client.h"

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define RECONNECT_COMMAND "reconnect"
#define SUSPEND_COMMAND	"suspend"
#define RESUME_COMMAND	"resume"

int main() {
	std::string str;
	st_service_pump service_pump;
	//st_tcp_sclient client(service_pump);
	st_sclient<st_connector_base<my_msg_buffer>> client(service_pump);
	//there is no corresponding echo client demo as server endpoint
	//because echo server with echo client made dead loop, and occupy almost all the network resource

	client.set_server_addr(SERVER_PORT + 100, SERVER_IP);
//	client.set_server_addr(SERVER_PORT, "::1"); //ipv6
//	client.set_server_addr(SERVER_PORT, "127.0.0.1"); //ipv4

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::cin >> str;
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str == RECONNECT_COMMAND)
			client.graceful_close(true);
		//the following two commands demonstrate how to suspend msg sending, no matter recv buffer been used or not
		else if (str == SUSPEND_COMMAND)
			client.suspend_send_msg(true);
		else if (str == RESUME_COMMAND)
			client.suspend_send_msg(false);
		else
			client.safe_send_msg(str);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer

//#undef HUGE_MSG
//#undef MAX_MSG_LEN
//#undef MAX_MSG_NUM
//restore configuration

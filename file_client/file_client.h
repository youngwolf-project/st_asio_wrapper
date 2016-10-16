
#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <boost/timer/timer.hpp>

#include "../file_server/packer_unpacker.h"
#include "../include/ext/st_asio_wrapper_client.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::ext;

#if BOOST_VERSION >= 105300
extern boost::atomic_ushort completed_client_num;
#else
extern st_atomic<unsigned short> completed_client_num;
#endif
extern int link_num;
extern fl_type file_size;

class file_socket : public base_socket, public st_connector
{
public:
	file_socket(boost::asio::io_service& io_service_) : st_connector(io_service_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {clear(); st_connector::reset();}

	void set_index(int index_) {index = index_;}
	fl_type get_rest_size() const
	{
		auto unpacker = boost::dynamic_pointer_cast<const data_unpacker>(inner_unpacker());
		return nullptr == unpacker ? 0 : unpacker->get_rest_size();
	}
	operator fl_type() const {return get_rest_size();}

	bool get_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		if (TRANS_IDLE != state)
			return false;

		if (0 == id())
			file = fopen(file_name.data(), "w+b");
		else
			file = fopen(file_name.data(), "r+b");

		if (nullptr == file)
		{
			printf("can't create file %s.\n", file_name.data());
			return false;
		}

		std::string order("\0", ORDER_LEN);
		order += file_name;

		state = TRANS_PREPARE;
		send_msg(order, true);

		return true;
	}

	void talk(const std::string& str)
	{
		if (TRANS_IDLE == state && !str.empty())
		{
			std::string order("\2", ORDER_LEN);
			order += str;
			send_msg(order, true);
		}
	}

protected:
	//msg handling
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	//we always handle messages in on_msg(), so we don't care the type of input queue and input container at all.
	virtual bool on_msg(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//we will change unpacker at runtime, this operation can only be done in on_msg(), reset() or constructor,
	//so we must guarantee all messages to be handled in on_msg()
	//virtual bool on_msg_handle(out_msg_type& msg, bool link_down) {handle_msg(msg); return true;}
	//msg handling end

private:
	void clear()
	{
		state = TRANS_IDLE;
		if (nullptr != file)
		{
			fclose(file);
			file = nullptr;
		}

		inner_unpacker(boost::make_shared<ST_ASIO_DEFAULT_UNPACKER>());
	}
	void trans_end() {clear(); ++completed_client_num;}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());
			trans_end();
			return;
		}
		else if (msg.size() <= ORDER_LEN)
		{
			printf("wrong order length: " ST_ASIO_SF ".\n", msg.size());
			return;
		}

		switch (*msg.data())
		{
		case 0:
			if (ORDER_LEN + DATA_LEN == msg.size() && nullptr != file && TRANS_PREPARE == state)
			{
				fl_type length;
				memcpy(&length, std::next(msg.data(), ORDER_LEN), DATA_LEN);
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

					auto my_length = length / link_num;
					auto offset = my_length * index;

					if (link_num - 1 == index)
						my_length = length - offset;
					if (my_length > 0)
					{
						char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
						*buffer = 1; //head
						memcpy(std::next(buffer, ORDER_LEN), &offset, OFFSET_LEN);
						memcpy(std::next(buffer, ORDER_LEN + OFFSET_LEN), &my_length, DATA_LEN);

						state = TRANS_BUSY;
						send_msg(buffer, sizeof(buffer), true);

						fseeko(file, offset, SEEK_SET);
						inner_unpacker(boost::make_shared<data_unpacker>(file, my_length));
					}
					else
						trans_end();
				}
			}
			break;
		case 2:
			if (0 == index)
				printf("server says: %s\n", std::next(msg.data(), ORDER_LEN));
			break;
		default:
			break;
		}
	}

private:
	int index;
};

class file_client : public st_tcp_client_base<file_socket>
{
public:
	static const tid TIMER_BEGIN = st_tcp_client_base<file_socket>::TIMER_END;
	static const tid UPDATE_PROGRESS = TIMER_BEGIN;
	static const tid TIMER_END = TIMER_BEGIN + 10;

	file_client(st_service_pump& service_pump_) : st_tcp_client_base<file_socket>(service_pump_) {}

	void start()
	{
		begin_time.start();
		set_timer(UPDATE_PROGRESS, 50, [this](tid id)->bool {return ST_THIS update_progress_handler(id, -1);});
	}

	void stop(const std::string& file_name)
	{
		stop_timer(UPDATE_PROGRESS);

		auto used_time = (double) begin_time.elapsed().wall / 1000000000;
		printf("\r100%%\ntransfer %s end, speed: %.0f kB/s.\n", file_name.data(), file_size / used_time / 1024);
	}

	fl_type get_total_rest_size()
	{
		fl_type total_rest_size = 0;
		do_something_to_all([&total_rest_size](object_ctype& item) {total_rest_size += *item;});
//		do_something_to_all([&total_rest_size](object_ctype& item) {total_rest_size += item->get_rest_size();});

		return total_rest_size;
	}

private:
	bool update_progress_handler(tid id, unsigned last_percent)
	{
		assert(UPDATE_PROGRESS == id);

		auto total_rest_size = get_total_rest_size();
		if (total_rest_size > 0)
		{
			auto new_percent = (unsigned) ((file_size - total_rest_size) * 100 / file_size);
			if (last_percent != new_percent)
			{
				printf("\r%u%%", new_percent);
				fflush(stdout);

				ST_THIS update_timer_info(id, 50, [new_percent, this](tid id)->bool {return ST_THIS update_progress_handler(id, new_percent);});
			}
		}

		return true;
	}

protected:
	boost::timer::cpu_timer begin_time;
};

#endif //#ifndef FILE_CLIENT_H_


#ifndef FILE_CLIENT_H_
#define FILE_CLIENT_H_

#include <boost/timer/timer.hpp>

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#include "../file_common/file_buffer.h"
#include "../file_common/unpacker.h"

extern int link_num;
extern fl_type file_size;
extern atomic_size transmit_size;

class file_matrix : public i_matrix
{
public:
	virtual void put_file(const std::string& file_name) = 0;
};

class file_socket : public base_socket, public client_socket2<file_matrix>
{
public:
	file_socket(file_matrix& matrix_) : client_socket2<file_matrix>(matrix_), index(-1) {}
	virtual ~file_socket() {clear();}

	//reset all, be ensure that there's no any operations performed on this file_socket when invoke it
	virtual void reset() {trans_end(); client_socket2<file_matrix>::reset();}

	bool is_idle() const {return TRANS_IDLE == state;}
	void set_index(int index_) {index = index_;}

	bool get_file(const std::string& file_name, bool master_only)
	{
		assert(!file_name.empty());

		if ((master_only && 0 != index) || (!master_only && 0 == index))
			return true;
		else if (TRANS_IDLE != state)
			return false;
		else if (master_only)
			file = fopen(file_name.data(), "w+b");
		else
			file = fopen(file_name.data(), "r+b");

		if (NULL == file)
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

	bool truncate_file(const std::string& file_name, bool master_only)
	{
		assert(!file_name.empty());

		if ((master_only && 0 != index) || (!master_only && 0 == index))
			return true;
		else if (TRANS_IDLE != state)
			return false;
		else if (master_only)
		{
			std::string order(ORDER_LEN, (char) 10);
			order += file_name;

			state = TRANS_PREPARE;
			send_msg(order, true);
		}

		return true;
	}

	bool put_file(const std::string& file_name)
	{
		assert(!file_name.empty());

		file = fopen(file_name.data(), "rb");
		bool re = NULL != file;
		if (re)
		{
			fseeko(file, 0, SEEK_END);
			fl_type length = ftello(file);
			if (0 == index)
				file_size = length;

			fl_type my_length = length / link_num;
			fl_type offset = my_length * index;
			fseeko(file, offset, SEEK_SET);

			if (link_num - 1 == index)
				my_length = length - offset;
			if (my_length > 0)
			{
				char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
				*buffer = 11; //head

				memcpy(boost::next(buffer, ORDER_LEN), &offset, OFFSET_LEN);
				memcpy(boost::next(buffer, ORDER_LEN + OFFSET_LEN), &my_length, DATA_LEN);

				std::string order(buffer, sizeof(buffer));
				order += file_name;

				state = TRANS_PREPARE;
				send_msg(order, true);
				return true;
			}
		}
		else
			printf("can not open file %s.\n", file_name.data());

		trans_end(false);
		return re;
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
#ifdef ST_ASIO_SYNC_DISPATCH
	virtual size_t on_msg(list<out_msg_type>& msg_can)
	{
		//st_asio_wrapper will never append empty message automatically for on_msg (if no message nor error returned from the unpacker) even with
		// macro ST_ASIO_PASSIVE_RECV, but will do it for on_msg_handle (with macro ST_ASIO_PASSIVE_RECV), please note.
		if (msg_can.empty())
			handle_msg(out_msg_type()); //we need empty message as a notification, it's just our business logic.
		else
		{
			st_asio_wrapper::do_something_to_all(msg_can, boost::bind(&file_socket::handle_msg, this, boost::placeholders::_1));
			msg_can.clear();
		}

		recv_msg(); //we always handled all messages, so calling recv_msg() at here is very reasonable.
		return 1;
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number)
		//if we handled nothing, return a positive number is also okey but will very slightly impact performance (if msg_can is not empty), return 0 is suggested
	}
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		//msg_can can't be empty, with macro ST_ASIO_PASSIVE_RECV, st_asio_wrapper will append an empty message automatically for on_msg_handle if no message nor
		// error returned from the unpacker to provide a chance to call recv_msg (calling recv_msg out of on_msg and on_msg_handle is forbidden), please note.
		assert(!msg_can.empty());
		out_container_type tmp_can;
		msg_can.swap(tmp_can);

		st_asio_wrapper::do_something_to_all(tmp_can, boost::bind(&file_socket::handle_msg, this, boost::placeholders::_1));

		recv_msg(); //we always handled all messages, so calling recv_msg() at here is very reasonable.
		return tmp_can.size();
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number), else, return 0
		//if we handled nothing, but want to re-dispatch messages immediately, return a positive number
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); if (0 == get_pending_recv_msg_size()) recv_msg(); return true;}
	//only raise recv_msg() invocation after recveiving buffer becomes empty, it's very important, otherwise we must use mutex to guarantee that at any time,
	//there only exists one or zero asynchronous reception.
#endif
	//msg handling end

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg)
	{
		file_buffer* buffer = dynamic_cast<file_buffer*>(&*msg.raw_buffer());
		if (NULL != buffer)
		{
			if (!buffer->read())
				trans_end(false);
			else if (buffer->empty())
			{
				puts("file sending end successfully");
				trans_end(false);
			}
			else
				direct_send_msg(msg, true);
		}
	}
#endif

	virtual void on_connect()
	{
		boost::uint_fast64_t id = index;
		char buffer[ORDER_LEN + sizeof(boost::uint_fast64_t)];

		*buffer = 3; //head
		memcpy(boost::next(buffer, ORDER_LEN), &id, sizeof(boost::uint_fast64_t));
		send_msg(buffer, sizeof(buffer), true);

		client_socket2<file_matrix>::on_connect();
	}

private:
	void clear()
	{
		if (NULL != file)
		{
			fclose(file);
			file = NULL;
		}
	}

	void trans_end(bool reset_unpacker = true)
	{
		clear();

		if (reset_unpacker)
			unpacker(boost::make_shared<ST_ASIO_DEFAULT_UNPACKER>());
		state = TRANS_IDLE;
	}

	void handle_msg(out_msg_ctype& msg)
	{
		if (TRANS_BUSY == state)
		{
			assert(msg.empty());

			BOOST_AUTO(unp, boost::dynamic_pointer_cast<file_unpacker>(unpacker()));
			if (!unp)
				trans_end();
			else if (unp->is_finished())
			{
				puts("file accepting end successfully");
				trans_end();
			}

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
			if (ORDER_LEN + DATA_LEN == msg.size() && NULL != file && TRANS_PREPARE == state)
			{
				fl_type length;
				memcpy(&length, boost::next(msg.data(), ORDER_LEN), DATA_LEN);
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

					fl_type my_length = length / link_num;
					fl_type offset = my_length * index;

					if (link_num - 1 == index)
						my_length = length - offset;
					if (my_length > 0)
					{
						char buffer[ORDER_LEN + OFFSET_LEN + DATA_LEN];
						*buffer = 1; //head
						memcpy(boost::next(buffer, ORDER_LEN), &offset, OFFSET_LEN);
						memcpy(boost::next(buffer, ORDER_LEN + OFFSET_LEN), &my_length, DATA_LEN);

						printf("start to accept the file from " ST_ASIO_LLF " with legnth " ST_ASIO_LLF "\n", offset, my_length);

						state = TRANS_BUSY;
						fseeko(file, offset, SEEK_SET);
						unpacker(boost::make_shared<file_unpacker>(file, my_length, &transmit_size));

						send_msg(buffer, sizeof(buffer), true); //replace the unpacker first, then response get file request
					}
					else
						trans_end();
				}
			}
			break;
		case 10:
			if (msg.size() > ORDER_LEN + 1 && NULL == file && TRANS_PREPARE == state)
			{
				const char* file_name = boost::next(msg.data(), ORDER_LEN + 1);
				if ('\0' != *boost::next(msg.data(), ORDER_LEN))
				{
					if (0 == index)
						printf("cannot create or truncated file %s on the server\n", file_name);
					trans_end();
				}
				else
				{
					if (0 == index)
						printf("prepare to send file %s\n", file_name);
					get_matrix()->put_file(boost::next(msg.data(), ORDER_LEN + 1));
				}
			}
			break;
		case 11:
			if (ORDER_LEN + DATA_LEN + 1 == msg.size() && NULL != file && TRANS_PREPARE == state)
			{
				if ('\0' != *boost::next(msg.data(), ORDER_LEN + DATA_LEN))
				{
					if (link_num - 1 == index)
						puts("put file failed!");
					trans_end();
				}
				else
				{
					fl_type my_length;
					memcpy(&my_length, boost::next(msg.data(), ORDER_LEN), DATA_LEN);
					printf("start to send the file with length " ST_ASIO_LLF "\n", my_length);

					state = TRANS_BUSY;
					BOOST_AUTO(buffer, new file_buffer(file, my_length, &transmit_size));
					if (buffer->is_good())
					{
						in_msg_type msg(buffer);
						direct_send_msg(msg, true);
					}
					else
					{
						delete buffer;
						trans_end();
					}
				}
			}
			break;
		case 2:
			if (0 == index)
				printf("server says: %s\n", boost::next(msg.data(), ORDER_LEN));
			break;
		default:
			break;
		}
	}

private:
	int index;
};

class file_client : public multi_client2<file_socket, file_matrix>
{
public:
	static const tid TIMER_BEGIN = multi_client2<file_socket, file_matrix>::TIMER_END;
	static const tid UPDATE_PROGRESS = TIMER_BEGIN;
	static const tid TIMER_END = TIMER_BEGIN + 5;

	file_client(service_pump& service_pump_) : multi_client2<file_socket, file_matrix>(service_pump_) {}

	void get_file(const boost::container::list<std::string>& files)
	{
		boost::unique_lock<boost::mutex> lock(file_list_mutex);
		for (BOOST_AUTO(iter, files.begin()); iter != files.end(); ++iter)
			file_list.emplace_back(std::make_pair(0, *iter));
		lock.unlock();

		transmit_file();
	}

	void put_file(const boost::container::list<std::string>& files)
	{
		boost::unique_lock<boost::mutex> lock(file_list_mutex);
		for (BOOST_AUTO(iter, files.begin()); iter != files.end(); ++iter)
			file_list.emplace_back(std::make_pair(1, *iter));
		lock.unlock();

		transmit_file();
	}

	bool is_end()
	{
		size_t idle_num = 0;
		do_something_to_all(boost::lambda::if_then(boost::lambda::bind(&file_socket::is_idle, *boost::lambda::_1), ++boost::lambda::var(idle_num)));
		return idle_num == size();
	}

protected:
	virtual void put_file(const std::string& file_name) {do_something_to_all(boost::bind(&file_socket::put_file, boost::placeholders::_1, boost::cref(file_name)));}

private:
	void transmit_file()
	{
		boost::lock_guard<boost::mutex> lock(file_list_mutex);

		if (is_timer(UPDATE_PROGRESS))
			return;

		while (!file_list.empty())
		{
			std::string file_name;
			file_name.swap(file_list.front().second);
			int type = file_list.front().first;
			file_list.pop_front();

			file_size = -1;
			transmit_size = 0;

			printf("transmit %s begin.\n", file_name.data());
			bool re = true;
			if (0 == type)
			{
				do_something_to_all(boost::lambda::if_then(!boost::lambda::bind(&file_socket::get_file, *boost::lambda::_1, boost::lambda::constant_ref(file_name), true), boost::lambda::var(re) = false));
				do_something_to_all(boost::lambda::if_then(!boost::lambda::bind(&file_socket::get_file, *boost::lambda::_1, boost::lambda::constant_ref(file_name), false), boost::lambda::var(re) = false));
			}
			else
				do_something_to_all(boost::lambda::if_then(!boost::lambda::bind(&file_socket::truncate_file, *boost::lambda::_1, boost::lambda::constant_ref(file_name), true), boost::lambda::var(re) = false));

			if (re)
			{
				begin_time.start();
				set_timer(UPDATE_PROGRESS, 50, boost::bind(&file_client::update_progress_handler, this, boost::placeholders::_1, -1));
				break;
			}
			else
				printf("transmit %s failed!\n", file_name.data());
		}
	}

	bool update_progress_handler(tid id, unsigned last_percent)
	{
		assert(UPDATE_PROGRESS == id);

		if (file_size < 0)
		{
			if (!is_end())
				return true;

			change_timer_status(id, timer_info::TIMER_CANCELED);
			transmit_file();

			return false;
		}
		else if (file_size > 0)
		{
			unsigned new_percent = (unsigned) (transmit_size * 100 / file_size);
			if (last_percent != new_percent)
			{
				printf("\r%u%%", new_percent);
				fflush(stdout);

				change_timer_call_back(id, boost::bind(&file_client::update_progress_handler, this, boost::placeholders::_1, new_percent));
			}
		}

		if (transmit_size < file_size)
		{
			if (!is_end())
				return true;

			change_timer_status(id, timer_info::TIMER_CANCELED);
			transmit_file();

			return false;
		}

		double used_time = (double) begin_time.elapsed().wall / 1000000000;
		printf("\r100%%\nend, speed: %f MBps.\n\n", file_size / used_time / 1024 / 1024);
		change_timer_status(id, timer_info::TIMER_CANCELED);

		//wait all file_socket to clean up themselves
		while (!is_end()) boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
		transmit_file();

		return false;
	}

protected:
	boost::timer::cpu_timer begin_time;

#if BOOST_VERSION >= 107900 && (defined(_MSC_VER) || defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L)
	std::list<std::pair<int, std::string>> file_list; //a workaround for a bug introduced in boost 1.79
#else
	boost::container::list<std::pair<int, std::string> > file_list;
#endif
	boost::mutex file_list_mutex;
};

#endif //#ifndef FILE_CLIENT_H_

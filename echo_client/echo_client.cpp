
#include <iostream>
#include <boost/timer/timer.hpp>
#include <boost/tokenizer.hpp>

//configuration
#define ST_ASIO_SERVER_PORT		9528
//#define ST_ASIO_REUSE_OBJECT //use objects pool
#define ST_ASIO_DELAY_CLOSE		5 //define this to avoid hooks for async call (and slightly improve efficiency)
#define ST_ASIO_CLEAR_OBJECT_INTERVAL 1
#define ST_ASIO_SYNC_DISPATCH
#define ST_ASIO_DISPATCH_BATCH_MSG
//#define ST_ASIO_WANT_MSG_SEND_NOTIFY
//#define ST_ASIO_FULL_STATISTIC //full statistic will slightly impact efficiency
//#define ST_ASIO_USE_STEADY_TIMER
#define ST_ASIO_USE_SYSTEM_TIMER
#define ST_ASIO_AVOID_AUTO_STOP_SERVICE
//#define ST_ASIO_DECREASE_THREAD_AT_RUNTIME
//#define ST_ASIO_MAX_SEND_BUF	65536
//#define ST_ASIO_MAX_RECV_BUF	65536
//if there's a huge number of links, please reduce messge buffer via ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF macro.
//please think about if we have 512 links, how much memory we can accupy at most with default ST_ASIO_MAX_SEND_BUF and ST_ASIO_MAX_RECV_BUF?
//it's 2 * 1M * 512 = 1G

//use the following macro to control the type of packer and unpacker
#define PACKER_UNPACKER_TYPE	0
//0-default packer and unpacker, head(length) + body
//1-packer2 and unpacker2, head(length) + body
//2-fixed length packer and unpacker
//3-prefix and/or suffix packer and unpacker

#if 0 == PACKER_UNPACKER_TYPE
#define ST_ASIO_HUGE_MSG
#define ST_ASIO_MSG_BUFFER_SIZE 1000000
#define ST_ASIO_MAX_SEND_BUF (10 * ST_ASIO_MSG_BUFFER_SIZE)
#define ST_ASIO_MAX_RECV_BUF (10 * ST_ASIO_MSG_BUFFER_SIZE)
#define ST_ASIO_DEFAULT_UNPACKER flexible_unpacker<>
//this unpacker only pre-allocated a buffer of 4000 bytes, but it can parse messages up to ST_ASIO_MSG_BUFFER_SIZE (here is 1000000) bytes,
//it works as the default unpacker for messages <= 4000, otherwise, it works as non_copy_unpacker
#elif 1 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER packer2<unique_buffer<std::string>, std::string>
#define ST_ASIO_DEFAULT_UNPACKER unpacker2<unique_buffer<std::string>, std::string, flexible_unpacker<std::string> >
#elif 2 == PACKER_UNPACKER_TYPE
#undef ST_ASIO_HEARTBEAT_INTERVAL
#define ST_ASIO_HEARTBEAT_INTERVAL	0 //not support heartbeat
#define ST_ASIO_DEFAULT_PACKER fixed_length_packer
#define ST_ASIO_DEFAULT_UNPACKER fixed_length_unpacker
#elif 3 == PACKER_UNPACKER_TYPE
#define ST_ASIO_DEFAULT_PACKER prefix_suffix_packer
#define ST_ASIO_DEFAULT_UNPACKER prefix_suffix_unpacker
#endif
//configuration

#include "../include/ext/tcp.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;
using namespace st_asio_wrapper::ext;
using namespace st_asio_wrapper::ext::tcp;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define STATUS			"status"
#define STATISTIC		"statistic"
#define LIST_ALL_CLIENT	"list all client"
#define INCREASE_THREAD	"increase thread"
#define DECREASE_THREAD	"decrease thread"

static bool check_msg;

///////////////////////////////////////////////////
//msg sending interface
#define TCP_RANDOM_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false, bool prior = false) \
{ \
	size_t index = (size_t) ((boost::uint64_t) rand() * (size() - 1) / RAND_MAX); \
	BOOST_AUTO(socket_ptr, at(index)); \
	return socket_ptr ? socket_ptr->SEND_FUNNAME(pstr, len, num, can_overflow, prior) : false; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//msg sending interface
///////////////////////////////////////////////////

class echo_socket : public client_socket
{
public:
	echo_socket(i_matrix& matrix_) : client_socket(matrix_), recv_bytes(0), recv_index(0)
	{
#if 3 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_PACKER>(packer())->prefix_suffix("begin", "end");
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("begin", "end");
#endif
	}

	boost::uint64_t get_recv_bytes() const {return recv_bytes;}
	operator boost::uint64_t() const {return recv_bytes;}

	void clear_status() {recv_bytes = recv_index = 0;}
	void begin(size_t msg_num_, size_t msg_len, char msg_fill)
	{
		clear_status();
		msg_num = msg_num_;

		std::string msg(msg_len, msg_fill);
		msg.replace(0, sizeof(size_t), (const char*) &recv_index, sizeof(size_t)); //seq

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
		send_msg(boost::cref(msg).get()); //can not apply new feature introduced by version 2.2.0, we need a whole message in on_msg_send
#else
		send_msg(msg); //new feature introduced by version 2.2.0, avoid one memory replication
#endif
	}

protected:
	//msg handling
#ifdef ST_ASIO_SYNC_DISPATCH
	//do not hold msg_can for further usage, return from on_msg as quickly as possible
	//access msg_can freely within this callback, it's always thread safe.
	virtual size_t on_msg(list<out_msg_type>& msg_can)
	{
		st_asio_wrapper::do_something_to_all(msg_can, boost::bind(&echo_socket::handle_msg, this, boost::placeholders::_1));
		msg_can.clear(); //if we left behind some messages in msg_can, they will be dispatched via on_msg_handle asynchronously, which means it's
		//possible that on_msg_handle be invoked concurrently with the next on_msg (new messages arrived) and then disorder messages.
		//here we always consumed all messages, so we can use sync message dispatching, otherwise, we should not use sync message dispatching
		//except we can bear message disordering.

		return 1;
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number)
		//if we handled nothing, return a positive number is also okey but will very slightly impact performance (if msg_can is not empty), return 0 is suggested
	}
#endif
#ifdef ST_ASIO_DISPATCH_BATCH_MSG
	//do not hold msg_can for further usage, access msg_can and return from on_msg_handle as quickly as possible
	//can only access msg_can via functions that marked as 'thread safe', if you used non-lock queue, its your responsibility to guarantee
	// that new messages will not come until we returned from this callback (for example, pingpong test).
	virtual size_t on_msg_handle(out_queue_type& msg_can)
	{
		out_container_type tmp_can;
		msg_can.swap(tmp_can); //to consume a part of the messages in msg_can, see echo_server

		st_asio_wrapper::do_something_to_all(tmp_can, boost::bind(&echo_socket::handle_msg, this, boost::placeholders::_1));
		return tmp_can.size();
		//if we indeed handled some messages, do return the actual number of handled messages (or a positive number), else, return 0
		//if we handled nothing, but want to re-dispatch messages immediately, return 1
	}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {handle_msg(msg); return true;}
#endif
	//msg handling end

#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
	virtual void on_msg_send(in_msg_type& msg)
	{
		if (0 == --msg_num)
			return;

		char* pstr = packer()->raw_data(msg);
		size_t msg_len = packer()->raw_data_len(msg);

		size_t send_index;
		memcpy(&send_index, pstr, sizeof(size_t));
		++send_index;
		memcpy(pstr, &send_index, sizeof(size_t)); //seq

		send_msg(pstr, msg_len, true);
	}
#endif

private:
	void handle_msg(out_msg_ctype& msg)
	{
		recv_bytes += msg.size();
		if (check_msg && (msg.size() < sizeof(size_t) || 0 != memcmp(&recv_index, msg.data(), sizeof(size_t))))
			printf("check msg error: " ST_ASIO_LLF "->" ST_ASIO_SF "/" ST_ASIO_SF ".\n", id(), recv_index, *(size_t*) msg.data());
		++recv_index;

		//i'm the bottleneck -_-
//		boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
	}

private:
	boost::uint64_t recv_bytes;
	size_t recv_index, msg_num;
};

class echo_client : public multi_client_base<echo_socket>
{
public:
	echo_client(service_pump& service_pump_) : multi_client_base<echo_socket>(service_pump_) {}

	boost::uint64_t get_recv_bytes()
	{
		boost::uint64_t total_recv_bytes = 0;
		do_something_to_all(total_recv_bytes += *boost::lambda::_1);
//		do_something_to_all(total_recv_bytes += boost::lambda::bind(&echo_socket::get_recv_bytes, *boost::lambda::_1));

		return total_recv_bytes;
	}

	void clear_status() {do_something_to_all(boost::mem_fn(&echo_socket::clear_status));}
	void begin(size_t msg_num, size_t msg_len, char msg_fill) {do_something_to_all(boost::bind(&echo_socket::begin, boost::placeholders::_1, msg_num, msg_len, msg_fill));}

	void shutdown_some_client(int n)
	{
		static int index = -1;
		++index;

		switch (index % 6)
		{
#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
			//method #1
			//notice: these methods need to define ST_ASIO_CLEAR_OBJECT_INTERVAL macro, because it just shut down the socket,
			//not really remove them from object pool, this will cause echo_client still send data via them, and wait responses from them.
			//for this scenario, the smaller ST_ASIO_CLEAR_OBJECT_INTERVAL macro is, the better experience you will get, so set it to 1 second.
		case 0: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::graceful_shutdown, *boost::lambda::_1, false, true)));	break;
		case 1: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::graceful_shutdown, *boost::lambda::_1, false, false)));	break;
		case 2: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::force_shutdown, *boost::lambda::_1, false)));				break;
#else
			//method #2
			//this is a equivalence of calling i_server::del_socket in server_socket_base::on_recv_error (see server_socket_base for more details).
		case 0: while (n-- > 0) graceful_shutdown(at(0));			break;
		case 1: while (n-- > 0) graceful_shutdown(at(0), false);	break;
		case 2: while (n-- > 0) force_shutdown(at(0));				break;
#endif
			//if you just want to reconnect to the server, you should do it like this:
		case 3: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::graceful_shutdown, *boost::lambda::_1, true, true)));		break;
		case 4: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::graceful_shutdown, *boost::lambda::_1, true, false)));	break;
		case 5: do_something_to_all(boost::lambda::if_then(boost::lambda::var(n)-- > 0, boost::lambda::bind(&echo_socket::force_shutdown, *boost::lambda::_1, true)));				break;
		}
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_RANDOM_SEND_MSG(random_send_msg, send_msg)
	TCP_RANDOM_SEND_MSG(random_send_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow is false, success at here just means putting the msg into tcp::socket's send buffer successfully
	TCP_RANDOM_SEND_MSG(safe_random_send_msg, safe_send_msg)
	TCP_RANDOM_SEND_MSG(safe_random_send_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////
};

void send_msg_one_by_one(echo_client& client, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = true;
	boost::uint64_t total_msg_bytes = msg_num * msg_len * client.size();

	boost::timer::cpu_timer begin_time;
	client.begin(msg_num, msg_len, msg_fill);
	unsigned percent = 0;
	do
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

		unsigned new_percent = (unsigned) (100 * client.get_recv_bytes() / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	} while (percent < 100);
	begin_time.stop();

	double used_time = (double) begin_time.elapsed().wall / 1000000000;
	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n", used_time, client.size() * msg_num / used_time, total_msg_bytes / used_time / 1024 / 1024);
}

void send_msg_randomly(echo_client& client, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = false;
	boost::uint64_t send_bytes = 0;
	boost::uint64_t total_msg_bytes = msg_num * msg_len;
	char* buff = new char[msg_len];
	memset(buff, msg_fill, msg_len);

	boost::timer::cpu_timer begin_time;
	unsigned percent = 0;
	for (size_t i = 0; i < msg_num; ++i)
	{
		memcpy(buff, &i, sizeof(size_t)); //seq

		client.safe_random_send_msg((const char*) buff, msg_len); //can_overflow is false, it's important
		send_bytes += msg_len;

		unsigned new_percent = (unsigned) (100 * send_bytes / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	}

	while(client.get_recv_bytes() < total_msg_bytes)
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

	begin_time.stop();
	delete[] buff;

	double used_time = (double) begin_time.elapsed().wall / 1000000000;
	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n", used_time, msg_num / used_time, total_msg_bytes / used_time / 1024 / 1024);
}

typedef boost::container::list<echo_client::object_type> link_container;
void thread_runtine(link_container& link_group, size_t msg_num, size_t msg_len, char msg_fill)
{
	char* buff = new char[msg_len];
	memset(buff, msg_fill, msg_len);
	for (size_t i = 0; i < msg_num; ++i)
	{
		memcpy(buff, &i, sizeof(size_t)); //seq
		st_asio_wrapper::do_something_to_all(link_group, boost::bind((bool (echo_socket::*)(const char*, size_t, bool, bool)) &echo_socket::safe_send_msg,
			boost::placeholders::_1, buff, msg_len, false, false));
		//can_overflow is false, it's important
	}
	delete[] buff;
}

void group_links(echo_client::object_ctype& link, std::vector<link_container>& link_groups, size_t group_link_num,
	size_t& group_index, size_t& this_group_link_num, size_t& left_link_num)
{
	if (0 == this_group_link_num)
	{
		this_group_link_num = group_link_num;
		if (left_link_num > 0)
		{
			++this_group_link_num;
			--left_link_num;
		}

		++group_index;
	}

	--this_group_link_num;
	link_groups[group_index].emplace_back(link);
}

//use up to a specific worker threads to send messages concurrently
void send_msg_concurrently(echo_client& client, size_t send_thread_num, size_t msg_num, size_t msg_len, char msg_fill)
{
	check_msg = true;
	size_t link_num = client.size();
	size_t group_num = std::min(send_thread_num, link_num);
	size_t group_link_num = link_num / group_num;
	size_t left_link_num = link_num - group_num * group_link_num;
	boost::uint64_t total_msg_bytes = link_num * msg_len;
	total_msg_bytes *= msg_num;

	size_t group_index = (size_t) -1;
	size_t this_group_link_num = 0;

	std::vector<link_container> link_groups(group_num);
	client.do_something_to_all(boost::bind(&group_links, boost::placeholders::_1, boost::ref(link_groups), group_link_num,
		boost::ref(group_index), boost::ref(this_group_link_num), boost::ref(left_link_num)));

	boost::timer::cpu_timer begin_time;
	boost::thread_group threads;
	for (BOOST_AUTO(iter, link_groups.begin()); iter != link_groups.end(); ++iter)
		threads.create_thread(boost::bind(&thread_runtine, boost::ref(*iter), msg_num, msg_len, msg_fill));

	unsigned percent = 0;
	do
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

		unsigned new_percent = (unsigned) (100 * client.get_recv_bytes() / total_msg_bytes);
		if (percent != new_percent)
		{
			percent = new_percent;
			printf("\r%u%%", percent);
			fflush(stdout);
		}
	} while (percent < 100);
	threads.join_all();
	begin_time.stop();

	double used_time = (double) begin_time.elapsed().wall / 1000000000;
	printf(" finished in %f seconds, TPS: %f(*2), speed: %f(*2) MBps.\n", used_time, link_num * msg_num / used_time, total_msg_bytes / used_time / 1024 / 1024);
}

static bool is_testing;
void start_test(int repeat_times, char mode, echo_client& client, size_t send_thread_num, size_t msg_num, size_t msg_len, char msg_fill)
{
	for (int i = 0; i < repeat_times; ++i)
	{
		printf("this is the %d / %d test...\n", i + 1, repeat_times);
		client.clear_status();
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
		if (0 == mode)
			send_msg_one_by_one(client, msg_num, msg_len, msg_fill);
		else
		{
			puts("if ST_ASIO_WANT_MSG_SEND_NOTIFY defined, only support mode 0!");
			break;
		}
#else
		if (0 == mode)
			send_msg_concurrently(client, send_thread_num, msg_num, msg_len, msg_fill);
		else
			send_msg_randomly(client, msg_num, msg_len, msg_fill);
#endif
	}

	is_testing = false;
}

int main(int argc, const char* argv[])
{
	printf("usage: %s [<service thread number=4> [<send thread number=8> [<port=%d> [<ip=%s> [link num=16]]]]]\n", argv[0], ST_ASIO_SERVER_PORT, ST_ASIO_SERVER_IP);
	if (argc >= 2 && (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")))
		return 0;
	else
		puts("type " QUIT_COMMAND " to end.");

	///////////////////////////////////////////////////////////
	size_t link_num = 16;
	if (argc > 5)
		link_num = std::min(ST_ASIO_MAX_OBJECT_NUM, std::max(atoi(argv[5]), 1));

	printf("exec: %s with " ST_ASIO_SF " links\n", argv[0], link_num);
	///////////////////////////////////////////////////////////

	service_pump sp;
#ifndef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
	//if you want to decrease service thread at runtime, then you cannot use multiple io_context, if somebody indeed needs it, please let me know.
	//with multiple io_context, the number of service thread must be bigger than or equal to the number of io_context, please note.
	//with multiple io_context, please also define macro ST_ASIO_AVOID_AUTO_STOP_SERVICE.
	sp.set_io_context_num(4);
#endif
	echo_client client(sp);
	//echo client means to cooperate with echo server while doing performance test, it will not send msgs back as echo server does,
	//otherwise, dead loop will occur, network resource will be exhausted.

//	argv[4] = "::1" //ipv6
//	argv[4] = "127.0.0.1" //ipv4
	std::string ip = argc > 4 ? argv[4] : ST_ASIO_SERVER_IP;
	unsigned short port = argc > 3 ? atoi(argv[3]) : ST_ASIO_SERVER_PORT;

	//method #1, create and add clients manually.
	BOOST_AUTO(socket_ptr, client.create_object());
	//socket_ptr->set_server_addr(port, ip); //we don't have to set server address at here, the following do_something_to_all will do it for us
	//some other initializations according to your business
	client.add_socket(socket_ptr);
	socket_ptr.reset(); //important, otherwise, object_pool will not be able to free or reuse this object.

	//method #2, add clients first without any arguments, then set the server address.
	for (size_t i = 1; i < link_num / 2; ++i)
		client.add_socket();
	client.do_something_to_all(boost::bind(&echo_socket::set_server_addr, boost::placeholders::_1, port, boost::ref(ip)));

	//method #3, add clients and set server address in one invocation.
	for (size_t i = std::max((size_t) 1, link_num / 2); i < link_num; ++i)
		client.add_socket(port, ip);

	size_t send_thread_num = 8;
	if (argc > 2)
		send_thread_num = (size_t) std::max(1, std::min(16, atoi(argv[2])));

	int thread_num = 1;
	if (argc > 1)
		thread_num = std::min(16, std::max(thread_num, atoi(argv[1])));
	//add one thread will seriously impact IO throughput when doing performance benchmark, this is because the business logic is very simple (send original messages back,
	//or just add up total message size), under this scenario, just one service thread without receiving buffer will obtain the best IO throughput.
	//the server has such behavior too.

	sp.start_service(std::max(thread_num, sp.get_io_context_num()));
	while(sp.is_running())
	{
		std::string str;
		std::getline(std::cin, str);
		if (str.empty())
			;
		else if (STATISTIC == str)
		{
			printf("link #: " ST_ASIO_SF ", valid links: " ST_ASIO_SF ", invalid links: " ST_ASIO_SF "\n\n", client.size(), client.valid_size(), client.invalid_object_size());
			static statistic last_stat;
			statistic this_stat = client.get_statistic();
			puts((this_stat - last_stat).to_string().data());
			last_stat = this_stat;
		}
		else if (STATUS == str)
			client.list_all_status();
		else if (LIST_ALL_CLIENT == str)
			client.list_all_object();
		else if (INCREASE_THREAD == str)
			sp.add_service_thread(1);
#ifdef ST_ASIO_DECREASE_THREAD_AT_RUNTIME
		else if (DECREASE_THREAD == str)
			sp.del_service_thread(1);
#endif
		else if (is_testing)
			puts("testing has not finished yet!");
		else if (QUIT_COMMAND == str)
			sp.stop_service();
		else if (RESTART_COMMAND == str)
		{
			sp.stop_service();
			sp.start_service(thread_num);
		}
		else
		{
			if ('+' == str[0] || '-' == str[0])
			{
				int n = atoi(boost::next(str.data()));
				if (n <= 0)
					n = 1;

				if ('+' == str[0])
					for (; n > 0 && client.add_socket(port, ip); --n);
				else
				{
					if ((size_t) n > client.size())
						n = (int) client.size();

					client.shutdown_some_client(n);
				}

				link_num = client.size();
				continue;
			}

#ifdef ST_ASIO_CLEAR_OBJECT_INTERVAL
			link_num = client.size();
			if (link_num != client.valid_size())
			{
				puts("please wait for a while, because object_pool has not cleaned up invalid links.");
				continue;
			}
#endif
			size_t msg_num = 1024;
			size_t msg_len = 1024; //must greater than or equal to sizeof(size_t)
			char msg_fill = '0';
			char mode = 0; //0 broadcast, 1 randomly pick one link per msg
			int repeat_times = 1;

			boost::char_separator<char> sep(" \t");
			boost::tokenizer<boost::char_separator<char> > parameters(str, sep);
			BOOST_AUTO(iter, parameters.begin());
			if (iter != parameters.end()) msg_num = std::max((size_t) atoi(iter++->data()), (size_t) 1);

#if 0 == PACKER_UNPACKER_TYPE || 1 == PACKER_UNPACKER_TYPE
			if (iter != parameters.end()) msg_len = std::min(packer<>::get_max_msg_size(),
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#elif 2 == PACKER_UNPACKER_TYPE
			if (iter != parameters.end()) ++iter;
			msg_len = 1024; //we hard code this because the default fixed length is 1024, and we not changed it
#elif 3 == PACKER_UNPACKER_TYPE
			if (iter != parameters.end()) msg_len = std::min((size_t) ST_ASIO_MSG_BUFFER_SIZE,
				std::max((size_t) atoi(iter++->data()), sizeof(size_t))); //include seq
#endif
			if (iter != parameters.end()) msg_fill = *iter++->data();
			if (iter != parameters.end()) mode = *iter++->data() - '0';
			if (iter != parameters.end()) repeat_times = std::max(atoi(iter++->data()), 1);

			if (0 != mode && 1 != mode)
				puts("unrecognized mode!");
			else
			{
				printf("test parameters after adjustment: " ST_ASIO_SF " " ST_ASIO_SF " %c %d\n", msg_num, msg_len, msg_fill, mode);
				puts("performance test begin, this application will have no response during the test!");

				is_testing = true;
				boost::thread(boost::bind(&start_test, repeat_times, mode, boost::ref(client), send_thread_num, msg_num, msg_len, msg_fill)).detach();
			}
		}
	}

    return 0;
}

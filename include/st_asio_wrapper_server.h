/*
 * st_asio_wrapper_server.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVER_H_
#define ST_ASIO_WRAPPER_SERVER_H_

#include "st_asio_wrapper_server_socket.h"

#ifndef SERVER_PORT
#define SERVER_PORT					5050
#endif
#ifndef MAX_CLIENT_NUM
#define MAX_CLIENT_NUM				4096
#endif

#ifndef ASYNC_ACCEPT_NUM
#define ASYNC_ACCEPT_NUM			1 //how many async_accept delivery concurrently
#endif

//something like memory pool, if you open REUSE_CLIENT, all clients in temp_client_can will never be freed,
//but waiting for reuse
//or, st_server_base will free the clients in temp_client_can automatically and periodically,
//use CLIENT_FREE_INTERVAL to set the interval,
//see temp_client_can at the end of st_server_base class for more details.
//#define REUSE_CLIENT
#ifndef REUSE_CLIENT
#define do_create_client create_client
	#ifndef CLIENT_FREE_INTERVAL
	#define CLIENT_FREE_INTERVAL	10 //seconds, validate only REUSE_CLIENT not defined
	#endif
#endif

//define this to have st_server_base invoke clear_all_closed_socket() automatically and periodically
//this feature may serious influence server performance with huge number of clients
//so, re-write st_tcp_socket::on_recv_error and invoke st_server_base::del_client() is recommended
//in long connection system
//in short connection system, you are recommended to open this feature, use CLEAR_CLOSED_SOCKET_INTERVAL
//to set the interval
//#define AUTO_CLEAR_CLOSED_SOCKET
#ifdef AUTO_CLEAR_CLOSED_SOCKET
	#ifndef CLEAR_CLOSED_SOCKET_INTERVAL
	#define CLEAR_CLOSED_SOCKET_INTERVAL	60 //seconds, validate only AUTO_CLEAR_CLOSED_SOCKET defined
	#endif
#endif

#ifndef INVALID_LINK_MAX_DURATION
	#define INVALID_LINK_MAX_DURATION	5 //seconds
	//after this duration, the corresponding client can be freed from the heap or reused again
#endif

//in set_server_addr, if the ip is empty, TCP_DEFAULT_IP_VERSION will define the ip version,
//or, the ip version will be deduced by the ip address.
//tcp::v4() means ipv4 and tcp::v6() means ipv6.
#ifndef TCP_DEFAULT_IP_VERSION
#define TCP_DEFAULT_IP_VERSION tcp::v4()
#endif

namespace st_asio_wrapper
{

template<typename Socket = st_server_socket, typename Server = i_server>
class st_server_base : public st_service_pump::i_service, public Server, public st_timer
{
protected:
	struct temp_client
	{
		const time_t closed_time;
		const boost::shared_ptr<Socket> client_ptr;

		temp_client(const boost::shared_ptr<Socket>& _client_ptr) :
			closed_time(time(nullptr)), client_ptr(_client_ptr) {}

		bool is_timeout(time_t t) const {return closed_time <= t;}
	};

public:
	st_server_base(st_service_pump& service_pump_) : i_service(service_pump_), st_timer(service_pump_),
		acceptor(service_pump_) {set_server_addr(SERVER_PORT);}

	void set_server_addr(unsigned short port, const std::string& ip = std::string())
	{
		if (ip.empty())
			server_addr = tcp::endpoint(TCP_DEFAULT_IP_VERSION, port);
		else
		{
			error_code ec;
			server_addr = tcp::endpoint(address::from_string(ip, ec), port); assert(!ec);
		}
	}

	virtual void init()
	{
		error_code ec;
		acceptor.open(server_addr.protocol(), ec); assert(!ec);
#ifndef NOT_REUSE_ADDRESS
		acceptor.set_option(tcp::acceptor::reuse_address(true), ec); assert(!ec);
#endif
		acceptor.bind(server_addr, ec); assert(!ec);
		if (ec) {service_pump.stop(); unified_out::error_out("bind failed."); return;}
		acceptor.listen(socket_base::max_connections, ec); assert(!ec);
		if (ec) {service_pump.stop(); unified_out::error_out("listen failed."); return;}

#ifndef REUSE_CLIENT
		set_timer(0, 1000 * CLIENT_FREE_INTERVAL, nullptr);
#endif
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		set_timer(1, 1000 * CLEAR_CLOSED_SOCKET_INTERVAL, nullptr);
#endif
		for (auto i = 0; i < ASYNC_ACCEPT_NUM; ++i)
			start_next_accept();
	}
	virtual void uninit() {stop_listen(); close_all_client(); stop_all_timer();}

	void stop_listen() {error_code ec; acceptor.cancel(ec); acceptor.close(ec);}
	bool is_listening() const {return acceptor.is_open();}

	virtual st_service_pump& get_service_pump() {return service_pump;}
	virtual const st_service_pump& get_service_pump() const {return service_pump;}
	virtual void del_client(const boost::shared_ptr<st_tcp_socket>& client_ptr)
	{
		auto found = false;

		mutex::scoped_lock lock(client_can_mutex);
		//client_can does not contain any duplicate items
		auto iter = std::find(std::begin(client_can), std::end(client_can), client_ptr);
		if (iter != std::end(client_can))
		{
			found = true;
			client_can.erase(iter);
		}
		lock.unlock();

		if (found)
		{
			client_ptr->show_info("client:", "quit.");
			client_ptr->force_close();
			client_ptr->direct_dispatch_all_msg();

			mutex::scoped_lock lock(temp_client_can_mutex);
			temp_client_can.push_back(dynamic_pointer_cast<Socket>(client_ptr));
		}
	}

	//Clear all closed socket from client list
	//Consider the following conditions:
	//1.You don't invoke del_client in on_recv_error and on_send_error,
	// or close the st_tcp_socket in on_unpack_error
	//2.For some reason(I haven't met yet), on_recv_error, on_send_error and on_unpack_error
	// not been invoked
	//st_server_base will automatically invoke this if AUTO_CLEAR_CLOSED_SOCKET been defined
	void clear_all_closed_socket(container::list<boost::shared_ptr<Socket>>& clients)
	{
		mutex::scoped_lock lock(client_can_mutex);
		for (auto iter = std::begin(client_can); iter != std::end(client_can);)
			if (!(*iter)->is_open())
			{
				(*iter)->direct_dispatch_all_msg();
				clients.resize(clients.size() + 1);
				clients.back().swap(*iter);
				iter = client_can.erase(iter);
			}
			else
				++iter;
	}

	size_t size()
	{
		mutex::scoped_lock lock(client_can_mutex);
		return client_can.size();
	}

	size_t closed_client_size()
	{
		mutex::scoped_lock lock(temp_client_can_mutex);
		return temp_client_can.size();
	}

	//free a specific number of client objects
	//if you use client pool(define REUSE_CLIENT), you may need to free some client objects
	//when the client pool(get_closed_client_size()) goes big enough for memory saving(because
	//the clients in temp_client_can are waiting for reuse and will never be freed)
	//if you don't use client pool, st_server_base will invoke this automatically and periodically
	//so, you don't need invoke this exactly
	void free_client(size_t num = -1)
	{
		if (0 == num)
			return;

		auto now = time(nullptr) - INVALID_LINK_MAX_DURATION;
		mutex::scoped_lock lock(temp_client_can_mutex);
		for (auto iter = std::begin(temp_client_can); num > 0 && iter != std::end(temp_client_can);)
			if (iter->closed_time <= now)
			{
				iter = temp_client_can.erase(iter);
				--num;
			}
			else
				++iter;
	}

	void close_all_client()
	{
		//do not use graceful_close() as client endpoint do,
		//because in this function, client_can_mutex has been locked,
		//graceful_close will wait until on_recv_error() been invoked,
		//in on_recv_error(), we need to lock client_can_mutex too(in del_client()), which made dead lock
		do_something_to_all([this](decltype(*std::begin(client_can))& item) {
			item->show_info("client:", "been closed.");
			item->force_close();
			item->direct_dispatch_all_msg();
		});
	}

	void list_all_client() {do_something_to_all(boost::bind(&Socket::show_info, _1, "", ""));}

	DO_SOMETHING_TO_ALL_MUTEX(client_can, client_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(client_can, client_can_mutex)

	//Empty ip means don't care, any ip will match
	//Zero port means don't care, any port will match
	void find_client(const std::string& ip, unsigned short port, container::list<boost::shared_ptr<Socket>>& clients)
	{
		if (ip.empty() && 0 == port)
		{
			mutex::scoped_lock lock(client_can_mutex);
			clients.insert(std::end(clients), std::begin(client_can), std::end(client_can));
		}
		else
			do_something_to_all([&](decltype(*std::begin(client_can))& item) {
				if (item->is_open())
				{
					auto ep = item->remote_endpoint();
					if ((0 == port || port == ep.port()) && (ip.empty() || ip == ep.address().to_string()))
						clients.push_back(item);
				}
			});
	}

	///////////////////////////////////////////////////
	//msg sending interface
	TCP_BROADCAST_MSG(broadcast_msg, send_msg)
	TCP_BROADCAST_MSG(broadcast_native_msg, send_native_msg)
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into st_tcp_socket's send buffer
	TCP_BROADCAST_MSG(safe_broadcast_msg, safe_send_msg)
	TCP_BROADCAST_MSG(safe_broadcast_native_msg, safe_send_native_msg)
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	virtual bool on_accept(const boost::shared_ptr<Socket>& client_ptr) {return true;}

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
		case 0:
#ifndef REUSE_CLIENT
			free_client();
			return true;
#endif
			break;
		case 1:
#ifdef AUTO_CLEAR_CLOSED_SOCKET
			{
				decltype(client_can) clients;
				clear_all_closed_socket(clients);
				if (!clients.empty())
				{
					mutex::scoped_lock lock(temp_client_can_mutex);
					temp_client_can.insert(std::end(temp_client_can), std::begin(clients), std::end(clients));
				}
				return true;
			}
#endif
			break;
		case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

protected:
	boost::shared_ptr<Socket> create_client() {return boost::make_shared<Socket>(boost::ref(*this));}

	void start_next_accept()
	{
		auto client_ptr = do_create_client();
		acceptor.async_accept(*client_ptr, boost::bind(&st_server_base::accept_handler, this,
			placeholders::error, client_ptr));
	}

	bool add_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		mutex::scoped_lock lock(client_can_mutex);
		auto client_num = client_can.size();
		if (client_num < MAX_CLIENT_NUM)
			client_can.push_back(client_ptr);
		lock.unlock();

		if (client_num < MAX_CLIENT_NUM)
		{
			client_ptr->show_info("client:", "arrive.");
			return true;
		}

		return false;
	}

	void accept_handler(const error_code& ec, const boost::shared_ptr<Socket>& client_ptr)
	{
		if (!ec)
		{
			if (on_accept(client_ptr) && add_client(client_ptr))
				client_ptr->start();
			start_next_accept();
		}
		else
			stop_listen();
	}

#ifdef REUSE_CLIENT
	boost::shared_ptr<Socket> do_create_client()
	{
		auto client_ptr = reuse_socket();
		return client_ptr ? client_ptr : create_client();
	}

	boost::shared_ptr<Socket> reuse_socket()
	{
		auto now = time(nullptr) - INVALID_LINK_MAX_DURATION;
		mutex::scoped_lock lock(temp_client_can_mutex);
		//temp_client_can does not contain any duplicate items
		auto iter = std::find_if(std::begin(temp_client_can), std::end(temp_client_can),
			std::bind2nd(std::mem_fun_ref(&temp_client::is_timeout), now));
		if (iter != std::end(temp_client_can))
		{
			auto client_ptr(std::move(iter->client_ptr));
			temp_client_can.erase(iter);
			lock.unlock();

			client_ptr->reuse();
			return client_ptr;
		}

		return boost::shared_ptr<Socket>();
	}
#endif

protected:
	tcp::endpoint server_addr;
	tcp::acceptor acceptor;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<Socket>> client_can;
	mutex client_can_mutex;

	//because all clients are dynamic created and stored in client_can, maybe when the recv error occur
	//(at this point, your standard practice is deleting the client from client_can), some other
	//asynchronous calls are still queued in boost::asio::io_service, and will be dequeued in the future,
	//we must guarantee these clients not be freed from the heap, so, we move these clients from
	//client_can to temp_client_can, and free them from the heap in the near future(controlled by the
	//0(id) timer)
	//if AUTO_CLEAR_CLOSED_SOCKET been defined, clear_all_closed_socket() will be invoked automatically
	//and periodically, and move all closed clients to temp_client_can.
	container::list<temp_client> temp_client_can;
	mutex temp_client_can_mutex;
};
typedef st_server_base<> st_server;

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_H_ */

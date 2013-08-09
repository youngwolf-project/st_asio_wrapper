/*
 * st_asio_wrapper_object_pool.h
 *
 *  Created on: 2013-8-7
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint, and in both tcp and udp socket
 */

#ifndef ST_ASIO_WRAPPER_OBJECT_POOL_H_
#define ST_ASIO_WRAPPER_OBJECT_POOL_H_

#include <boost/container/list.hpp>

#include "st_asio_wrapper_timer.h"
#include "st_asio_wrapper_service_pump.h"

#ifndef MAX_OBJECT_NUM
#define MAX_OBJECT_NUM				4096
#endif

//something like memory pool, if you open REUSE_OBJECT, all clients in temp_client_can will never be freed,
//but waiting for reuse
//or, st_server_base will free the clients in temp_client_can automatically and periodically,
//use CLIENT_FREE_INTERVAL to set the interval,
//see temp_client_can at the end of st_server_base class for more details.
#ifndef REUSE_OBJECT
	#ifndef CLIENT_FREE_INTERVAL
	#define CLIENT_FREE_INTERVAL	10 //seconds, validate only REUSE_OBJECT not defined
	#endif
#endif

//define this to have st_server_base invoke clear_all_closed_socket() automatically and periodically
//this feature may serious influence server performance with huge number of clients
//so, re-write st_tcp_socket::on_recv_error and invoke st_server_base::del_client() is recommended
//in long connection system
//in short connection system, you are recommended to open this feature, use CLEAR_CLOSED_SOCKET_INTERVAL
//to set the interval
#ifdef AUTO_CLEAR_CLOSED_SOCKET
	#ifndef CLEAR_CLOSED_SOCKET_INTERVAL
	#define CLEAR_CLOSED_SOCKET_INTERVAL	60 //seconds, validate only AUTO_CLEAR_CLOSED_SOCKET defined
	#endif
#endif

#ifndef INVALID_LINK_MAX_DURATION
	#define INVALID_LINK_MAX_DURATION	5 //seconds
	//after this duration, the corresponding client can be freed from the heap or reused again
#endif

namespace st_asio_wrapper
{

template<typename Socket>
class st_object_pool: public st_service_pump::i_service, public st_timer
{
protected:
	struct temp_client
	{
		const time_t closed_time;
		const boost::shared_ptr<Socket> client_ptr;

		temp_client(const boost::shared_ptr<Socket>& _client_ptr) :
			closed_time(time(NULL)), client_ptr(_client_ptr) {}

		bool is_timeout(time_t t) const {return closed_time <= t;}
	};

public:
	st_object_pool(st_service_pump& service_pump_) : i_service(service_pump_), st_timer(service_pump_) {}

	void start()
	{
#ifndef REUSE_OBJECT
		set_timer(0, 1000 * CLIENT_FREE_INTERVAL, NULL);
#endif
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		set_timer(1, 1000 * CLEAR_CLOSED_SOCKET_INTERVAL, NULL);
#endif
	}

	void stop() {stop_all_timer();}

	bool add_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		assert(client_ptr && &client_ptr->get_io_service() == &get_service_pump());
		mutex::scoped_lock lock(client_can_mutex);
		size_t client_num = client_can.size();
		if (client_num < MAX_OBJECT_NUM)
			client_can.push_back(client_ptr);
		lock.unlock();

		return client_num < MAX_OBJECT_NUM;
	}

	bool del_client(const boost::shared_ptr<Socket>& client_ptr)
	{
		bool found = false;

		mutex::scoped_lock lock(client_can_mutex);
		//client_can does not contain any duplicate items
		BOOST_AUTO(iter, std::find(client_can.begin(), client_can.end(), client_ptr));
		if (iter != client_can.end())
		{
			found = true;
			client_can.erase(iter);
		}
		lock.unlock();

		if (found)
		{
			mutex::scoped_lock lock(temp_client_can_mutex);
			temp_client_can.push_back(client_ptr);
		}

		return found;
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

	boost::shared_ptr<Socket> at(size_t index)
	{
		mutex::scoped_lock lock(client_can_mutex);
		assert(index < client_can.size());
		return index < client_can.size() ?
			*(boost::next(client_can.begin(), index)) : boost::shared_ptr<Socket>();
	}

	void list_all_client() {do_something_to_all(boost::bind(&Socket::show_info, _1, "", ""));}

	//Empty ip means don't care, any ip will match
	//Zero port means don't care, any port will match
	//this function only used with st_tcp_socket, because for st_udp_socket, remote endpoint means nothing.
	void find_client(const std::string& ip, unsigned short port, container::list<boost::shared_ptr<Socket> >& clients)
	{
		mutex::scoped_lock lock(client_can_mutex);
		if (ip.empty() && 0 == port)
			clients.insert(clients.end(), client_can.begin(), client_can.end());
		else
			for (BOOST_AUTO(iter, client_can.begin()); iter != client_can.end(); ++iter)
				if ((*iter)->is_open())
				{
					tcp::endpoint ep = (*iter)->remote_endpoint();
					if ((0 == port || port == ep.port()) && (ip.empty() || ip == ep.address().to_string()))
						clients.push_back((*iter));
				}
	}

	//Clear all closed socket from client list
	//Consider the following conditions:
	//1.You don't invoke del_client in on_recv_error and on_send_error,
	// or close the st_tcp_socket in on_unpack_error
	//2.For some reason(I haven't met yet), on_recv_error, on_send_error and on_unpack_error
	// not been invoked
	//st_server_base will automatically invoke this function if AUTO_CLEAR_CLOSED_SOCKET been defined
	void clear_all_closed_socket(container::list<boost::shared_ptr<Socket> >& clients)
	{
		mutex::scoped_lock lock(client_can_mutex);
		for (BOOST_AUTO(iter, client_can.begin()); iter != client_can.end();)
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

	//free a specific number of client objects
	//if you use client pool(define REUSE_OBJECT), you may need to free some client objects
	//when the client pool(get_closed_client_size()) goes big enough for memory saving(because
	//the clients in temp_client_can are waiting for reuse and will never be freed)
	//if you don't use client pool, st_server_base will invoke this automatically and periodically
	//so, you don't need invoke this exactly
	void free_client(size_t num = -1)
	{
		if (0 == num)
			return;

		time_t now = time(NULL) - INVALID_LINK_MAX_DURATION;
		mutex::scoped_lock lock(temp_client_can_mutex);
		for (BOOST_AUTO(iter, temp_client_can.begin()); num > 0 && iter != temp_client_can.end();)
			if (iter->closed_time <= now)
			{
				iter = temp_client_can.erase(iter);
				--num;
			}
			else
				++iter;
	}

	DO_SOMETHING_TO_ALL_MUTEX(client_can, client_can_mutex)
	DO_SOMETHING_TO_ONE_MUTEX(client_can, client_can_mutex)

protected:
	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch(id)
		{
#ifndef REUSE_OBJECT
		case 0:
			free_client();
			return true;
			break;
#endif
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		case 1:
			{
				BOOST_AUTO(clients, client_can);
				clear_all_closed_socket(clients);
				if (!clients.empty())
				{
					mutex::scoped_lock lock(temp_client_can_mutex);
					temp_client_can.insert(temp_client_can.end(), clients.begin(), clients.end());
				}
				return true;
			}
			break;
#endif
		case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

	boost::shared_ptr<Socket> reuse_object()
	{
#ifdef REUSE_OBJECT
		time_t now = time(NULL) - INVALID_LINK_MAX_DURATION;
		mutex::scoped_lock lock(temp_client_can_mutex);
		//temp_client_can does not contain any duplicate items
		BOOST_AUTO(iter, std::find_if(temp_client_can.begin(), temp_client_can.end(),
			std::bind2nd(std::mem_fun_ref(&temp_client::is_timeout), now)));
		if (iter != temp_client_can.end())
		{
			BOOST_AUTO(client_ptr, iter->client_ptr);
			temp_client_can.erase(iter);
			lock.unlock();

			client_ptr->reset();
			return client_ptr;
		}
#endif

		return boost::shared_ptr<Socket>();
	}

protected:
	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<boost::shared_ptr<Socket> > client_can;
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

} //namespace

#endif /* ST_ASIO_WRAPPER_OBJECT_POOL_H_ */

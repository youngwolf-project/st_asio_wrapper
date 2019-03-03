#ifndef _CLIENT_H_
#define _CLIENT_H_

//demonstrates how to call multi_client_base in client_socket_base (just like server_socket_base call server_base)
class my_matrix : public i_matrix
{
public:
	virtual bool del_link(const std::string& name) = 0;
};

typedef client_socket_base<ST_ASIO_DEFAULT_PACKER, ST_ASIO_DEFAULT_UNPACKER, my_matrix> my_client_socket_base;
class my_client_socket : public my_client_socket_base
{
public:
	my_client_socket(my_matrix& matrix_) : my_client_socket_base(matrix_)
	{
#if 3 == PACKER_UNPACKER_TYPE
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_PACKER>(packer())->prefix_suffix("", "\n");
		boost::dynamic_pointer_cast<ST_ASIO_DEFAULT_UNPACKER>(unpacker())->prefix_suffix("", "\n");
#endif
	}

	void name(const std::string& name_) {_name = name_;}
	const std::string& name() const {return _name;}

protected:
	//msg handling
#if 2 == PACKER_UNPACKER_TYPE
	//fixed_length_unpacker uses basic_buffer as its message type, it doesn't append additional \0 to the end of the message as std::string does,
	//so it cannot be printed by printf.
	virtual bool on_msg_handle(out_msg_type& msg) {printf("received: " ST_ASIO_SF ", I'm %s\n", msg.size(), _name.data()); return true;}
#else
	virtual bool on_msg_handle(out_msg_type& msg) {printf("received: %s, I'm %s\n", msg.data(), _name.data()); return true;}
#endif
	//msg handling end

	virtual void on_recv_error(const boost::system::error_code& ec) {get_matrix()->del_link(_name); my_client_socket_base::on_recv_error(ec);}

private:
	std::string _name;
};

typedef multi_client_base<my_client_socket, object_pool<my_client_socket>, my_matrix> my_client_base;
class my_client : public my_client_base
{
public:
	my_client(service_pump& service_pump_) : my_client_base(service_pump_) {}

	bool add_link(const std::string& name)
	{
		boost::lock_guard<boost::mutex> lock(link_map_mutex);
		if (link_map.count(name) > 0)
			printf("%s already exists.\n", name.data());
		else
		{
			BOOST_AUTO(socket_ptr, create_object());
			assert(socket_ptr);

			socket_ptr->name(name);
			//socket_ptr->set_server_addr(9527, "127.0.0.1"); //if you want to set server ip, do it at here like this

			if (add_socket(socket_ptr)) //exceed ST_ASIO_MAX_OBJECT_NUM
			{
				printf("add socket %s.\n", name.data());
				link_map[name] = socket_ptr->id();

				return true;
			}
		}

		return false;
	}

	boost::uint_fast64_t find_link(const std::string& name)
	{
		boost::lock_guard<boost::mutex> lock(link_map_mutex);
		BOOST_AUTO(iter, link_map.find(name));
		return iter != link_map.end() ? iter->second : -1;
	}

	boost::uint_fast64_t find_and_remove_link(const std::string& name)
	{
		boost::uint_fast64_t id = -1;

		boost::lock_guard<boost::mutex> lock(link_map_mutex);
		BOOST_AUTO(iter, link_map.find(name));
		if (iter != link_map.end())
		{
			id = iter->second;
			link_map.erase(iter);
		}

		return id;
	}

	bool shutdown_link(const std::string& name)
	{
		BOOST_AUTO(socket_ptr, find(find_and_remove_link(name)));
		return socket_ptr ? (socket_ptr->force_shutdown(), true) : false;
	}

	bool send_msg(const std::string& name, const std::string& msg) {std::string unused(msg); return send_msg(name, unused);}
	bool send_msg(const std::string& name, std::string& msg)
	{
		BOOST_AUTO(socket_ptr, find(find_link(name)));
		return socket_ptr ?  socket_ptr->send_msg(msg) : false;
	}

protected:
	//from my_matrix, pure virtual function, we must implement it.
	virtual bool del_link(const std::string& name)
	{
		boost::lock_guard<boost::mutex> lock(link_map_mutex);
		return link_map.erase(name) > 0;
	}

private:
	std::map<std::string, boost::uint_fast64_t> link_map;
	boost::mutex link_map_mutex;
};

#endif //#define _CLIENT_H_

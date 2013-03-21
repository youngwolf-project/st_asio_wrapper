/*
 * st_asio_wrapper_socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class used at both client and server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SOCKET_H_
#define ST_ASIO_WRAPPER_SOCKET_H_

#include "st_asio_wrapper_packer.h"
#include "st_asio_wrapper_unpacker.h"
#include "st_asio_wrapper_timer.h"

using namespace boost::asio::ip;

//#define FORCE_TO_USE_MSG_RECV_BUFFER
//use msg recv buffer all the time, you can gain some performance improvement because st_socket will not
//invoke on_msg() to decide whether to use msg recv buffer or not, but directly push the msgs into
//msg recv buffer. notice: there's no on_msg() virtual function any more.
//this is a compile time optimization

#if defined _MSC_VER
#define size_t_format "%Iu"
#else // defined __GNUC__
#define size_t_format "%tu"
#endif

///////////////////////////////////////////////////
//msg sending interface
#define SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) \
	{return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) \
	{return FUNNAME(str.data(), str.size(), can_overflow);}

#define SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	mutex::scoped_lock lock(send_msg_buffer_mutex); \
	if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) \
	{ \
		msg_type msg = packer_->pack_msg(pstr, len, num, NATIVE); \
		return direct_insert_msg(msg); \
	} \
	return false; \
} \
SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//msg sending interface
///////////////////////////////////////////////////

namespace st_asio_wrapper
{

class st_socket : public tcp::socket, public st_timer
{
public:
	typedef std::string msg_type;
	typedef const msg_type msg_ctype;

public:
	st_socket(io_service& io_service_) : tcp::socket(io_service_), st_timer(io_service_), sending(false), dispatching(false),
		packer_(boost::make_shared<packer>()), unpacker_(boost::make_shared<unpacker>()), closing(false) {}

	virtual void start() = 0;
	//reset all, be ensure that there's no any operations performed on this st_socket when invoke it
	void reset() {reset_state(); clear_buffer();}
	void reset_state()
	{
		reset_unpacker_state();
		sending = false;
		dispatching = false;
		closing = false;
	}
	void clear_buffer()
	{
		send_msg_buffer.clear();
		recv_msg_buffer.clear();
		temp_msg_buffer.clear();
	}

	void disconnect() {force_close();}
	void force_close() {clean_up();}
	void graceful_close() //will block until closing successfully or timeout
	{
		closing = true;

		error_code ec;
		shutdown(tcp::socket::shutdown_send, ec);
		if (ec) //graceful disconnecting is not possible
			clean_up();
		else
		{
			int loop_num = 0;
			while (++loop_num <= 100 && closing) //5 seconds
				this_thread::sleep(get_system_time() + posix_time::milliseconds(50));
			if (loop_num > 100) //graceful disconnecting is not possible
				clean_up();
			else
				closing = false; //restore state for next use
		}
	}
	bool is_closing() {return closing;}

	//get or change the packer and unpacker at runtime
	boost::shared_ptr<i_packer> inner_packer() const {return packer_;}
	void inner_packer(const boost::shared_ptr<i_packer>& _packer_) {packer_ = _packer_;};
	boost::shared_ptr<i_unpacker> inner_unpacker() const {return unpacker_;}
	void inner_unpacker(const boost::shared_ptr<i_unpacker>& _unpacker_) {unpacker_ = _unpacker_;}

	///////////////////////////////////////////////////
	//msg sending interface
	SEND_MSG(send_msg, false); //use the packer with native = false to pack the msgs
	SEND_MSG(send_native_msg, true); //use the packer with native = true to pack the msgs
	//msg sending interface
	///////////////////////////////////////////////////

	//don't use the packer but insert into the send_msg_buffer directly
	bool direct_send_msg(msg_ctype& msg, bool can_overflow = false)
	{
		msg_type tmp_msg(msg);
		return direct_send_msg(tmp_msg, can_overflow);
	}

	//after this call, msg becomes empty, please note.
	bool direct_send_msg(msg_type& msg, bool can_overflow = false)
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM)
			return direct_insert_msg(msg);

		return false;
	}

	//send buffered msgs, return false if send buffer is empty or invalidate status
	bool send_msg()
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return do_send_msg();
	}

	//generally used after the service stopped
	void direct_dispatch_all_msg()
	{
		//mutex::scoped_lock lock(recv_msg_buffer_mutex);
		if (!recv_msg_buffer.empty() || !temp_msg_buffer.empty())
		{
			recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer);
			st_asio_wrapper::do_something_to_all(recv_msg_buffer, boost::bind(&st_socket::on_msg_handle, this, _1));
			recv_msg_buffer.clear();
		}

		dispatching = false;
	}

	//how many msgs waiting for send
	size_t get_pending_msg_num()
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		return send_msg_buffer.size();
	}

	//the msg's format please refer to on_msg_send
	void peek_first_pending_msg(msg_type& msg)
	{
		msg.clear();
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (!send_msg_buffer.empty())
			msg = send_msg_buffer.front();
	}

	//the msg's format please refer to on_msg_send
	void pop_first_pending_msg(msg_type& msg)
	{
		msg.clear();
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		if (!send_msg_buffer.empty())
		{
			msg.swap(send_msg_buffer.front());
			send_msg_buffer.pop_front();
		}
	}

	//clear all pending msgs
	//the msg's format please refer to on_msg_send
	void pop_all_pending_msg(container::list<msg_type>& unsend_msg_list)
	{
		mutex::scoped_lock lock(send_msg_buffer_mutex);
		unsend_msg_list.splice(unsend_msg_list.end(), send_msg_buffer);
	}

	void show_info(const char* head, const char* tail)
	{
		error_code ec;
		BOOST_AUTO(ep, remote_endpoint(ec));
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

protected:
	virtual bool is_send_allowed() {return !get_io_service().stopped();} //can send data or not
	//this function can not be const only because get_io_service() does not have const version
	//it's a small flaw of asio

	//msg can not be unpacked
	//the link can continue to use, but need not close the st_socket at both client and server endpoint
	virtual void on_unpack_error() = 0;

	//recv error or peer endpoint quit(false ec means ok)
	virtual void on_recv_error(const error_code& ec) = 0;

	//generally, you need not re-write this for link broken judgment
	virtual void on_send_error(const error_code& ec) {}

#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//if you want to use your own recv buffer, you can move the msg to your own recv buffer,
	//and return false, then, handle the msg as your own strategy(may be you'll need a msg dispatch thread)
	//or, you can handle the msg at here and return false, but this will reduce efficiency(
	//because this msg handling block the next msg receiving on the same st_socket) unless you can
	//handle the msg very fast(which will inversely more efficient, because msg recv buffer and msg dispatching
	//are not needed any more).
	//
	//return true means use the msg recv buffer, you must handle the msgs in on_msg_handle()
	//notice: on_msg_handle() will not be invoked from within this function
	//
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual bool on_msg(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data()); return false;}
#endif

	//handling msg at here will not block msg receiving
	//if on_msg() return false, this function will not be invoked due to no msgs need to dispatch
	//notice: the msg is unpacked, using inconstant is for the convenience of swapping
	virtual void on_msg_handle(msg_type& msg)
		{unified_out::debug_out("recv(" size_t_format "): %s", msg.size(), msg.data());}

#ifdef WANT_MSG_SEND_NOTIFY
	//one msg has sent to the kernel buffer, msg is the right msg(remain in packed)
	//if the msg is custom packed, then obviously you know it
	//or the msg is packed as: len(2 bytes) + original msg, see st_asio_wrapper::packer for more details
	virtual void on_msg_send(msg_type& msg) {}
#endif
#ifdef WANT_ALL_MSG_SEND_NOTIFY
	//send buffer goes empty, msg remain in packed
	virtual void on_all_msg_send(msg_type& msg) {}
#endif

	virtual bool on_timer(unsigned char id, const void* user_data)
	{
		switch (id)
		{
		case 0: //delay dispatch msgs because of recv buffer overflow
			if (dispatch_msg())
				start(); //recv msg sequentially, that means second recv only after first recv success
			else
				return true;
			break;
		case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: //reserved
			break;
		default:
			return st_timer::on_timer(id, user_data);
			break;
		}

		return false;
	}

	//start the async read
	//it's child's responsibility to invoke this properly,
	//because st_socket doesn't know any of the connection status
	void do_recv_msg()
	{
		size_t min_recv_len;
		BOOST_AUTO(recv_buff, unpacker_->prepare_next_recv(min_recv_len));
		if (buffer_size(recv_buff) > 0)
			async_read(*this, recv_buff, transfer_at_least(min_recv_len), boost::bind(&st_socket::recv_handler, this,
				placeholders::error, placeholders::bytes_transferred));
	}

	//reset unpacker's state, generally used when unpack error occur
	void reset_unpacker_state() {unpacker_->reset_unpacker_state();}

	void clean_up()
	{
		error_code ec;
		shutdown(tcp::socket::shutdown_both, ec);
		close(ec);

		reset_unpacker_state();
		stop_all_timer();
		sending = false;
		closing = false;
	}

	bool dispatch_msg()
	{
		if (temp_msg_buffer.empty())
			return true;

		bool dispatch = false;
		mutex::scoped_lock lock(recv_msg_buffer_mutex);
		size_t msg_num = recv_msg_buffer.size();
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER //inefficient
		for (BOOST_AUTO(iter, temp_msg_buffer.begin()); iter != temp_msg_buffer.end();)
			if (!on_msg(*iter))
				temp_msg_buffer.erase(iter++);
			else if (msg_num < MAX_MSG_NUM) //msg recv buffer available
			{
				dispatch = true;
				recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer, iter++);
				++msg_num;
			}
			else
				++iter;
#else //efficient
		if (msg_num < MAX_MSG_NUM) //msg recv buffer available
		{
			dispatch = true;
			msg_num = MAX_MSG_NUM - msg_num; //max msg number this time can handle
			BOOST_AUTO(begin_iter, temp_msg_buffer.begin()); BOOST_AUTO(end_iter, temp_msg_buffer.end());
			if (temp_msg_buffer.size() > msg_num) //some msgs left behind
			{
				size_t left_num = temp_msg_buffer.size() - msg_num;
				if (left_num > msg_num) //find the minimum movement
					std::advance(end_iter = begin_iter, msg_num);
				else
					std::advance(end_iter, -(container::list<msg_type>::iterator::difference_type) left_num);
			}
			else
				msg_num = temp_msg_buffer.size();
			//use msg_num to avoid std::distance() call, so, msg_num must correct
			recv_msg_buffer.splice(recv_msg_buffer.end(), temp_msg_buffer, begin_iter, end_iter, msg_num);
		}
#endif

		if (dispatch)
			do_dispatch_msg();
		lock.unlock();

		return temp_msg_buffer.empty();
	}

	void recv_handler(const error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			bool unpack_ok = unpacker_->on_recv(bytes_transferred, temp_msg_buffer);
			bool all_dispatched = dispatch_msg();

			if (unpack_ok)
			{
				if (all_dispatched)
					start(); //recv msg sequentially, that means second recv only after first recv success
				else
					set_timer(0, 50, NULL);
			}
			else
				on_unpack_error();
		}
		else
			on_recv_error(ec);
	}

	void send_handler(const error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			assert(bytes_transferred > 0);
#ifdef WANT_MSG_SEND_NOTIFY
			on_msg_send(last_send_msg);
#endif
			mutex::scoped_lock lock(send_msg_buffer_mutex);
			sending = false;
			//send msg sequentially, that means second send only after first send success
			if (!do_send_msg())
#ifdef WANT_ALL_MSG_SEND_NOTIFY
				on_all_msg_send(last_send_msg)
#endif
				;
		}
		else
			on_send_error(ec);
	}

	void msg_handler()
	{
		on_msg_handle(last_dispatch_msg); //must before next msg dispatch to keep sequence
		mutex::scoped_lock lock(recv_msg_buffer_mutex);
		dispatching = false;
		//dispatch msg sequentially, that means second dispatch only after first dispatch success
		do_dispatch_msg();
	}

	//must mutex send_msg_buffer before invoke this function
	bool do_send_msg()
	{
		bool state = !closing && is_send_allowed() && is_open();
		if (!state)
			sending = false;
		else if (!sending && !send_msg_buffer.empty())
		{
			sending = true;
			last_send_msg.swap(send_msg_buffer.front());
			async_write(*this, buffer(last_send_msg), boost::bind(&st_socket::send_handler, this,
				placeholders::error, placeholders::bytes_transferred));
			send_msg_buffer.pop_front();
		}

		return sending;
	}

	//must mutex recv_msg_buffer before invoke this function
	void do_dispatch_msg()
	{
		io_service& io_service_ = get_io_service();
		bool state = !io_service_.stopped();
		if (!state)
			dispatching = false;
		else if (!dispatching && !recv_msg_buffer.empty())
		{
			dispatching = true;
			last_dispatch_msg.swap(recv_msg_buffer.front());
			io_service_.post(boost::bind(&st_socket::msg_handler, this));
			recv_msg_buffer.pop_front();
		}
	}

	//must mutex send_msg_buffer before invoke this function
	bool direct_insert_msg(msg_type& msg)
	{
		if (!msg.empty())
		{
			send_msg_buffer.push_back(msg_type());
			send_msg_buffer.back().swap(msg);
			do_send_msg();

			return true;
		}

		return false;
	}

protected:
	msg_type last_send_msg, last_dispatch_msg;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	container::list<msg_type> send_msg_buffer;
	mutex send_msg_buffer_mutex;
	bool sending;

	//keep size() constant time would better, because we invoke it frequently, so don't use std::list(gcc)
	//using this msg recv buffer or not is decided by the return value of on_msg()
	//see on_msg() for more details
	container::list<msg_type> recv_msg_buffer;
	mutex recv_msg_buffer_mutex;
	bool dispatching;

	//if on_msg() return true, which means use the msg recv buffer,
	//st_socket will invoke dispatch_msg() when got some msgs. if the msgs can't push into recv_msg_buffer
	//because of recv buffer overflow, st_socket will delay 50 milliseconds(nonblocking) to invoke
	//dispatch_msg() again, and now, as you known, temp_msg_buffer is used to hold these msgs temporarily.
	container::list<msg_type> temp_msg_buffer;

	boost::shared_ptr<i_packer> packer_;
	boost::shared_ptr<i_unpacker> unpacker_;

	bool closing;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SOCKET_H_ */

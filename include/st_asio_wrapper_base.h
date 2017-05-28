/*
 * st_asio_wrapper_base.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this is a global head file
 */

#ifndef ST_ASIO_WRAPPER_BASE_H_
#define ST_ASIO_WRAPPER_BASE_H_

#include <stdio.h>
#include <stdarg.h>

#include <vector>
#include <string>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>
#include <boost/date_time.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/container/list.hpp>
#if BOOST_VERSION >= 105300
#include <boost/atomic.hpp>
#endif

#include "st_asio_wrapper.h"

//msg send and recv buffer's maximum size (list::size()), corresponding buffers are expanded dynamically, which means only allocate memory when needed.
#ifndef ST_ASIO_MAX_MSG_NUM
#define ST_ASIO_MAX_MSG_NUM		1024
#endif
static_assert(ST_ASIO_MAX_MSG_NUM > 0, "message capacity must be bigger than zero.");

//buffer type used when receiving messages (unpacker's prepare_next_recv() need to return this type)
#ifndef ST_ASIO_RECV_BUFFER_TYPE
#define ST_ASIO_RECV_BUFFER_TYPE boost::asio::mutable_buffers_1
#endif

#if defined _MSC_VER
#define ST_ASIO_SF "%Iu"
#else // defined __GNUC__
#define ST_ASIO_SF "%zu"
#ifdef __x86_64__
#define ST_ASIO_LLF "%lu" //format used to print 'uint_fast64_t'
#endif
#endif

#ifndef ST_ASIO_LLF
#define ST_ASIO_LLF "%llu" //format used to print 'uint_fast64_t'
#endif

namespace st_asio_wrapper
{

#if BOOST_VERSION >= 105300
typedef boost::atomic_uint_fast64_t st_atomic_uint_fast64;
typedef boost::atomic_size_t st_atomic_size_t;
#else
template <typename T>
class st_atomic
{
public:
	st_atomic() : data(0) {}
	st_atomic(T _data) : data(_data) {}

	T operator++() {boost::lock_guard<boost::mutex> lock(data_mutex); return ++data;}
	//deliberately omitted operator++(int)
	T operator+=(T value) {boost::lock_guard<boost::mutex> lock(data_mutex); return data += value;}
	T operator--() {boost::lock_guard<boost::mutex> lock(data_mutex); return --data;}
	//deliberately omitted operator--(int)
	T operator-=(T value) {boost::lock_guard<boost::mutex> lock(data_mutex); return data -= value;}
	T operator=(T value) {boost::lock_guard<boost::mutex> lock(data_mutex); return data = value;}
	T exchange(T value, boost::memory_order) {boost::lock_guard<boost::mutex> lock(data_mutex); T pre_data = data; data = value; return pre_data;}
	T fetch_add(T value, boost::memory_order) {boost::lock_guard<boost::mutex> lock(data_mutex); T pre_data = data; data += value; return pre_data;}
	void store(T value, boost::memory_order) {boost::lock_guard<boost::mutex> lock(data_mutex); data = value;}

	bool is_lock_free() const {return false;}
	operator T() const {return data;}

private:
	T data;
	boost::mutex data_mutex;
};
typedef st_atomic<uint_fast64_t> st_atomic_uint_fast64;
typedef st_atomic<size_t> st_atomic_size_t;
#endif

template<typename atomic_type = st_atomic_size_t>
class scope_atomic_lock : public boost::noncopyable
{
public:
	scope_atomic_lock(atomic_type& atomic_) : _locked(false), atomic(atomic_) {lock();} //atomic_ must has been initialized with zero
	~scope_atomic_lock() {unlock();}

	void lock() {if (!_locked) _locked = 0 == atomic.exchange(1, boost::memory_order_acq_rel);}
	void unlock() {if (_locked) atomic.store(0, boost::memory_order_release); _locked = false;}
	bool locked() const {return _locked;}

private:
	bool _locked;
	atomic_type& atomic;
};

class st_service_pump;
class st_object;
class i_server
{
public:
	virtual st_service_pump& get_service_pump() = 0;
	virtual const st_service_pump& get_service_pump() const = 0;
	virtual bool del_socket(const boost::shared_ptr<st_object>& socket_ptr) = 0;
	virtual bool restore_socket(const boost::shared_ptr<st_object>& socket_ptr, uint_fast64_t id) = 0;
};

class i_buffer
{
public:
	virtual ~i_buffer() {}

	virtual bool empty() const = 0;
	virtual size_t size() const = 0;
	virtual const char* data() const = 0;
};

//convert '->' operation to '.' operation
//user need to allocate object, and auto_buffer will free it
template<typename T>
class auto_buffer : public boost::noncopyable
{
public:
	typedef T* buffer_type;
	typedef const buffer_type buffer_ctype;

	auto_buffer() : buffer(nullptr) {}
	auto_buffer(buffer_type _buffer) : buffer(_buffer) {}
	auto_buffer(auto_buffer&& other) : buffer(other.buffer) {other.buffer = nullptr;}
	~auto_buffer() {clear();}

	auto_buffer& operator=(auto_buffer&& other) {clear(); swap(other); return *this;}

	buffer_type raw_buffer() const {return buffer;}
	void raw_buffer(buffer_type _buffer) {buffer = _buffer;}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return nullptr == buffer || buffer->empty();}
	size_t size() const {return nullptr == buffer ? 0 : buffer->size();}
	const char* data() const {return nullptr == buffer ? nullptr : buffer->data();}
	void swap(auto_buffer& other) {std::swap(buffer, other.buffer);}
	void clear() {delete buffer; buffer = nullptr;}

protected:
	buffer_type buffer;
};

//convert '->' operation to '.' operation
//user need to allocate object, and shared_buffer will free it
template<typename T>
class shared_buffer
{
public:
	typedef boost::shared_ptr<T> buffer_type;
	typedef const buffer_type buffer_ctype;

	shared_buffer() {}
	shared_buffer(T* _buffer) {buffer.reset(_buffer);}
	shared_buffer(buffer_type _buffer) : buffer(_buffer) {}
	shared_buffer(const shared_buffer& other) : buffer(other.buffer) {}
	shared_buffer(shared_buffer&& other) : buffer(std::move(other.buffer)) {}
	const shared_buffer& operator=(const shared_buffer& other) {buffer = other.buffer; return *this;}
	~shared_buffer() {clear();}

	shared_buffer& operator=(shared_buffer&& other) {clear(); swap(other); return *this;}

	buffer_type raw_buffer() const {return buffer;}
	void raw_buffer(T* _buffer) {buffer.reset(_buffer);}
	void raw_buffer(buffer_ctype _buffer) {buffer = _buffer;}

	//the following five functions are needed by st_asio_wrapper
	bool empty() const {return !buffer || buffer->empty();}
	size_t size() const {return !buffer ? 0 : buffer->size();}
	const char* data() const {return !buffer ? nullptr : buffer->data();}
	void swap(shared_buffer& other) {buffer.swap(other.buffer);}
	void clear() {buffer.reset();}

protected:
	buffer_type buffer;
};
//not like auto_buffer, shared_buffer is copyable, but auto_buffer is a bit more efficient.

#if !defined(_MSC_VER) || _MSC_VER >= 1900 //for naughty VC++, it violate the standard and even itself usually.
typedef auto_buffer<i_buffer> replaceable_buffer;
#else
typedef shared_buffer<i_buffer> replaceable_buffer; //before VC++ 14.0, auto_buffer will lead compilation error
#endif
//packer or/and unpacker used replaceable_buffer as their msg type will be replaceable.

//packer concept
template<typename MsgType>
class i_packer
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;

protected:
	virtual ~i_packer() {}

public:
	virtual void reset() {}
	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
	virtual msg_type pack_heartbeat() {return msg_type();}
	virtual char* raw_data(msg_type& msg) const {return nullptr;}
	virtual const char* raw_data(msg_ctype& msg) const {return nullptr;}
	virtual size_t raw_data_len(msg_ctype& msg) const {return 0;}

	msg_type pack_msg(const char* pstr, size_t len, bool native = false) {return pack_msg(&pstr, &len, 1, native);}
	msg_type pack_msg(const std::string& str, bool native = false) {return pack_msg(str.data(), str.size(), native);}
};
//packer concept

//just provide msg_type definition, you should not call any functions of it, but send msgs directly
template<typename MsgType>
class dummy_packer : public i_packer<MsgType>
{
public:
	using typename i_packer<MsgType>::msg_type;
	using typename i_packer<MsgType>::msg_ctype;

	virtual msg_type pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) {assert(false); return msg_type();}
};

//unpacker concept
template<typename MsgType>
class i_unpacker
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;
	typedef boost::container::list<msg_type> container_type;
	typedef ST_ASIO_RECV_BUFFER_TYPE buffer_type;

protected:
	virtual ~i_unpacker() {}

public:
	virtual void reset() = 0;
	//heartbeat must not be included in msg_can, otherwise you must handle heartbeat at where you handle normal messges.
	virtual bool parse_msg(size_t bytes_transferred, container_type& msg_can) = 0;
	virtual size_t completion_condition(const boost::system::error_code& ec, size_t bytes_transferred) = 0;
	virtual buffer_type prepare_next_recv() = 0;
};

template<typename MsgType>
class udp_msg : public MsgType
{
public:
	boost::asio::ip::udp::endpoint peer_addr;

	udp_msg() {}
	udp_msg(const boost::asio::ip::udp::endpoint& _peer_addr) : peer_addr(_peer_addr) {}
	udp_msg(const boost::asio::ip::udp::endpoint& _peer_addr, const MsgType& msg) : MsgType(msg), peer_addr(_peer_addr) {}
	udp_msg(const boost::asio::ip::udp::endpoint& _peer_addr, MsgType&& msg) : MsgType(std::move(msg)), peer_addr(_peer_addr) {}

	using MsgType::operator =;
	using MsgType::swap;
	void swap(udp_msg& other) {std::swap(peer_addr, other.peer_addr); MsgType::swap(other);}
	void swap(boost::asio::ip::udp::endpoint& addr, MsgType&& tmp_msg) {std::swap(peer_addr, addr); MsgType::swap(tmp_msg);}
};

template<typename MsgType>
class i_udp_unpacker
{
public:
	typedef MsgType msg_type;
	typedef const msg_type msg_ctype;
	typedef boost::container::list<udp_msg<msg_type>> container_type;
	typedef ST_ASIO_RECV_BUFFER_TYPE buffer_type;

protected:
	virtual ~i_udp_unpacker() {}

public:
	virtual void reset() {}
	//heartbeat must not be returned (use empty message instead), otherwise you must handle heartbeat at where you handle normal messges.
	virtual msg_type parse_msg(size_t bytes_transferred) = 0;
	virtual buffer_type prepare_next_recv() = 0;
};
//unpacker concept

struct statistic
{
#ifdef ST_ASIO_FULL_STATISTIC
	typedef boost::posix_time::ptime stat_time;
	static stat_time local_time() {return boost::date_time::microsec_clock<boost::posix_time::ptime>::local_time();}
	typedef boost::posix_time::time_duration stat_duration;
#else
	struct dummy_duration {const dummy_duration& operator +=(const dummy_duration& other) {return *this;}}; //not a real duration, just satisfy compiler(d1 += d2)
	struct dummy_time {dummy_duration operator -(const dummy_time& other) {return dummy_duration();}}; //not a real time, just satisfy compiler(t1 - t2)

	typedef dummy_time stat_time;
	static stat_time local_time() {return stat_time();}
	typedef dummy_duration stat_duration;
#endif
	statistic() : send_msg_sum(0), send_byte_sum(0), recv_msg_sum(0), recv_byte_sum(0) {}
	void reset()
	{
		send_msg_sum = send_byte_sum = 0;
		send_delay_sum = send_time_sum = pack_time_sum = stat_duration();

		recv_msg_sum = recv_byte_sum = 0;
		dispatch_dealy_sum = recv_idle_sum = stat_duration();
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
		handle_time_1_sum = stat_duration();
#endif
		handle_time_2_sum = stat_duration();
		unpack_time_sum = stat_duration();
	}

	statistic& operator +=(const struct statistic& other)
	{
		send_msg_sum += other.send_msg_sum;
		send_byte_sum += other.send_byte_sum;
		send_delay_sum += other.send_delay_sum;
		send_time_sum += other.send_time_sum;
		pack_time_sum += other.pack_time_sum;

		recv_msg_sum += other.recv_msg_sum;
		recv_byte_sum += other.recv_byte_sum;
		dispatch_dealy_sum += other.dispatch_dealy_sum;
		recv_idle_sum += other.recv_idle_sum;
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
		handle_time_1_sum += other.handle_time_1_sum;
#endif
		handle_time_2_sum += other.handle_time_2_sum;
		unpack_time_sum += other.unpack_time_sum;

		return *this;
	}

	std::string to_string() const
	{
		std::ostringstream s;
#ifdef ST_ASIO_FULL_STATISTIC
		auto tw = boost::posix_time::time_duration::num_fractional_digits();
		s << std::setfill('0') << "send corresponding statistic:\n"
			<< "message sum: " << send_msg_sum << std::endl
			<< "size in bytes: " << send_byte_sum << std::endl
			<< "send delay: " << send_delay_sum.total_seconds() << "." << std::setw(tw) << send_delay_sum.fractional_seconds() << std::setw(0) << std::endl
			<< "send duration: " << send_time_sum.total_seconds() << "." << std::setw(tw) << send_time_sum.fractional_seconds() << std::setw(0) << std::endl
			<< "pack duration: " << pack_time_sum.total_seconds() << "." << std::setw(tw) << pack_time_sum.fractional_seconds() << std::setw(0) << std::endl
			<< "\nrecv corresponding statistic:\n"
			<< "message sum: " << recv_msg_sum << std::endl
			<< "size in bytes: " << recv_byte_sum << std::endl
			<< "dispatch delay: " << dispatch_dealy_sum.total_seconds() << "." << std::setw(tw) << dispatch_dealy_sum.fractional_seconds() << std::setw(0) << std::endl
			<< "recv idle duration: " << recv_idle_sum.total_seconds() << "." << std::setw(tw) << recv_idle_sum.fractional_seconds() << std::setw(0) << std::endl
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
			<< "on_msg duration: " << handle_time_1_sum.total_seconds() << "." << std::setw(tw) << handle_time_1_sum.fractional_seconds() << std::setw(0) << std::endl
#endif
			<< "on_msg_handle duration: " << handle_time_2_sum.total_seconds() << "." << std::setw(tw) << handle_time_2_sum.fractional_seconds() << std::endl
			<< "unpack duration: " << unpack_time_sum.total_seconds() << "." << std::setw(tw) << unpack_time_sum.fractional_seconds();
#else
		s << std::setfill('0') << "send corresponding statistic:\n"
			<< "message sum: " << send_msg_sum << std::endl
			<< "size in bytes: " << send_byte_sum << std::endl
			<< "\nrecv corresponding statistic:\n"
			<< "message sum: " << recv_msg_sum << std::endl
			<< "size in bytes: " << recv_byte_sum;
#endif
		return s.str();
	}

	//send corresponding statistic
	uint_fast64_t send_msg_sum; //not counted msgs in sending buffer
	uint_fast64_t send_byte_sum; //include data added by packer, not counted msgs in sending buffer
	stat_duration send_delay_sum; //from send_(native_)msg (exclude msg packing) to boost::asio::async_write
	stat_duration send_time_sum; //from boost::asio::async_write to send_handler
	//above two items indicate your network's speed or load
	stat_duration pack_time_sum; //st_udp_socket will not gather this item

	//recv corresponding statistic
	uint_fast64_t recv_msg_sum; //msgs returned by i_unpacker::parse_msg
	uint_fast64_t recv_byte_sum; //msgs (in bytes) returned by i_unpacker::parse_msg
	stat_duration dispatch_dealy_sum; //from parse_msg(exclude msg unpacking) to on_msg_handle
	stat_duration recv_idle_sum;
	//during this duration, st_socket suspended msg reception (receiving buffer overflow, msg dispatching suspended or doing congestion control)
#ifndef ST_ASIO_FORCE_TO_USE_MSG_RECV_BUFFER
	stat_duration handle_time_1_sum; //on_msg consumed time, this indicate the efficiency of msg handling
#endif
	stat_duration handle_time_2_sum; //on_msg_handle consumed time, this indicate the efficiency of msg handling
	stat_duration unpack_time_sum; //st_udp_socket will not gather this item
};

class auto_duration
{
public:
	auto_duration(statistic::stat_duration& duration_) : started(true), begin_time(statistic::local_time()), duration(duration_) {}
	~auto_duration() {end();}

	void end() {if (started) duration += statistic::local_time() - begin_time; started = false;}

private:
	bool started;
	statistic::stat_time begin_time;
	statistic::stat_duration& duration;
};

template<typename T>
struct obj_with_begin_time : public T
{
	obj_with_begin_time() {restart();}
	obj_with_begin_time(T&& msg) : T(std::move(msg)) {restart();}
	void restart() {restart(statistic::local_time());}
	void restart(const typename statistic::stat_time& begin_time_) {begin_time = begin_time_;}
	using T::operator =;
	using T::swap;
	void swap(obj_with_begin_time& other) {T::swap(other); std::swap(begin_time, other.begin_time);}

	typename statistic::stat_time begin_time;
};

//free functions, used to do something to any container(except map and multimap) optionally with any mutex
template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred) {boost::lock_guard<boost::mutex> lock(__mutex); for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Predicate>
void do_something_to_all(_Can& __can, const _Predicate& __pred) {for (auto& item : __can) __pred(item);}

template<typename _Can, typename _Mutex, typename _Predicate>
void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
{
	boost::lock_guard<boost::mutex> lock(__mutex);
	for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;
}

template<typename _Can, typename _Predicate>
void do_something_to_one(_Can& __can, const _Predicate& __pred) {for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;}

template<typename _Can>
bool splice_helper(_Can& dest_can, _Can& src_can, size_t max_size = ST_ASIO_MAX_MSG_NUM)
{
	auto size = dest_can.size();
	if (size < max_size) //dest_can can hold more items.
	{
		size = max_size - size; //maximum items can be handled this time
		auto begin_iter = std::begin(src_can), end_iter = std::end(src_can);
		if (src_can.size() > size) //some items left behind
		{
			auto left_size = src_can.size() - size;
			end_iter = left_size > size ? std::next(begin_iter, size) : std::prev(end_iter, left_size); //find the minimum movement
		}
		else
			size = src_can.size();
		//use size to avoid std::distance() call, so, size must correct
		dest_can.splice(std::end(dest_can), src_can, begin_iter, end_iter, size);

		return size > 0;
	}

	return false;
}

//member functions, used to do something to any member container(except map and multimap) optionally with any member mutex
#define DO_SOMETHING_TO_ALL_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ALL_MUTEX_NAME(do_something_to_all, CAN, MUTEX)
#define DO_SOMETHING_TO_ALL(CAN) DO_SOMETHING_TO_ALL_NAME(do_something_to_all, CAN)

#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {boost::lock_guard<boost::mutex> lock(MUTEX); for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto& item : CAN) __pred(item);} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto& item : CAN) __pred(item);}

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{boost::lock_guard<boost::mutex> lock(MUTEX); for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

//used by both TCP and UDP
#define SAFE_SEND_MSG_CHECK \
{ \
	if (this->stopped() || !this->is_ready()) return false; \
	boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50)); \
}

#define GET_PENDING_MSG_NUM(FUNNAME, CAN) size_t FUNNAME() const {return CAN.size();}
#define POP_FIRST_PENDING_MSG(FUNNAME, CAN, MSGTYPE) void FUNNAME(MSGTYPE& msg) {msg.clear(); CAN.try_dequeue(msg);}
#define POP_ALL_PENDING_MSG(FUNNAME, CAN, CANTYPE) void FUNNAME(CANTYPE& msg_queue) {msg_queue.clear(); CAN.swap(msg_queue);}

///////////////////////////////////////////////////
//TCP msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!can_overflow && !this->is_send_buffer_available()) \
		return false; \
	auto_duration dur(this->stat.pack_time_sum); \
	auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
	dur.end(); \
	return this->do_direct_send_msg(std::move(msg)); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
size_t FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!this->sending && !this->stopped() && this->is_ready()) \
	{ \
		scope_atomic_lock<> lock(this->send_atomic); \
		if (!this->sending && lock.locked()) \
		{ \
			this->sending = true; \
			lock.unlock(); \
			auto_duration dur(this->stat.pack_time_sum); \
			auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
			dur.end(); \
			if (msg.empty()) \
				unified_out::error_out("found an empty message, please check your packer."); \
			return do_sync_send_msg(msg); /*always send message even it's empty, this is very important*/ \
		} \
	} \
	return 0; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, size_t)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into st_tcp_socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {while (!SEND_FUNNAME(pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{this->do_something_to_all([=](typename Pool::object_ctype& item) {item->SEND_FUNNAME(pstr, len, num, can_overflow);});} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, can_overflow);} \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str, can_overflow);} \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!can_overflow && !this->is_send_buffer_available()) \
		return false; \
	in_msg_type msg(peer_addr, this->packer_->pack_msg(pstr, len, num, NATIVE)); \
	return this->do_direct_send_msg(std::move(msg)); \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define UDP_SYNC_SEND_MSG(FUNNAME, NATIVE) \
size_t FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
size_t FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	if (!this->sending && !this->stopped() && this->is_ready()) \
	{ \
		scope_atomic_lock<> lock(this->send_atomic); \
		if (!this->sending && lock.locked()) \
		{ \
			this->sending = true; \
			lock.unlock(); \
			auto msg = this->packer_->pack_msg(pstr, len, num, NATIVE); \
			if (msg.empty()) \
				unified_out::error_out("found an empty message, please check your packer."); \
			return do_sync_send_msg(peer_addr, msg); /*always send message even it's empty, this is very important*/ \
		} \
	} \
	return 0; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, size_t)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into st_udp_socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false)  {return FUNNAME(peer_addr, pstr, len, num, can_overflow);} \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//UDP msg sending interface
///////////////////////////////////////////////////

#include <sstream>

#ifndef ST_ASIO_UNIFIED_OUT_BUF_NUM
#define ST_ASIO_UNIFIED_OUT_BUF_NUM	2048
#endif

class log_formater
{
public:
	static void all_out(const char* head, char* buff, size_t buff_len, const char* fmt, va_list& ap)
	{
		assert(nullptr != buff && buff_len > 0);

		std::stringstream os;
		os.rdbuf()->pubsetbuf(buff, buff_len);

		if (nullptr != head)
			os << '[' << head << "] ";

		char time_buff[64];
		auto now = time(nullptr);
#ifdef _MSC_VER
		ctime_s(time_buff, sizeof(time_buff), &now);
#else
		ctime_r(&now, time_buff);
#endif
		auto len = strlen(time_buff);
		assert(len > 0);
		if ('\n' == *std::next(time_buff, --len))
			*std::next(time_buff, len) = '\0';

		os << time_buff << " -> ";

#if defined _MSC_VER || (defined __unix__ && !defined __linux__)
		os.rdbuf()->sgetn(buff, buff_len);
#endif
		len = (size_t) os.tellp();
		if (len >= buff_len)
			*std::next(buff, buff_len - 1) = '\0';
		else
#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
			vsnprintf_s(std::next(buff, len), buff_len - len, _TRUNCATE, fmt, ap);
#else
			vsnprintf(std::next(buff, len), buff_len - len, fmt, ap);
#endif
	}
};

#define all_out_helper(head, buff, buff_len) va_list ap; va_start(ap, fmt); log_formater::all_out(head, buff, buff_len, fmt, ap); va_end(ap)
#define all_out_helper2(head) char output_buff[ST_ASIO_UNIFIED_OUT_BUF_NUM]; all_out_helper(head, output_buff, sizeof(output_buff)); puts(output_buff)

#ifndef ST_ASIO_CUSTOM_LOG
class unified_out
{
public:
#ifdef ST_ASIO_NO_UNIFIED_OUT
	static void fatal_out(const char* fmt, ...) {}
	static void error_out(const char* fmt, ...) {}
	static void warning_out(const char* fmt, ...) {}
	static void info_out(const char* fmt, ...) {}
	static void debug_out(const char* fmt, ...) {}
#else
	static void fatal_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void error_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void warning_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void info_out(const char* fmt, ...) {all_out_helper2(nullptr);}
	static void debug_out(const char* fmt, ...) {all_out_helper2(nullptr);}
#endif
};
#endif

} //namespace

#endif /* ST_ASIO_WRAPPER_BASE_H_ */

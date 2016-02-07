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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>

#include "st_asio_wrapper.h"

//the size of the buffer used when receiving msg, must equal to or larger than the biggest msg size,
//the bigger this buffer is, the more msgs can be received in one time if there are enough msgs buffered in the SOCKET.
//every unpackers have a fixed buffer with this size, every st_tcp_sockets have an unpacker, so, this size is not the bigger the better.
//if you customized the packer and unpacker, the above principle maybe not right anymore, it should depends on your implementations.
#ifndef MSG_BUFFER_SIZE
#define MSG_BUFFER_SIZE			4000
#endif
//msg send and recv buffer's maximum size (list::size()), corresponding buffers are expanded dynamicly, which means only allocate memory when needed.
#ifndef MAX_MSG_NUM
#define MAX_MSG_NUM	1024
#endif

#if defined _MSC_VER
#define size_t_format "%Iu"
#define ST_THIS //workaround to make up the BOOST_AUTO's defect under vc2008 and compiler bugs before vc2012
#define ssize_t SSIZE_T
#else // defined __GNUC__
#define size_t_format "%tu"
#define ST_THIS this->
#endif

namespace st_asio_wrapper
{
	class i_buffer
	{
	protected:
		virtual ~i_buffer() {}

	public:
		virtual bool empty() const = 0;
		virtual size_t size() const = 0;
		virtual const char* data() const = 0;
	};

	class buffer : public std::string, public i_buffer
	{
	public:
		virtual bool empty() const {return std::string::empty();}
		virtual size_t size() const {return std::string::size();}
		virtual const char* data() const {return std::string::data();}
	};

	class replaceable_buffer
	{
	public:
		replaceable_buffer() {}
		replaceable_buffer(const boost::shared_ptr<i_buffer>& _buffer) : buffer(_buffer) {}
		replaceable_buffer(replaceable_buffer&& other) : buffer(other.buffer) {other.buffer.reset();}
		replaceable_buffer(const replaceable_buffer& other) : buffer(other.buffer) {}

		boost::shared_ptr<i_buffer> raw_buffer() {return buffer;}
		boost::shared_ptr<const i_buffer> raw_buffer() const {return buffer;}
		void raw_buffer(const boost::shared_ptr<i_buffer>& _buffer) {buffer = _buffer;}

		//the following five functions are needed by st_asio_wrapper, for other functions, depends on the implementation of your packer and unpacker
		bool empty() const {return !buffer || buffer->empty();}
		size_t size() const {return buffer ? buffer->size() : 0;}
		const char* data() const {return buffer ? buffer->data() : nullptr;}
		void swap(replaceable_buffer& other) {buffer.swap(other.buffer);}
		void clear() {buffer.reset();}

	protected:
		boost::shared_ptr<i_buffer> buffer;
	};

	//this buffer is more efficient than std::string if the memory is already allocated, because the replication been saved.
	//for example, you are sending memory-mapped files.
	class inflexible_buffer
	{
	public:
		inflexible_buffer() {do_detach();}
		inflexible_buffer(inflexible_buffer&& other) {do_attach(other.buff, other.len); other.do_detach();}
		~inflexible_buffer() {detach();}

		void assign(const char* _buff, size_t _len)
		{
			assert(_len > 0 && nullptr != _buff);
			auto _buff_ = new char[_len];
			memcpy(_buff_, _buff, _len);

			attach(_buff_, _len);
		}

		void attach(char* _buff, size_t _len) {detach(); do_attach(_buff, _len);}
		void detach() {delete buff; do_detach();}

		//the following five functions (char* data() is used by inflexible_unpacker, not counted) are needed by st_asio_wrapper,
		//for other functions, depends on the implementation of your packer and unpacker.
		bool empty() const {return 0 == len || nullptr == buff;}
		size_t size() const {return len;}
		const char* data() const {return buff;}
		char* data() {return buff;}
		void swap(inflexible_buffer& other) {std::swap(buff, other.buff); std::swap(len, other.len);}
		void clear() {detach();}

	protected:
		void do_attach(char* _buff, size_t _len) {buff = _buff; len = _len;}
		void do_detach() {buff = nullptr; len = 0;}

	protected:
		char* buff;
		size_t len;
	};

	//free functions, used to do something to any container(except map and multimap) optionally with any mutex
#if !defined _MSC_VER || _MSC_VER >= 1700
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred) {boost::shared_lock<boost::shared_mutex> lock(__mutex); for (auto& item : __can) __pred(item);}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred) {for (auto& item : __can) __pred(item);}
#else
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred) {boost::shared_lock<boost::shared_mutex> lock(__mutex); std::for_each(std::begin(__can), std::end(__can), __pred);}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred) {std::for_each(std::begin(__can), std::end(__can), __pred);}
#endif

	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		boost::shared_lock<boost::shared_mutex> lock(__mutex);
		for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_one(_Can& __can, const _Predicate& __pred) {for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;}

	template<typename _Can>
	bool splice_helper(_Can& dest_can, _Can& src_can, size_t max_size = MAX_MSG_NUM)
	{
		auto size = dest_can.size();
		if (size < max_size) //dest_can can hold more items.
		{
			size = max_size - size; //maximum items this time can handle
			auto begin_iter = std::begin(src_can), end_iter = std::end(src_can);
			if (src_can.size() > size) //some items left behind
			{
				auto left_num = src_can.size() - size;
				end_iter = left_num > size ? std::next(begin_iter, size) : std::prev(end_iter, left_num); //find the minimum movement
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

#if !defined _MSC_VER || _MSC_VER >= 1700
	#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) {boost::shared_lock<boost::shared_mutex> lock(MUTEX); for (auto& item : CAN) __pred(item);}

	#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto& item : CAN) __pred(item);} \
	template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto& item : CAN) __pred(item);}
#else
	#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) {boost::shared_lock<boost::shared_mutex> lock(MUTEX); std::for_each(std::begin(CAN), std::end(CAN), __pred);}

	#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) {std::for_each(std::begin(CAN), std::end(CAN), __pred);} \
	template<typename _Predicate> void NAME(const _Predicate& __pred) const {std::for_each(std::begin(CAN), std::end(CAN), __pred);}
#endif

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{boost::shared_lock<boost::shared_mutex> lock(MUTEX); for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const {for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

//used by both TCP and UDP
#define SAFE_SEND_MSG_CHECK \
{ \
	if (!ST_THIS is_send_allowed() || ST_THIS get_io_service().stopped()) return false; \
	boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50)); \
}

#define GET_PENDING_MSG_NUM(FUNNAME, CAN, MUTEX) size_t FUNNAME() {boost::shared_lock<boost::shared_mutex> lock(MUTEX); return CAN.size();}
#define PEEK_FIRST_PENDING_MSG(FUNNAME, CAN, MUTEX, MSGTYPE) \
void FUNNAME(MSGTYPE& msg) \
{ \
	msg.clear(); \
	boost::shared_lock<boost::shared_mutex> lock(MUTEX); \
	if (!CAN.empty()) \
		msg = CAN.front(); \
}
#define POP_FIRST_PENDING_MSG(FUNNAME, CAN, MUTEX, MSGTYPE) \
void FUNNAME(MSGTYPE& msg) \
{ \
	msg.clear(); \
	boost::unique_lock<boost::shared_mutex> lock(MUTEX); \
	if (!CAN.empty()) \
	{ \
		msg.swap(CAN.front()); \
		CAN.pop_front(); \
	} \
}
#define POP_ALL_PENDING_MSG(FUNNAME, CAN, MUTEX, CANTYPE) \
void FUNNAME(CANTYPE& msg_list) \
{ \
	boost::unique_lock<boost::shared_mutex> lock(MUTEX); \
	msg_list.splice(msg_list.end(), CAN); \
}

///////////////////////////////////////////////////
//TCP msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	boost::unique_lock<boost::shared_mutex> lock(ST_THIS send_msg_buffer_mutex); \
	return (can_overflow || ST_THIS send_msg_buffer.size() < MAX_MSG_NUM) ? ST_THIS do_direct_send_msg(ST_THIS packer_->pack_msg(pstr, len, num, NATIVE)) : false; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_POST_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {return ST_THIS direct_post_msg(ST_THIS packer_->pack_msg(pstr, len, num, NATIVE), can_overflow);} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into st_tcp_socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) {while (!SEND_FUNNAME(pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{ST_THIS do_something_to_all([=](typename Pool::object_ctype& item) {item->SEND_FUNNAME(pstr, len, num, can_overflow);});} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) {return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	boost::unique_lock<boost::shared_mutex> lock(ST_THIS send_msg_buffer_mutex); \
	if (can_overflow || ST_THIS send_msg_buffer.size() < MAX_MSG_NUM) \
	{ \
		udp_msg<typename Packer::msg_type> msg(peer_addr, ST_THIS packer_->pack_msg(pstr, len, num, NATIVE)); \
		return ST_THIS do_direct_send_msg(std::move(msg)); \
	} \
	return false; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define UDP_POST_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	udp_msg<typename Packer::msg_type> msg(peer_addr, ST_THIS packer_->pack_msg(pstr, len, num, NATIVE)); \
	return ST_THIS direct_post_msg(std::move(msg), can_overflow); \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false, success at here just means putting the msg into st_udp_socket's send buffer successfully
//if can_overflow equal to false and the buffer is not available, will wait until it becomes available
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//UDP msg sending interface
///////////////////////////////////////////////////

#include <sstream>

#ifndef UNIFIED_OUT_BUF_NUM
#define UNIFIED_OUT_BUF_NUM	2048
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
#define all_out_helper2(head) char output_buff[UNIFIED_OUT_BUF_NUM]; all_out_helper(head, output_buff, sizeof(output_buff)); puts(output_buff)

#ifndef CUSTOM_LOG
class unified_out
{
public:
#ifdef NO_UNIFIED_OUT
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

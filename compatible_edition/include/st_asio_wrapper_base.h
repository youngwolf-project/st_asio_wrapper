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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/typeof/typeof.hpp>

#include "st_asio_wrapper.h"

#ifndef UNIFIED_OUT_BUF_NUM
#define UNIFIED_OUT_BUF_NUM	2048
#endif
#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN			4000
#endif
//msg send and recv buffer's maximum size
//big buffer size won't occupy big memory if you store a little msgs
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

#define SHARED_OBJECT(CLASS_NAME, FATHER_NAME) \
class CLASS_NAME : public FATHER_NAME, public boost::enable_shared_from_this<CLASS_NAME>

#define SHARED_OBJECT_T(CLASS_NAME, FATHER_NAME, MSGTYPE, SOCKET, SERVER) \
class CLASS_NAME : public FATHER_NAME<MSGTYPE, SOCKET>, public boost::enable_shared_from_this<CLASS_NAME<MSGTYPE, SOCKET, SERVER> >

namespace st_asio_wrapper
{
	//free functions, used to do something to any container optionally with any mutex
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		boost::mutex::scoped_lock lock(__mutex);
		for(BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) __pred(*iter);
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred)
		{for(BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) __pred(*iter);}

	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		boost::mutex::scoped_lock lock(__mutex);
		for (BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) if (__pred(*iter)) break;
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_one(_Can& __can, const _Predicate& __pred)
		{for (BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) if (__pred(*iter)) break;}

	template<typename _Can>
	bool splice_helper(_Can& dest_can, _Can& src_can, size_t max_size = MAX_MSG_NUM)
	{
		size_t size = dest_can.size();
		if (size < max_size) //dest_can's buffer available
		{
			size = max_size - size; //maximum items this time can handle
			BOOST_AUTO(begin_iter, src_can.begin()); BOOST_AUTO(end_iter, src_can.end());
			if (src_can.size() > size) //some items left behind
			{
				size_t left_num = src_can.size() - size;
				if (left_num > size) //find the minimum movement
					std::advance(end_iter = begin_iter, size);
				else
					std::advance(end_iter, -(ssize_t) left_num);
			}
			else
				size = src_can.size();
			//use size to avoid std::distance() call, so, size must correct
			dest_can.splice(dest_can.end(), src_can, begin_iter, end_iter, size);

			return size > 0;
		}

		return false;
	}

//member functions, used to do something to any member container optionally with any member mutex
#define DO_SOMETHING_TO_ALL_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ALL_MUTEX_NAME(do_something_to_all, CAN, MUTEX)
#define DO_SOMETHING_TO_ALL(CAN) DO_SOMETHING_TO_ALL_NAME(do_something_to_all, CAN)

#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{ \
	boost::mutex::scoped_lock lock(MUTEX); \
	for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) __pred(*iter); \
}

#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) __pred(*iter);} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const \
	{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) __pred(*iter);}

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{ \
	boost::mutex::scoped_lock lock(MUTEX); \
	for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break; \
}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const \
	{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break;}

//used by both TCP and UDP
#define SAFE_SEND_MSG_CHECK \
{ \
	if (!ST_THIS is_send_allowed() || ST_THIS get_io_service().stopped()) return false; \
	boost::this_thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(50)); \
}

///////////////////////////////////////////////////
//TCP msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	boost::mutex::scoped_lock lock(send_msg_buffer_mutex); \
	if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) \
	{ \
		MsgType msg = ST_THIS packer_->pack_msg(pstr, len, num, NATIVE); \
		return ST_THIS do_direct_send_msg(msg); \
	} \
	return false; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(MsgType& str, bool can_overflow = false) \
	{if (NATIVE) return ST_THIS direct_send_msg(str, can_overflow); return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_POST_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	MsgType msg = ST_THIS packer_->pack_msg(pstr, len, num, NATIVE); \
	return ST_THIS direct_post_msg(msg, can_overflow); \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(MsgType& str, bool can_overflow = false) \
	{if (NATIVE) return ST_THIS direct_post_msg(str, can_overflow); return FUNNAME(str.data(), str.size(), can_overflow);}

//guarantee send msg successfully even if can_overflow equal to false
//success at here just means put the msg into st_tcp_socket's send buffer
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{while (!SEND_FUNNAME(pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(MsgType& str, bool can_overflow = false) \
	{while (!SEND_FUNNAME(str, can_overflow)) SAFE_SEND_MSG_CHECK return true;}

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{ST_THIS do_something_to_all(boost::bind(&Socket::SEND_FUNNAME, _1, pstr, len, num, can_overflow));} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//TCP msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//UDP msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) \
	{return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) \
	{return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, \
	const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	boost::mutex::scoped_lock lock(send_msg_buffer_mutex); \
	if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) \
	{ \
		udp_msg<MsgType> msg = {peer_addr, ST_THIS packer_->pack_msg(pstr, len, num, NATIVE)}; \
		return ST_THIS do_direct_send_msg(msg); \
	} \
	return false; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, MsgType& str, bool can_overflow = false) \
{ \
	if (NATIVE) \
	{ \
		boost::mutex::scoped_lock lock(send_msg_buffer_mutex); \
		if (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) \
		{ \
			udp_msg<MsgType> msg = {peer_addr}; \
			msg.str.swap(str); \
			return ST_THIS do_direct_send_msg(msg); \
		} \
		return false; \
	} \
	return FUNNAME(peer_addr, str.data(), str.size(), can_overflow); \
}

#define UDP_POST_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, \
	bool can_overflow = false) \
{ \
	udp_msg<MsgType> msg = {peer_addr, ST_THIS packer_->pack_msg(pstr, len, num, NATIVE)}; \
	return ST_THIS direct_post_msg(msg, can_overflow); \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, MsgType& str, bool can_overflow = false) \
{ \
	if (NATIVE) \
	{ \
		udp_msg<MsgType> msg = {peer_addr}; \
		msg.str.swap(str); \
		return ST_THIS direct_post_msg(msg); \
	} \
	return FUNNAME(peer_addr, str.data(), str.size(), can_overflow); \
}

//guarantee send msg successfully even if can_overflow equal to false
//success at here just means put the msg into st_udp_socket's send buffer
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, \
	bool can_overflow = false) \
	{while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow)) SAFE_SEND_MSG_CHECK return true;} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool) \
bool FUNNAME(const boost::asio::ip::udp::endpoint& peer_addr, MsgType& str, bool can_overflow = false) \
	{while (!SEND_FUNNAME(peer_addr, str, can_overflow)) SAFE_SEND_MSG_CHECK return true;}
//UDP msg sending interface
///////////////////////////////////////////////////

class log_formater
{
public:
	static void all_out(char* buff, const char* fmt, va_list& ap)
	{
		assert(NULL != buff);
		time_t now = time(NULL);
#if defined _MSC_VER
		ctime_s(buff, UNIFIED_OUT_BUF_NUM, &now);
#else
		ctime_r(&now, buff);
#endif
		size_t len = strlen(buff);
		assert(len > 0);
		if ('\n' == buff[len - 1])
			--len;
#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
		strcpy_s(buff + len, UNIFIED_OUT_BUF_NUM - len, " -> ");
		len += 4;
		vsnprintf_s(buff + len,  UNIFIED_OUT_BUF_NUM - len, _TRUNCATE, fmt, ap);
#else
		strcpy(buff + len, " -> ");
		len += 4;
		vsnprintf(buff + len, UNIFIED_OUT_BUF_NUM - len, fmt, ap);
#endif
	}
};

#define all_out_helper(buff) va_list ap; va_start(ap, fmt); log_formater::all_out(buff, fmt, ap); va_end(ap)
#define all_out_helper2 char output_buff[UNIFIED_OUT_BUF_NUM]; all_out_helper(output_buff); puts(output_buff)

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
	static void fatal_out(const char* fmt, ...) {all_out_helper2;}
	static void error_out(const char* fmt, ...) {all_out_helper2;}
	static void warning_out(const char* fmt, ...) {all_out_helper2;}
	static void info_out(const char* fmt, ...) {all_out_helper2;}
	static void debug_out(const char* fmt, ...) {all_out_helper2;}
#endif
};
#endif

} //namespace

#endif /* ST_ASIO_WRAPPER_BASE_H_ */

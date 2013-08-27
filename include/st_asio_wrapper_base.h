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
using namespace boost;
using namespace boost::asio;
using namespace boost::system;

#include "st_asio_wrapper_verification.h"

#ifndef UNIFIED_OUT_BUF_NUM
#define UNIFIED_OUT_BUF_NUM	2048
#endif
#define HEAD_LEN			(sizeof(unsigned short))
#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN			4000
#endif
//msg send and recv buffer's max size
//big buffer size won't occupy big memory if you store a little msgs
#ifndef MAX_MSG_NUM
#define MAX_MSG_NUM	1024
#endif

#if defined _MSC_VER
#define size_t_format "%Iu"
#define ST_THIS //workaround to make up the BOOST_AUTO's defect under vc2008 and compiler bugs before vc2012
#else // defined __GNUC__
#define size_t_format "%tu"
#define ST_THIS this->
#endif

#define SHARED_OBJECT(CLASS_NAME, FATHER_NAME) \
class CLASS_NAME : public FATHER_NAME, public boost::enable_shared_from_this<CLASS_NAME>

#define SHARED_OBJECT_T(CLASS_NAME, FATHER_NAME, TYPENAME) \
class CLASS_NAME : public FATHER_NAME, public boost::enable_shared_from_this<CLASS_NAME<TYPENAME>>

namespace st_asio_wrapper
{
	//free functions, used to do something to any container optionally with any mutex
#if !defined _MSC_VER || _MSC_VER >= 1700
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
		{mutex::scoped_lock lock(__mutex); for (auto& item : __can) __pred(item);}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred) {for (auto& item : __can) __pred(item);}
#else
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		mutex::scoped_lock lock(__mutex);
		std::for_each(std::begin(__can), std::end(__can), [&](decltype(*std::begin(__can))& item) {__pred(item);});
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred)
		{std::for_each(std::begin(__can), std::end(__can), [&](decltype(*std::begin(__can))& item) {__pred(item);});}
#endif

	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		mutex::scoped_lock lock(__mutex);
		for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_one(_Can& __can, const _Predicate& __pred)
		{for (auto iter = std::begin(__can); iter != std::end(__can); ++iter) if (__pred(*iter)) break;}

//member functions, used to do something to any member container optionally with any member mutex
#define DO_SOMETHING_TO_ALL_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ALL_MUTEX_NAME(do_something_to_all, CAN, MUTEX)
#define DO_SOMETHING_TO_ALL(CAN) DO_SOMETHING_TO_ALL_NAME(do_something_to_all, CAN)

#if !defined _MSC_VER || _MSC_VER >= 1700
	#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{mutex::scoped_lock lock(MUTEX); for (auto& item : CAN) __pred(item);}

	#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{for (auto& item : CAN) __pred(item);} \
	template<typename _Predicate> void NAME(const _Predicate& __pred) const \
	{for (auto& item : CAN) __pred(item);}
#else
	#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{ \
		mutex::scoped_lock lock(MUTEX); \
		std::for_each(std::begin(CAN), std::end(CAN), [&](decltype(*std::begin(CAN))& item) {__pred(item);}); \
	}

	#define DO_SOMETHING_TO_ALL_NAME(NAME, CAN) \
	template<typename _Predicate> void NAME(const _Predicate& __pred) \
	{std::for_each(std::begin(CAN), std::end(CAN), [&](decltype(*std::begin(CAN))& item) {__pred(item);});} \
	template<typename _Predicate> void NAME(const _Predicate& __pred) const \
	{std::for_each(std::begin(CAN), std::end(CAN), [&](decltype(*std::begin(CAN))& item) {__pred(item);});}
#endif

#define DO_SOMETHING_TO_ONE_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ONE_MUTEX_NAME(do_something_to_one, CAN, MUTEX)
#define DO_SOMETHING_TO_ONE(CAN) DO_SOMETHING_TO_ONE_NAME(do_something_to_one, CAN)

#define DO_SOMETHING_TO_ONE_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{ \
	mutex::scoped_lock lock(MUTEX); \
	for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break; \
}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const \
{for (auto iter = std::begin(CAN); iter != std::end(CAN); ++iter) if (__pred(*iter)) break;}

///////////////////////////////////////////////////
//tcp msg sending interface
#define TCP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const char* pstr, size_t len, bool can_overflow = false) {return FUNNAME(&pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const std::string& str, bool can_overflow = false) {return FUNNAME(str.data(), str.size(), can_overflow);}

#define TCP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	mutex::scoped_lock lock(send_msg_buffer_mutex); \
	return (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) ? \
		direct_insert_msg(packer_->pack_msg(pstr, len, num, NATIVE)) : false; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false
//success at here just means put the msg into st_tcp_socket's send buffer
#define TCP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
{ \
	while (!SEND_FUNNAME(pstr, len, num, can_overflow)) \
	{ \
		if (!is_send_allowed() || get_io_service().stopped()) return false; \
		this_thread::sleep(get_system_time() + posix_time::milliseconds(50)); \
	} \
	return true; \
} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

#define TCP_BROADCAST_MSG(FUNNAME, SEND_FUNNAME) \
void FUNNAME(const char* const pstr[], const size_t len[], size_t num, bool can_overflow = false) \
	{ST_THIS do_something_to_all(boost::bind(&Socket::SEND_FUNNAME, _1, pstr, len, num, can_overflow));} \
TCP_SEND_MSG_CALL_SWITCH(FUNNAME, void)
//tcp msg sending interface
///////////////////////////////////////////////////

///////////////////////////////////////////////////
//udp msg sending interface
#define UDP_SEND_MSG_CALL_SWITCH(FUNNAME, TYPE) \
TYPE FUNNAME(const udp::endpoint& peer_addr, const char* pstr, size_t len, bool can_overflow = false) \
	{return FUNNAME(peer_addr, &pstr, &len, 1, can_overflow);} \
TYPE FUNNAME(const udp::endpoint& peer_addr, const std::string& str, bool can_overflow = false) \
	{return FUNNAME(peer_addr, str.data(), str.size(), can_overflow);}

#define UDP_SEND_MSG(FUNNAME, NATIVE) \
bool FUNNAME(const udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, \
	bool can_overflow = false) \
{ \
	mutex::scoped_lock lock(send_msg_buffer_mutex); \
	return (can_overflow || send_msg_buffer.size() < MAX_MSG_NUM) ? \
		direct_insert_msg(peer_addr, packer_->pack_msg(pstr, len, num, NATIVE)) : false; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)

//guarantee send msg successfully even if can_overflow equal to false
//success at here just means put the msg into st_udp_socket's send buffer
#define UDP_SAFE_SEND_MSG(FUNNAME, SEND_FUNNAME) \
bool FUNNAME(const udp::endpoint& peer_addr, const char* const pstr[], const size_t len[], size_t num, \
	bool can_overflow = false) \
{ \
	while (!SEND_FUNNAME(peer_addr, pstr, len, num, can_overflow)) \
	{ \
		if (!is_send_allowed() || get_io_service().stopped()) return false; \
		this_thread::sleep(get_system_time() + posix_time::milliseconds(50)); \
	} \
	return true; \
} \
UDP_SEND_MSG_CALL_SWITCH(FUNNAME, bool)
//udp msg sending interface
///////////////////////////////////////////////////

#ifdef NO_UNIFIED_OUT
namespace unified_out
{
void null_out() {}
#define info_out(fmt, ...) null_out()
#define debug_out(fmt, ...) null_out()
#define error_out(fmt, ...) null_out()
}
#else
class unified_out
{
public:
	static void info_out(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		all_out(fmt, ap, 1);
		va_end(ap);
	}

	static void debug_out(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		all_out(fmt, ap, 2);
		va_end(ap);
	}

	static void error_out(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		all_out(fmt, ap, 3);
		va_end(ap);
	}

protected:
	static void all_out(const char* fmt, va_list& ap, int type, int level = 1)
	{
		char output_buff[UNIFIED_OUT_BUF_NUM];
		time_t now = time(nullptr);

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
		auto buffer_len = sizeof(output_buff) / 2;
		ctime_s(std::next(output_buff, buffer_len), buffer_len, &now);
		strcpy_s(output_buff, buffer_len, std::next(output_buff, buffer_len));
#else
		strcpy(output_buff, ctime(&now));
#endif
		auto len = strlen(output_buff);
		assert(len > 0);
		if ('\n' == *std::next(output_buff, len - 1))
			--len;
#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
		strcpy_s(std::next(output_buff, len), buffer_len, " -> ");
		len += 4;
		buffer_len = sizeof(output_buff) - len;
		vsnprintf_s(std::next(output_buff, len), buffer_len, buffer_len, fmt, ap);
#else
		strcpy(std::next(output_buff, len), " -> ");
		len += 4;
		vsnprintf(std::next(output_buff, len), sizeof(output_buff) - len, fmt, ap);
#endif

		puts(output_buff);
	}
};
#endif

} //namespace

#endif /* ST_ASIO_WRAPPER_BASE_H_ */

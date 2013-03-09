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

#include <boost/typeof/typeof.hpp>
#include <boost/thread.hpp>
using namespace boost;

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

namespace st_asio_wrapper
{
	//free functions, used to do something to any container optionally with any mutex
	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_all(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		mutex::scoped_lock lock(__mutex);
		for(BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) __pred(*iter);
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_all(_Can& __can, const _Predicate& __pred)
		{for(BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) __pred(*iter);}

	template<typename _Can, typename _Mutex, typename _Predicate>
	void do_something_to_one(_Can& __can, _Mutex& __mutex, const _Predicate& __pred)
	{
		mutex::scoped_lock lock(__mutex);
		for (BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) if (__pred(*iter)) break;
	}

	template<typename _Can, typename _Predicate>
	void do_something_to_one(_Can& __can, const _Predicate& __pred)
		{for (BOOST_AUTO(iter, __can.begin()); iter != __can.end(); ++iter) if (__pred(*iter)) break;}

//member functions, used to do something to any member container optionally with any member mutex
#define DO_SOMETHING_TO_ALL_MUTEX(CAN, MUTEX) DO_SOMETHING_TO_ALL_MUTEX_NAME(do_something_to_all, CAN, MUTEX)
#define DO_SOMETHING_TO_ALL(CAN) DO_SOMETHING_TO_ALL_NAME(do_something_to_all, CAN)

#define DO_SOMETHING_TO_ALL_MUTEX_NAME(NAME, CAN, MUTEX) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{ \
	mutex::scoped_lock lock(MUTEX); \
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
	mutex::scoped_lock lock(MUTEX); \
	for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break; \
}

#define DO_SOMETHING_TO_ONE_NAME(NAME, CAN) \
template<typename _Predicate> void NAME(const _Predicate& __pred) \
{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break;} \
template<typename _Predicate> void NAME(const _Predicate& __pred) const \
{for (BOOST_AUTO(iter, CAN.begin()); iter != CAN.end(); ++iter) if (__pred(*iter)) break;}

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
		time_t now = time(NULL);

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
		size_t buffer_len = sizeof(output_buff) / 2;
		ctime_s(output_buff + buffer_len, buffer_len, &now);
		strcpy_s(output_buff, buffer_len, output_buff + buffer_len);
#else
		strcpy(output_buff, ctime(&now));
#endif
		size_t len = strlen(output_buff);
		assert(len > 0);
		if ('\n' == output_buff[len - 1])
			--len;
#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
		strcpy_s(output_buff + len, buffer_len, " -> ");
		len += 4;
		buffer_len = sizeof(output_buff) - len;
		vsnprintf_s(output_buff + len, buffer_len, buffer_len, fmt, ap);
#else
		strcpy(output_buff + len, " -> ");
		len += 4;
		vsnprintf(output_buff + len, sizeof(output_buff) - len, fmt, ap);
#endif

		puts(output_buff);
	}
};
#endif

} //namespace

#endif /* ST_ASIO_WRAPPER_BASE_H_ */

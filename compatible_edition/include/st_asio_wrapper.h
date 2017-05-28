/*
 * st_asio_wrapper.h
 *
 *  Created on: 2012-10-21
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this head file contain some macros used to verify the compiler and update logs etc.

 * license: www.boost.org/LICENSE_1_0.txt
 *
 * change log:
 * See st_asio_wrapper.h in standard edition.
 *
 */

#ifndef ST_ASIO_WRAPPER_H_
#define ST_ASIO_WRAPPER_H_

#define ST_ASIO_WRAPPER_VER		10400	//[x]xyyzz -> [x]x.[y]y.[z]z
#define ST_ASIO_WRAPPER_VERSION	"1.4.0"

//boost and compiler check
#ifdef _MSC_VER
	#if _MSC_VER >= 1700
		#pragma message("Your compiler is Visual C++ 11.0 or higher, you can use the standard edition to gain some performance improvement.")
	#endif
#elif defined(__GNUC__)
	#ifdef __clang__
		#if __clang_major__ > 3 || __clang_major__ == 3 && __clang_minor__ >= 1
			#warning Your compiler is Clang 3.1 or higher, you can use the standard edition to gain some performance improvement.
		#endif
	#elif __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6
		#warning Your compiler is GCC 4.6 or higher, you can use the standard edition to gain some performance improvement.
	#endif

	#if defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(__cplusplus) && __cplusplus >= 201103L
		#warning st_asio_wrapper(compatible edition) does not need any c++11 features.
	#endif
#else
	#error st_asio_wrapper only support Visual C++, GCC and Clang.
#endif

#if BOOST_VERSION < 104900
	#error st_asio_wrapper only support boost 1.49 or higher.
#endif
//boost and compiler check

#endif /* ST_ASIO_WRAPPER_H_ */

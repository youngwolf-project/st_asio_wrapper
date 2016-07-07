/*
 * st_asio_wrapper_verification.h
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
 * 2012.7.7
 * Created
 *
 * 2012.7.7 - 2016.7.7
 * Beta edition
 *
 * 2016.7.7		version 1.0.0
 * First release
 *
 */

#ifndef ST_ASIO_WRAPPER_H_
#define ST_ASIO_WRAPPER_H_

#define ST_ASIO_WRAPPER_VERSION 10000 //[x]xxxxx -> [x]x.xx.xx

#ifdef _MSC_VER
	#if _MSC_VER >= 1600
		#warning Your compiler is Visual C++ 10.0 or higher, you can use the standard edition to gain some performance improvement.
	#endif
#elif defined(__GNUC__)
	//After a roughly reading from gcc.gnu.org and clang.llvm.org, I believed that the minimum version of GCC and Clang that support c++0x
	//are 4.6 and 3.1, so, I supply the following compiler verification. If there's something wrong, you can freely modify them,
	//and if you let me know, I'll be very appreciated.
	#ifdef __clang__
		#if __clang_major__ > 3 || __clang_major__ == 3 && __clang_minor__ >= 1
			#warning Your compiler is Clang 3.1 or higher, you can use the standard edition to gain some performance improvement.
		#endif
	#elif __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6
		#warning Your compiler is GCC 4.6 or higher, you can use the standard edition to gain some performance improvement.
	#endif

	#if defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(__cplusplus) && __cplusplus >= 201103L
		#warning st_asio_wrapper(compatible edition) does not need any c++0x features.
	#endif
#else
	#error st_asio_wrapper only support Visual C++, GCC and Clang.
#endif

#endif /* ST_ASIO_WRAPPER_H_ */

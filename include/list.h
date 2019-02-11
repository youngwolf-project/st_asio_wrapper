/*
 * list.h
 *
 *  Created on: 2019-2-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * list.
 */

#ifndef ST_ASIO_LIST_H_
#define ST_ASIO_LIST_H_

#include <boost/version.hpp>
#include <boost/container/list.hpp>

namespace st_asio_wrapper
{

template<typename T> class list : public boost::container::list<T>
{
private:
	typedef boost::container::list<T> super;

public:
	list() {}
	list(size_t n) : super(n) {}

#if BOOST_VERSION < 106200
	typename super::reference emplace_back() {super::emplace_back(); return super::back();}
	template<typename Arg> typename super::reference emplace_back(const Arg& arg) {super::emplace_back(arg); return super::back();}
	template<typename Arg1, typename Arg2>
	typename super::reference emplace_back(const Arg1& arg1, const Arg2& arg2) {super::emplace_back(arg1, arg2); return super::back();}
#endif
};

} //namespace

#endif /* ST_ASIO_LIST_H_ */

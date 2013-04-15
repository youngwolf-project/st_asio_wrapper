/*
 * st_asio_wrapper_packer.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * packer base class
 */

#ifndef ST_ASIO_WRAPPER_PACKER_H_
#define ST_ASIO_WRAPPER_PACKER_H_

#include <boost/asio.hpp>

#include "st_asio_wrapper_base.h"

namespace st_asio_wrapper
{

class i_packer
{
public:
	virtual ~i_packer() {}

	static size_t get_max_msg_size() {return MAX_MSG_LEN - HEAD_LEN;}
	virtual std::string pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
};

class packer : public i_packer
{
public:
	virtual std::string pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		std::string str;
		if (nullptr != pstr && nullptr != len)
		{
			size_t total_len = native ? 0 : HEAD_LEN;
			auto last_total_len = total_len;
			for (size_t i = 0; i < num; ++i)
			{
				total_len += len[i];
				if (last_total_len > total_len || nullptr == pstr[i] || total_len > MAX_MSG_LEN) //overflow or null pointer
					return str;
				last_total_len = total_len;
			}

			if (total_len > (native ? 0 : HEAD_LEN))
			{
				str.reserve(total_len);
				if (!native)
				{
					auto head_len = htons((unsigned short) total_len);
					str.append((const char*) &head_len, HEAD_LEN);
				}
				for (size_t i = 0; i < num; ++i)
					str.append(pstr[i], len[i]);
			}
		} //if (nullptr != pstr)

		return str;
	}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_PACKER_H_ */

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

#include "st_asio_wrapper_base.h"

#ifdef HUGE_MSG
#define HEAD_TYPE	uint32_t
#define HEAD_H2N	htonl
#else
#define HEAD_TYPE	uint16_t
#define HEAD_H2N	htons
#endif
#define HEAD_LEN	(sizeof(HEAD_TYPE))

namespace st_asio_wrapper
{

class i_packer
{
public:
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
				if (nullptr != pstr[i])
				{
					total_len += len[i];
					if (last_total_len > total_len || total_len > MAX_MSG_LEN) //overflow
					{
						unified_out::error_out("pack msg error: length exceeds the MAX_MSG_LEN!");
						return str;
					}
					last_total_len = total_len;
				}

			if (total_len > (native ? 0 : HEAD_LEN))
			{
				if (!native)
				{
					auto head_len = (HEAD_TYPE) total_len;
					if (total_len != head_len)
					{
						unified_out::error_out("pack msg error: length exceeds the header's range!");
						return str;
					}

					head_len = HEAD_H2N(head_len);
					str.reserve(total_len);
					str.append((const char*) &head_len, HEAD_LEN);
				}
				else
					str.reserve(total_len);

				for (size_t i = 0; i < num; ++i)
					if (nullptr != pstr[i])
						str.append(pstr[i], len[i]);
			}
		} //if (nullptr != pstr)

		return str;
	}
};

} //namespace

#endif /* ST_ASIO_WRAPPER_PACKER_H_ */

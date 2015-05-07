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

class packer_helper
{
public:
	//return (size_t) -1 means length exceeded the MAX_MSG_LEN
	static size_t msg_size_check(size_t pre_len, const char* const pstr[], const size_t len[], size_t num)
	{
		if (nullptr == pstr || nullptr == len)
			return -1;

		auto total_len = pre_len;
		auto last_total_len = total_len;
		for (size_t i = 0; i < num; ++i)
			if (nullptr != pstr[i])
			{
				total_len += len[i];
				if (last_total_len > total_len || total_len > MAX_MSG_LEN) //overflow
				{
					unified_out::error_out("pack msg error: length exceeded the MAX_MSG_LEN!");
					return -1;
				}
				last_total_len = total_len;
			}

		return total_len;
	}
};

template<typename MsgType>
class i_packer
{
public:
	virtual void reset_state() {}
	virtual MsgType pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false) = 0;
};

class packer : public i_packer<std::string>
{
public:
	static size_t get_max_msg_size() {return MAX_MSG_LEN - HEAD_LEN;}
	virtual std::string pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		std::string str;
		auto pre_len = native ? 0 : HEAD_LEN;
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 == total_len)
			return str;
		else if (total_len > pre_len)
		{
			if (!native)
			{
				auto head_len = (HEAD_TYPE) total_len;
				if (total_len != head_len)
				{
					unified_out::error_out("pack msg error: length exceeded the header's range!");
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
		} //if (total_len > pre_len)

		return str;
	}
};

class fixed_legnth_packer : public i_packer<std::string>
{
public:
	virtual std::string pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		std::string str;
		auto total_len = packer_helper::msg_size_check(0, pstr, len, num);
		if ((size_t) -1 == total_len)
			return str;
		else if (total_len > 0)
		{
			str.reserve(total_len);
			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					str.append(pstr[i], len[i]);
		} //if (total_len > 0)

		return str;
	}
};

class prefix_suffix_packer : public i_packer<std::string>
{
public:
	void prefix_suffix(const std::string& prefix, const std::string& suffix)
		{assert(!suffix.empty() && prefix.size() + suffix.size() < MAX_MSG_LEN); _prefix = prefix;  _suffix = suffix;}
	const std::string& prefix() const {return _prefix;}
	const std::string& suffix() const {return _suffix;}

public:
	virtual std::string pack_msg(const char* const pstr[], const size_t len[], size_t num, bool native = false)
	{
		std::string str;
		auto pre_len = native ? 0 : _prefix.size() + _suffix.size();
		auto total_len = packer_helper::msg_size_check(pre_len, pstr, len, num);
		if ((size_t) -1 == total_len)
			return str;
		else if (total_len > pre_len)
		{
			str.reserve(total_len);
			if (!native)
				str.append(_prefix);
			for (size_t i = 0; i < num; ++i)
				if (nullptr != pstr[i])
					str.append(pstr[i], len[i]);
			if (!native)
				str.append(_suffix);
		} //if (total_len > pre_len)

		return str;
	}

private:
	std::string _prefix, _suffix;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_PACKER_H_ */

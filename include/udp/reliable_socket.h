/*
 * reliable_socket.h
 *
 *  Created on: 2021-9-3
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * reliable UDP socket
 */

#ifndef ST_ASIO_RELIABLE_UDP_SOCKET_H_
#define ST_ASIO_RELIABLE_UDP_SOCKET_H_

#include <ikcp.h>

#include "socket.h"

namespace st_asio_wrapper { namespace udp {

template<typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class reliable_socket_base : public socket_base<Packer, Unpacker, Matrix, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef socket_base<Packer, Unpacker, Matrix, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static const typename super::tid TIMER_BEGIN = super::TIMER_END;
	static const typename super::tid TIMER_CC = TIMER_BEGIN;
	static const typename super::tid TIMER_KCP_UPDATE = TIMER_BEGIN + 1;
	static const typename super::tid TIMER_END = TIMER_BEGIN + 5;

public:
	reliable_socket_base(boost::asio::io_context& io_context_) : super(io_context_), kcp(NULL), need_kcp_check(false), max_nsnd_que(ST_ASIO_RELIABLE_UDP_NSND_QUE) {}
	reliable_socket_base(Matrix& matrix_) : super(matrix_), kcp(NULL), need_kcp_check(false), max_nsnd_que(ST_ASIO_RELIABLE_UDP_NSND_QUE) {}
	~reliable_socket_base() {release_kcp();}

	ikcpcb* get_kcpcb() {return kcp;}
	ikcpcb* create_kcpcb(IUINT32 conv, void* user) {if (ST_THIS started()) return NULL; release_kcp(); return (kcp = ikcp_create(conv, user));}

	IUINT32 get_max_nsnd_que() const {return max_nsnd_que;}
	void set_max_nsnd_que(IUINT32 max_nsnd_que_) {max_nsnd_que = max_nsnd_que_;}

	int output(const char* buf, int len)
	{
		boost::system::error_code ec;
		ST_THIS next_layer().send(boost::asio::buffer(buf, (size_t) len), 0, ec);
		return ec ? (unified_out::error_out(ST_ASIO_LLF " send msg error (%d)", ST_THIS id(), ec.value()), 0) : len;
	}

	//from kpc's test.h
	/* get system time */
	static inline void itimeofday(long *sec, long *usec)
	{
		#if defined(__unix)
		struct timeval time;
		gettimeofday(&time, NULL);
		if (sec) *sec = time.tv_sec;
		if (usec) *usec = time.tv_usec;
		#else
		static long mode = 0, addsec = 0;
		BOOL retval;
		static IINT64 freq = 1;
		IINT64 qpc;
		if (mode == 0) {
			retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
			freq = (freq == 0)? 1 : freq;
			retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
			addsec = (long)time(NULL);
			addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
			mode = 1;
		}
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		retval = retval * 2;
		if (sec) *sec = (long)(qpc / freq) + addsec;
		if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
		#endif
	}

	/* get clock in millisecond 64 */
	static inline IINT64 iclock64(void)
	{
		long s, u;
		IINT64 value;
		itimeofday(&s, &u);
		value = ((IINT64)s) * 1000 + (u / 1000);
		return value;
	}

	static inline IUINT32 iclock()
	{
		return (IUINT32)(iclock64() & 0xfffffffful);
	}
	//from kpc's test.h

protected:
	virtual bool do_start()
	{
		if (NULL != kcp)
			ST_THIS set_timer(TIMER_KCP_UPDATE, kcp_check(), boost::bind(&reliable_socket_base::timer_handler, this, boost::placeholders::_1));

		return super::do_start();
	}

	virtual void on_close() {release_kcp(); super::on_close();}

	virtual bool check_send_cc() //congestion control, return true means can continue to send messages
	{
		boost::lock_guard<boost::mutex> lock(mutex);
		if (NULL == kcp || kcp->nsnd_que <= max_nsnd_que)
			return true;

		ST_THIS set_timer(TIMER_CC, 10, boost::bind(&reliable_socket_base::timer_handler, this, boost::placeholders::_1));
		return false;
	}

	virtual bool do_send_msg(const typename super::in_msg& sending_msg)
	{
		boost::lock_guard<boost::mutex> lock(mutex);
		if (NULL == kcp)
			return false;

		int re = ikcp_send(kcp, sending_msg.data(), (long) sending_msg.size());
		if (re < 0)
			unified_out::error_out("ikcp_send return error: %d", re);
		else
			need_kcp_check = true;

		return true;
	}

	virtual void pre_handle_msg(typename Unpacker::container_type& msg_can)
	{
		boost::lock_guard<boost::mutex> lock(mutex);
		if (NULL == kcp)
			return;

		for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
		{
			int re = ikcp_input(kcp, iter->data(), (long) iter->size());
			if (re < 0)
				unified_out::error_out("ikcp_input return error: %d", re);
			else
				need_kcp_check = true;
		}

		msg_can.clear();
		char buff[ST_ASIO_MSG_BUFFER_SIZE];
		while (true)
		{
			int re = ikcp_recv(kcp, buff, sizeof(buff));
			if (re < 0)
				break;

			ST_THIS unpacker()->compose_msg(buff, (size_t) re, msg_can);
		}
	}

private:
	IUINT32 kcp_check()
	{
		IUINT32 now = iclock();
		return ikcp_check(kcp, now) - now;
	}

	bool timer_handler(typename super::tid id)
	{
		switch (id)
		{
		case TIMER_CC:
			if (NULL != kcp)
			{
				boost::unique_lock<boost::mutex> lock(mutex);
				if (kcp->nsnd_que > max_nsnd_que)
					return true; //continue CC
				lock.unlock();

				super::resume_sending();
			}
			break;
		case TIMER_KCP_UPDATE:
			if (NULL != kcp)
			{
				boost::lock_guard<boost::mutex> lock(mutex);
				ikcp_update(kcp, iclock());
				if (need_kcp_check)
				{
					need_kcp_check = false;
					ST_THIS change_timer_interval(TIMER_KCP_UPDATE, kcp_check());
				}

				return true;
			}
			break;
		default:
			assert(false);
			break;
		}

		return false;
	}

	void release_kcp() {if (NULL != kcp) ikcp_release(kcp); kcp = NULL;}

private:
	ikcpcb* kcp;
	boost::mutex mutex;

	bool need_kcp_check;
	IUINT32 max_nsnd_que;
};

}} //namespace

#endif /* ST_ASIO_RELIABLE_UDP_SOCKET_H_ */

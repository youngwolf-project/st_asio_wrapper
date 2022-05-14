#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>

#include "../include/base.h"
using namespace st_asio_wrapper;
using namespace st_asio_wrapper::tcp;

#ifdef _MSC_VER
#define fseeko _fseeki64
#define ftello _ftelli64
#define fl_type __int64
#else
#define fl_type off_t
#endif

#if BOOST_VERSION >= 105300
#define atomic_size	boost::atomic_int_fast64_t
#else
#define atomic_size atomic<boost::int_fast64_t>
#endif

#define ORDER_LEN	sizeof(char)
#define OFFSET_LEN	sizeof(fl_type)
#define DATA_LEN	OFFSET_LEN

/*
protocol:
head(1 byte) + body

if head equal to:
0: body is a filename
	request the file length, client->server->client
	return: same head + file length(8 bytes)
1: body is file offset(8 bytes) + data length(8 bytes)
	request the file content, client->server->client
	return: file content(no-protocol), repeat until all data requested by client been sent(client only need to request one time)
2: body is talk content
	talk, client->server. please note that server cannot talk to client, this is because server never knows whether
	it is going to transmit a file or not.
	return: na
3: body is object id(8 bytes)
	change file server's object ids, demonstrate how to use macro ST_ASIO_RESTORE_OBJECT.
	return: na

10:body is a filename
	request to create or truncate the file on the server, client->server->client
	return: same head + status(1 byte, 0 - success, !0 - failed) + filename
11:body is offset(8 bytes) + data length(8 bytes) + filename
	request to upload the file, client->server->client
	return: same head + data length(8 bytes) + status(1 byte, 0 - success, !0 - failed), then file content(no-protocol), repeat until all data been sent
*/

class base_socket
{
public:
	base_socket() : state(TRANS_IDLE), file(NULL) {}

protected:
	enum TRANS_STATE {TRANS_IDLE, TRANS_PREPARE, TRANS_BUSY};
	TRANS_STATE state;
	FILE* file;
};

#endif // COMMON_H_

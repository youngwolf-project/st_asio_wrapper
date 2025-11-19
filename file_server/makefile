
module = file_server
ext_cflag = -DST_ASIO_RESTORE_OBJECT -DST_ASIO_SERVER_PORT=5051 -DST_ASIO_START_OBJECT_ID=500 -DST_ASIO_WANT_MSG_SEND_NOTIFY -DST_ASIO_INPUT_QUEUE=non_lock_queue
ext_cflag += -DST_ASIO_RECV_BUFFER_TYPE="std::vector<boost::asio::mutable_buffer>" -DST_ASIO_SCATTERED_RECV_BUFFER -D_FILE_OFFSET_BITS=64

include ../config.mk


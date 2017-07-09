
ST_MAKE = ${MAKE_COMMAND}
ifeq (${MAKECMDGOALS}, debug)
	ST_MAKE += debug
else
	ifeq (${MAKECMDGOALS}, clean)
		ST_MAKE += clean
	endif
endif


release debug clean :
	cd echo_server && ${ST_MAKE}
	cd echo_client && ${ST_MAKE}
	cd client && ${ST_MAKE}
	cd file_server && ${ST_MAKE}
	cd file_client && ${ST_MAKE}
	cd pingpong_server && ${ST_MAKE}
	cd pingpong_client && ${ST_MAKE}
	cd concurrent_server && ${ST_MAKE}
	cd concurrent_client && ${ST_MAKE}
	cd udp_test && ${ST_MAKE}
	cd ssl_test && ${ST_MAKE}


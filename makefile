
ST_MAKE = ${MAKE_COMMAND}
ifeq (${MAKECMDGOALS}, debug)
	ST_MAKE += debug
else
	ifeq (${MAKECMDGOALS}, clean)
		ST_MAKE += clean
	endif
endif

target_machine = ${shell ${CXX} -dumpmachine}

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
	cd socket_management && ${ST_MAKE}
	cd udp_test && ${ST_MAKE}
	cd ssl_test && ${ST_MAKE}
	cd websocket_test && ${ST_MAKE}
	cd ssl_websocket_test && ${ST_MAKE}
ifeq (, ${findstring cygwin, ${target_machine}})
ifeq (, ${findstring mingw, ${target_machine}})
	cd unix_socket && ${ST_MAKE}
	cd unix_udp_test && ${ST_MAKE}
endif
endif


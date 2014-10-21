
MAKE = make
ifeq (${MAKECMDGOALS}, debug)
	MAKE = make debug
else
	ifeq (${MAKECMDGOALS}, clean)
		MAKE = make clean
	endif
endif


release debug clean :
	cd asio_server && ${MAKE}
	cd asio_client && ${MAKE}
	cd test_client && ${MAKE}
	cd file_server && ${MAKE}
	cd file_client && ${MAKE}
	cd udp_client && ${MAKE}
	cd ssl_test && ${MAKE}
	cd compatible_edition && ${MAKE}


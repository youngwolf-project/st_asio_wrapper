
ifeq ($(MAKECMDGOALS), debug)
	cflag = -g
	dir = ./debug
else
	cflag = -O2 -DNDEBUG
	lflag = -s
	dir = ./release
endif

module = asio_client
target = ${dir}/${module}


release debug : ${target}
${target} : makedir ${module}.o
	g++ -o ${target} ${target}.o ${lflag} -lboost_system -lboost_thread
${module}.o :
	g++ -Wall -fexceptions -std=c++11 ${cflag} -c asio_client.cpp -o ${target}.o


makedir:
	mkdir -p ${dir}


clean:
	rm -rf ./debug ./release



ifeq ($(MAKECMDGOALS), debug)
	cflag = -g
	dir = ./debug
else
	cflag = -O -DNDEBUG
	lflag = -s
	dir = ./release
endif

module = test_client
target = ${dir}/${module}


release debug : ${target}
${target} : makedir ${module}.o
	g++ -o ${target} ${target}.o ${lflag} -lboost_system -lboost_thread
${module}.o :
	g++ -Wall -fexceptions ${cflag} -c test_client.cpp -o ${target}.o


makedir:
	mkdir -p ${dir}


clean:
	rm -rf ./debug ./release


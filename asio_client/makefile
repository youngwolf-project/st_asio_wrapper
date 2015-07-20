
ifeq (${MAKECMDGOALS}, debug)
	cflag = -g -DDEBUG
	dir = debug
else
	cflag = -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflags = -Wall -fexceptions -std=c++11 ${cflag}

module = asio_client
target = ${dir}/${module}
sources = ${shell ls *.cpp}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell mkdir -p ${dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	g++ -o ${target} ${objects} ${lflag} -pthread -lboost_system -lboost_thread
${objects} : ${dir}/%.o : %.cpp
	g++ ${cflags} -E -MMD -MT '${subst .cpp,.o,${dir}/$<}' -MF ${subst .cpp,.d,${dir}/$<} $< 1>/dev/null
	g++ ${cflags} -c $< -o $@

.PHONY : clean
clean:
	-rm -rf debug release


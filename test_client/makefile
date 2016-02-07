
cflag = -Wall -fexceptions -std=c++11
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -pthread
#you may need the next line when compiling this project with clang under UNIX-like system, then please uncomment it
#cflag += -I/usr/local/include/
lflag += -pthread
#you may need the next line when compiling this project with clang under UNIX-like system, then please uncomment it
#lflag += -L/usr/local/lib/
libs = -lboost_system -lboost_thread -lboost_timer

module = test_client
target = ${dir}/${module}
sources = ${shell ls *.cpp}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell mkdir -p ${dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o ${target} ${objects} ${lflag} ${libs}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -MT '${subst .cpp,.o,${dir}/$<}' -MF ${subst .cpp,.d,${dir}/$<} $< 1>/dev/null
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	-rm -rf debug release


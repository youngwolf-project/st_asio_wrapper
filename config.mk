
# If your compiler cannot find boost, please specify it explicitly like this:
#boost_include_dir = -I/usr/local/include/
#boost_lib_dir = -L/usr/local/lib/

cflag = -Wall -fexceptions -std=c++98
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DBOOST_ASIO_NO_DEPRECATED
common_libs = -lboost_system -lboost_thread -lboost_chrono

kernel = ${shell uname -s}
ifeq (${kernel}, SunOS)
cflag += -pthreads ${ext_cflag} ${boost_include_dir}
lflag += -pthreads -lsocket -lnsl ${boost_lib_dir} ${common_libs} ${ext_libs}
else
ifeq (${kernel}, FreeBSD)
cflag += -pthread ${ext_cflag} ${boost_include_dir}
lflag += -pthread ${boost_lib_dir} ${common_libs} -lboost_atomic ${ext_libs}
else # here maybe still have other kernels need to be separated out
cflag += -pthread ${ext_cflag} ${boost_include_dir}
lflag += -pthread ${boost_lib_dir} ${common_libs} ${ext_libs}
endif
endif

target = ${dir}/${module}
sources = ${shell ls *.cpp}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell mkdir -p ${dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${subst .cpp,.d,${dir}/$<} $< 1>/dev/null
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	-rm -rf debug release



# If your compiler cannot find boost, please specify it explicitly like this:
#boost_include_dir = -I/usr/local/include/
#boost_lib_dir = -L/usr/local/lib/

ifndef STD
	STD = c++98 #if your boost version is too hight, you may need to specify c++11
endif

ifndef WIN_VER
	WIN_VER = -D_WIN32_WINNT=0x0501 #we may need to increase this version for example, to 0x0A00 (win 10)
endif

cflag = -Wall -fexceptions -std=${STD}
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DBOOST_ASIO_NO_DEPRECATED -DBOOST_CHRONO_HEADER_ONLY
common_libs = -lboost_system -lboost_thread #in some ENV, the file name of boost libraries may end with -mt
#if your boost version is too high, boost_system may not exists.

target_machine = ${shell ${CXX} -dumpmachine}
link_to_win = 0
ifneq (, ${findstring solaris, ${target_machine}})
	cflag += -pthreads
	lflag += -pthreads -lsocket -lnsl
else
	cflag += -pthread
	lflag += -pthread
	ifneq (, ${findstring freebsd, ${target_machine}})
		common_libs += -lboost_atomic
		#here maybe still have other machines need to be separated out
	endif

	ifneq (, ${findstring cygwin, ${target_machine}})
		cflag += -D__USE_W32_SOCKETS ${WIN_VER}
		link_to_win = 1
	endif
endif

ifneq (, ${findstring mingw, ${target_machine}})
	SHELL = cmd
	cflag += -D__USE_MINGW_ANSI_STDIO=1 ${WIN_VER}
	link_to_win = 1
	ignore = 1>nul
	make_dir = md ${dir} 2>nul
	del_dirs = -rd /S /Q debug release 2>nul
else
	ifneq (, ${findstring windows, ${target_machine}})
		link_to_win = 1
	endif
	ignore = 1>/dev/null
	make_dir = mkdir -p ${dir}
	del_dirs = -rm -rf debug release
endif

ifeq (1, ${link_to_win})
	lflag += -lws2_32 -lwsock32
endif

cflag += ${ext_cflag} ${boost_include_dir}
lflag += ${boost_lib_dir} ${common_libs} ${ext_libs}

target = ${dir}/${module}
objects = ${patsubst %.cpp,${dir}/%.o,${wildcard *.cpp}}
deps = ${patsubst %.o,%.d,${objects}}
${shell ${make_dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${patsubst %.cpp,%.d,${dir}/$<} $< ${ignore}
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	${del_dirs}

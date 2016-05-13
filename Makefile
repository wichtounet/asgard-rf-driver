CXX=g++
LD=g++

default: release

.PHONY: default release debug all clean

include make-utils/flags-pi.mk
include make-utils/cpp-utils.mk

pi.conf:
	echo "user=pi" > pi.conf
	echo "pi=192.168.20.161" >> pi.conf
	echo "password=raspberry" >> pi.conf
	echo "dir=/home/pi/asgard/asgard-rf-driver/" >> pi.conf

conf: pi.conf

include pi.conf

CXX_FLAGS += -pedantic -Irc-switch-rpi -Iasgard-lib/include/
LD_FLAGS += -lwiringPi

$(eval $(call auto_folder_compile,src))
$(eval $(call auto_folder_compile,rc-switch-rpi))
$(eval $(call auto_add_executable,rf_driver))

release: release_rf_driver
release_debug: release_debug_rf_driver
debug: debug_rf_driver

all: release release_debug debug

run: release
	sudo ./release/bin/rf_driver

remote_clean:
	sshpass -p ${password} ssh ${user}@${pi} "cd ${dir} && make clean"

remote_make:
	sshpass -p ${password} scp -p Makefile ${user}@${pi}:${dir}/
	sshpass -p ${password} scp -p src/*.cpp ${user}@${pi}:${dir}/src/
	sshpass -p ${password} scp -p asgard-lib/include/asgard/*.hpp ${user}@${pi}:${dir}/asgard-lib/include/asgard/
	sshpass -p ${password} ssh -t ${user}@${pi} "cd ${dir} && make"

remote_make_run:
	sshpass -p ${password} scp -p Makefile ${user}@${pi}:${dir}/
	sshpass -p ${password} scp -p src/*.cpp ${user}@${pi}:${dir}/src/
	sshpass -p ${password} scp -p asgard-lib/include/asgard/*.hpp ${user}@${pi}:${dir}/asgard-lib/include/asgard/
	sshpass -p ${password} ssh -t ${user}@${pi} "cd ${dir} && make && make run"

remote_run:
	sshpass -p ${password} ssh -t ${user}@${pi} "cd ${dir} && make run"

clean: base_clean

include make-utils/cpp-utils-finalize.mk

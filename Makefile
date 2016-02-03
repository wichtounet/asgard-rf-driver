CXX=g++
LD=g++

user=pi
pi=192.168.20.161
password=raspberry
dir=/home/${user}/asgard/asgard-rf-driver/

default: release

.PHONY: default release debug all clean

include make-utils/flags-pi.mk
include make-utils/cpp-utils.mk

CXX_FLAGS += -pedantic -Irc-switch-rpi
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
	sshpass -p ${password} scp Makefile ${user}@${pi}:${dir}/
	sshpass -p ${password} scp src/*.cpp ${user}@${pi}:${dir}/src/
	sshpass -p ${password} ssh ${user}@${pi} "cd ${dir} && make"

remote_run:
	sshpass -p ${password} ssh ${user}@${pi} "cd ${dir} && make run"

clean: base_clean

include make-utils/cpp-utils-finalize.mk

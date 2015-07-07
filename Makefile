default: release

.PHONY: default release debug all clean

include make-utils/flags-pi.mk
include make-utils/cpp-utils.mk

CXX_FLAGS += -pedantic
LD_FLAGS  += -llrfc_client

$(eval $(call auto_folder_compile,src))
$(eval $(call auto_add_executable,rf_driver))

release: release_rf_driver
release_debug: release_debug_rf_driver
debug: debug_rf_driver

all: release release_debug debug

run: release
	./release/bin/rf_driver

clean: base_clean

include make-utils/cpp-utils-finalize.mk

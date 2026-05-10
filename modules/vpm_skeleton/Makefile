obj-m += vpm_skeleton.o
vpm_skeleton-y := vpm_main.o vpm_core.o vpm_debugfs.o vpm_mock_bus.o
KDIR ?= /lib/modules/$(shell uname -r)/build

SRC_DIR := $(CURDIR)
BUILD_DIR := $(abspath $(SRC_DIR)/../../build/vpm_skeleton)

all:
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(KDIR) M=$(SRC_DIR) MO=$(BUILD_DIR) modules

clean:
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(KDIR) M=$(SRC_DIR) MO=$(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)

obj-m += vpm_skeleton.o

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

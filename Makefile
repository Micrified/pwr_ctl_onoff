BINARY      := pwr_ctl_onoff
KERNEL      := /lib/modules/$(shell uname -r)/build
C_FLAGS     := -Wall
KMOD_DIR    := $(shell pwd)
TARGET_PATH := /lib/modules/$(shell uname -r)/kernel/drivers/char
KBUILD_EXTRA_SYMBOLS := $(shell pwd)/Module.symvers

# Flags for the C compiler
ccflags-y += $(C_FLAGS)

# Adds binary name (pwr_ctl_onoff.o) to obj-m variable
obj-m += $(BINARY).o

# Rule for making all
all: $(BINARY).ko test_$(BINARY)

# Rule for building pwr_ctl_onoff.ko
$(BINARY).ko:
	make -C $(KERNEL) M=$(KMOD_DIR) modules

# Rule for building the module test program
test_$(BINARY): test_pwr_ctl_onoff.c
	cc -o $@ $^ $(C_FLAGS)

# Install the new module into the character device module directory
install:
	cp $(BINARY).ko $(TARGET_PATH)
	depmod -a

# Remove the module from the character device module directory
uninstall:
	rm $(TARGET_PATH)/$(BINARY).ko
	depmod -a

# Invoke clean in kernel module site
clean:
	make -C $(KERNEL) M=$(KMOD_DIR) clean

ifneq ($(KERNELRELEASE),)
obj-m := secure_mailbox_module.o
else
KDIR ?= ~/Desktop/linux
ARCH ?= arm
CROSS_COMPILE ?=

all:
	$(MAKE) -C $(KDIR) M=$$PWD ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	$(MAKE) -C $(KDIR) M=$$PWD ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
endif



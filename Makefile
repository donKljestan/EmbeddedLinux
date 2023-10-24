ifneq ($(KERNELRELEASE),)
obj-m := secure_mailbox_module.o
else
KDIR := ~/Desktop/linux 
 
all:  
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif



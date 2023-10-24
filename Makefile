# KDIR, ARCH and CROSS_COMPILE are passed on to the subprojects, e.g.:
#   make KDIR=~/linux CROSS_COMPILE=arm-linux-gnueabihf-
.PHONY: all driver app clean

all: driver app

driver:
	$(MAKE) -C driver

app:
	$(MAKE) -C app release

clean:
	$(MAKE) -C app clean
	$(MAKE) -C driver clean

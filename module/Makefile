obj-m += virtual_fb.o

build-vfb-module:
	$(MAKE) -C /lib/modules/$$(uname -r)/build M=`pwd`

KERNDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ifneq ($(KERNELRELEASE),)
	obj-m := hello.o

else

default:
	$(MAKE) -C $(KERNDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.mod.* Module.symvers modules.order *.ko

endif

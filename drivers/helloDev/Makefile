ifneq ($(KERNELRELEASE),)
obj-m := hello.o
 
else 
 
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  :=$(shell pwd)
default:
	$(MAKE)  -C  $(KERNELDIR) M=$(PWD) modules 
clean:
	$(RM) *.o *.ko *.mod.c Module.symvers modules.order 

endif


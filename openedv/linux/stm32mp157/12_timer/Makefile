KERNELDIR := /home/alex/study/stm32mp157/01_Source_Code/01、正点原子Linux出厂系统源码/linux-5.4.31
CURRENT_PATH := $(shell pwd)

obj-m := timer.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

# Use below command to build kernel mode module.
 make COPTS="-g3 -O0"

# Use below command to build user mode AP.
 gcc -g3 -O0 -o testdriver testdriver.c

# 加载驱动
 insmod chardev.ko

# 建立节点
 mknod /dev/mychrdev c 247 0
 247: dmesg 可以看到

# 测试
 testdriver

# 卸载驱动
 rmmod chardev

# 功能描述
加载驱动的同时，自动创建字符设备

# 测试
```shell
insmod newchrled.ko
rmmod newchrled
echo 1 > /dev/newchrled
echo 0 > /dev/newchrled
```
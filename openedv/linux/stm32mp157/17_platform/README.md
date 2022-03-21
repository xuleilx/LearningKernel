
# 功能描述
简单的led驱动

# 测试
```shell
insmod leddriver.ko
insmod leddevice.ko 
rmmod leddriver leddevice
mknod /dev/led c 200 0
echo 1 > /dev/led
echo 0 > /dev/led
```
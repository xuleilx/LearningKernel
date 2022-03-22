
# 功能描述
使用platform驱动led点灯

# 测试
```shell
insmod leddriver.ko
insmod leddevice.ko 
rmmod leddriver leddevice
mknod /dev/led c 200 0
echo 1 > /dev/led
echo 0 > /dev/led
```
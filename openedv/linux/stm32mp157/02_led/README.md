```shell
insmod led.ko
rmmod led
mknod /dev/led c 200 0
echo 1 > /dev/led
echo 0 > /dev/led
```
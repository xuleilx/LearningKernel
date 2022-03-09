```shell
insmod newchrled.ko
rmmod newchrled
echo 1 > /dev/newchrled
echo 0 > /dev/newchrled
```
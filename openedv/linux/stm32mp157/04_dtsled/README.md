```shell
insmod newchrled.ko
rmmod newchrled
echo 1 > /dev/newchrled
echo 0 > /dev/newchrled
```

```dts
    stm32mp1_led{
        compatible = "atkstm32mp1-led";
        status = "okay";
        reg = <0x50000A28 0x04 /* RCC_MP_AHB4ENSETR */
               0x5000A000 0x04 /* GPIOI_MODER */
               0x5000A004 0x04 /* GPIOI_OTYPER */
               0x5000A008 0x04 /* GPIOI_OSPEEDR */
               0x5000A00C 0x04 /* GPIOI_PUPDR */
               0x5000A018 0x04 >; /* GPIOI_BSRR */
    };
```
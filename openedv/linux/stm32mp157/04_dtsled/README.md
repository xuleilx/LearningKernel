# 功能描述
采用设备树配置gpio相关寄存器的地址信息

# 设备树
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

# 测试
```shell
insmod dtsled.ko
rmmod dtsled
echo 1 > /dev/dtsled
echo 0 > /dev/dtsled
```
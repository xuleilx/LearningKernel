
# 功能描述
1.通过dts加载platform设备驱动
2.使用misc子系统生产/dev/icm20608节点
3.使用spi子系统实现icm20608设备通信

# 设备树
## 添加 pinctrl 节点
stm32mp15-pinctrl.dtsi
参考Documentation/devicetree/bindings/pinctrl/st,stm32-pinctrl.yaml
```dts
&pinctrl{
	spi1_pins_a: spi1-0 {
		pins1 {
			pinmux = <STM32_PINMUX('Z', 0, AF5)>, /* SPI1_SCK */
                    <STM32_PINMUX('Z', 1, AF5)>,    /* SPI1_MISO */
				    <STM32_PINMUX('Z', 2, AF5)>; /* SPI1_MOSI */
			bias-disable;
			drive-push-pull;
			slew-rate = <1>;
		};

        pins2 {
            pinmux = <STM32_PINMUX('Z', 3, AF5)>; /* SPI1_NSS */
            drive-push-pull;
            bias-pull-up;
            output-high;
            slew-rate = <0>;
        };
	};

	spi1_sleep_pins_a: spi1-sleep-0 {
		pins {
			pinmux = <STM32_PINMUX('Z', 0, ANALOG)>, /* SPI1_SCK */
				 <STM32_PINMUX('Z', 1, ANALOG)>, /* SPI1_MISO */
				 <STM32_PINMUX('Z', 2, ANALOG)>, /* SPI1_MOSI */
                 <STM32_PINMUX('Z', 3, ANALOG)>; /* SPI1_NSS */
		};
	};
};
```
## 添加 key 设备节点
arch/arm/boot/dts/stm32mp157d-atk.dts
```dts
&spi1 {
   pinctrl-names = "default", "sleep";
   pinctrl-0 = <&spi1_pins_a>;
   pinctrl-1 = <&spi1_sleep_pins_a>;
   cs-gpios = <&gpioz 3 GPIO_ACTIVE_LOW>;
   status = "okay";

    spidev: icm20608@0 {
        compatible = "alientek,icm20608";
        reg = <0>; /* CS #0 */
        spi-max-frequency = <8000000>;
    };
};
```

# 测试
```shell
insmod icm20608.ko
rmmod icm20608
./icm20608App /dev/icm20608
cat /sys/devices/virtual/misc/icm20608/temp // temperature
```
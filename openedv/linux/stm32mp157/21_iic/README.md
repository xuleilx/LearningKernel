
# 功能描述
1.通过dts加载platform设备驱动
2.使用misc子系统生产/dev/ap3216c节点
3.使用i2c子系统实现ap3216c设备通信

# 设备树
## 添加 pinctrl 节点
stm32mp15-pinctrl.dtsi
参考Documentation/devicetree/bindings/pinctrl/st,stm32-pinctrl.yaml
```dts
&pinctrl{
	i2c5_pins_a: i2c5-0 {
		pins {
			pinmux = <STM32_PINMUX('A', 11, AF4)>, /* I2C5_SCL */
				 <STM32_PINMUX('A', 12, AF4)>; /* I2C5_SDA */
			bias-disable;
			drive-open-drain;
			slew-rate = <0>;
		};
	};

	i2c5_pins_sleep_a: i2c5-1 {
		pins {
			pinmux = <STM32_PINMUX('A', 11, ANALOG)>, /* I2C5_SCL */
				 <STM32_PINMUX('A', 12, ANALOG)>; /* I2C5_SDA */

		};
	};
};
```
## 添加 key 设备节点
arch/arm/boot/dts/stm32mp157d-atk.dts
```dts
&i2c5 {
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&i2c5_pins_a>;
    pinctrl-1 = <&i2c5_pins_sleep_a>;
    i2c-scl-rising-time-ns = <100>;
    i2c-scl-falling-time-ns = <7>;
    status = "okay";
    /delete-property/dmas;
    /delete-property/dma-names;

    ap3216c@1e {
		compatible = "LiteOn,ap3216c";
        reg = <0x1e>;
	};
};
```

# 测试
```shell
insmod ap3216c.ko
rmmod ap3216c
./ap3216cApp /dev/ap3216c
cat /sys/devices/virtual/misc/ap3216c/ir 
```
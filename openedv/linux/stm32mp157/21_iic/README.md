
# 功能描述
1.通过dts加载platform设备驱动
2.使用input子系统生成/dev/input/eventX字符设备
3.使用gpio控制key按键，模拟KEY_0

# 设备树
## 添加 pinctrl 节点
stm32mp15-pinctrl.dtsi
参考Documentation/devicetree/bindings/pinctrl/st,stm32-pinctrl.yaml
```dts
&pinctrl{
    /* 定义引脚作为GPIO时的复用 */
	key_pins_a: key_pins-0 {
		pins1{
			pinmux = <STM32_PINMUX('G', 3, GPIO)>, 	/* KEY0 */
					<STM32_PINMUX('H', 7, GPIO)>;	/* KEY1 */
			bias-pull-up;
			slew-rate = <0>;
		};
		pins2{
			pinmux = <STM32_PINMUX('A', 0, GPIO)>; /* WK_UP */
			bias-pull-down;
			slew-rate = <0>;
		};
	};
};
```
## 添加 key 设备节点
arch/arm/boot/dts/stm32mp157d-atk.dts
```dts
    key {
        compatible = "alex,key";
        gpios = <&gpiog 3 GPIO_ACTIVE_LOW>;
        status = "okay";
        pinctrl-names = "default";
        pinctrl-0 = <&key_pins_a>;
        interrupt-parent = <&gpiog>;
        interrupts = <3 IRQ_TYPE_EDGE_BOTH>;
    };
```

# 测试
```shell
insmod keyinput.ko
rmmod keyinput
hexdump /dev/input/eventX
```
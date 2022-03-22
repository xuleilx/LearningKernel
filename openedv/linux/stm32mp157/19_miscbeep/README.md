
# 功能描述
1.通过dts加载platform设备驱动
2.使用misc生成字符设备
3.使用gpio控制beep响铃

# 设备树
## 添加 pinctrl 节点
stm32mp15-pinctrl.dtsi
参考Documentation/devicetree/bindings/pinctrl/st,stm32-pinctrl.yaml
```dts
&pinctrl{
    /* 定义引脚作为GPIO时的复用 */
    beep_pins_a: beep_pins {    
        pins {
            pinmux = <STM32_PINMUX('C', 7, GPIO)>; /* BEEP */
            drive-push-pull;
            bias-pull-up;
            output-high;
            slew-rate = <0>;
        };
    };
};
```
## 添加 beep 设备节点
arch/arm/boot/dts/stm32mp157d-atk.dts
注意`&gpioc 7`冲突的问题，多处应用需要屏蔽
```dts
beep {
    compatible = "alientek,beep";
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&beep_pins_a>;
    beep-gpio = <&gpioc 7 GPIO_ACTIVE_HIGH>;
};
```

# 测试
```shell
insmod miscbeep.ko
rmmod miscbeep
mknod /dev/led c 200 0
echo 1 > /dev/miscbeep
echo 0 > /dev/miscbeep
```
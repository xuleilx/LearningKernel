# 功能描述
1. 采用pinctrl子系统配置电子电气属性
2. 采用gpio子系统读写寄存器
3. 优点：驱动分层，便于移植

# 设备树
Documentation/devicetree/bindings/gpio/gpio.txt
Documentation/devicetree/bindings/pinctrl/pinctrl-bindings.txt

```text
**定义**
pinctrl
  ->gpiog
  ->sdmmc1_b4_pins_a
**使用**
sdmmc1
  -> pinctrl-0 
    ->sdmmc1_b4_pins_a
  -> cd-gpios
    -> gpiog
```
在pinctrl节点下定义gpio和pin的复用配置，然后定义gpioled驱动的节点，并且引用到pinctrl中相关的配置。

按道理来讲，当我们将一个 IO 用作 GPIO 功能的时候也需要创建对应的 pinctrl 节点，并且
将所用的 IO 复用为 GPIO 功能，比如将 PI0 复用为 GPIO 的时候就需要设置 pinmux 属性为：
<STM32_PINMUX('I', 0, GPIO)>，但是！对于 STM32MP1 而言，如果一个 IO 用作 GPIO 功能
的时候不需要创建对应的 pinctrl 节点！

## 需要添加的
```dts
    key {
        compatible = "alex,key";
        gpios = <&gpiog 3 GPIO_ACTIVE_LOW>;
        status = "okay";
        /* 此处没有定义pin的引用情况 */
    };
```
## pinctrl
gpioi 控制器节点 stm32mp151.dtsi
```dts
pinctrl: pin-controller@50002000 {
    #address-cells = <1>;
    #size-cells = <1>;
    compatible = "st,stm32mp157-pinctrl";
    ......
    gpioi: gpio@5000a000 {
        gpio-controller;
        #gpio-cells = <2>;
        interrupt-controller;
        #interrupt-cells = <2>;
        reg = <0x8000 0x400>;
        clocks = <&rcc GPIOI>;
        st,bank-name = "GPIOI";
        status = "disabled";
    };
};
```
stm32mp15-pinctrl.dtsi
```dts
&pinctrl{
    /* 此处没有定义引脚作为GPIO时的复用 */
}
```

# 测试
```shell
insmod key.ko
rmmod key
hexdump -e '"%02x\n"' /dev/keydev
```
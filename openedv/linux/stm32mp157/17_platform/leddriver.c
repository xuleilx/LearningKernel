#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/platform_device.h>

#define LED_MAJOR 200  /* 主设备号 */
#define LED_NAME "led" /* 设备名字 */

#define LEDOFF 0 /* 关灯 */
#define LEDON 1  /* 开灯 */

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(char *sta)
{
    u32 val = 0;
    char *endptr;
    int base = 10;
    if (simple_strtoul(sta, &endptr, base) == LEDON)
    {
        printk("LEDON\n");
        val = readl(GPIOI_BSRR_PI);
        val |= (1 << 16);
        writel(val, GPIOI_BSRR_PI);
    }
    else if (simple_strtoul(sta, &endptr, base) == LEDOFF)
    {
        printk("LEDOFF\n");
        val = readl(GPIOI_BSRR_PI);
        val |= (1 << 0);
        writel(val, GPIOI_BSRR_PI);
    }
}

/*
 * @description		: 取消映射
 * @return 			: 无
 */
void led_unmap(void)
{
    /* 取消映射 */
    iounmap(MPU_AHB4_PERIPH_RCC_PI);
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * @description		: 从设备读取数据
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description		: 向设备写数据
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[128];
    // unsigned char ledstat;

    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    // ledstat = databuf[0]; /* 获取状态值 */

    led_switch(databuf);

    return cnt;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int led_probe(struct platform_device *pdev)
{
    int retvalue = 0;
    u32 val = 0;
    struct device *dev = &pdev->dev;
    struct resource *mem;
    /* 初始化LED */
    /* 1、寄存器地址映射 */
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    MPU_AHB4_PERIPH_RCC_PI = ioremap(mem->start, resource_size(mem));
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    GPIOI_MODER_PI = ioremap(mem->start, resource_size(mem));
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    GPIOI_OTYPER_PI = ioremap(mem->start, resource_size(mem));
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 3);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    GPIOI_OSPEEDR_PI = ioremap(mem->start, resource_size(mem));
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 4);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    GPIOI_PUPDR_PI = ioremap(mem->start, resource_size(mem));
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 5);
    if (!mem)
    {
        dev_err(dev, "Unable to get mmio space\n");
        return -EINVAL;
    }
    GPIOI_BSRR_PI = ioremap(mem->start, resource_size(mem));

    /* 2、使能PI时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的设置 */
    val |= (0X1 << 8);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 3、设置PI0通用的输出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0);  /* bit0:1设置01 */
    writel(val, GPIOI_MODER_PI);

    /* 3、设置PI0为推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，设置为上拉*/
    writel(val, GPIOI_OTYPER_PI);

    /* 4、设置PI0为高速。*/
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x2 << 0);  /* bit0:1 设置为10*/
    writel(val, GPIOI_OSPEEDR_PI);

    /* 5、设置PI0为上拉。*/
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零*/
    val |= (0x1 << 0);  /*bit0:1 设置为01*/
    writel(val, GPIOI_PUPDR_PI);

    /* 6、默认关闭LED */
    val = readl(GPIOI_BSRR_PI);
    val |= (0x1 << 0);
    writel(val, GPIOI_BSRR_PI);

    /* 6、注册字符设备驱动 */
    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if (retvalue < 0)
    {
        printk("register chrdev failed!\r\n");
        goto fail_map;
    }
    return 0;

fail_map:
    led_unmap();
    return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int led_remove(struct platform_device *dev)
{
    /* 取消映射 */
    led_unmap();

    /* 注销字符设备驱动 */
    unregister_chrdev(LED_MAJOR, LED_NAME);
    return 0;
}

static struct platform_driver led_driver = {
    .driver = {
        .name = "stm32mp1-led",
    },
    .probe = led_probe,
    .remove = led_remove,
};

static int __init leddriver_init(void)
{
    return platform_driver_register(&led_driver);
}

static void __exit leddriver_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex.xu");
MODULE_INFO(intree, "Y");

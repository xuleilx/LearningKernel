#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define NEWCHRLED_CNT 1
#define NEWCHRLED_NAME "dtsled"
#define LEDOFF 0
#define LEDON 1

/* ioremap from physical address to logical address */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

struct newchrled_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
};

struct newchrled_dev dtsled;

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

void led_unmap(void)
{
    iounmap(MPU_AHB4_PERIPH_RCC_PI);
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}

void led_remap(void)
{
    int ret;
    u32 regdata[12];
    const char *str;
    struct property *property;
    dtsled.nd = of_find_node_by_path("/stm32mp1_led");
    if(dtsled.nd == NULL){
        printk("stm32mp1_led node no find!\n");
    }else{
        printk("stm32mp1_led node find!\n");
    }
    property = of_find_property(dtsled.nd,"compatible",NULL);
    if(property == NULL){
        printk("compatible property find failed!\n");
    }else{
        printk("compatible = %s\n",(char*)property->value);
    }
    ret = of_property_read_string(dtsled.nd,"status",&str);
    if(ret < 0){
        printk("status read failed!\n");
    }else{
        printk("status = %s\n",str);
    }

    ret = of_property_read_u32_array(dtsled.nd,"reg",regdata,12);
    if(ret < 0){
        printk("reg property read failed!\n");
    }else{
        u8 i = 0;
        printk("reg data:\n");
        for(i=0; i<12; i++){
            printk("%#X ",regdata[i]);
        }
        printk("\n");
    }


    MPU_AHB4_PERIPH_RCC_PI = of_iomap(dtsled.nd, 0);
    GPIOI_MODER_PI = of_iomap(dtsled.nd, 1);
    GPIOI_OTYPER_PI = of_iomap(dtsled.nd, 2);
    GPIOI_OSPEEDR_PI = of_iomap(dtsled.nd, 3);
    GPIOI_PUPDR_PI = of_iomap(dtsled.nd, 4);
    GPIOI_BSRR_PI = of_iomap(dtsled.nd, 5);
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offset)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offset)
{
    int retvalue;
    unsigned char databuf[32];

    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("Kernel write failed\n");
        return -EFAULT;
    }
    led_switch(databuf);

    return cnt;
}

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

static int __init
led_init(void)
{
    u32 val = 0;
    int ret;

    /* remap physical address to logical address */
    led_remap();

    /* enable clock */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0x1 << 8);
    val |= (0x1 << 8);
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* output mode */
    val = readl(GPIOI_MODER_PI);
    val &= ~(0x3 << 0);
    val |= (0x1 << 0);
    writel(val, GPIOI_MODER_PI);

    /* push/pull mode */
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0x1 << 0);
    writel(val, GPIOI_OTYPER_PI);

    /* speed */
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0x3 << 0);
    val |= (0x2 << 0);
    writel(val, GPIOI_OSPEEDR_PI);

    /* pull up */
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0x3 << 0);
    val |= (0x1 << 0);
    writel(val, GPIOI_PUPDR_PI);

    /* close led */
    val = readl(GPIOI_BSRR_PI);
    val |= ~(0x1 << 0);
    writel(val, GPIOI_BSRR_PI);

    if (dtsled.major)
    {
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if (ret < 0)
        {
            pr_err("cannot register %s char driver [ret=%d]\n", NEWCHRLED_NAME, NEWCHRLED_CNT);
            goto fail_map;
        }
    }else{
        ret = alloc_chrdev_region(&dtsled.devid,0,NEWCHRLED_CNT,NEWCHRLED_NAME);
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", NEWCHRLED_NAME, ret);
			goto fail_map;
		}
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
	printk("dtsled major=%d,minor=%d\r\n",dtsled.major, dtsled.minor);	

    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev,&newchrled_fops);

    ret = cdev_add(&dtsled.cdev,dtsled.devid,NEWCHRLED_CNT);
    if(ret<0){
        goto del_unregister;
    }

    dtsled.class = class_create(THIS_MODULE,NEWCHRLED_NAME);
	if (IS_ERR(dtsled.class)) {
		goto del_cdev;
	}

    dtsled.device = device_create(dtsled.class,NULL,dtsled.devid,NULL,NEWCHRLED_NAME);
	if (IS_ERR(dtsled.device)) {
		goto destroy_class;
	}
	
	return 0;

destroy_class:
	class_destroy(dtsled.class);
del_cdev:
	cdev_del(&dtsled.cdev);
del_unregister:
	unregister_chrdev_region(dtsled.devid, NEWCHRLED_CNT);
fail_map:
	led_unmap();
	return -EIO;
}

static void __exit led_exit(void){
    led_unmap();

    cdev_del(&dtsled.cdev);
    unregister_chrdev_region(dtsled.devid,NEWCHRLED_CNT);

    device_destroy(dtsled.class,dtsled.devid);
    class_destroy(dtsled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree,"Y");
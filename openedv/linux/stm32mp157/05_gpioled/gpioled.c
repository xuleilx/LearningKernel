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
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define GPIOLED_CNT 1
#define GPIOLED_NAME "gpioled"
#define LEDOFF 0
#define LEDON 1

struct gpioled_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int led_gpio;
};

struct gpioled_dev gpioled;

int gpioled_parse(void)
{
    int ret = 0;
    const char *str;

    gpioled.nd = of_find_node_by_path("/gpioled");
    if (gpioled.nd == NULL)
    {
        printk("gpioled node not find!\n");
        return -EINVAL;
    }

    ret = of_property_read_string(gpioled.nd, "status", &str);
    if (ret < 0)
    {
        printk("status not find!\n");
        return -EINVAL;
    }
    if (strcmp(str, "okay"))
    {
        printk("status is not ok! %s\n",str);
        return -EINVAL;
    }

    ret = of_property_read_string(gpioled.nd, "compatible", &str);
    if (ret < 0)
    {
        printk("compatible not find!\n");
        return -EINVAL;
    }
    if (strcmp(str, "alex,led"))
    {
        printk("compatible is not ok!\n");
        return -EINVAL;
    }

    gpioled.led_gpio = of_get_named_gpio(gpioled.nd,"led-gpio",0);
    if(gpioled.led_gpio < 0){
        printk("cannot get leg-gpio\n");
        return -EINVAL;
    }
    printk("leg-gpio num = %d\n",gpioled.led_gpio);

    ret = gpio_request(gpioled.led_gpio,GPIOLED_NAME);
    if(ret < 0){
        printk(KERN_ERR "gpio_request failed\n");
        return ret;
    }

    ret = gpio_direction_output(gpioled.led_gpio,1);
    if(ret< 0){
        printk("cannot set gpio gpio_direction_output!\n");
    }

    return 0;
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &gpioled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offset)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offset)
{
    unsigned long ret = 0;
    unsigned long val;
    struct gpioled_dev *dev = filp->private_data;

    ret = kstrtoul_from_user(buf, cnt, 10, &val);
    if (ret)
        return ret;

    printk("%s: %ld\n",__func__, val);
    if (val == LEDON)
    {
        gpio_set_value(dev->led_gpio, 0);/* 打开 LED 灯 */
    }
    else if (val == LEDOFF)
    {
        gpio_set_value(dev->led_gpio, 1);
    }

    return cnt;
}

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

static int __init
led_init(void)
{
    int ret;

    /* remap physical address to logical address */
    gpioled_parse();

    if (gpioled.major)
    {
        gpioled.devid = MKDEV(gpioled.major, 0);
        ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        if (ret < 0)
        {
            pr_err("cannot register %s char driver [ret=%d]\n", GPIOLED_NAME, GPIOLED_CNT);
            goto free_gpio;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        if (ret < 0)
        {
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", GPIOLED_NAME, ret);
            goto free_gpio;
        }
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("gpioled major=%d,minor=%d\r\n", gpioled.major, gpioled.minor);

    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);

    ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
    if (ret < 0)
    {
        goto del_unregister;
    }

    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class))
    {
        goto del_cdev;
    }

    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(gpioled.device))
    {
        goto destroy_class;
    }

    return 0;

destroy_class:
    class_destroy(gpioled.class);
del_cdev:
    cdev_del(&gpioled.cdev);
del_unregister:
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
free_gpio:
    gpio_free(gpioled.led_gpio);
    return -EIO;
}

static void __exit led_exit(void)
{
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);
    gpio_free(gpioled.led_gpio);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree, "Y");
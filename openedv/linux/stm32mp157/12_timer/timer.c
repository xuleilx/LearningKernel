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
#include <linux/semaphore.h>
#include <linux/timer.h>

#define TIMER_CNT 1
#define TIMER_NAME "timer"
#define CLOSE_CMD (_IO(0xEF,0x1))
#define OPEN_CMD (_IO(0xEF,0x2))
#define SETPERIOD_CMD (_IO(0xEF,0x3))
#define LEDON   1
#define LEDOFF  0

struct timer_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int led_gpio;
    int timeperiod;
    struct timer_list timer;
    spinlock_t lock;
};

struct timer_dev timerdev;

static int led_parse(void)
{
    int ret = 0;
    const char *str;

    timerdev.nd = of_find_node_by_path("/gpioled");
    if (timerdev.nd == NULL)
    {
        printk("timerdev node not find!\n");
        return -EINVAL;
    }

    ret = of_property_read_string(timerdev.nd, "status", &str);
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

    ret = of_property_read_string(timerdev.nd, "compatible", &str);
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

    timerdev.led_gpio = of_get_named_gpio(timerdev.nd,"led-gpio",0);
    if(timerdev.led_gpio < 0){
        printk("cannot get leg-gpio\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\n",timerdev.led_gpio);

    ret = gpio_request(timerdev.led_gpio,TIMER_NAME);
    if(ret < 0){
        printk(KERN_ERR "gpio_request failed\n");
        return ret;
    }

    ret = gpio_direction_output(timerdev.led_gpio,1);
    if(ret< 0){
        printk("cannot set gpio gpio_direction_output!\n");
    }

    return 0;
}

static int timer_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &timerdev;

    timerdev.timeperiod = 1000;
    ret = led_parse();
    return ret;
}

static long timer_unlocked_ioctl(struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    unsigned long flags;

    switch (cmd){
        case CLOSE_CMD:
        del_timer_sync(&dev->timer);
        break;
        case OPEN_CMD:
        mod_timer(&dev->timer,jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
        case SETPERIOD_CMD:
        spin_lock_irqsave(&dev->lock, flags);
        dev->timeperiod = arg;
        spin_unlock_irqrestore(&dev->lock, flags);
        mod_timer(&dev->timer,jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
        default:
        break;
    }
	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
    struct timer_dev *dev = filp->private_data;
    gpio_set_value(dev->led_gpio,1);
    gpio_free(dev->led_gpio);
    del_timer_sync(&dev->timer);

    return 0;
}

static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .unlocked_ioctl = timer_unlocked_ioctl,
    .release = led_release,
};

void timer_function(struct timer_list *args) {
    struct timer_dev *dev = from_timer(dev,args,timer);
    static int sta = 1;
    sta = !sta;
    gpio_set_value(dev->led_gpio,sta);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
}

static int __init timer_init(void)
{
    int ret;

    spin_lock_init(&timerdev.lock);

    if (timerdev.major)
    {
        timerdev.devid = MKDEV(timerdev.major, 0);
        ret = register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
        if (ret < 0)
        {
            pr_err("cannot register %s char driver [ret=%d]\n", TIMER_NAME, TIMER_CNT);
            goto err;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        if (ret < 0)
        {
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", TIMER_NAME, ret);
            goto err;
        }
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    printk("timerdev major=%d,minor=%d\r\n", timerdev.major, timerdev.minor);

    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timer_fops);

    ret = cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
    if (ret < 0)
    {
        goto del_unregister;
    }

    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if (IS_ERR(timerdev.class))
    {
        goto del_cdev;
    }

    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if (IS_ERR(timerdev.device))
    {
        goto destroy_class;
    }
    
    timer_setup(&timerdev.timer,timer_function,0);

    return 0;

destroy_class:
    class_destroy(timerdev.class);
del_cdev:
    cdev_del(&timerdev.cdev);
del_unregister:
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);
err:
    return -EIO;
}

static void __exit mykey_exit(void)
{
    del_timer_sync(&timerdev.timer);
    cdev_del(&timerdev.cdev);
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);
    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree, "Y");
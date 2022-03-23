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
#include <linux/of_irq.h>
#include <linux/irq.h>

#define KEY_CNT 1
#define KEY_NAME "keyirq"

enum key_status{
    KEY_PRESS = 0,
    KEY_RELEASE,
    KEY_NONE,
};

struct key_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    int key_gpio;
    struct timer_list timer;
    int irq_num;
    spinlock_t spinlock;
    struct tasklet_struct tasklet;
    int status;
};

struct key_dev keydev={
    .status = KEY_NONE,
};

static irqreturn_t key_interrupt_handler(int irq, void *dev_id)
{
    tasklet_schedule(&keydev.tasklet);
    return IRQ_HANDLED;
}
static int key_parse(void)
{
    int ret = 0;
    const char *str;

    keydev.nd = of_find_node_by_path("/key");
    if (keydev.nd == NULL)
    {
        printk("keydev node not find!\n");
        return -EINVAL;
    }

    ret = of_property_read_string(keydev.nd, "status", &str);
    if (ret < 0)
    {
        printk("status not find!\n");
        return -EINVAL;
    }
    if (strcmp(str, "okay"))
    {
        printk("status is not ok! %s\n", str);
        return -EINVAL;
    }

    ret = of_property_read_string(keydev.nd, "compatible", &str);
    if (ret < 0)
    {
        printk("compatible not find!\n");
        return -EINVAL;
    }
    if (strcmp(str, "alex,key"))
    {
        printk("compatible is not ok!\n");
        return -EINVAL;
    }

    keydev.key_gpio = of_get_named_gpio(keydev.nd, "gpios", 0);
    if (keydev.key_gpio < 0)
    {
        printk("cannot get gpios\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\n", keydev.key_gpio);

    keydev.irq_num = irq_of_parse_and_map(keydev.nd, 0);
    if (!keydev.irq_num)
    {
        return -EINVAL;
    }

    printk("irq_num num = %d\r\n", keydev.irq_num);

    return 0;
}

static int key_gpio_init(void)
{
    int ret;
    unsigned long irq_flags;
    ret = gpio_request(keydev.key_gpio, KEY_NAME);
    if (ret < 0)
    {
        printk(KERN_ERR "gpio_request failed\n");
        return ret;
    }

    ret = gpio_direction_input(keydev.key_gpio);
    if (ret < 0)
    {
        printk("cannot set gpio gpio_direction_input!\n");
        return ret;
    }

    irq_flags = irq_get_trigger_type(keydev.irq_num);
    if (IRQF_TRIGGER_NONE == irq_flags)
    {
        irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
    }

    ret = request_irq(keydev.irq_num, key_interrupt_handler, irq_flags, "Key0_IRQ", NULL);
    if (ret)
    {
        gpio_free(keydev.key_gpio);
        return ret;
    }
    return 0;
}

static int key_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &keydev;
    return ret;
}

static int key_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t kery_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offset)
{
    unsigned long flags;
    int ret = 0;
    struct key_dev *dev = filp->private_data;
    spin_lock_irqsave(&dev->spinlock, flags);
    ret = copy_to_user(buf, &dev->status, sizeof(dev->status));
    if (ret)
    {
        ret = -EFAULT;
        goto err;
    }
    dev->status = KEY_NONE;
    ret = sizeof(dev->status);
err:
    spin_unlock_irqrestore(&dev->spinlock, flags);

    return ret;
}

static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = kery_read,
    .release = key_release,
};

static void timer_function(struct timer_list *args)
{
    struct key_dev *dev = from_timer(dev, args, timer);
    unsigned long flags;
    int cur_value ; 
    static int last_value = 1;

    spin_lock_irqsave(&dev->spinlock,flags);
    cur_value = gpio_get_value(dev->key_gpio);
    if(0 == cur_value && cur_value != last_value ){
        dev->status = KEY_PRESS;
    }else if(1 == cur_value && cur_value != last_value){
        dev->status = KEY_RELEASE;
    }else{
        dev->status = KEY_NONE;
    }
    last_value = cur_value;
    spin_unlock_irqrestore(&dev->spinlock, flags);

}

static void key_irq_tasklet(unsigned long drv)
{
	struct key_dev *dev = (struct key_dev *)drv;
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(15));
}

static int __init mykey_init(void)
{
    int ret;

    spin_lock_init(&keydev.spinlock);
    ret = key_parse();
    if (ret != 0)
    {
        printk("key_parse failed\n");
        return ret;
    }
    ret = key_gpio_init();
    if (ret != 0)
    {
        printk("key_gpio_init failed\n");
        return ret;
    }
    tasklet_init(&keydev.tasklet, key_irq_tasklet, (unsigned long)&keydev);

    ret = alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
    if (ret < 0)
    {
        pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY_NAME, ret);
        goto free_gpio;
    }

    printk("keydev major=%d,minor=%d\r\n", MAJOR(keydev.devid), MINOR(keydev.devid));

    keydev.cdev.owner = THIS_MODULE;
    cdev_init(&keydev.cdev, &timer_fops);

    ret = cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);
    if (ret < 0)
    {
        goto del_unregister;
    }

    keydev.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(keydev.class))
    {
        goto del_cdev;
    }

    keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
    if (IS_ERR(keydev.device))
    {
        goto destroy_class;
    }

    timer_setup(&keydev.timer, timer_function, 0);

    return 0;

destroy_class:
    class_destroy(keydev.class);
del_cdev:
    cdev_del(&keydev.cdev);
del_unregister:
    unregister_chrdev_region(keydev.devid, KEY_CNT);
free_gpio:
    free_irq(keydev.irq_num, NULL);
    gpio_free(keydev.key_gpio);
    return -EIO;
}

static void __exit mykey_exit(void)
{
    del_timer_sync(&keydev.timer);
    cdev_del(&keydev.cdev);
    unregister_chrdev_region(keydev.devid, KEY_CNT);
    device_destroy(keydev.class, keydev.devid);
    class_destroy(keydev.class);
    free_irq(keydev.irq_num, NULL);
    gpio_free(keydev.key_gpio);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree, "Y");
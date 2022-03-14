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

#define KEY_CNT 1
#define KEY_NAME "keydev"
#define KEY0_VALUE 0xF0
#define INVALKEY 0x00

struct key_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int key_gpio;
    atomic_t keyvalue;
};

struct key_dev keydev;

static int gpiokey_parse(void)
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
        printk("status is not ok! %s\n",str);
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

    keydev.key_gpio = of_get_named_gpio(keydev.nd,"gpios",0);
    if(keydev.key_gpio < 0){
        printk("cannot get leg-gpio\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\n",keydev.key_gpio);

    ret = gpio_request(keydev.key_gpio,KEY_NAME);
    if(ret < 0){
        printk(KERN_ERR "gpio_request failed\n");
        return ret;
    }

    ret = gpio_direction_input(keydev.key_gpio);
    if(ret< 0){
        printk("cannot set gpio gpio_direction_output!\n");
    }

    return 0;
}

static int key_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &keydev;
    ret = gpiokey_parse();
    return ret;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offset)
{
    int value;
    int ret;
    struct key_dev *keydev = filp->private_data;

    if(gpio_get_value(keydev->key_gpio) == 0 ){
        while(!gpio_get_value(keydev->key_gpio));
        atomic_set(&keydev->keyvalue,KEY0_VALUE);
    }else{
        atomic_set(&keydev->keyvalue,INVALKEY);
    }

    value = atomic_read(&keydev->keyvalue);
	if(copy_to_user(buf, &value, sizeof(value))) {
		ret = -EFAULT;
		goto out;
	}
 	ret = sizeof(value);
out:
    return ret;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offset)
{
    return -EINVAL;
}

static int key_release(struct inode *inode, struct file *filp)
{
    struct key_dev *keydev = filp->private_data;
    gpio_free(keydev->key_gpio);
    return 0;
}

static struct file_operations gpiokey_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
    .write = key_write,
    .release = key_release,
};

static int __init mykey_init(void)
{
    int ret;

    keydev.keyvalue = (atomic_t)ATOMIC_INIT(0);

    if (keydev.major)
    {
        keydev.devid = MKDEV(keydev.major, 0);
        ret = register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
        if (ret < 0)
        {
            pr_err("cannot register %s char driver [ret=%d]\n", KEY_NAME, KEY_CNT);
            goto err;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
        if (ret < 0)
        {
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY_NAME, ret);
            goto err;
        }
        keydev.major = MAJOR(keydev.devid);
        keydev.minor = MINOR(keydev.devid);
    }
    printk("keydev major=%d,minor=%d\r\n", keydev.major, keydev.minor);

    keydev.cdev.owner = THIS_MODULE;
    cdev_init(&keydev.cdev, &gpiokey_fops);

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
    
    return 0;

destroy_class:
    class_destroy(keydev.class);
del_cdev:
    cdev_del(&keydev.cdev);
del_unregister:
    unregister_chrdev_region(keydev.devid, KEY_CNT);
err:
    return -EIO;
}

static void __exit mykey_exit(void)
{
    cdev_del(&keydev.cdev);
    unregister_chrdev_region(keydev.devid, KEY_CNT);
    device_destroy(keydev.class, keydev.devid);
    class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree, "Y");
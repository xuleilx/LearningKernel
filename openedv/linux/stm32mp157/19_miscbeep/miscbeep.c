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
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>

#define MISCBEEP_NAME "miscbeep"

struct beep_device
{
	int gpio;
};

static struct beep_device beep_dev;

static int miscbeep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep_dev;
	return 0;
}
static ssize_t miscbeep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offset)
{
	struct beep_device *dev = filp->private_data;
	int ret;
	unsigned long value;
	ret = kstrtoul_from_user(buf, cnt, 0, &value);
	if (ret)
		return ret;

	gpio_set_value(dev->gpio, !value);

	return cnt;
}
static int miscbeep_release(struct inode *inode, struct file *filep)
{
	return 0;
}

static const struct file_operations miscbeep_fops = {
	.owner = THIS_MODULE,
	.open = miscbeep_open,
	.write = miscbeep_write,
	.release = miscbeep_release,
};

static struct miscdevice beep_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MISCBEEP_NAME,
	.fops = &miscbeep_fops,
};

static int miscbeep_gpio_init(struct device_node *nd)
{
	int ret = 0;
	beep_dev.gpio = of_get_named_gpio(nd, "gpios", 0);
	if (!gpio_is_valid(beep_dev.gpio))
	{
		printk("miscbeep: Failed to get beep-gpio\n");
		return -EINVAL;
	}

	ret = gpio_request(beep_dev.gpio, "miscbeep");
	if (ret)
	{
		printk("miscbeep: Failed to request beep-gpio\n");
		return ret;
	}

	gpio_direction_output(beep_dev.gpio, 1);

	return 0;
}

int miscbeep_probe(struct platform_device *pdev)
{
	int ret = 0;
	ret = miscbeep_gpio_init(pdev->dev.of_node);
	if (ret < 0)
	{
		return ret;
	}

	ret = misc_register(&beep_miscdev);
	if (ret < 0)
	{
		printk("miscbeep: Failed to misc_register\n");
		goto free_gpio;
	}
free_gpio:
	gpio_free(beep_dev.gpio);
	return 0;
}
int miscbeep_remove(struct platform_device *pdev)
{

	gpio_set_value(beep_dev.gpio, 1);
	gpio_free(beep_dev.gpio);
	misc_deregister(&beep_miscdev);
	return 0;
}

struct of_device_id miscbeep_of_match[] = {
	{.compatible = "alex,beep"},
	{},
};

static struct platform_driver miscbeep_driver = {
	.driver = {
		.name = "stm32mp1,beep",
		.of_match_table = miscbeep_of_match,
	},
	.probe = miscbeep_probe,
	.remove = miscbeep_remove,
};

module_platform_driver(miscbeep_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex.xu");
MODULE_INFO(intree, "Y");

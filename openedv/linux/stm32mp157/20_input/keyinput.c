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
#include <linux/input.h>
#include <linux/platform_device.h>

#define KEYINPUT_NAME "keyirq"

struct key_dev
{
	struct input_dev *iputdev;
	int gpio;
	int irq;
	struct timer_list timer;
	struct tasklet_struct tasklet;
};

struct key_dev keydev;

static irqreturn_t key_interrupt_handler(int irq, void *dev_id)
{
	tasklet_schedule(&keydev.tasklet);
	return IRQ_HANDLED;
}
static int key_parse(struct device_node *nd)
{
	keydev.gpio = of_get_named_gpio(nd, "gpios", 0);
	if (keydev.gpio < 0)
	{
		printk("cannot get gpios\n");
		return -EINVAL;
	}
	printk("key-gpio num = %d\n", keydev.gpio);

	keydev.irq = irq_of_parse_and_map(nd, 0);
	if (!keydev.irq)
	{
		return -EINVAL;
	}

	printk("irq num = %d\r\n", keydev.irq);

	return 0;
}

static int key_gpio_init(void)
{
	int ret;
	unsigned long irq_flags;
	ret = gpio_request(keydev.gpio, KEYINPUT_NAME);
	if (ret < 0)
	{
		printk(KERN_ERR "gpio_request failed\n");
		return ret;
	}

	ret = gpio_direction_input(keydev.gpio);
	if (ret < 0)
	{
		printk("cannot set gpio gpio_direction_output!\n");
		return ret;
	}

	irq_flags = irq_get_trigger_type(keydev.irq);
	if (IRQF_TRIGGER_NONE == irq_flags)
	{
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	}

	ret = request_irq(keydev.irq, key_interrupt_handler, irq_flags, "Key0_IRQ", NULL);
	if (ret)
	{
		gpio_free(keydev.gpio);
		return ret;
	}
	return 0;
}

static void timer_function(struct timer_list *args)
{
	struct key_dev *dev = from_timer(dev, args, timer);
	int value;

	value = gpio_get_value(dev->gpio);
	input_report_key(dev->iputdev,KEY_0,!value);
	input_sync(dev->iputdev);
}

static void key_irq_tasklet(unsigned long drv)
{
	struct key_dev *dev = (struct key_dev *)drv;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(15));
}

static int mykey_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = key_parse(pdev->dev.of_node);
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
	timer_setup(&keydev.timer, timer_function, 0);

	keydev.iputdev = input_allocate_device();
	if (!keydev.iputdev){
		ret = -ENOMEM;
		goto free_gpio;
	}
	keydev.iputdev->name = KEYINPUT_NAME;
	__set_bit(EV_KEY,keydev.iputdev->evbit);
	__set_bit(EV_REP,keydev.iputdev->evbit);
	__set_bit(KEY_0,keydev.iputdev->keybit);

	ret = input_register_device(keydev.iputdev);
	if(ret){
		printk("input_register_device failed\n");
		goto free_gpio;
	}

	return 0;

free_gpio:
	free_irq(keydev.irq, NULL);
	gpio_free(keydev.gpio);
	del_timer_sync(&keydev.timer);
	return ret;
}

static int mykey_remove(struct platform_device *pdev)
{
	del_timer_sync(&keydev.timer);
	free_irq(keydev.irq, NULL);
	gpio_free(keydev.gpio);
	input_unregister_device(keydev.iputdev);
	return 0;
}

static struct of_device_id	key_of_match[] = {
	{.compatible = "alex,key"},
	{},
};

static struct platform_driver mykey_driver = {
	.driver={
		.name			= "keyboard",
		.of_match_table	= key_of_match,
	},
	.probe	= mykey_probe,
	.remove	= mykey_remove,
};

module_platform_driver(mykey_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex");
MODULE_INFO(intree, "Y");
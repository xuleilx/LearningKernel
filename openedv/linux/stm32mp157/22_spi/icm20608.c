#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/ide.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include "icm20608reg.h"

#define ICM20608_NAME "icm20608"

struct icm20608_device
{
    struct spi_device *spi; /* spi设备 */
    struct miscdevice misc_dev;
};
static int icm20608_open(struct inode *, struct file *);
static int icm20608_release(struct inode *, struct file *);
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt,
                             loff_t *off);

/*
 * @description	: 从icm20608读取多个寄存器数据
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int icm20608_read_regs(struct icm20608_device *dev, u8 reg, void *buf,
                              int len)
{
    unsigned char txdata[1];
    txdata[0] = reg | 0x80;
    return spi_write_then_read(dev->spi, txdata, 1, buf, len);
}

/*
 * @description	: 向icm20608多个寄存器写入数据
 * @param - dev:  icm20608设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 icm20608_write_regs(struct icm20608_device *dev, u8 reg, u8 *buf,
                               u8 len)
{
    int ret = -1;
    unsigned char *txdata;

    txdata = kzalloc(sizeof(char) * len + 1, GFP_KERNEL);
    if (!txdata)
    {
        goto out1;
    }
    *txdata = reg & ~0x80;
    memcpy(txdata + 1, buf, len);
    ret = spi_write(dev->spi, txdata, len + 1);
    kfree(txdata);
out1:
    return ret;
}

/*
 * @description	: 读取icm20608指定寄存器值，读取一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char icm20608_read_onereg(struct icm20608_device *dev, u8 reg)
{
    u8 data = 0;
    icm20608_read_regs(dev, reg, &data, 1);
    return data;
}

/*
 * @description	: 向icm20608指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */

static void icm20608_write_onereg(struct icm20608_device *dev, u8 reg,
                                  u8 value)
{
    u8 buf = value;
    icm20608_write_regs(dev, reg, &buf, 1);
}

static int icm20608_read_temp_data(struct icm20608_device *dev, u16 *data) {
    int ret = -1;
    u16 tmp;
    ret = icm20608_read_regs(dev, ICM20_TEMP_OUT_H, &tmp, 2);
    *data = (signed short)((tmp << 8) | (tmp >> 8));
    printk("temp:%d\n",*data);
  	return ret;
}

static ssize_t show_temp(struct device *dev, struct device_attribute *attr,
                       char *buf) {
  u16 data;
  struct icm20608_device *device =
      (struct icm20608_device *)dev_get_drvdata(dev);
  if (icm20608_read_temp_data(device, &data)) {
    return sprintf(buf, "%d\n", -1);
  }
  return sprintf(buf, "%d\n", data);
}

static DEVICE_ATTR(temp, S_IRUGO, show_temp, NULL);

static struct attribute *icm20608_attrs[] = {
    &dev_attr_temp.attr, NULL};

static const struct attribute_group icm20608_attrs_group = {
    .attrs = icm20608_attrs,
};

static const struct attribute_group *icm20608_attr_groups[] = {
    &icm20608_attrs_group, NULL};


/* icm20608操作函数 */
static const struct file_operations icm20608_ops = {
    .owner = THIS_MODULE,
    .open = icm20608_open,
    .read = icm20608_read,
    .release = icm20608_release,
};
static struct icm20608_device icm20608_dev = {
    .misc_dev =
        {
            .name = ICM20608_NAME,
            .minor = MISC_DYNAMIC_MINOR,
            .fops = &icm20608_ops,
            .groups = icm20608_attr_groups,
        },
    .spi = NULL,
};

/*
 * @description	: 读取ICM20608的数据，读取原始数据，包括三轴陀螺仪、
 * 				: 三轴加速度计和内部温度。
 * @param - dev	: ICM20608设备
 * @return 		: 无。
 */
void icm20608_readdata(struct icm20608_device *dev, signed int *buf)
{
    unsigned char data[14];
    icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

    buf[0] = (signed short)((data[8] << 8) | data[9]);   /* 陀螺仪X轴原始值 	 */
    buf[1] = (signed short)((data[10] << 8) | data[11]); /* 陀螺仪Y轴原始值		*/
    buf[2] = (signed short)((data[12] << 8) | data[13]); /* 陀螺仪Z轴原始值     */
    buf[3] = (signed short)((data[0] << 8) | data[1]);   /* 加速度计X轴原始值 	*/
    buf[4] = (signed short)((data[2] << 8) | data[3]);   /* 加速度计Y轴原始值	*/
    buf[5] = (signed short)((data[4] << 8) | data[5]);   /* 加速度计Z轴原始值 	*/
    buf[6] = (signed short)((data[6] << 8) | data[7]);   /* 温度原始值 		    */
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &icm20608_dev;
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
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt,
                             loff_t *off)
{
    signed int data[7];
    long err = 0;
    struct icm20608_device *dev = filp->private_data;

    icm20608_readdata(dev, data);
    err = copy_to_user(buf, data, sizeof(data));
    return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * ICM20608内部寄存器初始化函数
 * @param - spi : 要操作的设备
 * @return 	: 无
 */
void icm20608_reginit(struct icm20608_device *dev)
{
    u8 value = 0;

    icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x80);
    mdelay(50);
    icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x01);
    mdelay(50);

    value = icm20608_read_onereg(dev, ICM20_WHO_AM_I);
    printk("ICM20608 ID = %#X\r\n", value);

    icm20608_write_onereg(dev, ICM20_SMPLRT_DIV, 0x00);    /* 输出速率是内部采样率					*/
    icm20608_write_onereg(dev, ICM20_GYRO_CONFIG, 0x18);   /* 陀螺仪±2000dps量程 */
    icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG, 0x18);  /* 加速度计±16G量程 					*/
    icm20608_write_onereg(dev, ICM20_CONFIG, 0x04);        /* 陀螺仪低通滤波BW=20Hz 				*/
    icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz */
    icm20608_write_onereg(dev, ICM20_PWR_MGMT_2, 0x00);    /* 打开加速度计和陀螺仪所有轴 				*/
    icm20608_write_onereg(dev, ICM20_LP_MODE_CFG, 0x00);   /* 关闭低功耗 						*/
    icm20608_write_onereg(dev, ICM20_FIFO_EN, 0x00);       /* 关闭FIFO						*/
}

/*
 * @description     : spi驱动的probe函数，当驱动与
 *                    设备匹配以后此函数就会执行
 * @param - spi  	: spi设备
 *
 */
static int icm20608_probe(struct spi_device *spi)
{
    printk("icm20608_probe\n");

    misc_register(&icm20608_dev.misc_dev);
    icm20608_dev.spi = spi;

    /*初始化spi_device */
    spi->mode = SPI_MODE_0; /*MODE0，CPOL=0，CPHA=0*/
    spi_setup(spi);

    /* 初始化ICM20608内部寄存器 */
    icm20608_reginit(&icm20608_dev);

    /* 保存icm20608_device结构体 */
    spi_set_drvdata(spi, &icm20608_dev);
    dev_set_drvdata(icm20608_dev.misc_dev.this_device, (void *)&icm20608_dev);
    
    return 0;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - spi 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int icm20608_remove(struct spi_device *spi)
{
    struct icm20608_device *dev = spi_get_drvdata(spi);
    misc_deregister(&dev->misc_dev);
    return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id icm20608_id[] = {{"alientek,icm20608", 0},
                                                   {}};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_of_match[] = {
    {.compatible = "alientek,icm20608"}, {/* Sentinel */}};

/* SPI驱动结构体 */
static struct spi_driver icm20608_driver = {
    .probe = icm20608_probe,
    .remove = icm20608_remove,
    .driver =
        {
            .owner = THIS_MODULE,
            .name = "icm20608",
            .of_match_table = icm20608_of_match,
        },
    .id_table = icm20608_id,
};

module_spi_driver(icm20608_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex.xu");
MODULE_INFO(intree, "Y");

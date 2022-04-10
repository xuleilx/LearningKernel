#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#define FT5426_DEVIDE_MODE_REG 0x00 /* mode register */
#define FT5426_TD_STATUS_REG 0x02   /* status register */
#define FT4426_TOUCH_DATA_REG 0x03  /* touch data register */
#define FT5426_ID_G_MODE_REG 0xA4   /* int mode register */

#define MAX_SUPPORT_POINTS 5 /* ft5426最大支持5点触摸 */

#define TOUCH_EVENT_DOWN 0x00
#define TOUCH_EVENT_UP 0x01
#define TOUCH_EVENT_ON 0x02 /* 接触 */
#define TOUCH_EVENT_RESERVED 0x03

struct edt_ft5426_dev
{
    struct i2c_client *client;
    struct input_dev *input;
    int reset_gpio;
    int irq_gpio;
};

static int edt_ft5426_ts_write(struct edt_ft5426_dev *ft5426, u8 addr, u8 *buf, u16 len)
{
    struct i2c_client *client = ft5426->client;
    u8 send_buf[6] = {0};
    struct i2c_msg msg;
    memcpy(&send_buf[1], buf, len);

    send_buf[0] = addr;
    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = len + 1;
    msg.buf = send_buf;

    if (1 != i2c_transfer(client->adapter, &msg, 1))
    {
        dev_err(&client->dev, "%s: write error, addr = 0x%x len=%d.\n",
                __func__, addr, len);
        return -1;
    }
    return 0;
}

static int edt_ft5426_ts_read(struct edt_ft5426_dev *ft5426, u8 addr, u8 *buf, u16 len)
{
    struct i2c_client *client = ft5426->client;
    struct i2c_msg msg[2] = {[0] =
                                 {
                                     .addr = client->addr,
                                     .flags = 0,
                                     .len = 1,
                                     .buf = &addr,
                                 },
                             [1] = {
                                 .addr = client->addr,
                                 .flags = I2C_M_RD,
                                 .len = len,
                                 .buf = buf,
                             }};
    if (2 != i2c_transfer(client->adapter, msg, 2))
    {
        dev_err(&client->dev, "%s: read error, addr = 0x%x len=%d.\n",
                __func__, addr, len);
        return -1;
    }

    return 0;
}

static int edt_ft5426_ts_reset(struct edt_ft5426_dev *ft5426)
{
    struct i2c_client *client = ft5426->client;
    int ret;

    ft5426->reset_gpio = of_get_named_gpio(client->dev.of_node, "reset-gpios", 0);
    if (!gpio_is_valid(ft5426->reset_gpio))
    {
        dev_err(&client->dev, "Failed to get ts reset gpio\n");
        return ft5426->reset_gpio;
    }

    ret = devm_gpio_request_one(&client->dev, ft5426->reset_gpio, GPIOF_OUT_INIT_HIGH, "ft5426 reset");
    if (ret < 0)
    {
        return ret;
    }

    msleep(20);
    gpio_set_value_cansleep(ft5426->reset_gpio, 0);
    msleep(5);
    gpio_set_value_cansleep(ft5426->reset_gpio, 1);
    return 0;
}

static irqreturn_t edt_ft5426_ts_isr(int irq, void *dev_id)
{
    struct edt_ft5426_dev *ft5426 = dev_id;
    u8 rdbuf[30] = {0};
    int i, type, x, y, id;
    bool down;
    int ret;

    ret = edt_ft5426_ts_read(ft5426, FT5426_TD_STATUS_REG, rdbuf, 29);
    if (ret)
    {
        goto out;
    }

    for (i = 0; i < MAX_SUPPORT_POINTS; i++)
    {
        u8 *buf = &rdbuf[i * 6 + 1];
        type = buf[0] >> 6;
        if (type == TOUCH_EVENT_RESERVED)
        {
            continue;
        }
        x = ((buf[2] << 8) | buf[3]) & 0x0fff;
        y = ((buf[0] << 8) | buf[1]) & 0x0fff;
        id = (buf[2] >> 4) & 0x0f;
        down = type != TOUCH_EVENT_UP;
        input_mt_slot(ft5426->input, id);
        input_mt_report_slot_state(ft5426->input, MT_TOOL_FINGER, down);
        if (!down)
        {
            continue;
        }

        input_report_abs(ft5426->input, ABS_MT_POSITION_X, x);
        input_report_abs(ft5426->input, ABS_MT_POSITION_Y, y);
    }
    input_mt_report_pointer_emulation(ft5426->input, true);
    input_sync(ft5426->input);
out:
    return IRQ_HANDLED;
}

static int edt_ft5426_ts_irq(struct edt_ft5426_dev *ft5426)
{
    struct i2c_client *client = ft5426->client;
    int ret;

    ft5426->irq_gpio = of_get_named_gpio(client->dev.of_node, "irq-gpios", 0);
    if (!gpio_is_valid(ft5426->irq_gpio))
    {
        dev_err(&client->dev, "Failed to get ts irq gpio\n");
        return ft5426->irq_gpio;
    }

    ret = devm_gpio_request_one(&client->dev, ft5426->irq_gpio, GPIOF_IN, "ft5426 irq");
    if (ret < 0)
    {
        return ret;
    }

    ret = devm_request_threaded_irq(&client->dev,
                                    gpio_to_irq(ft5426->irq_gpio),
                                    NULL, edt_ft5426_ts_isr,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    client->name, ft5426);
    if (ret)
    {
        dev_err(&client->dev, "Failed to request touchscreen IRQ.\n");
        return ret;
    }
    return 0;
}

static int edt_ft5426_ts_probe(struct i2c_client *client,
                               const struct i2c_device_id *id)
{
    struct edt_ft5426_dev *ft5426;
    struct input_dev *input;
    u8 data;
    int ret;

    ft5426 = devm_kmalloc(&client->dev, sizeof(struct edt_ft5426_dev), GFP_KERNEL);
    if (!ft5426)
    {
        dev_err(&client->dev, "Failed to allocate ft5426 driver data.\n");
        return -ENOMEM;
    }

    ft5426->client = client;

    ret = edt_ft5426_ts_reset(ft5426);
    if (ret != 0)
    {
        return ret;
    }

    msleep(5);
    /* init ft5426 */
    data = 0;
    edt_ft5426_ts_write(ft5426, FT5426_DEVIDE_MODE_REG, &data, 1);
    data = 1;
    edt_ft5426_ts_write(ft5426, FT5426_ID_G_MODE_REG, &data, 1);

    ret = edt_ft5426_ts_irq(ft5426);
    if (ret)
    {
        return ret;
    }

    input = devm_input_allocate_device(&client->dev);
    if (!input)
    {
        dev_err(&client->dev, "Failed to allocate input device.\n");
        return -ENOMEM;
    }

    ft5426->input = input;
    input->name = "FocalTech FT5426 touchscreen";
    input->id.bustype = BUS_I2C;

    input_set_abs_params(input, ABS_MT_POSITION_X,
                         0, 800, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y,
                         0, 480, 0, 0);
    ret = input_mt_init_slots(input, MAX_SUPPORT_POINTS, INPUT_MT_DIRECT);
    if (ret)
    {
        dev_err(&client->dev, "Failed to init MT slots.\n");
        return -ENOMEM;
    }

    ret = input_register_device(input);
    if(ret){
        return ret;
    }

    i2c_set_clientdata(client,ft5426);
    return 0;
}

static int edt_ft5426_ts_remove(struct i2c_client *client){
    struct edt_ft5426_dev * ft5426 = i2c_get_clientdata(client);
    input_unregister_device(ft5426->input);
    return 0;
}

static const struct of_device_id edt_ft5426_of_match[] = {
    {.compatible = "edt,edt-ft5206",},
    {}
};

static struct i2c_driver edt_ft5426_ts_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "edt_ft5426",
        .of_match_table = of_match_ptr(edt_ft5426_of_match),
    },
    .probe = edt_ft5426_ts_probe,
    .remove = edt_ft5426_ts_remove,
};

module_i2c_driver(edt_ft5426_ts_driver);

MODULE_AUTHOR("Alex.xu");
MODULE_DESCRIPTION("FT5426 touchscreen");
MODULE_LICENSE("GPL");
MODULE_INFO(intree, "Y");

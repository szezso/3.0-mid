/*
 * apds9802ps.c - Avago  Proximity Sensor Driver
 *
 * Copyright (C) 2009 Intel Corp
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>

MODULE_AUTHOR("Anantha Narayanan <Anantha.Narayanan@intel.com");
MODULE_DESCRIPTION("Avago apds9802ps Proximity Driver");
MODULE_LICENSE("GPL v2");

#define POWER_STA_ENABLE 1
#define POWER_STA_DISABLE 0

#define DRIVER_NAME "apds9802ps"

struct ps_data{
	struct mutex lock;
};

static ssize_t ps_proximity_output_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ps_data *data = i2c_get_clientdata(client);
	int ret_val = 0;
	int l, h;
	int retry = 10;

	pm_runtime_get_sync(dev);
	mutex_lock(&data->lock);

	/* start measurement */
	i2c_smbus_write_byte_data(client, 0x82, 0x2d);

	/* wait for data ready */
	do {
		msleep(5);
		ret_val = i2c_smbus_read_byte_data(client, 0x87);
		if (ret_val > 0 && (ret_val & 0x10))
			break;
	} while (retry--);

	if (!retry)
		dev_warn(dev, "timeout waiting for data ready\n");

	l = i2c_smbus_read_byte_data(client, 0x85); /* LSB data */
	if (l < 0)
		dev_warn(dev, "failed proximity out read LSB\n");

	h = i2c_smbus_read_byte_data(client, 0x86); /* MSB data */
	if (h < 0)
		dev_warn(dev, "failed proximity out read MSB\n");

	/* stop measurement and clear interrupt status */
	i2c_smbus_write_byte_data(client, 0x82, 0x0d);
	i2c_smbus_write_byte(client, 0x60);

	mutex_unlock(&data->lock);
	pm_runtime_put_sync(dev);

	ret_val = (h << 8 | l);
	return sprintf(buf, "%d\n", ret_val);
}

static void ps_set_power_state(struct i2c_client *client, bool on_off)
{
	char curr_val = 0;
	struct ps_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->lock);

	curr_val = i2c_smbus_read_byte_data(client, 0x80);
	if (on_off)
		curr_val = curr_val | 0x01;
	else
		curr_val = curr_val & 0xFE;

	if (i2c_smbus_write_byte_data(client, 0x80, curr_val) < 0)
		dev_warn(&client->dev, "failed power state write\n");

	mutex_unlock(&data->lock);
}

static DEVICE_ATTR(proximity_output, S_IRUGO,
		   ps_proximity_output_data_show, NULL);

static struct attribute *mid_att_ps[] = {
	&dev_attr_proximity_output.attr,
	NULL
};

static struct attribute_group m_ps_gr = {
	.name = "apds9802ps",
	.attrs = mid_att_ps
};

static void ps_set_default_config(struct i2c_client *client)
{
	/* Power ON */
	if (i2c_smbus_write_byte_data(client, 0x80, 0x01) < 0)
		dev_warn(&client->dev, "failed default power on write\n");

	/* 20 pulses, 100Khz Pulse frequency */
	if (i2c_smbus_write_byte_data(client, 0x81, 0x86) < 0)
		dev_warn(&client->dev, "failed pulse frequency write\n");

	/* 100MA LED current, 500ms interval delay */
	if (i2c_smbus_write_byte_data(client, 0x82, 0x0d) < 0)
		dev_warn(&client->dev, "failed interval delay write\n");
}

static int apds9802ps_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int res;
	struct ps_data *data;

	data = kzalloc(sizeof(struct ps_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev, "alloc ps_data failed\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, data);

	res = sysfs_create_group(&client->dev.kobj, &m_ps_gr);
	if (res) {
		dev_err(&client->dev, "sysfs file create failed\n");
		goto ps_error1;
	}

	dev_info(&client->dev, "proximity sensor chip found\n");

	ps_set_default_config(client);

	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);

	mutex_init(&data->lock);
	return res;

ps_error1:
	i2c_set_clientdata(client, NULL);
	kfree(data);
	return res;
}

static int __devexit apds9802ps_remove(struct i2c_client *client)
{
	struct ps_data *data = i2c_get_clientdata(client);

	pm_runtime_get_sync(&client->dev);

	ps_set_power_state(client, false);
	sysfs_remove_group(&client->dev.kobj, &m_ps_gr);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	kfree(data);
	return 0;
}

static int apds9802ps_suspend(struct i2c_client *client, pm_message_t mesg)
{
	ps_set_power_state(client, false);
	return 0;
}

static int apds9802ps_resume(struct i2c_client *client)
{
	ps_set_default_config(client);
	return 0;
}

static struct i2c_device_id apds9802ps_id[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds9802ps_id);

static int apds9802ps_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	ps_set_power_state(client, false);
	return 0;
}

static int apds9802ps_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	ps_set_power_state(client, true);
	return 0;
}

static const struct dev_pm_ops apds9802ps_pm_ops = {
	.runtime_suspend = apds9802ps_runtime_suspend,
	.runtime_resume = apds9802ps_runtime_resume,
};

static struct i2c_driver apds9802ps_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.pm = &apds9802ps_pm_ops,
	},
	.probe = apds9802ps_probe,
	.remove = __devexit_p(apds9802ps_remove),
	.suspend = apds9802ps_suspend,
	.resume = apds9802ps_resume,
	.id_table = apds9802ps_id,
};

static int __init sensor_apds9802ps_init(void)
{
	return i2c_add_driver(&apds9802ps_driver);
}

static void  __exit sensor_apds9802ps_exit(void)
{
	i2c_del_driver(&apds9802ps_driver);
}

module_init(sensor_apds9802ps_init);
module_exit(sensor_apds9802ps_exit);

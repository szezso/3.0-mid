/*
 * ak8974.c - AKEMD Compass Driver
 *
 * Copyright (C) 2010 Intel Corp
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * this program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE. See the GNU
 * General public License for more details.
 *
 * You should have received a copy of the GNU General public License along
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
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/hwmon-sysfs.h>
#include <linux/pm_runtime.h>

MODULE_AUTHOR("KalhanTrisal,Anantha Narayanan<anantha.narayanan@intel.com>");
MODULE_DESCRIPTION("ak8974 Compass Driver");
MODULE_LICENSE("GPL v2");

/* register address */
#define DEVICE_ID	0x0F
#define ADDR_TMPS	0x31
#define DATA_XL		0x10
#define DATA_XM		0x11
#define DATA_YL		0x12
#define DATA_YM		0x13
#define DATA_ZL		0x14
#define DATA_ZM		0x15
#define STAT_REG	0x18
#define CNTL_1		0x1B
  #define CNTL1_MODE_ACTIVE	(1 << 7)
  #define CNTL1_STAT_FORCE	(1 << 1)
#define CNTL_2		0x1C
#define CNTL_3		0x1D
  #define CNTL3_SOFT_RESET	(1 << 7)
  #define CNTL3_START_MEASURE	(1 << 6)
#define PRET_REG	0x30

struct compass_data {
	struct mutex write_lock;
};

static void set_power_state(struct i2c_client *client, bool on)
{
	struct compass_data *data = i2c_get_clientdata(client);
	u8 cntl1;

	mutex_lock(&data->write_lock);

	cntl1 = i2c_smbus_read_byte_data(client, CNTL_1);
	if (on)
		cntl1 |= CNTL1_MODE_ACTIVE;
	else
		cntl1 &= ~CNTL1_MODE_ACTIVE;

	if (i2c_smbus_write_byte_data(client, CNTL_1, cntl1) < 0)
		dev_warn(&client->dev, "failed power state write\n");

	mutex_unlock(&data->write_lock);
}

static ssize_t curr_xyz_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct compass_data *data = i2c_get_clientdata(client);
	int i;
	s16 values[3];

	pm_runtime_get_sync(dev);
	mutex_lock(&data->write_lock);

	if (i2c_smbus_write_byte_data(client, CNTL_3, CNTL3_START_MEASURE) < 0)
		dev_warn(dev, "failed xyz contrl3 register write\n");

	/* wait for data ready */
	msleep(15);

	/*Force Read*/
	i2c_smbus_read_i2c_block_data(client, DATA_XL, 6, (u8 *)values);
	for (i = 0; i < 3; i++)
		values[i] = le16_to_cpu(values[i]);

	mutex_unlock(&data->write_lock);
	pm_runtime_put_sync(dev);

	return sprintf(buf, "(%d,%d,%d)\n", values[0], values[1], values[2]);
}

/* change to active mode and do some default config */
static void set_default_config(struct i2c_client *client)
{
	u8 val;

	if (i2c_smbus_write_byte_data(client, CNTL_1, CNTL1_MODE_ACTIVE) < 0)
		dev_warn(&client->dev, "failed default power on write\n");

	val = i2c_smbus_read_byte_data(client, CNTL_1);
	if (i2c_smbus_write_byte_data(client, CNTL_1, val | CNTL1_STAT_FORCE))
		dev_warn(&client->dev, "failed to set force state\n");

	if (i2c_smbus_write_byte_data(client, PRET_REG, 0x00) < 0)
		dev_warn(&client->dev, "failed default control write\n");
}

static DEVICE_ATTR(curr_pos, S_IRUGO, curr_xyz_show, NULL);

static struct attribute *mid_att_compass[] = {
	&dev_attr_curr_pos.attr,
	NULL
};

static struct attribute_group m_compass_gr = {
	.name = "ak8974",
	.attrs = mid_att_compass
};

static int ak8974_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int res;
	struct compass_data *data;

	data = kzalloc(sizeof(struct compass_data), GFP_KERNEL);
	if (data == NULL) {
		dev_warn(&client->dev, "memory initialization failed\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, data);

	res = sysfs_create_group(&client->dev.kobj, &m_compass_gr);
	if (res) {
		dev_warn(&client->dev, "device_create_file failed\n");
		goto compass_error1;
	}

	dev_info(&client->dev, "compass chip found\n");
	set_default_config(client);
	mutex_init(&data->write_lock);

	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);

	return 0;

compass_error1:
	i2c_set_clientdata(client, NULL);
	kfree(data);
	return res;
}

static int ak8974_detect(struct i2c_client *client,
			struct i2c_board_info *info)
{
	u8 device_id;

	/* Check device ID (WIA register) */
	device_id = i2c_smbus_read_byte_data(client, DEVICE_ID);
	switch (device_id) {
	case 0x47:
		strlcpy(info->type, "ami304", I2C_NAME_SIZE);
		break;
	case 0x48:
		strlcpy(info->type, "ak8974", I2C_NAME_SIZE);
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static int __devexit ak8974_remove(struct i2c_client *client)
{
	struct compass_data *data = i2c_get_clientdata(client);

	pm_runtime_get_sync(&client->dev);

	set_power_state(client, false);
	sysfs_remove_group(&client->dev.kobj, &m_compass_gr);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	kfree(data);
	return 0;
}

static int ak8974_suspend(struct i2c_client *client, pm_message_t mesg)
{
	set_power_state(client, false);
	return 0;
}

static int ak8974_resume(struct i2c_client *client)
{
	set_default_config(client);

	return 0;
}

static int ak8974_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	set_power_state(client, false);
	return 0;
}

static int ak8974_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	set_power_state(client, true);
	return 0;
}

static const struct dev_pm_ops ak8974_pm_ops = {
	.runtime_suspend = ak8974_runtime_suspend,
	.runtime_resume = ak8974_runtime_resume,
};

static struct i2c_device_id ak8974_id[] = {
	{ "ak8974", 0 },
	{ "ami304", 0 },
	{ }
};
static struct i2c_driver ak8974_driver = {
	.driver = {
		.name = "ak8974",
		.pm = &ak8974_pm_ops,
	},
	.probe = ak8974_probe,
	.detect = ak8974_detect,
	.remove = __devexit_p(ak8974_remove),
	.suspend = ak8974_suspend,
	.resume = ak8974_resume,
	.id_table = ak8974_id,
};

static int __init sensor_ak8974_init(void)
{
	return i2c_add_driver(&ak8974_driver);
}

static void  __exit sensor_ak8974_exit(void)
{
	i2c_del_driver(&ak8974_driver);
}

module_init(sensor_ak8974_init);
module_exit(sensor_ak8974_exit);

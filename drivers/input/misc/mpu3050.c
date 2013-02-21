/*
 * MPU3050 Tri-axis gyroscope driver
 *
 * Copyright (C) 2011 Wistron Co.Ltd
 * Joseph Lai <joseph_lai@wistron.com>
 *
 * Trimmed down by Alan Cox <alan@linux.intel.com> to produce this version
 *
 * This is a 'lite' version of the driver, while we consider the right way
 * to present the other features to user space. In particular it requires the
 * device has an IRQ, and it only provides an input interface, so is not much
 * use for device orientation. A fuller version is available from the Meego
 * tree.
 *
 * This program is based on bma023.c.
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
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/earlysuspend.h>

#define MPU3050_CHIP_ID		0x69

#define MPU3050_AUTO_DELAY	1000

#define MPU3050_MIN_VALUE	-32768
#define MPU3050_MAX_VALUE	32767

#define MPU3050_DEFAULT_POLL_INTERVAL	200
#define MPU3050_MAX_POLL_INTERVAL	255
#define MPU3050_MIN_POLL_INTERVAL	0
#define MPU3050_DEFAULT_FS_RANGE	3

/* Register map */
#define MPU3050_CHIP_ID_REG	0x00
#define MPU3050_SMPLRT_DIV	0x15
#define MPU3050_DLPF_FS_SYNC	0x16
#define MPU3050_INT_CFG		0x17
#define MPU3050_XOUT_H		0x1D
#define MPU3050_PWR_MGM		0x3E
#define MPU3050_PWR_MGM_POS	6

/* Register bits */

/* DLPF_FS_SYNC */
#define MPU3050_EXT_SYNC_NONE		0x00
#define MPU3050_EXT_SYNC_TEMP		0x20
#define MPU3050_EXT_SYNC_GYROX		0x40
#define MPU3050_EXT_SYNC_GYROY		0x60
#define MPU3050_EXT_SYNC_GYROZ		0x80
#define MPU3050_EXT_SYNC_ACCELX		0xA0
#define MPU3050_EXT_SYNC_ACCELY		0xC0
#define MPU3050_EXT_SYNC_ACCELZ		0xE0
#define MPU3050_EXT_SYNC_MASK		0xE0
#define MPU3050_FS_250DPS		0x00
#define MPU3050_FS_500DPS		0x08
#define MPU3050_FS_1000DPS		0x10
#define MPU3050_FS_2000DPS		0x18
#define MPU3050_FS_MASK			0x18
#define MPU3050_DLPF_CFG_256HZ_NOLPF2	0x00
#define MPU3050_DLPF_CFG_188HZ		0x01
#define MPU3050_DLPF_CFG_98HZ		0x02
#define MPU3050_DLPF_CFG_42HZ		0x03
#define MPU3050_DLPF_CFG_20HZ		0x04
#define MPU3050_DLPF_CFG_10HZ		0x05
#define MPU3050_DLPF_CFG_5HZ		0x06
#define MPU3050_DLPF_CFG_2100HZ_NOLPF	0x07
#define MPU3050_DLPF_CFG_MASK		0x07
/* INT_CFG */
#define MPU3050_RAW_RDY_EN		0x01
#define MPU3050_MPU_RDY_EN		0x02
#define MPU3050_LATCH_INT_EN		0x04
/* PWR_MGM */
#define MPU3050_PWR_MGM_PLL_X		0x01
#define MPU3050_PWR_MGM_PLL_Y		0x02
#define MPU3050_PWR_MGM_PLL_Z		0x03
#define MPU3050_PWR_MGM_CLKSEL		0x07
#define MPU3050_PWR_MGM_STBY_ZG		0x08
#define MPU3050_PWR_MGM_STBY_YG		0x10
#define MPU3050_PWR_MGM_STBY_XG		0x20
#define MPU3050_PWR_MGM_SLEEP		0x40
#define MPU3050_PWR_MGM_RESET		0x80
#define MPU3050_PWR_MGM_MASK		0x40

struct axis_data {
	s16 x;
	s16 y;
	s16 z;
};

struct mpu3050_sensor {
	struct i2c_client *client;
	struct device *dev;
	struct input_dev *idev;
	struct input_polled_dev *input_polled;
	u8 poll_div;
	struct early_suspend es;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mpu3050_early_suspend(struct early_suspend *h);
static void mpu3050_late_resume(struct early_suspend *h);
#endif

/**
 *	mpu3050_xyz_read_reg	-	read the axes values
 *	@buffer: provide register addr and get register
 *	@length: length of register
 *
 *	Reads the register values in one transaction or returns a negative
 *	error code on failure.
 */
static int mpu3050_xyz_read_reg(struct i2c_client *client,
			       u8 *buffer, int length)
{
	/*
	 * Annoying we can't make this const because the i2c layer doesn't
	 * declare input buffers const.
	 */
	char cmd = MPU3050_XOUT_H;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	return i2c_transfer(client->adapter, msg, 2);
}

/**
 *	mpu3050_read_xyz	-	get co-ordinates from device
 *	@client: i2c address of sensor
 *	@coords: co-ordinates to update
 *
 *	Return the converted X Y and Z co-ordinates from the sensor device
 */
static void mpu3050_read_xyz(struct i2c_client *client,
			     struct axis_data *coords)
{
	u16 buffer[3];

	mpu3050_xyz_read_reg(client, (u8 *)buffer, 6);
	coords->x = be16_to_cpu(buffer[0]);
	coords->y = be16_to_cpu(buffer[1]);
	coords->z = be16_to_cpu(buffer[2]);
	dev_dbg(&client->dev, "%s: x %d, y %d, z %d\n", __func__,
					coords->x, coords->y, coords->z);
}

/**
 *	mpu3050_set_power_mode	-	set the power mode
 *	@client: i2c client for the sensor
 *	@val: value to switch on/off of power, 1: normal power, 0: low power
 *
 *	Put device to normal-power mode or low-power mode.
 */
static void mpu3050_set_power_mode(struct i2c_client *client, u8 val)
{
	u8 value;

	value = i2c_smbus_read_byte_data(client, MPU3050_PWR_MGM);
	value = (value & ~MPU3050_PWR_MGM_MASK) |
		(((val << MPU3050_PWR_MGM_POS) & MPU3050_PWR_MGM_MASK) ^
		 MPU3050_PWR_MGM_MASK);
	i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM, value);
}

/**
 *	mpu3050_input_open	-	called on input event open
 *	@input: input dev of opened device
 *
 *	The input layer calls this function when input event is opened. The
 *	function will push the device to resume. Then, the device is ready
 *	to provide data.
 */
static int mpu3050_input_open(struct input_dev *input)
{
	struct mpu3050_sensor *sensor = input_get_drvdata(input);

	pm_runtime_get(sensor->dev);

	return 0;
}

/**
 *	mpu3050_input_close	-	called on input event close
 *	@input: input dev of closed device
 *
 *	The input layer calls this function when input event is closed. The
 *	function will push the device to suspend.
 */
static void mpu3050_input_close(struct input_dev *input)
{
	struct mpu3050_sensor *sensor = input_get_drvdata(input);

	pm_runtime_put(sensor->dev);
}

static void mpu3050_poll_open(struct input_polled_dev *ipoll_dev)
{
	struct mpu3050_sensor *sensor = ipoll_dev->private;

	mpu3050_set_power_mode(sensor->client, 1);
	msleep(10);  /* wait for gyro chip resume */
}

static void mpu3050_poll_close(struct input_polled_dev *ipoll_dev)
{
	struct mpu3050_sensor *sensor = ipoll_dev->private;

	mpu3050_set_power_mode(sensor->client, 1);
}

static void mpu3050_report_xyz(struct mpu3050_sensor *sensor)
{
	struct axis_data axis;

	mpu3050_read_xyz(sensor->client, &axis);

	input_report_abs(sensor->idev, ABS_X, axis.x);
	input_report_abs(sensor->idev, ABS_Y, axis.y);
	input_report_abs(sensor->idev, ABS_Z, axis.z);
	input_sync(sensor->idev);
}

static void mpu3050_poll(struct input_polled_dev *dev)
{
	mpu3050_report_xyz(dev->private);
}

/**
 *	mpu3050_interrupt_thread	-	handle an IRQ
 *	@irq: interrupt numner
 *	@data: the sensor
 *
 *	Called by the kernel single threaded after an interrupt occurs. Read
 *	the sensor data and generate an input event for it.
 */
static irqreturn_t mpu3050_interrupt_thread(int irq, void *data)
{
	mpu3050_report_xyz(data);

	return IRQ_HANDLED;
}

/* Configures the sampling method */
static int mpu3050_hw_init(struct mpu3050_sensor *sensor)
{
	struct i2c_client *client = sensor->client;
	int ret;
	u8 reg;

	/* Reset */
	ret = i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM,
					MPU3050_PWR_MGM_RESET);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte_data(client, MPU3050_PWR_MGM);
	if (ret < 0)
		return ret;

	ret &= ~MPU3050_PWR_MGM_CLKSEL;
	ret |= MPU3050_PWR_MGM_PLL_Z;
	ret = i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM, ret);
	if (ret < 0)
		return ret;

	/* Output frequency divider. The poll interval */
	ret = i2c_smbus_write_byte_data(client, MPU3050_SMPLRT_DIV,
					MPU3050_DEFAULT_POLL_INTERVAL - 1);
	if (ret < 0)
		return ret;

	/* Set low pass filter and full scale */
	reg = MPU3050_DEFAULT_FS_RANGE;
	reg |= MPU3050_DLPF_CFG_42HZ << 3;
	reg |= MPU3050_EXT_SYNC_NONE << 5;
	ret = i2c_smbus_write_byte_data(client, MPU3050_DLPF_FS_SYNC, reg);
	if (ret < 0)
		return ret;

	return 0;
}

static void __devinit mpu3050_init_idev(struct mpu3050_sensor *sensor,
						struct input_dev *idev)
{
	idev->name = "MPU3050";
	idev->id.bustype = BUS_I2C;
	idev->dev.parent = &sensor->client->dev;

	__set_bit(EV_ABS, idev->evbit);
	input_set_abs_params(idev, ABS_X,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);
	input_set_abs_params(idev, ABS_Y,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);
	input_set_abs_params(idev, ABS_Z,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);
}

static int __devinit mpu3050_create_idev(struct mpu3050_sensor *sensor)
{
	struct input_dev *idev;
	int err;

	idev = input_allocate_device();
	if (!idev)
		return -ENODEV;

	mpu3050_init_idev(sensor, idev);

	idev->open = mpu3050_input_open;
	idev->close = mpu3050_input_close;
	input_set_drvdata(idev, sensor);

	err = input_register_device(idev);
	if (err) {
		input_free_device(idev);
		return err;
	}

	sensor->idev = idev;

	return 0;
}

static int __devinit mpu3050_create_polled_idev(struct mpu3050_sensor *sensor)
{
	struct input_polled_dev *ipoll_dev;
	int err;

	ipoll_dev = input_allocate_polled_device();
	if (!ipoll_dev)
		return -ENOMEM;

	ipoll_dev->private = sensor;
	ipoll_dev->open = mpu3050_poll_open;
	ipoll_dev->close = mpu3050_poll_close;
	ipoll_dev->poll = mpu3050_poll;
	ipoll_dev->poll_interval = MPU3050_DEFAULT_POLL_INTERVAL;
	ipoll_dev->poll_interval_min = MPU3050_MIN_POLL_INTERVAL;
	ipoll_dev->poll_interval_max = MPU3050_MAX_POLL_INTERVAL;

	mpu3050_init_idev(sensor, ipoll_dev->input);

	err = input_register_polled_device(ipoll_dev);
	if (err) {
		input_free_polled_device(ipoll_dev);
		return err;
	}

	sensor->input_polled = ipoll_dev;
	sensor->idev = ipoll_dev->input;

	return 0;
}

static ssize_t mpu3050_attr_get_poll(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mpu3050_sensor *mpu3050 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", mpu3050->poll_div);
}

static ssize_t mpu3050_attr_set_poll(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mpu3050_sensor *mpu3050 = dev_get_drvdata(dev);
	unsigned long uninitialized_var(poll_div);
	int ret, retry = 1;

	if (kstrtoul(buf, 10, &poll_div) || poll_div == 0)
		return -EINVAL;

	poll_div = clamp_t(unsigned long,
			   poll_div,
			   MPU3050_MIN_POLL_INTERVAL,
			   MPU3050_MAX_POLL_INTERVAL);

	pm_runtime_get_sync(mpu3050->dev);
	do {
		ret = i2c_smbus_write_byte_data(mpu3050->client,
						MPU3050_SMPLRT_DIV, poll_div);
		if (!ret)
			break;

		msleep(20);
	} while (retry--);
	pm_runtime_put_sync(mpu3050->dev);

	if (!ret)
		mpu3050->poll_div = (u8) poll_div;
	else
		dev_err(mpu3050->dev, "can't write poll divider\n");

	return size;
}
static DEVICE_ATTR(poll, S_IRUGO | S_IWUSR,
		   mpu3050_attr_get_poll, mpu3050_attr_set_poll);

static struct attribute *mpu3050_attributes[] = {
	&dev_attr_poll.attr,
	NULL
};

static struct attribute_group mpu3050_attr_group = {
	.name = "mpu3050",
	.attrs = mpu3050_attributes
};

/**
 *	mpu3050_probe	-	device detection callback
 *	@client: i2c client of found device
 *	@id: id match information
 *
 *	The I2C layer calls us when it believes a sensor is present at this
 *	address. Probe to see if this is correct and to validate the device.
 *
 *	If present install the relevant sysfs interfaces and input device.
 */
static int __devinit mpu3050_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mpu3050_sensor *sensor;
	int ret;
	int error;

	sensor = kzalloc(sizeof(struct mpu3050_sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	sensor->client = client;
	sensor->dev = &client->dev;

	mpu3050_set_power_mode(client, 1);
	msleep(10);

	ret = i2c_smbus_read_byte_data(client, MPU3050_CHIP_ID_REG);
	if (ret < 0) {
		dev_err(&client->dev, "failed to detect device\n");
		error = -ENXIO;
		goto err_free_mem;
	}

	if (ret != MPU3050_CHIP_ID) {
		dev_err(&client->dev, "unsupported chip id\n");
		error = -ENXIO;
		goto err_free_mem;
	}

	pm_runtime_set_active(&client->dev);

	error = mpu3050_hw_init(sensor);
	if (error)
		goto err_free_mem;

	if (client->irq > 0) {
		error = mpu3050_create_idev(sensor);
		if (error) {
			dev_err(&client->dev, "failed to register input device\n");
			goto err_free_irq;
		}

		error = request_threaded_irq(client->irq,
				NULL, mpu3050_interrupt_thread,
				IRQF_TRIGGER_RISING,
				"mpu3050", sensor);
		if (error) {
			dev_err(&client->dev, "can't get IRQ %d, error %d\n",
					client->irq, error);
			goto err_pm_set_suspended;
		}

		/* Enable interrupts */
		i2c_smbus_write_byte_data(sensor->client, MPU3050_INT_CFG,
					MPU3050_LATCH_INT_EN
					| MPU3050_RAW_RDY_EN
					| MPU3050_MPU_RDY_EN);
		if (ret < 0)
			goto err_free_irq;

		sensor->poll_div = MPU3050_DEFAULT_POLL_INTERVAL;

		dev_set_drvdata(sensor->dev, sensor);
		ret = sysfs_create_group(&client->dev.kobj,
					 &mpu3050_attr_group);
		if (ret < 0) {
			dev_err(&client->dev, "sysfs register failed\n");
			goto err_free_irq;
		}

	} else {
		error = mpu3050_create_polled_idev(sensor);
		if (error) {
			dev_err(&client->dev,
				"failed to register polled input device");
			goto err_pm_set_suspended;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	sensor->es.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10;
	sensor->es.suspend = mpu3050_early_suspend;
	sensor->es.resume = mpu3050_late_resume;
	register_early_suspend(&sensor->es);
#endif
	pm_runtime_enable(&client->dev);
	pm_runtime_set_autosuspend_delay(&client->dev, MPU3050_AUTO_DELAY);

	return 0;

err_free_irq:
	free_irq(client->irq, sensor);
err_pm_set_suspended:
	pm_runtime_set_suspended(&client->dev);
err_free_mem:
	kfree(sensor);
	return error;
}

/**
 *	mpu3050_remove	-	remove a sensor
 *	@client: i2c client of sensor being removed
 *
 *	Our sensor is going away, clean up the resources.
 */
static int __devexit mpu3050_remove(struct i2c_client *client)
{
	struct mpu3050_sensor *sensor = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&sensor->es);
#endif
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	if (client->irq > 0) {
		sysfs_remove_group(&client->dev.kobj, &mpu3050_attr_group);
		free_irq(client->irq, sensor);
		input_unregister_device(sensor->idev);
	} else {
		input_unregister_polled_device(sensor->input_polled);
		input_free_polled_device(sensor->input_polled);
	}
	kfree(sensor);

	return 0;
}

#ifdef CONFIG_PM
/**
 *	mpu3050_suspend		-	called on device suspend
 *	@dev: device being suspended
 *
 *	Put the device into sleep mode before we suspend the machine.
 */
static int mpu3050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	mpu3050_set_power_mode(client, 0);

	return 0;
}

/**
 *	mpu3050_resume		-	called on device resume
 *	@dev: device being resumed
 *
 *	Put the device into powered mode on resume.
 */
static int mpu3050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	mpu3050_set_power_mode(client, 1);
	msleep(10);  /* wait for gyro chip resume */

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mpu3050_early_suspend(struct early_suspend *h)
{
	struct mpu3050_sensor *sensor;
	struct device *dev;

	sensor = container_of(h, struct mpu3050_sensor, es);
	dev = &sensor->client->dev;

	mpu3050_suspend(dev);
}

static void mpu3050_late_resume(struct early_suspend *h)
{
	struct mpu3050_sensor *sensor;
	struct device *dev;

	sensor = container_of(h, struct mpu3050_sensor, es);
	dev = &sensor->client->dev;

	mpu3050_resume(dev);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct dev_pm_ops mpu3050_pm = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	/*
	 * Avoid calling into suspend/resume ops twice when earlysuspend
	 * enabled.
	 */
	SET_SYSTEM_SLEEP_PM_OPS(mpu3050_suspend, mpu3050_resume)
#endif
	SET_RUNTIME_PM_OPS(mpu3050_suspend, mpu3050_resume, NULL)
};

static const struct i2c_device_id mpu3050_ids[] = {
	{ "mpu3050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu3050_ids);

static struct i2c_driver mpu3050_i2c_driver = {
	.driver	= {
		.name	= "mpu3050",
		.owner	= THIS_MODULE,
		.pm	= &mpu3050_pm,
	},
	.probe		= mpu3050_probe,
	.remove		= __devexit_p(mpu3050_remove),
	.id_table	= mpu3050_ids,
};

static int __init mpu3050_init(void)
{
	return i2c_add_driver(&mpu3050_i2c_driver);
}
module_init(mpu3050_init);

static void __exit mpu3050_exit(void)
{
	i2c_del_driver(&mpu3050_i2c_driver);
}
module_exit(mpu3050_exit);

MODULE_AUTHOR("Wistron Corp.");
MODULE_DESCRIPTION("MPU3050 Tri-axis gyroscope driver");
MODULE_LICENSE("GPL");
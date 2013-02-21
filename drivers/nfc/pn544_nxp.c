/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/nfc/pn544.h>
#include <linux/nfc/pn544_nxp.h>

#define MAX_BUFFER_SIZE	512
#define PN544_STANDBY_TIMEOUT	1000	/* ms */

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	struct pn544_nfc_platform_data *platform_data;
	unsigned int		ven_gpio;
	unsigned int		firm_gpio;
	unsigned int		irq_gpio;
	unsigned int		nfc_enable;
	unsigned long		last_active;
};

static inline void pn544_update_active(struct pn544_dev *pn544_dev)
{
	pn544_dev->last_active = jiffies;
}

static bool pn544_is_in_standby(struct pn544_dev *pn544_dev)
{
	unsigned long last_active = pn544_dev->last_active;
	unsigned int delta;

	delta = jiffies_to_msecs(jiffies) - jiffies_to_msecs(last_active);
	return delta > PN544_STANDBY_TIMEOUT;
}

static void pn544_platform_init(struct pn544_dev *pn544_dev)
{
	int polarity, retry, ret;
	char rset_cmd[] = {0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5};
	int count = sizeof(rset_cmd);

	pr_info("%s : detecting nfc_en polarity\n", __func__);

	/* disable fw download */
	gpio_set_value(pn544_dev->firm_gpio, 0);

	for (polarity = 0; polarity < 2; polarity++) {
		pn544_dev->nfc_enable = polarity;
		retry = 3;
		while (retry--) {
			/* power off */
			gpio_set_value(pn544_dev->ven_gpio,
					!pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
			/* power on */
			gpio_set_value(pn544_dev->ven_gpio,
					pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
			/* send reset */
			pr_debug("%s : sending reset cmd\n", __func__);
			ret = i2c_master_send(pn544_dev->client,
					rset_cmd, count);
			if (ret == count) {
				pr_info("%s : nfc_en polarity : active %s\n",
					__func__,
					(polarity == 0 ? "low" : "high"));
				goto out;
			}
		}
	}

	pr_err("%s : could not detect nfc_en polarity, fallback to active high\n",
			__func__);
out:
	/* power off */
	gpio_set_value(pn544_dev->ven_gpio,
			!pn544_dev->nfc_enable);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;

	pr_debug("%s : IRQ ENTER\n", __func__);

	/* Wake up waiting readers */
	wake_up(&pn544_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	/*pr_debug("%s : reading %zu bytes.\n", __func__, count);*/

	mutex_lock(&pn544_dev->read_mutex);

	if (!gpio_get_value(pn544_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		ret = wait_event_interruptible(pn544_dev->read_wq,
				gpio_get_value(pn544_dev->irq_gpio));
		if (ret)
			goto fail;
	}

	/* Read data */
	ret = i2c_master_recv(pn544_dev->client, tmp, count);
	mutex_unlock(&pn544_dev->read_mutex);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : Bytes read = %d: ", __func__, ret);
	return ret;

fail:
	mutex_unlock(&pn544_dev->read_mutex);
	return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev  *pn544_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret, retries;

	pn544_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);

	/*
	 * If the PN544 is in standby mode (it goes there once bus
	 * has been quiet for some time) the first message send to
	 * the device will fail and bring it out of the standby
	 * mode. Data sheet recommends that the host retries to send the
	 * message once more after the failure.
	 *
	 * Delay between retries should be between T2 (5+1ms) and T3 (1s).
	 */
	retries = pn544_is_in_standby(pn544_dev) ? 1 : 0;
	do {
		/* Write data */
		ret = i2c_master_send(pn544_dev->client, tmp, count);
		if (ret < 0)
			usleep_range(6000, 10000);
	} while (ret < 0 && retries-- > 0);

	if (ret != count)
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);

	pn544_update_active(pn544_dev);
	return ret;
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = container_of(filp->private_data,
						struct pn544_dev,
						pn544_device);

	filp->private_data = pn544_dev;

	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static int pn544_dev_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;

	switch (cmd) {
	case PN544_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_info("%s power on with firmware\n", __func__);
			gpio_set_value(pn544_dev->ven_gpio,
					pn544_dev->nfc_enable);
			gpio_set_value(pn544_dev->firm_gpio, 1);
			usleep_range(10000, 15000);
			gpio_set_value(pn544_dev->ven_gpio,
					!pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
			gpio_set_value(pn544_dev->ven_gpio,
					pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
		} else if (arg == 1) {
			/* power on */
			pr_info("%s power on\n", __func__);
			gpio_set_value(pn544_dev->firm_gpio, 0);
			gpio_set_value(pn544_dev->ven_gpio,
					pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
		} else  if (arg == 0) {
			/* power off */
			pr_info("%s power off\n", __func__);
			gpio_set_value(pn544_dev->firm_gpio, 0);
			gpio_set_value(pn544_dev->ven_gpio,
					!pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
			gpio_set_value(pn544_dev->ven_gpio,
					pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
			gpio_set_value(pn544_dev->ven_gpio,
					!pn544_dev->nfc_enable);
			usleep_range(10000, 15000);
		} else {
			pr_err("%s bad arg %u\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static long pn544_dev_ioctl_wrapper(struct file *file, unsigned int cmd,
				    unsigned long arg) {
	long ret;

	ret = pn544_dev_ioctl(file->f_path.dentry->d_inode, file, cmd, arg);

	return ret;
}

static const struct file_operations pn544_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn544_dev_read,
	.write	= pn544_dev_write,
	.open	= pn544_dev_open,
	.unlocked_ioctl  = pn544_dev_ioctl_wrapper,
};

static int pn544_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn544_nfc_platform_data *platform_data;
	struct pn544_dev *pn544_dev;

	pr_debug("%s : entering probe\n", __func__);

	platform_data = client->dev.platform_data;
	if (platform_data == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
	if (pn544_dev == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = platform_data->request_resources(client);
	if (ret)
		goto err_exit;

	pn544_dev->irq_gpio = platform_data->get_gpio(NFC_GPIO_IRQ);
	pn544_dev->ven_gpio  = platform_data->get_gpio(NFC_GPIO_ENABLE);
	pn544_dev->firm_gpio  = platform_data->get_gpio(NFC_GPIO_FW_RESET);
	pn544_dev->client   = client;
	pn544_dev->platform_data = platform_data;

	pn544_platform_init(pn544_dev);

	/* init mutex and queues */
	init_waitqueue_head(&pn544_dev->read_wq);
	mutex_init(&pn544_dev->read_mutex);

	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	pn544_dev->pn544_device.name = "pn544";
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	ret = request_irq(client->irq, pn544_dev_irq_handler,
			  IRQF_TRIGGER_RISING, client->name, pn544_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}

	i2c_set_clientdata(client, pn544_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	mutex_destroy(&pn544_dev->read_mutex);
	kfree(pn544_dev);
err_exit:
	platform_data->free_resources();
	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn544_dev);
	misc_deregister(&pn544_dev->pn544_device);
	mutex_destroy(&pn544_dev->read_mutex);
	pn544_dev->platform_data->free_resources();
	kfree(pn544_dev);

	return 0;
}

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};

static struct i2c_driver pn544_driver = {
	.id_table	= pn544_id,
	.probe		= pn544_probe,
	.remove		= pn544_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
	},
};

/*
 * module load/unload record keeping
 */

static int __init pn544_dev_init(void)
{
	pr_info("Loading pn544 driver\n");
	return i2c_add_driver(&pn544_driver);
}
module_init(pn544_dev_init);

static void __exit pn544_dev_exit(void)
{
	pr_info("Unloading pn544 driver\n");
	i2c_del_driver(&pn544_driver);
}
module_exit(pn544_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");

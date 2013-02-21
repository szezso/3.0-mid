/* drivers/i2c/chips/a1026.c - a1026 voice processor driver
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/a1026.h>
#include <linux/firmware.h>
/* FIXME: once sound moves we can fix this */
#include "../staging/intel_sst/intel_sst.h"

/*
 * This driver is based on the eS305-UG-APIGINTEL-V0 2.pdf spec
 * for the eS305 Voice Processor
 */

struct vp_ctxt {
	unsigned char *data;
	unsigned int img_size;
	struct i2c_client *i2c_dev;
	struct a1026_platform_data *pdata;
} *es305;

/* Tunables */
#define RETRY_CNT		5
#define POLLING_RETRY_CNT	3
#define MAX_SIZE		8192	/* Largest buffer */

/* This is a mess and will all blow up if you ever have multiple instances */
static int execute_cmdmsg(struct vp_ctxt *vp, unsigned int msg);
static int suspend(struct vp_ctxt *vp);
static struct mutex a1026_mutex;
static long es305_opened;
static int es305_suspended;

static void debug_hex_dump(struct device *dev, const u8 *data,
				const char *name, int length)
{
	int i;
	for (i = 0; i < length; i++)
		dev_dbg(dev, "%s[%d] = %02x\n", name, i, data[i]);
}

static struct device *vp_to_dev(struct vp_ctxt *vp)
{
	return &vp->i2c_dev->dev;
}

static int es305_i2c_read(struct vp_ctxt *vp, u8 *rx_data, int length)
{
	int rc;
	struct i2c_client *client = vp->i2c_dev;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rx_data,
		},
	};

	rc = i2c_transfer(client->adapter, msgs, 1);
	if (rc < 0) {
		dev_dbg(&client->dev, "rx error %d\n", rc);
		return rc;
	}
	debug_hex_dump(&client->dev, rx_data, "rx", length);
	return 0;
}

static int es305_i2c_write(struct vp_ctxt *vp, const u8 *tx_data, int length)
{
	int rc;
	struct i2c_client *client = vp->i2c_dev;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = (char *)tx_data,
		},
	};

	rc = i2c_transfer(client->adapter, msg, 1);
	if (rc < 0) {
		dev_dbg(&client->dev, "tx error %d\n", rc);
		return rc;
	}
	debug_hex_dump(&client->dev, tx_data, "tx", length);
	return 0;
}

static void es305_i2c_sw_reset(struct vp_ctxt *vp, unsigned int reset_cmd)
{
	int rc;
	u8 msgbuf[4];

	msgbuf[0] = (reset_cmd >> 24) & 0xFF;
	msgbuf[1] = (reset_cmd >> 16) & 0xFF;
	msgbuf[2] = (reset_cmd >> 8) & 0xFF;
	msgbuf[3] = reset_cmd & 0xFF;

	rc = es305_i2c_write(vp, msgbuf, 4);
	if (!rc)
		msleep(20);
		/* 20ms is recommended polling period -- p8 spec*/
}

static int es305_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &es305_opened))
		return -EBUSY;
	file->private_data = es305;
	es305->img_size = 0;
	es305_opened = 1;
	return 0;
}


static int es305_release(struct inode *inode, struct file *file)
{
	clear_bit(0, &es305_opened);
	return 0;
}

static int suspend(struct vp_ctxt *vp)
{
	int rc;

	/* Put es305 into sleep mode */
	rc = execute_cmdmsg(vp, A100_msg_Sleep);
	if (rc < 0) {
		dev_err(vp_to_dev(vp), "suspend failed\n");
		return rc;
	}
	es305_suspended = 1;
	msleep(120); /* 120 defined by fig 2 of eS305 as the time to wait
			before clock gating */
	rc = intel_sst_set_pll(false, SST_PLL_AUDIENCE);
	if (rc)
		dev_err(vp_to_dev(vp),
			"ipc clk disable command failed: %d\n", rc);
	return rc;
}

static ssize_t es305_bootup_init(struct vp_ctxt *vp)
{
	int rc, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	int i;
	const unsigned char *index;
	char buf[2];
	const struct firmware *fw_entry;
	struct device *dev = vp_to_dev(vp);

	if (request_firmware(&fw_entry, "vpimg_es305b.bin", dev)) {
		dev_err(dev, "Firmware not available\n");
		return -EFAULT;
	}

	if (fw_entry->size > A1026_MAX_FW_SIZE) {
		dev_err(dev, "invalid es305 image size %d\n", fw_entry->size);
		return -EINVAL;
	}

	while (retry--) {
		/* Reset es305 chip */
		vp->pdata->reset(0);
		/* Take out of reset */
		vp->pdata->reset(1);
		msleep(50); /* Delay defined in Figure 1 of eS305 spec */

		/* Boot Cmd to es305 */
		buf[0] = A1026_msg_BOOT >> 8;
		buf[1] = A1026_msg_BOOT & 0xff;

		rc = es305_i2c_write(vp, buf, 2);
		if (rc < 0) {
			dev_err(dev, "set boot mode error (%d retries left)\n",
								retry);
			continue;
		}

		mdelay(1); /* eS305 internal delay */
		rc = es305_i2c_read(vp, buf, 1);
		if (rc < 0) {
			dev_dbg(dev, "boot mode ack error (%d retries left)\n",
								retry);
			continue;
		}

		if (buf[0] != A1026_msg_BOOT_ACK) {
			dev_dbg(dev, "not a boot-mode ack (%d retries left)\n",
								retry);
			continue;
		}
		dev_dbg(dev, "ACK =  %d\n", buf[0]);
		remaining = fw_entry->size;
		index = fw_entry->data;

		dev_dbg(dev, "starting to load image (%d passes)...\n",
								remaining);

		for (i = 0; i < remaining; i++) {
			rc = es305_i2c_write(vp, index, 1);
			index++;
			if (rc < 0)
				break;
		}
		dev_dbg(dev, "starting to load image (%s index)...\n", index);

		if (rc < 0) {
			dev_dbg(dev, "fw load error %d (%d retries left)\n",
								rc, retry);
			continue;
		}
		msleep(100); /* Delay time before issue a Sync Cmd
				BUGBUG should be 10*/

		dev_dbg(dev, "firmware loaded successfully\n");

		rc = execute_cmdmsg(vp, A100_msg_Sync);
		if (rc < 0) {
			dev_dbg(dev,
				"sync command error %d (%d retries left)\n",
								rc, retry);
			continue;
		}
		pass = 1;
		break;
	}
	rc = suspend(vp);
	if (pass && !rc)
		dev_dbg(dev, "initialized!\n");
	else
		dev_dbg(dev, "initialization failed\n");

	release_firmware(fw_entry);
	return rc;
}

static int chk_wakeup_es305(struct vp_ctxt *vp)
{
	int rc, retry = 3;

	if (!es305_suspended)
		return 0;

	vp->pdata->wakeup(0);
	msleep(120); /* fig 3 eS305 spec.  BUGBUG should be 30 */

	do {
		rc = execute_cmdmsg(vp, A100_msg_Sync);
	} while (rc < 0 && --retry);

	vp->pdata->wakeup(1);
	if (rc < 0)
		dev_err(vp_to_dev(vp), "wakeup failed (%d)\n", rc);
	else
		es305_suspended = 0;
	return rc;
}

int execute_cmdmsg(struct vp_ctxt *vp, unsigned int msg)
{
	struct device *dev = vp_to_dev(vp);
	int rc;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];
	unsigned int sw_reset;

	sw_reset = ((A100_msg_BootloadInitiate << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	memcpy(chkbuf, msgbuf, 4);

	rc = es305_i2c_write(vp, msgbuf, 4);
	if (rc < 0) {
		dev_dbg(dev, "%s: error %d\n", __func__, rc);
		es305_i2c_sw_reset(vp, sw_reset);
		return rc;
	}

	/* We don't need to get Ack after sending out a suspend command */
	if (msg == A100_msg_Sleep)
		return rc;

	retries = POLLING_RETRY_CNT;
	msleep(1);		/* BUGBUG should be 20 p8 spec */
	while (retries--) {
		rc = es305_i2c_read(vp, msgbuf, 4);
		if (rc < 0) {
			dev_dbg(dev, "ack-read error %d (%d retries)\n",
								rc, retries);
			continue;
		}

		if (msgbuf[0] == 0x80  && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			dev_dbg(dev, "ACK OF SYNC CMD\n");
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			dev_dbg(dev, "invalid cmd %08x\n", msg);
			rc = -EINVAL;
			break;
		} else if (msgbuf[0] == 0x00 && msgbuf[1] == 0x00) {
			dev_dbg(dev, "not ready (%d retries)\n", retries);
			rc = -EBUSY;
		} else {
			dev_dbg(dev, "cmd/ack mismatch: (%d retries left)\n",
								retries);
			debug_hex_dump(dev, msgbuf, "msgbug", 4);
			rc = -EBUSY;
		}
		msleep(20); /* eS305 spec p. 8 : use polling */
	}

	if (!pass) {
		dev_err(dev, "failed execute cmd %08x (%d)\n", msg, rc);
		es305_i2c_sw_reset(vp, sw_reset);
	}
	return rc;
}


static ssize_t es305_write(struct file *file, const char __user *buff,
	size_t count, loff_t *offp)
{
	struct vp_ctxt *vp = file->private_data;
	struct device *dev = vp_to_dev(vp);

	int rc, msg_buf_count, i, num_fourbyte;
	unsigned char *kbuf;
	unsigned int sw_reset;
	int size_cmd_snd = 4;

	if (count > MAX_SIZE)
		return -EMSGSIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buff, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	debug_hex_dump(dev, kbuf, "kbuf", count);
	sw_reset = ((A100_msg_BootloadInitiate << 16) | RESET_IMMEDIATE);

	mutex_lock(&a1026_mutex);
	rc = chk_wakeup_es305(vp);
	if (rc < 0)
		goto error;

	msg_buf_count = 0;
	num_fourbyte = count / size_cmd_snd;
	for (i = 0; i < num_fourbyte ; i++) {
		rc = es305_i2c_write(vp, &kbuf[msg_buf_count], size_cmd_snd);
		if (rc < 0) {
			dev_err(dev, "block write error!\n");
			goto reset_error;
		}
		mdelay(1);
#ifdef DEBUG
	{
		unsigned char msgbuf[4];
		memset(msgbuf, 0, sizeof(msgbuf));
		rc = es305_i2c_read(vp, msgbuf, 4);
		if (rc < 0) {
			dev_err(dev, "ES305 CMD block read error!\n");
			goto reset_error;
		}
		if (memcmp(kbuf + msg_buf_count, msgbuf, 4)) {
			dev_err(dev, "E305 cmd not ack'ed\n");
			rc = -ENXIO;
			goto error;
		}
		dev_dbg(dev, "ES305 WRITE:(0x%.2x%.2x%.2x%.2x)\n",
			kbuf[msg_buf_count], kbuf[msg_buf_count + 1],
			kbuf[msg_buf_count + 2], kbuf[msg_buf_count + 3]);
		dev_dbg(dev, "ES305 READ:(0x%.2x%.2x%.2x%.2x)\n",
			msgbuf[0], msgbuf[1], msgbuf[2], msgbuf[3]);
	}
#endif
		msg_buf_count += 4;
	}
	mutex_unlock(&a1026_mutex);
	kfree(kbuf);
	return count;

reset_error:
	es305_i2c_sw_reset(vp, sw_reset);
error:
	mutex_unlock(&a1026_mutex);
	kfree(kbuf);
	return rc;
}

static ssize_t es305_read(struct file *file, char __user *buff,
	size_t count, loff_t *offp)
{
	struct vp_ctxt *vp = file->private_data;
	unsigned char kbuf[4];
	int rc;

	mutex_lock(&a1026_mutex);
	rc = es305_i2c_read(vp, kbuf, 4);
	mutex_unlock(&a1026_mutex);
	if (rc < 0)
		return rc;
	count = min_t(int, count, rc);
	debug_hex_dump(vp_to_dev(vp), kbuf, "kbuf", count);
	if (copy_to_user(buff, kbuf, count))
		return -EFAULT;
	return count;
}

static long a1026_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct vp_ctxt *vp = file->private_data;
	struct device *dev = vp_to_dev(vp);
	int rc;

	mutex_lock(&a1026_mutex);
	switch (cmd) {
	case A1026_BOOTUP_INIT:
		rc = es305_bootup_init(vp);
		break;
	case A1026_SUSPEND:
		rc = suspend(vp);
		break;
	case A1026_ENABLE_CLOCK:
		rc = intel_sst_set_pll(true, SST_PLL_AUDIENCE);
		if (rc)
			dev_err(dev,
				"ipc clk enable command failed: %d\n", rc);
		break;
	default:
		rc = -ENOTTY;
		break;
	}
	mutex_unlock(&a1026_mutex);
	return rc;
}

static const struct file_operations a1026_fops = {
	.owner = THIS_MODULE,
	.open = es305_open,
	.release = es305_release,
	.write = es305_write,
	.read = es305_read,
	.unlocked_ioctl = a1026_ioctl,
};

static struct miscdevice a1026_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "audience_es305",
	.fops = &a1026_fops,
};

static int a1026_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int rc;
	struct vp_ctxt *vp;
	struct a1026_platform_data *pdata;

	dev_dbg(&client->dev, "probe\n");

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "platform data is invalid\n");
		return -EIO;
	}

	vp = kzalloc(sizeof(struct vp_ctxt), GFP_KERNEL);
	if (!vp) {
		rc = -ENOMEM;
		dev_err(&client->dev, "platform data is out of memory\n");
		goto err_exit;
	}

	vp->i2c_dev = client;
	i2c_set_clientdata(client, vp);

	rc = pdata->request_resources(client);
	if (rc) {
		dev_err(&client->dev, "Cannot get ressources\n");
		goto err_kfree;
	}
	mutex_init(&a1026_mutex);
	rc = misc_register(&a1026_device);
	if (rc) {
		dev_err(&client->dev, "es305_device register failed\n");
		goto err_misc_register;
	}

	es305 = vp;
	es305->pdata = pdata;
	pdata->wakeup(1);
	pdata->reset(1);

	return 0;

err_misc_register:
	mutex_destroy(&a1026_mutex);
err_kfree:
	kfree(vp);
err_exit:
	return rc;
}

static int a1026_remove(struct i2c_client *client)
{
	struct vp_ctxt *vp = i2c_get_clientdata(client);
	misc_deregister(&a1026_device);
	mutex_destroy(&a1026_mutex);
	vp->pdata->release_resources(client);
	kfree(vp);
	return 0;
}

/* FIXME: probably want to suspend here if not already suspended */
static int a1026_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int a1026_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id a1026_id[] = {
	{ "audience_es305", 0 },
	{ }
};

static struct i2c_driver a1026_driver = {
	.probe = a1026_probe,
	.remove = a1026_remove,
	.suspend = a1026_suspend,
	.resume	= a1026_resume,
	.id_table = a1026_id,
	.driver = {
		.name = "audience_es305",
	},
};

static int __init a1026_init(void)
{
	return i2c_add_driver(&a1026_driver);
}

static void __exit a1026_exit(void)
{
	i2c_del_driver(&a1026_driver);
}

module_init(a1026_init);
module_exit(a1026_exit);

MODULE_DESCRIPTION("A1026 voice processor driver");
MODULE_LICENSE("GPL");

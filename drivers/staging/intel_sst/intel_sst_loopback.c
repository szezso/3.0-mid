/*
 *  intel_sst_loopback.c
 *
 *  Copyright (C) 2008-11	Intel Corp
 *  Authors:	Vikas Gupta <vikas.gupta@intel.com>
 *		Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Hongbing Hu <hongbing.hu@intel.com>
 *		Prem Sasidharan <prem.sasidharan@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file contains all the functions related to loopback
 */

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include "intel_sst.h"
#include "intel_sst_common.h"
#include "intel_sst_loopback.h"

#ifdef INCLUDE_LOOPBACK_IF
static int intel_sst_loopback_open(struct inode *i_node,
				struct file *file_ptr);
static int intel_sst_loopback_release(struct inode *i_node,
				struct file *file_ptr);
static int intel_sst_loopback_read(struct file *file_ptr,
				char __user *buf, size_t count, loff_t *offset);

struct loopback_app_if loopback_if;

/* fops Routines */
static const struct file_operations intel_sst_loopback_fops = {
	.owner = THIS_MODULE,
	.open = intel_sst_loopback_open,
	.release = intel_sst_loopback_release,
	.read = intel_sst_loopback_read,
	/*.poll = intel_sst_loopback_poll,*/
};

static struct miscdevice lpe_loopback_dev = {
	/* dynamic allocation */
	.minor = MISC_DYNAMIC_MINOR,
	/* /dev/intel_loopback_sst */
	.name = "intel_loopback_sst",
	.fops = &intel_sst_loopback_fops
};

int loopback_register(void)
{
	int	retval;

	loopback_if.user = 0;
	loopback_if.flag = 0;
	loopback_if.rd_offset = 0;
	loopback_if.valid_len = 0;
	loopback_if.valid_size = 0;

	loopback_if.size = (
			(SOC_AUDIO_OUTPUT_SAMPLE_RATE *
			 SOC_AUDIO_MAX_SAMPLE_WIDTH *
			 SOC_AUDIO_OUTPUT_CHANNELS *
			 SOC_AUDIO_BUFFER_TIME_MS) / 1000) * 17;

	loopback_if.mem = kzalloc(loopback_if.size, GFP_KERNEL);

	if (NULL == loopback_if.mem) {
		pr_err("sst:loopback memory alloc failed\n");
		return -ENOMEM;
	}

	mutex_init(&loopback_if.lock);
	init_waitqueue_head(&loopback_if.sleep);
	spin_lock_init(&loopback_if.spin_lock);

	retval = misc_register(&lpe_loopback_dev);
	if (retval) {
		pr_err("sst:registering with loopback failed\n");
		kfree(loopback_if.mem);
		loopback_if.mem = NULL;
	}

	return retval;
}

int loopback_deregister(void)
{
	loopback_if.flag = 1;
	wake_up(&loopback_if.sleep);
	loopback_if.user = 0;
	if (NULL != loopback_if.mem)
		kfree(loopback_if.mem);
	loopback_if.mem = NULL;
	loopback_if.rd_offset = 0;
	loopback_if.valid_len = 0;
	loopback_if.valid_size = 0;
	return misc_deregister(&lpe_loopback_dev);
}

void intel_sst_job_processed_notify(void)
{
	unsigned long flags;
	struct ipc_ia_time_stamp_t __iomem *fw_tstamp;

	spin_lock_irqsave(&loopback_if.spin_lock, flags);

	if (loopback_if.user != 0) {
		fw_tstamp = (struct ipc_ia_time_stamp_t *)
				((sst_drv_ctx->mailbox +
				SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET));
		fw_tstamp += SND_SST_MAX_AUDIO_DEVICES;

		if (fw_tstamp->data_consumed) {
			loopback_if.flag = 1;
			/* size of data transfered in each period varies
			 * beacuse of SRC. valid_size, valid_len needs to be
			 * updated for each period */
			loopback_if.valid_len = (fw_tstamp->bytes_processed);
			if (loopback_if.valid_len >= loopback_if.valid_size)
				loopback_if.valid_size = loopback_if.valid_len;
			fw_tstamp->data_consumed = 0;
			wake_up(&loopback_if.sleep);
		}
	}
	spin_unlock_irqrestore(&loopback_if.spin_lock, flags);

	return;
}

/**
 * intel_sst_loopback_open - opens a handle to driver(/dev/intel_sst_loopback)
 *
 * @i_node:	inode structure
 * @file_ptr:pointer to file
 *
 * This function is called by OS when a user space component
 * tries to get a driver handle. Only one handle at a time
 * will be allowed
 */
static int intel_sst_loopback_open(struct inode *i_node, struct file *file_ptr)
{
	pr_debug("sst: loopback_open\n");

	mutex_lock(&loopback_if.lock);

	if (loopback_if.user) {
		mutex_unlock(&loopback_if.lock);
		return -EMFILE;
	}

	loopback_if.user++;

	mutex_unlock(&loopback_if.lock);
	return 0;
}

/**
 * intel_sst_loopback_release - releases a handle to driver
 *
 * @i_node:	inode structure
 * @file_ptr:	pointer to file
 *
 * This function is called by OS when a user space component
 * tries to release a driver handle.
 */
static int intel_sst_loopback_release(struct inode *i_node,
		struct file *file_ptr)
{
	mutex_lock(&loopback_if.lock);
	loopback_if.user = 0;
	mutex_unlock(&loopback_if.lock);
	return 0;
}

/*
 * intel_sst_read - read the encoded data
 *
 * @file_ptr: pointer to file
 * @buf: user buffer to be filled with captured data
 * @count: size of tthe buffer
 * @offset: offset to start from
 *
 * This function is called when user tries to capture data
 */
static int intel_sst_loopback_read(struct file *file_ptr, char __user *buf,
			size_t count, loff_t *offset)
{
	uint8_t	*rd_ptr;
	unsigned long flags;
	unsigned int retval;
	unsigned int bytes_to_copy;

	mutex_lock(&loopback_if.lock);
	spin_lock_irqsave(&loopback_if.spin_lock, flags);

	if (loopback_if.valid_size <= loopback_if.rd_offset) {
		spin_unlock_irqrestore(&loopback_if.spin_lock, flags);
		mutex_unlock(&loopback_if.lock);
		wait_event(loopback_if.sleep, (loopback_if.flag != 0));
		mutex_lock(&loopback_if.lock);
		spin_lock_irqsave(&loopback_if.spin_lock, flags);
		loopback_if.flag = 0;
	}

	if (loopback_if.rd_offset < loopback_if.valid_size) {
		/* (loopback_if.valid_size - loopback_if.rd_offset)
		 * bytes available for copy */
		bytes_to_copy = loopback_if.valid_size - loopback_if.rd_offset;
	} else {
		/* rd_offset needs to wrapped around
		 * bytes available is valid_len,
		 * valid_size reset to zero */
		loopback_if.rd_offset = 0;
		loopback_if.valid_size = 0;
		bytes_to_copy = loopback_if.valid_len;
	}
	rd_ptr = (uint8_t *)(loopback_if.mem);
	rd_ptr += loopback_if.rd_offset;

	bytes_to_copy = min(bytes_to_copy, count);
	spin_unlock_irqrestore(&loopback_if.spin_lock, flags);
	retval = copy_to_user(buf, rd_ptr, bytes_to_copy);
	if (retval || loopback_if.user == 0) {
		mutex_unlock(&loopback_if.lock);
		return -EFAULT;
	}

	loopback_if.rd_offset += bytes_to_copy;

	mutex_unlock(&loopback_if.lock);
	return bytes_to_copy;
}
#endif

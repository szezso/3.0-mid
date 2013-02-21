/*
 * OSIP driver for Medfield.
 *
 * Copyright (C) 2011 Intel Corp
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/reboot.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/debugfs.h>
#include <linux/genhd.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>

/* Boot target policies in place:
 *
 * 1. Normal reboots don't mess with the OSII records, they leave them
 * unchanged
 * 2. If osip.bootrestore is nonzero (default), all OSII records restored
 * on boot, unless a magic key is being held
 * 3. If the system is rebooted with "fastboot" or "recovery" as a parameter,
 * the appropriate OSII entries will be invalidated in the reboot hook such
 * that the device reboots into the desired mode.
 * 4. If "fastboot or "recovery" are written to "/proc/osip", the appropriate
 * OSII entries will be invalidated, such that when the device is next booted
 * or powercycled, it will reboot into the desired mode.
 * 5. If something other than "recovery" or "fastboot" are written to
 * "/proc/osip", all OSII entries will be restored */

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "osip."
static int bootrestore = 1;
module_param(bootrestore, int, S_IRUGO);
MODULE_PARM_DESC(bootrestore, "If nonzero, restore OSII entries on boot rather"
		" than at reboot()");

static int mkeys[6];
static int mkeys_count;
module_param_array(mkeys, int, &mkeys_count, S_IRUGO);
MODULE_PARM_DESC(mkeys, "List of magic GPIOs to check on boot. If a GPIO is"
		" matched, boot the corresponding OSII entry (entries start "
		"after the Android image). Use negative values for active "
		"high GPIOs.");

static int magic_target;

/* hardcoding addresses. According to the FAS, this is how the
 * OS image blob has to be loaded, and where is the bootstub
 * entry point */
#define OS_LOAD_ADDRESS     0x1100000
#define OS_ENTRY_POINT      0x1101000

/* OSII attributes that indicate the image type. We need
 * these to determine if a particular OSII entry is a
 * bootable kernel image */
#define ATTR_SIGNED_MAIN_OS		0x00
#define ATTR_UNSIGNED_MAIN_OS		0x01
#define ATTR_SIGNED_RECOVERY_OS		0x0C
#define ATTR_UNSIGNED_RECOVERY_OS	0x0D
#define ATTR_SIGNED_PROVISIONING_OS	0x0E
#define ATTR_UNSIGNED_PROVISIONING_OS	0x0F

/* change to "loop0" and use losetup for safe testing */
#define OSIP_BLKDEVICE "mmcblk0"

/* OSIP backup will be stored with this offset in the first sector */
#define OSIP_BACKUP_OFFSET 0xE0
#define MAX_OSII (7)
struct OSII {                   /* os image identifier */
	uint16_t os_rev_minor;
	uint16_t os_rev_major;
	uint32_t logical_start_block;

	uint32_t ddr_load_address;
	uint32_t entry_point;
	uint32_t size_of_os_image;

	uint8_t attribute;
	uint8_t reserved[3];
};

struct OSIP_header {            /* os image profile */
	uint32_t sig;
	uint8_t intel_reserved;
	uint8_t header_rev_minor;
	uint8_t header_rev_major;
	uint8_t header_checksum;
	uint8_t num_pointers;
	uint8_t num_images;
	uint16_t header_size;
	uint32_t reserved[5];

	struct OSII desc[MAX_OSII];
};
int mmcblk0_match(struct device *dev, void *data)
{
	if (strcmp(dev_name(dev), OSIP_BLKDEVICE) == 0)
		return 1;
	return 0;
}
static struct block_device *get_emmc_bdev(void)
{
	struct block_device *bdev;
	struct device *emmc_disk;

	emmc_disk = class_find_device(&block_class, NULL, NULL, mmcblk0_match);
	if (emmc_disk == 0) {
		pr_err("emmc not found!\n");
		return NULL;
	}
	/* partition 0 means raw disk */
	bdev = bdget_disk(dev_to_disk(emmc_disk), 0);
	if (bdev == NULL) {
		dev_err(emmc_disk, "unable to get disk\n");
		return NULL;
	}
	/* Note: this bdev ref will be freed after first
	   bdev_get/bdev_put cycle */
	return bdev;
}
static uint8_t calc_checksum(void *_buf, int size)
{
	int i;
	uint8_t checksum = 0, *buf = (uint8_t *)_buf;
	for (i = 0; i < size; i++)
		checksum = checksum ^ (buf[i]);
	return checksum;
}
/*
  Allows to access the osip image. Callback is passed for user to
  implement actual usage.
  This function takes care of the blkdev housekeeping

  how to do proper block access is got from:
  fs/partitions/check.c
  mtd/devices/block2mtd.c
*/
/* callbacks returns whether the OSIP was modified */
typedef int (*osip_callback_t)(struct OSIP_header *osip, void *data);

static int access_osip_record(osip_callback_t callback, void *cb_data)
{
	Sector sect;
	struct block_device *bdev;
	char *buffer;
	struct OSIP_header *osip;
	struct OSIP_header *osip_backup;
	int ret = 0;
	int dirty = 0;

	bdev = get_emmc_bdev();
	if (bdev == NULL) {
		pr_err("%s: get_emmc failed!\n", __func__);
		return -ENODEV;
	}
	/* make sure the block device is open rw */
	ret = blkdev_get(bdev, FMODE_READ|FMODE_WRITE, NULL);
	if (ret < 0) {
		pr_err("%s: blk_dev_get failed!\n", __func__);
		return -ret;
	}
	/* get memmap of the OSIP header */
	buffer = read_dev_sector(bdev, 0, &sect);

	if (buffer == NULL) {
		ret = -ENODEV;
		goto bd_put;
	}
	osip = (struct OSIP_header *) buffer;
	/* some sanity checks */
	if (osip->header_size <= 0 || osip->header_size > PAGE_SIZE) {
		pr_err("%s: corrupted osip!\n", __func__);
		ret = -EINVAL;
		goto put_sector;
	}
	if (calc_checksum(osip, osip->header_size) != 0) {
		pr_err("%s: corrupted osip!\n", __func__);
		ret = -EINVAL;
		goto put_sector;
	}
	/* store the OSIP backup which will be used to recover in PrOS */
	osip_backup = kmalloc(sizeof(struct OSIP_header), GFP_KERNEL);
	if (osip_backup == NULL)
		goto put_sector;
	memcpy(osip_backup, osip, sizeof(struct OSIP_header));

	lock_page(sect.v);
	dirty = callback(osip, cb_data);
	if (dirty) {
		memcpy(buffer + OSIP_BACKUP_OFFSET, osip_backup,
		       sizeof(struct OSIP_header));
		osip->header_checksum = 0;
		osip->header_checksum = calc_checksum(osip, osip->header_size);
		set_page_dirty(sect.v);
	}
	unlock_page(sect.v);
	sync_blockdev(bdev);
	kfree(osip_backup);
put_sector:
	put_dev_sector(sect);
bd_put:
	blkdev_put(bdev, FMODE_READ|FMODE_WRITE);
	return 0;
}

static int is_os_image(int id, struct OSIP_header *osip)
{
	switch (osip->desc[id].attribute) {
	case ATTR_SIGNED_MAIN_OS:
	case ATTR_UNSIGNED_MAIN_OS:
	case ATTR_SIGNED_RECOVERY_OS:
	case ATTR_UNSIGNED_RECOVERY_OS:
	case ATTR_SIGNED_PROVISIONING_OS:
	case ATTR_UNSIGNED_PROVISIONING_OS:
		return 1;
	default:
		return 0;
	}
}

static void restore_osii(int id, struct OSIP_header *osip)
{
	if (osip->desc[id].ddr_load_address != OS_LOAD_ADDRESS) {
		pr_debug("osip: restore OSII%d\n", id);
		osip->desc[id].ddr_load_address = OS_LOAD_ADDRESS;
		osip->desc[id].entry_point = OS_ENTRY_POINT;
	}
}

static void invalidate_osii(int id, struct OSIP_header *osip)
{
	if (osip->desc[id].ddr_load_address != 0) {
		pr_debug("osip: invalidate OSII%d\n", id);
		osip->desc[id].ddr_load_address = 0;
		osip->desc[id].entry_point = 0;
	}
}

/* Invalidate the specified number of kernel OSII records, restoring
 * any subsequent ones if necessary */
static int osip_invalidate(struct OSIP_header *osip, void *data)
{
	int num_records = (int)data;
	int num_kernels = 0;
	int i;

	/* Sanity check: Count the number of kernel images first; don't
	 * let the user invalidate all of them */
	for (i = 0; i < osip->num_pointers; i++) {
		if (is_os_image(i, osip))
			num_kernels++;
	}
	if (num_kernels <= num_records) {
		pr_err("osip: Refusing to invalidate %d records; "
				"only %d records exist\n", num_records,
				num_kernels);
		return 0;
	}

	for (i = 0; i < osip->num_pointers; i++) {
		if (!is_os_image(i, osip)) {
			/* The presence of non-os images in the OSIP
			 * shifts the indices, we are only counting
			 * kernel images */
			continue;
		}
		if (num_records) {
			invalidate_osii(i, osip);
			num_records--;
		} else
			restore_osii(i, osip);
	}
	return 1;
}

/* Restore the records in  the OSII entries in the OSIP for all OS
 * images, skipping other types */
static int osip_restore(struct OSIP_header *osip, void *data)
{
	int id;
	for (id = 0; id < osip->num_pointers; id++) {
		if (!is_os_image(id, osip))
			continue;
		restore_osii(id, osip);
	}
	pr_info("osip: Next boot will be into default kernel\n");
	return 1;
}

static int process_boot_cmd(char *cmd)
{
	if (0 == strcmp(cmd, "recovery")) {
		access_osip_record(osip_invalidate, (void *)1);
		pr_info("osip: Next boot will be in recovery mode\n");
		return 0;
	} else if (0 == strcmp(cmd, "fastboot") ||
		  (0 == strcmp(cmd, "bootloader"))) {
		/* Please ensure that if adding additional OS images,
		 * fastboot is always the last one; fastboot may not
		 * be present on production builds */
		access_osip_record(osip_invalidate, (void *)2);
		pr_info("osip: Next boot will be in fastboot mode\n");
		return 0;
	}
	return -1;
}

static int osip_reboot_notifier_call(struct notifier_block *notifier,
				     unsigned long what, void *data)
{
	int ret = NOTIFY_DONE;
	char *cmd = (char *)data;

	if (what != SYS_RESTART || !data)
		return NOTIFY_DONE;

	if (!process_boot_cmd(cmd))
		ret = NOTIFY_OK;

	return ret;
}

#ifdef DEBUG
/* to test without reboot...
echo -n fastboot >/sys/module/intel_mid_osip/parameters/test
*/
static int osip_test_write(const char *val, struct kernel_param *kp)
{
	osip_reboot_notifier_call(NULL, SYS_RESTART, (void *)val);
	return 0;
}

static int osip_test_read(char *buffer, struct kernel_param *kp)
{
	return 0;
}

module_param_call(test, osip_test_write,
		osip_test_read, NULL, 0644);

#endif

static struct notifier_block osip_reboot_notifier = {
	.notifier_call = osip_reboot_notifier_call,
};

/* useful for engineering, not for product */
#ifdef CONFIG_INTEL_MID_OSIP_DEBUG_FS
/* show and edit boot.bin's cmdline */

struct cmdline_priv {
	Sector sect;
	struct block_device *bdev;
	int lba;
	char *cmdline;
	int osip_id;
};

static int osip_find_cmdline(struct OSIP_header *osip, void *data)
{
	struct cmdline_priv *p = (struct cmdline_priv *) data;
	if (p->osip_id < MAX_OSII)
		p->lba = osip->desc[p->osip_id].logical_start_block;
	return 0;
}
int open_cmdline(struct inode *i, struct file *f)
{
	struct cmdline_priv *p;
	int ret ;
	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (i->i_private)
		p->osip_id = (int) i->i_private;
	f->private_data = 0;
	access_osip_record(osip_find_cmdline, (void *)p);
	/* need to open it again */
	p->bdev = get_emmc_bdev();
	if (!p->bdev) {
		pr_err("%s:access_osip_record failed!\n", __func__);
		ret = -ENODEV;
		goto free;
	}
	ret = blkdev_get(p->bdev, f->f_mode);
	if (ret < 0) {
		pr_err("%s: blk_dev_get failed!\n", __func__);
		goto put;
	}
	if (p->lba >= get_capacity(p->bdev->bd_disk)) {
		pr_err("%s: %d out of disk bound!\n", __func__, p->lba);
		ret = -EINVAL;
		goto put;
	}
	p->cmdline = read_dev_sector(p->bdev,
				     p->lba,
				     &p->sect);
	if (!p->cmdline) {
		pr_err("%s:read_dev_sector failed!\n", __func__);
		ret = -ENODEV;
		goto put;
	}
	f->private_data = p;
	return 0;
put:
	blkdev_put(p->bdev, f->f_mode);
free:
	kfree(p);
	return -ENODEV;
}
static ssize_t read_cmdline(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct cmdline_priv *p =
		(struct cmdline_priv *)file->private_data;
	if (!p)
		return -ENODEV;
	return simple_read_from_buffer(buf, count,
				       ppos,
				       p->cmdline, strnlen(p->cmdline, 0x100));
}

static ssize_t write_cmdline(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct cmdline_priv *p =
		(struct cmdline_priv *)file->private_data;
	int ret;
	if (!p)
		return -ENODEV;
	/* @todo detect if image is signed, and prevent write */
	lock_page(p->sect.v);
	ret = simple_write_to_buffer(p->cmdline, 0x100-1,
				     ppos,
				     buf, count);
	/* make sure we zero terminate the cmdline */
	if (file->f_pos + count < 0x100)
		p->cmdline[file->f_pos + count] = '\0';
	set_page_dirty(p->sect.v);
	unlock_page(p->sect.v);
	return ret;
}
int release_cmdline(struct inode *i, struct file *f)
{
	struct cmdline_priv *p =
		(struct cmdline_priv *)f->private_data;
	put_dev_sector(p->sect);
	blkdev_put(p->bdev, f->f_mode);
	kfree(p);
	return 0;
}
int fsync_cmdline(struct file *f, int datasync)
{
	struct cmdline_priv *p =
		(struct cmdline_priv *)f->private_data;
	sync_blockdev(p->bdev);
	return 0;
}
static const struct file_operations osip_cmdline_fops = {
	.open =         open_cmdline,
	.read =         read_cmdline,
	.write =        write_cmdline,
	.release =      release_cmdline,
	.fsync =        fsync_cmdline
};

/* decode the osip */

static int decode_show_cb(struct OSIP_header *osip, void *data)
{
	struct seq_file *s = (struct seq_file *) data;
	int i;

	seq_printf(s, "HEADER:\n"
		   "\tsig              = 0x%x\n"
		   "\theader_size      = 0x%hx\n"
		   "\theader_rev_minor = 0x%hhx\n"
		   "\theader_rev_major = 0x%hhx\n"
		   "\theader_checksum  = 0x%hhx\n"
		   "\tnum_pointers     = 0x%hhx\n"
		   "\tnum_images       = 0x%hhx\n",
		   osip->sig,
		   osip->header_size,
		   osip->header_rev_minor,
		   osip->header_rev_major,
		   osip->header_checksum,
		   osip->num_pointers,
		   osip->num_images);

	for (i = 0; i < osip->num_pointers; i++)
		seq_printf(s, "image%d\n"
			   "\tos_rev              =  0x%0hx\n"
			   "\tos_rev              = 0x%hx\n"
			   "\tlogical_start_block = 0x%x\n"
			   "\tddr_load_address    = 0x%0x\n"
			   "\tentry_point         = 0x%0x\n"
			   "\tsize_of_os_image    = 0x%x\n"
			   "\tattribute           = 0x%02x\n"
			   "\treserved            = %02x%02x%02x\n",
			   i,
			   osip->desc[i].os_rev_minor,
			   osip->desc[i].os_rev_major,
			   osip->desc[i].logical_start_block,
			   osip->desc[i].ddr_load_address,
			   osip->desc[i].entry_point,
			   osip->desc[i].size_of_os_image,
			   osip->desc[i].attribute,
			   osip->desc[i].reserved[0],
			   osip->desc[i].reserved[1],
			   osip->desc[i].reserved[2]);
	return 0;
}
static int decode_show(struct seq_file *s, void *unused)
{
	access_osip_record(decode_show_cb, (void *)s);
	return 0;
}
static int decode_open(struct inode *inode, struct file *file)
{
	return single_open(file, decode_show, NULL);
}

static const struct file_operations osip_decode_fops = {
	.open           = decode_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
static struct dentry *osip_dir;
static void create_debugfs_files(void)
{
	/* /sys/kernel/debug/osip */
	osip_dir = debugfs_create_dir("osip", NULL);
	/* /sys/kernel/debug/osip/cmdline */
	(void) debugfs_create_file("cmdline", S_IFREG | S_IRUGO | S_IWUGO,
				   osip_dir, (void *)0, &osip_cmdline_fops);
	/* /sys/kernel/debug/osip/cmdline_pos */
	(void) debugfs_create_file("cmdline_pos", S_IFREG | S_IRUGO | S_IWUGO,
				osip_dir, (void *)1, &osip_cmdline_fops);
	/* /sys/kernel/debug/osip/decode */
	(void) debugfs_create_file("decode", S_IFREG | S_IRUGO,
				osip_dir, NULL, &osip_decode_fops);
}
static void remove_debugfs_files(void)
{
	debugfs_remove_recursive(osip_dir);
}
#else /* defined(CONFIG_INTEL_MID_OSIP_DEBUG_FS) */
static void create_debugfs_files(void)
{
}
static void remove_debugfs_files(void)
{
}
#endif

static void restore_wq_handler(struct work_struct *w);
static DECLARE_DELAYED_WORK(restore_wq, restore_wq_handler);

static void magic_reboot_wq_handler(struct work_struct *w);
static DECLARE_DELAYED_WORK(magic_reboot_wq, magic_reboot_wq_handler);

/* We want to restore all the OSII entries at boot, but have to do
 * this in a workqueue as the MMC device takes a bit to come up */
static void restore_wq_handler(struct work_struct *w)
{
	/* Keep trying until it works, although the inital delay set
	 * waiting for MMC to come up should be sufficient */
	if (access_osip_record(osip_restore, NULL))
		schedule_delayed_work(&restore_wq, msecs_to_jiffies(100));
}

static void magic_reboot_wq_handler(struct work_struct *w)
{
	/* Keep trying until it works, although the inital delay set
	 * waiting for MMC to come up should be sufficient */
	if (access_osip_record(osip_invalidate, (void *)magic_target))
		schedule_delayed_work(&magic_reboot_wq, msecs_to_jiffies(100));
	else {
		pr_info("Rebooting on behalf of magic key press\n");
		sys_sync();
		kernel_restart(NULL);
	}
}

static int osip_proc_write(struct file *file, const char *buf,
		unsigned long count, void *data)
{
	char cmd[16];
	unsigned long slen = count;

	if (slen >= sizeof(cmd))
		slen = sizeof(cmd) - 1;
	if (copy_from_user(cmd, buf, slen))
		return -EFAULT;
	cmd[slen] = '\0';
	while (slen && cmd[slen - 1] == '\n')
		cmd[--slen] = '\0';

	/* If the cmd string doesn't match, just restore them all */
	if (process_boot_cmd(cmd))
		access_osip_record(osip_restore, NULL);
	return count;
}

static int __init osip_init(void)
{
	int i;
	struct proc_dir_entry *de;

	/* Create proc node to restore OSIP entries from userspace */
	de = create_proc_entry("osip", 0200, 0);
	de->write_proc = osip_proc_write;

	if (register_reboot_notifier(&osip_reboot_notifier))
		pr_warning("osip: unable to register reboot notifier");
	create_debugfs_files();
	for (i = 0; i < mkeys_count; i++) {
		int gpionum;
		int activehi;
		int gpioval;

		if (!mkeys[i])
			continue;

		gpionum = mkeys[i] > 0 ? mkeys[i] : -mkeys[i];
		activehi = (mkeys[i] < 0);

		if (gpio_request(gpionum, "intel_mid_osip") < 0) {
			pr_err("osip: failed to request GPIO %d\n", gpionum);
			continue;
		}
		if (gpio_direction_input(gpionum) < 0) {
			pr_err("osip: failed to configure direction "
					"for GPIO %d", gpionum);
			gpio_free(gpionum);
			continue;
		}
		gpioval = gpio_get_value(gpionum);
		gpio_free(gpionum);
		if (activehi ? gpioval : !gpioval) {
			magic_target = i + 1;
			pr_info("osip: Magic key on GPIO %d detected, "
					"reboot into OS %d\n", mkeys[i],
					magic_target);
			schedule_delayed_work(&magic_reboot_wq,
					msecs_to_jiffies(500));
			break;
		}
	}
	if (bootrestore && !magic_target)
		schedule_delayed_work(&restore_wq, msecs_to_jiffies(500));
	return 0;
}

static void __exit osip_exit(void)
{
	remove_proc_entry("osip", NULL);
	unregister_reboot_notifier(&osip_reboot_notifier);
	remove_debugfs_files();
}
module_init(osip_init);
module_exit(osip_exit);

MODULE_AUTHOR("Pierre Tardy <pierre.tardy@intel.com>");
MODULE_AUTHOR("Xiaokang Qin <xiaokang.qin@intel.com>");
MODULE_AUTHOR("Andrew Boie <andrew.p.boie@intel.com>");
MODULE_DESCRIPTION("Intel Medfield OSIP Driver");
MODULE_LICENSE("GPL v2");

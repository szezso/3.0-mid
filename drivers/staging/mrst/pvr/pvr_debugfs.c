/*
 * Copyright (C) 2011 Nokia Corporation
 * Copyright (C) 2011 Intel Corporation
 * Author: Luc Verhaegen <libv@codethink.co.uk>
 * Author: Imre Deak <imre.deak@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/kref.h>

#include "img_types.h"
#include "servicesext.h"
#include "services.h"
#include "syscommon.h"
#include "pvr_bridge_km.h"
#include "sgx_bridge_km.h"
#include "sgxutils.h"
#include "pvr_debugfs.h"
#include "mmu.h"
#include "bridged_support.h"
#include "mm.h"
#include "pvr_trace_cmd.h"
#include "pvr_debug_core.h"

static struct dentry *pvr_debugfs_dir;
static u32 reset_sgx;

static int pvr_debugfs_reset_sgx(void)
{
	PVRSRV_DEVICE_NODE *dev_node;

	dev_node = pvr_get_sgx_dev_node();
	if (!dev_node)
		return -ENODEV;

	return sgx_trigger_reset(dev_node);
}

static int pvr_debugfs_reset_sgx_wrapper(void *data, u64 val)
{
	u32 *var = data;

	if (var == &reset_sgx) {
		int r = -EINVAL;

		if (val == 1)
			r = pvr_debugfs_reset_sgx();
		return r;
	}

	BUG();
}

DEFINE_SIMPLE_ATTRIBUTE(pvr_debugfs_reset_sgx_fops, NULL,
			pvr_debugfs_reset_sgx_wrapper, "%llu\n");


#ifdef CONFIG_PVR_TRACE_CMD

static void *trcmd_str_buf;
static u8 *trcmd_snapshot;
static size_t trcmd_snapshot_size;
static int trcmd_busy;
static DEFINE_SPINLOCK(trcmd_lock);	/* locks trcmd_busy */

static int pvr_dbg_trcmd_open(struct inode *inode, struct file *file)
{
	int r;

	spin_lock(&trcmd_lock);
	if (trcmd_busy) {
		spin_unlock(&trcmd_lock);
		return -EBUSY;
	}
	trcmd_busy = 1;
	spin_unlock(&trcmd_lock);

	trcmd_str_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!trcmd_str_buf) {
		r = -ENOMEM;
		goto err;
	}

	r = pvr_trcmd_create_snapshot(&trcmd_snapshot, &trcmd_snapshot_size);
	if (r < 0) {
		kfree(trcmd_str_buf);
		goto err;
	}
	return 0;
err:
	spin_lock(&trcmd_lock);
	trcmd_busy = 0;
	spin_unlock(&trcmd_lock);

	return r;
}

static int pvr_dbg_trcmd_release(struct inode *inode, struct file *file)
{
	pvr_trcmd_destroy_snapshot(trcmd_snapshot);
	kfree(trcmd_str_buf);

	spin_lock(&trcmd_lock);
	trcmd_busy = 0;
	spin_unlock(&trcmd_lock);

	return 0;
}

static ssize_t pvr_dbg_trcmd_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = pvr_trcmd_print(trcmd_str_buf, max_t(size_t, PAGE_SIZE, count),
			      trcmd_snapshot, trcmd_snapshot_size, ppos);
	if (copy_to_user(buffer, trcmd_str_buf, ret))
		return -EFAULT;

	return ret;
}

static const struct file_operations pvr_dbg_trcmd_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_trcmd_open,
	.release	= pvr_dbg_trcmd_release,
	.read		= pvr_dbg_trcmd_read,
};
#endif

static struct sgx_fw_state *fw_state;
static int fw_state_busy;
static DEFINE_SPINLOCK(fw_state_lock);  /* locks fw_state_busy */

static int pvr_dbg_fw_state_open(struct inode *inode, struct file *file)
{
	PVRSRV_DEVICE_NODE *dev_node;
	int r = 0;

	dev_node = pvr_get_sgx_dev_node();
	if (!dev_node)
		return -ENODEV;

	spin_lock(&fw_state_lock);
	if (fw_state_busy) {
		spin_unlock(&fw_state_lock);
		return -EBUSY;
	}
	fw_state_busy = 1;
	spin_unlock(&fw_state_lock);

	fw_state = vmalloc(sizeof(*fw_state));
	if (!fw_state) {
		r = -ENOMEM;
		goto err;
	}

	r = sgx_save_fw_state(dev_node, fw_state);
	if (r < 0) {
		vfree(fw_state);
		goto err;
	}

	return 0;
err:
	spin_lock(&fw_state_lock);
	fw_state_busy = 0;
	spin_unlock(&fw_state_lock);

	return r;
}

static int pvr_dbg_fw_state_release(struct inode *inode, struct file *file)
{
	vfree(fw_state);

	spin_lock(&fw_state_lock);
	fw_state_busy = 0;
	spin_unlock(&fw_state_lock);

	return 0;
}

static ssize_t pvr_dbg_fw_state_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	char rec[48];
	ssize_t ret;

	if (*ppos >= ARRAY_SIZE(fw_state->trace) + 1)
		return 0;

	if (!*ppos)
		ret = sgx_print_fw_status_code(rec, sizeof(rec),
						fw_state->status_code);
	else
		ret = sgx_print_fw_trace_rec(rec, sizeof(rec), fw_state,
						*ppos - 1);
	(*ppos)++;

	if (copy_to_user(buffer, rec, ret))
		return -EFAULT;

	return ret;
}

static const struct file_operations pvr_dbg_fw_state_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_fw_state_open,
	.release	= pvr_dbg_fw_state_release,
	.read		= pvr_dbg_fw_state_read,
};

static struct sgx_registers *sgx_regs;
static int sgx_regs_busy;
static DEFINE_SPINLOCK(sgx_regs_lock);		/* locks sgx_regs_busy */

static int pvr_dbg_sgx_regs_open(struct inode *inode, struct file *file)
{
	PVRSRV_DEVICE_NODE *dev_node;
	int r = 0;

	dev_node = pvr_get_sgx_dev_node();
	if (!dev_node)
		return -ENODEV;

	spin_lock(&sgx_regs_lock);
	if (sgx_regs_busy) {
		spin_unlock(&sgx_regs_lock);
		return -EBUSY;
	}
	sgx_regs_busy = 1;
	spin_unlock(&sgx_regs_lock);

	sgx_regs = vmalloc(sizeof(*sgx_regs));
	if (!sgx_regs) {
		r = -ENOMEM;
		goto err;
	}

	r = sgx_save_registers(dev_node, sgx_regs);
	if (r < 0) {
		vfree(sgx_regs);
		goto err;
	}

	return 0;
err:
	spin_lock(&sgx_regs_lock);
	sgx_regs_busy = 1;
	spin_unlock(&sgx_regs_lock);

	return r;
}

static int pvr_dbg_sgx_regs_release(struct inode *inode, struct file *file)
{
	vfree(sgx_regs);

	spin_lock(&sgx_regs_lock);
	sgx_regs_busy = 0;
	spin_unlock(&sgx_regs_lock);

	return 0;
}

static ssize_t pvr_dbg_sgx_regs_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	char rec[48];
	ssize_t ret;

	if (*ppos >= ARRAY_SIZE(sgx_regs->v))
		return 0;

	ret = scnprintf(rec, sizeof(rec), "%04lx %08lx\n",
			(unsigned long)*ppos * 4,
			(unsigned long)sgx_regs->v[*ppos]);
	(*ppos)++;

	if (copy_to_user(buffer, rec, ret))
		return -EFAULT;

	return ret;
}

static const struct file_operations pvr_dbg_sgx_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_sgx_regs_open,
	.release	= pvr_dbg_sgx_regs_release,
	.read		= pvr_dbg_sgx_regs_read,
};

/*
 * hwrec_event will block until a recovery event happens. Until it's opened
 * for the next time the other hwrec_* entries will provide more details about
 * the recovery. After this all hwrec_* entries will be reset.
 */
struct hwrec_snapshot {
	unsigned count;
	unsigned long long time;
	struct sgx_registers sgx_regs;
	int error;
	int reserved;
	struct kref ref;		/* locks the above in this struct */
	struct list_head entry;
};

static atomic_t hwrec_cnt;

#define HWREC_MAX_SNAPSHOT_CNT 2
static atomic_t hwrec_snapshot_cnt;
static LIST_HEAD(hwrec_snapshots);
static int hwrec_snapshots_disabled;
/* locks hwrec_snapshots, hwrec_snapshots_disabled */
static DEFINE_SPINLOCK(hwrec_snapshots_lock);
static DECLARE_WAIT_QUEUE_HEAD(hwrec_wait);

struct hwrec_debugfs_entry {
	ssize_t (*read)(struct hwrec_debugfs_entry *he, char __user *buffer,
			size_t count, loff_t *ppos);
	const char *name;
	int is_lock_entry;
	struct hwrec_snapshot *hwrec_snapshot;
	int busy;
	spinlock_t lock;		/* locks busy */
};

static ssize_t pvr_dbg_hwrec_sgx_regs_read(struct hwrec_debugfs_entry *he,
					   char __user *buffer, size_t count,
					   loff_t *ppos)
{
	struct sgx_registers *regs = &he->hwrec_snapshot->sgx_regs;
	char rec[48];
	int r;

	if (*ppos >= ARRAY_SIZE(regs->v))
		return 0;

	r = scnprintf(rec, sizeof(rec), "%04lx %08lx\n",
			(unsigned long)*ppos * 4,
			(unsigned long)regs->v[*ppos]);
	(*ppos)++;

	if (copy_to_user(buffer, rec, r))
		return -EFAULT;

	return r;
}

static ssize_t pvr_dbg_hwrec_event_read(struct hwrec_debugfs_entry *he,
					   char __user *buffer, size_t count,
					   loff_t *ppos)
{
	struct hwrec_snapshot *s = he->hwrec_snapshot;
	char rec[48];
	int r;

	if (*ppos >= 1)
		return 0;

	r = scnprintf(rec, sizeof(rec), "%lu %llu\n",
			(unsigned long)s->count, s->time);
	(*ppos)++;

	if (copy_to_user(buffer, rec, r))
		return -EFAULT;

	return r;
}

static struct hwrec_debugfs_entry hwrec_debugfs_entries[] = {
	{
		.name = "hwrec_event",
		.read = pvr_dbg_hwrec_event_read,
		.is_lock_entry = 1,
	},
	{
		.name = "hwrec_sgx_registers",
		.read = pvr_dbg_hwrec_sgx_regs_read,
	},
};

static struct hwrec_snapshot *hwrec_alloc_snapshot(void)
{
	struct hwrec_snapshot *s;

	if (atomic_inc_return(&hwrec_snapshot_cnt) > HWREC_MAX_SNAPSHOT_CNT)
		goto err;

	s = vmalloc(sizeof(*s));
	if (!s)
		goto err;
	kref_init(&s->ref);

	return s;

err:
	atomic_dec(&hwrec_snapshot_cnt);

	return NULL;
}

static void hwrec_snapshot_get(struct hwrec_snapshot *s)
{
	kref_get(&s->ref);
}

static void hwrec_release_snapshot(struct kref *kref)
{
	struct hwrec_snapshot *s;

	s = container_of(kref, struct hwrec_snapshot, ref);
	vfree(s);
	atomic_dec(&hwrec_snapshot_cnt);
}

static void hwrec_snapshot_put(struct hwrec_snapshot *s)
{
	kref_put(&s->ref, hwrec_release_snapshot);
}

static int hwrec_wait_snapshot(int dont_block, struct hwrec_snapshot **s_ret)
{
	struct hwrec_snapshot *s;
	int r = 0;

	spin_lock(&hwrec_snapshots_lock);

	while (list_empty(&hwrec_snapshots)) {
		spin_unlock(&hwrec_snapshots_lock);

		if (dont_block)
			return -EAGAIN;
		r = wait_event_interruptible(hwrec_wait,
					     !list_empty(&hwrec_snapshots));
		if (r < 0)
			return r;

		spin_lock(&hwrec_snapshots_lock);
	}
	s = list_first_entry(&hwrec_snapshots, struct hwrec_snapshot, entry);
	hwrec_snapshot_get(s);
	*s_ret = s;

	spin_unlock(&hwrec_snapshots_lock);

	return r;
}

void pvr_debugfs_hwrec_create_snapshot(PVRSRV_DEVICE_NODE *dev_node)
{
	struct hwrec_snapshot *s;
	unsigned long count;
	int disabled;

	count = atomic_inc_return(&hwrec_cnt);

	s = hwrec_alloc_snapshot();
	if (!s)
		/*
		 * no place to record the details of this event, we note
		 * it only by incrementing hwrec_count.
		 */
		return;

	s->count = count;

	s->time = cpu_clock(get_cpu());
	put_cpu();

	sgx_save_registers_no_pwron(dev_node, &s->sgx_regs);
	s->error = 0;

	/*
	 * Note that the ordering of hwrec_snapshots on the list is guaranteed:
	 * this function won't be re-entered since the HW recovery path is
	 * non re-entrant in itself.
	 */
	spin_lock(&hwrec_snapshots_lock);
	disabled = hwrec_snapshots_disabled;
	if (!disabled)
		list_add_tail(&s->entry, &hwrec_snapshots);
	spin_unlock(&hwrec_snapshots_lock);

	if (!disabled)
		wake_up_interruptible(&hwrec_wait);
	else
		hwrec_snapshot_put(s);
}

static void remove_hwrec_snapshot(struct hwrec_snapshot *s)
{
	spin_lock(&hwrec_snapshots_lock);
	list_del(&s->entry);
	spin_unlock(&hwrec_snapshots_lock);

	hwrec_snapshot_put(s);
}

static int pvr_dbg_hwrec_open(struct inode *inode, struct file *file)
{
	struct hwrec_debugfs_entry *he = inode->i_private;

	spin_lock(&he->lock);
	if (he->busy) {
		spin_unlock(&he->lock);
		return -EBUSY;
	}
	he->busy = 1;
	spin_unlock(&he->lock);

	file->private_data = he;

	/* remove old snapshot if this is the lock debugfs entry */
	if (he->is_lock_entry && he->hwrec_snapshot) {
		remove_hwrec_snapshot(he->hwrec_snapshot);
		he->hwrec_snapshot = NULL;
	}

	return 0;
}

static ssize_t pvr_dbg_hwrec_read(struct file *file,
					char __user *buffer,
					size_t count, loff_t *ppos)
{
	struct hwrec_debugfs_entry *he = file->private_data;
	int r;

	if (!he->hwrec_snapshot) {
		/* suppress used before init warning */
		struct hwrec_snapshot *s = NULL;
		int dont_block = file->f_flags & O_NONBLOCK;

		r = hwrec_wait_snapshot(dont_block, &s);
		if (r < 0)
			return r;
		he->hwrec_snapshot = s;

		r = he->hwrec_snapshot->error;
		if (r < 0)
			return r;
	}

	return he->read(he, buffer, count, ppos);
}

static int pvr_dbg_hwrec_release(struct inode *inode, struct file *file)
{
	struct hwrec_debugfs_entry *he = file->private_data;

	if (he->hwrec_snapshot) {
		hwrec_snapshot_put(he->hwrec_snapshot);
		/*
		 * In case of the lock debugfs entry, we hold on to the
		 * snapshot, so we can delay the removal of it until the
		 * debugfs entry is opened next time.
		 */
		if (!he->is_lock_entry)
			he->hwrec_snapshot = NULL;
	}

	spin_lock(&he->lock);
	he->busy = 0;
	spin_unlock(&he->lock);

	return 0;
}

static unsigned int pvr_dbg_hwrec_poll(struct file *file, poll_table *wait)
{
	struct hwrec_debugfs_entry *he = file->private_data;

	poll_wait(file, &hwrec_wait, wait);
	if (!he->hwrec_snapshot) {
		int r;

		r = hwrec_wait_snapshot(1, &he->hwrec_snapshot);
		if (r == -EAGAIN)
			return 0;
		if (r < 0 || he->hwrec_snapshot->error)
			return POLLERR | POLLHUP;
	}

	return POLLIN | POLLRDNORM;
}

static const struct file_operations pvr_dbg_hwrec_fops = {
	.owner		= THIS_MODULE,
	.open		= pvr_dbg_hwrec_open,
	.read		= pvr_dbg_hwrec_read,
	.release	= pvr_dbg_hwrec_release,
	.poll		= pvr_dbg_hwrec_poll,
};

static void init_hwrec_debugfs_entries(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hwrec_debugfs_entries); i++)
		spin_lock_init(&hwrec_debugfs_entries[i].lock);
}

static void cleanup_hwrec_debugfs_entries(void)
{
	struct hwrec_snapshot *s, *s_tmp;

	spin_lock(&hwrec_snapshots_lock);
	hwrec_snapshots_disabled = 1;
	spin_unlock(&hwrec_snapshots_lock);

	/*
	 * No need to take the lock; hwrec_snapshots_disabled guarantees
	 * that no one else will modify the list.
	 */
	list_for_each_entry_safe(s, s_tmp, &hwrec_snapshots, entry)
		remove_hwrec_snapshot(s);
}

int pvr_debugfs_init(void)
{
	int i;

	pvr_debugfs_dir = debugfs_create_dir("pvr", NULL);
	if (!pvr_debugfs_dir)
		goto err;

	if (!debugfs_create_file("reset_sgx", S_IWUSR, pvr_debugfs_dir,
				 &reset_sgx, &pvr_debugfs_reset_sgx_fops))
		goto err;

#ifdef CONFIG_PVR_TRACE_CMD
	if (!debugfs_create_file("command_trace", S_IRUGO, pvr_debugfs_dir,
				NULL, &pvr_dbg_trcmd_fops))
		goto err;
#endif
	if (!debugfs_create_file("firmware_trace", S_IRUGO, pvr_debugfs_dir,
				NULL, &pvr_dbg_fw_state_fops))
		goto err;

	if (!debugfs_create_file("sgx_registers", S_IRUGO, pvr_debugfs_dir,
				NULL, &pvr_dbg_sgx_regs_fops))
		goto err;

	init_hwrec_debugfs_entries();
	for (i = 0; i < ARRAY_SIZE(hwrec_debugfs_entries); i++) {
		struct hwrec_debugfs_entry *he = &hwrec_debugfs_entries[i];

		if (!debugfs_create_file(he->name, S_IRUGO, pvr_debugfs_dir,
					 he, &pvr_dbg_hwrec_fops))
			goto err;
	}

	return 0;
err:
	debugfs_remove_recursive(pvr_debugfs_dir);
	cleanup_hwrec_debugfs_entries();
	pr_err("pvr: debugfs init failed\n");

	return -ENODEV;
}

void pvr_debugfs_cleanup(void)
{
	debugfs_remove_recursive(pvr_debugfs_dir);
	cleanup_hwrec_debugfs_entries();
}


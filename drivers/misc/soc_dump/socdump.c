
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "socdump.h"

static DEFINE_SPINLOCK(sources_lock);
static LIST_HEAD(dump_sources);
static u32 reg_count;
static struct dentry *dump_sources_stats_dentry;


/**
 * dump_source_create - Create a struct dump_source object.
 */
struct dump_source *dump_source_create(void)
{
	struct dump_source *ds;

	ds = kzalloc(sizeof(*ds), GFP_KERNEL);
	if (!ds)
		return NULL;

	spin_lock_init(&ds->lock);
	INIT_LIST_HEAD(&ds->entry);
	return ds;
}
EXPORT_SYMBOL(dump_source_create);

/**
 * dump_source_destroy - Destroy a struct dump_source object.
 * @ds: Wakeup source to destroy.
 */
void dump_source_destroy(struct dump_source *ds)
{
	if (!ds)
		return;
	kfree(ds);
}
EXPORT_SYMBOL(dump_source_destroy);

/**
 * dump_source_add - Add given object to the list of register sources.
 * @ds: dump source object to add to the list.
 */
void dump_source_add(struct dump_source *ds)
{
	if (WARN_ON(!ds))
		return;

	spin_lock(&sources_lock);
	reg_count += ds->count;
	list_add(&ds->entry, &dump_sources);
	spin_unlock(&sources_lock);
}
EXPORT_SYMBOL(dump_source_add);

/**
 * dump_source_remove - Remove given object from the register sources list.
 * @ds: dump source object to remove from the list.
 */
void dump_source_remove(struct dump_source *ds)
{
	if (WARN_ON(!ds))
		return;

	spin_lock(&sources_lock);
	reg_count -= ds->count;
	list_del(&ds->entry);
	spin_unlock(&sources_lock);
}
EXPORT_SYMBOL(dump_source_remove);

static void *dr_seq_start(struct seq_file *s, loff_t *pos)
{
	void *start = NULL;

	if (*pos < reg_count)
		start = s->private + (*pos * sizeof(struct reg_info));

	return start;
}

static void *dr_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	++*pos ;
	if (*pos >= reg_count)
		return NULL;

	return s->private + (*pos * sizeof(struct reg_info));
}

static void dr_seq_stop(struct seq_file *s, void *v) { }

/*
 * The show function.
 */
static int dr_seq_show(struct seq_file *s, void *v)
{
	struct reg_info *reg = v;
	return seq_write(s, reg, sizeof(struct reg_info));
}

static const struct seq_operations dr_seq_ops = {
	.start = dr_seq_start,
	.next  = dr_seq_next,
	.stop  = dr_seq_stop,
	.show  = dr_seq_show
};


static int dump_sources_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct dump_source *ds;
	struct seq_file *sf;
	void *buf;
	int buf_size, offset;
	struct reg_info *reg;

	buf_size = sizeof(struct reg_info) * reg_count;
	rc = seq_open_private(file, &dr_seq_ops, buf_size);
	if (rc < 0)
		goto out;

	sf = file->private_data;
	buf = sf->private;
	offset = 0;


	list_for_each_entry(ds, &dump_sources, entry) {
		ds->dump_regs(buf + offset);
		reg = buf;
		offset += sizeof(struct reg_info) * ds->count;
	}
out:
	return rc;
}


static const struct file_operations dump_sources_fops = {
	.owner = THIS_MODULE,
	.open = dump_sources_open,
	.read = seq_read,
	.release = seq_release_private,
};

static int dump_sources_init(void)
{
	dump_sources_stats_dentry =
		debugfs_create_file("dump_registers",
				S_IFREG | S_IRUGO, NULL, NULL,
				&dump_sources_fops);
	return 0;
}

static void dump_sources_exit(void)
{
	struct dump_source *ds;

	list_for_each_entry(ds, &dump_sources, entry)
		dump_source_destroy(ds);
	debugfs_remove(dump_sources_stats_dentry);
}

module_init(dump_sources_init);
module_exit(dump_sources_exit);

MODULE_AUTHOR("Dirk Brandewie <dirk.j.brandewie@intel.com");
MODULE_DESCRIPTION("Register dump module");
MODULE_LICENSE("GPL v2");

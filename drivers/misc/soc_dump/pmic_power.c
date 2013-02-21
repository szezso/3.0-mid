
#include <linux/device.h>
#include <asm/intel_scu_ipc.h>

#include "socdump.h"
static struct dump_source *ds;


static int reg_offsets[] = {
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
	0x44, 0x45, 0x46, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E,
	0x4F, 0x50, 0x51, 0x52,};



static void pmic_power_dump(void *buf)
{
	struct reg_info *reg;
	int i;
	u8 val;
	int ret;

	reg = (struct reg_info *)buf;
	for (i = 0; i < (sizeof(reg_offsets) / sizeof(int)); i++, reg++) {
		reg->valid = 1;
		ret = intel_scu_ipc_ioread8(reg_offsets[i], &val);
		if (ret < 0)
			reg->valid = 0;
		reg->version		= INFO_VER;
		reg->chip_id		= PMIC_POWER;
		reg->register_id	= reg_offsets[i];
		reg->register_value	= val;
	}
}

static int pmic_power_chip_present(void)
{
	int ret;
	u8 r;
	ret = intel_scu_ipc_ioread8(0x00, &r);
	if (ret < 0)
		return 0;
	return r == 0x3a ? 1 : 0;
}


static int pmic_power_init(void)
{
	if (!pmic_power_chip_present())
		return -ENODEV;

	ds = dump_source_create();
	if (!ds)
		return -ENOMEM;
	ds->dump_regs = pmic_power_dump;
	ds->count = sizeof(reg_offsets) / sizeof(int);
	dump_source_add(ds);
	return 0;
}

static void pmic_power_exit(void)
{
	dump_source_remove(ds);
	dump_source_destroy(ds);
}

module_init(pmic_power_init);
module_exit(pmic_power_exit);
MODULE_LICENSE("GPL v2");

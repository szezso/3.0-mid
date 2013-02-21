
#include <linux/device.h>
#include <asm/intel_scu_ipc.h>

#include "socdump.h"
static struct dump_source *ds;


static int reg_offsets[] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x22, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x13F,};

static void pmic_rtc_dump(void *buf)
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
		reg->chip_id		= PMIC_RTC;
		reg->register_id	= reg_offsets[i];
		reg->register_value	= val;
	}
}

static int pmic_rtc_chip_present(void)
{
	int ret;
	u8 r;
	ret = intel_scu_ipc_ioread8(0x00, &r);
	if (ret < 0)
		return 0;
	return r == 0x3a ? 1 : 0;
}


static int pmic_rtc_init(void)
{
	if (!pmic_rtc_chip_present())
		return -ENODEV;

	ds = dump_source_create();
	if (!ds)
		return -ENOMEM;
	ds->dump_regs = pmic_rtc_dump;
	ds->count = sizeof(reg_offsets) / sizeof(int);
	dump_source_add(ds);
	return 0;
}

static void pmic_rtc_exit(void)
{
	dump_source_remove(ds);
	dump_source_destroy(ds);
}

module_init(pmic_rtc_init);
module_exit(pmic_rtc_exit);
MODULE_LICENSE("GPL v2");

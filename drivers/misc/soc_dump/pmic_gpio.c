
#include <linux/device.h>
#include <asm/intel_scu_ipc.h>

#include "socdump.h"
static struct dump_source *ds;


static int reg_offsets[] = {
	0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEC,
	0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4,};



static void pmic_gpio_dump(void *buf)
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
		reg->chip_id		= PMIC_GPIO;
		reg->register_id	= reg_offsets[i];
		reg->register_value	= val;
	}
}

static int pmic_gpio_chip_present(void)
{
	int ret;
	u8 r;
	ret = intel_scu_ipc_ioread8(0x00, &r);
	if (ret < 0)
		return 0;
	return r == 0x3a ? 1 : 0;
}


static int pmic_gpio_init(void)
{
	if (!pmic_gpio_chip_present())
		return -ENODEV;

	ds = dump_source_create();
	if (!ds)
		return -ENOMEM;
	ds->dump_regs = pmic_gpio_dump;
	ds->count = sizeof(reg_offsets) / sizeof(int);
	dump_source_add(ds);
	return 0;
}

static void pmic_gpio_exit(void)
{
	dump_source_remove(ds);
	dump_source_destroy(ds);
}

module_init(pmic_gpio_init);
module_exit(pmic_gpio_exit);
MODULE_LICENSE("GPL v2");

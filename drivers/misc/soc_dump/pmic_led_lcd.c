
#include <linux/device.h>
#include <asm/intel_scu_ipc.h>

#include "socdump.h"
static struct dump_source *ds;


static int reg_offsets[] = {
	0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D,};

static void pmic_led_lcd_dump(void *buf)
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
		reg->chip_id		= PMIC_LED_LCD;
		reg->register_id	= reg_offsets[i];
		reg->register_value	= val;
	}
}

static int pmic_led_lcd_chip_present(void)
{
	int ret;
	u8 r;
	ret = intel_scu_ipc_ioread8(0x00, &r);
	if (ret < 0)
		return 0;
	return r == 0x3a ? 1 : 0;
}


static int pmic_led_lcd_init(void)
{
	if (!pmic_led_lcd_chip_present())
		return -ENODEV;

	ds = dump_source_create();
	if (!ds)
		return -ENOMEM;
	ds->dump_regs = pmic_led_lcd_dump;
	ds->count = sizeof(reg_offsets) / sizeof(int);
	dump_source_add(ds);
	return 0;
}

static void pmic_led_lcd_exit(void)
{
	dump_source_remove(ds);
	dump_source_destroy(ds);
}

module_init(pmic_led_lcd_init);
module_exit(pmic_led_lcd_exit);
MODULE_LICENSE("GPL v2");

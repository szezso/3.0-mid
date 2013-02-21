/*
 * mrst.c: Intel Moorestown platform specific setup code
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Jacob Pan (jacob.jun.pan@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define pr_fmt(fmt) "mrst: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/sfi.h>
#include <linux/intel_pmic_gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/spi/spi.h>
#include <linux/cyttsp.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/gpio.h>
#include <linux/i2c/tc35876x.h>
#include <linux/gpio_keys.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/power/smb347-charger.h>
#include <linux/input.h>
#include <linux/input/intel_mid_keypad.h>
#include <linux/hsi/hsi.h>
#include <linux/hsi/intel_mid_hsi.h>
#include <linux/platform_device.h>
#include <linux/mfd/intel_msic.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/a1026.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/input/lis3dh.h>
#include <linux/nfc/pn544.h>
#include <linux/mmc/sdhci-pci-data.h>
#include <linux/pci.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>
#include <linux/ms5607.h>
#include <linux/input/ltr301als.h>

#include <linux/atomisp_platform.h>
#include <media/v4l2-subdev.h>

#include <asm/setup.h>
#include <asm/mpspec_def.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/mrst.h>
#include <asm/mrst-vrtc.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/intel_scu_ipc.h>
#include <asm/apb_timer.h>
#include <asm/reboot.h>

/*
 * the clockevent devices on Moorestown/Medfield can be APBT or LAPIC clock,
 * cmdline option x86_mrst_timer can be used to override the configuration
 * to prefer one or the other.
 * at runtime, there are basically three timer configurations:
 * 1. per cpu apbt clock only
 * 2. per cpu always-on lapic clocks only, this is Penwell/Medfield only
 * 3. per cpu lapic clock (C3STOP) and one apbt clock, with broadcast.
 *
 * by default (without cmdline option), platform code first detects cpu type
 * to see if we are on lincroft or penwell, then set up both lapic or apbt
 * clocks accordingly.
 * i.e. by default, medfield uses configuration #2, moorestown uses #1.
 * config #3 is supported but not recommended on medfield.
 *
 * rating and feature summary:
 * lapic (with C3STOP) --------- 100
 * apbt (always-on) ------------ 110
 * lapic (always-on,ARAT) ------ 150
 */

__cpuinitdata enum mrst_timer_options mrst_timer_options;

static u32 sfi_mtimer_usage[SFI_MTMR_MAX_NUM];
static struct sfi_timer_table_entry sfi_mtimer_array[SFI_MTMR_MAX_NUM];
enum mrst_cpu_type __mrst_cpu_chip;
EXPORT_SYMBOL_GPL(__mrst_cpu_chip);

int sfi_mtimer_num;

struct sfi_rtc_table_entry sfi_mrtc_array[SFI_MRTC_MAX];
EXPORT_SYMBOL_GPL(sfi_mrtc_array);
int sfi_mrtc_num;

#define INTEL_MSIC_CHIP_COLD_OFF (1 << 3)

static void mrst_power_off(void)
{
	if (__mrst_cpu_chip == MRST_CPU_CHIP_LINCROFT)
		intel_scu_ipc_simple_command(IPCMSG_COLD_RESET, 1);
	else if (__mrst_cpu_chip == MRST_CPU_CHIP_PENWELL)
		intel_msic_reg_update(INTEL_MSIC_CHIPCNTRL,
			INTEL_MSIC_CHIP_COLD_OFF, INTEL_MSIC_CHIP_COLD_OFF);
}

/* parse all the mtimer info to a static mtimer array */
static int __init sfi_parse_mtmr(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_timer_table_entry *pentry;
	struct mpc_intsrc mp_irq;
	int totallen;

	sb = (struct sfi_table_simple *)table;
	if (!sfi_mtimer_num) {
		sfi_mtimer_num = SFI_GET_NUM_ENTRIES(sb,
					struct sfi_timer_table_entry);
		pentry = (struct sfi_timer_table_entry *) sb->pentry;
		totallen = sfi_mtimer_num * sizeof(*pentry);
		memcpy(sfi_mtimer_array, pentry, totallen);
	}

	pr_debug("SFI MTIMER info (num = %d):\n", sfi_mtimer_num);
	pentry = sfi_mtimer_array;
	for (totallen = 0; totallen < sfi_mtimer_num; totallen++, pentry++) {
		pr_debug("timer[%d]: paddr = 0x%08x, freq = %dHz,"
			" irq = %d\n", totallen, (u32)pentry->phys_addr,
			pentry->freq_hz, pentry->irq);
			if (!pentry->irq)
				continue;
			mp_irq.type = MP_INTSRC;
			mp_irq.irqtype = mp_INT;
/* triggering mode edge bit 2-3, active high polarity bit 0-1 */
			mp_irq.irqflag = 5;
			mp_irq.srcbus = MP_BUS_ISA;
			mp_irq.srcbusirq = pentry->irq;	/* IRQ */
			mp_irq.dstapic = MP_APIC_ALL;
			mp_irq.dstirq = pentry->irq;
			mp_save_irq(&mp_irq);
	}

	return 0;
}

struct sfi_timer_table_entry *sfi_get_mtmr(int hint)
{
	int i;
	if (hint < sfi_mtimer_num) {
		if (!sfi_mtimer_usage[hint]) {
			pr_debug("hint taken for timer %d irq %d\n",\
				hint, sfi_mtimer_array[hint].irq);
			sfi_mtimer_usage[hint] = 1;
			return &sfi_mtimer_array[hint];
		}
	}
	/* take the first timer available */
	for (i = 0; i < sfi_mtimer_num;) {
		if (!sfi_mtimer_usage[i]) {
			sfi_mtimer_usage[i] = 1;
			return &sfi_mtimer_array[i];
		}
		i++;
	}
	return NULL;
}

void sfi_free_mtmr(struct sfi_timer_table_entry *mtmr)
{
	int i;
	for (i = 0; i < sfi_mtimer_num;) {
		if (mtmr->irq == sfi_mtimer_array[i].irq) {
			sfi_mtimer_usage[i] = 0;
			return;
		}
		i++;
	}
}

/* parse all the mrtc info to a global mrtc array */
int __init sfi_parse_mrtc(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_rtc_table_entry *pentry;
	struct mpc_intsrc mp_irq;

	int totallen;

	sb = (struct sfi_table_simple *)table;
	if (!sfi_mrtc_num) {
		sfi_mrtc_num = SFI_GET_NUM_ENTRIES(sb,
						struct sfi_rtc_table_entry);
		pentry = (struct sfi_rtc_table_entry *)sb->pentry;
		totallen = sfi_mrtc_num * sizeof(*pentry);
		memcpy(sfi_mrtc_array, pentry, totallen);
	}

	pr_debug("SFI RTC info (num = %d):\n", sfi_mrtc_num);
	pentry = sfi_mrtc_array;
	for (totallen = 0; totallen < sfi_mrtc_num; totallen++, pentry++) {
		pr_debug("RTC[%d]: paddr = 0x%08x, irq = %d\n",
			totallen, (u32)pentry->phys_addr, pentry->irq);
		mp_irq.type = MP_INTSRC;
		mp_irq.irqtype = mp_INT;
		mp_irq.irqflag = 0xf;	/* level trigger and active low */
		mp_irq.srcbus = MP_BUS_ISA;
		mp_irq.srcbusirq = pentry->irq;	/* IRQ */
		mp_irq.dstapic = MP_APIC_ALL;
		mp_irq.dstirq = pentry->irq;
		mp_save_irq(&mp_irq);
	}
	return 0;
}

static unsigned long __init mrst_calibrate_tsc(void)
{
	unsigned long flags, fast_calibrate;
	if (__mrst_cpu_chip == MRST_CPU_CHIP_PENWELL) {
		u32 lo, hi, ratio, fsb;

		rdmsr(MSR_IA32_PERF_STATUS, lo, hi);
		pr_debug("IA32 perf status is 0x%x, 0x%0x\n", lo, hi);
		ratio = (hi >> 8) & 0x1f;
		pr_debug("ratio is %d\n", ratio);
		if (!ratio) {
			pr_err("read a zero ratio, should be incorrect!\n");
			pr_err("force tsc ratio to 16 ...\n");
			ratio = 16;
		}
		rdmsr(MSR_FSB_FREQ, lo, hi);
		if ((lo & 0x7) == 0x7)
			fsb = PENWELL_FSB_FREQ_83SKU;
		else
			fsb = PENWELL_FSB_FREQ_100SKU;
		fast_calibrate = ratio * fsb;
		pr_debug("read penwell tsc %lu khz\n", fast_calibrate);
		lapic_timer_frequency = fsb * 1000 / HZ;
		/* mark tsc clocksource as reliable */
		set_cpu_cap(&boot_cpu_data, X86_FEATURE_TSC_RELIABLE);
	} else {
		local_irq_save(flags);
		fast_calibrate = apbt_quick_calibrate();
		local_irq_restore(flags);
	}
	
	if (fast_calibrate)
		return fast_calibrate;

	return 0;
}

static void __init mrst_time_init(void)
{
	sfi_table_parse(SFI_SIG_MTMR, NULL, NULL, sfi_parse_mtmr);
	switch (mrst_timer_options) {
	case MRST_TIMER_APBT_ONLY:
		break;
	case MRST_TIMER_LAPIC_APBT:
		x86_init.timers.setup_percpu_clockev = setup_boot_APIC_clock;
		x86_cpuinit.setup_percpu_clockev = setup_secondary_APIC_clock;
		break;
	default:
		if (!boot_cpu_has(X86_FEATURE_ARAT))
			break;
		x86_init.timers.setup_percpu_clockev = setup_boot_APIC_clock;
		x86_cpuinit.setup_percpu_clockev = setup_secondary_APIC_clock;
		return;
	}
	/* we need at least one APB timer */
	pre_init_apic_IRQ0();
	apbt_time_init();
}

static void __cpuinit mrst_arch_setup(void)
{
	if (boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model == 0x27)
		__mrst_cpu_chip = MRST_CPU_CHIP_PENWELL;
	else if (boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model == 0x26)
		__mrst_cpu_chip = MRST_CPU_CHIP_LINCROFT;
	else {
		pr_err("Unknown Moorestown CPU (%d:%d), default to Lincroft\n",
			boot_cpu_data.x86, boot_cpu_data.x86_model);
		__mrst_cpu_chip = MRST_CPU_CHIP_LINCROFT;
	}
	pr_debug("Moorestown CPU %s identified\n",
		(__mrst_cpu_chip == MRST_CPU_CHIP_LINCROFT) ?
		"Lincroft" : "Penwell");
}

/* MID systems don't have i8042 controller */
static int mrst_i8042_detect(void)
{
	return 0;
}

/*
 * Moorestown does not have external NMI source nor port 0x61 to report
 * NMI status. The possible NMI sources are from pmu as a result of NMI
 * watchdog or lock debug. Reading io port 0x61 results in 0xff which
 * misled NMI handler.
 */
static unsigned char mrst_get_nmi_reason(void)
{
	return 0;
}

/*
 * Moorestown specific x86_init function overrides and early setup
 * calls.
 */
void __init x86_mrst_early_setup(void)
{
	x86_init.resources.probe_roms = x86_init_noop;
	x86_init.resources.reserve_resources = x86_init_noop;

	x86_init.timers.timer_init = mrst_time_init;
	x86_init.timers.setup_percpu_clockev = x86_init_noop;

	x86_init.irqs.pre_vector_init = x86_init_noop;

	x86_init.oem.arch_setup = mrst_arch_setup;

	x86_cpuinit.setup_percpu_clockev = apbt_setup_secondary_clock;

	x86_platform.calibrate_tsc = mrst_calibrate_tsc;
	x86_platform.i8042_detect = mrst_i8042_detect;
	x86_init.timers.wallclock_init = mrst_rtc_init;
	x86_platform.get_nmi_reason = mrst_get_nmi_reason;

	x86_init.pci.init = pci_mrst_init;
	x86_init.pci.fixup_irqs = x86_init_noop;

	legacy_pic = &null_legacy_pic;

	/* Moorestown specific power_off/restart method */
	pm_power_off = mrst_power_off;
	machine_ops.restart  = intel_scu_ipc_restart;
	machine_ops.emergency_restart  = intel_scu_ipc_emergency_restart;

	/* Avoid searching for BIOS MP tables */
	x86_init.mpparse.find_smp_config = x86_init_noop;
	x86_init.mpparse.get_smp_config = x86_init_uint_noop;
	set_bit(MP_BUS_ISA, mp_bus_not_pci);
}

/*
 * if user does not want to use per CPU apb timer, just give it a lower rating
 * than local apic timer and skip the late per cpu timer init.
 */
static inline int __init setup_x86_mrst_timer(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (strcmp("apbt_only", arg) == 0)
		mrst_timer_options = MRST_TIMER_APBT_ONLY;
	else if (strcmp("lapic_and_apbt", arg) == 0)
		mrst_timer_options = MRST_TIMER_LAPIC_APBT;
	else {
		pr_warning("X86 MRST timer option %s not recognised"
			   " use x86_mrst_timer=apbt_only or lapic_and_apbt\n",
			   arg);
		return -EINVAL;
	}
	return 0;
}
__setup("x86_mrst_timer=", setup_x86_mrst_timer);

/*
 * Parsing GPIO table first, since the DEVS table will need this table
 * to map the pin name to the actual pin.
 */
static struct sfi_gpio_table_entry *gpio_table;
static int gpio_num_entry;

static int __init sfi_parse_gpio(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_gpio_table_entry *pentry;
	int num, i;

	if (gpio_table)
		return 0;
	sb = (struct sfi_table_simple *)table;
	num = SFI_GET_NUM_ENTRIES(sb, struct sfi_gpio_table_entry);
	pentry = (struct sfi_gpio_table_entry *)sb->pentry;

	gpio_table = (struct sfi_gpio_table_entry *)
				kmalloc(num * sizeof(*pentry), GFP_KERNEL);
	if (!gpio_table)
		return -1;
	memcpy(gpio_table, pentry, num * sizeof(*pentry));
	gpio_num_entry = num;

	pr_debug("GPIO pin info:\n");
	for (i = 0; i < num; i++, pentry++)
		pr_debug("info[%2d]: controller = %16.16s, pin_name = %16.16s,"
		" pin = %d\n", i,
			pentry->controller_name,
			pentry->pin_name,
			pentry->pin_no);
	return 0;
}

static int get_gpio_by_name(const char *name)
{
	struct sfi_gpio_table_entry *pentry = gpio_table;
	int i;

	if (!pentry)
		return -1;
	for (i = 0; i < gpio_num_entry; i++, pentry++) {
		if (!strncmp(name, pentry->pin_name, SFI_NAME_LEN))
			return pentry->pin_no;
	}
	return -1;
}

/*
 * Here defines the array of devices platform data that IAFW would export
 * through SFI "DEVS" table, we use name and type to match the device and
 * its platform data.
 */
struct devs_id {
	char name[SFI_NAME_LEN + 1];
	u8 type;
	u8 delay;
	void *(*get_platform_data)(void *info);
};

/* the offset for the mapping of global gpio pin to irq */
#define MRST_IRQ_OFFSET 0x100

static void __init *pmic_gpio_platform_data(void *info)
{
	static struct intel_pmic_gpio_platform_data pmic_gpio_pdata;
	int gpio_base = get_gpio_by_name("pmic_gpio_base");

	if (gpio_base == -1)
		gpio_base = 64;
	pmic_gpio_pdata.gpio_base = gpio_base;
	pmic_gpio_pdata.irq_base = gpio_base + MRST_IRQ_OFFSET;
	pmic_gpio_pdata.gpiointr = 0xffffeff8;

	return &pmic_gpio_pdata;
}

static void __init *max3111_platform_data(void *info)
{
	struct spi_board_info *spi_info = info;
	int intr = get_gpio_by_name("max3111_int");

	spi_info->mode = SPI_MODE_0;
	if (intr == -1)
		return NULL;
	spi_info->irq = intr + MRST_IRQ_OFFSET;
	return NULL;
}

/* we have multiple max7315 on the board ... */
#define MAX7315_NUM 2
static void __init *max7315_platform_data(void *info)
{
	static struct pca953x_platform_data max7315_pdata[MAX7315_NUM];
	static int nr;
	struct pca953x_platform_data *max7315 = &max7315_pdata[nr];
	struct i2c_board_info *i2c_info = info;
	int gpio_base, intr;
	char base_pin_name[SFI_NAME_LEN + 1];
	char intr_pin_name[SFI_NAME_LEN + 1];

	if (nr == MAX7315_NUM) {
		pr_err("too many max7315s, we only support %d\n",
				MAX7315_NUM);
		return NULL;
	}
	/* we have several max7315 on the board, we only need load several
	 * instances of the same pca953x driver to cover them
	 */
	strcpy(i2c_info->type, "max7315");
	if (nr++) {
		sprintf(base_pin_name, "max7315_%d_base", nr);
		sprintf(intr_pin_name, "max7315_%d_int", nr);
	} else {
		strcpy(base_pin_name, "max7315_base");
		strcpy(intr_pin_name, "max7315_int");
	}

	gpio_base = get_gpio_by_name(base_pin_name);
	intr = get_gpio_by_name(intr_pin_name);

	if (gpio_base == -1)
		return NULL;
	max7315->gpio_base = gpio_base;
	if (intr != -1) {
		i2c_info->irq = intr + MRST_IRQ_OFFSET;
		max7315->irq_base = gpio_base + MRST_IRQ_OFFSET;
	} else {
		i2c_info->irq = -1;
		max7315->irq_base = -1;
	}
	return max7315;
}

#define	NTC_47K_TGAIN	0xE4E4
#define	NTC_47K_TOFF	0x2F1D

static void *max17042_platform_data(void *info)
{
	static struct max17042_platform_data platform_data;
	struct i2c_board_info *i2c_info = (struct i2c_board_info *)info;
	int intr;
	static struct max17042_config_data config_data = {
		.tgain = NTC_47K_TGAIN,
		.toff = NTC_47K_TOFF,
		.config = 0x2210,
		.learn_cfg = 0x0076,
		.filter_cfg = 0x87A4,
		.relax_cfg = 0x506B,
		.rcomp0 = 0x0092,
		.tcompc0 = 0x081D,
		.empty_tempco = 0x0B19,
		.kempty0 = 0x0D83,
		.fullcap = 14727,
		.design_cap = 14727,
		.fullcapnom = 14727,
		.ichgt_term = 0x500,
		.cell_technology = POWER_SUPPLY_TECHNOLOGY_LION,
		.cell_char_tbl = {
	/* Data to be written from 0x80h */
	0xABB0, 0xB2B0, 0xBB10, 0xBBB0, 0xBC10, 0xBC70, 0xBD00, 0xBD70,
	0xBDC0, 0xBE10, 0xC010, 0xC130, 0xC4A0, 0xC9C0, 0xCD10, 0xD090,
	/* Data to be written from 0x90h */
	0x0620, 0x0420, 0x1900, 0x3600, 0x3DA0, 0x2CA0, 0x3C20, 0x3500,
	0x3500, 0x0440, 0x1240, 0x0DF0, 0x08F0, 0x0870, 0x07F0, 0x07F0,
	/* Data to be written from 0xA0h */
	0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
	0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
		},
	};

	intr = get_gpio_by_name("max17042");

	if (intr != -1)
		i2c_info->irq = intr + MRST_IRQ_OFFSET;

	platform_data.enable_por_init = 1;
	platform_data.config_data = &config_data;
	platform_data.enable_current_sense = 1;

	return &platform_data;
}

#define SMB347_IRQ_GPIO		52

static struct smb347_charger_platform_data smb347_pdata = {
	.battery_info	= {
		.name			= "UP110005",
		.technology		= POWER_SUPPLY_TECHNOLOGY_LIPO,
		.voltage_max_design	= 3700000,
		.voltage_min_design	= 3000000,
		.charge_full_design	= 6894000,
	},
	.max_charge_current		= 3360000,
	.max_charge_voltage		= 4200000,
	.otg_uvlo_voltage		= 3300000,
	.chip_temp_threshold		= 120,
	.soft_cold_temp_limit		= 5,
	.soft_hot_temp_limit		= 50,
	.hard_cold_temp_limit		= 5,
	.hard_hot_temp_limit		= 55,
	.suspend_on_hard_temp_limit	= true,
	.soft_temp_limit_compensation	= SMB347_SOFT_TEMP_COMPENSATE_CURRENT
					| SMB347_SOFT_TEMP_COMPENSATE_VOLTAGE,
	.charge_current_compensation	= 900000,
	.use_mains			= true,
	.enable_control			= SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	.otg_control			= SMB347_OTG_CONTROL_SW_AUTO,
	.irq_gpio			= SMB347_IRQ_GPIO,
};

static void *smb347_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = info;

	/*
	 * The I2C slave address is wrong in the SFI tables on Medfield
	 * tablets. Therefore we correct it here.
	 */
	if (i2c_info->addr != 6)
		i2c_info->addr = 6;

	return &smb347_pdata;
}

static void *tca6416_platform_data(void *info)
{
	static struct pca953x_platform_data tca6416;
	struct i2c_board_info *i2c_info = info;
	int gpio_base, intr;
	char base_pin_name[SFI_NAME_LEN + 1];
	char intr_pin_name[SFI_NAME_LEN + 1];

	strcpy(i2c_info->type, "tca6416");
	strcpy(base_pin_name, "tca6416_base");
	strcpy(intr_pin_name, "tca6416_int");

	gpio_base = get_gpio_by_name(base_pin_name);
	intr = get_gpio_by_name(intr_pin_name);

	if (gpio_base == -1)
		return NULL;
	tca6416.gpio_base = gpio_base;
	if (intr != -1) {
		i2c_info->irq = intr + MRST_IRQ_OFFSET;
		tca6416.irq_base = gpio_base + MRST_IRQ_OFFSET;
	} else {
		i2c_info->irq = -1;
		tca6416.irq_base = -1;
	}
	return &tca6416;
}

static void *mpu3050_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = info;
	int intr = get_gpio_by_name("gyro_int");

	if (intr == -1)
		return NULL;

	strcpy(i2c_info->type, "mpu3050");
	i2c_info->irq = intr + MRST_IRQ_OFFSET;

	return NULL;
}

static void *ltr502als_i2c_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = info;
	int intr;
	/* exit if sfi table has proper irq setting */
	if (i2c_info->irq)
		return NULL;

	intr = get_gpio_by_name("AL-intr");

	/*
	 * oddly driver expects gpio number in i2c_info->irq,
	 * driver also does gpio requesting, direction and gpio_to_irq().
	 */
	if (intr > 0)
		i2c_info->irq = intr;

	return NULL;
}

static void *ltr301als_platform_data(void *info)
{
	static struct ltr301als_platform_data ltr301_pdata;
	struct i2c_board_info *i2c_info = info;
	int intr;

	/* set irq number based on gpio if it's not set properly in i2c table */
	if (!i2c_info->irq) {
		intr = get_gpio_by_name("AL-intr");
		if (intr > 0)
			i2c_info->irq = intr + MRST_IRQ_OFFSET;
	}

	/* max opacity is 100, use 89% window loss */
	ltr301_pdata.window_opacity = 11;

	return &ltr301_pdata;
}

static void *ektf2136_spi_platform_data(void *info)
{
	static int dummy;
	struct spi_board_info *spi_info = info;
	int intr = get_gpio_by_name("ts_int");

	if (intr == -1)
		return NULL;
	spi_info->irq = intr + MRST_IRQ_OFFSET;

	/* we return a dummy pdata */
	return &dummy;
}

static void __init *emc1403_platform_data(void *info)
{
	static short intr2nd_pdata;
	struct i2c_board_info *i2c_info = info;
	int intr = get_gpio_by_name("thermal_int");
	int intr2nd = get_gpio_by_name("thermal_alert");

	if (intr == -1 || intr2nd == -1)
		return NULL;

	i2c_info->irq = intr + MRST_IRQ_OFFSET;
	intr2nd_pdata = intr2nd + MRST_IRQ_OFFSET;

	return &intr2nd_pdata;
}

static void __init *lis331dl_platform_data(void *info)
{
	static short intr2nd_pdata;
	struct i2c_board_info *i2c_info = info;
	int intr = get_gpio_by_name("accel_int");
	int intr2nd = get_gpio_by_name("accel_2");

	if (intr == -1 || intr2nd == -1)
		return NULL;

	i2c_info->irq = intr + MRST_IRQ_OFFSET;
	intr2nd_pdata = intr2nd + MRST_IRQ_OFFSET;

	return &intr2nd_pdata;
}

/* AUDIENCE es305 platform data */

#define AUDIENCE_WAKEUP_GPIO		"audience-wakeup"
#define AUDIENCE_RESET_GPIO		"audience-reset"

static int audience_wakeup_gpio, audience_reset_gpio;

static int audience_request_resources(struct i2c_client *client)
{
	int ret = gpio_request(audience_wakeup_gpio, AUDIENCE_WAKEUP_GPIO);
	if (ret) {
		dev_err(&client->dev, "AUDIENCE WAKEUP GPIO unavailable %d\n",
								ret);
		return -1;
	}
	ret = gpio_direction_output(audience_wakeup_gpio, 0);
	if (ret) {
		dev_err(&client->dev, "Set GPIO Direction fails %d\n", ret);
		goto err_wake;
	}

	ret = gpio_request(audience_reset_gpio, AUDIENCE_RESET_GPIO);
	if (ret) {
		dev_err(&client->dev,
			"Request for Audience reset GPIO fails %d\n", ret);
		goto err_reset;
	}
	ret = gpio_direction_output(audience_reset_gpio, 0);
	if (ret) {
		dev_err(&client->dev, "Set GPIO Direction fails %d\n", ret);
		goto err_reset;
	}
	return 0;
err_reset:
	gpio_free(audience_reset_gpio);
err_wake:
	gpio_free(audience_wakeup_gpio);
	return -1;
}

static void audience_free_resources(struct i2c_client *client)
{
	gpio_free(audience_reset_gpio);
	gpio_free(audience_wakeup_gpio);
}

static void audience_wake_up(bool state)
{
	gpio_set_value(audience_wakeup_gpio, !!state);
}

static void audience_reset(bool state)
{
	gpio_set_value(audience_reset_gpio, !!state);
}

void *audience_platform_data(void *info)
{
	static struct a1026_platform_data mfld_audience_platform_data;

	audience_wakeup_gpio = get_gpio_by_name(AUDIENCE_WAKEUP_GPIO);
	if (audience_wakeup_gpio == -1)
		return NULL;
	audience_reset_gpio = get_gpio_by_name(AUDIENCE_RESET_GPIO);
	if (audience_reset_gpio  == -1)
		return NULL;

	mfld_audience_platform_data.request_resources =
						audience_request_resources;
	mfld_audience_platform_data.release_resources =
						audience_free_resources;
	mfld_audience_platform_data.wakeup = audience_wake_up;
	mfld_audience_platform_data.reset = audience_reset;

	return &mfld_audience_platform_data;
}

struct sd_board_info {
	char		name[SFI_NAME_LEN];
	int		bus_num;
	unsigned short	addr;
	u32		board_ref_clock;
};

static struct sd_board_info wl12xx_sd_info;

static struct wl12xx_platform_data wlan_data __initdata = {
	.board_tcxo_clock = WL12XX_TCXOCLOCK_26,
	.platform_quirks = WL12XX_PLATFORM_QUIRK_EDGE_IRQ,
};

static void __init *wl12xx_platform_data_init(void *info)
{
	memcpy(&wl12xx_sd_info, info, sizeof(struct sd_board_info));
	wl12xx_set_platform_data(&wlan_data);
	return NULL;
}

static char wl12xx_sdio_dev_name[16];

static struct regulator_consumer_supply wlan_enable_supply[] = {
	REGULATOR_SUPPLY("vmmc", wl12xx_sdio_dev_name),
};

static struct regulator_init_data wlan_enable_init_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = wlan_enable_supply,
};

static struct fixed_voltage_config wlan_enable_config = {
	.supply_name		= "wl12xx-enable",
	.microvolts		= 1800000,
	.startup_delay		= 70000,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &wlan_enable_init_data,
};

static struct platform_device wlan_enable_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data	= &wlan_enable_config,
	},
};

static void wl12xx_sdio_init(void)
{
	int err, irq, irq_gpio, en_gpio;
	struct wl12xx_platform_data *wd;

#if defined(CONFIG_WL12XX_PLATFORM_DATA) || defined(CONFIG_WL12XX_COMPAT_PLATFORM_DATA)
	wd = (struct wl12xx_platform_data *)wl12xx_get_platform_data();
#else
	wd = ERR_PTR(-EINVAL);
#endif
	if (IS_ERR(wd))
		goto out_err;

	irq_gpio = get_gpio_by_name("WLAN-interrupt");
	if (irq_gpio == -1)
		goto out_err;

	err = gpio_request(irq_gpio, "wl12xx-irq");
	if (err < 0)
		goto out_free;

	err = gpio_direction_input(irq_gpio);
	if (err < 0)
		goto out_free;

	irq = gpio_to_irq(irq_gpio);
	if (irq < 0)
		goto out_free;

	wd->irq = irq;

	if (wl12xx_sd_info.board_ref_clock == 26000000)
		wd->board_ref_clock = WL12XX_REFCLOCK_26;
	else if (wl12xx_sd_info.board_ref_clock == 38400000)
		wd->board_ref_clock = WL12XX_REFCLOCK_38;
	else
		goto out_free;

	en_gpio = get_gpio_by_name("WLAN-enable");
	if (en_gpio == -1)
		goto out_free;

	wlan_enable_config.gpio = en_gpio;

	sprintf(wl12xx_sdio_dev_name, "0000:00:%02x.%01x",
		wl12xx_sd_info.addr >> 8, wl12xx_sd_info.addr & 0xff);

	err = platform_device_register(&wlan_enable_device);
	if (err)
		goto out_free;

	return;

out_free:
	gpio_free(irq_gpio);

out_err:
	pr_err("wl12xx sdio setup failed\n");
}

static int mrst_sdhci_pci_wl12xx_setup(struct sdhci_pci_data *data)
{
	static bool done;

	if (done)
		return 0;
	done = true;
	wl12xx_sdio_init();
	return 0;
}

static void mrst_sdhci_pci_cleanup(struct sdhci_pci_data *data)
{
	kfree(data);
}

static struct sdhci_pci_data *mrst_sdhci_pci_get_data(struct pci_dev *pdev,
						      int slotno)
{
	struct sdhci_pci_data *data;

	if (slotno)
		return NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_SD:
	case PCI_DEVICE_ID_INTEL_MFD_SDIO1:
	case PCI_DEVICE_ID_INTEL_MFD_EMMC0:
	case PCI_DEVICE_ID_INTEL_MFD_EMMC1:
		break;
	default:
		return NULL;
	}

	data = kzalloc(sizeof(struct sdhci_pci_data), GFP_KERNEL);
	if (!data)
		return NULL;

	data->pdev = pdev;
	data->slotno = slotno;
	data->rst_n_gpio = -EINVAL;
	data->cd_gpio = -EINVAL;
	data->cleanup = mrst_sdhci_pci_cleanup;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_SD:
		data->cd_gpio = 69;
	case PCI_DEVICE_ID_INTEL_MFD_SDIO1:
		data->setup = mrst_sdhci_pci_wl12xx_setup;
		break;
	case PCI_DEVICE_ID_INTEL_MFD_EMMC0:
		data->rst_n_gpio = get_gpio_by_name("emmc0_rst");
		if (data->rst_n_gpio == -1)
			data->rst_n_gpio = -EINVAL;
		break;
	case PCI_DEVICE_ID_INTEL_MFD_EMMC1:
		data->rst_n_gpio = get_gpio_by_name("EMMC1_RST_N");
		if (data->rst_n_gpio == -1)
			data->rst_n_gpio = get_gpio_by_name("emmc1_rst");
		if (data->rst_n_gpio == -1)
			data->rst_n_gpio = -EINVAL;
		break;
	}

	return data;
}

static int __init mrst_sdhci_init(void)
{
	if (mrst_identify_cpu())
		sdhci_pci_get_data = mrst_sdhci_pci_get_data;
	return 0;
}
arch_initcall(mrst_sdhci_init);

static void *baro_platform_data(void *data)
{
	static struct ms5607_platform_data pdata = {
		.poll_interval = 100,
		.min_interval = 0,
	};

	return &pdata;
}

/*
 * The I2C driver using this regulator is a dirty hack to overcome the problem
 * when LCD panels AVDD is turned off. It looks like it pulls the I2C bus lines
 * down towards 1V which causes the whole bus 2 to be unstable. To fix this we
 * require the LCD AVDD to be enabled also when using the I2C bus.
 *
 * By doing this we can be sure that all the devices are suspended before
 * AVDD goes down. Also in resume path we make sure that AVDD is turned on
 * before any devices behind the bus 2 are accessed.
 */
static struct regulator_consumer_supply lcd_regulator_supply[] = {
	REGULATOR_SUPPLY("vlcm_dvdd", "2-0060"),
	REGULATOR_SUPPLY("v-i2c", "0000:00:00.5"),
};

static struct regulator_init_data lcd_regulator_init_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(lcd_regulator_supply),
	.consumer_supplies = lcd_regulator_supply,
};

static struct fixed_voltage_config lcd_regulator_config = {
	.supply_name		= "lcd_regulator",
	.microvolts		= 3300000,
	.startup_delay		= 1000,
	.enable_high		= 1,
	.enabled_at_boot	= 1,
	.gpio			= -EINVAL,
	.init_data		= &lcd_regulator_init_data,
};

static struct platform_device lcd_regulator = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev = {
		.platform_data	= &lcd_regulator_config,
	},
};

static void __devinit mrst_lcd_regulator_init(struct pci_dev *dev)
{
	static bool initialized;
	int gpio;

	if (initialized)
		return;

	gpio = get_gpio_by_name("EN_VREG_LCD_V3P3");
	if (gpio >= 0)
		lcd_regulator_config.gpio = gpio;

	if (platform_device_register(&lcd_regulator))
		pr_err("failed to register LCD regulator\n");

	initialized = true;
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_INTEL, 0x082E, mrst_lcd_regulator_init);

#define CABC_EN_GPIO 176
#define CABC_EN_GPIO_NAME "CABC_EN"

/* tc35876x DSI-LVDS bridge chip and panel platform data */
static void *tc35876x_platform_data(void *data)
{
	static struct tc35876x_platform_data pdata;
	int cabc_en;

	/* gpio pins set to -1 will not be used by the driver */
	pdata.gpio_bridge_reset = get_gpio_by_name("LCMB_RXEN");
	pdata.gpio_panel_bl_en = get_gpio_by_name("6S6P_BL_EN");

	/* Enable CABC control pin on DV1.5 and later devices */
	cabc_en = get_gpio_by_name(CABC_EN_GPIO_NAME);

	/* HACK: The firmware in DV1.5 and later should know this pin, but
	 * it does not. This is a firmware bug, so work around it. It is
	 * ok to fiddle with this pin on DV1.0 and earlier since it's not
	 * connected to anything on those devices. Remove when firmware fixed.
	 */
	if (cabc_en < 0)
		cabc_en = CABC_EN_GPIO;

	/* Turn it on. It still needs to be enabled in the panel. */
	if (cabc_en > -1)
		gpio_request_one(cabc_en, GPIOF_OUT_INIT_HIGH,
				 CABC_EN_GPIO_NAME);

	return &pdata;
}

static void *hsi_modem_platform_data(void *data)
{
	static struct hsi_board_info hsi_info[2] = {
		[0] = {
			.name = "hsi_char",
			.hsi_id = 0,
			.port = 0,
			.archdata = NULL,
			.tx_cfg.speed = 200000,	/* tx clock, kHz */
			.tx_cfg.channels = 8,
			.tx_cfg.mode = HSI_MODE_FRAME,
/*			.tx_cfg.arb_mode = HSI_ARB_RR, */
/*			.rx_cfg.flow = HSI_FLOW_SYNC, */
			.rx_cfg.mode = HSI_MODE_FRAME,
			.rx_cfg.channels = 8
		},
		[1] = {
			.name = "hsi-ffl",
			.hsi_id = 0,
			.port = 0,
			.archdata = NULL,
			.tx_cfg.speed = 100000,	/* tx clock, kHz */
			.tx_cfg.channels = 8,
			.tx_cfg.mode = HSI_MODE_FRAME,
/*			.tx_cfg.arb_mode = HSI_ARB_RR, */
/*			.rx_cfg.flow = HSI_FLOW_SYNC, */
			.rx_cfg.mode = HSI_MODE_FRAME,
			.rx_cfg.channels = 8
		}
	};

	static struct hsi_mid_platform_data mid_info = {
		.tx_dma_channels[0] = -1,
		.tx_dma_channels[1] = 5,
		.tx_dma_channels[2] = -1,
		.tx_dma_channels[3] = -1,
		.tx_dma_channels[4] = -1,
		.tx_dma_channels[5] = -1,
		.tx_dma_channels[6] = -1,
		.tx_dma_channels[7] = -1,
		.tx_fifo_sizes[0] = -1,
		.tx_fifo_sizes[1] = 1024,
		.tx_fifo_sizes[2] = -1,
		.tx_fifo_sizes[3] = -1,
		.tx_fifo_sizes[4] = -1,
		.tx_fifo_sizes[5] = -1,
		.tx_fifo_sizes[6] = -1,
		.tx_fifo_sizes[7] = -1,
		.rx_dma_channels[0] = -1,
		.rx_dma_channels[1] = 1,
		.rx_dma_channels[2] = -1,
		.rx_dma_channels[3] = -1,
		.rx_dma_channels[4] = -1,
		.rx_dma_channels[5] = -1,
		.rx_dma_channels[6] = -1,
		.rx_dma_channels[7] = -1,
		.rx_fifo_sizes[0] = -1,
		.rx_fifo_sizes[1] = 1024,
		.rx_fifo_sizes[2] = -1,
		.rx_fifo_sizes[3] = -1,
		.rx_fifo_sizes[4] = -1,
		.rx_fifo_sizes[5] = -1,
		.rx_fifo_sizes[6] = -1,
		.rx_fifo_sizes[7] = -1,
	};
	int rst_out = get_gpio_by_name("ifx_mdm_rst_out");
	int pwr_on = get_gpio_by_name("ifx_mdm_pwr_on");
	int rst_pmu = get_gpio_by_name("ifx_mdm_rst_pmu");
	int fcdp_rb = get_gpio_by_name("modem-gpio2");

	/* GCC is being annoying about initializers of sub elements
	   of anonymous unions, so them by hand afterwards instead */

	printk(KERN_INFO "HSI platform data setup\n");

	printk(KERN_INFO "HSI mdm GPIOs %d, %d, %d, %d\n",
	       rst_out, pwr_on, rst_pmu, fcdp_rb);

	mid_info.gpio_mdm_rst_out = rst_out;
	mid_info.gpio_mdm_pwr_on = pwr_on;
	mid_info.gpio_mdm_rst_bbn = rst_pmu;
	mid_info.gpio_fcdp_rb = fcdp_rb;

	hsi_info[0].rx_cfg.flow = HSI_FLOW_SYNC;
	hsi_info[1].rx_cfg.flow = HSI_FLOW_SYNC;
	hsi_info[0].tx_cfg.arb_mode = HSI_ARB_RR;
	hsi_info[1].tx_cfg.arb_mode = HSI_ARB_RR;

	hsi_info[0].platform_data = (void *)&mid_info;
	hsi_info[1].platform_data = (void *)&mid_info;

	return &hsi_info[0];
}

/* MFLD iCDK touchscreen data */
#define CYTTSP_GPIO_PIN		0x3E
static int cyttsp_init(int on)
{
	int ret;

	if (on) {
		ret = gpio_request(CYTTSP_GPIO_PIN, "cyttsp_irq");
		if (ret < 0) {
			pr_err("%s: gpio request failed\n", __func__);
			return ret;
		}

		ret = gpio_direction_input(CYTTSP_GPIO_PIN);
		if (ret < 0) {
			pr_err("%s: gpio direction config failed\n", __func__);
			gpio_free(CYTTSP_GPIO_PIN);
			return ret;
		}
	} else {
		gpio_free(CYTTSP_GPIO_PIN);
	}
	return 0;
}

static void *cyttsp_platform_data(void *info)
{
	static struct cyttsp_platform_data cyttsp_pdata = {
		.init = cyttsp_init,
		.mt_sync = input_mt_sync,
		.maxx = 479,
		.maxy = 853,
		.flags = 0,
		.gen = CY_GEN3,
		.use_st = 0,
		.use_mt = 1,
		.use_trk_id = 0,
		.use_hndshk = 1,
		.use_timer = 0,
		.use_sleep = 1,
		.use_gestures = 0,
		.act_intrvl = CY_ACT_INTRVL_DFLT,
		.tch_tmout = CY_TCH_TMOUT_DFLT,
		.lp_intrvl = CY_LP_INTRVL_DFLT / 2,
		.name = CY_I2C_NAME,
		.irq_gpio = CYTTSP_GPIO_PIN,
	};

	return &cyttsp_pdata;
}

/* Atmel mxt toucscreen platform setup*/
static int atmel_mxt_init_platform_hw(void)
{
	int rc;
	int reset_gpio, int_gpio;

	reset_gpio = get_gpio_by_name("ts_rst");
	int_gpio = get_gpio_by_name("ts_int");

	/* init interrupt gpio */
	rc = gpio_request(int_gpio, "mxt_ts_intr");
	if (rc < 0)
		return rc;

	rc = gpio_direction_input(int_gpio);
	if (rc < 0)
		goto err_int;

	/* init reset gpio */
	rc = gpio_request(reset_gpio, "mxt_ts_rst");
	if (rc < 0)
		goto err_int;

	rc = gpio_direction_output(reset_gpio, 1);
	if (rc < 0) {
		goto err_reset;
	}

	/* reset the chip */
	gpio_set_value(reset_gpio, 1);
	msleep(10);
	gpio_set_value(reset_gpio, 0);
	msleep(10);
	gpio_set_value(reset_gpio, 1);
	msleep(100);

	return 0;

err_reset:
	gpio_free(reset_gpio);
err_int:
	pr_err("mxt touchscreen: configuring reset or int gpio failed\n");
	gpio_free(int_gpio);

	return rc;
}

static void *atmel_mxt_platform_data_init(void *info)
{
	struct i2c_board_info *i2c_info = (struct i2c_board_info *) info;
	static struct mxt_platform_data mxt_platform_data;
	int ts_intr = get_gpio_by_name("ts_int");

	memset(&mxt_platform_data, 0x00,
	       sizeof(struct mxt_platform_data));

	mxt_platform_data.irqflags = IRQF_TRIGGER_FALLING;
	mxt_platform_data.init_platform_hw = &atmel_mxt_init_platform_hw;

	i2c_info->irq = ts_intr + MRST_IRQ_OFFSET;

	return &mxt_platform_data;
}

static void *lis3dh_platform_data(void *info)
{
	static struct lis3dh_acc_platform_data lis3dh_pdata = {
		.poll_interval = 200,
		.negate_x = 1,
		.negate_y = 0,
		.negate_z = 0,
		.axis_map_x = 0,
		.axis_map_y = 1,
		.axis_map_z = 2,
		.gpio_int1 = 60,
		.gpio_int2 = 61,
	};
	return &lis3dh_pdata;
}

static int nfc_gpio_enable = -1;
static int nfc_gpio_fw_reset = -1;
static int nfc_gpio_irq = -1;

static int pn544_request_resources(struct i2c_client *client)
{
	int ret;

	ret = gpio_request_one(nfc_gpio_enable, GPIOF_OUT_INIT_LOW,
			       "NFC enable");
	if (ret < 0)
		return ret;

	ret = gpio_request_one(nfc_gpio_fw_reset, GPIOF_OUT_INIT_LOW,
			       "NFC FW reset");
	if (ret < 0)
		goto fail_free_enable;

	ret = gpio_request_one(nfc_gpio_irq, GPIOF_IN, "NFC interrupt\n");
	if (ret < 0)
		goto fail_free_reset;

	client->irq = gpio_to_irq(nfc_gpio_irq);
	return 0;

fail_free_reset:
	gpio_free(nfc_gpio_fw_reset);
fail_free_enable:
	gpio_free(nfc_gpio_enable);

	return ret;
}

static void pn544_free_resources(void)
{
	gpio_free(nfc_gpio_irq);
	gpio_free(nfc_gpio_fw_reset);
	gpio_free(nfc_gpio_enable);
}

static void pn544_enable(int fw)
{
	gpio_set_value(nfc_gpio_enable, 1);
	gpio_set_value(nfc_gpio_fw_reset, !!fw);
}

static void pn544_disable(void)
{
	gpio_set_value(nfc_gpio_enable, 0);
}

static int pn544_get_gpio(int type)
{
	int gpio = -1;

	switch (type) {
	case NFC_GPIO_ENABLE:
		gpio = nfc_gpio_enable;
		break;
	case NFC_GPIO_FW_RESET:
		gpio = nfc_gpio_fw_reset;
		break;
	case NFC_GPIO_IRQ:
		gpio = nfc_gpio_irq;
		break;
	}

	return gpio;
}

static struct regulator_consumer_supply pn544_regs_supply[] = {
	REGULATOR_SUPPLY("Vdd_IO", "2-0028"),
	REGULATOR_SUPPLY("VBat", "2-0028"),
	REGULATOR_SUPPLY("VSim", "2-0028"),
};

static struct regulator_init_data pn544_regs_init_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(pn544_regs_supply),
	.consumer_supplies = pn544_regs_supply,
};

static struct fixed_voltage_config pn544_regs_config = {
	.supply_name		= "pn544_regs",
	.microvolts		= 1800000,
	.gpio			= -EINVAL,
	.init_data		= &pn544_regs_init_data,
};

static struct platform_device pn544_regs = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev = {
		.platform_data	= &pn544_regs_config,
	},
};

static void *pn544_platform_data(void *info)
{
	static struct pn544_nfc_platform_data pdata = {
		.request_resources	= pn544_request_resources,
		.free_resources		= pn544_free_resources,
		.enable			= pn544_enable,
		.disable		= pn544_disable,
		.get_gpio		= pn544_get_gpio,
	};

	nfc_gpio_enable = get_gpio_by_name("NFC-enable");
	if (nfc_gpio_enable < 0) {
		pr_err("failed to get NFC enable GPIO\n");
		return NULL;
	}

	nfc_gpio_fw_reset = get_gpio_by_name("NFC-reset");
	if (nfc_gpio_fw_reset < 0) {
		pr_err("failed to get NFC reset GPIO\n");
		return NULL;
	}

	nfc_gpio_irq = get_gpio_by_name("NFC-intr");
	if (nfc_gpio_irq < 0) {
		pr_err("failed to get NFC interrupt GPIO\n");
		return NULL;
	}

	if (platform_device_register(&pn544_regs)) {
		pr_err("failed to register NFC fixed voltage regulator\n");
		return NULL;
	}

	return &pdata;
}

static void __init *no_platform_data(void *info)
{
	return NULL;
}

static struct resource msic_resources[] = {
	{
		.start	= INTEL_MSIC_IRQ_PHYS_BASE,
		.end	= INTEL_MSIC_IRQ_PHYS_BASE + 64 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct intel_msic_platform_data msic_pdata;

static struct platform_device msic_device = {
	.name		= "intel_msic",
	.id		= -1,
	.dev		= {
		.platform_data	= &msic_pdata,
	},
	.num_resources	= ARRAY_SIZE(msic_resources),
	.resource	= msic_resources,
};

static inline bool mrst_has_msic(void)
{
	return mrst_identify_cpu() == MRST_CPU_CHIP_PENWELL;
}

static int msic_scu_status_change(struct notifier_block *nb,
				  unsigned long code, void *data)
{
	if (code == SCU_DOWN) {
		platform_device_unregister(&msic_device);
		return 0;
	}

	return platform_device_register(&msic_device);
}

static int __init msic_init(void)
{
	static struct notifier_block msic_scu_notifier = {
		.notifier_call	= msic_scu_status_change,
	};

	/*
	 * We need to be sure that the SCU IPC is ready before MSIC device
	 * can be registered.
	 */
	if (mrst_has_msic())
		intel_scu_notifier_add(&msic_scu_notifier);

	return 0;
}
arch_initcall(msic_init);

/*
 * msic_generic_platform_data - sets generic platform data for the block
 * @info: pointer to the SFI device table entry for this block
 * @block: MSIC block
 *
 * Function sets IRQ number from the SFI table entry for given device to
 * the MSIC platform data.
 */
static void *msic_generic_platform_data(void *info, enum intel_msic_block block)
{
	struct sfi_device_table_entry *entry = info;

	BUG_ON(block < 0 || block >= INTEL_MSIC_BLOCK_LAST);
	msic_pdata.irq[block] = entry->irq;

	return no_platform_data(info);
}

static void *msic_adc_platform_data(void *info)
{
	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_ADC);
}

static void *msic_battery_platform_data(void *info)
{
	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_BATTERY);
}

static void *msic_gpio_platform_data(void *info)
{
	static struct intel_msic_gpio_pdata pdata;
	int gpio = get_gpio_by_name("msic_gpio_base");

	if (gpio < 0)
		return NULL;

	pdata.gpio_base = gpio;
	msic_pdata.gpio = &pdata;

	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_GPIO);
}

static void *msic_audio_platform_data(void *info)
{
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_alloc("switch-mid", -1);
	if (!pdev) {
		pr_err("failed to allocate switch-mid platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add switch-mid platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	pdev = platform_device_alloc("sst-platform", -1);
	if (!pdev) {
		pr_err("failed to allocate audio platform device\n");
		return NULL;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add audio platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_AUDIO);
}

static void *msic_power_btn_platform_data(void *info)
{
	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_POWER_BTN);
}

static void *msic_ocd_platform_data(void *info)
{
	static struct intel_msic_ocd_pdata pdata;
	int gpio = get_gpio_by_name("ocd_gpio");

	if (gpio < 0)
		return NULL;

	pdata.gpio = gpio;
	msic_pdata.ocd = &pdata;

	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_OCD);
}

static void *msic_thermal_platform_data(void *info)
{
	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_THERMAL);
}

/* MFLD camera sensor GPIOs, from firmware */

#define GP_CAMERA_0_POWER_DOWN		"cam0_vcm_2p8"
#define GP_CAMERA_1_POWER_DOWN		"GP_CAMERA_1_PWN"
#define GP_CAMERA_0_RESET		"GP_CAMERA_0_RST"
#define GP_CAMERA_1_RESET		"GP_CAMERA_1_RST"
/* Need modify sensor driver's platform data structure to eliminate static */
static int gp_camera0_reset;
static int gp_camera0_power_down;
static int gp_camera1_reset;
static int gp_camera1_power_down;
static int camera_vprog1_on;

/*
 * One-time gpio initialization.
 * @name: gpio name: coded in SFI table
 * @gpio: gpio pin number (bypass @name)
 * @dir: GPIOF_DIR_IN or GPIOF_DIR_OUT
 * @value: if dir = GPIOF_DIR_OUT, this is the init value for output pin
 * if dir = GPIOF_DIR_IN, this argument is ignored
 * return: a positive pin number if succeeds, otherwise a negative value
 */
static int camera_sensor_gpio(int gpio, char *name, int dir, int value)
{
	int ret, pin;

	if (gpio == -1) {
		pin = get_gpio_by_name(name);
		if (pin == -1) {
			pr_debug("%s: no gpio(name: %s), expected on dv1\n",
				__func__, name);
			return -EINVAL;
		}
	} else {
		pin = gpio;
	}

	ret = gpio_request(pin, name);
	if (ret) {
		pr_err("%s: failed to request gpio(pin %d)\n", __func__, pin);
		return -EINVAL;
	}

	if (dir == GPIOF_DIR_OUT)
		ret = gpio_direction_output(pin, value);
	else
		ret = gpio_direction_input(pin);

	if (ret) {
		pr_err("%s: failed to set gpio(pin %d) direction\n",
							__func__, pin);
		gpio_free(pin);
	}

	return ret ? ret : pin;
}

/*
 * Configure MIPI CSI physical parameters.
 * @port: ATOMISP_CAMERA_PORT_PRIMARY or ATOMISP_CAMERA_PORT_SECONDARY
 * @lanes: for ATOMISP_CAMERA_PORT_PRIMARY, there could be 2 or 4 lanes
 * for ATOMISP_CAMERA_PORT_SECONDARY, there is only one lane.
 * @format: MIPI CSI pixel format, see include/linux/atomisp_platform.h
 * @bayer_order: MIPI CSI bayer order, see include/linux/atomisp_platform.h
 */
static int camera_sensor_csi(struct v4l2_subdev *sd, u32 port,
			u32 lanes, u32 format, u32 bayer_order, int flag)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *csi = NULL;

	if (flag) {
		csi = kzalloc(sizeof(*csi), GFP_KERNEL);
		if (!csi) {
			dev_err(&client->dev, "out of memory\n");
			return -ENOMEM;
		}
		csi->port = port;
		csi->num_lanes = lanes;
		csi->input_format = format;
		csi->raw_bayer_order = bayer_order;
		v4l2_set_subdev_hostdata(sd, (void *)csi);
	} else {
		csi = v4l2_get_subdev_hostdata(sd);
		kfree(csi);
	}

	return 0;
}


/*
 * MFLD PR2 primary camera sensor - MT9E013 platform data
 */
static int mt9e013_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	if (gp_camera0_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					 GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		gp_camera0_reset = ret;
	}

	if (flag) {
		gpio_set_value(gp_camera0_reset, 0);
		msleep(20);
		gpio_set_value(gp_camera0_reset, 1);
	} else {
		gpio_set_value(gp_camera0_reset, 0);
	}

	return 0;
}

static int mt9e013_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, flag);
}

static int mt9e013_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	/*
	 * Start from DV1 board, this gpio pin may be invalid for different
	 * boards. If we can't get this gpio pin from firmware, this pin
	 * should be ignored.
	 */
	if (gp_camera0_power_down < 0)
		gp_camera0_power_down = camera_sensor_gpio(-1,
				GP_CAMERA_0_POWER_DOWN, GPIOF_DIR_OUT, 1);

	if (flag) {
		if (gp_camera0_power_down >= 0)
			gpio_set_value(gp_camera0_power_down, 1);
		if (!camera_vprog1_on) {
			camera_vprog1_on = 1;
			intel_scu_ipc_msic_vprog1(1);
		}
	} else {
		if (camera_vprog1_on) {
			camera_vprog1_on = 0;
			intel_scu_ipc_msic_vprog1(0);
		}
		if (gp_camera0_power_down >= 0)
			gpio_set_value(gp_camera0_power_down, 0);
	}

	return 0;
}

static int mt9e013_csi_configure(struct v4l2_subdev *sd, int flag)
{
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, 2,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_grbg, flag);
}

static struct camera_sensor_platform_data mt9e013_sensor_platform_data = {
	.gpio_ctrl	= mt9e013_gpio_ctrl,
	.flisclk_ctrl	= mt9e013_flisclk_ctrl,
	.power_ctrl	= mt9e013_power_ctrl,
	.csi_cfg	= mt9e013_csi_configure,
};

static void *mt9e013_platform_data_init(void *info)
{
	gp_camera0_reset = -1;
	gp_camera0_power_down = -1;

	return &mt9e013_sensor_platform_data;
}

/*
 * MFLD PR2 secondary camera sensor - MT9M114 platform data
 */
static int mt9m114_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	if (gp_camera1_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
					 GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		gp_camera1_reset = ret;
	}

	if (flag)
		gpio_set_value(gp_camera1_reset, 1);
	else
		gpio_set_value(gp_camera1_reset, 0);

	return 0;
}

static int mt9m114_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM1, flag);
}

static int mt9e013_reset;
static int mt9m114_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	/*
	 * Start from DV1 board, this gpio pin may be invalid for different
	 * boards. If we can't get this gpio pin from firmware, this pin
	 * should be ignored.
	 */
	if (gp_camera1_power_down < 0)
		gp_camera1_power_down = camera_sensor_gpio(-1,
				GP_CAMERA_1_POWER_DOWN, GPIOF_DIR_OUT, 1);

	if (gp_camera1_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
					 GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		gp_camera1_reset = ret;
	}

	if (flag) {
		/* workaround to avoid I2C SDA to be pulled down by camera 0 */
		if (!mt9e013_reset) {
			mt9e013_power_ctrl(sd, 1);
			mt9e013_gpio_ctrl(sd, 0);
			mt9e013_gpio_ctrl(sd, 1);
			mt9e013_gpio_ctrl(sd, 0);
			mt9e013_power_ctrl(sd, 0);
			mt9e013_reset = 1;
		}
		gpio_direction_output(gp_camera1_reset, 0);
		gpio_set_value(gp_camera1_reset, 0);
		if (!camera_vprog1_on) {
			camera_vprog1_on = 1;
			intel_scu_ipc_msic_vprog1(1);
		}
		if (gp_camera1_power_down >= 0)
			gpio_set_value(gp_camera1_power_down, 1);
	} else {
		if (camera_vprog1_on) {
			camera_vprog1_on = 0;
			intel_scu_ipc_msic_vprog1(0);
		}
		if (gp_camera1_power_down >= 0)
			gpio_set_value(gp_camera1_power_down, 0);

		mt9e013_reset = 0;
	}

	return 0;
}

static int mt9m114_csi_configure(struct v4l2_subdev *sd, int flag)
{
	/* soc sensor, there is no raw bayer order (set to -1) */
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, 1,
		ATOMISP_INPUT_FORMAT_YUV422_8, -1, flag);
}

static struct camera_sensor_platform_data mt9m114_sensor_platform_data = {
	.gpio_ctrl	= mt9m114_gpio_ctrl,
	.flisclk_ctrl	= mt9m114_flisclk_ctrl,
	.power_ctrl	= mt9m114_power_ctrl,
	.csi_cfg	= mt9m114_csi_configure,
};

static void *mt9m114_platform_data_init(void *info)
{
	gp_camera1_reset = -1;
	gp_camera1_power_down = -1;

	return &mt9m114_sensor_platform_data;
}

static void *lm3554_platform_data_init(void *info)
{
	static struct camera_flash_platform_data lm3554_platform_data;
	void *ret = &lm3554_platform_data;

	lm3554_platform_data.gpio_reset  = get_gpio_by_name("GP_FLASH_RESET");
	lm3554_platform_data.gpio_strobe = get_gpio_by_name("GP_FLASH_STROBE");
	lm3554_platform_data.gpio_torch  = get_gpio_by_name("GP_FLASH_TORCH");

	if (lm3554_platform_data.gpio_reset == -1) {
		pr_err("%s: Unable to find GP_FLASH_RESET\n", __func__);
		ret = NULL;
	}
	if (lm3554_platform_data.gpio_strobe == -1) {
		pr_err("%s: Unable to find GP_FLASH_STROBE\n", __func__);
		ret = NULL;
	}
	if (lm3554_platform_data.gpio_torch == -1) {
		pr_err("%s: Unable to find GP_FLASH_TORCH\n", __func__);
		ret = NULL;
	}

	return ret;
}

static const struct devs_id __initconst device_ids[] = {
	{"bma023", SFI_DEV_TYPE_I2C, 1, &no_platform_data},
	{"pmic_gpio", SFI_DEV_TYPE_SPI, 1, &pmic_gpio_platform_data},
	{"pmic_gpio", SFI_DEV_TYPE_IPC, 1, &pmic_gpio_platform_data},
	{"cy8ctma340", SFI_DEV_TYPE_I2C, 1, &cyttsp_platform_data},
	{"spi_max3111", SFI_DEV_TYPE_SPI, 0, &max3111_platform_data},
	{"i2c_max7315", SFI_DEV_TYPE_I2C, 1, &max7315_platform_data},
	{"i2c_max7315_2", SFI_DEV_TYPE_I2C, 1, &max7315_platform_data},
	{"tca6416", SFI_DEV_TYPE_I2C, 1, &tca6416_platform_data},
	{"emc1403", SFI_DEV_TYPE_I2C, 1, &emc1403_platform_data},
	{"i2c_accel", SFI_DEV_TYPE_I2C, 0, &lis331dl_platform_data},
	{"pmic_audio", SFI_DEV_TYPE_IPC, 1, &no_platform_data},
	{"gyro", SFI_DEV_TYPE_I2C, 1, &mpu3050_platform_data},
	{"max17042", SFI_DEV_TYPE_I2C, 0, &max17042_platform_data},
	{"ektf2136_spi", SFI_DEV_TYPE_SPI, 0, &ektf2136_spi_platform_data},
	{"hsi_ifx_modem", SFI_DEV_TYPE_HSI, 0, &hsi_modem_platform_data},
	{"audience_es305", SFI_DEV_TYPE_I2C, 0, &audience_platform_data},
	{"wl12xx_clk_vmmc", SFI_DEV_TYPE_SD, 0, &wl12xx_platform_data_init},
	{"mxt1386", SFI_DEV_TYPE_I2C, 1, &atmel_mxt_platform_data_init},
	{"WintekA0", SFI_DEV_TYPE_I2C, 1, &atmel_mxt_platform_data_init},
	{"HannstouchA0", SFI_DEV_TYPE_I2C, 1, &atmel_mxt_platform_data_init},
	{"als", SFI_DEV_TYPE_I2C, 0, &ltr502als_i2c_platform_data},
	{"ltr301", SFI_DEV_TYPE_I2C, 0, &ltr301als_platform_data},
	{"accel", SFI_DEV_TYPE_I2C, 0, &lis3dh_platform_data},
	{"pn544", SFI_DEV_TYPE_I2C, 0, &pn544_platform_data},
	{"baro", SFI_DEV_TYPE_I2C, 0, &baro_platform_data},
	{"i2c_disp_brig", SFI_DEV_TYPE_I2C, 0, &tc35876x_platform_data},
	{"smb347", SFI_DEV_TYPE_I2C, 0, &smb347_platform_data},

	/* MSIC subdevices */
	{"msic_adc", SFI_DEV_TYPE_IPC, 1, &msic_adc_platform_data},
	{"msic_battery", SFI_DEV_TYPE_IPC, 1, &msic_battery_platform_data},
	{"msic_gpio", SFI_DEV_TYPE_IPC, 1, &msic_gpio_platform_data},
	{"msic_audio", SFI_DEV_TYPE_IPC, 1, &msic_audio_platform_data},
	{"msic_power_btn", SFI_DEV_TYPE_IPC, 1, &msic_power_btn_platform_data},
	{"msic_ocd", SFI_DEV_TYPE_IPC, 1, &msic_ocd_platform_data},
	{"msic_thermal", SFI_DEV_TYPE_IPC, 1, &msic_thermal_platform_data},

	/*
	 * I2C devices for camera image subsystem which will not be load into
	 * I2C core while initialize
	 */
	{"lm3554", SFI_DEV_TYPE_I2C, 0, &lm3554_platform_data_init},
	{"mt9e013", SFI_DEV_TYPE_I2C, 0, &mt9e013_platform_data_init},
	{"mt9m114", SFI_DEV_TYPE_I2C, 0, &mt9m114_platform_data_init},

	{},
};

static const struct intel_v4l2_subdev_id v4l2_ids[] = {
	{"mt9e013", RAW_CAMERA, ATOMISP_CAMERA_PORT_PRIMARY},
	{"mt9m114", SOC_CAMERA, ATOMISP_CAMERA_PORT_SECONDARY},
	{"lm3554", LED_FLASH, -1},
	{},
};

static struct atomisp_platform_data *v4l2_subdev_table_head;

static void intel_ignore_i2c_device_register(int bus,
					     struct i2c_board_info *idev)
{
	const struct intel_v4l2_subdev_id *vdev = v4l2_ids;
	struct intel_v4l2_subdev_i2c_board_info *info;
	static struct intel_v4l2_subdev_table *subdev_table;
	enum intel_v4l2_subdev_type type = 0;
	enum atomisp_camera_port port;
	static int i;

	while (vdev->name[0]) {
		if (!strncmp(vdev->name, idev->type, 16)) {
			/* compare name */
			type = vdev->type;
			port = vdev->port;
			break;
		}
		vdev++;
	}

	if (!type) /* not found */
		return;

	info = kzalloc(sizeof(struct intel_v4l2_subdev_i2c_board_info),
		       GFP_KERNEL);
	if (!info) {
		pr_err("MRST: fail to alloc mem for ignored i2c dev %s\n",
		       idev->type);
		return;
	}

	info->i2c_adapter_id = bus;
	/* set platform data */
	memcpy(&info->board_info, idev, sizeof(*idev));

	if (v4l2_subdev_table_head == NULL) {
		subdev_table = kzalloc(sizeof(struct intel_v4l2_subdev_table)
			* ARRAY_SIZE(v4l2_ids), GFP_KERNEL);

		if (!subdev_table) {
			pr_err("MRST: fail to alloc mem for v4l2_subdev_table %s\n",
			       idev->type);
			kfree(info);
			return;
		}

		v4l2_subdev_table_head = kzalloc(
			sizeof(struct atomisp_platform_data), GFP_KERNEL);
		if (!v4l2_subdev_table_head) {
			pr_err("MRST: fail to alloc mem for v4l2_subdev_table %s\n",
			       idev->type);
			kfree(info);
			kfree(subdev_table);
			return;
		}
		v4l2_subdev_table_head->subdevs = subdev_table;
	}

	memcpy(&subdev_table[i].v4l2_subdev, info, sizeof(*info));
	subdev_table[i].type = type;
	subdev_table[i].port = port;
	i++;
	kfree(info);
	return;
}

const struct atomisp_platform_data *intel_get_v4l2_subdev_table(void)
{
	if (v4l2_subdev_table_head)
		return v4l2_subdev_table_head;
	else {
		pr_err("MRST: no camera device in the SFI table\n");
		return NULL;
	}
}
EXPORT_SYMBOL_GPL(intel_get_v4l2_subdev_table);

#define MAX_IPCDEVS	24
static struct platform_device *ipc_devs[MAX_IPCDEVS];
static int ipc_next_dev;

#define MAX_SCU_SPI	24
static struct spi_board_info *spi_devs[MAX_SCU_SPI];
static int spi_next_dev;

#define MAX_SCU_I2C	24
static struct i2c_board_info *i2c_devs[MAX_SCU_I2C];
static int i2c_bus[MAX_SCU_I2C];
static int i2c_next_dev;

static void __init intel_scu_device_register(struct platform_device *pdev)
{
	if(ipc_next_dev == MAX_IPCDEVS)
		pr_err("too many SCU IPC devices");
	else
		ipc_devs[ipc_next_dev++] = pdev;
}

static void __init intel_scu_spi_device_register(struct spi_board_info *sdev)
{
	struct spi_board_info *new_dev;

	if (spi_next_dev == MAX_SCU_SPI) {
		pr_err("too many SCU SPI devices");
		return;
	}

	new_dev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	if (!new_dev) {
		pr_err("failed to alloc mem for delayed spi dev %s\n",
			sdev->modalias);
		return;
	}
	memcpy(new_dev, sdev, sizeof(*sdev));

	spi_devs[spi_next_dev++] = new_dev;
}

static void __init intel_scu_i2c_device_register(int bus,
						struct i2c_board_info *idev)
{
	struct i2c_board_info *new_dev;

	if (i2c_next_dev == MAX_SCU_I2C) {
		pr_err("too many SCU I2C devices");
		return;
	}

	new_dev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (!new_dev) {
		pr_err("failed to alloc mem for delayed i2c dev %s\n",
			idev->type);
		return;
	}
	memcpy(new_dev, idev, sizeof(*idev));

	i2c_bus[i2c_next_dev] = bus;
	i2c_devs[i2c_next_dev++] = new_dev;
}

BLOCKING_NOTIFIER_HEAD(intel_scu_notifier);
EXPORT_SYMBOL_GPL(intel_scu_notifier);

/* Called by IPC driver */
void intel_scu_devices_create(void)
{
	int i;

	for (i = 0; i < ipc_next_dev; i++)
		platform_device_add(ipc_devs[i]);

	for (i = 0; i < spi_next_dev; i++)
		spi_register_board_info(spi_devs[i], 1);

	for (i = 0; i < i2c_next_dev; i++) {
		struct i2c_adapter *adapter;
		struct i2c_client *client;

		adapter = i2c_get_adapter(i2c_bus[i]);
		if (adapter) {
			client = i2c_new_device(adapter, i2c_devs[i]);
			if (!client)
				pr_err("can't create i2c device %s\n",
					i2c_devs[i]->type);
		} else
			i2c_register_board_info(i2c_bus[i], i2c_devs[i], 1);
	}
	intel_scu_notifier_post(SCU_AVAILABLE, 0L);
}
EXPORT_SYMBOL_GPL(intel_scu_devices_create);

/* Called by IPC driver */
void intel_scu_devices_destroy(void)
{
	int i;

	intel_scu_notifier_post(SCU_DOWN, 0L);

	for (i = 0; i < ipc_next_dev; i++)
		platform_device_del(ipc_devs[i]);
}
EXPORT_SYMBOL_GPL(intel_scu_devices_destroy);

static void __init install_irq_resource(struct platform_device *pdev, int irq)
{
	/* Single threaded */
	static struct resource __initdata res = {
		.name = "IRQ",
		.flags = IORESOURCE_IRQ,
	};
	res.start = irq;
	platform_device_add_resources(pdev, &res, 1);
}

static void __init sfi_handle_ipc_dev(struct sfi_device_table_entry *entry)
{
	const struct devs_id *dev = device_ids;
	struct platform_device *pdev;
	void *pdata = NULL;

	while (dev->name[0]) {
		if (dev->type == SFI_DEV_TYPE_IPC &&
			!strncmp(dev->name, entry->name, SFI_NAME_LEN)) {
			pdata = dev->get_platform_data(entry);
			break;
		}
		dev++;
	}

	/*
	 * On Medfield the platform device creation is handled by the MSIC
	 * MFD driver so we don't need to do it here.
	 */
	if (mrst_has_msic())
		return;

	pdev = platform_device_alloc(entry->name, 0);
	if (pdev == NULL) {
		pr_err("out of memory for SFI platform device '%s'.\n",
			entry->name);
		return;
	}
	install_irq_resource(pdev, entry->irq);

	pdev->dev.platform_data = pdata;
	intel_scu_device_register(pdev);
}

static void __init sfi_handle_spi_dev(struct spi_board_info *spi_info)
{
	const struct devs_id *dev = device_ids;
	void *pdata = NULL;

	while (dev->name[0]) {
		if (dev->type == SFI_DEV_TYPE_SPI &&
				!strncmp(dev->name, spi_info->modalias, SFI_NAME_LEN)) {
			pdata = dev->get_platform_data(spi_info);
			break;
		}
		dev++;
	}
	spi_info->platform_data = pdata;
	if (dev->delay)
		intel_scu_spi_device_register(spi_info);
	else
		spi_register_board_info(spi_info, 1);
}

static void __init sfi_handle_i2c_dev(int bus, struct i2c_board_info *i2c_info)
{
	const struct devs_id *dev = device_ids;
	const struct intel_v4l2_subdev_id *vdev = v4l2_ids;
	void *pdata = NULL;

	while (dev->name[0]) {
		if (dev->type == SFI_DEV_TYPE_I2C &&
			!strncmp(dev->name, i2c_info->type, SFI_NAME_LEN)) {
			pdata = dev->get_platform_data(i2c_info);
			break;
		}
		dev++;
	}
	i2c_info->platform_data = pdata;

	while (vdev->name[0]) {
		if (!strncmp(vdev->name, i2c_info->type, 16)) {
			intel_ignore_i2c_device_register(bus, i2c_info);
			return;
		}
		vdev++;
	}

	if (dev->delay)
		intel_scu_i2c_device_register(bus, i2c_info);
	else
		i2c_register_board_info(bus, i2c_info, 1);
}

static void __init sfi_handle_sd_dev(struct sd_board_info *sd_info)
{
	const struct devs_id *dev = device_ids;

	while (dev->name[0]) {
		if (dev->type == SFI_DEV_TYPE_SD &&
				!strncmp(dev->name, sd_info->name, 16)) {
			dev->get_platform_data(sd_info);
			break;
		}
		dev++;
	}
}

static void sfi_handle_hsi_dev(struct hsi_board_info *hsi_info)
{
	const struct devs_id *dev = device_ids;
	void *pdata = NULL;

	while (dev->name[0]) {
		if (dev->type == SFI_DEV_TYPE_HSI &&
			!strncmp(dev->name, hsi_info->name, HSI_NAME_LEN)) {
			pdata = dev->get_platform_data(hsi_info);
			break;
		}
		dev++;
	}
	if (pdata)
		hsi_register_board_info(pdata, 2);
}

static int __init sfi_parse_devs(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_device_table_entry *pentry;
	struct spi_board_info spi_info;
	struct i2c_board_info i2c_info;
	struct hsi_board_info hsi_info;
	struct sd_board_info sd_info;
	int num, i, bus;
	int ioapic;
	struct io_apic_irq_attr irq_attr;

	sb = (struct sfi_table_simple *)table;
	num = SFI_GET_NUM_ENTRIES(sb, struct sfi_device_table_entry);
	pentry = (struct sfi_device_table_entry *)sb->pentry;

	for (i = 0; i < num; i++, pentry++) {
		int irq = pentry->irq;

		if (irq != (u8)0xff) { /* native RTE case */
			/* these SPI2 devices are not exposed to system as PCI
			 * devices, but they have separate RTE entry in IOAPIC
			 * so we have to enable them one by one here
			 */
			ioapic = mp_find_ioapic(irq);
			irq_attr.ioapic = ioapic;
			irq_attr.ioapic_pin = irq;
			irq_attr.trigger = 1;
			irq_attr.polarity = 1;
			io_apic_set_pci_routing(NULL, irq, &irq_attr);
		} else
			irq = 0; /* No irq */

		switch (pentry->type) {
		case SFI_DEV_TYPE_IPC:
			pr_debug("info[%2d]: IPC bus, name = %16.16s, "
				"irq = 0x%2x\n", i, pentry->name, irq);
			sfi_handle_ipc_dev(pentry);
			break;
		case SFI_DEV_TYPE_SPI:
			memset(&spi_info, 0, sizeof(spi_info));
			strncpy(spi_info.modalias, pentry->name, SFI_NAME_LEN);
			spi_info.irq = irq;
			spi_info.bus_num = pentry->host_num;
			spi_info.chip_select = pentry->addr;
			spi_info.max_speed_hz = pentry->max_freq;
			pr_debug("info[%2d]: SPI bus = %d, name = %16.16s, "
				"irq = 0x%2x, max_freq = %d, cs = %d\n", i,
				spi_info.bus_num,
				spi_info.modalias,
				spi_info.irq,
				spi_info.max_speed_hz,
				spi_info.chip_select);
			sfi_handle_spi_dev(&spi_info);
			break;
		case SFI_DEV_TYPE_I2C:
			memset(&i2c_info, 0, sizeof(i2c_info));
			bus = pentry->host_num;
			strncpy(i2c_info.type, pentry->name, SFI_NAME_LEN);
			i2c_info.irq = irq;
			i2c_info.addr = pentry->addr;
			pr_debug("info[%2d]: I2C bus = %d, name = %16.16s, "
				"irq = 0x%2x, addr = 0x%x\n", i, bus,
				i2c_info.type,
				i2c_info.irq,
				i2c_info.addr);

			/* Start work around. This means to fix that firmware
			 * got the name wrong for both cameras.
			 * This should be deleted after we have the right
			 * firmware.   By Chen Meng.
			 */
			if (!strcmp(i2c_info.type, "axt524124")) {
				memset(i2c_info.type, 0, 16);
				strcpy(i2c_info.type, "mt9e013");
				pr_info("Reset firmware wrong name axt524124 to %s\n",
					i2c_info.type);
			}
			if (!strcmp(i2c_info.type, "axt530124")) {
				memset(i2c_info.type, 0, 16);
				strcpy(i2c_info.type, "mt9m114");
				pr_info("Reset firmware wrong name axt530124 to %s\n",
					i2c_info.type);
			}
			/* end workaround */

			sfi_handle_i2c_dev(bus, &i2c_info);
			break;
		case SFI_DEV_TYPE_SD:
			memset(&sd_info, 0, sizeof(sd_info));
			strncpy(sd_info.name, pentry->name, SFI_NAME_LEN);
			sd_info.bus_num = pentry->host_num;
			sd_info.board_ref_clock = pentry->max_freq;
			sd_info.addr = pentry->addr;
			pr_info("info[%2d]: SDIO bus = %d, name = %16.16s, "
				"ref_clock = %d, addr =0x%x\n", i,
				sd_info.bus_num,
				sd_info.name,
				sd_info.board_ref_clock,
				sd_info.addr);
			sfi_handle_sd_dev(&sd_info);
			break;
		case SFI_DEV_TYPE_HSI:
			memset(&hsi_info, 0, sizeof(hsi_info));
			strncpy(hsi_info.name, pentry->name, HSI_NAME_LEN);
			hsi_info.hsi_id = pentry->host_num;
			hsi_info.port = pentry->addr;
			pr_info("info[%2d]: HSI bus = %d, name = %16.16s, "
				"port = %d\n", i,
				hsi_info.hsi_id,
				hsi_info.name,
				hsi_info.port);
			sfi_handle_hsi_dev(&hsi_info);
			break;
		case SFI_DEV_TYPE_UART:
		default:
			;
		}
	}
	return 0;
}

static int __init mrst_platform_init(void)
{
	sfi_table_parse(SFI_SIG_GPIO, NULL, NULL, sfi_parse_gpio);
	sfi_table_parse(SFI_SIG_DEVS, NULL, NULL, sfi_parse_devs);
	return 0;
}
arch_initcall(mrst_platform_init);

/*
 * we will search these buttons in SFI GPIO table (by name)
 * and register them dynamically. Please add all possible
 * buttons here, we will shrink them if no GPIO found.
 */
static struct gpio_keys_button gpio_button[] = {
	{KEY_POWER,		-1, 1, "power_btn",	EV_KEY, 0, 3000},
	{KEY_PROG1,		-1, 1, "prog_btn1",	EV_KEY, 0, 20},
	{KEY_PROG2,		-1, 1, "prog_btn2",	EV_KEY, 0, 20},
	{SW_LID,		-1, 1, "lid_switch",	EV_SW,  0, 20},
	{KEY_VOLUMEUP,		-1, 1, "vol_up",	EV_KEY, 0, 20},
	{KEY_VOLUMEDOWN,	-1, 1, "vol_down",	EV_KEY, 0, 20},
	{KEY_CAMERA,		-1, 1, "camera_full",	EV_KEY, 0, 20},
	{KEY_CAMERA_FOCUS,	-1, 1, "camera_half",	EV_KEY, 0, 20},
	{SW_KEYPAD_SLIDE,	-1, 1, "MagSw1",	EV_SW,  0, 20},
	{SW_KEYPAD_SLIDE,	-1, 1, "MagSw2",	EV_SW,  0, 20},
	{KEY_HOMEPAGE,		-1, 1, "home_btn",	EV_KEY, 0, 20},
	{SW_ROTATE_LOCK,	-1, 1, "rot_lock",	EV_SW,  0, 20},
	{KEY_LEFTMETA,		-1, 1, "KEY_LEFTMETA",	EV_KEY, 0, 20},
	{SW_ROTATE_LOCK,	-1, 1, "SW_ROTATE_LOCK",EV_SW,  0, 20},
	{KEY_POWER,		-1, 1, "KEY_POWER",	EV_KEY, 0, 20},
};

static struct gpio_keys_platform_data mrst_gpio_keys = {
	.buttons	= gpio_button,
	.rep		= 1,
	.nbuttons	= -1, /* will fill it after search */
};

static struct platform_device pb_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &mrst_gpio_keys,
	},
};

/*
 * Shrink the non-existent buttons, register the gpio button
 * device if there is some
 */
static int __init pb_keys_init(void)
{
	struct gpio_keys_button *gb = gpio_button;
	int i, num, good = 0;

	num = sizeof(gpio_button) / sizeof(struct gpio_keys_button);
	for (i = 0; i < num; i++) {
		gb[i].gpio = get_gpio_by_name(gb[i].desc);
		pr_debug("info[%2d]: name = %s,"
				"gpio = %d\n", i, gb[i].desc, gb[i].gpio);
		if (gb[i].gpio == -1)
			continue;

		if (i != good)
			gb[good] = gb[i];
		good++;
	}

	if (good) {
		mrst_gpio_keys.nbuttons = good;
		return platform_device_register(&pb_device);
	}
	return 0;
}
late_initcall(pb_keys_init);

#define HSU0_CTS (13)
#define HSU0_RTS (96 + 29)
#define HSU1_RX (64)
#define HSU1_TX (65)
#define HSU1_CTS (68)
#define HSU1_RTS (66)
#define HSU1_ALT_RX (96 + 30)
#define HSU1_ALT_TX (96 + 31)
#define HSU2_RX (67)

/* on = 1: the port1 is muxed (named as port 3) for debug output
 * on = 0: the port1 is for modem fw download.
 */
void mfld_hsu_port1_switch(int on)
{
	static int first = 1;

	if (unlikely(first)) {
		gpio_request(HSU1_RX, "hsu");
		gpio_request(HSU1_TX, "hsu");
		gpio_request(HSU1_CTS, "hsu");
		gpio_request(HSU1_RTS, "hsu");
		gpio_request(HSU1_ALT_RX, "hsu");
		gpio_request(HSU1_ALT_TX, "hsu");
		first = 0;
	}
	if (on) {
		lnw_gpio_set_alt(HSU1_RX, LNW_GPIO);
		lnw_gpio_set_alt(HSU1_TX, LNW_GPIO);
		lnw_gpio_set_alt(HSU1_CTS, LNW_GPIO);
		lnw_gpio_set_alt(HSU1_RTS, LNW_GPIO);
		gpio_direction_input(HSU1_RX);
		gpio_direction_input(HSU1_TX);
		gpio_direction_input(HSU1_CTS);
		gpio_direction_input(HSU1_RTS);
		gpio_direction_input(HSU1_ALT_RX);
		gpio_direction_output(HSU1_ALT_TX, 0);
		lnw_gpio_set_alt(HSU1_ALT_RX, LNW_ALT_1);
		lnw_gpio_set_alt(HSU1_ALT_TX, LNW_ALT_1);
	} else {
		lnw_gpio_set_alt(HSU1_ALT_RX, LNW_GPIO);
		lnw_gpio_set_alt(HSU1_ALT_TX, LNW_GPIO);
		gpio_direction_input(HSU1_ALT_RX);
		gpio_direction_input(HSU1_ALT_TX);
		gpio_direction_input(HSU1_RX);
		gpio_direction_output(HSU1_TX, 0);
		gpio_direction_input(HSU1_CTS);
		gpio_direction_output(HSU1_RTS, 0);
		lnw_gpio_set_alt(HSU1_RX, LNW_ALT_1);
		lnw_gpio_set_alt(HSU1_TX, LNW_ALT_1);
		lnw_gpio_set_alt(HSU1_CTS, LNW_ALT_1);
		lnw_gpio_set_alt(HSU1_RTS, LNW_ALT_2);
	}
}
EXPORT_SYMBOL_GPL(mfld_hsu_port1_switch);

void mfld_hsu_enable_wakeup(int index, struct device *dev, irq_handler_t wakeup)
{
	int ret;

	switch (index) {
	case 0:
		lnw_gpio_set_alt(HSU0_CTS, LNW_GPIO);
		ret = request_irq(gpio_to_irq(HSU0_CTS), wakeup,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"hsu0_cts_wakeup", dev);
		if (ret)
			dev_err(dev, "hsu0: failed to register wakeup irq\n");

		/* turn off flow control */
		gpio_set_value(HSU0_RTS, 1);
		lnw_gpio_set_alt(HSU0_RTS, LNW_GPIO);
		udelay(100);
		break;
	case 1:
		lnw_gpio_set_alt(HSU1_RX, LNW_GPIO);
		udelay(100);
		ret = request_irq(gpio_to_irq(HSU1_RX), wakeup,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"hsu1_rx_wakeup", dev);
		if (ret)
			dev_err(dev, "hsu1: failed to register wakeup irq\n");
		break;
	case 2:
		lnw_gpio_set_alt(HSU2_RX, LNW_GPIO);
		udelay(100);
		ret = request_irq(gpio_to_irq(HSU2_RX), wakeup,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"hsu2_rx_wakeup", dev);
		if (ret)
			dev_err(dev, "hsu2: failed to register wakeup irq\n");
		break;
	case 3:
		lnw_gpio_set_alt(HSU1_ALT_RX, LNW_GPIO);
		udelay(100);
		ret = request_irq(gpio_to_irq(HSU1_ALT_RX), wakeup,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				"hsu1_rx_wakeup", dev);
		if (ret)
			dev_err(dev, "hsu1: failed to register wakeup irq\n");
		break;
	default:
		dev_err(dev, "hsu: unknow hsu port\n");
	}
}
EXPORT_SYMBOL_GPL(mfld_hsu_enable_wakeup);

void mfld_hsu_disable_wakeup(int index, struct device *dev)
{
	switch (index) {
	case 0:
		free_irq(gpio_to_irq(HSU0_CTS), dev);
		lnw_gpio_set_alt(HSU0_CTS, LNW_ALT_1);
		lnw_gpio_set_alt(HSU0_RTS, LNW_ALT_1);
		break;
	case 1:
		free_irq(gpio_to_irq(HSU1_RX), dev);
		lnw_gpio_set_alt(HSU1_RX, LNW_ALT_1);
		break;
	case 2:
		free_irq(gpio_to_irq(HSU2_RX), dev);
		lnw_gpio_set_alt(HSU2_RX, LNW_ALT_1);
		break;
	case 3:
		free_irq(gpio_to_irq(HSU1_ALT_RX), dev);
		lnw_gpio_set_alt(HSU1_ALT_RX, LNW_ALT_1);
		break;
	default:
		dev_err(dev, "hsu: unknow hsu port\n");
	}
}

/*
 * Mininal initial implementation of the mrst keypad hooks. We need this 
 * to merge the keypad drivers and can then tweak this for the platforms
 * that are different
 */

int mrst_keypad_enabled(void)
{
	return 1;
}
EXPORT_SYMBOL_GPL(mrst_keypad_enabled);

struct mrst_keypad_platform_data *mrst_keypad_platform_data(void)
{
	return NULL;
}
EXPORT_SYMBOL_GPL(mrst_keypad_platform_data);

#ifdef CONFIG_ANDROID_RAM_CONSOLE

struct resource ram_console_res = {
	.name  = "RAM Console",
	.flags = IORESOURCE_MEM,
};

struct platform_device ram_console_dev = {
	.name = "ram_console",
	.id = -1,
	.num_resources = 1,
	.resource = &ram_console_res,
};

static int __init ram_console_init(void)
{
	if (!mrst_identify_cpu())
		return 0;
	return platform_device_register(&ram_console_dev);
}
device_initcall(ram_console_init);

#endif

/* wl128x BT, FM connectivity chip */
static int mrst_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
       return 0;
}

static int mrst_kim_resume(struct platform_device *pdev)
{
       return 0;
}

static struct ti_st_plat_data wl128x_kim_pdata = {
	.dev_name = "/dev/ttyMFD0",
	.flow_cntrl = 1,
	.baud_rate = 3500000,
	.suspend = mrst_kim_suspend,
	.resume = mrst_kim_resume,
};

static struct platform_device wl128x_kim_device = {
	.name = "kim",
	.id = -1,
	.dev.platform_data = &wl128x_kim_pdata,
};

static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device *wl128x_devices[] = {
	&wl128x_kim_device,
	&btwilink_device,
};

static int __init wl128x_init(void)
{
	wl128x_kim_pdata.nshutdown_gpio = get_gpio_by_name("BT-reset");
	if (wl128x_kim_pdata.nshutdown_gpio == -1)
		return -ENODEV;

	return platform_add_devices(wl128x_devices, ARRAY_SIZE(wl128x_devices));
}
device_initcall(wl128x_init);

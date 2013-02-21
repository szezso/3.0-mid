/*
 * intel_mdf_battery.c - Intel Medfield MSIC Internal charger and Battery Driver
 *
 * Copyright (C) 2010 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ananth Krishna <ananth.krishna.r@intel.com>,
 *         Anantha Narayanan <anantha.narayanan@intel.com>
 *         Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>

#include <asm/intel_scu_ipc.h>
#include <linux/usb/penwell_otg.h>

#define DRIVER_NAME "intel_mdf_battery"

/*********************************************************************
 *		Generic defines
 *********************************************************************/

#define MSIC_BATT_PRESENT		1
#define MSIC_BATT_NOT_PRESENT		0
#define MSIC_USB_CHARGER_PRESENT	1
#define MSIC_USB_CHARGER_NOT_PRESENT	0

/* Interrupt registers*/
#define MSIC_BATT_CHR_PWRSRCINT_ADDR	0x005
#define MSIC_BATT_CHR_BATTDET_MASK	(1 << 0)
#define MSIC_BATT_CHR_USBDET_MASK	(1 << 1)
#define MSIC_BATT_CHR_ADPLVDET_MASK	(1 << 3)
#define MSIC_BATT_CHR_ADPHVDET_MASK	(1 << 4)

#define MSIC_BATT_PWRSRC_MASK		0x1A

#define MSIC_BATT_CHR_PWRSRCINT1_ADDR	0x006
#define MSIC_BATT_CHR_USBDCDET_MASK	(1 << 2)
#define MSIC_BATT_CHR_USBCHPDET_MASK	(1 << 6)

#define MSIC_BATT_CHR_CHRINT_ADDR	0x007
#define MSIC_BATT_CHR_BATTOCP_MASK	(1 << 1)
#define MSIC_BATT_CHR_BATTOTP_MASK	(1 << 2)
#define MSIC_BATT_CHR_LOWBATT_MASK	(1 << 3)
#define MSIC_BATT_CHR_WDTIMEEXP_MASK	(1 << 5)
#define MSIC_BATT_CHR_ADPOVP_MASK	(1 << 6)
#define MSIC_BATT_CHR_TIMEEXP_MASK	(1 << 7)

#define MSIC_BATT_CHRINT_EXCP_MASK	0x5E
#define MSIC_BATT_CHR_CHRINT1_ADDR	0x008
#define MSIC_BATT_CHR_WKVINDET_MASK	(1 << 2)
#define MSIC_BATT_CHR_VINREGMINT_MASK	(1 << 4)
#define MSIC_BATT_CHR_CHROTP_MASK	(1 << 3)
#define MSIC_BATT_CHR_BATTOVP_MASK	(1 << 5)
#define MSIC_BATT_CHR_USBOVP_MASK	(1 << 6)
#define MSIC_BATT_CHR_CHRCMPLT_MASK	(1 << 7)
#define MSIC_BATT_CHRINT1_EXCP_MASK	0x68

/* Interrupt Mask registers */
#define MSIC_BATT_CHR_MPWRSRCINT_ADDR	0x014
#define MSIC_BATT_CHR_MPWRSRCINT1_ADDR	0x015
#define MSIC_BATT_CHR_MCHRINT_ADDR	0x016
#define MSIC_BATT_CHR_MCHRINT1_ADDR	0x017

/* Internal charger control registers */
#define MSIC_BATT_CHR_CHRCTRL_ADDR	0x188
#define CHRCNTL_CHRG_DISABLE		(1 << 2)

#define MSIC_BATT_CHR_CHRCVOLTAGE_ADDR	0x189
/* Set Charger Voltage to 4140 mV */
#define CHR_CHRVOLTAGE_SET_DEF		4140

#define MSIC_BATT_CHR_CHRCCURRENT_ADDR	0x18A

#define MSIC_BATT_CHR_SPCHARGER_ADDR	0x18B
#define CHR_SPCHRGER_LOWCHR_ENABLE	(1 << 5)
#define CHR_SPCHRGER_WEAKVIN		0x04

#define MSIC_BATT_CHR_CHRTTIME_ADDR	0x18C
#define CHR_CHRTIME_SET_13HRS		0x0F

#define MSIC_BATT_CHR_CHRCTRL1_ADDR	0x18D
#define MSIC_EMRG_CHRG_ENBL		(1 << 3)
/* Lower Temp thresold set to -10C */
#define MSIC_EMRG_CHRG_TEMP		(1 << 4)

/* Safe limit registers */
#define MSIC_BATT_CHR_PWRSRCLMT_ADDR	0x18E  /* Temperature limits */
#define CHR_PWRSRCLMT_SET_RANGE		0xC0

#define MSIC_BATT_CHR_CHRSTWDT_ADDR	0x18F  /* Watchdog timer */
#define CHR_WDT_DISABLE			0x0
#define CHR_WDT_SET_60SEC		0x10

#define MSIC_BATT_CHR_WDTWRITE_ADDR	0x190
#define WDTWRITE_UNLOCK_VALUE		0x01

#define MSIC_BATT_CHR_CHRSAFELMT_ADDR	0x191  /* Maximum safe charging
						  voltage and current */

/* Status registers */
#define MSIC_BATT_CHR_SPWRSRCINT_ADDR	0x192
#define MSIC_BATT_CHR_SPWRSRCINT1_ADDR	0x193
#define MSIC_BATT_CHR_USBSLOWBATT_MASK	(1 << 0)

/* ADC1 - registers */
#define ADC_CHNL_START_ADDR	0x1C5	/* increments by 1 */
#define ADC_DATA_START_ADDR     0x1D4   /* increments by 2 */

#define  MSIC_ADC1CNTL1_ADDR	0x1C0
#define  MSIC_CNTL1_ADC_ENBL	0x10
#define  MSIC_CNTL1_RR_ENBL	0x08

#define  MSIC_ADC1CNTL2_ADDR		0x1C1
#define  MSIC_ADC1CNTL3_ADDR		0x1C2
#define  MSIC_CNTL3_ADCTHERM_ENBL	0x04
#define  MSIC_CNTL3_ADCRRDATA_ENBL	0x05
#define  MSIC_CHANL_MASK_VAL		0x0F

#define  MSIC_STOPBIT_MASK	0x10
#define  MSIC_ADCTHERM_MASK	4
#define  ADC_CHANLS_MAX		15	/* num of adc channels */
#define MSIC_BATT_SENSORS	4	/* 3 for battery pack and one USB */
#define  ADC_LOOP_MAX		(ADC_CHANLS_MAX - MSIC_BATT_SENSORS)

/* ADC Channel Numbers */
#define MSIC_BATT_PACK_VOL	0x0
#define MSIC_BATT_PACK_CUR	0x1
#define MSIC_BATT_PACK_TEMP	0x7
#define MSIC_USB_VOLTAGE	0x5
#define MSIC_ADC_VOL_IDX	0
#define MSIC_ADC_CUR_IDX	1
#define MSIC_ADC_TEMP_IDX	2
#define MSIC_ADC_USB_VOL_IDX	3


#define MSIC_VAUDA		0x0DB
#define MSIC_VAUDA_VAL		0xFF

/*MSIC battery temperature  attributes*/
#define MSIC_BTP_ADC_MIN	107
#define MSIC_BTP_ADC_MAX	977


/*convert adc_val to voltage mV */
#define MSIC_MAX_VOL_DEV		((5 * 4692) / 1000)
#define MSIC_ADC_TO_VOL(adc_val)	((4692 * (adc_val)) / 1000)

/*convert adc_val to current mA */
#define MSIC_ADC_MAX_CUR		4000	/* In milli Amph */
#define MSIC_ADC_TO_CUR(adc_val)	((78125 * (adc_val)) / 10000)


/* Convert ADC value to VBUS voltage */
#define MSIC_ADC_TO_VBUS_VOL(adc_val)	((6843 * (adc_val)) / 1000)

/* ADC2 - Coulomb Counter registers */
#define MSIC_BATT_ADC_CCADCHA_ADDR	0x205
#define MSIC_BATT_ADC_CCADCLA_ADDR	0x206

#define MSIC_BATT_ADC_CHRGNG_MASK	(1 << 31)
#define MSIC_BATT_ADC_ACCCHRGVAL_MASK   0x7FFFFFFF

#define CHR_STATUS_FLULT_REG	0x37D
#define CHR_STATUS_TMR_RST	(1 << 7)
#define CHR_STATUS_VOTG_ENBL	(1 << 7)
#define CHR_STATUS_STAT_ENBL	(1 << 6)

#define CHR_STATUS_BIT_MASK	0x30
#define CHR_STATUS_BIT_READY	0x0
#define CHR_STATUS_BIT_PROGRESS	0x1
#define CHR_STATUS_BIT_CYCLE	0x2
#define CHR_STATUS_BIT_FAULT	0x3

#define CHR_FAULT_BIT_MASK	0x7
#define CHR_FAULT_BIT_NORMAL	0x0
#define CHR_FAULT_BIT_VBUS_OVP	0x1
#define CHR_FAULT_BIT_SLEEP	0x2
#define CHR_FAULT_BIT_LOW_VBUS	0x3
#define CHR_FAULT_BIT_BATT_OVP	0x4
#define CHR_FAULT_BIT_THRM	0x5
#define CHR_FAULT_BIT_TIMER	0x6
#define CHR_FAULT_BIT_NO_BATT	0x7

/*
 * Convert the voltage form decimal to
 * Register writable format
 */
#define CONV_VOL_DEC_MSICREG(a)	(((a - 3500) / 20) << 2)

#define BATT_LOWBATT_CUTOFF_VOLT	3800	/* 3800 mV */
#define BATT_DEAD_CUTOFF_VOLT		3400	/* 3400 mV */
#define BATT_ADC_VOLT_ERROR		40	/* 40 mV */
#define CHARGE_FULL_IN_MAH		1500	/* 1500 mAh */
#define MSIC_CHRG_RBATT_VAL		180	/* 180 mOhms */
#define COLMB_TO_MAHRS_CONV_FCTR	3600
#define IDLE_STATE_CUR_LMT		-500	/* -500mA */

#define MSIC_BATT_TEMP_MAX		60000	/* 60000 milli degrees */
#define MSIC_BATT_TEMP_MIN		0
#define MSIC_TEMP_HYST_ERR		4000	/* 4000 milli degrees */

/* internal return values */
#define BIT_SET		1
#define BIT_RESET	0

/* IPC defines */
#define IPCMSG_BATTERY		0xEF
#define IPCCMD_CC_WRITE		0x00
#define IPCCMD_CC_READ		0x01

#define TEMP_CHARGE_DELAY_JIFFIES	(HZ * 30)	/*30 sec */
#define CHARGE_STATUS_DELAY_JIFFIES	(HZ * 60)	/*60 sec */

#define IRQ_FIFO_MAX		16
#define THERM_CURVE_MAX_SAMPLES 7
#define THERM_CURVE_MAX_VALUES	4
#define BATT_STRING_MAX		8
#define HYSTR_SAMPLE_MAX	4

#define DISCHRG_CURVE_MAX_SAMPLES 17
#define DISCHRG_CURVE_MAX_COLOMNS 2


/*
 * This array represents the Discharge curve of the battery
 * Colomn 0 represnts Voltage in mV and colomn 1 represent
 * charge in mColoumbs.
 */
static uint32_t const dischargeCurve[DISCHRG_CURVE_MAX_SAMPLES]
				[DISCHRG_CURVE_MAX_COLOMNS] = {
	/* in mV , in mC */
	{4200, 56603000},
	{4150, 55754000},
	{4100, 54905000},
	{4050, 52385000},
	{4000, 49811000},
	{3950, 46697000},
	{3900, 43584000},
	{3850, 39339000},
	{3800, 35094000},
	{3750, 27452000},
	{3700, 19811000},
	{3650, 12735000},
	{3600, 5660000},
	{3550, 3963000},
	{3500, 2264000},
	{3450, 1132000},
	{3400, 0}
};


/* Valid msic exceptional events */
enum msic_event {
	MSIC_EVENT_BATTOCP_EXCPT,
	MSIC_EVENT_BATTOTP_EXCPT,
	MSIC_EVENT_LOWBATT_EXCPT,
	MSIC_EVENT_BATTOVP_EXCPT,
	MSIC_EVENT_ADPOVP_EXCPT,
	MSIC_EVENT_CHROTP_EXCPT,
	MSIC_EVENT_USBOVP_EXCPT,
	MSIC_EVENT_USB_VINREG_EXCPT,
	MSIC_EVENT_WEAKVIN_EXCPT,
	MSIC_EVENT_TIMEEXP_EXCPT,
};

/* Valid Charging modes */
enum {
	BATT_CHARGING_MODE_NONE = 0,
	BATT_CHARGING_MODE_NORMAL,
	BATT_CHARGING_MODE_MAINTAINENCE,
};


static void *otg_handle;
static struct device *msic_dev;

/* Variables for counting charge cycles */
static unsigned int charge_cycle_ctr;
static unsigned int chr_clmb_cnt;


/*
 * This array represents the Battery Pack thermistor
 * temarature and corresponding ADC value limits
 */
static int const therm_curve_data[THERM_CURVE_MAX_SAMPLES]
						[THERM_CURVE_MAX_VALUES] = {
	/* {temp_max, temp_min, adc_max, adc_min} */
	{-10, -20, 977, 941},
	{0, -10, 941, 887},
	{10, 0, 887, 769},
	{50, 10, 769, 357},
	{75, 50, 357, 186},
	{100, 75, 186, 107},
};


/*********************************************************************
 *		SFI table entries Structures
 *********************************************************************/

/* Battery Identifier */
struct battery_id {
	unsigned char manufac[3];
	unsigned char model[5];
	unsigned char sub_ver[3];
};

/* Parameters defining the range */
struct temperature_monitoring_range {
	unsigned char range_number;
	char temp_low_lim;
	char temp_up_lim;
	short int full_chrg_cur;
	short int full_chrg_vol;
	short int maint_chrg_cur;
	short int maint_chrg_vol_ll;
	short int maint_chrg_vol_ul;
};

/* SFI table entries structure. This code
 * will be modified or removed when the
 * Firmware supports SFI entries for Battery
 */
struct msic_batt_sfi_prop {
	unsigned char sign[5];
	unsigned int length;
	unsigned char revision;
	unsigned char checksum;
	unsigned char oem_id[7];
	unsigned char oem_tid[9];
	struct battery_id batt_id;
	unsigned short int voltage_max;
	unsigned int capacity;
	unsigned char battery_type;
	char safe_temp_low_lim;
	char safe_temp_up_lim;
	unsigned short int safe_vol_low_lim;
	unsigned short int safe_vol_up_lim;
	unsigned short int chrg_cur_lim;
	char chrg_term_lim;
	unsigned short int term_cur;
	char temp_mon_ranges;
	struct temperature_monitoring_range temp_mon_range[4];
	unsigned int sram_addr;
};

static struct msic_batt_sfi_prop *sfi_table;

/*********************************************************************
 *		Battery properties
 *********************************************************************/
struct charge_params {
	short int cvol;
	short int ccur;
	short int vinilmt;
	enum usb_charger_type chrg_type;
};

struct msic_batt_props {
	unsigned int status;
	unsigned int health;
	unsigned int present;
	unsigned int technology;
	unsigned int vol_max_des;
	unsigned int vol_now;
	unsigned int cur_now;
	unsigned int charge_full_des;	/* in mAh */
	unsigned int charge_full;	/* in mAh */
	unsigned int charge_now;	/* in mAh */
	unsigned int charge_ctr;	/* Coloumb counter raw value */
	unsigned int charge_avg;	/* moving avg of charge now */
	unsigned int energy_full;	/* in mWh */
	unsigned int energy_now;	/* in mWh */
	unsigned int capacity_level;	/* enumerated values */
	unsigned int capacity;		/* in units persentage */
	unsigned int temperature;	/* in milli Centigrade*/
	char model[BATT_STRING_MAX];
	char vender[BATT_STRING_MAX];
};

struct msic_charg_props {
	unsigned int charger_present;
	unsigned int charger_health;
	unsigned int vbus_vol;
	char charger_model[BATT_STRING_MAX];
	char charger_vender[BATT_STRING_MAX];
};

/*
 * All interrupt request are queued from interrupt
 * handler and processed in the bottom half
 */
static DEFINE_KFIFO(irq_fifo, u32, IRQ_FIFO_MAX);

/*
 * msic battery info
 */
struct msic_power_module_info {

	struct platform_device *pdev;

	/* msic charger data */
	/* lock to protect usb charger properties
	 * locking is applied whereever read or write
	 * operation is being performed to the msic usb
	 * charger property structure.
	 */
	struct mutex usb_chrg_lock;
	struct msic_charg_props usb_chrg_props;
	struct power_supply usb;

	/* msic battery data */
	/* lock to protect battery  properties
	 * locking is applied whereever read or write
	 * operation is being performed to the msic battery
	 * property structure.
	 */
	struct mutex batt_lock;
	struct msic_batt_props batt_props;
	struct power_supply batt;

	uint16_t adc_index;		/* ADC Channel Index */
	int irq;				/* GPE_ID or IRQ# */

	struct delayed_work connect_handler;
	struct delayed_work disconn_handler;
	struct charge_params ch_params;	/* holds the charge parameters */

	unsigned long update_time;		/* jiffies when data read */

	void __iomem *msic_regs_iomap;

	/* spin lock to protect driver event related variables
	 * these event variables are being modified from
	 * interrupt context(msic charger callback) also.
	 */
	spinlock_t event_lock;
	int batt_event;
	int charging_mode;
	int emrg_chrg_enbl;	/* Emergency call charge enable */
	int usr_chrg_enbl;	/* User Charge Enable or Disable */
	int refresh_charger;	/* Refresh charger parameters */

	/* Worker to monitor status and faluts */
	struct delayed_work chr_status_monitor;

	/* Worker to handle otg callback events */
	struct delayed_work chrg_callback_dwrk;

	/* lock to avoid concurrent  access to HW Registers.
	 * As some chargeer control and parameter registers
	 * can be read or write at same time, ipc_rw_lock lock
	 * is used to syncronize those IPC read or write calls.
	 */
	struct mutex ipc_rw_lock;
};


static ssize_t set_emrg_chrg(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t get_emrg_chrg(struct device *device,
			struct device_attribute *attr, char *buf);
/* Sysfs Entry for enable or disable Emergency Charging */
static DEVICE_ATTR(emrg_charge_enable, S_IWUGO | S_IRUGO,
				get_emrg_chrg, set_emrg_chrg);


/* Sysfs Entry for enable or disable Charging from user space */
static ssize_t set_chrg_enable(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t get_chrg_enable(struct device *device,
			struct device_attribute *attr, char *buf);
static DEVICE_ATTR(charge_enable, S_IWUGO | S_IRUGO,
				get_chrg_enable, set_chrg_enable);


/*
 * msic usb properties
 */
static enum power_supply_property msic_usb_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

/*
 * msic battery properties
 */
static enum power_supply_property msic_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_AVG,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};


static int enable_adc(struct msic_power_module_info *mbi)
{
	int ret;
	uint8_t data;
	/* enabling the VAUDA line
	 * this is a temporary workaround for MSIC issue
	 */
	ret = intel_scu_ipc_iowrite8(MSIC_VAUDA, MSIC_VAUDA_VAL);
	if (ret) {
		dev_warn(&mbi->pdev->dev,
			"%s:VAUDA:ipc write failed\n", __func__);
		return ret;
	}

	ret = intel_scu_ipc_ioread8(MSIC_ADC1CNTL1_ADDR, &data);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "%s:ipc read failed\n", __func__);
		return ret;
	}

	data |= MSIC_CNTL1_ADC_ENBL;	/*enable ADC */
	data |= MSIC_CNTL1_RR_ENBL;	/*Start Convertion */
	ret = intel_scu_ipc_iowrite8(MSIC_ADC1CNTL1_ADDR, data);
	if (ret)
		dev_warn(&mbi->pdev->dev,
			"%s:ADC CNTRL1 enabling failed\n", __func__);
	return ret;
}

/**
 * set_up_batt_pack_chnl - to set thermal for conversion
 * @base_addr: index of free msic adc channel
 * @therm: struct thermal module info
 * Context: can sleep
 *
 * To set up the adc for reading thermistor
 * and converting the same into actual temp value
 * on the platform
 */
static int set_up_batt_pack_chnl(u16 ch_index,
				 struct msic_power_module_info *mbi)
{
	int ret;
	u16 base_addr;

	base_addr = ADC_CHNL_START_ADDR + ch_index;

	/* enabling the MSIC_BATT_PACK_VOL channel */
	ret = intel_scu_ipc_iowrite8(base_addr, MSIC_BATT_PACK_VOL);
	if (ret) {
		dev_warn(&mbi->pdev->dev,
			"%s:enabling skin therm sensor0 failed\n", __func__);
		goto fail;
	}

	/* enabling the MSIC_BATT_PACK_CUR channel */
	base_addr++;
	ret = intel_scu_ipc_iowrite8(base_addr,	MSIC_BATT_PACK_CUR);
	if (ret) {
		dev_warn(&mbi->pdev->dev,
			"%s:enabling skin therm sensor1 failed\n", __func__);
		goto fail;
	}

	/* enabling the MSIC_BATT_PACK_TEMP channel */
	base_addr++;
	ret = intel_scu_ipc_iowrite8(base_addr,	MSIC_BATT_PACK_TEMP);
	if (ret) {
		dev_warn(&mbi->pdev->dev,
			"%s:enabling sys therm sensor failed\n", __func__);
		goto fail;
	}
	/*
	* enabling the MSIC USB VBUS channel
	* emabling stop bit for the last channel
	*/
	base_addr++;
	ret = intel_scu_ipc_iowrite8(base_addr,
				MSIC_USB_VOLTAGE | MSIC_STOPBIT_MASK);
	if (ret) {
		dev_warn(&mbi->pdev->dev,
			"%s:enabling sys therm sensor failed\n", __func__);
		goto fail;
	}
fail:
	enable_adc(mbi);
	return ret;
}

static void free_adc_channels(u16 ch_index, struct msic_power_module_info *mbi)
{
	int ret, i;
	u16 base_addr;

	base_addr = ADC_CHNL_START_ADDR + ch_index;
	for (i = 0; i < 4 && base_addr < ADC_CHANLS_MAX; i++) {
		ret = intel_scu_ipc_iowrite8(base_addr, 0x0);
		if (ret)
			dev_warn(&mbi->pdev->dev, "%s:ipc write failed\n",
								__func__);
		base_addr++;
	}
}

/*
 * reset_stopbit - sets the stop bit to 0 on the given channel
 * @addr: address of the channel
 */
static int reset_stopbit(uint16_t addr)
{
	int ret;
	uint8_t data;
	ret = intel_scu_ipc_ioread8(addr, &data);
	if (ret)
		return ret;
	data &= ~MSIC_STOPBIT_MASK;	/* setting the stop bit to zero */
	ret = intel_scu_ipc_iowrite8(addr, data);
	return ret;
}


/*
 * find_free_channel - finds an empty channel for conversion
 * @mbi: struct msic power module info
 * Context: can sleep
 *
 * If adc is not enabled then start using 0th channel
 * itself. Otherwise find an empty channel by looking for
 * one in which the stopbit is set to 1.
 * returns the base address if succeeds,-EINVAL otherwise
 */
static int find_free_channel(struct msic_power_module_info *mbi)
{
	int ret;
	int i;
	uint8_t data;

	/* Looping for empty channel */
	for (i = 0; i < ADC_CHANLS_MAX; i++) {
		ret = intel_scu_ipc_ioread8(ADC_CHNL_START_ADDR + i, &data);
		if (ret) {
			dev_warn(&mbi->pdev->dev, "%s:ipc read failed\n",
								__func__);
			return ret;
		}
		if (data & MSIC_STOPBIT_MASK)
			break;
	}

	if (i >= ADC_CHANLS_MAX-1) {
		/* No STOP bit found, Return channel number as zero */
		return 0;
	}

	/* Free Channels should be more than 3(VOL,CUR,TEMP) */
	if (i > ADC_LOOP_MAX) {
		dev_warn(&mbi->pdev->dev,
			"%s:Cannot set up adc, no channels free : %d\n",
							 __func__, i);
		return -EINVAL;
	}

	/*
	*  Reset STOP bit of the current channel
	*  No need to reset if the current channel index is 12
	*/
	if (i != ADC_LOOP_MAX) {
		ret = reset_stopbit(ADC_CHNL_START_ADDR + i);
		if (ret) {
			dev_err(&mbi->pdev->dev,
					"%s:ipc r/w failed", __func__);
			return ret;
		}
	}

	/*
	*  i points to the current channel
	*  so retrun the next free channel
	*/
	return i + 1;
}

/*
 * mid_initialize_adc - initializing the adc
 * @therm: struct thermal module info
 * Context: can sleep
 *
 * Unitialize the adc for reading thermistor
 * and converting the same into actual temp value
 * on the platform
 */
static int mdf_initialize_adc(struct msic_power_module_info *mbi)
{
	int ret;
	int channel_index = -1;

	channel_index = find_free_channel(mbi);
	if (channel_index < 0)
		return channel_index;

	/* Program the free ADC channel */
	ret = set_up_batt_pack_chnl(channel_index, mbi);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "adc initialization fauled\n");
		return ret;
	}

	return channel_index;
}

/* Check for valid Temp ADC range */
static bool is_valid_temp_adc(int adc_val)
{
	if (adc_val >= MSIC_BTP_ADC_MIN && adc_val <= MSIC_BTP_ADC_MAX)
		return true;
	else
		return false;
}

/* Temperature conversion Macros */
static int conv_adc_temp(int adc_val, int adc_max, int adc_diff, int temp_diff)
{
	int ret;

	ret = (adc_max - adc_val) * temp_diff;
	return ret/adc_diff;
}

/* Check if the adc value is in the curve sample range */
static bool is_valid_temp_adc_range(int val, int min, int max)
{
	if (val > min && val <= max)
		return true;
	else
		return false;
}

static int adc_to_temp(uint16_t adc_val)
{
	int temp;
	int i;

	if (!is_valid_temp_adc(adc_val))
		return -ERANGE;

	for (i = 0; i < THERM_CURVE_MAX_SAMPLES; i++) {
		/* linear approximation for battery pack temperature*/
		if (is_valid_temp_adc_range(adc_val, therm_curve_data[i][3],
						therm_curve_data[i][2])) {

			temp = conv_adc_temp(adc_val, therm_curve_data[i][2],
				therm_curve_data[i][2]-therm_curve_data[i][3],
				therm_curve_data[i][0]-therm_curve_data[i][1]);

			temp += therm_curve_data[i][1];
			break;
		}
	}

	if (i >= THERM_CURVE_MAX_SAMPLES)
		dev_warn(msic_dev, "Invalid temp adc range\n");

	/*convert tempertaure in celsius to milli degree celsius*/
	return temp * 1000;
}

static int mdf_read_adc_regs(int sensor,
			struct msic_power_module_info *mbi)
{
	uint16_t adc_val = 0, addr;
	uint8_t data;
	int ret;

	/*
	 * After setting or enabling the ADC control register
	 * it should not be modified untll we read the ADC value
	 * through ADC DATA regs. Using the ipc rw lock we are
	 * ensuring all ADC ipc call are performed in a proper sequence
	 */
	mutex_lock(&mbi->ipc_rw_lock);
	ret = intel_scu_ipc_ioread8(MSIC_ADC1CNTL3_ADDR, &data);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "%s:ipc write failed\n",
							__func__);
		goto ipc_failed;;
	}

	/* enable the msic for conversion before reading */
	ret = intel_scu_ipc_iowrite8(MSIC_ADC1CNTL3_ADDR,
					data | MSIC_CNTL3_ADCRRDATA_ENBL);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "%s:ipc write failed\n",
							__func__);
		goto ipc_failed;;
	}

	/* re-toggle the RRDATARD bit
	* temporary workaround */
	ret = intel_scu_ipc_iowrite8(MSIC_ADC1CNTL3_ADDR,
					data | MSIC_CNTL3_ADCTHERM_ENBL);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "%s:ipc write failed",
							__func__);
		goto ipc_failed;;
	}

	/* reading the higher bits of data */
	addr = ADC_DATA_START_ADDR+2*(mbi->adc_index + sensor);
	ret = intel_scu_ipc_ioread8(addr, &data);
	if (ret) {
		dev_warn(&mbi->pdev->dev, "%s:ipc read failed", __func__);
		goto ipc_failed;;
	}
	/* shifting bits to accomodate the lower two data bits */
	adc_val = (data << 2);
	addr++;
	ret = intel_scu_ipc_ioread8(addr, &data);/* reading lower bits */
	if (ret)
		dev_warn(&mbi->pdev->dev, "%s:ipc read failed", __func__);

ipc_failed:
	mutex_unlock(&mbi->ipc_rw_lock);
	if (ret)
		return ret;
	/*adding lower two bits to the higher bits*/
	data &= 0x3;
	adc_val += data;

	switch (sensor) {
	case MSIC_ADC_VOL_IDX:
		ret = MSIC_ADC_TO_VOL(adc_val);
		break;
	case MSIC_ADC_CUR_IDX:
		ret = MSIC_ADC_TO_CUR(adc_val & 0x1FF);
		/* if D9 bit is set battery is discharging */
		if (adc_val & 0x200)
			ret = -(MSIC_ADC_MAX_CUR - ret);
		break;
	case MSIC_ADC_TEMP_IDX:
		ret = adc_to_temp(adc_val);
		break;
	case MSIC_ADC_USB_VOL_IDX:
		ret = MSIC_ADC_TO_VBUS_VOL(adc_val);
		break;
	default:
		dev_err(&mbi->pdev->dev, "invalid adc_code:%d", adc_val);
		ret = -EINVAL;
	}
	return ret;
}

static unsigned int msic_read_coloumb_ctr(void)
{
	int err;
	uint32_t cvalue;

	/* determine other parameters */
	err = intel_scu_ipc_command(IPCMSG_BATTERY, IPCCMD_CC_READ, NULL, 0,
						&cvalue, 1);
	if (err)
		dev_warn(msic_dev, "IPC Command Failed %s\n", __func__);

	return cvalue;
}

static unsigned int cc_to_coloumbs(unsigned int cc_val)
{
	unsigned int coloumbs = 0;

	/* Every LSB of cc adc bit equal to 95.37uC
	 * Approxmating it to 95uC
	 */
	coloumbs = (cc_val & MSIC_BATT_ADC_ACCCHRGVAL_MASK) * 95;

	/* return in milli coloumbs */
	return coloumbs / 1000;
}

static unsigned int msic_get_charge_now(void)
{
	unsigned int temp_val, coloumbs;

	temp_val = msic_read_coloumb_ctr();
	coloumbs = cc_to_coloumbs(temp_val);

	/*
	 * Convert the milli Coloumbs into mAh
	 * 1 mAh = 3600 mC
	 */
	return coloumbs / COLMB_TO_MAHRS_CONV_FCTR;
}

/**
 * msic_usb_get_property - usb power source get property
 * @psy: usb power supply context
 * @psp: usb power source property
 * @val: usb power source property value
 * Context: can sleep
 *
 * MSIC usb power source property needs to be provided to power_supply
 * subsytem for it to provide the information to users.
 */
static int msic_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct msic_power_module_info *mbi = container_of(psy,
				struct msic_power_module_info, usb);

	mutex_lock(&mbi->usb_chrg_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = mbi->usb_chrg_props.charger_present;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = mbi->usb_chrg_props.charger_health;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mbi->usb_chrg_props.vbus_vol =
				mdf_read_adc_regs(MSIC_ADC_USB_VOL_IDX, mbi);
		val->intval = mbi->usb_chrg_props.vbus_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = mbi->usb_chrg_props.charger_model;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = mbi->usb_chrg_props.charger_vender;
		break;
	default:
		mutex_unlock(&mbi->usb_chrg_lock);
		return -EINVAL;
	}

	mutex_unlock(&mbi->usb_chrg_lock);
	return 0;
}

static unsigned int mdf_cal_avg(unsigned int avg)
{
	unsigned int charge_now;

	charge_now = msic_read_coloumb_ctr();

	/* Conver charge now to mAh */
	avg += cc_to_coloumbs(charge_now) / COLMB_TO_MAHRS_CONV_FCTR;

	return avg / 2;
}

static unsigned int dischrg_curve_lookup(unsigned int val, int j)
{
	int val_diff, val_total_diff, total_diff, i, k;
	unsigned int lookup_val;

	/* if j = 0 input is voltage, we need to lookup
	 * for charge else if if j =1 input is charge,
	 * we need to lookup for volatge
	 */
	k = !j;

	/* Find the index of most nearest voltage value */
	for (i = 0; i < DISCHRG_CURVE_MAX_SAMPLES; i++) {
		if (val < dischargeCurve[i][j])
			continue;
		else
			break;
	}

	if (i >= DISCHRG_CURVE_MAX_SAMPLES) {
		dev_dbg(msic_dev, "charge out of range\n");
		return 0;
	}

	if ((i == 0) || (val == dischargeCurve[i][j]))
		return dischargeCurve[i][k];

	/* Linear approximation of the discharge curve */
	val_diff = val - dischargeCurve[i][j];
	val_total_diff = dischargeCurve[i-1][j] - dischargeCurve[i][j];
	total_diff = dischargeCurve[i-1][k] - dischargeCurve[i][k];

	lookup_val  =  dischargeCurve[i][k] +
				(val_diff * val_total_diff) / total_diff;

	return lookup_val;
}

static unsigned int msic_read_energy_now(struct msic_power_module_info *mbi)
{
	unsigned int vltg, chrg;

	/* Read battery charge in mAh */
	chrg = msic_get_charge_now();

	/* get voltage form lookup table */
	vltg = dischrg_curve_lookup(chrg, 1);

	return  (vltg * chrg) / 1000;
}

static int read_avg_ibatt(struct msic_power_module_info *mbi)
{
	int i, ibatt[HYSTR_SAMPLE_MAX], ibatt_avg = 0;

	/* Measure  battey average current */
	for (i = 0; i < HYSTR_SAMPLE_MAX; i++) {
		ibatt[i] = mdf_read_adc_regs(MSIC_ADC_CUR_IDX, mbi);
		ibatt_avg += ibatt[i];
	}
	ibatt_avg = ibatt_avg / HYSTR_SAMPLE_MAX;

	return ibatt_avg;
}

static unsigned int calculate_batt_capacity(struct msic_power_module_info *mbi)
{
	int vocv, vbatt, ibatt;
	unsigned int cap_perc;

	vbatt = mdf_read_adc_regs(MSIC_ADC_VOL_IDX, mbi);
	ibatt = read_avg_ibatt(mbi);

	vocv = vbatt - (ibatt * MSIC_CHRG_RBATT_VAL) / 1000;

	/* Temporary fix for calculating capacity.
	 * will be modified once Coloumb Counter
	 * works correctly.
	 */
	vocv = vocv - BATT_DEAD_CUTOFF_VOLT;
	if (vocv < 0)
		vocv = 0;

	cap_perc = (vocv * 100) / (mbi->batt_props.vol_max_des -
						BATT_DEAD_CUTOFF_VOLT);

	/* vocv can be slighlty more than voltage
	 * max value if the ibatt is -ve and vbatt
	 * has reached a maximum value.
	 */
	if (cap_perc > 100)
		cap_perc = 100;

	return cap_perc;
}

static int msic_read_capacity_level(struct msic_power_module_info *mbi)
{
	int cap_perc, cap_level;

	/* Calculate % of charge left */
	cap_perc = calculate_batt_capacity(mbi);

	/*
	 * 0, 15, 40, 85 threshold cut-offs are temporary
	 * actuall  thresholds will be read from SMIP header
	 */
	if (cap_perc >= 0 && cap_perc < 15)
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (cap_perc >= 15 && cap_perc < 40)
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (cap_perc >= 40 && cap_perc < 85)
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (cap_perc >= 85 && cap_perc < 100)
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (cap_perc == 100)
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else
		cap_level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;

	return cap_level;
}

/**
 * msic_battery_get_property - battery power source get property
 * @psy: battery power supply context
 * @psp: battery power source property
 * @val: battery power source property value
 * Context: can sleep
 *
 * MSIC battery power source property needs to be provided to power_supply
 * subsytem for it to provide the information to users.
 */
static int msic_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct msic_power_module_info *mbi = container_of(psy,
				struct msic_power_module_info, batt);

	/*
	* All voltages, currents, charges, energies, time and temperatures
	* in uV, µA, µAh, µWh, seconds and tenths of degree Celsius un
	* less otherwise stated. It's driver's job to convert its raw values
	* to units in which this class operates.
	*/

	mutex_lock(&mbi->batt_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = mbi->batt_props.status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = mbi->batt_props.health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = mbi->batt_props.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = mbi->batt_props.technology;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = mbi->batt_props.vol_max_des * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mbi->batt_props.vol_now =
				mdf_read_adc_regs(MSIC_ADC_VOL_IDX, mbi);
		val->intval = mbi->batt_props.vol_now * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mbi->batt_props.cur_now =
				mdf_read_adc_regs(MSIC_ADC_CUR_IDX, mbi);
		val->intval = mbi->batt_props.cur_now * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		mbi->batt_props.charge_now = msic_get_charge_now();
		val->intval = mbi->batt_props.charge_now * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		mbi->batt_props.charge_ctr = msic_read_coloumb_ctr();
		val->intval =  mbi->batt_props.charge_ctr;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = mbi->batt_props.charge_full_des * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = mbi->batt_props.charge_full * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_AVG:
		mbi->batt_props.charge_avg =
				mdf_cal_avg(mbi->batt_props.charge_avg);
		val->intval = mbi->batt_props.charge_avg * 1000;
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = mbi->batt_props.energy_full * 1000;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		mbi->batt_props.energy_now = msic_read_energy_now(mbi);
		val->intval = mbi->batt_props.energy_now * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		mbi->batt_props.capacity_level = msic_read_capacity_level(mbi);
		val->intval = mbi->batt_props.capacity_level;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mbi->batt_props.capacity = calculate_batt_capacity(mbi);
		val->intval = mbi->batt_props.capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		mbi->batt_props.temperature =
				mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, mbi);
		val->intval = mbi->batt_props.temperature * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = mbi->batt_props.model;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = mbi->batt_props.vender;
		break;
	default:
		mutex_unlock(&mbi->batt_lock);
		return -EINVAL;
	}

	mutex_unlock(&mbi->batt_lock);

	return 0;
}

static ssize_t set_emrg_chrg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev  = container_of(dev,
				struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned long value;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	spin_lock(&mbi->event_lock);
	if (value)
		mbi->emrg_chrg_enbl = true;
	else
		mbi->emrg_chrg_enbl = false;
	mbi->refresh_charger = 1;
	spin_unlock(&mbi->event_lock);

	/* As the emrgency charging state is changed
	 * execute the worker as early as possible
	 * if the worker is pending on timer.
	 */
	flush_delayed_work(&mbi->connect_handler);
	return count;
}
static ssize_t get_emrg_chrg(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev  = container_of(dev,
				struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned int val;

	spin_lock(&mbi->event_lock);
	val =  mbi->emrg_chrg_enbl;
	spin_unlock(&mbi->event_lock);

	return sprintf(buf, "%d\n", val);
}


/**
 * msic_log_exception_event - log battery events
 * @event: msic event to be logged
 * Context: can sleep
 *
 * There are multiple battery and internal charger events
 * which may be of interest to users.
 * this battery function logs the different events onto the
 * kernel log messages.
 */
static void msic_log_exception_event(enum msic_event event)
{
	switch (event) {
	case MSIC_EVENT_BATTOCP_EXCPT:
		dev_warn(msic_dev,
			"over battery charge current condition detected\n");
		break;
	case MSIC_EVENT_BATTOTP_EXCPT:
		dev_warn(msic_dev,
			"high battery temperature condition detected\n");
		break;
	case MSIC_EVENT_LOWBATT_EXCPT:
		dev_warn(msic_dev,
			"Low battery voltage condition detected\n");
		break;
	case MSIC_EVENT_BATTOVP_EXCPT:
		dev_warn(msic_dev,
			"battery overvoltage condition detected\n");
		break;
	case MSIC_EVENT_CHROTP_EXCPT:
		dev_warn(msic_dev,
			"charger high temperature condition detected\n");
		break;
	case MSIC_EVENT_USBOVP_EXCPT:
		dev_warn(msic_dev, "USB over voltage condition detected\n");
		break;
	case MSIC_EVENT_USB_VINREG_EXCPT:
		dev_warn(msic_dev, "USB Input voltage regulation "
						"condition detected\n");
		break;
	case MSIC_EVENT_WEAKVIN_EXCPT:
		dev_warn(msic_dev, "USB Weak VBUS volatge "
						"condition detected\n");
		break;
	case MSIC_EVENT_TIMEEXP_EXCPT:
		dev_warn(msic_dev, "Charger Total Time Expiration "
						"condition detected\n");
		break;
	default:
		dev_warn(msic_dev, "unknown error %u detected\n", event);
		break;
	}
}

/**
 * msic_handle_exception - handle any exception scenario
 * @mbi: device info structure to update the information
 * Context: can sleep
 *
 */

static void msic_handle_exception(struct msic_power_module_info *mbi,
		uint8_t CHRINT_reg_value, uint8_t CHRINT1_reg_value)
{
	enum msic_event exception;

	/* Battery Events */
	mutex_lock(&mbi->batt_lock);
	if (CHRINT_reg_value & MSIC_BATT_CHR_BATTOCP_MASK) {
		exception = MSIC_EVENT_BATTOCP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT_reg_value & MSIC_BATT_CHR_BATTOTP_MASK) {
		mbi->batt_props.health = POWER_SUPPLY_HEALTH_OVERHEAT;
		exception = MSIC_EVENT_BATTOTP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT_reg_value & MSIC_BATT_CHR_LOWBATT_MASK) {
		mbi->batt_props.health = POWER_SUPPLY_HEALTH_DEAD;
		exception = MSIC_EVENT_LOWBATT_EXCPT;
		msic_log_exception_event(exception);
	}
	if (CHRINT_reg_value & MSIC_BATT_CHR_TIMEEXP_MASK) {
		exception = MSIC_EVENT_TIMEEXP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT1_reg_value & MSIC_BATT_CHR_BATTOVP_MASK) {
		mbi->batt_props.health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		exception = MSIC_EVENT_BATTOVP_EXCPT;
		msic_log_exception_event(exception);
	}
	mutex_unlock(&mbi->batt_lock);

	/* Charger Events */
	mutex_lock(&mbi->usb_chrg_lock);
	if (CHRINT1_reg_value & MSIC_BATT_CHR_CHROTP_MASK) {
		exception = MSIC_EVENT_CHROTP_EXCPT;
		msic_log_exception_event(exception);
	}

	if (CHRINT1_reg_value & MSIC_BATT_CHR_USBOVP_MASK) {
		mbi->usb_chrg_props.charger_health =
						POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		exception = MSIC_EVENT_USBOVP_EXCPT;
		msic_log_exception_event(exception);
	}
	if (CHRINT1_reg_value & MSIC_BATT_CHR_WKVINDET_MASK) {
		exception = MSIC_EVENT_WEAKVIN_EXCPT;
		msic_log_exception_event(exception);
	}
	if (CHRINT1_reg_value & MSIC_BATT_CHR_VINREGMINT_MASK) {
		mbi->usb_chrg_props.charger_health =
						POWER_SUPPLY_HEALTH_DEAD;
		exception = MSIC_EVENT_USB_VINREG_EXCPT;
		msic_log_exception_event(exception);
	}
	mutex_unlock(&mbi->usb_chrg_lock);

	if (CHRINT1_reg_value || (CHRINT_reg_value &
				~(MSIC_BATT_CHR_LOWBATT_MASK))) {
		mutex_lock(&mbi->batt_lock);
		if ((mbi->batt_props.status == POWER_SUPPLY_STATUS_CHARGING) ||
			(mbi->batt_props.status == POWER_SUPPLY_STATUS_FULL))
			mbi->batt_props.status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
		mutex_unlock(&mbi->batt_lock);
	}
}

/**
 *	msic_write_multi	-	multi-write helper
 *	@mbi: msic power module
 *	@address: addresses of IPC writes
 *	@data: data for IPC writes
 *	@n: size of write table
 *
 *	Write a series of values to the SCU while respecting the ipc_rw_lock
 *	across the entire sequence. Handle any error reporting and pass back
 *	error codes on failure
 */
static int msic_write_multi(struct msic_power_module_info *mbi,
				const u16 *address, const u8 *data, int n)
{
	int retval = 0, i;
	mutex_lock(&mbi->ipc_rw_lock);
	for (i = 0; i < n; i++) {
		retval = intel_scu_ipc_iowrite8(*address++, *data++);
		if (retval) {
			dev_warn(&mbi->pdev->dev, "%s:ipc msic write failed\n",
								__func__);
			break;
		}
	}
	mutex_unlock(&mbi->ipc_rw_lock);
	return retval;
}

static int ipc_read_modify_chr_param_reg(struct msic_power_module_info *mbi,
					uint16_t addr, uint8_t val, int set)
{
	int ret = 0;
	static u16 address[2] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, 0
	};
	static u8 data[2] = {
		WDTWRITE_UNLOCK_VALUE, 0
	};

	address[1] = addr;

	/* Unlock Charge parameter registers before reading */
	ret = intel_scu_ipc_iowrite8(address[0], data[0]);
	if (ret) {
		dev_warn(msic_dev, "%s:ipc write failed\n", __func__);
		return ret;
	}

	ret = intel_scu_ipc_ioread8(address[1], &data[1]);
	if (ret) {
		dev_warn(msic_dev, "%s:ipc read failed\n", __func__);
		return ret;
	}

	if (set)
		data[1] |= val;
	else
		data[1] &= (~val);

	return msic_write_multi(mbi, address, data, 2);
}

static int msic_batt_stop_charging(struct msic_power_module_info *mbi)
{
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR,
		MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR,
		MSIC_BATT_CHR_CHRSTWDT_ADDR,
	};
	static const u8 data[] = {
		WDTWRITE_UNLOCK_VALUE, /* Unlock chrg params */
		CHRCNTL_CHRG_DISABLE, /*  Disable Charging */
		WDTWRITE_UNLOCK_VALUE, /* Unlock chrg params */
		CHR_WDT_DISABLE, /*  Disable WDT Timer */
	};

	/*
	 * Charger connect handler delayed work also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied by
	 * msic_write_multi
	 */
	return msic_write_multi(mbi, address, data, 4);
}

/**
 * msic_batt_do_charging - set battery charger
 * @mbi: device info structure
 * @chrg: charge mode to set battery charger in
 * Context: can sleep
 *
 * MsIC battery charger needs to be enabled based on the charger
 * capabilities connected to the platform.
 */
static int msic_batt_do_charging(struct msic_power_module_info *mbi,
				struct charge_params *params, int is_maint_mode)
{
	int retval;
	static u8 data[] = {
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, 0,
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_SET_60SEC
	};
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCCURRENT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCVOLTAGE_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR
	};

	data[1] = params->ccur;
	data[3] = params->cvol; /* charge voltage 4.14V */
	data[5] = params->vinilmt;

	/*
	 * Charger disconnect handler also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied
	 */
	retval = msic_write_multi(mbi, address, data, 8);
	if (retval < 0)
		return retval;

	dev_dbg(msic_dev, "Charger Enabled\n");

	mutex_lock(&mbi->batt_lock);
	if (is_maint_mode)
		mbi->batt_props.status = POWER_SUPPLY_STATUS_FULL;
	else
		mbi->batt_props.status = POWER_SUPPLY_STATUS_CHARGING;
	mbi->batt_props.health = POWER_SUPPLY_HEALTH_GOOD;
	mutex_unlock(&mbi->batt_lock);

	mutex_lock(&mbi->usb_chrg_lock);
	mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_GOOD;
	mbi->usb_chrg_props.charger_present = MSIC_USB_CHARGER_PRESENT;
	memcpy(mbi->usb_chrg_props.charger_model, "msic", sizeof("msic"));
	memcpy(mbi->usb_chrg_props.charger_vender, "Intel", sizeof("Intel"));

	if (mbi->ch_params.chrg_type == CHRG_CDP)
		mbi->usb.type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (mbi->ch_params.chrg_type == CHRG_DCP)
		mbi->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	else if (mbi->ch_params.chrg_type == CHRG_ACA)
		mbi->usb.type = POWER_SUPPLY_TYPE_USB_ACA;
	else
		mbi->usb.type = POWER_SUPPLY_TYPE_USB;
	mutex_unlock(&mbi->usb_chrg_lock);

	power_supply_changed(&mbi->batt);
	power_supply_changed(&mbi->usb);

	return 0;
}

static void reset_wdt_timer(struct msic_power_module_info *mbi)
{
	static const u16 address[2] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR
	};
	static const u8 data[2] = {
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_SET_60SEC
	};

	/*
	 * Charger disconnect handler also modifies the
	 * MSIC charger parameter registers.To avoid concurrent
	 * read writes to same set of registers  locking applied
	 */
	msic_write_multi(mbi, address, data, 2);
}

static void msic_update_disconn_status(struct msic_power_module_info *mbi,
								int event)
{
	mutex_lock(&mbi->usb_chrg_lock);
	mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	memcpy(mbi->usb_chrg_props.charger_model, "Unknown", sizeof("Unknown"));
	memcpy(mbi->usb_chrg_props.charger_vender, "Unknown",
							sizeof("Unknown"));
	if (event == USBCHRG_EVENT_SUSPEND) {
		mbi->usb_chrg_props.charger_present =
					MSIC_USB_CHARGER_PRESENT;
	} else {
		mbi->usb_chrg_props.charger_present =
					MSIC_USB_CHARGER_NOT_PRESENT;
		mbi->usb.type = POWER_SUPPLY_TYPE_USB;
	}
	mutex_unlock(&mbi->usb_chrg_lock);

	mutex_lock(&mbi->batt_lock);
	if (event == USBCHRG_EVENT_SUSPEND) {
		mbi->batt_props.status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		mbi->batt_props.status =
					POWER_SUPPLY_STATUS_DISCHARGING;
	}
	mutex_unlock(&mbi->batt_lock);

	power_supply_changed(&mbi->batt);
	power_supply_changed(&mbi->usb);
}

static int check_charge_full(struct msic_power_module_info *mbi, int vref)
{
	static int vbat_prev[HYSTR_SAMPLE_MAX];
	int vflag, vbat, vbat_diff, vocv, i;
	int ibatt_avg, is_full = false;

	/* Read battey vbatt */
	vbat = mdf_read_adc_regs(MSIC_ADC_VOL_IDX, mbi);
	/* Read battery current */
	ibatt_avg = read_avg_ibatt(mbi);

	/* Check the Vbat is consitant with in the range */
	for (i = 0; i < HYSTR_SAMPLE_MAX; i++) {
		vbat_diff = vbat - vbat_prev[i];
		if (vbat_diff >= -20 && vbat_diff <= 20)
			continue;
		else
			break;
	}

	if (i >= HYSTR_SAMPLE_MAX)
		vflag = true;
	else
		vflag = false;

	/*
	 * Rbatt value should be read from SMIP header but as a
	 * temporary fix we are hardcoding the Rbatt as 180 mOhms
	 */
	vocv = vbat - (ibatt_avg * MSIC_CHRG_RBATT_VAL)/1000;
	dev_dbg(msic_dev, "vref:%d vocv:%d  vbat:%d\n", vref, vocv, vbat);

	/*
	 * Check if Vbat is equal to max charge voltage and
	 * charge rate is less than or equal to 0.05 coloumbs
	 */
	if (vflag && (vocv >= (vref - BATT_ADC_VOLT_ERROR))) {
		dev_dbg(msic_dev, "Charge Full Detected\n");
		/* Disable Charging */
		msic_batt_stop_charging(mbi);
		is_full = true;
	}

	/* Push the array with latest value */
	for (i = HYSTR_SAMPLE_MAX - 1; i > 0; i--)
		vbat_prev[i] = vbat_prev[i-1];
	vbat_prev[0] = vbat;

	return is_full;
}

static bool is_maint_cutoff_vol(int adc_volt)
{
	static int vmaint_prev[HYSTR_SAMPLE_MAX];
	int i;
	bool flag;

	/* Check whether the voltage is droping
	 * consistantly to enable maintenence charging
	 */
	for (i = 0; i < HYSTR_SAMPLE_MAX; i++) {
		if (adc_volt < vmaint_prev[i])
			continue;
		else
			break;
	}

	if (i >= HYSTR_SAMPLE_MAX)
		flag = true;
	else
		flag = false;

	/* Push the array with latest value */
	for (i = HYSTR_SAMPLE_MAX - 1; i > 0; i--)
		vmaint_prev[i] = vmaint_prev[i-1];
	vmaint_prev[0] = adc_volt;

	return flag;
}

static void calculate_charge_cycles(struct msic_power_module_info *mbi,
								bool start_bit)
{
	static unsigned int chr_prev;
	unsigned int chr_now;

	if (start_bit)
		chr_prev = 0;

	chr_now = msic_get_charge_now();

	/* count inflow charge given by charger */
	if (!chr_prev)
		chr_clmb_cnt += chr_now - chr_prev;

	if (chr_clmb_cnt >= CHARGE_FULL_IN_MAH) {
		charge_cycle_ctr++;
		chr_clmb_cnt = 0;
		dev_dbg(msic_dev, "charge cycle count:%d\n", charge_cycle_ctr);
	}
	chr_prev = chr_now;
}

/**
* msic_batt_temp_charging - manages the charging based on temperature
* @charge_param: charging parameter
* @sfi_table: SFI table structure
*
* To manage the charging based on the
* temperature of the battery
*/
static void msic_batt_temp_charging(struct work_struct *work)
{
	int ret, i, is_maint_chrg = false, is_lowchrg_enbl;
	static int iprev = -1, is_chrg_enbl;
	short int cv = 0, cc = 0, vinlimit = 0, cvref;
	int adc_temp, adc_vol;
	struct charge_params charge_param;
	struct msic_power_module_info *mbi = container_of(work,
			struct msic_power_module_info, connect_handler.work);
	struct temperature_monitoring_range *temp_mon = NULL;

	memset(&charge_param, 0x0, sizeof(struct charge_params));
	charge_param.vinilmt = mbi->ch_params.vinilmt;
	charge_param.chrg_type = mbi->ch_params.chrg_type;

	spin_lock(&mbi->event_lock);
	if (mbi->refresh_charger) {
		/*
		 * If the the charger type is unknown or None
		 * better start the charging again and compute
		 * the properties again.
		 */
		mbi->refresh_charger = 0;
		iprev = -1;
		is_chrg_enbl = false;
	}
	spin_unlock(&mbi->event_lock);

	ret = mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, mbi);
	/* mdf_read_adc_regs returns in milli Centigrade */
	adc_temp = ret / 1000;

	for (i = 0; i < sfi_table->temp_mon_ranges; i++) {
		if ((adc_temp >=  sfi_table->temp_mon_range[i].temp_low_lim) &&
		(adc_temp < sfi_table->temp_mon_range[i].temp_up_lim)) {

			cv = sfi_table->temp_mon_range[i].full_chrg_vol;
			cc = sfi_table->temp_mon_range[i].full_chrg_cur;
			cvref = cv;

			/* D7,D6 bits of CHRCNTL will set the VINILMT */
			if (charge_param.vinilmt > 950)
				vinlimit = 0xC0; /* VINILMT set to No Limit */
			else if (charge_param.vinilmt > 500)
				vinlimit = 0x80; /* VINILMT set to 950mA */
			else if (charge_param.vinilmt > 100)
				vinlimit = 0x40; /* VINILMT set to 500mA */
			else
				vinlimit = 0x00; /* VINILMT set to 100mA */

			break;
		}
	}

	if (i >= sfi_table->temp_mon_ranges) {
		dev_warn(msic_dev, "TEMP RANGE NOT EXIST\n");
		/*
		 * If we are in middle of charge cycle is safer to Reset WDT
		 * Timer Register.Because battery temperature and Changre
		 * status register are not related.
		 */
		reset_wdt_timer(mbi);
		goto lbl_sched_work;
	}

	/*
	 * Check for Charge full condition and set the battery
	 * properties accordingly. Also check for charging mode
	 * whether it is normal or maintenence mode.
	 */
	spin_lock(&mbi->event_lock);
	if (mbi->charging_mode == BATT_CHARGING_MODE_MAINTAINENCE) {
		cvref = sfi_table->temp_mon_range[i].maint_chrg_vol_ul;
		is_maint_chrg = true;
	}
	spin_unlock(&mbi->event_lock);

	ret = check_charge_full(mbi, cvref);
	if (ret) {
		is_chrg_enbl = false;
		if (!is_maint_chrg) {
			dev_dbg(msic_dev, "Going to Maintenence CHRG Mode\n");

			spin_lock(&mbi->event_lock);
			mbi->charging_mode = BATT_CHARGING_MODE_MAINTAINENCE;
			spin_unlock(&mbi->event_lock);

			mutex_lock(&mbi->batt_lock);
			mbi->batt_props.charge_full = msic_get_charge_now();
			mbi->batt_props.status = POWER_SUPPLY_STATUS_FULL;
			mutex_unlock(&mbi->batt_lock);

			is_maint_chrg = true;
			power_supply_changed(&mbi->batt);
		}
	}

	/*
	 * If we are in same Temparature range check for check for the
	 * maintainence charging mode and enable the charging depending
	 * on the voltage.If Temparature range is changed then anyways
	 * we need to set charging parameters and enable charging.
	 */
	if (i == iprev) {
		/*
		 * Check if the voltage falls below lower threshold
		 * if we are in maintenence mode charging.
		 */
		if (is_maint_chrg && !is_chrg_enbl) {
			temp_mon = &sfi_table->temp_mon_range[i];
			/* Read battery Voltage */
			adc_vol = mdf_read_adc_regs(MSIC_ADC_VOL_IDX, mbi);

			/* Cosidering ADC error of BATT_ADC_VOLT_ERROR */
			if ((adc_vol + 20) <= temp_mon->maint_chrg_vol_ll &&
						is_maint_cutoff_vol(adc_vol)) {
				dev_dbg(msic_dev, "restart charging\n");
				cv = temp_mon->maint_chrg_vol_ul;
			} else {
				dev_dbg(msic_dev, "vbat is more than ll\n");
				goto lbl_sched_work;
			}
		} else {
			/* count charge cycles */
			calculate_charge_cycles(mbi, false);
			/* Reset WDT Timer Register for 60 Sec */
			reset_wdt_timer(mbi);
			goto lbl_sched_work;
		}
	}

	iprev = i;
	charge_param.cvol = CONV_VOL_DEC_MSICREG(cv);

	cc = cc - 550;
	if (cc <= 0)
		cc = 0;
	else
		cc = cc / 100;
	cc = cc << 3;

	charge_param.ccur = cc;
	charge_param.vinilmt = vinlimit;

	dev_dbg(msic_dev, "params  vol: %x  cur:%x vinilmt:%x\n",
		charge_param.cvol, charge_param.ccur, charge_param.vinilmt);
	/*
	 * Check if emergency charging is not enabled and temperarure is < 0
	 * limit the charging current to LOW CHARGE (325 mA) else
	 * allow the charging as set in charge current register
	 */
	spin_lock(&mbi->event_lock);
	if (!mbi->emrg_chrg_enbl && (adc_temp < 0)) {
		dev_dbg(msic_dev, "Emeregency Charging Enabled\n");
		is_lowchrg_enbl = BIT_SET;
	} else {
		dev_dbg(msic_dev, "Emeregency Charging Not Enabled\n");
		is_lowchrg_enbl = BIT_RESET;
	}
	spin_unlock(&mbi->event_lock);

	ipc_read_modify_chr_param_reg(mbi, MSIC_BATT_CHR_SPCHARGER_ADDR,
				CHR_SPCHRGER_LOWCHR_ENABLE, is_lowchrg_enbl);

	/* enable charging here */
	ret = msic_batt_do_charging(mbi, &charge_param, is_maint_chrg);
	if (ret) {
		dev_warn(msic_dev, "msic_batt_do_charging failed\n");
		goto lbl_sched_work;
	}
	/* Start cycle counting */
	calculate_charge_cycles(mbi, true);
	is_chrg_enbl = true;

lbl_sched_work:
	/* Schedule teh work after 30 Seconds */
	schedule_delayed_work(&mbi->connect_handler, TEMP_CHARGE_DELAY_JIFFIES);
}

static void msic_batt_disconn(struct work_struct *work)
{
	int ret, event;
	struct msic_power_module_info *mbi = container_of(work,
			struct msic_power_module_info, disconn_handler.work);

	ret = msic_batt_stop_charging(mbi);
	if (ret) {
		dev_dbg(msic_dev, "%s: failed\n", __func__);
		return;
	}

	spin_lock(&mbi->event_lock);
	event = mbi->batt_event;
	spin_unlock(&mbi->event_lock);

	msic_update_disconn_status(mbi, event);
}

static int msic_event_handler(void *arg, int event, struct otg_bc_cap *cap)
{
	struct msic_power_module_info *mbi =
					(struct msic_power_module_info *)arg;

	/* msic callback can be called from interrupt
	 * context, So used spin lock read or modify the
	 * msic event related variables and scheduling
	 * connection or disconnection delayed works depending
	 * on the USB event.
	 */
	spin_lock(&mbi->event_lock);
	if ((mbi->batt_event == event && event != USBCHRG_EVENT_UPDATE) ||
						(!mbi->usr_chrg_enbl)) {
		spin_unlock(&mbi->event_lock);
		return 0;
	}
	mbi->batt_event = event;
	spin_unlock(&mbi->event_lock);

	switch (event) {
	case USBCHRG_EVENT_CONNECT:
	case USBCHRG_EVENT_RESUME:
	case USBCHRG_EVENT_UPDATE:
		/*
		 * If previous event CONNECT and current is event is
		 * UPDATE, we have already queued the work.
		 * Its better to dequeue the previous work
		 * and add the new work to the queue.
		 */
		cancel_delayed_work(&mbi->connect_handler);

		if (pm_runtime_suspended(&mbi->pdev->dev))
			pm_runtime_get_sync(&mbi->pdev->dev);
		/* minimum charge current is 550 mA */
		mbi->ch_params.vinilmt = cap->mA;
		mbi->ch_params.chrg_type = cap->chrg_type;
		dev_dbg(msic_dev, "CHRG TYPE:%d %d\n",
					cap->chrg_type, cap->mA);
		spin_lock(&mbi->event_lock);
		mbi->refresh_charger = 1;
		if (mbi->charging_mode == BATT_CHARGING_MODE_NONE)
			mbi->charging_mode = BATT_CHARGING_MODE_NORMAL;
		spin_unlock(&mbi->event_lock);

		schedule_delayed_work(&mbi->connect_handler, 0);
		break;
	case USBCHRG_EVENT_DISCONN:
	case USBCHRG_EVENT_SUSPEND:
		dev_dbg(msic_dev, "USB DISCONN or SUSPEND\n");
		cancel_delayed_work(&mbi->connect_handler);
		schedule_delayed_work(&mbi->disconn_handler, 0);

		spin_lock(&mbi->event_lock);
		mbi->refresh_charger = 0;
		mbi->charging_mode = BATT_CHARGING_MODE_NONE;
		spin_unlock(&mbi->event_lock);
		if (!pm_runtime_suspended(&mbi->pdev->dev))
			pm_runtime_put_sync(&mbi->pdev->dev);
		break;
	default:
		dev_warn(msic_dev, "Invalid OTG Event:%s\n", __func__);
	}
	return 0;
}

static void msic_chrg_callback_worker(struct work_struct *work)
{
	struct otg_bc_cap cap;
	struct msic_power_module_info *mbi = container_of(work,
			struct msic_power_module_info, chrg_callback_dwrk.work);

	penwell_otg_query_charging_cap(&cap);
	msic_event_handler(mbi, cap.current_event, &cap);
}

/*
 * msic_charger_callback - callback for USB OTG
 * @arg: device info structure
 * @event: USB event
 * @cap: charging capabilities
 * Context: Interrupt Context can not sleep
 *
 * Will be called from the OTG driver.Depending on the event
 * schedules a bottem half to enbale or disable the charging.
 */
static int msic_charger_callback(void *arg, int event, struct otg_bc_cap *cap)
{
	struct msic_power_module_info *mbi =
					(struct msic_power_module_info *)arg;

	schedule_delayed_work(&mbi->chrg_callback_dwrk, 0);
	return 0;
}

/**
 * msic_status_monitor - worker function to monitor status
 * @work: delayed work handler structure
 * Context: Can sleep
 *
 * Will be called from the threaded IRQ function.
 * Monitors status of the charge register and temperature.
 */
static void msic_status_monitor(struct work_struct *work)
{
	uint8_t data;
	int retval, val, volt, temp, ibatt_avg, is_active = 0, is_fault = 0;
	int chr_mode, chr_event;
	unsigned int delay = CHARGE_STATUS_DELAY_JIFFIES;
	struct msic_power_module_info *mbi = container_of(work,
			struct msic_power_module_info, chr_status_monitor.work);

	/* If battery is not charging Ibatt value will be in -ve
	 * direction and if platfrom is idle then the current drawn
	 * by board is around 500mA
	 */
	ibatt_avg = read_avg_ibatt(mbi);
	if (ibatt_avg < IDLE_STATE_CUR_LMT)
		is_active = 1;

	retval = intel_scu_ipc_ioread8(CHR_STATUS_FLULT_REG, &data);
	if (retval) {
		dev_warn(msic_dev, "%s:ipc read failed\n", __func__);
		return ;
	}

	/* Check Fluts bits in Status Register */
	val = (data & CHR_STATUS_BIT_MASK) >> 4;
	if (val == CHR_STATUS_BIT_FAULT) {
		dev_dbg(msic_dev, "chr status regisrer:%x\n", data);
		is_fault = 1;
		goto fault_detected;
	}

	/* Compute Charger health */
	mutex_lock(&mbi->usb_chrg_lock);
	if (mbi->usb_chrg_props.charger_health != POWER_SUPPLY_HEALTH_UNKNOWN &&
		mbi->usb_chrg_props.charger_health != POWER_SUPPLY_HEALTH_GOOD)
		mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_GOOD;
	mutex_unlock(&mbi->usb_chrg_lock);

	spin_lock(&mbi->event_lock);
	chr_mode = mbi->charging_mode;
	chr_event = mbi->batt_event;
	spin_unlock(&mbi->event_lock);

	/* Compute Battery health */
	mutex_lock(&mbi->batt_lock);
	if (mbi->batt_props.health == POWER_SUPPLY_HEALTH_OVERHEAT) {
		temp = mdf_read_adc_regs(MSIC_ADC_TEMP_IDX, mbi);
		dev_dbg(msic_dev, "temp in millC :%d\n", temp);
		/*
		 * Valid temperature window is 0 to 60 Degrees
		 * and thermister has 2 drgee hysteris and considering
		 * 2 degree adc error, fault revert temperature will
		 * be 4 to 56 degrees. These vaules will be fine tuned later.
		 */
		if (temp >= (MSIC_BATT_TEMP_MAX - MSIC_TEMP_HYST_ERR) ||
			temp <= (MSIC_BATT_TEMP_MIN + MSIC_TEMP_HYST_ERR)) {
			mutex_unlock(&mbi->batt_lock);
			is_fault = 1;
			goto fault_detected;
		}
	}

	if (chr_mode == BATT_CHARGING_MODE_MAINTAINENCE)
		mbi->batt_props.status = POWER_SUPPLY_STATUS_FULL;
	else if (chr_mode == BATT_CHARGING_MODE_NORMAL)
		mbi->batt_props.status = POWER_SUPPLY_STATUS_CHARGING;
	else if (chr_event == USBCHRG_EVENT_SUSPEND)
		mbi->batt_props.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		mbi->batt_props.status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (mbi->batt_props.health == POWER_SUPPLY_HEALTH_DEAD) {
		volt = mdf_read_adc_regs(MSIC_ADC_VOL_IDX, mbi);
		dev_dbg(msic_dev, "low vol in mV :%d\n", volt);
		if (volt < BATT_LOWBATT_CUTOFF_VOLT) {
			mutex_unlock(&mbi->batt_lock);
			if (volt < BATT_DEAD_CUTOFF_VOLT)
				power_supply_changed(&mbi->batt);
			is_fault = 1;
			goto fault_detected;
		}
	}
	mbi->batt_props.health = POWER_SUPPLY_HEALTH_GOOD;
	mutex_unlock(&mbi->batt_lock);

fault_detected:
	if (is_fault && is_active)
		delay = delay / 60;	/* 1 Sec delay */
	else if (is_fault && !is_active)
		delay = delay / 12;	/* 5 Sec delay */
	else if (!is_fault && is_active)
		delay = delay / 6;	/* 10 Sec delay */

	schedule_delayed_work(&mbi->chr_status_monitor, delay);

}
/**
 * msic_battery_interrupt_handler - msic battery interrupt handler
 * Context: interrupt context
 *
 * MSIC battery interrupt handler which will be called on insertion
 * of valid power source to charge the battery or an exception
 * condition occurs.
 */
static irqreturn_t msic_battery_interrupt_handler(int id, void *dev)
{
	struct msic_power_module_info *mbi = dev;
	u32 reg_int_val;

	/* We have only one concurrent fifo reader
	 * and only one concurrent writer, so we are not
	 * using any lock to protect fifo.
	 */
	if (unlikely(kfifo_is_full(&irq_fifo))) {
		dev_warn(&mbi->pdev->dev, "KFIFO Full\n");
		return IRQ_WAKE_THREAD;
	}
	/* Copy Interrupt registers locally */
	reg_int_val = readl(mbi->msic_regs_iomap);
	/* Add the Interrupt regs to  FIFO */
	kfifo_in(&irq_fifo, &reg_int_val, sizeof(u32));

	return IRQ_WAKE_THREAD;
}

/**
 * msic_battery_thread_handler - msic battery threaded IRQ function
 * Context: can sleep
 *
 * MSIC battery needs to either update the battery status as full
 * if it detects battery full condition caused the interrupt or needs
 * to enable battery charger if it detects usb and battery detect
 * caused the source of interrupt.
 */
static irqreturn_t msic_battery_thread_handler(int id, void *dev)
{
	int ret;
	unsigned char data[2];
	struct msic_power_module_info *mbi = dev;
	u32 tmp;
	unsigned char intr_stat;

	/* We have only one concurrent fifo reader
	 * and only one concurrent writer, we are not
	 * using any lock to protect fifo.
	 */
	if (unlikely(kfifo_is_empty(&irq_fifo))) {
		dev_warn(msic_dev, "KFIFO Empty\n");
		return IRQ_NONE;
	}

	/*
	 * Get the Interrupt regs sate from FIFO (it will not fail due to above
	 * check).
	 */
	if (kfifo_out(&irq_fifo, &tmp, 1) != 1) {
		dev_warn(msic_dev, "KFIFO underflow\n");
		return IRQ_NONE;
	}

	/* CHRINT Register */
	data[0] = (tmp & 0x00ff0000) >> 16;
	/* CHRINT1 Register */
	data[1] = (tmp & 0xff000000) >> 24;

	dev_dbg(msic_dev, "PWRSRC Int %x %x\n", tmp & 0xff,
						(tmp & 0xff00) >> 8);
	dev_dbg(msic_dev, "CHR Int %x %x\n", data[0], data[1]);

	/* Check if charge complete */
	if (data[1] & MSIC_BATT_CHR_CHRCMPLT_MASK)
		dev_dbg(msic_dev, "CHRG COMPLT\n");

	if (data[1] & MSIC_BATT_CHR_WKVINDET_MASK) {
		dev_dbg(msic_dev, "CHRG WeakVIN Detected\n");

		spin_lock(&mbi->event_lock);
		tmp = mbi->charging_mode;
		spin_unlock(&mbi->event_lock);
		/* Sometimes for USB unplug we are recieving WeakVIN
		 * interrupts, so as software work-around we check the
		 * SPWRSRCINT SUSBDET bit to know the USB connection.
		 */
		ret = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR,
								&intr_stat);
		if (!(intr_stat & MSIC_BATT_CHR_USBDET_MASK) &&
				(tmp != BATT_CHARGING_MODE_NONE)) {
			data[1] &= ~MSIC_BATT_CHR_WKVINDET_MASK;
			dev_dbg(msic_dev, "force disconnect event\n");
			msic_event_handler(mbi, USBCHRG_EVENT_DISCONN, NULL);
		}
	}

	/* Check if an exception occured */
	if (data[0] || (data[1] & ~(MSIC_BATT_CHR_CHRCMPLT_MASK))) {
		msic_handle_exception(mbi, data[0], data[1]);
		power_supply_changed(&mbi->batt);
		power_supply_changed(&mbi->usb);
	}

	return IRQ_HANDLED;

}

static int check_charger_conn(struct msic_power_module_info *mbi)
{
	int retval;
	struct otg_bc_cap cap;
	unsigned char data;

	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR, &data);
	if (retval) {
		dev_warn(msic_dev, "%s:ipc msic read failed\n", __func__);
		return retval;
	}

	if (data & MSIC_BATT_CHR_USBDET_MASK) {
		retval = penwell_otg_query_charging_cap(&cap);
		if (retval) {
			dev_warn(msic_dev, "%s(): usb otg power query "
			"failed with error code %d\n", __func__, retval);
			return  retval;
		}
		/* Enable charging only if vinilmt is >= 100mA */
		if (cap.mA >= 100)
			msic_charger_callback(mbi, USBCHRG_EVENT_CONNECT, &cap);
	}

	return retval;
}

static ssize_t set_chrg_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev  = container_of(dev,
				struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned long value;
	int retval, chr_mode;

	if (strict_strtoul(buf, 10, &value))
		return -EINVAL;

	spin_lock(&mbi->event_lock);
	if (value)
		mbi->usr_chrg_enbl = true;
	else
		mbi->usr_chrg_enbl = false;
	chr_mode = mbi->charging_mode;
	spin_unlock(&mbi->event_lock);

	if (!value && (chr_mode != BATT_CHARGING_MODE_NONE)) {
		dev_dbg(msic_dev, "User App charger disable !\n");
		msic_event_handler(mbi, USBCHRG_EVENT_DISCONN, NULL);

	} else if (value && (chr_mode == BATT_CHARGING_MODE_NONE)) {
		dev_dbg(msic_dev, "User App charger enable!\n");
		retval = check_charger_conn(mbi);
		if (retval)
			dev_warn(msic_dev, "check_charger_conn failed\n");
	}

	return count;
}

static ssize_t get_chrg_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev  = container_of(dev,
				struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	unsigned int val;

	spin_lock(&mbi->event_lock);
	val =  mbi->usr_chrg_enbl;
	spin_unlock(&mbi->event_lock);

	return sprintf(buf, "%d\n", val);
}

/**
 * sfi_table_populate - Simple Firmware Interface table Populate
 * @sfi_table: Simple Firmware Interface table structure
 *
 * SFI table has entries for the temperature limits
 * which is populated in a local structure
 */
static void sfi_table_populate(struct msic_batt_sfi_prop *sfi_table)
{

	/* This is temporary code, will be removed
	 * or modified as soon as the firmware supports
	 * SFI entries for MSIC battery.
	 */
	memcpy(sfi_table->sign, "BATT", sizeof("BATT"));
	sfi_table->length = 183;
	sfi_table->revision = 1;
	sfi_table->checksum = 15;
	memcpy(sfi_table->oem_id, "INTEL", sizeof("INTEL"));
	memcpy(sfi_table->oem_tid, "OEMTID", sizeof("OEMTID"));
	memcpy(sfi_table->batt_id.manufac, "NK", sizeof("NK"));
	memcpy(sfi_table->batt_id.model, "BP4L", sizeof("BP4L"));
	memcpy(sfi_table->batt_id.sub_ver, "00", sizeof("00"));
	sfi_table->voltage_max = 4200;
	sfi_table->capacity = 1500;
	sfi_table->battery_type = 2;	/* POWER_SUPPLY_TECHNOLOGY_LION */
	sfi_table->safe_temp_low_lim = 0;
	sfi_table->safe_temp_up_lim = 60;
	sfi_table->safe_vol_low_lim = 3700;
	sfi_table->safe_vol_up_lim = 4200;
	sfi_table->chrg_cur_lim = 1000;
	sfi_table->chrg_term_lim = 1;
	sfi_table->term_cur = 50;
	sfi_table->temp_mon_ranges = 4;

	sfi_table->temp_mon_range[0].range_number = 0;
	sfi_table->temp_mon_range[0].temp_low_lim = 45;
	sfi_table->temp_mon_range[0].temp_up_lim = 60;
	sfi_table->temp_mon_range[0].full_chrg_cur = 950;
	sfi_table->temp_mon_range[0].full_chrg_vol = 4100;
	sfi_table->temp_mon_range[0].maint_chrg_cur = 950;
	sfi_table->temp_mon_range[0].maint_chrg_vol_ll = 4000;
	sfi_table->temp_mon_range[0].maint_chrg_vol_ul = 4050;
	sfi_table->temp_mon_range[1].range_number = 1;
	sfi_table->temp_mon_range[1].temp_low_lim = 10;
	sfi_table->temp_mon_range[1].temp_up_lim = 45;
	sfi_table->temp_mon_range[1].full_chrg_cur = 950;
	sfi_table->temp_mon_range[1].full_chrg_vol = 4200;
	sfi_table->temp_mon_range[1].maint_chrg_cur = 950;
	sfi_table->temp_mon_range[1].maint_chrg_vol_ll = 4100;
	sfi_table->temp_mon_range[1].maint_chrg_vol_ul = 4150;
	sfi_table->temp_mon_range[2].range_number = 2;
	sfi_table->temp_mon_range[2].temp_low_lim = 0;
	sfi_table->temp_mon_range[2].temp_up_lim = 10;
	sfi_table->temp_mon_range[2].full_chrg_cur = 950;
	sfi_table->temp_mon_range[2].full_chrg_vol = 4100;
	sfi_table->temp_mon_range[2].maint_chrg_cur = 950;
	sfi_table->temp_mon_range[2].maint_chrg_vol_ll = 4000;
	sfi_table->temp_mon_range[2].maint_chrg_vol_ul = 4050;
	sfi_table->temp_mon_range[3].range_number = 3;
	sfi_table->temp_mon_range[3].temp_low_lim = -10;
	sfi_table->temp_mon_range[3].temp_up_lim = 0;
	sfi_table->temp_mon_range[3].full_chrg_cur = 950;
	sfi_table->temp_mon_range[3].full_chrg_vol = 3900;
	sfi_table->temp_mon_range[3].maint_chrg_cur = 950;
	sfi_table->temp_mon_range[3].maint_chrg_vol_ll = 3950;
	sfi_table->temp_mon_range[3].maint_chrg_vol_ul = 3950;

	sfi_table->sram_addr = 0xFFFF7FC3;
}

/**
 * init_batt_props - initialize battery properties
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_batt_props function initializes the
 * MSIC battery properties.
 */
static void init_batt_props(struct msic_power_module_info *mbi)
{
	unsigned char data;
	int retval;

	mbi->batt_event = USBCHRG_EVENT_DISCONN;
	mbi->charging_mode = BATT_CHARGING_MODE_NONE;
	mbi->emrg_chrg_enbl = false;
	mbi->usr_chrg_enbl = true;

	mbi->batt_props.status = POWER_SUPPLY_STATUS_DISCHARGING;
	mbi->batt_props.health = POWER_SUPPLY_HEALTH_GOOD;
	mbi->batt_props.present = MSIC_BATT_NOT_PRESENT;
	mbi->batt_props.technology = sfi_table->battery_type;
	mbi->batt_props.vol_max_des =  sfi_table->voltage_max;
	mbi->batt_props.vol_now = 0x0;
	mbi->batt_props.cur_now = 0x0;
	mbi->batt_props.charge_full_des = CHARGE_FULL_IN_MAH;
	mbi->batt_props.charge_full = CHARGE_FULL_IN_MAH;
	mbi->batt_props.charge_now = 0x0;
	mbi->batt_props.charge_ctr = 0x0;
	mbi->batt_props.charge_avg = 0x0;
	mbi->batt_props.capacity = 0x0;
	mbi->batt_props.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	mbi->batt_props.temperature = 0x0;

	mbi->batt_props.energy_now = 0x0;
	mbi->batt_props.energy_full = (mbi->batt_props.vol_max_des *
					mbi->batt_props.charge_full_des) / 1000;

	memcpy(mbi->batt_props.vender, sfi_table->batt_id.manufac,
				sizeof(sfi_table->batt_id.manufac));
	memcpy(mbi->batt_props.model, sfi_table->batt_id.model,
				sizeof(sfi_table->batt_id.model));

	/* read specific to determine the status */
	retval = intel_scu_ipc_ioread8(MSIC_BATT_CHR_SPWRSRCINT_ADDR, &data);
	if (retval)
		dev_warn(&mbi->pdev->dev, "%s:ipc read failed\n", __func__);

	/* determine battery Presence */
	if (data & MSIC_BATT_CHR_BATTDET_MASK)
		mbi->batt_props.present = MSIC_BATT_PRESENT;
	else
		mbi->batt_props.present = MSIC_BATT_NOT_PRESENT;

	/* Enable Status Register */
	retval = intel_scu_ipc_iowrite8(CHR_STATUS_FLULT_REG,
				CHR_STATUS_TMR_RST | CHR_STATUS_STAT_ENBL);
	if (retval)
		dev_warn(&mbi->pdev->dev, "%s:ipc r/w failed\n", __func__);

}

/**
 * init_charger_props - initialize charger properties
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_charger_props function initializes the
 * MSIC usb charger properties.
 */
static void init_charger_props(struct msic_power_module_info *mbi)
{
	mbi->usb_chrg_props.charger_present = MSIC_USB_CHARGER_NOT_PRESENT;
	mbi->usb_chrg_props.charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	memcpy(mbi->usb_chrg_props.charger_model, "Unknown", sizeof("Unknown"));
	memcpy(mbi->usb_chrg_props.charger_vender, "Unknown",
							sizeof("Unknown"));
}

/**
 * init_msic_regs - initialize msic registers
 * @mbi: msic module device structure
 * Context: can sleep
 *
 * init_msic_regs function initializes the
 * MSIC registers like CV,Power Source LMT,etc..
 */
static int init_msic_regs(struct msic_power_module_info *mbi)
{
	static const u16 address[] = {
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_PWRSRCLMT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCVOLTAGE_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRTTIME_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_SPCHARGER_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRSTWDT_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCTRL_ADDR,
		MSIC_BATT_CHR_WDTWRITE_ADDR, MSIC_BATT_CHR_CHRCTRL1_ADDR
	};
	static u8 data[] = {
		WDTWRITE_UNLOCK_VALUE, CHR_PWRSRCLMT_SET_RANGE,
		WDTWRITE_UNLOCK_VALUE,
			CONV_VOL_DEC_MSICREG(CHR_CHRVOLTAGE_SET_DEF),
		WDTWRITE_UNLOCK_VALUE, CHR_CHRTIME_SET_13HRS,
		WDTWRITE_UNLOCK_VALUE,
			(~CHR_SPCHRGER_LOWCHR_ENABLE & CHR_SPCHRGER_WEAKVIN),
		WDTWRITE_UNLOCK_VALUE, CHR_WDT_DISABLE,
		WDTWRITE_UNLOCK_VALUE, CHRCNTL_CHRG_DISABLE,
		WDTWRITE_UNLOCK_VALUE, MSIC_EMRG_CHRG_ENBL | MSIC_EMRG_CHRG_TEMP

	};

	return msic_write_multi(mbi, address, data, 14);
}


/**
 * msic_battery_probe - msic battery initialize
 * @pdev: msic battery platform device structure
 * Context: can sleep
 *
 * MSIC battery initializes its internal data strucrue and other
 * infrastructure components for it to work as expected.
 */
static int msic_battery_probe(struct platform_device *pdev)
{
	int retval;
	struct msic_power_module_info *mbi = NULL;

	mbi = kzalloc(sizeof(struct msic_power_module_info), GFP_KERNEL);
	if (!mbi) {
		dev_err(&pdev->dev, "%s(): memory allocation failed\n",
								__func__);
		return -ENOMEM;
	}

	sfi_table = kzalloc(sizeof(struct msic_batt_sfi_prop), GFP_KERNEL);
	if (!sfi_table) {
		dev_err(&pdev->dev, "%s(): memory allocation failed\n",
								__func__);
		kfree(mbi);
		return -ENOMEM;
	}

	mbi->pdev = pdev;
	mbi->irq =  platform_get_irq(pdev, 0);
	platform_set_drvdata(pdev, mbi);
	msic_dev = &pdev->dev;

	/* initialize all required framework before enabling interrupts */

	/* OTG Disconnect is being called from IRQ context
	 * so calling ipc function is not approprite from otg callback
	 */
	INIT_DELAYED_WORK(&mbi->disconn_handler, msic_batt_disconn);
	INIT_DELAYED_WORK(&mbi->connect_handler, msic_batt_temp_charging);
	INIT_DELAYED_WORK(&mbi->chr_status_monitor, msic_status_monitor);
	INIT_DELAYED_WORK(&mbi->chrg_callback_dwrk, msic_chrg_callback_worker);

	/* Initialize the spin locks */
	spin_lock_init(&mbi->event_lock);

	/* Initialize mutex locks */
	mutex_init(&mbi->usb_chrg_lock);
	mutex_init(&mbi->batt_lock);
	mutex_init(&mbi->ipc_rw_lock);

	/* Populate data from SFI Table */
	sfi_table_populate(sfi_table);

	/* Initialize battery and charger Properties*/
	init_batt_props(mbi);
	init_charger_props(mbi);

	 /* Re Map Phy address space for MSIC regs */
	mbi->msic_regs_iomap = ioremap_nocache(sfi_table->sram_addr, 8);
	if (!mbi->msic_regs_iomap) {
		dev_err(&pdev->dev, "battery: ioremap Failed\n");
		retval = -ENOMEM;
		goto ioremap_failed;
	}

	/* Init MSIC Registers */
	retval = init_msic_regs(mbi);
	if (retval < 0)
		dev_err(&pdev->dev, "Regisrer init failed\n");

	/* Initialize ADC Channels */
	mbi->adc_index = mdf_initialize_adc(mbi);

	/* register msic-batt with power supply subsystem */
	mbi->batt.name = "msic-battery";
	mbi->batt.type = POWER_SUPPLY_TYPE_BATTERY;
	mbi->batt.properties = msic_battery_props;
	mbi->batt.num_properties = ARRAY_SIZE(msic_battery_props);
	mbi->batt.get_property = msic_battery_get_property;
	retval = power_supply_register(&pdev->dev, &mbi->batt);
	if (retval) {
		dev_err(&pdev->dev, "%s(): failed to register msic battery "
					"device with power supply subsystem\n",
					__func__);
		goto power_reg_failed_batt;
	}

	/* register msic-usb with power supply subsystem */
	mbi->usb.name = "msic-charger";
	mbi->usb.type = POWER_SUPPLY_TYPE_USB;
	mbi->usb.properties = msic_usb_props;
	mbi->usb.num_properties = ARRAY_SIZE(msic_usb_props);
	mbi->usb.get_property = msic_usb_get_property;
	retval = power_supply_register(&pdev->dev, &mbi->usb);
	if (retval) {
		dev_err(&pdev->dev, "%s(): failed to register msic usb "
					"device with power supply subsystem\n",
					__func__);
		goto power_reg_failed_usb;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_emrg_charge_enable);
	if (retval)
		goto sysfs1_create_faied;

	retval = device_create_file(&pdev->dev, &dev_attr_charge_enable);
	if (retval)
		goto sysfs2_create_faied;


	/* Register with OTG */
	otg_handle = penwell_otg_register_bc_callback(msic_charger_callback,
								(void *)mbi);
	if (!otg_handle) {
		dev_err(&pdev->dev, "battery: OTG Registration failed\n");
		retval = -EBUSY;
		goto otg_failed;
	}

	/* Init Runtime PM State */
	pm_runtime_set_active(&mbi->pdev->dev);
	pm_runtime_enable(&mbi->pdev->dev);
	pm_schedule_suspend(&mbi->pdev->dev, MSEC_PER_SEC);

	/* Check if already exist a Charger connection */
	retval = check_charger_conn(mbi);
	if (retval)
		dev_err(&pdev->dev, "check charger Conn failed\n");

	/* register interrupt */
	retval = request_threaded_irq(mbi->irq, msic_battery_interrupt_handler,
						msic_battery_thread_handler,
						0, DRIVER_NAME, mbi);
	if (retval) {
		dev_err(&pdev->dev, "%s(): cannot get IRQ\n", __func__);
		goto requestirq_failed;
	}


	/* Start the status monitoring worker */
	schedule_delayed_work(&mbi->chr_status_monitor, 0);
	return retval;

requestirq_failed:
	penwell_otg_unregister_bc_callback(otg_handle);
otg_failed:
	device_remove_file(&pdev->dev, &dev_attr_charge_enable);
sysfs2_create_faied:
	device_remove_file(&pdev->dev, &dev_attr_emrg_charge_enable);
sysfs1_create_faied:
	power_supply_unregister(&mbi->usb);
power_reg_failed_usb:
	power_supply_unregister(&mbi->batt);
power_reg_failed_batt:
	iounmap(mbi->msic_regs_iomap);
ioremap_failed:
	kfree(sfi_table);
	kfree(mbi);

	return retval;
}

static void do_exit_ops(struct msic_power_module_info *mbi)
{
	/* disable MSIC Charger */
	mutex_lock(&mbi->batt_lock);
	if (mbi->batt_props.status != POWER_SUPPLY_STATUS_DISCHARGING)
		msic_batt_stop_charging(mbi);
	mutex_unlock(&mbi->batt_lock);

	/* Check charge Cycles */
	if (chr_clmb_cnt >= (CHARGE_FULL_IN_MAH / 2))
		charge_cycle_ctr++;
}

/**
 * msic_battery_remove - msic battery finalize
 * @pdev: msic battery platform  device structure
 * Context: can sleep
 *
 * MSIC battery finalizes its internal data structure and other
 * infrastructure components that it initialized in
 * msic_battery_probe.
 */
static int msic_battery_remove(struct platform_device *pdev)
{
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);

	if (mbi) {
		penwell_otg_unregister_bc_callback(otg_handle);
		flush_scheduled_work();
		free_adc_channels(mbi->adc_index, mbi);
		free_irq(mbi->irq, mbi);
		do_exit_ops(mbi);
		if (mbi->msic_regs_iomap != NULL)
			iounmap(mbi->msic_regs_iomap);
		device_remove_file(&pdev->dev, &dev_attr_emrg_charge_enable);
		device_remove_file(&pdev->dev, &dev_attr_charge_enable);
		power_supply_unregister(&mbi->usb);
		power_supply_unregister(&mbi->batt);

		kfree(sfi_table);
		kfree(mbi);
	}

	return 0;
}

#ifdef CONFIG_PM
static int msic_battery_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	int event;

	spin_lock(&mbi->event_lock);
	event = mbi->batt_event;
	spin_unlock(&mbi->event_lock);

	if (event == USBCHRG_EVENT_CONNECT ||
		event == USBCHRG_EVENT_UPDATE ||
		event == USBCHRG_EVENT_RESUME) {

		msic_event_handler(mbi, USBCHRG_EVENT_SUSPEND, NULL);
		dev_dbg(&mbi->pdev->dev, "Forced suspend\n");
	}

	return 0;
}

static int msic_battery_resume(struct platform_device *pdev)
{
	int retval = 0;
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	int event;

	spin_lock(&mbi->event_lock);
	event = mbi->batt_event;
	spin_unlock(&mbi->event_lock);

	if (event == USBCHRG_EVENT_SUSPEND ||
		event == USBCHRG_EVENT_DISCONN) {
		/* Check if already exist a Charger connection */
		retval = check_charger_conn(mbi);
		if (retval)
			dev_warn(msic_dev, "check_charger_conn failed\n");
	}

	return retval;
}
#else
#define msic_battery_suspend    NULL
#define msic_battery_resume     NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int msic_runtime_suspend(struct device *dev)
{

	/* ToDo: Check for MSIC Power rails */
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int msic_runtime_resume(struct device *dev)
{
	/* ToDo: Check for MSIC Power rails */
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int msic_runtime_idle(struct device *dev)
{
	struct platform_device *pdev  = container_of(dev,
				struct platform_device, dev);
	struct msic_power_module_info *mbi = platform_get_drvdata(pdev);
	int event;

	dev_dbg(dev, "%s called\n", __func__);

	spin_lock(&mbi->event_lock);
	event = mbi->batt_event;
	spin_unlock(&mbi->event_lock);

	if (event == USBCHRG_EVENT_CONNECT ||
		event == USBCHRG_EVENT_UPDATE ||
		event == USBCHRG_EVENT_RESUME) {

		dev_warn(&mbi->pdev->dev, "%s: device busy\n", __func__);

		return -EBUSY;
	}

	return 0;
}
#else
#define msic_runtime_suspend	NULL
#define msic_runtime_resume	NULL
#define msic_runtime_idle	NULL
#endif
/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct platform_device_id battery_id_table[] = {
	{ "msic_battery", 1 },
};

static const struct dev_pm_ops msic_batt_pm_ops = {
	.runtime_suspend =	msic_runtime_suspend,
	.runtime_resume =	msic_runtime_resume,
	.runtime_idle =		msic_runtime_idle,
};

static struct platform_driver msic_battery_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &msic_batt_pm_ops,
	},
	.probe = msic_battery_probe,
	.remove = __devexit_p(msic_battery_remove),
	.suspend = msic_battery_suspend,
	.resume = msic_battery_resume,
	.id_table = battery_id_table,
};
static int __init msic_battery_module_init(void)
{
	int ret;

	ret = platform_driver_register(&msic_battery_driver);
	if (ret)
		dev_err(msic_dev, "driver_register failed");

	return ret;
}

static void __exit msic_battery_module_exit(void)
{
	platform_driver_unregister(&msic_battery_driver);
}
module_init(msic_battery_module_init);
module_exit(msic_battery_module_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_AUTHOR("Anantha Narayanan <anantha.narayanan@intel.com>");
MODULE_AUTHOR("Ananth Krishna <ananth.krishna.r@intel.com>");
MODULE_DESCRIPTION("Intel Medfield MSIC Battery Driver");
MODULE_LICENSE("GPL");

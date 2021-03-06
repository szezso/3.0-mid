/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>

/* Status register bits */
#define STATUS_POR_BIT         (1 << 1)
#define STATUS_BST_BIT         (1 << 3)
#define STATUS_VMN_BIT         (1 << 8)
#define STATUS_TMN_BIT         (1 << 9)
#define STATUS_SMN_BIT         (1 << 10)
#define STATUS_BI_BIT          (1 << 11)
#define STATUS_VMX_BIT         (1 << 12)
#define STATUS_TMX_BIT         (1 << 13)
#define STATUS_SMX_BIT         (1 << 14)
#define STATUS_BR_BIT          (1 << 15)

/* Interrupt mask bits */
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define STATUS_INTR_SOC_BIT	(1 << 14)
#define STATUS_INTR_LOW_SOC_BIT	(1 << 10)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define MODEL_UNLOCK1	0X0059
#define MODEL_UNLOCK2	0X00C4
#define MODEL_LOCK1		0X0000
#define MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

struct max17042_chip {
	struct i2c_client *client;
	struct power_supply battery;
	struct max17042_platform_data *pdata;
	struct work_struct work;
};

static int max17042_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17042_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void max17042_set_reg(struct i2c_client *client,
			     struct max17042_reg_data *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
		max17042_write_reg(client, data[i].addr, data[i].data);
}

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
};

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_STATUS);
		if (val->intval & MAX17042_STATUS_BattAbsent)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_Cycles);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_MinMaxVolt);
		val->intval >>= 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_V_empty);
		val->intval >>= 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_VCELL);
		val->intval >>= 3;
		val->intval *= 625; /* Units of LSB = 625 uV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_AvgVCELL);
		val->intval >>= 3;
		val->intval *= 625; /* Units of LSB = 625 uV */
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_RepSOC) / 256;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_RepSOC);
		if ((val->intval / 256) >= MAX17042_BATTERY_FULL)
			val->intval = 1;
		else if (val->intval >= 0)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = max17042_read_reg(chip->client,
				MAX17042_TEMP);
		/* The value is signed. */
		if (val->intval & 0x8000) {
			val->intval = (0x7fff & ~val->intval) + 1;
			val->intval *= -1;
		}
		/* The value is converted into deci-centigrade scale */
		/* Units of LSB = 1 / 256 degree Celsius */
		val->intval = val->intval * 10 / 256;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* First get voltage */
		val->intval = max17042_read_reg(chip->client,
				MAX17042_AvgVCELL);
		val->intval >>= 3;
		val->intval *= 625; /* Units of LSB = 625 uV */
		val->intval /= 1000; /* convert to mV to prevent overflow */

		/* Multiply by mAh capacity to get energy in uWh.
		 * Register units = 5 uVh */
		val->intval *= max17042_read_reg(chip->client,
				MAX17042_RepCap) * 5000 / chip->pdata->r_sns;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->pdata->enable_current_sense) {
			val->intval = max17042_read_reg(chip->client,
					MAX17042_Current);
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval >>= 4;
			val->intval *= 1000000 * 25 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->pdata->enable_current_sense) {
			val->intval = max17042_read_reg(chip->client,
					MAX17042_AvgCurrent);
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17042_write_verify_reg(struct i2c_client *client,
				u8 reg, u16 value)
{
	int retries = 8;
	int ret;
	u16 read_value;

	do {
		ret = i2c_smbus_write_word_data(client, reg, value);
		read_value =  max17042_read_reg(client, reg);
		if (read_value != value) {
			ret = -EIO;
			retries--;
		}
	} while (retries && read_value != value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static inline void max17042_override_por(
	struct i2c_client *client, u8 reg, u16 value)
{
	if (value)
		max17042_write_reg(client, reg, value);
}

static inline void max10742_unlock_model(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	max17042_write_reg(client, MAX17042_MLOCKReg1, MODEL_UNLOCK1);
	max17042_write_reg(client, MAX17042_MLOCKReg2, MODEL_UNLOCK2);
}

static inline void max10742_lock_model(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	max17042_write_reg(client, MAX17042_MLOCKReg1, MODEL_LOCK1);
	max17042_write_reg(client, MAX17042_MLOCKReg2, MODEL_LOCK2);
}

static inline void max17042_write_model_data(struct max17042_chip *chip,
					u8 addr, int size)
{
	struct i2c_client *client = chip->client;
	int i;
	for (i = 0; i < size; i++)
		max17042_write_reg(client, addr + i,
				chip->pdata->config_data->cell_char_tbl[i]);
}

static inline void max17042_read_model_data(struct max17042_chip *chip,
					u8 addr, u16 *data, int size)
{
	struct i2c_client *client = chip->client;
	int i;

	for (i = 0; i < size; i++)
		data[i] = max17042_read_reg(client, addr + i);
}

static inline int max17042_model_data_compare(struct max17042_chip *chip,
					u16 *data1, u16 *data2, int size)
{
	int i;

	if (memcmp(data1, data2, size)) {
		dev_err(&chip->client->dev, "%s compare failed\n", __func__);
		for (i = 0; i < size; i++)
			dev_info(&chip->client->dev, "0x%x, 0x%x",
				data1[i], data2[i]);
		dev_info(&chip->client->dev, "\n");
		return -EINVAL;
	}
	return 0;
}

static int max17042_init_model(struct max17042_chip *chip)
{
	int ret;
	int table_size =
		sizeof(chip->pdata->config_data->cell_char_tbl)/sizeof(u16);
	u16 *temp_data;

	temp_data = kzalloc(sizeof(chip->pdata->config_data->cell_char_tbl),
			    GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_write_model_data(chip, MAX17042_MODELChrTbl,
				table_size);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);

	ret = max17042_model_data_compare(
		chip,
		chip->pdata->config_data->cell_char_tbl,
		temp_data,
		table_size);

	max10742_lock_model(chip);
	kfree(temp_data);

	return ret;
}

static int max17042_verify_model_lock(struct max17042_chip *chip)
{
	int i;
	int table_size =
		sizeof(chip->pdata->config_data->cell_char_tbl)/sizeof(u16);
	u16 *temp_data;
	int ret = 0;

	temp_data = kzalloc(sizeof(chip->pdata->config_data->cell_char_tbl),
			    GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);
	for (i = 0; i < table_size; i++)
		if (temp_data[i]) {
			ret = -EINVAL;
			break;
		}
	kfree(temp_data);
	return ret;
}

static void max17042_write_config_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_write_reg(chip->client, MAX17042_CONFIG, config->config);
	max17042_write_reg(chip->client, MAX17042_LearnCFG, config->learn_cfg);
	max17042_write_reg(chip->client, MAX17042_FilterCFG,
			config->filter_cfg);
	max17042_write_reg(chip->client, MAX17042_RelaxCFG, config->relax_cfg);
}

static void  max17042_write_custom_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_write_verify_reg(chip->client, MAX17042_RCOMP0,
				config->rcomp0);
	max17042_write_verify_reg(chip->client, MAX17042_TempCo,
				config->tcompc0);
	max17042_write_reg(chip->client, MAX17042_EmptyTempCo,
			config->empty_tempco);
	max17042_write_verify_reg(chip->client, MAX17042_K_empty0,
				config->kempty0);
	max17042_write_verify_reg(chip->client, MAX17042_ICHGTerm,
				config->ichgt_term);
}

static void max17042_update_capacity_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
				config->fullcap);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
				config->fullcapnom);
}

static void max17042_reset_vfsoc0_reg(struct max17042_chip *chip)
{
	u16 vfSoc;

	vfSoc = max17042_read_reg(chip->client, MAX17042_VFSOC);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(chip->client, MAX17042_VFSOC0, vfSoc);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_LOCK);
}

static void max17042_load_new_capacity_params(struct max17042_chip *chip)
{
	u16 full_cap0, rep_cap, dq_acc, vfSoc;
	u32 rem_cap;

	struct max17042_config_data *config = chip->pdata->config_data;

	full_cap0 = max17042_read_reg(chip->client, MAX17042_FullCAP0);
	vfSoc = max17042_read_reg(chip->client, MAX17042_VFSOC);

	/* fg_vfSoc needs to shifted by 8 bits to get the
	 * perc in 1% accuracy, to get the right rem_cap multiply
	 * full_cap0, fg_vfSoc and devide by 100
	 */
	rem_cap = ((vfSoc >> 8) * full_cap0) / 100;
	max17042_write_verify_reg(chip->client, MAX17042_RemCap, (u16)rem_cap);

	rep_cap = (u16)rem_cap;
	max17042_write_verify_reg(chip->client, MAX17042_RepCap, rep_cap);

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = config->fullcap / dQ_ACC_DIV;
	max17042_write_verify_reg(chip->client, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(chip->client, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
			config->fullcap);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
			config->fullcapnom);
}

/*
 * Block write all the override values coming from platform data.
 * This function MUST be called before the POR initialization proceedure
 * specified by maxim.
 */
static inline void max17042_override_por_values(struct max17042_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por(client, MAX17042_TGAIN, config->tgain);
	max17042_override_por(client, MAx17042_TOFF, config->toff);
	max17042_override_por(client, MAX17042_CGAIN, config->cgain);
	max17042_override_por(client, MAX17042_COFF, config->coff);

	max17042_override_por(client, MAX17042_VALRT_Th, config->valrt_thresh);
	max17042_override_por(client, MAX17042_TALRT_Th, config->talrt_thresh);
	max17042_override_por(client, MAX17042_SALRT_Th,
			config->soc_alrt_thresh);
	max17042_override_por(client, MAX17042_CONFIG, config->config);
	max17042_override_por(client, MAX17042_SHDNTIMER, config->shdntimer);

	max17042_override_por(client, MAX17042_DesignCap, config->design_cap);
	max17042_override_por(client, MAX17042_ICHGTerm, config->ichgt_term);

	max17042_override_por(client, MAX17042_AtRate, config->at_rate);
	max17042_override_por(client, MAX17042_LearnCFG, config->learn_cfg);
	max17042_override_por(client, MAX17042_FilterCFG, config->filter_cfg);
	max17042_override_por(client, MAX17042_RelaxCFG, config->relax_cfg);
	max17042_override_por(client, MAX17042_MiscCFG, config->misc_cfg);
	max17042_override_por(client, MAX17042_MaskSOC, config->masksoc);

	max17042_override_por(client, MAX17042_FullCAP, config->fullcap);
	max17042_override_por(client, MAX17042_FullCAPNom, config->fullcapnom);
	max17042_override_por(client, MAX17042_SOC_empty, config->socempty);
	max17042_override_por(client, MAX17042_LAvg_empty, config->lavg_empty);
	max17042_override_por(client, MAX17042_dQacc, config->dqacc);
	max17042_override_por(client, MAX17042_dPacc, config->dpacc);

	max17042_override_por(client, MAX17042_V_empty, config->vempty);
	max17042_override_por(client, MAX17042_TempNom, config->temp_nom);
	max17042_override_por(client, MAX17042_TempLim, config->temp_lim);
	max17042_override_por(client, MAX17042_FCTC, config->fctc);
	max17042_override_por(client, MAX17042_RCOMP0, config->rcomp0);
	max17042_override_por(client, MAX17042_TempCo, config->tcompc0);
	max17042_override_por(client, MAX17042_EmptyTempCo,
			config->empty_tempco);
	max17042_override_por(client, MAX17042_K_empty0, config->kempty0);
}

static int max17042_init_chip(struct max17042_chip *chip)
{
	int val;
	int ret;

	val = max17042_read_reg(chip->client, MAX17042_STATUS);

	if (val & STATUS_POR_BIT) {

		max17042_override_por_values(chip);
		/* After Power up, the MAX17042 requires 500mS in order
		 * to perform signal debouncing and initial SOC reporting
		 */
		msleep(500);

		/* Initialize configaration */
		max17042_write_config_regs(chip);

		/* write cell characterization data */
		ret = max17042_init_model(chip);
		if (ret) {
			dev_err(&chip->client->dev, "%s init failed\n",
				__func__);
			return -EIO;
		}
		ret = max17042_verify_model_lock(chip);
		if (ret) {
			dev_err(&chip->client->dev, "%s lock verify failed\n",
				__func__);
			return -EIO;
		}
		/* write custom parameters */
		max17042_write_custom_regs(chip);

		/* update capacity params */
		max17042_update_capacity_regs(chip);

		/* delay must be atleast 350mS to allow VFSOC
		 * to be calculated from the new configuration
		 */
		msleep(350);

		/* reset vfsoc0 reg */
		max17042_reset_vfsoc0_reg(chip);


		/* load new capacity params */
		max17042_load_new_capacity_params(chip);

		/* Init complete, Clear the POR bit */
		val = max17042_read_reg(chip->client, MAX17042_STATUS);
		max17042_write_reg(chip->client, MAX17042_STATUS,
						val & (~STATUS_POR_BIT));
	}
	return 0;
}

static void max17042_set_soc_threshold(struct max17042_chip *chip, u16 off)
{
	u16 soc, soc_tr;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'off' perc change in the soc
	 */
	soc = max17042_read_reg(chip->client, MAX17042_RepSOC) >> 8;
	soc_tr = (soc + off) << 8;
	soc_tr |= (soc - off);
	max17042_write_reg(chip->client, MAX17042_SALRT_Th, soc_tr);
}

static irqreturn_t max17042_intr_handler(int id, void *dev)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;
	u16 val;

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	if ((val & STATUS_INTR_SOC_BIT) ||
		(val & STATUS_INTR_LOW_SOC_BIT)) {
		printk(KERN_ERR "SOC threshold INTR\n");
		dev_info(&chip->client->dev, "SOC threshold INTR\n");
		max17042_set_soc_threshold(chip, 1);
	}

	power_supply_changed(&chip->battery);
	return IRQ_HANDLED;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, work);
	struct i2c_client *client = chip->client;
	int ret;
	u16 reg;

	/* Initialize registers according to values from the platform data */
	if (chip->pdata->init_data)
		max17042_set_reg(client, chip->pdata->init_data,
				 chip->pdata->num_init_data);

	if (chip->pdata->enable_por_init && chip->pdata->config_data) {
		ret = max17042_init_chip(chip);
		if (ret)
			return;
	}

	if (!chip->pdata->enable_current_sense) {
		max17042_write_reg(client, MAX17042_CGAIN, 0x0000);
		max17042_write_reg(client, MAX17042_MiscCFG, 0x0003);
		max17042_write_reg(client, MAX17042_LearnCFG, 0x0007);
	} else {
		if (chip->pdata->r_sns == 0)
			chip->pdata->r_sns = MAX17042_DEFAULT_SNS_RESISTOR;
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq, max17042_intr_handler,
						max17042_thread_handler,
						IRQF_TRIGGER_FALLING,
						chip->battery.name, chip);
		if (!ret) {
			reg =  max17042_read_reg(client, MAX17042_CONFIG);
			reg |= CONFIG_ALRT_BIT_ENBL;
			max17042_write_reg(client, MAX17042_CONFIG, reg);
			max17042_set_soc_threshold(chip, 1);
		} else
			dev_err(&client->dev, "%s(): cannot get IRQ\n",
				__func__);

	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
	}
}

static int __devinit max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "max17042_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	/* When current is not measured,
	 * CURRENT_NOW and CURRENT_AVG properties should be invisible. */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 2;

	INIT_WORK(&chip->work, max17042_init_worker);
	schedule_work(&chip->work);

	return 0;
}

static int __devexit max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->battery);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
	},
	.probe		= max17042_probe,
	.remove		= __devexit_p(max17042_remove),
	.id_table	= max17042_id,
};

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}
module_init(max17042_init);

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}
module_exit(max17042_exit);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");

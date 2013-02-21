/*
 * intel_mid_thermal.c - Intel MID platform thermal driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.        See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Durgadoss R <durgadoss.r@intel.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#include <asm/intel_mid_gpadc.h>

/* ADC channel code values */
#define SKIN_SENSOR0_CODE	0x08
#define SKIN_SENSOR1_CODE	0x09
#define SYS_SENSOR_CODE		0x0A
#define MSIC_DIE_SENSOR_CODE	0x03

/* ADC code range */
#define ADC_MAX			977
#define ADC_MIN			162
#define ADC_VAL0C		887
#define ADC_VAL20C		720
#define ADC_VAL40C		508
#define ADC_VAL60C		315

/* MSIC die attributes */
#define MSIC_DIE_ADC_MIN	488
#define MSIC_DIE_ADC_MAX	1004

static struct thermal_device_info {
	char *name;
	int adc_channel;
	bool direct;
	void *adc_handle;
} thermal_devices[] = {
	{
		.name		= "skin0",
		.adc_channel	= SKIN_SENSOR0_CODE,
	},
	{
		.name		= "skin1",
		.adc_channel	= SKIN_SENSOR1_CODE,
	},
	{
		.name		= "sys",
		.adc_channel	= SYS_SENSOR_CODE,
	},
	{
		.name		= "msicdie",
		.adc_channel	= MSIC_DIE_SENSOR_CODE,
		.direct		= true,
	},
};

struct platform_info {
	struct platform_device *pdev;
	struct thermal_zone_device *tzd[ARRAY_SIZE(thermal_devices)];
};

/**
 * to_msic_die_temp - converts adc_val to msic_die temperature
 * @adc_val: ADC value to be converted
 *
 * Can sleep
 */
static int to_msic_die_temp(uint16_t adc_val)
{
	return (368 * (adc_val) / 1000) - 220;
}

/**
 * is_valid_adc - checks whether the adc code is within the defined range
 * @min: minimum value for the sensor
 * @max: maximum value for the sensor
 *
 * Can sleep
 */
static int is_valid_adc(uint16_t adc_val, uint16_t min, uint16_t max)
{
	return (adc_val >= min) && (adc_val <= max);
}

/**
 * adc_to_temp - converts the ADC code to temperature in C
 * @direct: true if ths channel is direct index
 * @adc_val: the adc_val that needs to be converted
 * @tp: temperature return value
 *
 * Linear approximation is used to covert the skin adc value into temperature.
 * This technique is used to avoid very long look-up table to get
 * the appropriate temp value from ADC value.
 * The adc code vs sensor temp curve is split into five parts
 * to achieve very close approximate temp value with less than
 * 0.5C error
 */
static int adc_to_temp(bool direct, uint16_t adc_val, unsigned long *tp)
{
	int temp;

	/* Direct conversion for die temperature */
	if (direct) {
		if (is_valid_adc(adc_val, MSIC_DIE_ADC_MIN, MSIC_DIE_ADC_MAX)) {
			*tp = to_msic_die_temp(adc_val) * 1000;
			return 0;
		}
		return -ERANGE;
	}

	if (!is_valid_adc(adc_val, ADC_MIN, ADC_MAX))
		return -ERANGE;

	/* Linear approximation for skin temperature */
	if (adc_val > ADC_VAL0C)
		temp = 177 - (adc_val/5);
	else if ((adc_val <= ADC_VAL0C) && (adc_val > ADC_VAL20C))
		temp = 111 - (adc_val/8);
	else if ((adc_val <= ADC_VAL20C) && (adc_val > ADC_VAL40C))
		temp = 92 - (adc_val/10);
	else if ((adc_val <= ADC_VAL40C) && (adc_val > ADC_VAL60C))
		temp = 91 - (adc_val/10);
	else
		temp = 112 - (adc_val/6);

	/* Convert temperature in celsius to milli degree celsius */
	*tp = temp * 1000;
	return 0;
}

/**
 * mid_read_temp - read sensors for temperature
 * @temp: holds the current temperature for the sensor after reading
 *
 * reads the adc_code from the channel and converts it to real
 * temperature. The converted value is stored in temp.
 *
 * Can sleep
 */
static int mid_read_temp(struct thermal_zone_device *tzd, unsigned long *temp)
{
	struct thermal_device_info *td_info = tzd->devdata;
	unsigned long curr_temp;
	int adc_val, ret;

	ret = intel_mid_gpadc_sample(td_info->adc_handle, 1, &adc_val);
	if (ret)
		return ret;

	/* Convert ADC value to temperature */
	ret = adc_to_temp(td_info->direct, adc_val, &curr_temp);
	if (ret == 0)
		*temp = curr_temp;
	return ret;
}

/* Can't be const */
static struct thermal_zone_device_ops tzd_ops = {
	.get_temp = mid_read_temp,
};

/**
 * mid_thermal_probe - mfld thermal initialize
 * @pdev: platform device structure
 *
 * mid thermal probe initializes the hardware and registers
 * all the sensors with the generic thermal framework. Can sleep.
 */
static int mid_thermal_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct platform_info *pinfo;

	pinfo = kzalloc(sizeof(struct platform_info), GFP_KERNEL);
	if (!pinfo)
		return -ENOMEM;

	/* Register each sensor with the generic thermal framework*/
	for (i = 0; i < ARRAY_SIZE(thermal_devices); i++) {
		struct thermal_device_info *tdi = &thermal_devices[i];

		tdi->adc_handle = intel_mid_gpadc_alloc(1,
					tdi->adc_channel | CH_NEED_VREF);
		if (!tdi->adc_handle)
			goto reg_fail;

		pinfo->tzd[i] = thermal_zone_device_register(tdi->name, 0, tdi,
							&tzd_ops, 0, 0, 0, 0);
		if (IS_ERR(pinfo->tzd[i])) {
			intel_mid_gpadc_free(tdi->adc_handle);
			goto reg_fail;
		}
	}

	pinfo->pdev = pdev;
	platform_set_drvdata(pdev, pinfo);
	return 0;

reg_fail:
	ret = PTR_ERR(pinfo->tzd[i]);
	while (--i >= 0) {
		struct thermal_device_info *tdi = pinfo->tzd[i]->devdata;

		thermal_zone_device_unregister(pinfo->tzd[i]);
		intel_mid_gpadc_free(tdi->adc_handle);
	}
	kfree(pinfo);
	return ret;
}

/**
 * mid_thermal_remove - mfld thermal finalize
 * @dev: platform device structure
 *
 * MLFD thermal remove unregisters all the sensors from the generic
 * thermal framework. Can sleep.
 */
static int mid_thermal_remove(struct platform_device *pdev)
{
	int i;
	struct platform_info *pinfo = platform_get_drvdata(pdev);

	for (i = 0; i < ARRAY_SIZE(thermal_devices); i++) {
		struct thermal_device_info *tdi = pinfo->tzd[i]->devdata;

		thermal_zone_device_unregister(pinfo->tzd[i]);
		intel_mid_gpadc_free(tdi->adc_handle);
	}

	kfree(pinfo);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#define DRIVER_NAME "msic_sensor"

static const struct platform_device_id therm_id_table[] = {
	{ DRIVER_NAME, 1 },
	{ "msic_thermal", 1 },
	{ }
};

static struct platform_driver mid_thermal_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mid_thermal_probe,
	.remove = __devexit_p(mid_thermal_remove),
	.id_table = therm_id_table,
};

static int __init mid_thermal_module_init(void)
{
	return platform_driver_register(&mid_thermal_driver);
}

static void __exit mid_thermal_module_exit(void)
{
	platform_driver_unregister(&mid_thermal_driver);
}

module_init(mid_thermal_module_init);
module_exit(mid_thermal_module_exit);

MODULE_AUTHOR("Durgadoss R <durgadoss.r@intel.com>");
MODULE_DESCRIPTION("Intel Medfield Platform Thermal Driver");
MODULE_LICENSE("GPL");

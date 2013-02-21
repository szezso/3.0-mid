/*
 *  psb backlight using HAL
 *
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: Eric Knopp
 *
 */

#include <linux/backlight.h>
#include <linux/version.h>
#include <asm/intel_scu_ipc.h>
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_drv.h"
#include "psb_powermgmt.h"
#include "mdfld_dsi_dbi.h"
#include "tc35876x-dsi-lvds.h"

#define MRST_BLC_MAX_PWM_REG_FREQ	    0xFFFF
#define BLC_PWM_PRECISION_FACTOR 100	/* 10000000 */
#define BLC_PWM_FREQ_CALC_CONSTANT 32
#define MHz 1000000
#define BRIGHTNESS_MIN_LEVEL 1
#define BRIGHTNESS_MAX_LEVEL 100
#define BRIGHTNESS_MASK	0xFF
#define BLC_POLARITY_NORMAL 0
#define BLC_POLARITY_INVERSE 1
#define BLC_ADJUSTMENT_MAX 100

#define PSB_BLC_PWM_PRECISION_FACTOR    10
#define PSB_BLC_MAX_PWM_REG_FREQ        0xFFFE
#define PSB_BLC_MIN_PWM_REG_FREQ        0x2

#define PSB_BACKLIGHT_PWM_POLARITY_BIT_CLEAR (0xFFFE)
#define PSB_BACKLIGHT_PWM_CTL_SHIFT	(16)

static struct backlight_device *psb_backlight_device;

int psb_set_brightness(struct backlight_device *bd)
{
	struct drm_device *dev =
	    (struct drm_device *)bl_get_data(psb_backlight_device);
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	int level = bd->props.brightness;

	DRM_DEBUG_DRIVER("backlight level set to %d\n", level);

	/* Perform value bounds checking */
	if (level < BRIGHTNESS_MIN_LEVEL)
		level = BRIGHTNESS_MIN_LEVEL;

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
		u32 adjusted_level = 0;

		/*
		 * Adjust the backlight level with the percent in
		 * dev_priv->blc_adj2
		 */
		adjusted_level = level * dev_priv->blc_adj2;
		adjusted_level = adjusted_level / BLC_ADJUSTMENT_MAX;
		dev_priv->brightness_adjusted = adjusted_level;

#ifndef CONFIG_MDFLD_DSI_DPU
		if((!(dev_priv->dsr_fb_update & MDFLD_DSR_MIPI_CONTROL)) &&
			(dev_priv->dbi_panel_on || dev_priv->dbi_panel_on2)){
			mdfld_dsi_dbi_exit_dsr(dev,MDFLD_DSR_MIPI_CONTROL);
			PSB_DEBUG_ENTRY("Out of DSR before set brightness to %d.\n",
					dev_priv->brightness_adjusted);
		}
#endif
		if (get_panel_type(dev, 0) == TC35876X) {
			if (dev_priv->dpi_panel_on || dev_priv->dpi_panel_on2)
				tc35876x_brightness_control(dev,
							    dev_priv->brightness_adjusted);
		} else {
			if (dev_priv->dbi_panel_on || dev_priv->dpi_panel_on)
				mdfld_dsi_brightness_control(dev, 0,
							     dev_priv->brightness_adjusted);
		}

		if ((dev_priv->dbi_panel_on2) || (dev_priv->dpi_panel_on2))
			mdfld_dsi_brightness_control(dev, 2, dev_priv->brightness_adjusted);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	/* cache the brightness for later use */
	dev_priv->brightness = level;
	return 0;
}

int psb_get_brightness(struct backlight_device *bd)
{
	struct drm_device *dev =
	    (struct drm_device *)bl_get_data(psb_backlight_device);
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	DRM_DEBUG_DRIVER("brightness = 0x%x \n", dev_priv->brightness);

	/* return locally cached var instead of HW read (due to DPST etc.) */
	return dev_priv->brightness;
}

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static struct backlight_ops psb_ops = {
	.get_brightness = psb_get_brightness,
	.update_status  = psb_set_brightness,
};

static int device_backlight_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *)
		dev->dev_private;

	dev_priv->blc_adj1 = BLC_ADJUSTMENT_MAX;
	dev_priv->blc_adj2 = BLC_ADJUSTMENT_MAX;

	return 0;
}
#endif

int psb_backlight_init(struct drm_device *dev)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	struct backlight_properties props;
	uint8_t data = 0;
	int ret = 0;

	dev_priv->b_pmic_backlight = false;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = BRIGHTNESS_MAX_LEVEL;
	props.type = BACKLIGHT_PLATFORM;
	psb_backlight_device = backlight_device_register("psb-bl", NULL, (void *)dev, &psb_ops, &props);

	if (IS_ERR(psb_backlight_device))
		return PTR_ERR(psb_backlight_device);

	if ((ret = device_backlight_init(dev)) != 0)
		return ret;

	psb_backlight_device->props.brightness = BRIGHTNESS_MAX_LEVEL;
	psb_backlight_device->props.max_brightness = BRIGHTNESS_MAX_LEVEL;
	if (dev_priv->b_pmic_backlight) {
		intel_scu_ipc_ioread8(0x28, &data);
		dev_priv->saveBKLTCNT = data;
		intel_scu_ipc_ioread8(0x29, &data);
		dev_priv->saveBKLTREQ = data;
		intel_scu_ipc_ioread8(0x2A, &data);
		dev_priv->saveBKLTBRTL = data;
	}
	backlight_update_status(psb_backlight_device);
#endif
	return 0;
}

void psb_backlight_exit(void)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	psb_backlight_device->props.brightness = 0;
	backlight_update_status(psb_backlight_device);
	backlight_device_unregister(psb_backlight_device);
#endif
	return;
}

struct backlight_device * psb_get_backlight_device(void)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	return psb_backlight_device;
#endif
	return NULL;
}

/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* Definition for debug print format */
#define pr_fmt(fmt)	"[otm_hdmi]: " fmt

#include <linux/slab.h>
#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/switch.h>
#include "psb_intel_drv.h"
#include "psb_intel_reg.h"
#include "psb_drv.h"

#include "otm_hdmi_types.h"
#include "otm_hdmi.h"
#include "android_hdmi.h"
#ifdef OTM_HDMI_HDCP_ENABLE
#include "hdcp_api.h"
#endif

/* TODO: need to remove this, once I2C issue is worked out. */
#include <asm/intel_scu_ipc.h>
#include "psb_intel_hdmi.h"
#include "psb_powermgmt.h"

/* TODO: metadata should be populated using table from mode_info.c */
static const struct {
	int width, height, htotal, vtotal, dclk, vrefr, vic;
} vic_formats[11] = {
	{  640,  480,  800,  525,  25200, 60,  1 }, /* 640x480p60 4:3 */
	{  720,  480,  858,  525,  27027, 60,  2 }, /* 720x480p60 4:3 */
	{  720,  480,  858,  525,  27027, 60,  3 }, /* 720x480p60 16:9 */
	{ 1280,  720, 1650,  750,  74250, 60,  4 }, /* 1280x720p60 16:9 */
	{ 1920, 1080, 2200, 1125, 148500, 60, 16 }, /* 1920x1080p60 16:9 */
	{  720,  576,  864,  625,  27000, 50, 17 }, /* 720x576p50 4:3 */
	{  720,  576,  864,  625,  27000, 50, 18 }, /* 720x576p50 16:9 */
	{ 1280,  720, 1980,  750,  74250, 50, 19 }, /* 1280x720p50 16:9 */
	{ 1920, 1080, 2750, 1125,  74250, 24, 32 }, /* 1920x1080p24 16:9 */
	{ 1920, 1080, 2640, 1125,  74250, 25, 33 }, /* 1920x1080p25 16:9 */
	{ 1920, 1080, 2200, 1125,  74250, 30, 34 }, /* 1920x1080p30 16:9 */
};

/* Function declarations for interrupt routines */
static irqreturn_t android_hdmi_irq_callback(int irq, void *data);
static irqreturn_t __hdmi_irq_handler_bottomhalf(void *data);

static int calculate_refresh_rate(struct drm_display_mode *mode);

/*
 * TODO: Remove this structure and counter afer EDID Parse
 *	 for established modes is implemented
 */
#define DEBUG_MODES 100
static struct debug_modes__t {
	int clk;
	int frq;
	char name[DRM_DISPLAY_MODE_LEN + 1];
} arr_modes[DEBUG_MODES];

static u32 debug_modes_count;

/*OTM_HDMI_FIXME: this should be from get attribute interface*/
#define OTM_HDMI_I2C_ADAPTER_NUM 8
#define OTM_HDMI_MAX_DDC_WRITE_SIZE 20

#define SWITCH_DEV_HDMI_NAME "hdmi"
#define SWITCH_DEV_DVI_NAME "dvi"

#define WPT_IOBAR_OFFSET_BASE	   0x1F0000
#define WPT_IOBAR_INDEX_REGISTER	0x2110
#define WPT_IOBAR_DATA_REGISTER	 0x2114

/* Default HDMI Edid - 640x480p 720x480p 1280x720p */
static unsigned char default_edid[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x25, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x14, 0x01, 0x03, 0x80, 0x00, 0x00, 0xFF,
	0x2A, 0xBA, 0x45, 0xA1, 0x59, 0x55, 0x9D, 0x28,
	0x0D, 0x50, 0x54, 0x20, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D,
	0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28,
	0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0x8F, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10,
	0x10, 0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x49,
	0x4E, 0x54, 0x45, 0x4C, 0x2D, 0x54, 0x56, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x0C, 0x37, 0x3D, 0x1F, 0x31, 0x0F, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xCE,
	0x02, 0x03, 0x13, 0x41, 0x42, 0x04, 0x02, 0x23,
	0x09, 0x07, 0x07, 0x67, 0x03, 0x0C, 0x00, 0x10,
	0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B
};

/*store the state whether the edid is ready
 *in HPD (1) or not (0)*/
static int edid_ready_in_hpd = 0;

/**
 * This function handles bottom half of HDMI hotplug interrupts
 * @data	: android hdmi private structure
 *
 * Returns the behavior of the interrupt handler
 *	IRQ_HANDLED - if interrupt handled
 * This function handles bottom half of HDMI hotplug interrupts.
 * IRQ interrupt bottomhalf handler callback. This callback
 * will be called for hdmi plug/unplug interrupts
 */
static irqreturn_t __hdmi_irq_handler_bottomhalf(void *data)
{
	struct android_hdmi_priv *hdmi_priv = data;
	bool hpd = otm_hdmi_power_rails_on();

	if ((true == hpd) && (hdmi_priv != NULL)) {
		/* Get monitor type. This is required to appropriately
		 * issue switch class event to the user space depending
		 * on the monitor type - HDMI or DVI.
		 */
		struct drm_mode_config *mode_config = NULL;
		struct edid *edid = NULL;
		struct drm_connector *connector = NULL;
		struct i2c_adapter *adapter = NULL;
		u8 hdmi_status = 0;

		if (!hdmi_priv->dev)
			return IRQ_HANDLED;

		/* Check HDMI status, read EDID only if connected */
		intel_scu_ipc_ioread8(MSIC_HDMI_STATUS, &hdmi_status);
#ifdef OTM_HDMI_HDCP_ENABLE
		otm_hdmi_hdcp_set_hpd_state(hdmi_priv->context,
				(hdmi_status & HPD_SIGNAL_STATUS));
#endif
		if (!(hdmi_status & HPD_SIGNAL_STATUS))
			goto exit;

		adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);
		if (!adapter) {
			pr_err("Unable to get i2c adapter for HDMI");
			goto exit;
		}

		mode_config = &hdmi_priv->dev->mode_config;
		list_for_each_entry(connector,
					&mode_config->connector_list,
					head) {
			if (DRM_MODE_CONNECTOR_DVID ==
				connector->connector_type) {
				edid = (struct edid *)
					drm_get_edid(connector, adapter);
				if (edid) {
					if (drm_detect_hdmi_monitor(edid))
						/* MONITOR_TYPE_HDMI */
						hdmi_priv->monitor_type = 1;
					else
						/* MONITOR_TYPE_DVI */
						hdmi_priv->monitor_type = 2;
					/* Store raw edid in HDMI context */
					otm_hdmi_set_raw_edid(
						hdmi_priv->context,
						(char *)edid);
					/* Raw edid is ready in HDMI context */
					edid_ready_in_hpd = 1;
					kfree(edid);
				} else {
					pr_err("Edid Read failed");
					/* Retry in next get modes */
					edid_ready_in_hpd = 0;
				}
				break;
			}
		}
exit:
		/* Notify user space */
		drm_helper_hpd_irq_event(hdmi_priv->dev);
	}

	return IRQ_HANDLED;
}

#ifdef OTM_HDMI_FIXME
gdl_ret_t android_hdmi_audio_control(void *context, bool flag)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	hdmi_device_t *dev = &ctx->dev;
	gdl_ret_t rc = GDL_SUCCESS;

	if (flag) {
		/* Enable audio */
		IOW(HDMI_UNIT_CONTROL, 0x67);
		IOR(HDMI_UNIT_CONTROL);

		IOW(0x51a8, 0x10);
		IOR(0x51a8);

		IOW(HDMI_AUDIO_CONTROL, 0x1);
		IOR(HDMI_AUDIO_CONTROL);
	} else {
		/* Disable audio */
		IOW(0x51a8, 0x0);
		IOR(0x51a8);

		IOW(HDMI_AUDIO_CONTROL, 0x0);
		IOR(HDMI_AUDIO_CONTROL);

		IOW(HDMI_UNIT_CONTROL, 0x47);
		IOR(HDMI_UNIT_CONTROL);
	}

	return rc;
}
#endif

#ifdef OTM_HDMI_HDCP_ENABLE
static int hdmi_ddc_read_write(bool read,
			uint8_t i2c_addr,
			uint8_t offset,
			uint8_t *buffer,
			int size)
{
	struct i2c_adapter *adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);
	struct i2c_msg msgs[2];
	int num_of_msgs = 0;
	uint8_t wr_buffer[OTM_HDMI_MAX_DDC_WRITE_SIZE];

	/* Use one i2c message to write and two to read as some
	 * monitors don't handle two write messages properly
	*/
	if (read) {
		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = 1,
		msgs[0].buf    = &offset,

		msgs[1].addr   = i2c_addr,
		msgs[1].flags  = ((read) ? I2C_M_RD : 0),
		msgs[1].len    = size,
		msgs[1].buf    = buffer,

		num_of_msgs = 2;
	} else {
		BUG_ON(size + 1 > OTM_HDMI_MAX_DDC_WRITE_SIZE);

		wr_buffer[0] = offset;
		memcpy(&wr_buffer[1], buffer, size);

		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = size + 1,
		msgs[0].buf    = wr_buffer,

		num_of_msgs = 1;
	}

	if (adapter != NULL && i2c_transfer(adapter, msgs, num_of_msgs) ==
								num_of_msgs)
		return 1;

	return 0;
}
#endif

#define android_hdmi_connector_funcs mdfld_hdmi_connector_funcs
#define android_hdmi_connector_helper_funcs mdfld_hdmi_connector_helper_funcs
#define android_hdmi_enc_helper_funcs mdfld_hdmi_helper_funcs
#define android_hdmi_enc_funcs psb_intel_lvds_enc_funcs

/**
 * This function initializes the hdmi driver called during bootup
 * @dev		: handle to drm_device
 * @mode_dev	: device mode
 *
 * Returns nothing
 *
 * This function initializes the hdmi driver called during bootup
 * which includes initializing drm_connector, drm_encoder, hdmi audio
 * and msic and collects all information reqd in hdmi private.
 */
void android_hdmi_driver_init(struct drm_device *dev,
				    void *mode_dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;
	struct psb_intel_output *psb_intel_output;
	struct drm_connector *connector;
	struct drm_encoder *encoder;

	pr_debug("%s E", __func__);

	psb_intel_output = kzalloc(sizeof(struct psb_intel_output), GFP_KERNEL);
	if (!psb_intel_output)
		return;

	psb_intel_output->mode_dev = mode_dev;
	connector = &psb_intel_output->base;
	encoder = &psb_intel_output->enc;
	drm_connector_init(dev, &psb_intel_output->base,
			   &android_hdmi_connector_funcs,
			   DRM_MODE_CONNECTOR_DVID);

	drm_encoder_init(dev, &psb_intel_output->enc, &android_hdmi_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	drm_mode_connector_attach_encoder(&psb_intel_output->base,
					  &psb_intel_output->enc);
	psb_intel_output->type = INTEL_OUTPUT_HDMI;

	psb_intel_output->dev_priv = hdmi_priv;

	drm_encoder_helper_add(encoder, &android_hdmi_enc_helper_funcs);
	drm_connector_helper_add(connector,
				 &android_hdmi_connector_helper_funcs);

	drm_connector_attach_property(connector,
					dev->mode_config.scaling_mode_property,
					DRM_MODE_SCALE_ASPECT);

	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	/* Enable polling */
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	/* TODO: remove this once all code moved into OTM */
	psb_intel_output->hdmi_i2c_adapter =
				i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);

	if (psb_intel_output->hdmi_i2c_adapter)
		pr_debug("Enter mdfld_hdmi_init, i2c_adapter is availabe.\n");
	else
		printk(KERN_ALERT "No ddc adapter available!\n");
	hdmi_priv->hdmi_i2c_adapter = psb_intel_output->hdmi_i2c_adapter;
#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_init(hdmi_priv->context, &hdmi_ddc_read_write);
#endif
	mdfld_hdmi_audio_init(hdmi_priv);
	mdfld_msic_init(hdmi_priv);

	pr_debug("%s X", __func__);
}
/**
 * This function setups the interrupt handler for hotplug
 * @dev		: handle to drm_device
 *
 * Returns nothing
 *
 * This function enables the interrupt handler to handle
 * incoming Hot plug events and prints an error message
 * incase of failure to initialize the hotplug interrupt
 * request.
 * The hotplug interrupt should be enabled after the
 * psb_fbdev_init (after all drm_ object is ready)
 */
void android_hdmi_enable_hotplug(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;

	/* Drm is ready for incoming HPD now */
	if (otm_hdmi_setup_irq(hdmi_priv->context, dev->pdev,
				&android_hdmi_irq_callback,
				(void *)hdmi_priv)) {
		pr_err("failed to initialize hdmi HPD IRQ\n");
		return;
	}
}

/**
 * IRQ interrupt bottomhalf handler callback.
 * @irq		: IRQ number
 * @data		: hdmi_priv data
 *
 * Returns IRQ_HANDLED when the interrupt has been handled.
 * IRQ interrupt bottomhalf handler callback.
 * This callback will be called for hdmi plug/unplug interrupts.
 */
static irqreturn_t android_hdmi_irq_callback(int irq, void *data)
{
	pr_debug("%s: IRQ Interrupt callback", __func__);

	return __hdmi_irq_handler_bottomhalf(data);
}

/**
 * This function sets the hdmi driver during bootup
 * @dev		: handle to drm_device
 *
 * Returns nothing
 *
 * This function is called from psb driver to setup the
 * hdmi driver. Called only once during boot-up of the system
 */
void android_hdmi_driver_setup(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv;
	int ret;

	pr_debug("%s E", __func__);

	/* HDMI private data */
	hdmi_priv = kzalloc(sizeof(struct android_hdmi_priv), GFP_KERNEL);
	if (!hdmi_priv) {
		pr_err("failed to allocate memory");
		goto out;
	}

	pr_debug("%s: Initialize the HDMI device", __func__);
	/* Initialize the HDMI context */
	if (otm_hdmi_device_init(&(hdmi_priv->context), dev->pdev)) {
		pr_err("failed to initialize hdmi device\n");
		goto free;
	}

	hdmi_priv->dev = dev;

	/* TODO: No need to expose these values to outside.
	 * keeping for now as oktl psb files are referring these.
	 * has to be removed when cleaning up the oktl psb files.
	 * medfield psb files don't need this, hence disabling
	 */
	/* hdmi_priv->regs = ctx->io_address; */

	/*FIXME: May need to get this somewhere,
	 * but CG code seems hard coded it
	 */
	hdmi_priv->hdmib_reg = HDMIB_CONTROL;
	hdmi_priv->has_hdmi_sink = false;
	/* TODO: get this from get/set attribute table.
	 */
	hdmi_priv->monitor_type = 1;/* MONITOR_TYPE_HDMI */
	/* TODO: set this to false for oaktrail */
	/* TODO: get this from get/set attribute table.
	 */
	hdmi_priv->is_hdcp_supported = true;

	dev_priv->hdmi_priv = (void *)hdmi_priv;
	dev_priv->hdmi_present = 1;

	pr_debug("%s: Register switch class devices", __func__);
	/* Register as a switch class device */
	g_switch_hdmi_dev.name = SWITCH_DEV_HDMI_NAME;
	ret = switch_dev_register(&g_switch_hdmi_dev);
	if (ret) {
		pr_debug("Failed to register switch class device %s (%d)",
				SWITCH_DEV_HDMI_NAME, ret);
		goto free;
	}

	g_switch_dvi_dev.name = SWITCH_DEV_DVI_NAME;
	ret = switch_dev_register(&g_switch_dvi_dev);
	if (ret) {
		pr_debug("Failed to register switch class device %s (%d)",
				SWITCH_DEV_DVI_NAME, ret);
		goto free;
	}

#ifdef OTM_HDMI_FIXME
	android_hdmi_audio_control(hdmi_priv->context, false);
	 /* request io port region for audio configuration */
	res = request_region(WPT_IOBAR_INDEX_REGISTER,
			WPT_IOBAR_DATA_REGISTER - WPT_IOBAR_INDEX_REGISTER + 1,
			"OKTLHDMI");
	if (res == NULL) {
		pr_err("Failed to allocate io port region\n");
		goto free;
	}
#endif
	pr_debug("%s X", __func__);
	return;
free:
	kfree(hdmi_priv);
out:
	dev_priv->hdmi_present = 0;
	return;
}

/* structure for hdmi cmdline module
 * don't upstream the code
 */
typedef struct {
	int hdisplay, vdisplay;
	int refresh;
	int refresh_specified;
	int vic;
	int vic_specified;
	int specified; /* 1: cmdline_mode is set */
} otm_cmdline_mode;

static otm_cmdline_mode cmdline_mode = { 0, 0, 0, 0, 0, 0, 0 };

/**
 * This function is used by external tools to force modes
 * @cmdoption		: cmd line option parameter
 *
 * Returns -1 on error 0 on success -2 on invalid output
 * This function gets the input parameters mentioned and sets the
 * driver to the mentioned mode. These utility functions are
 * for validating the various modes and useful for compliance
 * testing as well as easy debug
 */
int otm_cmdline_parse_option(char *cmdoption)
{
	int ret = 0;
	int namelen = 0;
	int i;
	int v_spec = 0;
	char *name;
	if (NULL == cmdoption)
		return -1;

	cmdline_mode.specified = 0;
	cmdline_mode.refresh_specified = 0;
	cmdline_mode.vic_specified = 0;

	name = cmdoption;
	namelen = strlen(name);
	for (i = namelen-1; i >= 0; i--) {
		switch (name[i]) {
		case '@':
			namelen = i;
			cmdline_mode.refresh =
				simple_strtol(&name[i+1], NULL, 10);
			cmdline_mode.refresh_specified = 1;
			break;
		case 'x':
		case 'X':
			cmdline_mode.vdisplay =
				simple_strtol(&name[i+1], NULL, 10);
			v_spec = 1;
			break;
		case '0' ... '9':
			break;
		default:
			/* invalid input */
			return -2;
		}
	}

	if ((i < 0) && (1 == v_spec))
		cmdline_mode.hdisplay = simple_strtol(name, NULL, 10);

	cmdline_mode.specified = 1;
	return ret;
}
EXPORT_SYMBOL_GPL(otm_cmdline_parse_option);

/**
 * This function is used by external tools to force the vic#
 * @vic		: vic number
 *
 * Returns 0 on success and -1 on invalid input vic
 * This function gets the input parameters mentioned and sets the
 * driver to the mentioned vic number. These utility functions are
 * for validating the various modes and useful for compliance
 * testing as well as easy debug
 */
int otm_cmdline_set_vic_option(int vic)
{
	int i = 0;

	cmdline_mode.specified = 0;
	cmdline_mode.refresh_specified = 0;
	cmdline_mode.vic_specified = 0;

	for (i = 0; i < ARRAY_SIZE(vic_formats); i++) {
		if (vic == vic_formats[i].vic) {
			cmdline_mode.refresh = vic_formats[i].vrefr;
			cmdline_mode.hdisplay = vic_formats[i].width;
			cmdline_mode.vdisplay = vic_formats[i].height;
			cmdline_mode.vic = vic;
			cmdline_mode.specified = 1;
			cmdline_mode.refresh_specified = 1;
			cmdline_mode.vic_specified = 1;
			return 0;
		}
	}

	printk(KERN_INFO "HDMI cmdline: Unsupported VIC(%d) specified\n", vic);
	return -1;
}
EXPORT_SYMBOL_GPL(otm_cmdline_set_vic_option);

/**
 * This function is used by tools to print the cmdline options
 *
 * Returns nothing
 * This function is used by external tools to print
 * the cmdline options passed through tools
 */
void otm_print_cmdline_option(void)
{
	if (1 == cmdline_mode.specified) {
		if (1 == cmdline_mode.vic_specified)
			printk(KERN_INFO "HDMI cmdline option: %dx%d@%d (%d)\n",
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh,
				cmdline_mode.vic);
		else if (1 == cmdline_mode.refresh_specified)
			printk(KERN_INFO "HDMI cmdline option: %dx%d@%d\n",
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh);
		else
			printk(KERN_INFO "HDMI cmdline option: %dx%d\n",
				cmdline_mode.hdisplay, cmdline_mode.vdisplay);
	} else
		printk(KERN_INFO "HDMI cmdline option is not set\n");
}
EXPORT_SYMBOL_GPL(otm_print_cmdline_option);

/**
 * DRM connector helper routine.
 * @connector	: drm_connector handle
 * @mode		: drm_display_mode handle
 *
 * Returns integer values which tell whether the hdmi mode
 * is valid or not
 * MODE_CLOCK_LOW - mode clock less than min pixel clock value
 * MODE_CLOCK_HIGH - mode clock greater than min pixel clock value
 * MODE_BAD - mode values are incorrect
 * MODE_OK - mode values are correct
 * MODE_NO_DBLESCAN - double scan mode not supported
 * MODE_NO_INTERLACE - interlace mode not supported
 * This is the DRM connector helper routine
 */
int android_hdmi_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	unsigned int pc_min, pc_max;

	pr_debug("display info. hdisplay = %d, vdisplay = %d, clock = %d.\n",
			mode->hdisplay, mode->vdisplay, mode->clock);

	/* Restricting modes within the supported pixel clock */
	if (OTM_HDMI_SUCCESS == otm_hdmi_get_pixel_clock_range(
					&pc_min, &pc_max)) {
		if (mode->clock < pc_min) {
			pr_debug("pruned mode %dx%d@%d.\n",
				mode->hdisplay,
				mode->vdisplay,
				mode->clock);
			return MODE_CLOCK_LOW;
		}
		if (mode->clock > pc_max) {
			pr_debug("pruned mode %dx%d@%d.\n",
				mode->hdisplay,
				mode->vdisplay,
				mode->clock);
			return MODE_CLOCK_HIGH;
		}
	}

#ifdef OTM_HDMI_UNIT_TEST
	/* if cmdline_mode is set, prune all other modes.*/
	if (1 == cmdline_mode.specified) {
		if ((cmdline_mode.hdisplay != mode->hdisplay) ||
			(cmdline_mode.vdisplay != mode->vdisplay) ||
			((1 == cmdline_mode.refresh_specified) &&
			(cmdline_mode.refresh !=
			calculate_refresh_rate(mode)))) {
			return MODE_BAD;
		}
	}
#endif

	if (mode->type == DRM_MODE_TYPE_USERDEF)
		return MODE_OK;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	return MODE_OK;
}

/**
 * This function maps the timings to drm_display_mode
 * @timings	: This holds the timings information
 * @dev		: drm_device handle
 *
 * Returns the mapped drm_display_mode
 * This function maps the timings in EDID information
 * to drm_display_mode and returns the same
 */
static struct drm_display_mode
*android_hdmi_get_drm_mode_from_pdt(const otm_hdmi_timing_t *timings,
				    struct drm_device *dev)
{
	struct drm_display_mode *mode;
	int i;
	static const struct {
	int w, h;
	} cea_interlaced[7] = {
		{ 1920, 1080 },
		{  720,  480 },
		{ 1440,  480 },
		{ 2880,  480 },
		{  720,  576 },
		{ 1440,  576 },
		{ 2880,  576 },
	};

	if (timings == NULL || dev == NULL)
		return NULL;

	mode = drm_mode_create(dev);
	if (mode == NULL)
		return NULL;

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->clock = timings->dclk;

	mode->hdisplay = timings->width;
	mode->hsync_start = timings->hsync_start;
	mode->hsync_end = timings->hsync_end;
	mode->htotal = timings->htotal;

	mode->vdisplay = timings->height;
	mode->vsync_start = timings->vsync_start;
	mode->vsync_end = timings->vsync_end;
	mode->vtotal = timings->vtotal;

	if (timings->mode_info_flags & PD_SCAN_INTERLACE) {

		mode->flags |= DRM_MODE_FLAG_INTERLACE;

		for (i = 0; i < ARRAY_SIZE(cea_interlaced); i++) {
			if ((mode->hdisplay == cea_interlaced[i].w) &&
			    (mode->vdisplay == cea_interlaced[i].h / 2)) {
				mode->vdisplay *= 2;
				mode->vsync_start *= 2;
				mode->vsync_end *= 2;
				mode->vtotal *= 2;
				mode->vtotal |= 1;
			}
		}
	}

	drm_mode_set_name(mode);

	mode->flags |= (timings->mode_info_flags & PD_HSYNC_HIGH) ?
		DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;
	mode->flags |= (timings->mode_info_flags & PD_VSYNC_HIGH) ?
		DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

	return mode;
}

/**
 * This function adds the cea modes in block 1 of EDID
 * @connector	: handle to drm_connector
 * @edid	: holds edid information
 *
 * Returns the number of modes added
 */
static int android_hdmi_add_cea_edid_modes(struct drm_connector *connector,
					   struct edid *edid)
{
	struct drm_display_mode *newmode = NULL;
	int count = 0, i = 0, ret_count = 0;
	u8 *default_edid = (u8 *)edid;
	static otm_hdmi_timing_t pdt[30];

	if (connector == NULL || edid == NULL)
		return 0;

	for (i = 1; i <= default_edid[0x7e]; i++) {
		u8 *ext = default_edid + (i * EDID_LENGTH);
		switch (*ext) {
		case CEA_EXT:
			count += otm_hdmi_timing_from_cea_modes(ext,
								&pdt[count]);
			break;
		default:
			break;
		}
	}

	/* Do Mapping from PDT to drm_display_mode */
	for (i = 0; i < count; i++) {
		newmode = android_hdmi_get_drm_mode_from_pdt(&pdt[i],
							     connector->dev);
		if (!newmode)
			continue;
		drm_mode_probed_add(connector, newmode);
		ret_count++;
	}

	return ret_count;
}

#ifdef OTM_HDMI_UNIT_TEST
static bool android_hdmi_probed_mode_exists(
				struct drm_connector *connector,
				int hdisplay, int vdisplay, int vrefresh)
{
	struct drm_display_mode *mode, *t;
	if (!connector || hdisplay < 0 || vdisplay < 0 || vrefresh < 0)
		goto exit;

	/* loop through all probed modes to match */
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		if (mode->hdisplay == hdisplay &&
			mode->vdisplay == vdisplay &&
			vrefresh == drm_mode_vrefresh(mode)) {
			return true;
		}
	}
exit:
	return false;
}

/**
 * This function adds the edid information from cmdline
 * @context	: handle hdmi_context
 * @connector	: handle drm_connector
 * @hdisplay	: width
 * @vdisplay	: height
 * @vrefresh	: refresh rate
 *
 * Returns true if mode added, false if not added
 * This function is used to set the user requested mode
 * into the mode list
 */
static bool android_hdmi_add_noedid_mode(
				void *context,
				struct drm_connector *connector,
				int hdisplay, int vdisplay, int vrefresh)
{
	struct drm_display_mode *newmode = NULL;
	const otm_hdmi_timing_t *pdt = NULL;

	if (!context || !connector || hdisplay < 0 ||
			vdisplay < 0 || vrefresh < 0)
		goto exit;

	/* get mode timings */
	pdt = otm_hdmi_get_mode_timings(context, hdisplay, vdisplay, vrefresh);
	if (!pdt)
		goto exit;

	/* add mode */
	newmode = android_hdmi_get_drm_mode_from_pdt(pdt, connector->dev);
	if (newmode) {
		drm_mode_probed_add(connector, newmode);
		return true;
	}
exit:
	return false;
}
#endif

/**
 * Calculate refresh rate from mode
 * @mode	: handle to drm_display_mode
 *
 * Returns the calculated refresh rate
 */
static int calculate_refresh_rate(struct drm_display_mode *mode)
{
	int refresh_rate = 0;

	if (!mode)
		return refresh_rate;

	refresh_rate = (((mode->flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 1) *
			mode->clock * 1000) /
			(mode->htotal * mode->vtotal);

	return refresh_rate;
}

/**
 * DRM get modes helper routine
 * @connector	: handle to drm_connector
 *
 * Returns the number of modes added
 * This is a helper routines for DRM get modes.
 * This function gets the edid information from the external sink
 * device using i2c when connected and parses the edid information
 * obtained and adds the modes to connector list
 * If sink device is not connected, then static edid timings are
 * used and those modes are added to the connector list
 */
int android_hdmi_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;
	struct edid *edid = NULL;
	/* Edid address in HDMI context */
	struct edid *ctx_edid = NULL;
	struct drm_display_mode *mode, *t;
	int i = 0, j = 0, ret = 0;
	int refresh_rate = 0;
	int pref_mode_found = -1;
	struct i2c_adapter *adapter = NULL;

	debug_modes_count = 0;
	pr_debug("%s E\n", __func__);

	/* Lazy edid read feature, which can save I2C transactions largely.
	 * Basically, HPD will do edid read and store to HDMI context.
	 * Therefore, get modes should read edid with the condition
	 * whether the edid is ready in HDP or not in a lazy way. */
	if (edid_ready_in_hpd)
		goto edid_is_ready;

	adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);

	/* FIXME: drm_get_edid cause the system hung at DV1 boot up */
	/* Read edid blocks from i2c device */
	if (NULL != adapter)
		edid = (struct edid *)drm_get_edid(connector, adapter);

	if (edid == NULL) {
		pr_err("%s Edid Read failed -use default edid", __func__);
		/* OTM_HDMI_FIXME: this should provide by OTM */
		edid = (struct edid *)default_edid;
	} else
		pr_debug("Edid Read Done in get modes\n");

	/* Store raw edid into HDMI context */
	otm_hdmi_set_raw_edid(hdmi_priv->context, (char *)edid);

	/* Release edid */
	if (edid && ((unsigned char *)edid != (unsigned char *)default_edid))
		kfree(edid);

edid_is_ready:
	/* Get the edid stored in HDMI context */
	otm_hdmi_get_raw_edid(hdmi_priv->context, (char **)&ctx_edid);

	/* Parse the edid */
	otm_hdmi_edid_parse(hdmi_priv->context, OTM_HDMI_USE_EDID_REAL);

	/* Add modes into DRM mode list */
	drm_mode_connector_update_edid_property(connector, ctx_edid);
	ret = drm_add_edid_modes(connector, ctx_edid);
	ret += android_hdmi_add_cea_edid_modes(connector, ctx_edid);

#ifdef OTM_HDMI_UNIT_TEST
	if (1 == cmdline_mode.specified) {
		/* Add cmdline mode if it does not exist in EDID */
		if (!android_hdmi_probed_mode_exists(connector,
			cmdline_mode.hdisplay,
			cmdline_mode.vdisplay,
			cmdline_mode.refresh))
			if (android_hdmi_add_noedid_mode(
				hdmi_priv->context,
				connector,
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh))
				ret++;
	}
#endif
	connector->display_info.raw_edid = NULL;
	/* TODO: MUST REVERT
	 * monitor_type is being used by mdfld_hdmi_set_property
	 * to switch state between HDMI & DVI
	 * That mechnism will be changed to use get attribute
	 * and at that time this code must be removed */
	if (otm_hdmi_is_monitor_hdmi(hdmi_priv->context))
		hdmi_priv->monitor_type = 1;/* MONITOR_TYPE_HDMI */
	else
		hdmi_priv->monitor_type = 2;/* MONITOR_TYPE_DVI */

	j = 0;
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		refresh_rate = calculate_refresh_rate(mode);
		pr_debug("Mode %02d: %s %dHz\t Clk: %dKHz H/V: %c,%c"
			"flags: 0x%08x",
			j, mode->name, refresh_rate, mode->clock,
			(mode->flags & DRM_MODE_FLAG_PHSYNC) ? '+' : '-',
			(mode->flags & DRM_MODE_FLAG_PVSYNC) ? '+' : '-',
			mode->flags);

	if (debug_modes_count < DEBUG_MODES) {
		strncpy(arr_modes[debug_modes_count].name, mode->name,
			strlen(mode->name));
		arr_modes[debug_modes_count].name[strlen(mode->name)] = '\0';
		arr_modes[debug_modes_count].frq = refresh_rate;
		arr_modes[debug_modes_count].clk = mode->clock;
		debug_modes_count++;
	} else {
		pr_err("Increase size of DEBUG_MODES, some modes not"
			 " listed in report_edid.sh\n");
	}

#ifdef OTM_HDMI_FIXME
		/*
		 * prune modes that don't have proper stall values
		 */
		if (otm_hdmi_is_monitor_hdmi(hdmi_priv->context) &&
			(get_stall_value(mode) == -1)) {
			pr_debug("%s: mode %dx%d@%dHz don't have stall"
				" value configured\n", __func__,
				mode->hdisplay,	mode->vdisplay,
				refresh_rate);
			i++;
			drm_mode_remove(connector, mode);
		}
#endif
		j++;
	}

	/* choose a preferred mode and set the mode type accordingly */
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		/* check whether the display has support for 720P.
		 * 720P is the minimum requirement expected from
		 * external display.
		 * (extend this if condition to set other modes as preferred).
		 */
		refresh_rate = calculate_refresh_rate(mode);
		if (otm_hdmi_is_preferred_mode(mode->hdisplay, mode->vdisplay,
						refresh_rate)) {
			pr_debug("External display has %dx%d support\n",
				mode->hdisplay, mode->vdisplay);
			mode->type |= DRM_MODE_TYPE_PREFERRED;
			pref_mode_found = 1;
			break;
		}
	}

	/* clear any other preferred modes*/
	if (pref_mode_found == 1) {
		list_for_each_entry_safe(mode, t, &connector->probed_modes,
					 head) {
			refresh_rate = calculate_refresh_rate(mode);
			if (otm_hdmi_is_preferred_mode(mode->hdisplay,
						       mode->vdisplay,
						       refresh_rate))
				continue;
			mode->type &= ~DRM_MODE_TYPE_PREFERRED;
		}
	}

	pr_debug("%s X (%d)", __func__, (ret - i));

	return ret - i;
}

/**
 * helper function to print the display mode details.
 * @mode:		drm display mode to print
 *
 * Returns:	none.
 */
static void __android_hdmi_dump_crtc_mode(struct drm_display_mode *mode)
{
	if (mode == NULL)
		return;

	pr_debug("hdisplay = %d\n", mode->hdisplay);
	pr_debug("vdisplay = %d\n", mode->vdisplay);
	pr_debug("hsync_start = %d\n", mode->hsync_start);
	pr_debug("hsync_end = %d\n", mode->hsync_end);
	pr_debug("htotal = %d\n", mode->htotal);
	pr_debug("vsync_start = %d\n", mode->vsync_start);
	pr_debug("vsync_end = %d\n", mode->vsync_end);
	pr_debug("vtotal = %d\n", mode->vtotal);
	pr_debug("clock = %d\n", mode->clock);
	pr_debug("flags = 0x%x\n", mode->flags);
}

/* Derive 59.94Hz dot clock from 60Hz dot clock
 */
static int __f5994(int dotclock)
{
	return DIV_ROUND_UP(dotclock*1000, 1001);
}

/**
 * helper function to convert drm_display_mode to otm_hdmi_timing.
 * @otm_mode:		otm hdmi mode to be populated
 * @drm_mode:		drm_display_mode
 *
 * Returns:	none.
 */
static void __android_hdmi_drm_mode_to_otm_timing(otm_hdmi_timing_t *otm_mode,
				struct drm_display_mode *drm_mode)
{
	uint8_t i = 0;

	if (otm_mode == NULL || drm_mode == NULL)
		return;

	otm_mode->width			= (unsigned short)
						drm_mode->crtc_hdisplay;
	otm_mode->height		= (unsigned short)
						drm_mode->crtc_vdisplay;
	otm_mode->dclk			= (unsigned long)
						drm_mode->clock;
	otm_mode->htotal		= (unsigned short)
						drm_mode->crtc_htotal;
	otm_mode->hblank_start		= (unsigned short)
						drm_mode->crtc_hblank_start;
	otm_mode->hblank_end		= (unsigned short)
						drm_mode->crtc_hblank_end;
	otm_mode->hsync_start		= (unsigned short)
						drm_mode->crtc_hsync_start;
	otm_mode->hsync_end		= (unsigned short)
						drm_mode->crtc_hsync_end;
	otm_mode->vtotal		= (unsigned short)
						drm_mode->crtc_vtotal;
	otm_mode->vblank_start		= (unsigned short)
						drm_mode->crtc_vblank_start;
	otm_mode->vblank_end		= (unsigned short)
						drm_mode->crtc_vblank_end;
	otm_mode->vsync_start		= (unsigned short)
						drm_mode->crtc_vsync_start;
	otm_mode->vsync_end		= (unsigned short)
						drm_mode->crtc_vsync_end;
	otm_mode->mode_info_flags	= (unsigned long)
						drm_mode->flags;

	/* TODO: metadata should be populated using table from mode_info.c */
	otm_mode->metadata = 0;
	for (i = 0; i < ARRAY_SIZE(vic_formats); i++) {
		if (otm_mode->width == vic_formats[i].width &&
		   otm_mode->height == vic_formats[i].height &&
		   otm_mode->htotal == vic_formats[i].htotal &&
		   otm_mode->vtotal == vic_formats[i].vtotal &&
		   (otm_mode->dclk == vic_formats[i].dclk ||
		    otm_mode->dclk == __f5994(vic_formats[i].dclk))) {
			if (1 == cmdline_mode.specified &&
			    1 == cmdline_mode.vic_specified) {
				if (cmdline_mode.vic == vic_formats[i].vic) {
					otm_mode->metadata = cmdline_mode.vic;
					break;
				}
				/* else continue */
			} else {
				otm_mode->metadata = vic_formats[i].vic;
				break;
			}
		}
	}
}

/* TODO: get these values depending on the platform */
#define OTM_HDMI_MDFLD_MIPI_NATIVE_HDISPLAY 1280
#define OTM_HDMI_MDFLD_MIPI_NATIVE_VDISPLAY 800
#define OTM_HDMI_MDFLD_PFIT_WIDTH_LIMIT 1024

/**
 * crtc mode set for hdmi pipe.
 * @crtc		: crtc
 * @mode		:mode requested
 * @adjusted_mode:adjusted mode
 * @x		:x value
 * @y		:y value
 * @old_fb	: old frame buffer values for flushing old planes
 *
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */
int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb)
{
	struct drm_device *dev;
	struct psb_intel_crtc *psb_intel_crtc;
	struct drm_psb_private *dev_priv;
	struct drm_framebuffer *fb;
	struct android_hdmi_priv *hdmi_priv;
	struct drm_mode_config *mode_config;
#ifdef MFLD_HDMI_PR3
	struct psb_intel_output *psb_intel_output = NULL;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	uint64_t scalingType = DRM_MODE_SCALE_CENTER;
#endif
	int pipe;
	otm_hdmi_timing_t otm_mode, otm_adjusted_mode;
	uint32_t clock_khz;
	int fb_width, fb_height;
#if !defined(MFLD_HDMI_PR3) && !defined(MFLD_HDMI_DV1)
	u32 width_align, pipebstride;
#endif
	pr_debug("%s E", __func__);

	if (crtc == NULL || mode == NULL || adjusted_mode == NULL)
		return -EINVAL;

	/* get handles for required data */
	dev = crtc->dev;
	psb_intel_crtc = to_psb_intel_crtc(crtc);
	pipe = psb_intel_crtc->pipe;
	dev_priv = dev->dev_private;
	fb = crtc->fb;
	fb_width = fb->width;
	fb_height = fb->height;
	hdmi_priv = dev_priv->hdmi_priv;
	mode_config = &dev->mode_config;

	if (pipe != 1) {
		pr_err("%s: Invalid pipe %d", __func__, pipe);
		return 0;
	}

	pr_debug("%s mode info:\n", __func__);
	__android_hdmi_dump_crtc_mode(mode);
	pr_debug("%s adjusted mode info:\n", __func__);
	__android_hdmi_dump_crtc_mode(adjusted_mode);

	memcpy(&psb_intel_crtc->saved_mode, mode,
				sizeof(struct drm_display_mode));
	memcpy(&psb_intel_crtc->saved_adjusted_mode, adjusted_mode,
				sizeof(struct drm_display_mode));

	__android_hdmi_drm_mode_to_otm_timing(&otm_mode, mode);
	__android_hdmi_drm_mode_to_otm_timing(&otm_adjusted_mode,
						adjusted_mode);

#ifdef MFLD_HDMI_PR3
	list_for_each_entry(connector, &mode_config->connector_list, head) {
		if (!connector)
			continue;
		encoder = connector->encoder;
		if (!encoder)
			continue;
		if (encoder->crtc != crtc)
			continue;
		psb_intel_output = to_psb_intel_output(connector);
	}

	if (psb_intel_output)
		drm_connector_property_get_value(&psb_intel_output->base,
			dev->mode_config.scaling_mode_property, &scalingType);

	psb_intel_crtc->scaling_type = scalingType;
#endif
	/* Disable the VGA plane that we never use */
	REG_WRITE(VGACNTRL, VGA_DISP_DISABLE);

	/* Disable the panel fitter if it was on our pipe */
	/* TODO: do this down the layers. */
	if (psb_intel_panel_fitter_pipe(dev) == pipe)
		REG_WRITE(PFIT_CONTROL, 0);

	/* Flush the plane changes */
	{
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc_funcs->mode_set_base(crtc, x, y, old_fb);
	}

#if !defined(MFLD_HDMI_PR3) && !defined(MFLD_HDMI_DV1)
	if (OTM_HDMI_MDFLD_MIPI_NATIVE_HDISPLAY == fb->width
		&& otm_adjusted_mode.width < OTM_HDMI_MDFLD_PFIT_WIDTH_LIMIT) {
		/* setup HDMI buffers for SW downscaling.
		 * The scenario where this SW downscaling will be executed has
		 * a very low probability. The likely scenario is, where the
		 * external display can only support modes less than
		 * 1024x768, which has very less probability, as most of the
		 * monitors has atleast 1024x768 support.
		 */
		android_hdmi_setup_hdmibuffers(dev, otm_adjusted_mode.width,
			otm_adjusted_mode.height, 2,
			fb->bits_per_pixel, fb->width, fb->height);

		width_align = (otm_adjusted_mode.width + 31) & ~31;
		pipebstride = 4 * width_align;
		REG_WRITE(PSB_DSPSTRIDE(PSB_PIPE_B), pipebstride);
		fb_width = otm_adjusted_mode.width;
		fb_height = otm_adjusted_mode.height;
	}
#endif

	/*
	 * TODO: clock_khz: is used in mdfld_hdmi_audio.c
	 * while returning hdmi audio capabilities.
	 * remove this field from mode_set interfaces and move
	 * into audio interfaces of OTM when implemented
	 */
	if (otm_hdmi_crtc_mode_set(hdmi_priv->context, &otm_mode,
				&otm_adjusted_mode, fb_width,
				fb_height, &clock_khz)) {
		pr_err("%s: failed to perform hdmi crtc mode set",
					__func__);
		return 0;
	}

	/*
	 * TODO: this field is used in mdfld_hdmi_audio.c
	 * while returning hdmi audio capabilities.
	 * Remove this and move into audio interfaces of OTM when implemented.
	 */
	dev_priv->tmds_clock_khz = clock_khz;

#ifdef MFLD_HDMI_PR3
	/*
	 * SW workaround for Compliance 7-29 ACR test on 576p@50
	 * use the nominal pixel clock, instead of the actual clock
	 */
	if (otm_adjusted_mode.metadata == 17 ||
			otm_adjusted_mode.metadata == 18)
		dev_priv->tmds_clock_khz = otm_adjusted_mode.dclk;
#endif

	psb_intel_wait_for_vblank(dev);

	pr_debug("%s X", __func__);
	return 0;
}

/**
 * encoder mode set for hdmi pipe.
 * @encoder:		hdmi encoder
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	none.
 */
void android_hdmi_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev;
	struct drm_crtc *crtc;
	struct android_hdmi_priv *hdmi_priv;
	struct drm_psb_private *dev_priv;
#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
	void *had_pvt_data;
	enum had_event_type event_type = HAD_EVENT_MODE_CHANGING;
#endif
	otm_hdmi_timing_t otm_mode, otm_adjusted_mode;

	pr_debug("%s E", __func__);

	if (encoder == NULL || mode == NULL || adjusted_mode == NULL)
		return;

	/* get handles for required data */
	dev = encoder->dev;
	crtc = encoder->crtc;
	dev_priv = dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;
#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
	had_pvt_data = dev_priv->had_pvt_data;
#endif

	__android_hdmi_drm_mode_to_otm_timing(&otm_mode, mode);
	__android_hdmi_drm_mode_to_otm_timing(&otm_adjusted_mode,
						adjusted_mode);

	/*
	 * FIXME HDMI driver doesn't follow kms design correctly.
	 * Fudge the user mode {hdisplay,vdisplay} to avoid failing
	 * sanity checks, and fudge the crtc_{hdisplay,vdisplay} to
	 * fix overlay clipping.
	 */
	crtc->mode.hdisplay = crtc->mode.crtc_hdisplay = crtc->fb->width;
	crtc->mode.vdisplay = crtc->mode.crtc_vdisplay = crtc->fb->height;

	if (otm_hdmi_enc_mode_set(hdmi_priv->context, &otm_mode,
				&otm_adjusted_mode)) {
		pr_err("%s: failed to perform hdmi enc mode set",
					__func__);
		return;
	}

#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
	/* Send MODE_CHANGE event to Audio driver */
	if (dev_priv->mdfld_had_event_callbacks)
		(*dev_priv->mdfld_had_event_callbacks)(event_type,
				had_pvt_data);
#endif

#ifdef OTM_HDMI_HDCP_ENABLE
	/* enable hdcp */
	if (otm_hdmi_hdcp_enable(hdmi_priv->context))
		pr_debug("hdcp enabled");
	else
		pr_err("hdcp could not be enabled");
#endif
	return;
}

/**
 * save the register for HDMI display
 * @dev:		drm device
 *
 * Returns:	none.
 */
void android_hdmi_save_display_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	uint8_t data;
	if (NULL == dev)
		return;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return;
	/* TODO: get hpd status using get attribute */
	/* Check if monitor is attached to HDMI connector. */
	intel_scu_ipc_ioread8(MSIC_HDMI_STATUS, &data);
	otm_hdmi_save_display_registers(hdmi_priv->context,
				(data & HPD_SIGNAL_STATUS));
	return;
}

/**
 * Restore the register and enable the HDMI display
 * @dev:		drm device
 *
 * Returns:	none.
 */
void android_hdmi_restore_and_enable_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	uint8_t data;
	if (NULL == dev)
		return;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return;
	/* TODO: get hpd status using get attribute */
	/* Check if monitor is attached to HDMI connector. */
	intel_scu_ipc_ioread8(MSIC_HDMI_STATUS, &data);
	otm_hdmi_restore_and_enable_display(hdmi_priv->context,
			(data & HPD_SIGNAL_STATUS));
}

/**
 * disable HDMI display
 * @dev:		drm device
 *
 * Returns:	none.
 */
void android_disable_hdmi(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev)
		return;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return;
	otm_disable_hdmi(hdmi_priv->context);
	return;
}

/**
 * hdmi helper function to detect whether hdmi/dvi is connected or not.
 * @connector:	hdmi connector
 *
 * Returns:	connector_status_connected if hdmi/dvi is connected.
 *		connector_status_disconnected if hdmi/dvi is not connected.
 */
enum drm_connector_status android_hdmi_detect(struct drm_connector *connector)
{
	struct drm_device *dev = NULL;
	struct drm_psb_private *dev_priv = NULL;
	struct android_hdmi_priv *hdmi_priv = NULL;
	u8 data = 0;

	struct i2c_adapter *adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);

	if (NULL == connector || NULL == adapter)
		return connector_status_disconnected;

	dev = connector->dev;
	dev_priv = (struct drm_psb_private *)dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;

	/* Check if monitor is attached to HDMI connector. */
	intel_scu_ipc_ioread8(MSIC_HDMI_STATUS, &data);
	pr_debug("%s: HPD connected data = 0x%x.\n", __func__, data);

#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_set_hpd_state(hdmi_priv->context,
				(data & HPD_SIGNAL_STATUS));
#endif

	if (data & HPD_SIGNAL_STATUS) {
		/*
		 * Handle Hot-plug of HDMI. Display B would be power-gated
		 * by ospm_post_init if HDMI is not detected during driver load.
		 * This will power-up Display B if HDMI is
		 * connected post driver load.
		 */
		/*
		 * If pmu_nc_set_power_state fails then accessing HW
		 * reg would result in a crash - IERR/Fabric error.
		 */
		if (pmu_nc_set_power_state(OSPM_DISPLAY_B_ISLAND,
					OSPM_ISLAND_UP, OSPM_REG_TYPE))
			BUG();

		return connector_status_connected;
	} else {
		/*
		 * Clean up the HDMI connector attached encoder, to make
		 * drm_crtc_helper_set_config() do mode setting each time,
		 * especially when plug out and plug in HDMI.
		 */
#ifdef MFLD_HDMI_PR3
		drm_helper_disable_unused_functions(dev);
#endif

#ifdef OTM_HDMI_HDCP_ENABLE
		/* TODO: HPD status should be used by HDCP through attributes */
		if (otm_hdmi_hdcp_disable(hdmi_priv->context))
			pr_debug("hdcp disabled\n");
		else
			pr_debug("failed to disable hdcp\n");
#endif
		return connector_status_disconnected;
	}
}

/**
 * hdmi helper function to manage power to the display (dpms)
 * @encoder:	hdmi encoder
 * @mode:	dpms on or off
 *
 * Returns:	none
 */
void android_hdmi_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;

	if (encoder == NULL)
		return;

	dev = encoder->dev;
	dev_priv = dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;

	/* TODO: Move entire mdfld_hdmi_dpms function into OTM */
#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_set_dpms(hdmi_priv->context, (mode == DRM_MODE_DPMS_ON));
#endif
}

/*
 *
 * Internal scripts wrapper functions.
 *
 */
/*
 * TODO: Remove this function afer EDID Parse
 *	 for established modes is implemented
 */

/* Starting this off, but all scripts/unit test helpers should move
 * to another file.
 */

#ifdef OTM_HDMI_UNIT_TEST

/**
 * test_otm_hdmi_report_edid_full() - Report current EDID information
 *
 * This routine simply dumps the EDID information
 * Returns - nothing
 */
void test_otm_hdmi_report_edid_full(void)
{
	int i = 0;
	printk(KERN_ALERT "\n*** Supported Modes ***\n");

	for (i = 0; i < debug_modes_count; i++)
		printk(KERN_ALERT "Mode %02d: %s @%dHz Clk: %dKHz\n", i,
		arr_modes[i].name, arr_modes[i].frq, arr_modes[i].clk);

	printk(KERN_ALERT "\n");
}
EXPORT_SYMBOL_GPL(test_otm_hdmi_report_edid_full);
#endif

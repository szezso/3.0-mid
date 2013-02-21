/*
 * Copyright © 2006-2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include "psb_intel_drv.h"
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_hdmi_reg.h"
#include "psb_intel_hdmi_edid.h"
#include "psb_intel_hdmi.h"
#include "mdfld_dsi_output.h"
#include "mdfld_hdmi_audio_if.h"
#include "mdfld_msic.h"
#include "mdfld_hdcp_if.h"
#include <linux/switch.h>
#include <linux/pm_runtime.h>
#include <asm/intel_scu_ipc.h>

/* Global devices for switch class used for hotplug notification */
struct switch_dev g_switch_hdmi_dev;
struct switch_dev g_switch_dvi_dev;

/* FIXME_MDFLD HDMI EDID supports */

static char EDID_Samsung[EDID_LENGTH + HDMI_CEA_EDID_BLOCK_SIZE] =
{
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4C, 0x2D, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 
	0x14, 0x11, 0x01, 0x03, 0x80, 0x10, 0x09, 0x8C, 0x0A, 0xE2, 0xBD, 0xA1, 0x5B, 0x4A, 0x98, 0x24, 
	0x15, 0x47, 0x4A, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 
	0x55, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
	0xB8, 0x28, 0x55, 0x40, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x31, 
	0x3D, 0x0F, 0x2E, 0x08, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 
	0x00, 0x53, 0x41, 0x4D, 0x53, 0x55, 0x4E, 0x47, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xB3,
	0x02, 0x03, 0x1A, 0x71, 0x46, 0x84, 0x13, 0x05, 0x14, 0x03, 0x12, 0x23, 0x09, 0x07, 0x07, 0x83, 
	0x01, 0x00, 0x00, 0x66, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x80, 0x01, 0x1D, 0x80, 0x18, 0x71, 0x1C, 
	0x16, 0x20, 0x58, 0x2C, 0x25, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x9E, 0x01, 0x1D, 0x80, 0xD0, 
	0x72, 0x1C, 0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x9E, 0x8C, 0x0A, 
	0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x18,
	0x8C, 0x0A, 0xD0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0C, 0x40, 0x55, 0x00, 0xA0, 0x5A, 0x00, 0x00, 
	0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0
};

static char EDID_Samsung_2493HM[EDID_LENGTH] =
{
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x4c, 0x2d, 0x68, 0x03, 0x34, 0x32, 0x49, 0x4b,
	0x0c, 0x12, 0x01, 0x03, 0x0e, 0x34, 0x20, 0xa0, 0x2a, 0xef, 0x91, 0xa3, 0x54, 0x4c, 0x9b, 0x26,
	0x0f, 0x50, 0x54, 0xbf, 0xef, 0x80, 0xa9, 0x40, 0x81, 0x80, 0x81, 0x40, 0x71, 0x4f, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x28, 0x3c, 0x80, 0xa0, 0x70, 0xb0, 0x23, 0x40, 0x30, 0x20,
	0x36, 0x00, 0x06, 0x44, 0x21, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x38, 0x4b, 0x1e,
	0x51, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x53,
	0x79, 0x6e, 0x63, 0x4d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x0a, 0x20, 0x20, 0x00, 0x00, 0x00, 0xff,
	0x00, 0x48, 0x56, 0x52, 0x51, 0x33, 0x30, 0x30, 0x35, 0x35, 0x30, 0x0a, 0x20, 0x20, 0x00, 0x20
};

static char EDID_Dell[EDID_LENGTH + HDMI_CEA_EDID_BLOCK_SIZE] =
{
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x10, 0xac, 0x5d, 0xa0, 0x55, 0x36, 0x4e, 0x32,
	0x23, 0x14, 0x01, 0x03, 0x80, 0x35, 0x1e, 0x78, 0xee, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 
	0x0f, 0x50, 0x54, 0xa5, 0x4b, 0x00, 0x71, 0x4f, 0x81, 0x80, 0xd1, 0xc0, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x58, 0x31, 0x37,
	0x35, 0x52, 0x30, 0x38, 0x51, 0x32, 0x4e, 0x36, 0x55, 0x0a, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x44,
	0x45, 0x4c, 0x4c, 0x20, 0x53, 0x54, 0x32, 0x34, 0x31, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x32, 0x4c, 0x1e, 0x53, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x6d,
	0x02, 0x03, 0x1f, 0xf1, 0x4c, 0x90, 0x05, 0x04, 0x03, 0x02, 0x07, 0x16, 0x01, 0x14, 0x1f, 0x12,
	0x13, 0x23, 0x09, 0x07, 0x07, 0x65, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x83, 0x01, 0x00, 0x00, 0x02,
	0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00,
	0x1e, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16, 0x20, 0x58, 0x2c, 0x25, 0x00, 0x13, 0x2b, 0x21,
	0x00, 0x00, 0x9e, 0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28, 0x55, 0x00, 0x13,
	0x2b, 0x21, 0x00, 0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96,
	0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b
};

static char EDID_Toshiba_32RV525RZ[EDID_LENGTH + HDMI_CEA_EDID_BLOCK_SIZE] =
{
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x10, 0xac, 0x5d, 0xa0, 0x55, 0x36, 0x4e, 0x32,
	0x23, 0x14, 0x01, 0x03, 0x80, 0x35, 0x1e, 0x78, 0xee, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0xa5, 0x4b, 0x00, 0x71, 0x4f, 0x81, 0x80, 0xd1, 0xc0, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x58, 0x31, 0x37,
	0x35, 0x52, 0x30, 0x38, 0x51, 0x32, 0x4e, 0x36, 0x55, 0x0a, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x44,
	0x45, 0x4c, 0x4c, 0x20, 0x53, 0x54, 0x32, 0x34, 0x31, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x32, 0x4c, 0x1e, 0x53, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x6d,
	0x02, 0x03, 0x1f, 0xf1, 0x4c, 0x90, 0x05, 0x04, 0x03, 0x02, 0x07, 0x16, 0x01, 0x14, 0x1f, 0x12,
	0x13, 0x23, 0x09, 0x07, 0x07, 0x65, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x83, 0x01, 0x00, 0x00, 0x02,
	0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x13, 0x2b, 0x21, 0x00, 0x00,
	0x1e, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16, 0x20, 0x58, 0x2c, 0x25, 0x00, 0x13, 0x2b, 0x21,
	0x00, 0x00, 0x9e, 0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28, 0x55, 0x00, 0x13,
	0x2b, 0x21, 0x00, 0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96,
	0x00, 0x13, 0x2b, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b
};

static char EDID_Toshiba_Regza[EDID_LENGTH + HDMI_CEA_EDID_BLOCK_SIZE] =
{
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x52, 0x62, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x11, 0x01, 0x03, 0x80, 0x69, 0x3b, 0x78, 0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27, 
	0x12, 0x48, 0x4c, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 
	0x45, 0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 
	0x10, 0x3e, 0x96, 0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x54, 
	0x53, 0x42, 0x2d, 0x54, 0x56, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd, 
	0x00, 0x17, 0x3d, 0x0f, 0x44, 0x0f, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x03, 
	0x02, 0x03, 0x20, 0x77, 0x4a, 0x90, 0x05, 0x04, 0x03, 0x07, 0x02, 0x06, 0x01, 0x20, 0x22, 0x23, 
	0x09, 0x07, 0x07, 0x6c, 0x03, 0x0c, 0x00, 0x30, 0x00, 0x00, 0x1e, 0xc0, 0x2b, 0x2b, 0x33, 0x33, 
	0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28, 0x55, 0x00, 0xc4, 0x8e, 0x21, 0x00, 
	0x00, 0x1e, 0x8c, 0x0a, 0xa0, 0x14, 0x51, 0xf0, 0x16, 0x00, 0x26, 0x7c, 0x43, 0x00, 0xc4, 0x8e, 
	0x21, 0x00, 0x00, 0x98, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00, 
	0x13, 0x8e, 0x21, 0x00, 0x00, 0x18, 0x8c, 0x0a, 0xa0, 0x14, 0x51, 0xf0, 0x16, 0x00, 0x26, 0x7c, 
	0x43, 0x00, 0x13, 0x8e, 0x21, 0x00, 0x00, 0x98, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16, 0x20, 
	0x58, 0x2c, 0x25, 0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb3 
};

static struct hdmi_edid_info mdfld_hdmi_edid[] = {
	{ HDMI_EDID_INFO("SAMSUNG", EDID_Samsung) },
	{ HDMI_EDID_INFO("SAMSUNG_2493HM", EDID_Samsung_2493HM) },
	{ HDMI_EDID_INFO("DELL", EDID_Dell) },
	{ HDMI_EDID_INFO("TOSHIBA_32RV525RZ", EDID_Toshiba_32RV525RZ) },
	{ HDMI_EDID_INFO("TOSHIBA_REGZA", EDID_Toshiba_Regza) },
};

#if 1 /*FIXME_MDFLD_HDMI remove it later */	
static void mdfld_hdmi_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct psb_intel_output *output = enc_to_psb_intel_output(encoder);
	struct mid_intel_hdmi_priv *hdmi_priv = output->dev_priv;
	u32 hdmib, hdmi_phy_misc;

	PSB_DEBUG_ENTRY("\n");

	/* Set panel fitting. */
	if ((adjusted_mode->hdisplay != mode->hdisplay)
			 || (adjusted_mode->vdisplay != mode->vdisplay))
		REG_WRITE(PFIT_CONTROL, PFIT_ENABLE | PFIT_PIPE_SELECT_B);

	hdmib = REG_READ(hdmi_priv->hdmib_reg) | HDMIB_PORT_EN | HDMIB_PIPE_B_SELECT | HDMIB_NULL_PACKET;
	hdmi_phy_misc = REG_READ(HDMIPHYMISCCTL) & ~HDMI_PHY_POWER_DOWN; 

	REG_WRITE(HDMIPHYMISCCTL, hdmi_phy_misc);
	REG_WRITE(hdmi_priv->hdmib_reg, hdmib);
	REG_READ(hdmi_priv->hdmib_reg);
}

static bool mdfld_hdmi_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
       struct drm_device *dev = encoder->dev;
       struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(encoder->crtc);
       PSB_DEBUG_ENTRY("hdisplay = %d, vdisplay = %d. a_hdisplay = %d, a_vdisplay = %d.\n", mode->hdisplay, mode->vdisplay, adjusted_mode->hdisplay, adjusted_mode->vdisplay);

	/* Should never happen!! */
	if (psb_intel_crtc->pipe != 1) {
		printk(KERN_ERR
			"Only support HDMI on pipe B on MID\n");
		return false;
	}

#if 0 // MDFLD_HDMI_JLIU7_HACKS /* Debug HDMI - Can't enalbe HDMI */
#if 1 /* 720p - Adeel */
	adjusted_mode->hdisplay = 0x500;
	adjusted_mode->htotal = 0x672;
	adjusted_mode->hsync_start = 0x56e;
	adjusted_mode->hsync_end = 0x596;
	adjusted_mode->vdisplay = 0x2d0; 
	adjusted_mode->vtotal = 0x2ee;
	adjusted_mode->vsync_start = 0x2d5;
	adjusted_mode->vsync_end = 0x2da;
#endif

#if 0 /* 1080p - Brian */
	adjusted_mode->hdisplay = 0x780;
	adjusted_mode->htotal = 0x898;
	adjusted_mode->hsync_start = 0x7d8;
	adjusted_mode->hsync_end = 0x804;
	adjusted_mode->vdisplay = 0x438; 
	adjusted_mode->vtotal = 0x464;
	adjusted_mode->vsync_start = 0x43c;
	adjusted_mode->vsync_end = 0x446;
#endif
#if 0 /* 1080p - Adeel */
	adjusted_mode->hdisplay = 0x780;
	adjusted_mode->htotal = 0xabe;
	adjusted_mode->hsync_start = 0x9fe;
	adjusted_mode->hsync_end = 0xa2a;
	adjusted_mode->vdisplay = 0x438; 
	adjusted_mode->vtotal = 0x465;
	adjusted_mode->vsync_start = 0x43c;
	adjusted_mode->vsync_end = 0x441;
#endif


#if 0 /* 480p - Adeel */
	adjusted_mode->hdisplay = 0x280;
	adjusted_mode->htotal = 0x320;
	adjusted_mode->hsync_start = 0x290;
	adjusted_mode->hsync_end = 0x2f0;
	adjusted_mode->vdisplay = 0x1e0; 
	adjusted_mode->vtotal = 0x20d;
	adjusted_mode->vsync_start = 0x1ea;
	adjusted_mode->vsync_end = 0x1ec;
#endif

#if 0 /* 480p - icdk */
	adjusted_mode->hdisplay = 0x280;
	adjusted_mode->htotal = 0x35a;
	adjusted_mode->hsync_start = 0x2e0;
	adjusted_mode->hsync_end = 0x31e;
	adjusted_mode->vdisplay = 0x1e0; 
	adjusted_mode->vtotal = 0x20e;
	adjusted_mode->vsync_start = 0x1ea;
	adjusted_mode->vsync_end = 0x1ec;
#endif
#if 0 /* 720p - Adeel */
		REG_WRITE(htot_reg, 0x067104ff); 
		REG_WRITE(hblank_reg, 0x067104ff);
		REG_WRITE(hsync_reg, 0x0595056d); 
		REG_WRITE(vtot_reg, 0x02ed02cf);
		REG_WRITE(vblank_reg, 0x02ed02cf);
		REG_WRITE(vsync_reg, 0x02d902d4);
#endif
#if 0 /* 1080p - Brian */
		REG_WRITE(htot_reg, 0x0897077f); 
		REG_WRITE(hblank_reg, 0x0897077f);
		REG_WRITE(hsync_reg, 0x080307d7); 
		REG_WRITE(vtot_reg, 0x04630437);
		REG_WRITE(vblank_reg, 0x04630437);
		REG_WRITE(vsync_reg, 0x0445043b);
#endif

#if 0 /* 1080p - Adeel */
		REG_WRITE(htot_reg, 0x0abd077f); 
		REG_WRITE(hblank_reg, 0x0abd077f);
		REG_WRITE(hsync_reg, 0x0a2909fd); 
		REG_WRITE(vtot_reg, 0x04640437);
		REG_WRITE(vblank_reg, 0x04640437);
		REG_WRITE(vsync_reg, 0x0440043b);
#endif


#if 0 /* 480p - Adeel */
		REG_WRITE(htot_reg, 0x031f027f); 
		REG_WRITE(hblank_reg, 0x031f027f);
		REG_WRITE(hsync_reg, 0x02ef028f); 
		REG_WRITE(vtot_reg, 0x020c01df);
		REG_WRITE(vblank_reg, 0x020c01df);
		REG_WRITE(vsync_reg, 0x01eb01e9);
#endif

#if 0 /* 480p - icdk */
		REG_WRITE(htot_reg, 0x0359027f); 
		REG_WRITE(hblank_reg, 0x0359027f);
		REG_WRITE(hsync_reg, 0x031d02df); 
		REG_WRITE(vtot_reg, 0x020d01df);
		REG_WRITE(vblank_reg, 0x020d01df);
		REG_WRITE(vsync_reg, 0x01eb01e9);
#endif
	drm_mode_set_crtcinfo(adjusted_mode,
			CRTC_INTERLACE_HALVE_V);
#endif /* MDFLD_HDMI_JLIU7_HACKS */ /* Debug HDMI - Can't enalbe HDMI */
	return true;
}


static void mdfld_hdmi_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_output *output = enc_to_psb_intel_output(encoder);
	struct android_hdmi_priv *hdmi_priv = output->dev_priv;
	u32 hdmib, hdmi_phy_misc;

	PSB_DEBUG_ENTRY("%s\n", mode == DRM_MODE_DPMS_ON ? "on" : "off");

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				       true)) {
		PSB_DEBUG_ENTRY("%s failed to power on\n", __func__);
		return;
	}

	hdmib = REG_READ(hdmi_priv->hdmib_reg) | HDMIB_PIPE_B_SELECT;
	if (hdmi_priv->monitor_type == MONITOR_TYPE_DVI) {
		hdmib &= ~(HDMIB_NULL_PACKET | HDMI_AUDIO_ENABLE);
		REG_WRITE(VIDEO_DIP_CTL, 0x0);
		REG_WRITE(AUDIO_DIP_CTL, 0x0);
	} else
		hdmib |= (HDMIB_NULL_PACKET | HDMI_AUDIO_ENABLE);
	hdmi_phy_misc = REG_READ(HDMIPHYMISCCTL);

	if (mode != DRM_MODE_DPMS_ON) {
		/* Disabling HDMI audio Driver before powering off Phy */
		if (dev_priv->mdfld_had_event_callbacks)
			(*dev_priv->mdfld_had_event_callbacks)
			(HAD_EVENT_HOT_UNPLUG, dev_priv->had_pvt_data);
		REG_WRITE(hdmi_priv->hdmib_reg,
				hdmib & ~HDMIB_PORT_EN & ~HDMI_AUDIO_ENABLE);
		REG_WRITE(HDMIPHYMISCCTL, hdmi_phy_misc | HDMI_PHY_POWER_DOWN);
	} else {
		/* Enabling HDMI audio Driver after powering on Phy */
		REG_WRITE(HDMIPHYMISCCTL, hdmi_phy_misc & ~HDMI_PHY_POWER_DOWN);
		REG_WRITE(hdmi_priv->hdmib_reg, hdmib | HDMIB_PORT_EN);
		if (dev_priv->mdfld_had_event_callbacks)
			(*dev_priv->mdfld_had_event_callbacks)
			(HAD_EVENT_HOT_PLUG, dev_priv->had_pvt_data);
	}
	REG_READ(hdmi_priv->hdmib_reg);

	android_hdmi_dpms(encoder, mode);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
}

static void mdfld_hdmi_save(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct psb_intel_output *output = to_psb_intel_output(connector);
	struct mid_intel_hdmi_priv *hdmi_priv = output->dev_priv;

	PSB_DEBUG_ENTRY("\n");

	hdmi_priv->save_HDMIB = REG_READ(hdmi_priv->hdmib_reg);
}

static void mdfld_hdmi_restore(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct psb_intel_output *output = to_psb_intel_output(connector);
	struct mid_intel_hdmi_priv *hdmi_priv = output->dev_priv;

	PSB_DEBUG_ENTRY("\n");

	REG_WRITE(hdmi_priv->hdmib_reg, hdmi_priv->save_HDMIB);
	REG_READ(hdmi_priv->hdmib_reg);
}

/* HDMI DIP related stuff */
static int mdfld_hdmi_get_cached_edid_block(struct drm_connector *connector, uint32_t num_block, uint8_t *edid_block, uint32_t size)
{
    struct drm_display_info *displayinfo = &(connector->display_info);
    if (num_block >= MAX_EDID_BLOCKS)
    {
	DRM_ERROR("mdfld_hdmi_get_cached_edid_block() - Invalid EDID block\n");
        return 0; 
    }
    edid_block = &displayinfo->raw_edid[EDID_BLOCK_SIZE*num_block];
    return 1;
}

/////////////////////////////////////////////////////////////////////////
//    INTHDMIENCODER_CreateEELDPacket():
//    This function parses v1.3 base EDID and CEA-861b EDID Timing Extension
//    Version3 and creates EELD (Enhanced EDID Like Data) packet. This EELD data contains
//    audio configuration information and other details read EDID.This can also contain Vendor specific Data
//
/////////////////////////////////////////////////////////////////////////
static int mdfld_hdmi_create_eeld_packet(struct drm_connector *connector)
{
    struct psb_intel_output *output = to_psb_intel_output(connector);
    struct mid_intel_hdmi_priv *hdmi_priv = output->dev_priv;
    uint8_t ucEdidBlock[128];
    hdmi_eeld_t *pEEld                = NULL;
    baseedid_1_x_t *pEdid        = NULL;
    ce_edid_t *pCeEdid        = NULL;
    int dwNumOfBytes        = 0;
    int sizeOfCEADataBlock    = 0;
    uint8_t * pDataBlock        = NULL;
    edid_dtd_timing_t *pDTD    = NULL;
    uint8_t *pData            = NULL;
    uint8_t ucDataBlockTag    = 0;
    cea_861b_adb_t *pADB        = NULL;
    uint8_t i                    = 0;
    uint8_t j                    = 0;
    uint8_t * pSADBlocks = NULL;
    uint8_t * pCurrentSADBlocks = NULL;
    uint32_t ulNumSADBytes = 0;
    //vsdb_byte6_to_byte8_t *pVSDB = NULL;
    uint32_t ulIndex = 0;
    //uint8_t b48kHzCADPresent = false;

    pEEld = (hdmi_eeld_t *) &hdmi_priv->eeld;

    // Fill Version info
    pEEld->cea_edid_rev_id = HDMI_EELD_CEA_EDID_VERSION;
    pEEld->eld_ver = HDMI_EELD_VERSION;

    // Fill BaseLine ELD length
    // This is 80 bytes as per EELD proposal
    pEEld->baseline_eld_length = HDMI_EELD_BASELINE_DATA_LENGTH;

    //Zero out EDID block buffer
    memset(ucEdidBlock, 0, sizeof(ucEdidBlock));

    // Get Extn EDID
    if(!mdfld_hdmi_get_cached_edid_block(connector, 1, ucEdidBlock, EDID_BLOCK_SIZE))
    {
        return 0;
    }    

    pCeEdid = (ce_edid_t *) ucEdidBlock;

    //allocate memory (48 bytes) for SAD Blocks buffer
    pSADBlocks = kcalloc(1, 48, GFP_KERNEL);

    if(pSADBlocks == NULL)
    {
	DRM_ERROR("mdfld_hdmi_create_eld_packaet() - Failed to allocate mem for pSADBlocks\n");
        return 0;
    }

    pCurrentSADBlocks = pSADBlocks;

    // Now pull out data from CEA Extension EDID
    // If Offset <= 4, we will not have CEA DataBlocks
    if(pCeEdid->ucDTDOffset > CEA_EDID_HEADER_SZIE)
    {
        sizeOfCEADataBlock = pCeEdid->ucDTDOffset - CEA_EDID_HEADER_SZIE;

        pDataBlock = (uint8_t *)pCeEdid;

        // skip header (first 4 bytes) in CEA EDID Timing Extension
        // and set pointer to start of DataBlocks collection
        pDataBlock += CEA_EDID_HEADER_SZIE;

        // General Format of CEA Data Block Collection
        // -----------+--------------------+-----------------------------------------+
        //            |Byte#   |bits5-7    |       bits 0-4                          |
        // -----------|--------------------+-----------------------------------------+
        //            |  1     | Video Tag |Length = total #of video bytes following |
        //            |        |    Code   |this byte (L1)                           |
        //            |--------------------+-----------------------------------------+
        //  Video     |  2     | CEA Short Video Descriptor 1                        |
        //  Data      |--------+-----------------------------------------------------|
        //  Block     |  3     | CEA Short Video Descriptor 2                        |
        //            |--------+-----------------------------------------------------|
        //            | ...    | ...                                                 |
        //            |--------------------------------------------------------------+
        //            | 1+L1   | CEA Short Video Descriptor L1                       |
        // -----------+--------------------+-----------------------------------------+
        //            | 2+L1   | Audio Tag |Length = total #of audio bytes following |
        //            |        |    Code   |this byte (L2)                           |
        //            |--------------------+-----------------------------------------+
        //  Audio     | 3+L1   |                                                     |
        //  Data      |--------+                                                     |
        //  Block     | 4+L1   | CEA Short Audio Descriptor 1                        |
        //            |--------+                                                     |
        //            | 5+L1   |                                                     |
        //            |--------------------------------------------------------------+
        //            | ...    |                                                     |
        //            |        |                                                     |
        //            |        |                                                     |
        //            | ...    |                                                     |
        //            |---------------------------------------------------------------
        //            |L1+L2   |                                                     |
        //            |--------|                                                     |
        //            |1+L1+L2 | CEA Short Audio Descriptor L2/3                     |
        //            |--------|                                                     |
        //            |2+L1+L2 |                                                     |
        // -----------+--------------------------------------------------------------+
        //            |3+L1+L2 |  Speaker  |Length = total #of SA bytes following    |
        //            |        | Tag Code  |this byte (L1)                           |
        //  Speaker   |--------------------------------------------------------------+
        //  Allocation|4+L1+L2 |                                                     |
        //  Data      |--------|                                                     |
        //  Block     |5+L1+L2 | Speaker Allocation Data Block Payload(3 bytes)      |
        //            |--------|                                                     |
        //            |6+L1+L2 |                                                     |
        // -----------+--------------------------------------------------------------+
        //            |7+L1+L2 | VSDB  Tag |Length = total #of VSDB bytes following  |
        //            |        |    Code   |this byte (L1)                           |
        //  Vendor    |--------------------------------------------------------------+
        //  Specific  |8+L1+L2 |                                                     |
        //  Data      |--------|                                                     |
        //  Block     |9+L1+L2 | 24-bit IEEE Registration Identifier (LSB first)     |
        //            |--------|                                                     |
        //            |10+L1+L2|                                                     |
        //            |--------------------------------------------------------------+
        //            | ...    | Vendor Specific Data block Payload                  |
        // -----------+--------------------------------------------------------------+

        while(sizeOfCEADataBlock > 0)
        {
            // Get the Size of CEA DataBlock in bytes and TAG
            dwNumOfBytes = *pDataBlock & CEA_DATABLOCK_LENGTH_MASK;
            ucDataBlockTag = (*pDataBlock & CEA_DATABLOCK_TAG_MASK) >> 5;

            switch(ucDataBlockTag)
            {
                case CEA_AUDIO_DATABLOCK:
                    // move beyond tag/length byte
                    ++pDataBlock;
                    for (i = 0; i < (dwNumOfBytes / 3); ++i, pDataBlock += 3)
                    {
                        pADB = (cea_861b_adb_t*)pDataBlock;
                        switch(pADB->audio_format_code)
                        {
                            // uncompressed audio (Linear PCM)
                            case AUDIO_LPCM:
                                memcpy(&(hdmi_priv->lpcm_sad),pDataBlock,3);
                                //save these blocks
                                memcpy(pCurrentSADBlocks, pDataBlock, 3);
                                // move pointer in SAD blocks buffer
                                pCurrentSADBlocks += 3;
                                // update SADC field
                                pEEld->sadc += 1;
                                break;
                            // compressed audio
                            case AUDIO_AC3:
                            case AUDIO_MPEG1:
                            case AUDIO_MP3:
                            case AUDIO_MPEG2:
                            case AUDIO_AAC:
                            case AUDIO_DTS:
                            case AUDIO_ATRAC:
                            case AUDIO_OBA:
                            case AUDIO_DOLBY_DIGITAL:
                            case AUDIO_DTS_HD:
                            case AUDIO_MAT:
                            case AUDIO_DST:
                            case AUDIO_WMA_PRO:
                                //save these blocks
                                memcpy(pCurrentSADBlocks, pDataBlock, 3);
                                // move pointer in SAD blocks buffer
                                pCurrentSADBlocks += 3;
                                // update SADC field
                                pEEld->sadc += 1;
                                break;
                        }
                    }
                    break;

                case CEA_VENDOR_DATABLOCK:
                    // audio wants data from 6th byte of VSDB onwards
                    //Sighting 94842: 

                    // | Byte # |    bits[7-0]                                              |
                    // |--------------------------------------------------------------------|
                    // | 1-3    |24-bit IEEE Registration Identifier (0x000C03)             |
                    // |--------------------------------------------------------------------|
                    // | 4-5    |       Source Physical Address                             |
                    // |--------------------------------------------------------------------|
                    // | 6      |SupportsAI|DC48bit|DC36bit|Dc30bit|DCY444|Rsvd|Rsvd|DVIDual|
                    // |--------------------------------------------------------------------|
                    // | 7      |   Max TMDS clock                                          |
                    // |--------------------------------------------------------------------|
                    // | 8      |Latency_Field |I_Latency_Field| Reserved bits 5-0          |
                    // |        |   _Present   |  _Present     |                            |
                    // |--------------------------------------------------------------------|
                    // | 9      |               Video Latency                               |
                    // |--------------------------------------------------------------------|
                    // | 10     |               Audio Latency                               |
                    // |--------------------------------------------------------------------|
                    // | 11     |            Interlaced Video Latency                       |
                    // |--------------------------------------------------------------------|
                    // | 12     |            Interlaced Audio Latency                       |
                    // |--------------------------------------------------------------------|

                    ++pDataBlock;
                    // move pointer to next CEA Datablock
                    pDataBlock += dwNumOfBytes;
                    break;                 

                case CEA_SPEAKER_DATABLOCK:
                    pEEld->speaker_allocation_block = *(++pDataBlock);
                    // move pointer to next CEA Datablock
                    pDataBlock += dwNumOfBytes;
                    break;                       

                default:
                    // Move pointer to next CEA DataBlock
                    pDataBlock += (dwNumOfBytes + 1);
            }
            // Decrement size of CEA DataBlock
            sizeOfCEADataBlock -= (dwNumOfBytes + 1);
        }    
    }

    //Copy all the saved SAD blocks at the end of ELD
    //SAD blocks should be written after the Monitor name and VSDB.
    //See ELD definition in iHDMI.h
    ulNumSADBytes = (pEEld->sadc) * 3; //Size of each SAD block is 3 bytes

    //DCN 460119: Audio does not play on displays which do not provide SAB in EDID.
    //Solution: Graphics driver should create a default SAB in ELD with front left and front right
    //speakers enabled if the display supports basic audio. 
    pDataBlock = (uint8_t *)pCeEdid;
    if((*(pDataBlock + HDMI_CEA_EXTENSION_BLOCK_BYTE_3) & HDMI_BASIC_AUDIO_SUPPORTED) && (pEEld->speaker_allocation_block == 0)) 
    {
        pEEld->flr = 1;
    }
    //End of DCN 460119

    // zero out local buffers
    memset(ucEdidBlock, 0, sizeof(ucEdidBlock));

    // Get base EDID
    if(!mdfld_hdmi_get_cached_edid_block(connector, 0, ucEdidBlock, EDID_BLOCK_SIZE))
    {
        return 0;
    }

    pEdid = (baseedid_1_x_t*) ucEdidBlock;
    pDTD = &pEdid->DTD[1];

    //Update the Manufacturer ID and Product Code here
    memcpy(pEEld->manufacturer_id,pEdid->ManufacturerID,2);
    memcpy(pEEld->product_id,pEdid->ProductID,2);

    // Now Fill the monitor string name
    // Search through DTD blocks, looking for monitor name
    for (i = 0; i < MAX_BASEEDID_DTD_BLOCKS - 1; ++i, ++pDTD)
    {
        // Set a uint8_t pointer to DTD data
        pData = (uint8_t *)pDTD;

        // Check the Flag (the first two bytes) to determine
        // if this block is used as descriptor
        if (pData[0] == 0x00 && pData[1] == 0x00)
        {
            // And now check Data Type Tag within this descriptor
            // Tag = 0xFC, then monitor name stored as ASCII
            if (pData[3] == 0xFC)
            {
                ulIndex = 0;
                // Copy monitor name
                for (j = 0; (j < 13) && (pData[j+5] != 0x0A); ++j)
                {
                    pEEld->mn_sand_sads[ulIndex] = pData[j+5];
                    ulIndex++;
                }
                pEEld->mnl = j;
                break;
            }
        }
    }

    //Check if number of SAD Bytes > 0 and for size within limits of allowed Base line Data size as per EELD spec
    if((ulNumSADBytes > 0) && (ulNumSADBytes <= 64))
    {
        //Copy the SADs immediately after the Monitor Name String
        memcpy(&pEEld->mn_sand_sads[j], pSADBlocks, ulNumSADBytes);
    }


    // Header = 4, Baseline Data = 60 and Vendor (INTEL) specific = 2
    // 4 + 60 + 2 = 66
    hdmi_priv->hdmi_eeld_size = HDMI_EELD_SIZE;
    
    //free the buffer allocated for SAD blocks
    kfree(pSADBlocks);
    pSADBlocks = NULL;
    pCurrentSADBlocks = NULL;
    return 1;
}

static enum drm_connector_status
mdfld_hdmi_edid_detect(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct psb_intel_output *output = to_psb_intel_output(connector);
	struct mid_intel_hdmi_priv *hdmi_priv = output->dev_priv;
	struct edid *edid = NULL;
	enum drm_connector_status status = connector_status_disconnected;
	int i = 0;
	int monitor_number = sizeof(mdfld_hdmi_edid) / sizeof(struct hdmi_edid_info);

	PSB_DEBUG_ENTRY("\n");

#if 0
	if (!psb_intel_output->hdmi_i2c_adapter) {
		DRM_INFO("Enter mdfld_hdmi_get_modes, i2c_adapter is NULL. \n");
		
		/* hard-coded the HDMI_I2C_ADAPTER_ID to be 3, Should get from GCT*/
		psb_intel_output->hdmi_i2c_adapter = i2c_get_adapter(3);
	}

	if (!psb_intel_output->hdmi_i2c_adapter) {
		DRM_INFO("Enter mdfld_hdmi_get_modes, no valid i2c_adapter . \n");
		return ret;
	}

	edid =
	    drm_get_edid(&psb_intel_output->base,
			 psb_intel_output->hdmi_i2c_adapter);

	hdmi_priv->has_hdmi_sink = false;
	if (edid) {
		if (edid->input & DRM_EDID_INPUT_DIGITAL) {
			status = connector_status_connected;
			hdmi_priv->has_hdmi_sink = drm_detect_hdmi_monitor(edid);
			mdfld_hdmi_create_eeld_packet(connector);
		}

		intel_output->base.display_info.raw_edid = NULL;
		kfree(edid);
	}
#else
	if ((HDMI_EDID == NULL) || (strlen(HDMI_EDID) > HDMI_MONITOR_NAME_LENGTH))
		return status;

	DRM_INFO("Detected HDMI monitor %s.\n", HDMI_EDID);

	for (i = 0; i < monitor_number; i++) {
		if (!strcasecmp(HDMI_EDID, mdfld_hdmi_edid[i].monitor_name)) {
			edid = (struct edid *)mdfld_hdmi_edid[i].edid_info;
			break;
		}
	}

	if (i == monitor_number)
		edid = (struct edid *)mdfld_hdmi_edid[monitor_number - 1].edid_info;

	hdmi_priv->has_hdmi_sink = false;
	if (edid) {
		status = connector_status_connected;
		hdmi_priv->has_hdmi_sink = drm_detect_hdmi_monitor(edid);
		mdfld_hdmi_create_eeld_packet(connector);
	}

	dev_priv->hdmi_done_reading_edid = true;
#endif 
	return status;
}

/* FIXME: start using the force parameter */
static enum drm_connector_status mdfld_hdmi_detect(struct drm_connector
						*connector, bool force)
{
	return mdfld_hdmi_edid_detect(connector);
}

static int mdfld_hdmi_set_property(struct drm_connector *connector,
				       struct drm_property *property,
				       uint64_t value)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_encoder *pEncoder = connector->encoder;
	struct android_hdmi_priv *hdmi_priv = NULL;
	static int prev_state;
	int state = 0, audio_notify = 0;

	PSB_DEBUG_ENTRY("connector info, type = %d, type_id=%d, base=0x%p, base.id=0x%x.\n",
			connector->connector_type, connector->connector_type_id, &connector->base, connector->base.id);
	if (pEncoder) {
		PSB_DEBUG_ENTRY("encoder info, base.id=%d, encoder_type=%d, dev=0x%p, base=0x%p, possible_clones=0x%x.\n",
				pEncoder->base.id, pEncoder->encoder_type, pEncoder->dev, &pEncoder->base, pEncoder->possible_clones);
		PSB_DEBUG_ENTRY("encoder info, possible_crtcs=0x%x, crtc=0x%p.\n",
				pEncoder->possible_crtcs, pEncoder->crtc);
	} else {
		PSB_DEBUG_ENTRY("encoder info NULL\n");
	}

	if (!strcmp(property->name, "scaling mode") && pEncoder) {
		PSB_DEBUG_ENTRY("scaling mode \n");
	} else if (!strcmp(property->name, "backlight") && pEncoder) {
		PSB_DEBUG_ENTRY("backlight \n");
	} else if (!strcmp(property->name, "DPMS") && pEncoder) {
		PSB_DEBUG_ENTRY("DPMS \n");
	}

	if (!strcmp(property->name, "hdmi-send-uevent")) {
		hdmi_priv = to_psb_intel_output(connector)->dev_priv;
		if (hdmi_priv != NULL) {
			/* user space expects:
			 * '0' for disconnected, '1' for connected.
			 * whereas: connector_status_connected is '1' and
			 * connector_status_disconnected is '2'.
			 * so, make proper conversion.
			 */
			state = (value == connector_status_connected) ? 1 : 0;

			/* TODO: Use get/set attribute to get monitor_type */
			if (hdmi_priv->monitor_type == MONITOR_TYPE_HDMI) {
				/*
				 * Notify the audio driver only if there is
				 * a change in state.
				 */
				audio_notify = state ? HAD_EVENT_HOT_PLUG :
					HAD_EVENT_HOT_UNPLUG;

				if (state != prev_state &&
				    dev_priv->mdfld_had_event_callbacks) {
					(*dev_priv->mdfld_had_event_callbacks)
						(audio_notify,
						 dev_priv->had_pvt_data);
				}

				switch_set_state(&g_switch_hdmi_dev, state);
			} else
				switch_set_state(&g_switch_dvi_dev, state);

			prev_state = state;
		}
	}

	if (!strcmp(property->name, "scaling mode") && pEncoder) {
		struct psb_intel_crtc *pPsbCrtc = to_psb_intel_crtc(pEncoder->crtc);
		bool bTransitionFromToCentered;
		uint64_t curValue;

		if (!pPsbCrtc)
			goto set_prop_error;

		switch (value) {
		case DRM_MODE_SCALE_FULLSCREEN:
			break;
		case DRM_MODE_SCALE_NO_SCALE:
			break;
		case DRM_MODE_SCALE_ASPECT:
			break;
		default:
			goto set_prop_error;
		}

		if (drm_connector_property_get_value(connector, property, &curValue))
			goto set_prop_error;

		if (curValue == value)
			goto set_prop_done;

		if (drm_connector_property_set_value(connector, property, value))
			goto set_prop_error;

		bTransitionFromToCentered = (curValue == DRM_MODE_SCALE_NO_SCALE) ||
			(value == DRM_MODE_SCALE_NO_SCALE);

		if (pPsbCrtc->saved_mode.hdisplay != 0 &&
		    pPsbCrtc->saved_mode.vdisplay != 0) {
			if (bTransitionFromToCentered) {
				if (!drm_crtc_helper_set_mode(pEncoder->crtc, &pPsbCrtc->saved_mode,
					    pEncoder->crtc->x, pEncoder->crtc->y, pEncoder->crtc->fb))
					goto set_prop_error;
			} else {
				struct drm_encoder_helper_funcs *pEncHFuncs  = pEncoder->helper_private;
				pEncHFuncs->mode_set(pEncoder, &pPsbCrtc->saved_mode,
						     &pPsbCrtc->saved_adjusted_mode);
			}
		}
	} 
set_prop_done:
    return 0;
set_prop_error:
    return -1;
}

/**
 * Return the list of HDMI DDC modes if available.
 */
static int mdfld_hdmi_get_modes(struct drm_connector *connector)
{
	struct psb_intel_output *psb_intel_output = to_psb_intel_output(connector);
	struct edid *edid = NULL;
	int ret = 0;
#if 1 /* FIXME_JLIU7 HDMI*/	
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	struct drm_display_mode *panel_fixed_mode = NULL;
#endif 
	int i = 0;
	int monitor_number = sizeof(mdfld_hdmi_edid) / sizeof(struct hdmi_edid_info);

	PSB_DEBUG_ENTRY("\n");

	if (dsi_config != NULL) {
		panel_fixed_mode
			 = drm_mode_duplicate(dev, dsi_config->fixed_mode);
	}

#if 0
	if (!psb_intel_output->hdmi_i2c_adapter) {
		DRM_INFO("Enter mdfld_hdmi_get_modes, i2c_adapter is NULL. \n");
		
		/* hard-coded the HDMI_I2C_ADAPTER_ID to be 3, Should get from GCT*/
		psb_intel_output->hdmi_i2c_adapter = i2c_get_adapter(3);
	}

	if (!psb_intel_output->hdmi_i2c_adapter) {
		DRM_INFO("Enter mdfld_hdmi_get_modes, no valid i2c_adapter . \n");
		return ret;
	}

	edid =
	    drm_get_edid(&psb_intel_output->base,
			 psb_intel_output->hdmi_i2c_adapter);
#else
	edid = NULL;
#endif 
	if (edid) {
		drm_mode_connector_update_edid_property(&psb_intel_output->
							base, edid);
		ret = drm_add_edid_modes(&psb_intel_output->base, edid);
		kfree(edid);
	}
#if MDFLD_HDMI_JLIU7_DEBUG_1
	else {
		DRM_INFO("mdfld_hdmi_get_modes, fake edid info. \n");
#if 1 //MDFLD_HDMI_JLIU7_DEBUG_1
		for (i = 0; i < monitor_number; i++) {
			if (!strcasecmp(HDMI_EDID, mdfld_hdmi_edid[i].monitor_name)) {
				edid = (struct edid *)mdfld_hdmi_edid[i].edid_info;
				break;
			}
		}

		if (i == monitor_number)
			edid = (struct edid *)mdfld_hdmi_edid[monitor_number - 1].edid_info;

		connector->display_info.raw_edid = (char *)edid;
		drm_mode_connector_update_edid_property(&psb_intel_output->
							base, edid);
		ret = drm_add_edid_modes(&psb_intel_output->base, edid);
#if 1 /* FIXME_JLIU7 HDMI*/	
	if (panel_fixed_mode != NULL) {
		struct drm_display_mode *mode =
		    drm_mode_duplicate(dev, panel_fixed_mode);
		drm_mode_probed_add(connector, mode);
	}
#endif /* FIXME_JLIU7 HDMI*/	
#endif /* MDFLD_HDMI_JLIU7_DEBUG_1 */
	}
#endif /* MDFLD_HDMI_JLIU7_DEBUG_1 */

	if (ret)
		return ret;

	/* Didn't get an EDID, so
	 * Set wide sync ranges so we get all modes
	 * handed to valid_mode for checking
	 */
	connector->display_info.min_vfreq = 0;
	connector->display_info.max_vfreq = 200;
	connector->display_info.min_hfreq = 0;
	connector->display_info.max_hfreq = 200;

	return 0;
}

static int mdfld_hdmi_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{

	PSB_DEBUG_ENTRY("display info. hdisplay = %d, vdisplay = %d. \n", mode->hdisplay, mode->vdisplay);

#if 0 /* MDFLD_HDMI_JLIU7_DEBUG */
	if (mode->clock > 165000)
		return MODE_CLOCK_HIGH;
	if (mode->clock < 20000)
		return MODE_CLOCK_HIGH;
#endif /* MDFLD_HDMI_JLIU7_DEBUG */

	/* just in case */
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	/* just in case */
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

#if !MDFLD_HDMI_JLIU7_1080
	if (mode->hdisplay > 1600)
		return MODE_PANEL;
	if (mode->vdisplay > 1080)
		return MODE_PANEL;
#endif /* MDFLD_HDMI_JLIU7_1080 */

	return MODE_OK;
}

static void mdfld_hdmi_connector_dpms(struct drm_connector *connector, int mode)
{
	drm_helper_connector_dpms(connector, mode);
}

/**
 * psb_intel_lvds_destroy - unregister and free LVDS structures
 * @connector: connector to free
 *
 * Unregister the DDC bus for this connector then free the driver private
 * structure.
 *
 * Note: taken from psb_intel_lvds.c
 */
static void psb_intel_lvds_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

/* Note: taken from psb_intel_lvds.c */
static void psb_intel_lvds_enc_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

/* Note: taken from psb_intel_lvds.c */
const struct drm_encoder_funcs psb_intel_lvds_enc_funcs = {
	.destroy = psb_intel_lvds_enc_destroy,
};

const struct drm_encoder_helper_funcs mdfld_hdmi_helper_funcs = {
	.dpms = mdfld_hdmi_dpms,
	.mode_fixup = mdfld_hdmi_mode_fixup,
	.prepare = psb_intel_encoder_prepare,
	.mode_set = android_hdmi_enc_mode_set,
	.commit = psb_intel_encoder_commit,
};

const struct drm_connector_helper_funcs
    mdfld_hdmi_connector_helper_funcs = {
	.get_modes = android_hdmi_get_modes,
	.mode_valid = android_hdmi_mode_valid,
	.best_encoder = psb_intel_best_encoder,
};

const struct drm_connector_funcs mdfld_hdmi_connector_funcs = {
	.dpms = mdfld_hdmi_connector_dpms,
	.save = mdfld_hdmi_save,
	.restore = mdfld_hdmi_restore,
	.detect = android_hdmi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = mdfld_hdmi_set_property,
	.destroy = psb_intel_lvds_destroy,
};

void mdfld_hdmi_init(struct drm_device *dev,
		    struct psb_intel_mode_device *mode_dev)
{
	PSB_DEBUG_ENTRY("\n");

	android_hdmi_driver_init(dev, (void *) mode_dev);

	/* turn on HDMI power rails. These will be on in all non-S0iX
	 * states so that HPD and connection status will work. VCC330
	 * will have ~1.7mW usage during idle states when the display
	 * is active.
	 */
	intel_scu_ipc_iowrite8(MSIC_VCC330CNT, VCC330_ON);

	/* MSIC documentation requires that there be a 500us delay
	 * after enabling VCC330 before you can enable VHDMI
	 */
	usleep_range(500, 1000);

	/* Extend VHDMI switch de-bounce time, to avoid redundant MSIC
	 * VREG/HDMI interrupt during HDMI cable plugged
	 * in/out.
	 */
	intel_scu_ipc_iowrite8(MSIC_VHDMICNT, VHDMI_ON | VHDMI_DB_30MS);

	return;
}
#endif

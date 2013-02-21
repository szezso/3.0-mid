/*
 * Copyright (c)  2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicensen
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
 * Thomas Eaton <thomas.g.eaton@intel.com>
 * Scott Rowe <scott.m.rowe@intel.com>
*/

#include <linux/init.h>
#include "mdfld_output.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_output.h"

#include "displays/tpo_cmd.h"
#include "displays/tpo_vid.h"
#include "displays/tmd_vid.h"
#include "displays/hdmi.h"
#include "tc35876x-dsi-lvds.h"

enum panel_type get_panel_type(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	return dev_priv->panel_id;
}

int is_panel_vid_or_cmd(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	int ret = 0;
	switch(dev_priv->panel_id) {
	case TMD_VID:
	case TPO_VID:
	case TC35876X:
		ret =  MDFLD_DSI_ENCODER_DPI;
		break;
	case TMD_CMD:
	case TPO_CMD:
	default:
		ret =  MDFLD_DSI_ENCODER_DBI;
		break;
	}
	return ret;
}

void mdfld_output_init(struct drm_device* dev)
{
	enum panel_type p_type1, p_type2;

	/* MIPI panel 1 */
	p_type1 = get_panel_type(dev, 0);
	init_panel(dev, 0, p_type1);

#ifdef CONFIG_MDFD_DUAL_MIPI
	/* MIPI panel 2 */
	p_type2 = get_panel_type(dev, 2);
	init_panel(dev, 2, p_type2);
#endif

#ifdef CONFIG_MDFD_HDMI
	/* HDMI panel */
	init_panel(dev, 1, HDMI);
#endif
}

void init_panel(struct drm_device* dev, int mipi_pipe, enum panel_type p_type)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct panel_funcs * p_cmd_funcs = NULL; 
	struct panel_funcs * p_vid_funcs = NULL;

	p_cmd_funcs = kzalloc(sizeof(struct panel_funcs), GFP_KERNEL);
	p_vid_funcs = kzalloc(sizeof(struct panel_funcs), GFP_KERNEL);
	
	switch (p_type) {
	case TPO_CMD:
		tpo_cmd_init(dev, p_cmd_funcs);
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, p_cmd_funcs, NULL);
		break;
	case TPO_VID:
		tpo_vid_init(dev, p_vid_funcs);
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, NULL, p_vid_funcs);
		break;
	case TMD_CMD:
		/*tmd_cmd_init(dev, p_cmd_funcs);*/
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, p_cmd_funcs, NULL);
		break;
	case TC35876X:
		tc35876x_init(dev, p_vid_funcs);
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, NULL, p_vid_funcs);
		break;
	case TMD_VID:
		tmd_vid_init(dev, p_vid_funcs);
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, NULL, p_vid_funcs);
		break;
	case TPO:	/*TPO panel supports both cmd & vid interfaces*/
		tpo_cmd_init(dev, p_cmd_funcs);
		tpo_vid_init(dev, p_vid_funcs);
		mdfld_dsi_output_init(dev, mipi_pipe, NULL, p_cmd_funcs, p_vid_funcs);
		break;
	case TMD:
		break;
	case HDMI:
		if (dev_priv && dev_priv->hdmi_present) {
			printk(KERN_ALERT "GFX: Initializing HDMI");
			mdfld_hdmi_init(dev, &dev_priv->mode_dev);
		} else {
			printk(KERN_ERR "HDMI dev priv should not be null"
			       "at this time!\n");
			BUG();
		}

		break;
	default:
		break;
	}
}


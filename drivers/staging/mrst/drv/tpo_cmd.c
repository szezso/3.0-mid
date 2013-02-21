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



#include "displays/tpo_cmd.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_pkg_sender.h"
#include "psb_powermgmt.h"

static struct drm_display_mode*
tpo_cmd_get_config_mode(struct drm_device* dev)
{
	struct drm_display_mode *mode;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct mrst_timing_info *ti = &dev_priv->gct_data.DTD;
	bool use_gct = false;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	if (use_gct) {
		PSB_DEBUG_ENTRY("gct find MIPI panel. \n");

		mode->hdisplay = (ti->hactive_hi << 8) | ti->hactive_lo;
		mode->vdisplay = (ti->vactive_hi << 8) | ti->vactive_lo;
		mode->hsync_start = mode->hdisplay + \
				((ti->hsync_offset_hi << 8) | \
				ti->hsync_offset_lo);
		mode->hsync_end = mode->hsync_start + \
				((ti->hsync_pulse_width_hi << 8) | \
				ti->hsync_pulse_width_lo);
		mode->htotal = mode->hdisplay + ((ti->hblank_hi << 8) | \
								ti->hblank_lo);
		mode->vsync_start = \
			mode->vdisplay + ((ti->vsync_offset_hi << 8) | \
						ti->vsync_offset_lo);
		mode->vsync_end = \
			mode->vsync_start + ((ti->vsync_pulse_width_hi << 8) | \
						ti->vsync_pulse_width_lo);
		mode->vtotal = mode->vdisplay + \
				((ti->vblank_hi << 8) | ti->vblank_lo);
		mode->clock = ti->pixel_clock * 10;

		PSB_DEBUG_ENTRY("hdisplay is %d\n", mode->hdisplay);
		PSB_DEBUG_ENTRY("vdisplay is %d\n", mode->vdisplay);
		PSB_DEBUG_ENTRY("HSS is %d\n", mode->hsync_start);
		PSB_DEBUG_ENTRY("HSE is %d\n", mode->hsync_end);
		PSB_DEBUG_ENTRY("htotal is %d\n", mode->htotal);
		PSB_DEBUG_ENTRY("VSS is %d\n", mode->vsync_start);
		PSB_DEBUG_ENTRY("VSE is %d\n", mode->vsync_end);
		PSB_DEBUG_ENTRY("vtotal is %d\n", mode->vtotal);
		PSB_DEBUG_ENTRY("clock is %d\n", mode->clock);
	} else {
		mode->hdisplay = 864;
		mode->vdisplay = 480;
		mode->hsync_start = 872;
		mode->hsync_end = 876;
		mode->htotal = 884;
		mode->vsync_start = 482;
		mode->vsync_end = 494;
		mode->vtotal = 486;
		mode->clock = 25777;
	}

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);
	
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static bool mdfld_dsi_dbi_mode_fixup(struct drm_encoder * encoder,
				     struct drm_display_mode * mode,
				     struct drm_display_mode * adjusted_mode)
{
	struct drm_device* dev = encoder->dev;
	struct drm_display_mode * fixed_mode = tpo_cmd_get_config_mode(dev);

	PSB_DEBUG_ENTRY("\n");

	if(fixed_mode) {
		adjusted_mode->hdisplay = fixed_mode->hdisplay;
		adjusted_mode->hsync_start = fixed_mode->hsync_start;
		adjusted_mode->hsync_end = fixed_mode->hsync_end;
		adjusted_mode->htotal = fixed_mode->htotal;
		adjusted_mode->vdisplay = fixed_mode->vdisplay;
		adjusted_mode->vsync_start = fixed_mode->vsync_start;
		adjusted_mode->vsync_end = fixed_mode->vsync_end;
		adjusted_mode->vtotal = fixed_mode->vtotal;
		adjusted_mode->clock = fixed_mode->clock;
		drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);
		kfree(fixed_mode);
	}
	
	return true;
}

static void mdfld_dsi_dbi_set_power(struct drm_encoder *encoder, bool on)
{
	int ret = 0;
	struct mdfld_dsi_encoder *dsi_encoder =
		MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_dbi_output *dbi_output =
		MDFLD_DSI_DBI_OUTPUT(dsi_encoder);
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_encoder_get_config(dsi_encoder);
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_encoder_get_pkg_sender(dsi_encoder);
	/*struct drm_device * dev = dbi_output->dev;*/
	struct drm_device* dev = encoder->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	int pipe = (dbi_output->channel_num == 0) ? 0 : 2;
	u32 data = 0;
	
	PSB_DEBUG_ENTRY("pipe %d : %s, panel on: %s\n",pipe, on ? "On" : "Off", dbi_output->dbi_panel_on ? "True" : "False");
        
	if(pipe == 2) {
		if(on)
			dev_priv->dual_mipi = true;
		else
			dev_priv->dual_mipi = false;
	} else {
		if (!on)
			dev_priv->dual_mipi = false;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true)) {
		DRM_ERROR("hw begin failed\n");
		return;
	}

	
	if(on) {
		if(dbi_output->dbi_panel_on)
			goto out_err;
					
		ret = mdfld_dsi_dbi_update_power(dbi_output, DRM_MODE_DPMS_ON);
		if(ret) {
			DRM_ERROR("power on error\n");
			goto out_err;
		}
    
		dbi_output->dbi_panel_on = true;

		if(pipe == 2)
			dev_priv->dbi_panel_on2 = true;
		else
			dev_priv->dbi_panel_on = true;

		mdfld_enable_te(dev, pipe);
	} else {
		if(!dbi_output->dbi_panel_on && !dbi_output->first_boot) 
			goto out_err;
					
		dbi_output->dbi_panel_on = false;
		dbi_output->first_boot = false;
				
		if (pipe == 2)
			dev_priv->dbi_panel_on2 = false;
		else
			dev_priv->dbi_panel_on = false;

		mdfld_disable_te(dev, pipe);

		ret = mdfld_dsi_dbi_update_power(dbi_output, DRM_MODE_DPMS_OFF);
		if(ret) {
			DRM_ERROR("power on error\n");
			goto out_err;
		}
	}

	/**
	 * FIXME: this is a WA for TPO panel crash on DPMS on & off around
	 * 83 times. the root cause of this issue is that Booster in
	 * drvIC crashed. Add this WA so that we can resume the driver IC
	 * once we found that booster has a fault
	 */
	mdfld_dsi_get_power_mode(dsi_config, &data, true);
	if (on && data && !(data & BIT(7))) {
		/*soft reset*/
		mdfld_dsi_send_mcs_short(sender, DCS_SOFT_RESET, 0, 0, true);

		/*init drvIC*/
		if (dbi_output->p_funcs->drv_ic_init)
			dbi_output->p_funcs->drv_ic_init(dsi_config,
							 pipe);
	}

out_err:
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	
	if(ret)
		DRM_ERROR("failed\n");
	else
		PSB_DEBUG_ENTRY("successfully\n");
}


static void mdfld_dsi_dbi_mode_set(struct drm_encoder * encoder,
				   struct drm_display_mode * mode,
				   struct drm_display_mode * adjusted_mode)
{
	int ret = 0;
	struct drm_device * dev = encoder->dev;
	struct drm_psb_private * dev_priv = (struct drm_psb_private*)dev->dev_private;
	struct mdfld_dsi_encoder * dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_dbi_output * dsi_output = MDFLD_DSI_DBI_OUTPUT(dsi_encoder);
	struct mdfld_dsi_config * dsi_config = mdfld_dsi_encoder_get_config(dsi_encoder);
	struct mdfld_dsi_connector * dsi_connector = dsi_config->connector;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_encoder_get_pkg_sender(&dsi_output->base);
	int pipe = dsi_connector->pipe;
	
	/*regs*/
	u32 dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	u32 pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	
	/*values*/
	u32 dspcntr_val = dev_priv->dspcntr;
	u32 pipeconf_val = dev_priv->pipeconf;
	u32 h_active_area = mode->hdisplay; 
	u32 v_active_area = mode->vdisplay; 
	u32 mipi_val = (PASS_FROM_SPHY_TO_AFE | SEL_FLOPPED_HSTX);

	mipi_val = PASS_FROM_SPHY_TO_AFE | SEL_FLOPPED_HSTX |
		TE_TRIGGER_GPIO_PIN;

	PSB_DEBUG_ENTRY("mipi_val =0x%x\n", mipi_val);

	PSB_DEBUG_ENTRY("type %s\n", (pipe == 2) ? "MIPI2" : "MIPI");
	PSB_DEBUG_ENTRY("h %d v %d\n", mode->hdisplay, mode->vdisplay);

	if(pipe == 2) {
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		dspcntr_val = dev_priv->dspcntr2;
		pipeconf_val = dev_priv->pipeconf2;
	} else {
		mipi_val |= 0x2; /*two lanes for port A and C respectively*/
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true)) {
		DRM_ERROR("hw begin failed\n");
		return;
	}
	
	REG_WRITE(dspcntr_reg, dspcntr_val);
	REG_READ(dspcntr_reg);
    
	/*20ms delay before sending exit_sleep_mode*/
	msleep(20);
    
	/*send exit_sleep_mode DCS*/
	ret = mdfld_dsi_send_mcs_short(sender, DCS_EXIT_SLEEP_MODE, 0, 0, true);
	if(ret) {
		DRM_ERROR("sent exit_sleep_mode faild\n");
		goto out_err;
	}
	
	/*send set_tear_on DCS*/
	ret = mdfld_dsi_send_mcs_short(sender, DCS_SET_TEAR_ON, 0, 1, true);
	if (ret) {
		DRM_ERROR("%s - sent set_tear_on faild\n", __FUNCTION__);
		goto out_err;
	}
	
	REG_WRITE(pipeconf_reg, pipeconf_val | PIPEACONF_DSR);
	REG_READ(pipeconf_reg);
	
	/*TODO: this looks ugly, try to move it to CRTC mode setting*/
	if(pipe == 2) {
		dev_priv->pipeconf2 |= PIPEACONF_DSR;
	} else {
		dev_priv->pipeconf |= PIPEACONF_DSR;
	}
	
	PSB_DEBUG_ENTRY("pipeconf %x\n",  REG_READ(pipeconf_reg));
	
	ret = mdfld_dsi_dbi_update_area(dsi_output, 0, 0, h_active_area - 1, v_active_area - 1);
	if(ret) {
		DRM_ERROR("update area failed\n");
		goto out_err;
	}

out_err:
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	if(ret) {
		DRM_ERROR("mode set failed\n");
	} else {
		PSB_DEBUG_ENTRY("mode set done successfully\n");
	}
}

static void mdfld_dsi_dbi_prepare(struct drm_encoder * encoder) 
{
	struct mdfld_dsi_encoder * dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_dbi_output * dbi_output = MDFLD_DSI_DBI_OUTPUT(dsi_encoder);
	
	PSB_DEBUG_ENTRY("\n");
	
	dbi_output->mode_flags |= MODE_SETTING_IN_ENCODER;
	dbi_output->mode_flags &= ~MODE_SETTING_ENCODER_DONE;

	mdfld_dsi_dbi_set_power(encoder, false);
	
}

static void mdfld_dsi_dbi_commit(struct drm_encoder * encoder) 
{
	struct mdfld_dsi_encoder * dsi_encoder = MDFLD_DSI_ENCODER(encoder);
	struct mdfld_dsi_dbi_output * dbi_output = MDFLD_DSI_DBI_OUTPUT(dsi_encoder);
	struct drm_device * dev = dbi_output->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	
/*DSI DPU was still on debugging, will remove this option later*/
#ifdef CONFIG_MDFLD_DSI_DPU		
	struct psb_drm_dpu_rect rect;
#endif
	
	PSB_DEBUG_ENTRY("\n");

	mdfld_dsi_dbi_set_power(encoder, true);
	
	dbi_output->mode_flags &= ~MODE_SETTING_IN_ENCODER;

#ifdef CONFIG_MDFLD_DSI_DPU
	rect.x = rect.y = 0;
	rect.width = 864;
	rect.height = 480;
#endif	
	
	if(dbi_output->channel_num == 1) {
		dev_priv->dsr_fb_update |= MDFLD_DSR_2D_3D_2;
#ifdef CONFIG_MDFLD_DSI_DPU
		/*if dpu enabled report a fullscreen damage*/
		mdfld_dbi_dpu_report_damage(dev, MDFLD_PLANEC, &rect);
#endif
	} else {
		dev_priv->dsr_fb_update |= MDFLD_DSR_2D_3D_0;
		
#ifdef CONFIG_MDFLD_DSI_DPU		
		mdfld_dbi_dpu_report_damage(dev, MDFLD_PLANEA, &rect);
#endif
	}
	
	dbi_output->mode_flags |= MODE_SETTING_ENCODER_DONE;
}

static void mdfld_dsi_dbi_dpms(struct drm_encoder *encoder, int mode)
{
	struct mdfld_dsi_encoder * dsi_encoder = MDFLD_DSI_ENCODER(encoder);
        struct mdfld_dsi_dbi_output * dbi_output = MDFLD_DSI_DBI_OUTPUT(dsi_encoder);
        struct drm_device * dev = dbi_output->dev;
	static bool bdispoff = false;

	PSB_DEBUG_ENTRY("%s \n",  (mode == DRM_MODE_DPMS_ON ? "on":"off"));

	if (mode == DRM_MODE_DPMS_ON){
		mdfld_dsi_dbi_set_power(encoder, true);
	} else {
		/**
		 * I am not sure whether this is the perfect place to 
		 * turn rpm on since we still have a lot of CRTC turnning 
		 * on work to do.
		 */ 
		bdispoff = true;
		mdfld_dsi_dbi_set_power(encoder, false);
	}
}


/**
 * Update the DBI MIPI Panel Frame Buffer.
 */
static void mdfld_dsi_dbi_update_fb (struct mdfld_dsi_dbi_output * dbi_output, int pipe)
{
	struct mdfld_dsi_pkg_sender * sender = 
		mdfld_dsi_encoder_get_pkg_sender(&dbi_output->base);
	struct drm_device * dev = dbi_output->dev;
	struct drm_crtc * crtc = dbi_output->base.base.crtc;
	struct psb_intel_crtc * psb_crtc = (crtc) ? to_psb_intel_crtc(crtc) : NULL; 

	u32 dpll_reg = PSB_DSI_PLL_CTRL;
	u32 dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	u32 pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	u32 dsplinoff_reg = PSB_DSPLINOFF(PSB_PIPE_A);
	u32 dspsurf_reg = PSB_DSPSURF(PSB_PIPE_A);
	
	/*if mode setting on-going, back off*/
	if((dbi_output->mode_flags & MODE_SETTING_ON_GOING) ||
		(psb_crtc && psb_crtc->mode_flags & MODE_SETTING_ON_GOING) ||
		!(dbi_output->mode_flags & MODE_SETTING_ENCODER_DONE))
		return;
		
	if(pipe == 2) {
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		dsplinoff_reg = PSB_DSPLINOFF(PSB_PIPE_C);
		dspsurf_reg = PSB_DSPSURF(PSB_PIPE_C);
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true)) {
		DRM_ERROR("hw begin failed\n");
		return;
	}

	/*check DBI FIFO status*/
	if (!(REG_READ(dpll_reg) & DPLL_VCO_ENABLE) ||
	   !(REG_READ(dspcntr_reg) & DISPLAY_PLANE_ENABLE) ||
	   !(REG_READ(pipeconf_reg) & DISPLAY_PLANE_ENABLE)) {
		goto update_fb_out0;
	}
	
	/*refresh plane changes*/
	REG_WRITE(dsplinoff_reg, REG_READ(dsplinoff_reg));
	REG_WRITE(dspsurf_reg, REG_READ(dspsurf_reg));
	REG_READ(dspsurf_reg);

	mdfld_dsi_write_mem_start(sender);
	
	dbi_output->dsr_fb_update_done = true;
update_fb_out0:
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
}

static int tpo_cmd_get_panel_info(struct drm_device * dev,
				int pipe,
				struct panel_info * pi)
{
	if(!dev || !pi)
		return -EINVAL;

	pi->width_mm = TPO_PANEL_WIDTH;
	pi->height_mm = TPO_PANEL_HEIGHT;

	return 0;
}


/*TPO DBI encoder helper funcs*/
static const struct drm_encoder_helper_funcs mdfld_dsi_dbi_helper_funcs = {
	.dpms = mdfld_dsi_dbi_dpms,
	.mode_fixup = mdfld_dsi_dbi_mode_fixup,
	.prepare = mdfld_dsi_dbi_prepare,
	.mode_set = mdfld_dsi_dbi_mode_set,
	.commit = mdfld_dsi_dbi_commit,
};

/*TPO DBI encoder funcs*/
static const struct drm_encoder_funcs mdfld_dsi_dbi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

void tpo_cmd_init(struct drm_device* dev, struct panel_funcs* p_funcs)
{
	p_funcs->encoder_funcs = &mdfld_dsi_dbi_encoder_funcs;
	p_funcs->encoder_helper_funcs = &mdfld_dsi_dbi_helper_funcs;
	p_funcs->get_config_mode = &tpo_cmd_get_config_mode;
	p_funcs->update_fb = mdfld_dsi_dbi_update_fb;
	p_funcs->get_panel_info = tpo_cmd_get_panel_info;
	p_funcs->reset = mdfld_dsi_panel_reset;
	p_funcs->drv_ic_init = mdfld_dsi_brightness_init;
}

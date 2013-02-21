/*
 * Copyright © 2010 Intel Corporation
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
 *  jim liu <jim.liu@intel.com>
 *  Jackie Li<yaodong.li@intel.com>
 */

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include "mdfld_dsi_pkg_sender.h"

#include "psb_powermgmt.h"

static int enter_dsr;
struct mdfld_dsi_dbi_output *gdbi_output;

#define MDFLD_DSR_MAX_IDLE_COUNT	2
#define MDFLD_DSI_MAX_RETURN_PACKET_SIZE	512

/**
 * set refreshing area
 */
int mdfld_dsi_dbi_update_area(struct mdfld_dsi_dbi_output * dbi_output,
				u16 x1, u16 y1, u16 x2, u16 y2)
{
	struct mdfld_dsi_pkg_sender * sender = 
		mdfld_dsi_encoder_get_pkg_sender(&dbi_output->base);
	u8 param[5];
	int err;

	if(!sender) {
		DRM_ERROR("Cannot get PKG sender\n");
		return -EINVAL;
	}

	/*set column*/
	param[0] = DCS_SET_COLUMN_ADDRESS;
	param[1] = x1 >> 8;
	param[2] = x1;
	param[3] = x2 >> 8;
	param[4] = x2;

	err = mdfld_dsi_send_mcs_long(sender, param, 5, true);
	if(err) {
		DRM_ERROR("DCS_SET_COLUMN_ADDRESS sent failed\n");
		goto err_out;
	}

	/*set page*/
	param[0] = DCS_SET_PAGE_ADDRESS;
	param[1] = y1 >> 8;
	param[2] = y1;
	param[3] = y2 >> 8;
	param[4] = y2;

	err = mdfld_dsi_send_mcs_long(sender, param, 5, true);
	if(err) {
		DRM_ERROR("DCS_SET_PAGE_ADDRESS sent failed\n");
		goto err_out;
	}

	/*update screen*/
	err = mdfld_dsi_write_mem_start(sender);
	if(err) {
		DRM_ERROR("DCS_WRITE_MEM_START sent failed\n");
		goto err_out;
        }

err_out:
        return err;
}

/**
 * set panel's power state
 */ 
int mdfld_dsi_dbi_update_power(struct mdfld_dsi_dbi_output * dbi_output, int mode)
{
	struct drm_device * dev = dbi_output->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct mdfld_dsi_pkg_sender * sender = 
		mdfld_dsi_encoder_get_pkg_sender(&dbi_output->base);
	u32 err = 0;
	
	if(!sender) {
		DRM_ERROR("Cannot get PKG sender\n");
		return -EINVAL;
	}
	
	if(mode == DRM_MODE_DPMS_ON) {
		/*exit sleep mode*/
		err = mdfld_dsi_send_mcs_short(sender, DCS_EXIT_SLEEP_MODE,
					0, 0, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n",
					DCS_EXIT_SLEEP_MODE);
			goto power_err;
		}

		/*set display on*/
		err = mdfld_dsi_send_mcs_short(sender, DCS_SET_DISPLAY_ON,
					0, 0, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n",
					DCS_SET_DISPLAY_ON);
			goto power_err;
		}
		
		/* set tear effect on */
		err = mdfld_dsi_send_mcs_short(sender, DCS_SET_TEAR_ON,
					0, 1, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n", DCS_SET_TEAR_ON);
			goto power_err;
		}
		
		/**
		 * FIXME: remove this later
		 */ 
		err = mdfld_dsi_write_mem_start(sender);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n",
					DCS_WRITE_MEM_START);
			goto power_err;
		}
	} else {
		/*set tear effect off */
		err = mdfld_dsi_send_mcs_short(sender, DCS_SET_TEAR_OFF,
					0, 0, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n", set_tear_off);
			goto power_err;
		}
		
		/*set display off*/
		err = mdfld_dsi_send_mcs_short(sender, DCS_SET_DISPLAY_OFF,
					0, 0, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n",
					DCS_SET_DISPLAY_OFF);
			goto power_err;
		}		
		
		/*enter sleep mode*/
		err = mdfld_dsi_send_mcs_short(sender, DCS_ENTER_SLEEP_MODE,
					0, 0, true);
		if (err) {
			DRM_ERROR("DCS 0x%x sent failed\n",
					DCS_ENTER_SLEEP_MODE);
			goto power_err;
		}
	}
	
power_err:
	return err;
}

/**
 * Enter DSR 
 */
void mdfld_dsi_dbi_enter_dsr (struct mdfld_dsi_dbi_output * dbi_output, int pipe)
{
	u32 reg_val;
	struct drm_device * dev = dbi_output->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct drm_crtc * crtc = dbi_output->base.base.crtc;
	struct psb_intel_crtc * psb_crtc = (crtc) ? to_psb_intel_crtc(crtc) : NULL; 
	u32 dpll_reg = PSB_DSI_PLL_CTRL;
	u32 pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	u32 dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	u32 dspbase_reg = PSB_DSPBASE(PSB_PIPE_A);
	u32 dspsurf_reg = PSB_DSPSURF(PSB_PIPE_A);

	PSB_DEBUG_ENTRY(" \n");
	
	if(!dbi_output)
		return;
	
	gdbi_output = dbi_output;
	if((dbi_output->mode_flags & MODE_SETTING_ON_GOING) ||
		(psb_crtc && psb_crtc->mode_flags & MODE_SETTING_ON_GOING)) 
		return;
		
	if(pipe == 2) {
		dpll_reg = PSB_DSI_PLL_CTRL;
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		dspbase_reg = PSB_DSPBASE(PSB_PIPE_C);
		dspsurf_reg = PSB_DSPSURF(PSB_PIPE_C);
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true)) {
		DRM_ERROR("hw begin failed\n");
		return;
	}
		
	/*disable te interrupts. */
	mdfld_disable_te(dev, pipe);

	/*disable plane*/
	reg_val = REG_READ(dspcntr_reg);
	if(!(reg_val & DISPLAY_PLANE_ENABLE)) {
		REG_WRITE(dspcntr_reg, reg_val & ~DISPLAY_PLANE_ENABLE);
		REG_READ(dspcntr_reg);
	}
	
	/*disable pipe*/
	reg_val = REG_READ(pipeconf_reg);
	if(!(reg_val & DISPLAY_PLANE_ENABLE)) {
		reg_val &= ~DISPLAY_PLANE_ENABLE;
		reg_val |= (PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF);
		REG_WRITE(pipeconf_reg, reg_val);
		REG_READ(pipeconf_reg);
		mdfldWaitForPipeDisable(dev, pipe);
	}
	
	/*disable DPLL*/
	reg_val = REG_READ(dpll_reg);
	if(!(reg_val & DPLL_VCO_ENABLE)) {
		reg_val &= ~DPLL_VCO_ENABLE;
		REG_WRITE(dpll_reg, reg_val);
		REG_READ(dpll_reg);
		udelay(500);
	}
	
	/*gate power of DSI DPLL*/
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	
	/*update mode state to IN_DSR*/
	dbi_output->mode_flags |= MODE_SETTING_IN_DSR;

	if (pipe == 2)
		enter_dsr = 1;
}

#ifndef CONFIG_MDFLD_DSI_DPU
static void mdfld_dbi_output_exit_dsr (struct mdfld_dsi_dbi_output * dbi_output, int pipe)
{
	struct drm_device * dev = dbi_output->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_crtc * crtc = dbi_output->base.base.crtc;
	struct psb_intel_crtc * psb_crtc = (crtc) ? to_psb_intel_crtc(crtc) : NULL; 
	u32 reg_val;
	u32 dpll_reg = PSB_DSI_PLL_CTRL;
	u32 pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	u32 dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);

	/*if mode setting on-going, back off*/
	if((dbi_output->mode_flags & MODE_SETTING_ON_GOING) ||
		(psb_crtc && psb_crtc->mode_flags & MODE_SETTING_ON_GOING)) 
		return;
		
	if(pipe == 2) {
		dpll_reg = PSB_DSI_PLL_CTRL;
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true)) {
		DRM_ERROR("hw begin failed\n");
		return;
	}

	/*enable DPLL*/
	reg_val = REG_READ(dpll_reg);
	if(!(reg_val & DPLL_VCO_ENABLE)) {
		
		if(reg_val & MDFLD_PWR_GATE_EN) {
			reg_val &= ~MDFLD_PWR_GATE_EN;
			REG_WRITE(dpll_reg, reg_val);
			REG_READ(dpll_reg);
			udelay(500);
		}
		
		reg_val |= DPLL_VCO_ENABLE;
		REG_WRITE(dpll_reg, reg_val);
		REG_READ(dpll_reg);
		udelay(500);
		
		/*add timeout*/
		while (!(REG_READ(pipeconf_reg) & PIPECONF_DSIPLL_LOCK)) {
			cpu_relax();
		}
	}
	
	/*enable pipe*/
	reg_val = REG_READ(pipeconf_reg);
	if(!(reg_val & PIPEACONF_ENABLE)) {
		reg_val |= PIPEACONF_ENABLE;
		REG_WRITE(pipeconf_reg, reg_val);
		REG_READ(pipeconf_reg);
		udelay(500);
		mdfldWaitForPipeEnable(dev, pipe);
	}
	
	/*enable plane*/
	reg_val = REG_READ(dspcntr_reg);
	if(!(reg_val & DISPLAY_PLANE_ENABLE)) {
		reg_val |= DISPLAY_PLANE_ENABLE;
		REG_WRITE(dspcntr_reg, reg_val);
		REG_READ(dspcntr_reg);
		udelay(500);
	}

	/*enable TE interrupt on this pipe*/
	mdfld_enable_te(dev, pipe);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	/*clean IN_DSR flag*/
	dbi_output->mode_flags &= ~MODE_SETTING_IN_DSR;
}

/**
 * Exit from DSR 
 */
void mdfld_dsi_dbi_exit_dsr(struct drm_device *dev, u32 update_src)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dbi_dsr_info * dsr_info = dev_priv->dbi_dsr_info;
	struct mdfld_dsi_dbi_output ** dbi_output;
	int i;
	int pipe;
	
	PSB_DEBUG_ENTRY("\n");

	dbi_output = dsr_info->dbi_outputs;

	/*for each output, exit dsr*/
	for(i=0; i<dsr_info->dbi_output_num; i++) {
		/*if panel has been turned off, skip*/
		if (!dbi_output[i] || !dbi_output[i]->dbi_panel_on)
			continue;

		pipe = dbi_output[i]->channel_num ? 2 : 0;
		enter_dsr = 0;
		mdfld_dbi_output_exit_dsr(dbi_output[i], pipe);
	}
	
	dev_priv->dsr_fb_update |= update_src;
}

static bool mdfld_dbi_is_in_dsr(struct drm_device * dev)
{
	if (REG_READ(PSB_DSI_PLL_CTRL) & DPLL_VCO_ENABLE)
		return false;
	if ((REG_READ(PSB_PIPECONF(PSB_PIPE_A)) & PIPEACONF_ENABLE) ||
	    (REG_READ(PSB_PIPECONF(PSB_PIPE_C)) & PIPEACONF_ENABLE))
		return false;
	if ((REG_READ(PSB_DSPCNTR(PSB_PIPE_A)) & DISPLAY_PLANE_ENABLE) ||
	    (REG_READ(PSB_DSPCNTR(PSB_PIPE_C)) & DISPLAY_PLANE_ENABLE))
		return false;
	
	return true;
}

/* Perodically update dbi panel */
void mdfld_dbi_update_panel (struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct mdfld_dbi_dsr_info *dsr_info = dev_priv->dbi_dsr_info;
	struct mdfld_dsi_dbi_output **dbi_outputs;
	struct mdfld_dsi_dbi_output *dbi_output;
	int i;
	int can_enter_dsr = 0;
	int enter_dsr = 0;
	u32 damage_mask = 0;

	dbi_outputs = dsr_info->dbi_outputs;
	dbi_output = pipe ? dbi_outputs[1] : dbi_outputs[0];

	if (!dbi_output)
		return;

	if (pipe == 0)
		damage_mask = dev_priv->dsr_fb_update & MDFLD_DSR_DAMAGE_MASK_0;
	else if (pipe == 2)
		damage_mask = dev_priv->dsr_fb_update & MDFLD_DSR_DAMAGE_MASK_2;
	else
		return;

	/*if FB is damaged and panel is on update on-panel FB*/
	if (damage_mask && dbi_output->dbi_panel_on) {
		dbi_output->dsr_fb_update_done = false;

		if (dbi_output->p_funcs->update_fb)
			dbi_output->p_funcs->update_fb(dbi_output, pipe);

		if (dev_priv->b_dsr_enable && dbi_output->dsr_fb_update_done)
			dev_priv->dsr_fb_update &= ~damage_mask;

		/*clean IN_DSR flag*/
		dbi_output->mode_flags &= ~MODE_SETTING_IN_DSR;
 
		dbi_output->dsr_idle_count = 0;

	} else {
		dbi_output->dsr_idle_count++;
	}

	switch (dsr_info->dbi_output_num) {
	case 1:
		if (dbi_output->dsr_idle_count > MDFLD_DSR_MAX_IDLE_COUNT)
			can_enter_dsr = 1;
		break;
	case 2:
		if (dbi_outputs[0]->dsr_idle_count > MDFLD_DSR_MAX_IDLE_COUNT
		   && dbi_outputs[1]->dsr_idle_count > MDFLD_DSR_MAX_IDLE_COUNT)
			can_enter_dsr = 1;
		break;
	default:
		DRM_ERROR("Wrong DBI output number\n");
		can_enter_dsr = 0;
	}

	/*try to enter DSR*/
	if (can_enter_dsr) {
		for(i=0; i<dsr_info->dbi_output_num; i++) {
			if (!mdfld_dbi_is_in_dsr(dev) && dbi_outputs[i] &&
			   !(dbi_outputs[i]->mode_flags & MODE_SETTING_ON_GOING)) {
				mdfld_dsi_dbi_enter_dsr(dbi_outputs[i],
					dbi_outputs[i]->channel_num ? 2 : 0);
#if 0
				enter_dsr = 1;
				printk(KERN_ALERT "%s: enter_dsr = 1 \n", __func__);
#endif
			}
		}
	}
}

int mdfld_dbi_dsr_init(struct drm_device * dev) 
{
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct mdfld_dbi_dsr_info * dsr_info = dev_priv->dbi_dsr_info;
	
	if(!dsr_info || IS_ERR(dsr_info)) {
		dsr_info = kzalloc(sizeof(struct mdfld_dbi_dsr_info), GFP_KERNEL);
		if(!dsr_info) {
			DRM_ERROR("No memory\n");
			return -ENOMEM;
		}
		
		dev_priv->dbi_dsr_info = dsr_info;
	}

	PSB_DEBUG_ENTRY("successfully\n");
	
	return 0;
}

void mdfld_dbi_dsr_exit(struct drm_device * dev)
{
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct mdfld_dbi_dsr_info * dsr_info = dev_priv->dbi_dsr_info;
	
	if(!dsr_info) {
		return;
	}
	
	/*free dsr info*/
	kfree(dsr_info);
	
	dev_priv->dbi_dsr_info = NULL;
}
#endif

void mdfld_dsi_controller_dbi_init(struct mdfld_dsi_config * dsi_config, int pipe)
{
	struct drm_device * dev = dsi_config->dev;
	int lane_count = dsi_config->lane_count;
	u32 val = 0;

	PSB_DEBUG_ENTRY("Init DBI interface on pipe %d...\n", pipe);

	/*un-ready device*/
	REG_WRITE(MIPI_DEVICE_READY_REG(pipe), 0x00000000);
	
	/*init dsi adapter before kicking off*/
	REG_WRITE(MIPI_CTRL_REG(pipe), 0x00000018);
	
	/*TODO: figure out how to setup these registers*/
	REG_WRITE(MIPI_DPHY_PARAM_REG(pipe), 0x150c3408);
	REG_WRITE(MIPI_CLK_LANE_SWITCH_TIME_CNT_REG(pipe), 0x000a0014);
	REG_WRITE(MIPI_DBI_BW_CTRL_REG(pipe), 0x00000400);
	REG_WRITE(MIPI_DBI_FIFO_THROTTLE_REG(pipe), 0x00000001);
	REG_WRITE(MIPI_HS_LS_DBI_ENABLE_REG(pipe), 0x00000000);
	
	/*enable all interrupts*/
	REG_WRITE(MIPI_INTR_EN_REG(pipe), 0xffffffff);
	/*max value: 20 clock cycles of txclkesc*/
	REG_WRITE(MIPI_TURN_AROUND_TIMEOUT_REG(pipe), 0x0000001f);
	/*min 21 txclkesc, max: ffffh*/
	REG_WRITE(MIPI_DEVICE_RESET_TIMER_REG(pipe), 0x0000ffff);
	/*min: 7d0 max: 4e20*/
	REG_WRITE(MIPI_INIT_COUNT_REG(pipe), 0x00000fa0);
		
	/* set up max return packet size */
	REG_WRITE(MIPI_MAX_RETURN_PACK_SIZE_REG(pipe),
		MDFLD_DSI_MAX_RETURN_PACKET_SIZE);

	/*set up func_prg*/
	val |= lane_count;
	val |= (dsi_config->channel_num << DSI_DBI_VIRT_CHANNEL_OFFSET);
	val |= DSI_DBI_COLOR_FORMAT_OPTION2;
	REG_WRITE(MIPI_DSI_FUNC_PRG_REG(pipe), val);
	
	REG_WRITE(MIPI_HS_TX_TIMEOUT_REG(pipe), 0x3fffff);
	REG_WRITE(MIPI_LP_RX_TIMEOUT_REG(pipe), 0xffff);

	/*de-assert dbi_stall when half of DBI FIFO is empty*/
	//REG_WRITE((MIPIA_DBI_FIFO_THROTTLE_REG + reg_offset), 0x00000000);
	
	REG_WRITE(MIPI_HIGH_LOW_SWITCH_COUNT_REG(pipe), 0x46);
	REG_WRITE(MIPI_EOT_DISABLE_REG(pipe), 0x00000000);
	REG_WRITE(MIPI_LP_BYTECLK_REG(pipe), 0x00000004);
	REG_WRITE(MIPI_DEVICE_READY_REG(pipe), 0x00000001);
}

#if 0
/*DBI encoder helper funcs*/
static const struct drm_encoder_helper_funcs mdfld_dsi_dbi_helper_funcs = {
	.dpms = mdfld_dsi_dbi_dpms,
	.mode_fixup = mdfld_dsi_dbi_mode_fixup,
	.prepare = mdfld_dsi_dbi_prepare,
	.mode_set = mdfld_dsi_dbi_mode_set,
	.commit = mdfld_dsi_dbi_commit,
};

/*DBI encoder funcs*/
static const struct drm_encoder_funcs mdfld_dsi_dbi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

#endif

/*
 * Init DSI DBI encoder. 
 * Allocate an mdfld_dsi_encoder and attach it to given @dsi_connector
 * return pointer of newly allocated DBI encoder, NULL on error
 */ 
struct mdfld_dsi_encoder * mdfld_dsi_dbi_init(struct drm_device * dev, 
					struct mdfld_dsi_connector * dsi_connector,
					struct panel_funcs* p_funcs)
{
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct mdfld_dsi_dbi_output * dbi_output = NULL;
	struct mdfld_dsi_config * dsi_config;
	struct drm_connector * connector = NULL;
	struct drm_encoder * encoder = NULL;
	struct drm_display_mode * fixed_mode = NULL;
	struct psb_gtt * pg = dev_priv ? (dev_priv->pg) : NULL;

#ifdef CONFIG_MDFLD_DSI_DPU
	struct mdfld_dbi_dpu_info * dpu_info = dev_priv ? (dev_priv->dbi_dpu_info) : NULL;
#else
	struct mdfld_dbi_dsr_info * dsr_info = dev_priv ? (dev_priv->dbi_dsr_info) : NULL;
#endif	
	u32 data = 0;
	int pipe;
	int ret;

	PSB_DEBUG_ENTRY("\n");

	if (!pg || !dsi_connector || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return NULL;
	}

	dsi_config = mdfld_dsi_get_config(dsi_connector);
	pipe = dsi_connector->pipe;

	/*panel hard-reset*/
	if (p_funcs->reset) {
		ret = p_funcs->reset(pipe);
		if (ret) {
			DRM_ERROR("Panel %d hard-reset failed\n", pipe);
			return NULL;
		}
	}

	/*panel drvIC init*/
	if (p_funcs->drv_ic_init)
		p_funcs->drv_ic_init(dsi_config, pipe);

	/*panel power mode detect*/
	ret = mdfld_dsi_get_power_mode(dsi_config, &data, true);
	if (ret) {
		DRM_ERROR("Panel %d get power mode failed\n", pipe);

		dsi_connector->status = connector_status_disconnected;
	} else {
		DRM_INFO("pipe %d power mode 0x%x\n", pipe, data);

		dsi_connector->status = connector_status_connected;
	}

	/*TODO: get panel info from DDB*/

	dbi_output = kzalloc(sizeof(struct mdfld_dsi_dbi_output), GFP_KERNEL);
	if(!dbi_output) {
		DRM_ERROR("No memory\n");
		return NULL;
	}

	if(dsi_connector->pipe == 0) {
		dbi_output->channel_num = 0;
		dev_priv->dbi_output = dbi_output;
	} else if (dsi_connector->pipe == 2) {
		dbi_output->channel_num = 1;
		dev_priv->dbi_output2 = dbi_output;
	} else {
		DRM_ERROR("only support 2 DSI outputs\n");
		goto out_err1;
	}
	
	dbi_output->dev = dev;
	dbi_output->p_funcs = p_funcs;
	
	/*get fixed mode*/
	fixed_mode = dsi_config->fixed_mode;
	
	dbi_output->panel_fixed_mode = fixed_mode;
	
	/*create drm encoder object*/
	connector = &dsi_connector->base.base;
	encoder = &dbi_output->base.base;
	drm_encoder_init(dev,
			encoder,
			p_funcs->encoder_funcs,
			DRM_MODE_ENCODER_LVDS);
	drm_encoder_helper_add( encoder,
				p_funcs->encoder_helper_funcs);
	
	/*attach to given connector*/
	drm_mode_connector_attach_encoder(connector, encoder);
	
	/*set possible crtcs and clones*/
	if(dsi_connector->pipe) {
		encoder->possible_crtcs = (1 << 2);
		encoder->possible_clones = (1 << 1);
	} else {
		encoder->possible_crtcs = (1 << 0);
		encoder->possible_clones = (1 << 0);
	}

	dev_priv->dsr_fb_update = 0;
	dev_priv->b_dsr_enable = false;
	
	dbi_output->first_boot = true;
	dbi_output->mode_flags = MODE_SETTING_IN_ENCODER;

#ifdef CONFIG_MDFLD_DSI_DPU
	/*add this output to dpu_info*/

	if (dsi_connector->status == connector_status_connected) {
		if (dsi_connector->pipe == 0)
			dpu_info->dbi_outputs[0] = dbi_output;
		else
			dpu_info->dbi_outputs[1] = dbi_output;

		dpu_info->dbi_output_num++;
	}

#else /*CONFIG_MDFLD_DSI_DPU*/	
	if (dsi_connector->status == connector_status_connected) {
		/*add this output to dsr_info*/
		if (dsi_connector->pipe == 0)
			dsr_info->dbi_outputs[0] = dbi_output;
		else
			dsr_info->dbi_outputs[1] = dbi_output;

		dsr_info->dbi_output_num++;
	}
#endif

	PSB_DEBUG_ENTRY("successfully\n");
	
	return &dbi_output->base;
	
out_err1: 
	if(dbi_output) {
		kfree(dbi_output);
	}
	
	return NULL;
}

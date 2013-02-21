/**************************************************************************
 * Copyright (c) 2009, Intel Corporation.
 * All Rights Reserved.

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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Benjamin Defnet <benjamin.r.defnet@intel.com>
 *    Rajesh Poornachandran <rajesh.poornachandran@intel.com>
 *
 */
#include "psb_powermgmt.h"
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include "mdfld_gl3.h"
#include "pvr_trace_cmd.h"

#include <linux/mutex.h>
#include <linux/intel_mid_pm.h>
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"
#include <asm/intel_scu_ipc.h>

#undef OSPM_GFX_DPK

struct drm_device *gpDrmDevice = NULL;
static struct mutex g_ospm_mutex;
static int g_hw_power_status_mask;
static atomic_t g_display_access_count;
static atomic_t g_graphics_access_count;
static atomic_t g_videoenc_access_count;
static atomic_t g_videodec_access_count;

void ospm_power_island_up(int hw_islands);
void ospm_power_island_down(int hw_islands);
static bool gbSuspended = false;
static int psb_runtime_hdmi_audio_suspend(struct drm_device *drm_dev);

#if 1
static int ospm_runtime_check_msvdx_hw_busy(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int ret = 1;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO DEC HW is not on\n", __func__);
		ret = -1;
		goto out;
	}

	msvdx_priv->msvdx_hw_busy = REG_READ(0x20D0) & (0x1 << 9);
	if (psb_check_msvdx_idle(dev)) {
		//printk(KERN_ALERT "%s video decode hw busy\n", __func__);
		ret = 1;
	} else {
		//printk(KERN_ALERT "%s video decode hw idle\n", __func__);
		ret = 0;
	}
out:
	return ret;
}

static int ospm_runtime_check_topaz_hw_busy(struct drm_device *dev)
{
	//struct drm_psb_private *dev_priv = dev->dev_private;
	//struct topaz_private *topaz_priv = dev_priv->topaz_private;
	int ret = 1;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO ENC HW is not on\n", __func__);
		ret = -1;
		goto out;
	}

	//topaz_priv->topaz_hw_busy = REG_READ(0x20D0) & (0x1 << 11);

	if (pnw_check_topaz_idle(dev)) {
		//printk(KERN_ALERT "%s video encode hw busy %d\n", __func__,
		//       topaz_priv->topaz_hw_busy);
		ret = 1;
	} else {
		//printk(KERN_ALERT "%s video encode hw idle\n", __func__);
		ret = 0;
	}
out:
	return ret;
}

static int ospm_runtime_pm_msvdx_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	//printk(KERN_ALERT "enter %s\n", __func__);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO DEC HW is not on\n", __func__);
		goto out;
	}

	if (atomic_read(&g_videodec_access_count)) {
		//printk(KERN_ALERT "%s videodec access count exit\n", __func__);
		ret = -1;
		goto out;
	}

	msvdx_priv->msvdx_hw_busy = REG_READ(0x20D0) & (0x1 << 9);
	if (psb_check_msvdx_idle(dev)) {
		//printk(KERN_ALERT "%s video decode hw busy exit\n", __func__);
		ret = -2;
		goto out;
	}

	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);
	psb_irq_uninstall_islands(dev, OSPM_VIDEO_DEC_ISLAND);
	psb_msvdx_save_context(dev);
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
	//printk(KERN_ALERT "%s done\n", __func__);
out:
	return ret;
}

static int ospm_runtime_pm_msvdx_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	//printk(KERN_ALERT "ospm_runtime_pm_msvdx_resume\n");

	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERUP);

	psb_msvdx_restore_context(dev);

	return 0;
}

static int ospm_runtime_pm_topaz_suspend(struct drm_device *dev)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;

	//printk(KERN_ALERT "enter %s\n", __func__);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

	if (encode_ctx)
		PSB_DEBUG_PM("Topaz: has encode context, running=%d\n",
			     encode_running);
	else
		PSB_DEBUG_PM("Topaz: no encode context\n");

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		//printk(KERN_ALERT "%s VIDEO ENC HW is not on\n", __func__);
		goto out;
	}

	if (atomic_read(&g_videoenc_access_count)) {
		//printk(KERN_ALERT "%s videoenc access count exit\n", __func__);
		ret = -1;
		goto out;
	}

	if (pnw_check_topaz_idle(dev)) {
		//printk(KERN_ALERT "%s video encode hw busy exit\n", __func__);
		ret = -2;
		goto out;
	}

	psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);

	if (encode_running) /* has encode session running */
		pnw_topaz_save_mtx_state(dev);
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERDOWN);

	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
	//printk(KERN_ALERT "%s done\n", __func__);
out:
	return ret;
}

static int ospm_runtime_pm_topaz_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	int encode_ctx = 0, encode_running = 0;

	//printk(KERN_ALERT "ospm_runtime_pm_topaz_resume\n");
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		int entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture) {
			encode_ctx = 1;
			break;
		}
	}

	/* have encode context, but not started, or is just closed */
	if (encode_ctx && dev_priv->topaz_ctx)
		encode_running = 1;

	if (encode_ctx)
		PSB_DEBUG_PM("Topaz: has encode context, running=%d\n",
			     encode_running);
	else
		PSB_DEBUG_PM("Topaz: no encode running\n");

	if (encode_running) { /* has encode session running */
		psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
		pnw_topaz_restore_mtx_state(dev);
	}
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERUP);

	return 0;
}
#endif

#ifdef FIX_OSPM_POWER_DOWN
void ospm_apm_power_down_msvdx(struct drm_device *dev)
{
	return;
	mutex_lock(&g_ospm_mutex);

	if (atomic_read(&g_videodec_access_count))
		goto out;
	if (psb_check_msvdx_idle(dev))
		goto out;

	psb_msvdx_save_context(dev);
#ifdef FIXME_MRST_VIDEO_DEC
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
#endif
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	return; /* todo for OSPM */

	mutex_lock(&g_ospm_mutex);

	if (atomic_read(&g_videoenc_access_count))
		goto out;
	if (lnc_check_topaz_idle(dev))
		goto out;

	lnc_topaz_save_mtx_state(dev);
	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}
#else
void ospm_apm_power_down_msvdx(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	mutex_lock(&g_ospm_mutex);
	if (!ospm_power_is_hw_on(OSPM_VIDEO_DEC_ISLAND))
		goto out;

	if (atomic_read(&g_videodec_access_count))
		goto out;
	if (psb_check_msvdx_idle(dev))
		goto out;

	psb_msvdx_save_context(dev);
	ospm_power_island_down(OSPM_VIDEO_DEC_ISLAND);
	MSVDX_NEW_PMSTATE(dev, msvdx_priv, PSB_PMSTATE_POWERDOWN);
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}

void ospm_apm_power_down_topaz(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *pnw_topaz_priv = dev_priv->topaz_private;

	mutex_lock(&g_ospm_mutex);

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND))
		goto out;
	if (atomic_read(&g_videoenc_access_count))
		goto out;
	if (pnw_check_topaz_idle(dev))
		goto out;

	psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	pnw_topaz_save_mtx_state(dev);
	PNW_TOPAZ_NEW_PMSTATE(dev, pnw_topaz_priv, PSB_PMSTATE_POWERDOWN);

	ospm_power_island_down(OSPM_VIDEO_ENC_ISLAND);
out:
	mutex_unlock(&g_ospm_mutex);
	return;
}
#endif

#ifdef CONFIG_EARLYSUSPEND
/*
 * REVISIT: The early suspend and late resume handlers should not call
 * pm_runtime_put() and pm_runtime_get_sync() directly, but rather the DPMS
 * handlers should do it.
 */
static void gfx_early_suspend(struct early_suspend *es)
{
	struct drm_psb_private *dev_priv =
		container_of(es, struct drm_psb_private, early_suspend);
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;

	dev_dbg(&dev->pdev->dev, "%s\n", __func__);
	dev_priv->hdmi_audio_busy =
			psb_runtime_hdmi_audio_suspend(dev) == -EBUSY;

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct drm_encoder_helper_funcs *ehf = encoder->helper_private;
		if (drm_helper_encoder_in_use(encoder) && ehf && ehf->dpms
#ifndef JIRA_ANDROID-1553
	/*
	 * Local MIPI fails to turn back on from a DPMS off/on cycle if
	 * HDMI audio returns busy to disallow system suspend.
	 * Once ANDROID-1553 is fixed, the expectation is to turn off
	 * MIPI but keep display island on if there is active audio
	 * playback over HDMI.
	 * Refer Jira bug# Android-1553 for more details.
	 */
			&& !dev_priv->hdmi_audio_busy
#endif
		)
			ehf->dpms(encoder, DRM_MODE_DPMS_OFF);
	}
	mutex_unlock(&dev->mode_config.mutex);

	pm_runtime_put(&dev->pdev->dev);
}

static void gfx_late_resume(struct early_suspend *es)
{
	struct drm_psb_private *dev_priv =
		container_of(es, struct drm_psb_private, early_suspend);
	struct drm_device *dev = dev_priv->dev;
	struct drm_encoder *encoder;

	dev_dbg(&dev->pdev->dev, "%s\n", __func__);

	pm_runtime_get_sync(&dev->pdev->dev);

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct drm_encoder_helper_funcs *ehf = encoder->helper_private;

		if (drm_helper_encoder_in_use(encoder) && ehf && ehf->mode_set
		    && ehf->dpms
#ifndef JIRA_ANDROID-1553
	/*
	  Local MIPI fails to turn back on from a DPMS off/on cycle if HDMI
	  audio returns busy to disallow system suspend.
	  Once ANDROID-1553 is fixed, the expectation is to turn off MIPI but
	  keep display island on if there is active audio playback over HDMI
	  Refer Jira bug# Android-1553 for more details.
	*/
			&& !(dev_priv->hdmi_audio_busy)
#endif
		) {
			struct drm_crtc *crtc = encoder->crtc;

			if (crtc)
				ehf->mode_set(encoder,
					      &crtc->mode,
					      &crtc->hwmode);
			ehf->dpms(encoder, DRM_MODE_DPMS_ON);
		}
	}
	mutex_unlock(&dev->mode_config.mutex);
}
#endif

static inline unsigned long palette_reg(int pipe, int idx)
{
	return PSB_PALETTE(pipe) + (idx << 2);
}

/*
 * mdfld_save_pipe_registers
 *
 * Description: We are going to suspend so save current display
 * register state.
 *
 * Notes: FIXME_JLIU7 need to add the support for DPI MIPI & HDMI audio
 */
static int mdfld_save_pipe_registers(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_pipe_regs *pr = &dev_priv->pipe_regs[pipe];
	int i;

	PSB_DEBUG_ENTRY("\n");

	switch (pipe) {
	case 0:
		pr->pll_ctrl = PSB_RVDC32(PSB_DSI_PLL_CTRL);
		pr->pll_div = PSB_RVDC32(PSB_DSI_PLL_DIV_M1);
		pr->mipi_ctrl = PSB_RVDC32(MIPI_PORT_CONTROL(pipe));
		break;
	case 1:
		pr->pll_ctrl = PSB_RVDC32(PSB_DPLL_CTRL);
		pr->pll_div = PSB_RVDC32(PSB_DPLL_DIV0);
		dev_priv->savePFIT_CONTROL = PSB_RVDC32(PFIT_CONTROL);
		dev_priv->savePFIT_PGM_RATIOS = PSB_RVDC32(PFIT_PGM_RATIOS);
		dev_priv->saveHDMIPHYMISCCTL = PSB_RVDC32(HDMIPHYMISCCTL);
		dev_priv->saveHDMIB_CONTROL = PSB_RVDC32(HDMIB_CONTROL);
		break;
	case 2:
		pr->mipi_ctrl = PSB_RVDC32(MIPI_PORT_CONTROL(pipe));
		break;
	default:
		DRM_ERROR("%s, invalid pipe number. \n", __FUNCTION__);
		return -EINVAL;
	}

	/* Pipe & plane A info */
	pr->pipe_conf = PSB_RVDC32(PSB_PIPECONF(pipe));
	pr->dsp_cntr = PSB_RVDC32(PSB_DSPCNTR(pipe));

	pr->htotal		= PSB_RVDC32(PSB_HTOTAL(pipe));
	pr->hblank		= PSB_RVDC32(PSB_HBLANK(pipe));
	pr->hsync		= PSB_RVDC32(PSB_HSYNC(pipe));
	pr->vtotal		= PSB_RVDC32(PSB_VTOTAL(pipe));
	pr->vblank		= PSB_RVDC32(PSB_VBLANK(pipe));
	pr->vsync		= PSB_RVDC32(PSB_VSYNC(pipe));
	pr->src			= PSB_RVDC32(PSB_PIPESRC(pipe));
	pr->dsp_stride		= PSB_RVDC32(PSB_DSPSTRIDE(pipe));
	pr->dsp_line_offs	= PSB_RVDC32(PSB_DSPLINOFF(pipe));
	pr->dsp_tile_offs	= PSB_RVDC32(PSB_DSPTILEOFF(pipe));
	pr->dsp_size		= PSB_RVDC32(PSB_DSPSIZE(pipe));
	pr->dsp_pos		= PSB_RVDC32(PSB_DSPPOS(pipe));
	pr->dsp_surf		= PSB_RVDC32(PSB_DSPSURF(pipe));
	pr->dsp_status		= PSB_RVDC32(PSB_PIPESTAT(pipe));

	/*save palette (gamma) */
	for (i = 0; i < ARRAY_SIZE(pr->palette); i++)
		pr->palette[i] = PSB_RVDC32(palette_reg(pipe, i));

	return 0;
}
/*
 * mdfld_save_cursor_overlay_registers
 *
 * Description: We are going to suspend so save current cursor and overlay display
 * register state.
 */
static int mdfld_save_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i;

	/*save cursor regs*/
	dev_priv->saveDSPACURSOR_CTRL = PSB_RVDC32(CURACNTR);
	dev_priv->saveDSPACURSOR_BASE = PSB_RVDC32(CURABASE);
	dev_priv->saveDSPACURSOR_POS = PSB_RVDC32(CURAPOS);

	dev_priv->saveDSPBCURSOR_CTRL = PSB_RVDC32(CURBCNTR);
	dev_priv->saveDSPBCURSOR_BASE = PSB_RVDC32(CURBBASE);
	dev_priv->saveDSPBCURSOR_POS = PSB_RVDC32(CURBPOS);

	dev_priv->saveDSPCCURSOR_CTRL = PSB_RVDC32(CURCCNTR);
	dev_priv->saveDSPCCURSOR_BASE = PSB_RVDC32(CURCBASE);
	dev_priv->saveDSPCCURSOR_POS = PSB_RVDC32(CURCPOS);

	/* HW overlay */
	for (i = 0; i < ARRAY_SIZE(dev_priv->overlays); i++)
		mdfld_overlay_suspend(dev_priv->overlays[i]);

	return 0;
}
/*
 * mdfld_restore_pipe_registers
 *
 * Description: We are going to resume so restore display register state.
 *
 * Notes: FIXME_JLIU7 need to add the support for DPI MIPI & HDMI audio
 */
static int mdfld_restore_pipe_registers(struct drm_device *dev, int pipe)
{
	//to get  panel out of ULPS mode.
	u32 temp = 0;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_pipe_regs *pr = &dev_priv->pipe_regs[pipe];
	struct mdfld_dsi_dbi_output * dsi_output = dev_priv->dbi_output;
	struct mdfld_dsi_config * dsi_config = NULL;
	u32 i = 0;
	u32 dpll = 0;
	u32 dpll_reg = 0;
	u32 pll_div_reg = 0;
	u32 dpll_val;

	PSB_DEBUG_ENTRY("\n");

	switch (pipe) {
	case 0:
		dpll_reg = PSB_DSI_PLL_CTRL;
		pll_div_reg = PSB_DSI_PLL_DIV_M1;
		dsi_config = dev_priv->dsi_configs[0];
		break;
	case 1:
		dpll_reg = PSB_DPLL_CTRL;
		pll_div_reg = PSB_DPLL_DIV0;
		break;
	case 2:
		dsi_output = dev_priv->dbi_output2;
		dsi_config = dev_priv->dsi_configs[1];
		break;
	default:
		DRM_ERROR("%s, invalid pipe number. \n", __FUNCTION__);
		return -EINVAL;
	}

	dpll_val = pr->pll_ctrl & ~DPLL_VCO_ENABLE;

	/*make sure VGA plane is off. it initializes to on after reset!*/
	PSB_WVDC32(0x80000000, VGACNTRL);

	switch (pipe) {
	case 1:
		PSB_WVDC32(dpll_val & ~DPLL_VCO_ENABLE, dpll_reg);
		PSB_RVDC32(dpll_reg);
		PSB_WVDC32(pr->pll_div, pll_div_reg);
		break;

	case 0:
		dpll = PSB_RVDC32(dpll_reg);

		if (!(dpll & DPLL_VCO_ENABLE)) {
			/* When ungating power of DPLL, needs to wait 0.5us before enable the VCO */
			if (dpll & MDFLD_PWR_GATE_EN) {
				dpll &= ~MDFLD_PWR_GATE_EN;
				PSB_WVDC32(dpll, dpll_reg);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(500);
			}

			PSB_WVDC32(pr->pll_div, pll_div_reg);
			PSB_WVDC32(dpll_val, dpll_reg);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);

			dpll_val |= DPLL_VCO_ENABLE;
			PSB_WVDC32(dpll_val, dpll_reg);
			PSB_RVDC32(dpll_reg);

			if (REG_FLAG_WAIT_SET(PSB_PIPECONF(pipe),
					      PIPECONF_DSIPLL_LOCK)) {
				DRM_ERROR("%s, can't lock DSIPLL.\n", __func__);
				return -EINVAL;
			}
		}
		break;
	}

	PSB_WVDC32(pr->htotal,		PSB_HTOTAL(pipe));
	PSB_WVDC32(pr->hblank,		PSB_HBLANK(pipe));
	PSB_WVDC32(pr->hsync,		PSB_HSYNC(pipe));
	PSB_WVDC32(pr->vtotal,		PSB_VTOTAL(pipe));
	PSB_WVDC32(pr->vblank,		PSB_VBLANK(pipe));
	PSB_WVDC32(pr->vsync,		PSB_VSYNC(pipe));
	PSB_WVDC32(pr->src,		PSB_PIPESRC(pipe));
	PSB_WVDC32(pr->dsp_status,	PSB_PIPESTAT(pipe));
	PSB_WVDC32(pr->dsp_stride,	PSB_DSPSTRIDE(pipe));
	PSB_WVDC32(pr->dsp_line_offs,	PSB_DSPLINOFF(pipe));
	PSB_WVDC32(pr->dsp_tile_offs,	PSB_DSPTILEOFF(pipe));
	PSB_WVDC32(pr->dsp_size,	PSB_DSPSIZE(pipe));
	PSB_WVDC32(pr->dsp_pos,		PSB_DSPPOS(pipe));
	PSB_WVDC32(pr->dsp_surf,	PSB_DSPSURF(pipe));

	if (pipe == 1) {
		/* restore palette (gamma) */
		/*DRM_UDELAY(50000); */
		for (i = 0; i < ARRAY_SIZE(pr->palette); i++)
			PSB_WVDC32(pr->palette[i], palette_reg(pipe, i));

		PSB_WVDC32(dev_priv->savePFIT_CONTROL, PFIT_CONTROL);
		PSB_WVDC32(dev_priv->savePFIT_PGM_RATIOS, PFIT_PGM_RATIOS);

		/*TODO: resume HDMI port */

		/*TODO: resume pipe*/

		/*enable the plane*/
		PSB_WVDC32(pr->dsp_cntr & ~DISPLAY_PLANE_ENABLE,
			   PSB_DSPCNTR(pipe));

		return 0;
	}

	/*set up pipe related registers*/
	PSB_WVDC32(pr->mipi_ctrl, MIPI_PORT_CONTROL(pipe));

	/*setup MIPI adapter + MIPI IP registers*/
	if (dsi_config)
		mdfld_dsi_controller_init(dsi_config, pipe);

	if (in_atomic() || in_interrupt())
		mdelay(20);
	else
		msleep(20);

	/*enable the plane*/
	PSB_WVDC32(pr->dsp_cntr, PSB_DSPCNTR(pipe));

	if (in_atomic() || in_interrupt())
		mdelay(20);
	else
		msleep(20);

	/* LP Hold Release */
	temp = REG_READ(MIPI_PORT_CONTROL(pipe));
	temp |= LP_OUTPUT_HOLD_RELEASE;
	REG_WRITE(MIPI_PORT_CONTROL(pipe), temp);
	mdelay(1);

	if (pipe == PSB_PIPE_A) {
		/* Set DSI host to exit from Utra Low Power State */
		temp = REG_READ(MIPI_DEVICE_READY_REG(pipe));
		temp &= ~ULPS_MASK;
		temp |= 0x3;
		temp |= EXIT_ULPS_DEV_READY;
		REG_WRITE(MIPI_DEVICE_READY_REG(pipe), temp);
		mdelay(1);

		temp = REG_READ(MIPI_DEVICE_READY_REG(pipe));
		temp &= ~ULPS_MASK;
		temp |= EXITING_ULPS;
		REG_WRITE(MIPI_DEVICE_READY_REG(pipe), temp);
		mdelay(1);
	}

	/*enable the pipe*/
	PSB_WVDC32(pr->pipe_conf, PSB_PIPECONF(pipe));

	/* restore palette (gamma) */
	/*DRM_UDELAY(50000); */
	for (i = 0; i < ARRAY_SIZE(pr->palette); i++)
		PSB_WVDC32(pr->palette[i], palette_reg(pipe, i));

	return 0;
}

/*
 * mdfld_restore_cursor_overlay_registers
 *
 * Description: We are going to resume so restore cursor and overlay register state.
 */
static int mdfld_restore_cursor_overlay_registers(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i;

	/*Enable Cursor A*/
	PSB_WVDC32(dev_priv->saveDSPACURSOR_CTRL, CURACNTR);
	PSB_WVDC32(dev_priv->saveDSPACURSOR_POS, CURAPOS);
	PSB_WVDC32(dev_priv->saveDSPACURSOR_BASE, CURABASE);

	PSB_WVDC32(dev_priv->saveDSPBCURSOR_CTRL, CURBCNTR);
	PSB_WVDC32(dev_priv->saveDSPBCURSOR_POS, CURBPOS);
	PSB_WVDC32(dev_priv->saveDSPBCURSOR_BASE, CURBBASE);

	PSB_WVDC32(dev_priv->saveDSPCCURSOR_CTRL, CURCCNTR);
	PSB_WVDC32(dev_priv->saveDSPCCURSOR_POS, CURCPOS);
	PSB_WVDC32(dev_priv->saveDSPCCURSOR_BASE, CURCBASE);

	for (i = 0; i < ARRAY_SIZE(dev_priv->overlays); i++)
		mdfld_overlay_resume(dev_priv->overlays[i]);

	return 0;
}

/*
 * powermgmt_suspend_display
 *
 * Description: Suspend the display hardware saving state and disabling
 * as necessary.
 */
static void ospm_suspend_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	//to put panel into ULPS mode.
	u32 temp = 0;
	u32 device_ready_reg = DEVICE_READY_REG;
	u32 mipi_reg = MIPI;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "%s \n", __func__);
#endif
	if (!(g_hw_power_status_mask & OSPM_DISPLAY_ISLAND))
		return;

	mdfld_save_cursor_overlay_registers(dev);

	mdfld_save_pipe_registers(dev, 0);
	mdfld_save_pipe_registers(dev, 2);
	android_hdmi_save_display_registers(dev);
	/* save the gunit register controlling write-combining */
	dev_priv->savePERF_MODE = PSB_RVDC32(MRST_PERF_MODE);

	mdfld_disable_crtc(dev, 0);
	mdfld_disable_crtc(dev, 2);
	android_disable_hdmi(dev);

	/* Put the panel in ULPS mode for S0ix. */
	temp = REG_READ(device_ready_reg);
	temp &= ~ULPS_MASK;
	temp |= ENTERING_ULPS;
	REG_WRITE(device_ready_reg, temp);

	//LP Hold
	temp = REG_READ(mipi_reg);
	temp &= ~LP_OUTPUT_HOLD;
	REG_WRITE(mipi_reg, temp);
	mdelay(1);

	ospm_power_island_down(OSPM_DISPLAY_ISLAND);
}

/*
 * ospm_resume_display
 *
 * Description: Resume the display hardware restoring state and enabling
 * as necessary.
 */
static void ospm_resume_display(struct drm_device *drm_dev)
{
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "%s \n", __func__);
#endif
	if (g_hw_power_status_mask & OSPM_DISPLAY_ISLAND)
		return;

	/* turn on the display power island */
	ospm_power_island_up(OSPM_DISPLAY_ISLAND);

	PSB_WVDC32(pg->pge_ctl | _PSB_PGETBL_ENABLED, PSB_PGETBL_CTL);
	pci_write_config_word(drm_dev->pdev, PSB_GMCH_CTRL,
			      pg->gmch_ctrl | _PSB_GMCH_ENABLED);

	/* Don't reinitialize the GTT as it is unnecessary.  The gtt is
	 * stored in memory so it will automatically be restored.  All
	 * we need to do is restore the PGETBL_CTL which we already do
	 * above.
	 */
	/*psb_gtt_init(dev_priv->pg, 1);*/

	/* restore gunit register controlling write-combining */
	PSB_WVDC32(dev_priv->savePERF_MODE, MRST_PERF_MODE);
	android_hdmi_restore_and_enable_display(drm_dev);
	mdfld_restore_pipe_registers(drm_dev, 0);
	mdfld_restore_pipe_registers(drm_dev, 2);
	mdfld_restore_cursor_overlay_registers(drm_dev);
}

static void pvrcmd_device_power(unsigned type, enum pvr_trcmd_device dev)
{
	struct pvr_trcmd_power *p;
	if (in_interrupt())
		p = pvr_trcmd_reserve(type, 0, "irq", sizeof *p);
	else
		p = pvr_trcmd_reserve(type, task_tgid_nr(current),
				current->comm, sizeof *p);
	p->dev = dev;
	pvr_trcmd_commit(p);
}

/*
 * ospm_suspend_pci
 *
 * Description: Suspend the pci device saving state and disabling
 * as necessary.
 */
static void ospm_suspend_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (gbSuspended)
		return;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "ospm_suspend_pci\n");
#endif

#ifdef CONFIG_MDFD_GL3
	gl3_invalidate();
#endif
	pvrcmd_device_power(PVR_TRCMD_SUSPEND, PVR_TRCMD_DEVICE_PCI);
	/* Power off GL3 after all GFX sub-systems are powered off. */
	ospm_power_island_down(OSPM_GL3_CACHE_ISLAND);

	pci_read_config_dword(pdev, 0x5C, &dev_priv->saveBSM);
	pci_read_config_dword(pdev, 0xFC, &dev_priv->saveVBT);
	pci_read_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, &dev_priv->msi_addr);
	pci_read_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, &dev_priv->msi_data);

	gbSuspended = true;
}

/*
 * ospm_resume_pci
 *
 * Description: Resume the pci device restoring state and enabling
 * as necessary.
 */
static bool ospm_resume_pci(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (!gbSuspended)
		return true;

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "ospm_resume_pci\n");
#endif
	pvrcmd_device_power(PVR_TRCMD_RESUME, PVR_TRCMD_DEVICE_PCI);

	pci_write_config_dword(pdev, 0x5c, dev_priv->saveBSM);
	pci_write_config_dword(pdev, 0xFC, dev_priv->saveVBT);
	pci_write_config_dword(pdev, PSB_PCIx_MSI_ADDR_LOC, dev_priv->msi_addr);
	pci_write_config_dword(pdev, PSB_PCIx_MSI_DATA_LOC, dev_priv->msi_data);

	gbSuspended = false;

#ifdef CONFIG_MDFD_GL3
	/* Powerup GL3 - can be used by any GFX-sub-system. */
	ospm_power_island_up(OSPM_GL3_CACHE_ISLAND);
#endif

	return true;
}

/*
 * ospm_power_suspend
 *
 * Description: OSPM is telling our driver to suspend so save state
 * and power down all hardware.
 */
int ospm_power_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int ret = 0;
	int graphics_access_count;
	int videoenc_access_count;
	int videodec_access_count;
	int display_access_count;
	bool suspend_pci = true;

	mutex_lock(&g_ospm_mutex);

	if (gbSuspended)
		goto out;

	graphics_access_count = atomic_read(&g_graphics_access_count);
	videoenc_access_count = atomic_read(&g_videoenc_access_count);
	videodec_access_count = atomic_read(&g_videodec_access_count);
	display_access_count = atomic_read(&g_display_access_count);

	if (graphics_access_count || videoenc_access_count ||
		videodec_access_count || display_access_count) {
		ret = -EBUSY;

		printk(KERN_ALERT "%s: device busy: graphics %d videoenc %d videodec %d display %d\n",
			__func__, graphics_access_count, videoenc_access_count,
			videodec_access_count, display_access_count);
		goto out;
	}

	psb_irq_uninstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
	ospm_suspend_display(drm_dev);

	/* FIXME: video driver support for Linux Runtime PM */
	if (ospm_runtime_pm_msvdx_suspend(drm_dev))
		suspend_pci = false;

	if (ospm_runtime_pm_topaz_suspend(drm_dev))
		suspend_pci = false;

	if (suspend_pci)
		ospm_suspend_pci(pdev);
	/*
	 * REVISIT: else pci is not suspended but this happily returns success
	 * status?!
	 */

	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
out:
	mutex_unlock(&g_ospm_mutex);
	return ret;
}

/* The PMU/P-Unit driver has different island definitions */
static int to_pmu_islands(int reg_type, int islands)
{
	int ret = 0;

	if (reg_type == APM_REG_TYPE) {
		if (islands & OSPM_GRAPHICS_ISLAND)
			ret |= APM_GRAPHICS_ISLAND;
		if (islands & OSPM_VIDEO_DEC_ISLAND)
			ret |= APM_VIDEO_DEC_ISLAND;
		if (islands & OSPM_VIDEO_ENC_ISLAND)
			ret |= APM_VIDEO_ENC_ISLAND;
		if (islands & OSPM_GL3_CACHE_ISLAND)
			ret |= APM_GL3_CACHE_ISLAND;
	} else if (reg_type == OSPM_REG_TYPE) {
		if (islands & OSPM_DISPLAY_ISLAND)
			ret |= OSPM_DISPLAY_A_ISLAND | OSPM_DISPLAY_B_ISLAND |
				OSPM_DISPLAY_C_ISLAND | OSPM_MIPI_ISLAND;
	}

	return ret;
}

static int ospm_set_power_state(int state_type, int islands)
{
	int pmu_islands;

	pmu_islands = to_pmu_islands(APM_REG_TYPE, islands);
	if (pmu_islands)
		pmu_nc_set_power_state(pmu_islands, state_type, APM_REG_TYPE);

	pmu_islands = to_pmu_islands(OSPM_REG_TYPE, islands);
	if (pmu_islands)
		pmu_nc_set_power_state(pmu_islands, state_type, OSPM_REG_TYPE);

	return 0;
}

/*
 * ospm_power_island_up
 *
 * Description: Restore power to the specified island(s) (powergating)
 */
void ospm_power_island_up(int islands)
{
#ifndef CONFIG_MDFD_GL3
	islands &= ~OSPM_GL3_CACHE_ISLAND;
#endif

	if (islands & OSPM_GRAPHICS_ISLAND)
		pvrcmd_device_power(PVR_TRCMD_RESUME, PVR_TRCMD_DEVICE_SGX);

	if (islands & OSPM_DISPLAY_ISLAND)
		pvrcmd_device_power(PVR_TRCMD_RESUME, PVR_TRCMD_DEVICE_DISPC);

	ospm_set_power_state(OSPM_ISLAND_UP, islands);

	g_hw_power_status_mask |= islands;
}

/*
 * ospm_power_island_down
 *
 * Description: Cut power to the specified island(s) (powergating)
 */
void ospm_power_island_down(int islands)
{
	g_hw_power_status_mask &= ~islands;

	if (islands & OSPM_GRAPHICS_ISLAND)
		pvrcmd_device_power(PVR_TRCMD_SUSPEND, PVR_TRCMD_DEVICE_SGX);

	if (islands & OSPM_DISPLAY_ISLAND)
		pvrcmd_device_power(PVR_TRCMD_SUSPEND, PVR_TRCMD_DEVICE_DISPC);

	ospm_set_power_state(OSPM_ISLAND_DOWN, islands);
}

/*
 * ospm_power_resume
 */
int ospm_power_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	mutex_lock(&g_ospm_mutex);

#ifdef OSPM_GFX_DPK
	printk(KERN_ALERT "OSPM_GFX_DPK: ospm_power_resume \n");
#endif

	ospm_resume_pci(pdev);

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	ospm_resume_display(drm_dev);
	psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
	psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);

	mutex_unlock(&g_ospm_mutex);

	return 0;
}

/*
 * ospm_power_is_hw_on
 *
 * Description: do an instantaneous check for if the specified islands
 * are on.  Only use this in cases where you know the g_state_change_mutex
 * is already held such as in irq install/uninstall.  Otherwise, use
 * ospm_power_using_hw_begin().
 */
bool ospm_power_is_hw_on(int hw_islands)
{
	return ((g_hw_power_status_mask & hw_islands) == hw_islands) ? true : false;
}

bool ospm_power_using_hw_begin_atomic(int hw_island)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */

	/* fail if island is off, no can do in atomic context  */
	if (!(g_hw_power_status_mask & hw_island))
		return false;

	pm_runtime_get(&drm_dev->pdev->dev);

	switch (hw_island) {
	case OSPM_GRAPHICS_ISLAND:
		atomic_inc(&g_graphics_access_count);
		break;
	case OSPM_VIDEO_ENC_ISLAND:
		atomic_inc(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		atomic_inc(&g_videodec_access_count);
		break;
	case OSPM_DISPLAY_ISLAND:
		atomic_inc(&g_display_access_count);
		break;
	}

	return true;
}

/*
 * ospm_power_using_hw_begin
 *
 * Description: Notify PowerMgmt module that you will be accessing the
 * specified island's hw so don't power it off.  If force_on is true,
 * this will power on the specified island if it is off.
 * Otherwise, this will return false and the caller is expected to not
 * access the hw.
 *
 * NOTE *** If this is called from and interrupt handler or other atomic
 * context, then it will return false if we are in the middle of a
 * power state transition and the caller will be expected to handle that
 * even if force_on is set to true.
 */
bool ospm_power_using_hw_begin(int hw_island, bool force_on)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */
	bool ret = true;

	WARN(in_interrupt() || in_atomic(), "%s called in atomic context\n",
		__func__);

	/* no force, increase count if island on, otherwise fail */
	if (!force_on)
		return ospm_power_using_hw_begin_atomic(hw_island);

	/* note: the runtime pm resume callback takes g_ospm_mutex */
	pm_runtime_get_sync(&drm_dev->pdev->dev);

	mutex_lock(&g_ospm_mutex);

	/* our job here is done if island is already on */
	if (g_hw_power_status_mask & hw_island)
		goto increase_count;

	switch (hw_island) {
	case OSPM_DISPLAY_ISLAND:
		ospm_resume_display(drm_dev);
		psb_irq_preinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
		psb_irq_postinstall_islands(drm_dev, OSPM_DISPLAY_ISLAND);
		break;

	case OSPM_GRAPHICS_ISLAND:
		ospm_power_island_up(OSPM_GRAPHICS_ISLAND);
		psb_irq_preinstall_islands(drm_dev, OSPM_GRAPHICS_ISLAND);
		psb_irq_postinstall_islands(drm_dev, OSPM_GRAPHICS_ISLAND);
		break;

	case OSPM_VIDEO_DEC_ISLAND:
		WARN_ON(!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND));

		ospm_power_island_up(OSPM_VIDEO_DEC_ISLAND);
		ospm_runtime_pm_msvdx_resume(drm_dev);
		psb_irq_preinstall_islands(drm_dev, OSPM_VIDEO_DEC_ISLAND);
		psb_irq_postinstall_islands(drm_dev, OSPM_VIDEO_DEC_ISLAND);
		break;

	case OSPM_VIDEO_ENC_ISLAND:
		WARN_ON(!ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND));

		ospm_power_island_up(OSPM_VIDEO_ENC_ISLAND);
		ospm_runtime_pm_topaz_resume(drm_dev);
		psb_irq_preinstall_islands(drm_dev, OSPM_VIDEO_ENC_ISLAND);
		psb_irq_postinstall_islands(drm_dev, OSPM_VIDEO_ENC_ISLAND);
		break;

	default:
		BUG();
	}

increase_count:
	switch (hw_island) {
	case OSPM_GRAPHICS_ISLAND:
		atomic_inc(&g_graphics_access_count);
		break;
	case OSPM_VIDEO_ENC_ISLAND:
		atomic_inc(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		atomic_inc(&g_videodec_access_count);
		break;
	case OSPM_DISPLAY_ISLAND:
		atomic_inc(&g_display_access_count);
		break;
	}

	mutex_unlock(&g_ospm_mutex);

	return ret;
}


/*
 * ospm_power_using_hw_end
 *
 * Description: Notify PowerMgmt module that you are done accessing the
 * specified island's hw so feel free to power it off.  Note that this
 * function doesn't actually power off the islands.
 */
void ospm_power_using_hw_end(int hw_island)
{
	struct drm_device *drm_dev = gpDrmDevice; /* FIXME: Pass as parameter */

	switch (hw_island) {
	case OSPM_GRAPHICS_ISLAND:
		atomic_dec(&g_graphics_access_count);
		break;
	case OSPM_VIDEO_ENC_ISLAND:
		atomic_dec(&g_videoenc_access_count);
		break;
	case OSPM_VIDEO_DEC_ISLAND:
		atomic_dec(&g_videodec_access_count);
		break;
	case OSPM_DISPLAY_ISLAND:
		atomic_dec(&g_display_access_count);
		break;
	}

	pm_runtime_put(&drm_dev->pdev->dev);

	WARN_ON(atomic_read(&g_graphics_access_count) < 0);
	WARN_ON(atomic_read(&g_videoenc_access_count) < 0);
	WARN_ON(atomic_read(&g_videodec_access_count) < 0);
	WARN_ON(atomic_read(&g_display_access_count) < 0);
}

#ifdef CONFIG_SND_INTELMID_HDMI_AUDIO
static int psb_runtime_hdmi_audio_suspend(struct drm_device *drm_dev)
{
	struct drm_psb_private *dev_priv = drm_dev->dev_private;
	pm_event_t pm_event = {0};
	int r;

	if (!dev_priv->had_pvt_data)
		return 0;

	r = dev_priv->had_interface->suspend(dev_priv->had_pvt_data, pm_event);

	return r ? -EBUSY : 0;
}

static void psb_runtime_hdmi_audio_resume(struct drm_device *drm_dev)
{
	struct drm_psb_private *dev_priv = drm_dev->dev_private;

	if (dev_priv->had_pvt_data)
		dev_priv->had_interface->resume(dev_priv->had_pvt_data);
}
#else
static inline int psb_runtime_hdmi_audio_suspend(struct drm_device *drm_dev)
{
	return 0;
}
static inline void psb_runtime_hdmi_audio_resume(struct drm_device *drm_dev)
{
}
#endif

int psb_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = drm_dev->dev_private;

	if (dev_priv->dpi_panel_on || dev_priv->dpi_panel_on2 ||
	    atomic_read(&g_graphics_access_count) ||
	    atomic_read(&g_videoenc_access_count) ||
	    atomic_read(&g_videodec_access_count) ||
	    atomic_read(&g_display_access_count) ||
	    ospm_runtime_check_msvdx_hw_busy(drm_dev) == 1 ||
	    ospm_runtime_check_topaz_hw_busy(drm_dev) == 1 ||
	    dev_priv->hdmi_audio_busy) {

#ifdef OSPM_GFX_DPK
		dev_dbg(&drm_dev->pdev->dev,
			"%s: GFX: %d VEC: %d VED: %d DC: %d\n",
			__func__,
			atomic_read(&g_graphics_access_count),
			atomic_read(&g_videoenc_access_count),
			atomic_read(&g_videodec_access_count),
			atomic_read(&g_display_access_count));
#endif

		return -EBUSY;
	}

	return 0;
}

int psb_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	int r;

#ifdef OSPM_GFX_DPK
	dev_dbg(&drm_dev->pdev->dev, "%s\n", __func__);
#endif

	r = psb_runtime_idle(dev);
	if (r)
		return r;

	/* REVISIT: if ospm_power_suspend fails, do what with hdmi audio? */

	return ospm_power_suspend(dev);
}

int psb_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);

	ospm_power_resume(dev);
	psb_runtime_hdmi_audio_resume(drm_dev);

	return 0;
}

/*
 * ospm_power_init
 *
 * Description: Initialize this ospm power management module
 */
void ospm_power_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	gpDrmDevice = dev;

	dev_priv->apm_base = MDFLD_MSG_READ32(PSB_PUNIT_PORT, PSB_APMBA) &
		0xffff;

	mutex_init(&g_ospm_mutex);

	/* Specify the islands to keep powered up at boot */
	g_hw_power_status_mask = OSPM_ALL_ISLANDS;
#ifndef CONFIG_MDFD_GL3
	g_hw_power_status_mask &= ~OSPM_GL3_CACHE_ISLAND;
#endif

	/* Set power island states according to g_hw_power_status_mask */
	ospm_set_power_state(OSPM_ISLAND_UP, g_hw_power_status_mask);
	ospm_set_power_state(OSPM_ISLAND_DOWN,
			OSPM_ALL_ISLANDS ^ g_hw_power_status_mask);

	atomic_set(&g_display_access_count, 0);
	atomic_set(&g_graphics_access_count, 0);
	atomic_set(&g_videoenc_access_count, 0);
	atomic_set(&g_videodec_access_count, 0);

#ifdef CONFIG_EARLYSUSPEND
	dev_priv->early_suspend.suspend = gfx_early_suspend;
	dev_priv->early_suspend.resume = gfx_late_resume;
	dev_priv->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	register_early_suspend(&dev_priv->early_suspend);
#endif

	/* Runtime PM for PCI drivers. */
	pm_runtime_put_noidle(&dev->pdev->dev);
}

/*
 * ospm_power_uninit
 *
 * Description: Uninitialize this ospm power management module
 */
void ospm_power_uninit(struct drm_device *drm_dev)
{
	/* Runtime PM for PCI drivers. */
	pm_runtime_get_noresume(&drm_dev->pdev->dev);

	mutex_destroy(&g_ospm_mutex);
}

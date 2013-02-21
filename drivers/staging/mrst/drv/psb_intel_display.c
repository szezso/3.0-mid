/*
 * Copyright Â© 2006-2007 Intel Corporation
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
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 */

#include <linux/i2c.h>
#include <linux/pm_runtime.h>

#include <drm/drmP.h>
#include "psb_fb.h"
#include "psb_drv.h"
#include "psb_intel_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_display.h"
#include "psb_page_flip.h"
#include "psb_powermgmt.h"
#include "mdfld_output.h"

static int mdfld__intel_pipe_set_base(struct drm_crtc *crtc, int x, int y,
				struct drm_framebuffer *old_fb);
static int mdfld_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb);
static void mdfld_crtc_dpms(struct drm_crtc *crtc, int mode);

struct psb_intel_clock_t {
	/* given values */
	int n;
	int m1, m2;
	int p1, p2;
	/* derived values */
	int dot;
	int vco;
	int m;
	int p;
};

struct psb_intel_range_t {
	int min, max;
};

/**
 * Returns whether any output on the specified pipe is of the specified type
 */
bool psb_intel_pipe_has_type(struct drm_crtc *crtc, int type)
{
	struct drm_device *dev = crtc->dev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_connector *l_entry;

	list_for_each_entry(l_entry, &mode_config->connector_list, head) {
		if (l_entry->encoder && l_entry->encoder->crtc == crtc) {
			struct psb_intel_output *psb_intel_output =
			    to_psb_intel_output(l_entry);
			if (psb_intel_output->type == type)
				return true;
		}
	}
	return false;
}

void psb_intel_wait_for_vblank(struct drm_device *dev)
{
	/* Wait for 20ms, i.e. one cycle at 50hz. */
	mdelay(20);
}

static void psb_intel_crtc_prepare(struct drm_crtc *crtc)
{
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	crtc_funcs->dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void psb_intel_crtc_commit(struct drm_crtc *crtc)
{
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	crtc_funcs->dpms(crtc, DRM_MODE_DPMS_ON);
}

void psb_intel_encoder_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs =
	    encoder->helper_private;
	/* lvds has its own version of prepare see psb_intel_lvds_prepare */
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
}

void psb_intel_encoder_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs =
	    encoder->helper_private;
	/* lvds has its own version of commit see psb_intel_lvds_commit */
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
}

static bool psb_intel_crtc_mode_fixup(struct drm_crtc *crtc,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}


/**
 * Return the pipe currently connected to the panel fitter,
 * or -1 if the panel fitter is not present or not in use
 */
int psb_intel_panel_fitter_pipe(struct drm_device *dev)
{
	u32 pfit_control;

	/* i830 doesn't have a panel fitter */
	if (IS_I830(dev))
		return -1;

	pfit_control = REG_READ(PFIT_CONTROL);

	/* See if the panel fitter is in use */
	if ((pfit_control & PFIT_ENABLE) == 0)
		return -1;

	/* 965 can place panel fitter on either pipe */
	return (pfit_control >> 29) & 0x3;
}

/** Loads the palette/gamma unit for the CRTC with the prepared values */
void psb_intel_crtc_load_lut(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_psb_private *dev_priv =
				(struct drm_psb_private *)dev->dev_private;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	int palreg;
	int i;

	/* The clocks have to be on to load the palette. */
	if (!crtc->enabled)
		return;

	palreg = PSB_PALETTE(psb_intel_crtc->pipe);

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
		for (i = 0; i < 256; i++) {
			REG_WRITE(palreg + 4 * i,
				  ((psb_intel_crtc->lut_r[i] +
				  psb_intel_crtc->lut_adj[i]) << 16) |
				  ((psb_intel_crtc->lut_g[i] +
				  psb_intel_crtc->lut_adj[i]) << 8) |
				  (psb_intel_crtc->lut_b[i] +
				  psb_intel_crtc->lut_adj[i]));
		}
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	} else {
		for (i = 0; i < 256; i++) {
			dev_priv->pipe_regs[0].palette[i] =
				  ((psb_intel_crtc->lut_r[i] +
				  psb_intel_crtc->lut_adj[i]) << 16) |
				  ((psb_intel_crtc->lut_g[i] +
				  psb_intel_crtc->lut_adj[i]) << 8) |
				  (psb_intel_crtc->lut_b[i] +
				  psb_intel_crtc->lut_adj[i]);
		}

	}
}

/**
 * Save HW states of giving crtc
 */
static void psb_intel_crtc_save(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	/* struct drm_psb_private *dev_priv =
			(struct drm_psb_private *)dev->dev_private; */
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct psb_intel_crtc_state *crtc_state = psb_intel_crtc->crtc_state;
	int pipe = psb_intel_crtc->pipe;
	int pipeA = (psb_intel_crtc->pipe == 0);
	uint32_t paletteReg;
	int i;

	DRM_DEBUG("\n");

	if (!crtc_state) {
		DRM_DEBUG("No CRTC state found\n");
		return;
	}

	crtc_state->saveDSPCNTR = REG_READ(PSB_DSPCNTR(pipe));
	crtc_state->savePIPECONF = REG_READ(PSB_PIPECONF(pipe));
	crtc_state->savePIPESRC = REG_READ(PSB_PIPESRC(pipe));
	crtc_state->saveFP0 = REG_READ(pipeA ? FPA0 : FPB0);
	crtc_state->saveFP1 = REG_READ(pipeA ? FPA1 : FPB1);
	crtc_state->saveDPLL = REG_READ(pipeA ? DPLL_A : DPLL_B);
	crtc_state->saveHTOTAL = REG_READ(PSB_HTOTAL(pipe));
	crtc_state->saveHBLANK = REG_READ(PSB_HBLANK(pipe));
	crtc_state->saveHSYNC = REG_READ(PSB_HSYNC(pipe));
	crtc_state->saveVTOTAL = REG_READ(PSB_VTOTAL(pipe));
	crtc_state->saveVBLANK = REG_READ(PSB_VBLANK(pipe));
	crtc_state->saveVSYNC = REG_READ(PSB_VSYNC(pipe));
	crtc_state->saveDSPSTRIDE = REG_READ(PSB_DSPSTRIDE(pipe));

	/*NOTE: DSPSIZE DSPPOS only for psb*/
	crtc_state->saveDSPSIZE = REG_READ(PSB_DSPSIZE(pipe));
	crtc_state->saveDSPPOS = REG_READ(PSB_DSPPOS(pipe));

	crtc_state->saveDSPBASE = REG_READ(PSB_DSPBASE(pipe));

	DRM_DEBUG("(%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x)\n",
			crtc_state->saveDSPCNTR,
			crtc_state->savePIPECONF,
			crtc_state->savePIPESRC,
			crtc_state->saveFP0,
			crtc_state->saveFP1,
			crtc_state->saveDPLL,
			crtc_state->saveHTOTAL,
			crtc_state->saveHBLANK,
			crtc_state->saveHSYNC,
			crtc_state->saveVTOTAL,
			crtc_state->saveVBLANK,
			crtc_state->saveVSYNC,
			crtc_state->saveDSPSTRIDE,
			crtc_state->saveDSPSIZE,
			crtc_state->saveDSPPOS,
			crtc_state->saveDSPBASE
		);

	paletteReg = PSB_PALETTE(pipe);
	for (i = 0; i < 256; ++i)
		crtc_state->savePalette[i] = REG_READ(paletteReg + (i << 2));
}

/**
 * Restore HW states of giving crtc
 */
static void psb_intel_crtc_restore(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	/* struct drm_psb_private * dev_priv =
				(struct drm_psb_private *)dev->dev_private; */
	struct psb_intel_crtc *psb_intel_crtc =  to_psb_intel_crtc(crtc);
	struct psb_intel_crtc_state *crtc_state = psb_intel_crtc->crtc_state;
	/* struct drm_crtc_helper_funcs * crtc_funcs = crtc->helper_private; */
	int pipe = psb_intel_crtc->pipe;
	int pipeA = (psb_intel_crtc->pipe == 0);
	uint32_t paletteReg;
	int i;

	DRM_DEBUG("\n");

	if (!crtc_state) {
		DRM_DEBUG("No crtc state\n");
		return;
	}

	DRM_DEBUG(
		"current:(%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x)\n",
		REG_READ(PSB_DSPCNTR(pipe)),
		REG_READ(PSB_PIPECONF(pipe)),
		REG_READ(PSB_PIPESRC(pipe)),
		REG_READ(pipeA ? FPA0 : FPB0),
		REG_READ(pipeA ? FPA1 : FPB1),
		REG_READ(pipeA ? DPLL_A : DPLL_B),
		REG_READ(PSB_HTOTAL(pipe)),
		REG_READ(PSB_HBLANK(pipe)),
		REG_READ(PSB_HSYNC(pipe)),
		REG_READ(PSB_VTOTAL(pipe)),
		REG_READ(PSB_VBLANK(pipe)),
		REG_READ(PSB_VSYNC(pipe)),
		REG_READ(PSB_DSPSTRIDE(pipe)),
		REG_READ(PSB_DSPSIZE(pipe)),
		REG_READ(PSB_DSPPOS(pipe)),
		REG_READ(PSB_DSPBASE(pipe))
		);

	DRM_DEBUG(
		"saved: (%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x)\n",
		crtc_state->saveDSPCNTR,
		crtc_state->savePIPECONF,
		crtc_state->savePIPESRC,
		crtc_state->saveFP0,
		crtc_state->saveFP1,
		crtc_state->saveDPLL,
		crtc_state->saveHTOTAL,
		crtc_state->saveHBLANK,
		crtc_state->saveHSYNC,
		crtc_state->saveVTOTAL,
		crtc_state->saveVBLANK,
		crtc_state->saveVSYNC,
		crtc_state->saveDSPSTRIDE,
		crtc_state->saveDSPSIZE,
		crtc_state->saveDSPPOS,
		crtc_state->saveDSPBASE
		);

	if (crtc_state->saveDPLL & DPLL_VCO_ENABLE) {
		REG_WRITE(pipeA ? DPLL_A : DPLL_B,
			crtc_state->saveDPLL & ~DPLL_VCO_ENABLE);
		REG_READ(pipeA ? DPLL_A : DPLL_B);
		DRM_DEBUG("write dpll: %x\n",
				REG_READ(pipeA ? DPLL_A : DPLL_B));
		udelay(150);
	}

	REG_WRITE(pipeA ? FPA0 : FPB0, crtc_state->saveFP0);
	REG_READ(pipeA ? FPA0 : FPB0);

	REG_WRITE(pipeA ? FPA1 : FPB1, crtc_state->saveFP1);
	REG_READ(pipeA ? FPA1 : FPB1);

	REG_WRITE(pipeA ? DPLL_A : DPLL_B, crtc_state->saveDPLL);
	REG_READ(pipeA ? DPLL_A : DPLL_B);
	udelay(150);

	REG_WRITE(PSB_HTOTAL(pipe), crtc_state->saveHTOTAL);
	REG_WRITE(PSB_HBLANK(pipe), crtc_state->saveHBLANK);
	REG_WRITE(PSB_HSYNC(pipe), crtc_state->saveHSYNC);
	REG_WRITE(PSB_VTOTAL(pipe), crtc_state->saveVTOTAL);
	REG_WRITE(PSB_VBLANK(pipe), crtc_state->saveVBLANK);
	REG_WRITE(PSB_VSYNC(pipe), crtc_state->saveVSYNC);
	REG_WRITE(PSB_DSPSTRIDE(pipe), crtc_state->saveDSPSTRIDE);

	REG_WRITE(PSB_DSPSIZE(pipe), crtc_state->saveDSPSIZE);
	REG_WRITE(PSB_DSPPOS(pipe), crtc_state->saveDSPPOS);

	REG_WRITE(PSB_PIPESRC(pipe), crtc_state->savePIPESRC);
	REG_WRITE(PSB_DSPBASE(pipe), crtc_state->saveDSPBASE);
	REG_WRITE(PSB_PIPECONF(pipe), crtc_state->savePIPECONF);

	psb_intel_wait_for_vblank(dev);

	REG_WRITE(PSB_DSPCNTR(pipe), crtc_state->saveDSPCNTR);
	REG_WRITE(PSB_DSPBASE(pipe), crtc_state->saveDSPBASE);

	psb_intel_wait_for_vblank(dev);

	paletteReg = PSB_PALETTE(pipe);
	for (i = 0; i < 256; ++i)
		REG_WRITE(paletteReg + (i << 2), crtc_state->savePalette[i]);
}

/* FIXME: Start using the start and size parameters */
static void psb_intel_crtc_gamma_set(struct drm_crtc *crtc, u16 *red,
				u16 *green, u16 *blue, uint32_t start,
				uint32_t size)
{
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	int i;

	if (size != 256)
		return;

	for (i = 0; i < 256; i++) {
		psb_intel_crtc->lut_r[i] = red[i] >> 8;
		psb_intel_crtc->lut_g[i] = green[i] >> 8;
		psb_intel_crtc->lut_b[i] = blue[i] >> 8;
	}

	psb_intel_crtc_load_lut(crtc);
}

static void psb_intel_crtc_destroy(struct drm_crtc *crtc)
{
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);

	psb_page_flip_crtc_fini(psb_intel_crtc);
	kfree(psb_intel_crtc->crtc_state);
	drm_crtc_cleanup(crtc);
	kfree(psb_intel_crtc);
}

static const struct drm_crtc_helper_funcs mdfld_helper_funcs;
static const struct drm_crtc_funcs mdfld_intel_crtc_funcs;

/*
 * Set the default value of cursor control and base register
 * to zero. This is a workaround for h/w defect on oaktrail
 */
static void psb_intel_cursor_init(struct drm_device *dev, int pipe)
{
	uint32_t control;
	uint32_t base;

	switch (pipe) {
	case 0:
		control = CURACNTR;
		base = CURABASE;
		break;
	case 1:
		control = CURBCNTR;
		base = CURBBASE;
		break;
	case 2:
		control = CURCCNTR;
		base = CURCBASE;
		break;
	default:
		return;
	}

	REG_WRITE(control, 0);
	REG_WRITE(base, 0);
}

void psb_intel_crtc_init(struct drm_device *dev, int pipe,
		     struct psb_intel_mode_device *mode_dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_crtc *psb_intel_crtc;
	int i;
	uint16_t *r_base, *g_base, *b_base;

	PSB_DEBUG_ENTRY("\n");

	/* We allocate a extra array of drm_connector pointers
	 * for fbdev after the crtc */
	psb_intel_crtc =
	    kzalloc(sizeof(struct psb_intel_crtc) +
		    (INTELFB_CONN_LIMIT * sizeof(struct drm_connector *)),
		    GFP_KERNEL);
	if (psb_intel_crtc == NULL)
		return;

	psb_intel_crtc->crtc_state =
		kzalloc(sizeof(struct psb_intel_crtc_state), GFP_KERNEL);
	if (!psb_intel_crtc->crtc_state) {
		DRM_INFO("Crtc state error: No memory\n");
		kfree(psb_intel_crtc);
		return;
	}

	drm_crtc_init(dev, &psb_intel_crtc->base, &mdfld_intel_crtc_funcs);

	drm_mode_crtc_set_gamma_size(&psb_intel_crtc->base, 256);
	psb_intel_crtc->pipe = pipe;
	psb_intel_crtc->plane = pipe;

	r_base = psb_intel_crtc->base.gamma_store;
	g_base = r_base + 256;
	b_base = g_base + 256;
	for (i = 0; i < 256; i++) {
		psb_intel_crtc->lut_r[i] = i;
		psb_intel_crtc->lut_g[i] = i;
		psb_intel_crtc->lut_b[i] = i;
		r_base[i] = i << 8;
		g_base[i] = i << 8;
		b_base[i] = i << 8;

		psb_intel_crtc->lut_adj[i] = 0;
	}

	psb_intel_crtc->mode_dev = mode_dev;
	psb_intel_crtc->cursor_addr = 0;

	drm_crtc_helper_add(&psb_intel_crtc->base, &mdfld_helper_funcs);

	/* Setup the array of drm_connector pointer array */
	psb_intel_crtc->mode_set.crtc = &psb_intel_crtc->base;
	BUG_ON(pipe >= ARRAY_SIZE(dev_priv->plane_to_crtc_mapping) ||
	       dev_priv->plane_to_crtc_mapping[psb_intel_crtc->plane] != NULL);
	dev_priv->plane_to_crtc_mapping[psb_intel_crtc->plane] = &psb_intel_crtc->base;
	dev_priv->pipe_to_crtc_mapping[psb_intel_crtc->pipe] = &psb_intel_crtc->base;
	psb_intel_crtc->mode_set.connectors =
	    (struct drm_connector **) (psb_intel_crtc + 1);
	psb_intel_crtc->mode_set.num_connectors = 0;

	psb_intel_cursor_init(dev, pipe);

	psb_page_flip_crtc_init(psb_intel_crtc);

	init_waitqueue_head(&psb_intel_crtc->vbl_wait);
}

int psb_intel_get_pipe_from_crtc_id(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_psb_get_pipe_from_crtc_id_arg *pipe_from_crtc_id = data;
	struct drm_mode_object *drmmode_obj;
	struct psb_intel_crtc *crtc;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	drmmode_obj = drm_mode_object_find(dev, pipe_from_crtc_id->crtc_id,
			DRM_MODE_OBJECT_CRTC);

	if (!drmmode_obj) {
		DRM_ERROR("no such CRTC id\n");
		return -EINVAL;
	}

	crtc = to_psb_intel_crtc(obj_to_crtc(drmmode_obj));
	pipe_from_crtc_id->pipe = crtc->pipe;

	return 0;
}

int psb_intel_connector_clones(struct drm_device *dev, int type_mask)
{
	int index_mask = 0;
	struct drm_connector *connector;
	int entry = 0;

	list_for_each_entry(connector, &dev->mode_config.connector_list,
			    head) {
		struct psb_intel_output *psb_intel_output =
		    to_psb_intel_output(connector);
		if (type_mask & (1 << psb_intel_output->type))
			index_mask |= (1 << entry);
		entry++;
	}
	return index_mask;
}

/* current intel driver doesn't take advantage of encoders
   always give back the encoder for the connector
*/
struct drm_encoder *psb_intel_best_encoder(struct drm_connector *connector)
{
	struct psb_intel_output *psb_intel_output =
					to_psb_intel_output(connector);

	return &psb_intel_output->enc;
}

struct mrst_limit_t {
	struct psb_intel_range_t dot, m, p1;
};

struct mrst_clock_t {
	/* derived values */
	int dot;
	int m;
	int p1;
};

#define COUNT_MAX 0x10000000

static const struct drm_crtc_helper_funcs mdfld_helper_funcs = {
	.dpms = mdfld_crtc_dpms,
	.mode_fixup = psb_intel_crtc_mode_fixup,
	.mode_set = mdfld_crtc_mode_set,
	.mode_set_base = mdfld__intel_pipe_set_base,
	.prepare = psb_intel_crtc_prepare,
	.commit = psb_intel_crtc_commit,
};

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"

#ifdef CONFIG_MDFLD_DSI_DPU
#include "mdfld_dsi_dbi_dpu.h"
#endif

#include <linux/pm_runtime.h>

void mdfldWaitForPipeDisable(struct drm_device *dev, int pipe)
{
	int count, temp;
	u32 pipeconf_reg = PSB_PIPECONF(pipe);

	/* FIXME JLIU7_PO */
	psb_intel_wait_for_vblank(dev);
	return;

	/* Wait for for the pipe disable to take effect. */
	for (count = 0; count < COUNT_MAX; count++) {
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_PIPE_STATE) == 0)
			break;
	}

	PSB_DEBUG_ENTRY("cout = %d. \n", count);
}

void mdfldWaitForPipeEnable(struct drm_device *dev, int pipe)
{
	int count, temp;
	u32 pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	
	switch (pipe) {
	case 0:
		break;
	case 1:
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_B);
		break;
	case 2:
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	/* FIXME JLIU7_PO */
	psb_intel_wait_for_vblank(dev);
	return;

	/* Wait for for the pipe enable to take effect. */
	for (count = 0; count < COUNT_MAX; count++) {
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_PIPE_STATE) == 1)
			break;
	}

	PSB_DEBUG_ENTRY("cout = %d. \n", count);
}


static int mdfld_intel_crtc_cursor_set(struct drm_crtc *crtc,
				 struct drm_file *file_priv,
				 uint32_t handle,
				 uint32_t width, uint32_t height)
{
	struct drm_device *dev = crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct psb_intel_mode_device *mode_dev = psb_intel_crtc->mode_dev;
	int pipe = psb_intel_crtc->pipe;
	uint32_t control = CURACNTR;
	uint32_t base = CURABASE;
	uint32_t temp;
	size_t addr = 0;
	uint32_t page_offset;
	size_t size;
	void *bo;
	int ret;

	DRM_DEBUG("\n");

	switch (pipe) {
	case 0:
		break;
	case 1:
		control = CURBCNTR;
		base = CURBBASE;
		break;
	case 2:
		control = CURCCNTR;
		base = CURCBASE;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return -EINVAL;
	}
	
#if 1 /* FIXME_JLIU7 can't enalbe cursorB/C HW issue. need to remove after HW fix */
	if (pipe != 0)
		return 0;
#endif 
	/* if we want to turn of the cursor ignore width and height */
	if (!handle) {
		DRM_DEBUG("cursor off\n");
		/* turn off the cursor */
		temp = 0;
		temp |= CURSOR_MODE_DISABLE;

		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
			REG_WRITE(control, temp);
			REG_WRITE(base, 0);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}

		/* unpin the old bo */
		if (psb_intel_crtc->cursor_bo) {
			mode_dev->bo_unpin_for_scanout(dev,
						       psb_intel_crtc->
						       cursor_bo);
			mode_dev->bo_unref(dev, psb_intel_crtc->cursor_bo);
			psb_intel_crtc->cursor_bo = NULL;
		}
		return 0;
	}

	/* Currently we only support 64x64 cursors */
	if (width != 64 || height != 64) {
		DRM_ERROR("we currently only support 64x64 cursors\n");
		return -EINVAL;
	}

	bo = mode_dev->bo_from_handle(dev, file_priv, handle);
	if (!bo)
		return -ENOENT;

	ret = mode_dev->bo_pin_for_scanout(dev, bo);
	if (ret)
		goto unref_bo;
	size = mode_dev->bo_size(dev, bo);
	if (size < width * height * 4) {
		DRM_ERROR("buffer is to small\n");
		ret = -ENOMEM;
		goto unpin_bo;
	}

        /*insert this bo into gtt*/
//        DRM_INFO("%s: map meminfo for hw cursor. handle %x, pipe = %d \n", __FUNCTION__, handle, pipe);

	ret = psb_gtt_map_meminfo(dev, bo, &page_offset);
        if(ret) {
                DRM_ERROR("Can not map meminfo to GTT. handle 0x%x\n", handle);
		goto unpin_bo;
        }

	addr = page_offset << PAGE_SHIFT;

	psb_intel_crtc->cursor_addr = addr;

	temp = 0;
	/* set the pipe for the cursor */
	temp |= (pipe << 28);
	temp |= CURSOR_MODE_64_ARGB_AX | MCURSOR_GAMMA_ENABLE;

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
		REG_WRITE(control, temp);
		REG_WRITE(base, addr);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	/* unpin the old bo */
	if (psb_intel_crtc->cursor_bo && psb_intel_crtc->cursor_bo != bo) {
		mode_dev->bo_unpin_for_scanout(dev, psb_intel_crtc->cursor_bo);
		mode_dev->bo_unref(dev, psb_intel_crtc->cursor_bo);
		psb_intel_crtc->cursor_bo = bo;
	}

	return 0;

 unpin_bo:
	mode_dev->bo_unpin_for_scanout(dev, bo);
 unref_bo:
	mode_dev->bo_unref(dev, bo);
 out:
	return ret;
}

static int mdfld_intel_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
#ifndef CONFIG_MDFLD_DSI_DPU
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
#else
	struct psb_drm_dpu_rect rect;
#endif
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	int pipe = psb_intel_crtc->pipe;
	uint32_t pos = CURAPOS;
	uint32_t base = CURABASE;
	uint32_t temp = 0;
	uint32_t addr;

	switch (pipe) {
	case 0:
#ifndef CONFIG_MDFLD_DSI_DPU
		if (!(dev_priv->dsr_fb_update & MDFLD_DSR_CURSOR_0))
			mdfld_dsi_dbi_exit_dsr (dev, MDFLD_DSR_CURSOR_0);
#else /*CONFIG_MDFLD_DSI_DPU*/
		rect.x = x;
		rect.y = y;
		
		mdfld_dbi_dpu_report_damage(dev, MDFLD_CURSORA, &rect);
		mdfld_dpu_exit_dsr(dev);
#endif
		break;
	case 1:
		pos = CURBPOS;
		base = CURBBASE;
		break;
	case 2:
#ifndef CONFIG_MDFLD_DSI_DPU
		if (!(dev_priv->dsr_fb_update & MDFLD_DSR_CURSOR_2))
			mdfld_dsi_dbi_exit_dsr (dev, MDFLD_DSR_CURSOR_2);
#else /*CONFIG_MDFLD_DSI_DPU*/
		mdfld_dbi_dpu_report_damage(dev, MDFLD_CURSORC, &rect);
		mdfld_dpu_exit_dsr(dev);
#endif
		pos = CURCPOS;
		base = CURCBASE;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return -EINVAL;
	}
		
#if 1 /* FIXME_JLIU7 can't enalbe cursorB/C HW issue. need to remove after HW fix */
	if (pipe != 0)
		return 0;
#endif 
	if (x < 0) {
		temp |= (CURSOR_POS_SIGN << CURSOR_X_SHIFT);
		x = -x;
	}
	if (y < 0) {
		temp |= (CURSOR_POS_SIGN << CURSOR_Y_SHIFT);
		y = -y;
	}

	temp |= ((x & CURSOR_POS_MASK) << CURSOR_X_SHIFT);
	temp |= ((y & CURSOR_POS_MASK) << CURSOR_Y_SHIFT);

	addr = psb_intel_crtc->cursor_addr;

	if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
		REG_WRITE(pos, temp);
		REG_WRITE(base, addr);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	}

	return 0;
}

static const struct drm_crtc_funcs mdfld_intel_crtc_funcs = {
	.save = psb_intel_crtc_save,
	.restore = psb_intel_crtc_restore,
	.cursor_set = mdfld_intel_crtc_cursor_set,
	.cursor_move = mdfld_intel_crtc_cursor_move,
	.gamma_set = psb_intel_crtc_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = psb_intel_crtc_destroy,
	.page_flip = psb_intel_crtc_page_flip,
};

static struct drm_device globle_dev;

void mdfld__intel_plane_set_alpha(int enable)
{
	struct drm_device *dev = &globle_dev;
	int dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	u32 dspcntr;

	dspcntr = REG_READ(dspcntr_reg);

	if (enable) {
		dspcntr &= ~DISPPLANE_32BPP_NO_ALPHA;
		dspcntr |= DISPPLANE_32BPP;
	} else {
		dspcntr &= ~DISPPLANE_32BPP;
		dspcntr |= DISPPLANE_32BPP_NO_ALPHA;
	}

	REG_WRITE(dspcntr_reg, dspcntr);
}

static int check_fb(const struct drm_framebuffer *fb)
{
	if (!fb)
		return 0;

	switch (fb->bits_per_pixel) {
	case 8:
	case 16:
	case 24:
	case 32:
		return 0;
	default:
		DRM_ERROR("Unknown color depth\n");
		return -EINVAL;
	}
}

static int mdfld__intel_pipe_set_base(struct drm_crtc *crtc, int x, int y,
				struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	/* struct drm_i915_master_private *master_priv; */
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	struct psb_framebuffer *psbfb = to_psb_fb(crtc->fb);
	int pipe = psb_intel_crtc->pipe;
	unsigned long Start, Offset;
	int dsplinoff = PSB_DSPLINOFF(PSB_PIPE_A);
	int dspsurf = PSB_DSPSURF(PSB_PIPE_A);
	int dspstride = PSB_DSPSTRIDE(PSB_PIPE_A);
	int dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	u32 dspcntr;
	int ret;

	memcpy(&globle_dev, dev, sizeof(struct drm_device));

	PSB_DEBUG_ENTRY("pipe = 0x%x. \n", pipe);

	/* no fb bound */
	if (!crtc->fb) {
		PSB_DEBUG_ENTRY("No FB bound\n");
		return 0;
	}

	ret = check_fb(crtc->fb);
	if (ret)
		return ret;

	switch (pipe) {
	case 0:
		dsplinoff = PSB_DSPLINOFF(PSB_PIPE_A);
		break;
	case 1:
		dsplinoff = PSB_DSPLINOFF(PSB_PIPE_B);
		dspsurf = PSB_DSPSURF(PSB_PIPE_B);
		dspstride = PSB_DSPSTRIDE(PSB_PIPE_B);
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_B);
		break;
	case 2:
		dsplinoff = PSB_DSPLINOFF(PSB_PIPE_C);
		dspsurf = PSB_DSPSURF(PSB_PIPE_C);
		dspstride = PSB_DSPSTRIDE(PSB_PIPE_C);
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return -EINVAL;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
		return 0;

	Start = psbfb->offset;
	Offset = y * crtc->fb->pitches[0] + x * (crtc->fb->bits_per_pixel / 8);

	REG_WRITE(dspstride, crtc->fb->pitches[0]);
	dspcntr = REG_READ(dspcntr_reg);
	dspcntr &= ~DISPPLANE_PIXFORMAT_MASK;

	switch (crtc->fb->bits_per_pixel) {
	case 8:
		dspcntr |= DISPPLANE_8BPP;
		break;
	case 16:
		if (crtc->fb->depth == 15)
			dspcntr |= DISPPLANE_15_16BPP;
		else
			dspcntr |= DISPPLANE_16BPP;
		break;
	case 24:
	case 32:
		dspcntr |= DISPPLANE_32BPP_NO_ALPHA;
		break;
	}
	REG_WRITE(dspcntr_reg, dspcntr);

	PSB_DEBUG_ENTRY("Writing base %08lX %08lX %d %d\n", Start, Offset, x, y);

	REG_WRITE(dsplinoff, Offset);
	REG_READ(dsplinoff);
	REG_WRITE(dspsurf, Start);
	REG_READ(dspsurf);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return 0;
}

/**
 * Disable the pipe, plane and pll.
 *
 */
void mdfld_disable_crtc (struct drm_device *dev, int pipe)
{
	int dpll_reg = PSB_DSI_PLL_CTRL;
	int dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	int dspbase_reg = PSB_DSPBASE(PSB_PIPE_A);
	int pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	u32 temp;

	PSB_DEBUG_ENTRY("pipe = %d \n", pipe);


	switch (pipe) {
	case 0:
		break;
	case 1:
		dpll_reg = PSB_DPLL_CTRL;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_B);
		dspbase_reg = PSB_DSPSURF(PSB_PIPE_B);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_B);
		break;
	case 2:
		dpll_reg = PSB_DSI_PLL_CTRL;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		dspbase_reg = PSB_DSPBASE(PSB_PIPE_C);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	if (pipe != 1)
		mdfld_dsi_gen_fifo_ready(dev, MIPI_GEN_FIFO_STAT_REG(pipe), HS_CTRL_FIFO_EMPTY | HS_DATA_FIFO_EMPTY);

	/* Disable display plane */
	temp = REG_READ(dspcntr_reg);
	if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
		REG_WRITE(dspcntr_reg,
			  temp & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
		REG_READ(dspbase_reg);
	}

	/* FIXME_JLIU7 MDFLD_PO revisit */
	/* Wait for vblank for the disable to take effect */
// MDFLD_PO_JLIU7		psb_intel_wait_for_vblank(dev);

	/* Next, disable display pipes */
	temp = REG_READ(pipeconf_reg);
	if ((temp & PIPEACONF_ENABLE) != 0) {
		temp &= ~PIPEACONF_ENABLE;
		temp |= PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF;
		REG_WRITE(pipeconf_reg, temp);
		REG_READ(pipeconf_reg);

		/* Wait for for the pipe disable to take effect. */
		mdfldWaitForPipeDisable(dev, pipe);
	}

	temp = REG_READ(dpll_reg);
	if (temp & DPLL_VCO_ENABLE) {
		if (((pipe != 1) &&
		    !((REG_READ(PSB_PIPECONF(PSB_PIPE_A)) |
		       REG_READ(PSB_PIPECONF(PSB_PIPE_C))) & PIPEACONF_ENABLE))
				|| (pipe == 1)){
			temp &= ~(DPLL_VCO_ENABLE);
			REG_WRITE(dpll_reg, temp);
			REG_READ(dpll_reg);
			/* Wait for the clocks to turn off. */
			/* FIXME_MDFLD PO may need more delay */
			udelay(500);

			if (!(temp & MDFLD_PWR_GATE_EN)) {
				/* gating power of DPLL */
				REG_WRITE(dpll_reg, temp | MDFLD_PWR_GATE_EN);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(5000);
			}
		}
	}

}

void mdfld_pipe_disabled(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	int i;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);

		if (psb_intel_crtc->pipe == pipe) {
			drm_flip_helper_clear(&psb_intel_crtc->flip_helper);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(dev_priv->overlays); i++) {
		if (!dev_priv->overlays[i])
			continue;
		mdfld_overlay_pipe_disabled(dev_priv->overlays[i], pipe);
	}
}

/**
 * Sets the power management mode of the pipe and plane.
 *
 * This code should probably grow support for turning the cursor off and back
 * on appropriately at the same time as we're turning the pipe off/on.
 */
static void mdfld_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct drm_device *dev = crtc->dev;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	int pipe = psb_intel_crtc->pipe;
	int dpll_reg = PSB_DSI_PLL_CTRL;
	int dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	int dspbase_reg = PSB_DSPBASE(PSB_PIPE_A);
	int pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	u32 pipestat_reg = PSB_PIPESTAT(PSB_PIPE_A);
	u32 pipeconf = dev_priv->pipeconf;
	u32 dspcntr = dev_priv->dspcntr;
	u32 temp;
	bool enabled;
	int timeout = 0;

	PSB_DEBUG_ENTRY("mode = %d, pipe = %d \n", mode, pipe);

/* FIXME_JLIU7 MDFLD_PO replaced w/ the following function */
/* mdfld_dbi_dpms (struct drm_device *dev, int pipe, bool enabled) */

	switch (pipe) {
	case 0:
		break;
	case 1:
		dpll_reg = DPLL_B;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_B);
		dspbase_reg = PSB_DSPBASE(PSB_PIPE_B);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_B);
		pipeconf = dev_priv->pipeconf1;
		dspcntr = dev_priv->dspcntr1;
		dpll_reg = PSB_DPLL_CTRL;
		break;
	case 2:
		dpll_reg = PSB_DSI_PLL_CTRL;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		dspbase_reg = PSB_DSPBASE(PSB_PIPE_C);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		pipestat_reg = PSB_PIPESTAT(PSB_PIPE_C);
		pipeconf = dev_priv->pipeconf2;
		dspcntr = dev_priv->dspcntr2;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
		return;

	/* XXX: When our outputs are all unaware of DPMS modes other than off
	 * and on, we should map those modes to DRM_MODE_DPMS_OFF in the CRTC.
	 */
	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
		/* Enable the DPLL */
		temp = REG_READ(dpll_reg);

		if ((temp & DPLL_VCO_ENABLE) == 0) {
			/* When ungating power of DPLL, needs to wait 0.5us before enable the VCO */
			if (temp & MDFLD_PWR_GATE_EN) {
				temp &= ~MDFLD_PWR_GATE_EN;
				REG_WRITE(dpll_reg, temp);
				/* FIXME_MDFLD PO - change 500 to 1 after PO */
				udelay(500);
			}

			REG_WRITE(dpll_reg, temp);
			REG_READ(dpll_reg);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);
			
			REG_WRITE(dpll_reg, temp | DPLL_VCO_ENABLE);
			REG_READ(dpll_reg);

			/**
			 * wait for DSI PLL to lock
			 * NOTE: only need to poll status of pipe 0 and pipe 1,
			 * since both MIPI pipes share the same PLL.
			 */
			while ((pipe != 2) && (timeout < 20000) && !(REG_READ(pipeconf_reg) & PIPECONF_DSIPLL_LOCK)) {
				udelay(150);
				timeout ++;
			}
		}

		/* Enable the plane */
		temp = REG_READ(dspcntr_reg);
		if ((temp & DISPLAY_PLANE_ENABLE) == 0) {
			REG_WRITE(dspcntr_reg,
				temp | DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
		}

		/* Enable the pipe */
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_ENABLE) == 0) {
			REG_WRITE(pipeconf_reg, pipeconf);

			/* Wait for for the pipe enable to take effect. */
			mdfldWaitForPipeEnable(dev, pipe);
		}

		/*workaround for sighting 3741701 Random X blank display*/
		/*perform w/a in video mode only on pipe A or C*/
		if ((pipe == 0 || pipe == 2) &&
			(is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DPI)) {
			REG_WRITE(pipestat_reg, REG_READ(pipestat_reg));
			msleep(100);
			if(PIPE_VBLANK_STATUS & REG_READ(pipestat_reg)) {
				printk(KERN_ALERT "OK");
			} else {
				printk(KERN_ALERT "STUCK!!!!");
				/*shutdown controller*/
				temp = REG_READ(dspcntr_reg);
				REG_WRITE(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);
				REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
				/*mdfld_dsi_dpi_shut_down(dev, pipe);*/
				REG_WRITE(0xb048, 1);
				msleep(100);
				temp = REG_READ(pipeconf_reg);
				temp &= ~PIPEACONF_ENABLE;
				REG_WRITE(pipeconf_reg, temp);
				msleep(100); /*wait for pipe disable*/
			/*printk(KERN_ALERT "70008 is %x\n", REG_READ(0x70008));
			printk(KERN_ALERT "b074 is %x\n", REG_READ(0xb074));*/
				REG_WRITE(MIPI_DEVICE_READY_REG(pipe), 0);
				msleep(100);
			printk(KERN_ALERT "70008 is %x\n", REG_READ(0x70008));
			printk(KERN_ALERT "b074 is %x\n", REG_READ(0xb074));
				REG_WRITE(0xb004, REG_READ(0xb004));
				/* try to bring the controller back up again*/
				REG_WRITE(MIPI_DEVICE_READY_REG(pipe), 1);
				temp = REG_READ(dspcntr_reg);
				REG_WRITE(dspcntr_reg, temp | DISPLAY_PLANE_ENABLE);
				REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
				/*mdfld_dsi_dpi_turn_on(dev, pipe);*/
				REG_WRITE(0xb048, 2);
				msleep(100);
				temp = REG_READ(pipeconf_reg);
				temp |= PIPEACONF_ENABLE;
				REG_WRITE(pipeconf_reg, temp);
			}
		}

		psb_intel_crtc_load_lut(crtc);

		/* Give the overlay scaler a chance to enable
		   if it's on this pipe */
		/* psb_intel_crtc_dpms_video(crtc, true); TODO */

		break;
	case DRM_MODE_DPMS_OFF:
		/* Give the overlay scaler a chance to disable
		 * if it's on this pipe */
		/* psb_intel_crtc_dpms_video(crtc, FALSE); TODO */
		if (pipe != 1)
			mdfld_dsi_gen_fifo_ready(dev, MIPI_GEN_FIFO_STAT_REG(pipe), HS_CTRL_FIFO_EMPTY | HS_DATA_FIFO_EMPTY);

		/* Disable the VGA plane that we never use */
		REG_WRITE(VGACNTRL, VGA_DISP_DISABLE);

		/* Disable display plane */
		temp = REG_READ(dspcntr_reg);
		if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
			REG_WRITE(dspcntr_reg,
				  temp & ~DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
			REG_READ(dspbase_reg);
		}

		/* FIXME_JLIU7 MDFLD_PO revisit */
		/* Wait for vblank for the disable to take effect */
// MDFLD_PO_JLIU7		psb_intel_wait_for_vblank(dev);

		/* Next, disable display pipes */
		temp = REG_READ(pipeconf_reg);
		if ((temp & PIPEACONF_ENABLE) != 0) {
			temp &= ~PIPEACONF_ENABLE;
			temp |= PIPECONF_PLANE_OFF | PIPECONF_CURSOR_OFF;
			REG_WRITE(pipeconf_reg, temp);
//			REG_WRITE(pipeconf_reg, 0);
			REG_READ(pipeconf_reg);

			/* Wait for for the pipe disable to take effect. */
			mdfldWaitForPipeDisable(dev, pipe);
		}

		temp = REG_READ(dpll_reg);
		if (temp & DPLL_VCO_ENABLE) {
			if (((pipe != 1) &&
			     !((REG_READ(PSB_PIPECONF(PSB_PIPE_A)) |
				REG_READ(PSB_PIPECONF(PSB_PIPE_C))) &
						PIPEACONF_ENABLE)) ||
			    (pipe == 1)) {
				temp &= ~(DPLL_VCO_ENABLE);
				REG_WRITE(dpll_reg, temp);
				REG_READ(dpll_reg);
				/* Wait for the clocks to turn off. */
				/* FIXME_MDFLD PO may need more delay */
				udelay(500);
#if 0 /* MDFLD_PO_JLIU7 */	
		if (!(temp & MDFLD_PWR_GATE_EN)) {
			/* gating power of DPLL */
			REG_WRITE(dpll_reg, temp | MDFLD_PWR_GATE_EN);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(5000);
		}
#endif  /* MDFLD_PO_JLIU7 */	
			}
		}

		mdfld_pipe_disabled(dev, pipe);
		break;
	}

	enabled = crtc->enabled && mode != DRM_MODE_DPMS_OFF;

#if 0				/* JB: Add vblank support later */
	if (enabled)
		dev_priv->vblank_pipe |= (1 << pipe);
	else
		dev_priv->vblank_pipe &= ~(1 << pipe);
#endif

#if 0				/* JB: Add sarea support later */
	if (!dev->primary->master)
		return;

	master_priv = dev->primary->master->driver_priv;
	if (!master_priv->sarea_priv)
		return;

	switch (pipe) {
	case 0:
		master_priv->sarea_priv->planeA_w =
		    enabled ? crtc->mode.hdisplay : 0;
		master_priv->sarea_priv->planeA_h =
		    enabled ? crtc->mode.vdisplay : 0;
		break;
	case 1:
		master_priv->sarea_priv->planeB_w =
		    enabled ? crtc->mode.hdisplay : 0;
		master_priv->sarea_priv->planeB_h =
		    enabled ? crtc->mode.vdisplay : 0;
		break;
	default:
		DRM_ERROR("Can't update pipe %d in SAREA\n", pipe);
		break;
	}
#endif

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
}


#define MDFLD_LIMT_DPLL_19	    0
#define MDFLD_LIMT_DPLL_25	    1
#define MDFLD_LIMT_DPLL_83	    2
#define MDFLD_LIMT_DPLL_100	    3
#define MDFLD_LIMT_DSIPLL_19	    4
#define MDFLD_LIMT_DSIPLL_25	    5
#define MDFLD_LIMT_DSIPLL_83	    6
#define MDFLD_LIMT_DSIPLL_100	    7

#define MDFLD_DOT_MIN		  19750  /* FIXME_MDFLD JLIU7 need to find out  min & max for MDFLD */
#define MDFLD_DOT_MAX		  120000
#define MDFLD_DPLL_M_MIN_19	    113
#define MDFLD_DPLL_M_MAX_19	    155
#define MDFLD_DPLL_P1_MIN_19	    2
#define MDFLD_DPLL_P1_MAX_19	    10
#define MDFLD_DPLL_M_MIN_25	    101
#define MDFLD_DPLL_M_MAX_25	    130
#define MDFLD_DPLL_P1_MIN_25	    2
#define MDFLD_DPLL_P1_MAX_25	    10
#define MDFLD_DPLL_M_MIN_83	    64
#define MDFLD_DPLL_M_MAX_83	    64
#define MDFLD_DPLL_P1_MIN_83	    2
#define MDFLD_DPLL_P1_MAX_83	    2
#define MDFLD_DPLL_M_MIN_100	    64
#define MDFLD_DPLL_M_MAX_100	    64
#define MDFLD_DPLL_P1_MIN_100	    2
#define MDFLD_DPLL_P1_MAX_100	    2
#define MDFLD_DSIPLL_M_MIN_19	    131
#define MDFLD_DSIPLL_M_MAX_19	    175
#define MDFLD_DSIPLL_P1_MIN_19	    3
#define MDFLD_DSIPLL_P1_MAX_19	    8
#define MDFLD_DSIPLL_M_MIN_25	    97
#define MDFLD_DSIPLL_M_MAX_25	    140
#define MDFLD_DSIPLL_P1_MIN_25	    3
#define MDFLD_DSIPLL_P1_MAX_25	    9
#define MDFLD_DSIPLL_M_MIN_83	    33
#define MDFLD_DSIPLL_M_MAX_83	    92
#define MDFLD_DSIPLL_P1_MIN_83	    2
#define MDFLD_DSIPLL_P1_MAX_83	    3
#define MDFLD_DSIPLL_M_MIN_100	    97
#define MDFLD_DSIPLL_M_MAX_100	    140
#define MDFLD_DSIPLL_P1_MIN_100	    3
#define MDFLD_DSIPLL_P1_MAX_100	    9

static const struct mrst_limit_t mdfld_limits[] = {
	{			/* MDFLD_LIMT_DPLL_19 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DPLL_M_MIN_19, .max = MDFLD_DPLL_M_MAX_19},
	 .p1 = {.min = MDFLD_DPLL_P1_MIN_19, .max = MDFLD_DPLL_P1_MAX_19},
	 },
	{			/* MDFLD_LIMT_DPLL_25 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DPLL_M_MIN_25, .max = MDFLD_DPLL_M_MAX_25},
	 .p1 = {.min = MDFLD_DPLL_P1_MIN_25, .max = MDFLD_DPLL_P1_MAX_25},
	 },
	{			/* MDFLD_LIMT_DPLL_83 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DPLL_M_MIN_83, .max = MDFLD_DPLL_M_MAX_83},
	 .p1 = {.min = MDFLD_DPLL_P1_MIN_83, .max = MDFLD_DPLL_P1_MAX_83},
	 },
	{			/* MDFLD_LIMT_DPLL_100 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DPLL_M_MIN_100, .max = MDFLD_DPLL_M_MAX_100},
	 .p1 = {.min = MDFLD_DPLL_P1_MIN_100, .max = MDFLD_DPLL_P1_MAX_100},
	 },
	{			/* MDFLD_LIMT_DSIPLL_19 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DSIPLL_M_MIN_19, .max = MDFLD_DSIPLL_M_MAX_19},
	 .p1 = {.min = MDFLD_DSIPLL_P1_MIN_19, .max = MDFLD_DSIPLL_P1_MAX_19},
	 },
	{			/* MDFLD_LIMT_DSIPLL_25 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DSIPLL_M_MIN_25, .max = MDFLD_DSIPLL_M_MAX_25},
	 .p1 = {.min = MDFLD_DSIPLL_P1_MIN_25, .max = MDFLD_DSIPLL_P1_MAX_25},
	 },
	{			/* MDFLD_LIMT_DSIPLL_83 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DSIPLL_M_MIN_83, .max = MDFLD_DSIPLL_M_MAX_83},
	 .p1 = {.min = MDFLD_DSIPLL_P1_MIN_83, .max = MDFLD_DSIPLL_P1_MAX_83},
	 },
	{			/* MDFLD_LIMT_DSIPLL_100 */
	 .dot = {.min = MDFLD_DOT_MIN, .max = MDFLD_DOT_MAX},
	 .m = {.min = MDFLD_DSIPLL_M_MIN_100, .max = MDFLD_DSIPLL_M_MAX_100},
	 .p1 = {.min = MDFLD_DSIPLL_P1_MIN_100, .max = MDFLD_DSIPLL_P1_MAX_100},
	 },
};

#define MDFLD_M_MIN	    21
#define MDFLD_M_MAX	    180
static const u32 mdfld_m_converts[] = {
/* M configuration table from 9-bit LFSR table */
	224, 368, 440, 220, 366, 439, 219, 365, 182, 347, /* 21 - 30 */
	173, 342, 171, 85, 298, 149, 74, 37, 18, 265,   /* 31 - 40 */
	388, 194, 353, 432, 216, 108, 310, 155, 333, 166, /* 41 - 50 */
	83, 41, 276, 138, 325, 162, 337, 168, 340, 170, /* 51 - 60 */
	341, 426, 469, 234, 373, 442, 221, 110, 311, 411, /* 61 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213, /* 71 - 80 */
	106, 53, 282, 397, 354, 227, 113, 56, 284, 142, /* 81 - 90 */
	71, 35, 273, 136, 324, 418, 465, 488, 500, 506, /* 91 - 100 */
	253, 126, 63, 287, 399, 455, 483, 241, 376, 444, /* 101 - 110 */
	478, 495, 503, 251, 381, 446, 479, 239, 375, 443, /* 111 - 120 */
	477, 238, 119, 315, 157, 78, 295, 147, 329, 420, /* 121 - 130 */
	210, 105, 308, 154, 77, 38, 275, 137, 68, 290, /* 131 - 140 */
	145, 328, 164, 82, 297, 404, 458, 485, 498, 249, /* 141 - 150 */
	380, 190, 351, 431, 471, 235, 117, 314, 413, 206, /* 151 - 160 */
	103, 51, 25, 12, 262, 387, 193, 96, 48, 280, /* 161 - 170 */
	396, 198, 99, 305, 152, 76, 294, 403, 457, 228, /* 171 - 180 */
};

static const struct mrst_limit_t *mdfld_limit(struct drm_crtc *crtc)
{
	const struct mrst_limit_t *limit = NULL;
	struct drm_device *dev = crtc->dev;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	if (psb_intel_pipe_has_type(crtc, INTEL_OUTPUT_MIPI)
	    || psb_intel_pipe_has_type(crtc, INTEL_OUTPUT_MIPI2)) {
		if ((dev_priv->ksel == KSEL_CRYSTAL_19) || (dev_priv->ksel == KSEL_BYPASS_19))
			limit = &mdfld_limits[MDFLD_LIMT_DSIPLL_19];
		else if (dev_priv->ksel == KSEL_BYPASS_25) 
			limit = &mdfld_limits[MDFLD_LIMT_DSIPLL_25];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) && (dev_priv->core_freq == 166))
			limit = &mdfld_limits[MDFLD_LIMT_DSIPLL_83];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
			 (dev_priv->core_freq == 100 || dev_priv->core_freq == 200))
			limit = &mdfld_limits[MDFLD_LIMT_DSIPLL_100];
	} else if (psb_intel_pipe_has_type(crtc, INTEL_OUTPUT_HDMI)) {
		if ((dev_priv->ksel == KSEL_CRYSTAL_19) || (dev_priv->ksel == KSEL_BYPASS_19))
			limit = &mdfld_limits[MDFLD_LIMT_DPLL_19];
		else if (dev_priv->ksel == KSEL_BYPASS_25) 
			limit = &mdfld_limits[MDFLD_LIMT_DPLL_25];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) && (dev_priv->core_freq == 166))
			limit = &mdfld_limits[MDFLD_LIMT_DPLL_83];
		else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
			 (dev_priv->core_freq == 100 || dev_priv->core_freq == 200))
			limit = &mdfld_limits[MDFLD_LIMT_DPLL_100];
	} else {
		limit = NULL;
		PSB_DEBUG_ENTRY("mdfld_limit Wrong display type. \n");
	}

	return limit;
}

/** Derive the pixel clock for the given refclk and divisors for 8xx chips. */
static void mdfld_clock(int refclk, struct mrst_clock_t *clock)
{
	clock->dot = (refclk * clock->m) / clock->p1;
}

/**
 * Returns a set of divisors for the desired target clock with the given refclk,
 * or FALSE.  Divisor values are the actual divisors for
 */
static bool
mdfldFindBestPLL(struct drm_crtc *crtc, int target, int refclk,
		struct mrst_clock_t *best_clock)
{
	struct mrst_clock_t clock;
	const struct mrst_limit_t *limit = mdfld_limit(crtc);
	int err = target;

	memset(best_clock, 0, sizeof(*best_clock));

	PSB_DEBUG_ENTRY("mdfldFindBestPLL target = %d,"
			 "m_min = %d, m_max = %d, p_min = %d, p_max = %d. \n", target, limit->m.min, limit->m.max, limit->p1.min, limit->p1.max);

	for (clock.m = limit->m.min; clock.m <= limit->m.max; clock.m++) {
		for (clock.p1 = limit->p1.min; clock.p1 <= limit->p1.max;
		     clock.p1++) {
			int this_err;

			mdfld_clock(refclk, &clock);

			this_err = abs(clock.dot - target);
			if (this_err < err) {
				*best_clock = clock;
				err = this_err;
			}
		}
	}
	PSB_DEBUG_ENTRY("mdfldFindBestPLL target = %d,"
			 "m = %d, p = %d. \n", target, best_clock->m, best_clock->p1);
	PSB_DEBUG_ENTRY("mdfldFindBestPLL err = %d.\n", err);
	
	return err != target;
}

static int mdfld_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct psb_intel_crtc *psb_intel_crtc = to_psb_intel_crtc(crtc);
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	int pipe = psb_intel_crtc->pipe;
	int fp_reg = PSB_DSI_PLL_DIV_M1;
	int dpll_reg = PSB_DSI_PLL_CTRL;
	int dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_A);
	int pipeconf_reg = PSB_PIPECONF(PSB_PIPE_A);
	int htot_reg = PSB_HTOTAL(PSB_PIPE_A);
	int hblank_reg = PSB_HBLANK(PSB_PIPE_A);
	int hsync_reg = PSB_HSYNC(PSB_PIPE_A);
	int vtot_reg = PSB_VTOTAL(PSB_PIPE_A);
	int vblank_reg = PSB_VBLANK(PSB_PIPE_A);
	int vsync_reg = PSB_VSYNC(PSB_PIPE_A);
	int dspsize_reg = PSB_DSPSIZE(PSB_PIPE_A);
	int dsppos_reg = PSB_DSPPOS(PSB_PIPE_A);
	int pipesrc_reg = PSB_PIPESRC(PSB_PIPE_A);
	u32 *pipeconf = &dev_priv->pipeconf;
	u32 *dspcntr = &dev_priv->dspcntr;
	int refclk = 0;
	int clk_n = 0, clk_p2 = 0, clk_byte = 1, clk = 0, m_conv = 0, clk_tmp = 0;
	struct mrst_clock_t clock;
	bool ok;
	u32 dpll = 0, fp = 0;
	bool is_crt = false, is_lvds = false, is_tv = false;
	bool is_mipi = false, is_mipi2 = false, is_hdmi = false;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct psb_intel_output *psb_intel_output = NULL;
	uint64_t scalingType = DRM_MODE_SCALE_FULLSCREEN;
	struct drm_encoder *encoder;
	struct drm_connector * connector;
	int timeout = 0;
	int ret;

	PSB_DEBUG_ENTRY("pipe = 0x%x\n", pipe);

	ret = check_fb(crtc->fb);
	if (ret)
		return ret;

	if (pipe == 1) {
		if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
			return 0;
		android_hdmi_crtc_mode_set(crtc, mode, adjusted_mode,
			x, y, old_fb);
		goto mrst_crtc_mode_set_exit;
	}

	switch (pipe) {
	case 0:
		break;
	case 1:
		fp_reg = FPB0;
		dpll_reg = DPLL_B;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_B);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_B);
		htot_reg = PSB_HTOTAL(PSB_PIPE_B);
		hblank_reg = PSB_HBLANK(PSB_PIPE_B);
		hsync_reg = PSB_HSYNC(PSB_PIPE_B);
		vtot_reg = PSB_VTOTAL(PSB_PIPE_B);
		vblank_reg = PSB_VBLANK(PSB_PIPE_B);
		vsync_reg = PSB_VSYNC(PSB_PIPE_B);
		dspsize_reg = PSB_DSPSIZE(PSB_PIPE_B);
		dsppos_reg = PSB_DSPPOS(PSB_PIPE_B);
		pipesrc_reg = PSB_PIPESRC(PSB_PIPE_B);
		pipeconf = &dev_priv->pipeconf1;
		dspcntr = &dev_priv->dspcntr1;
		fp_reg = PSB_DPLL_DIV0;
		dpll_reg = PSB_DPLL_CTRL;
		break;
	case 2:
		dpll_reg = PSB_DSI_PLL_CTRL;
		dspcntr_reg = PSB_DSPCNTR(PSB_PIPE_C);
		pipeconf_reg = PSB_PIPECONF(PSB_PIPE_C);
		htot_reg = PSB_HTOTAL(PSB_PIPE_C);
		hblank_reg = PSB_HBLANK(PSB_PIPE_C);
		hsync_reg = PSB_HSYNC(PSB_PIPE_C);
		vtot_reg = PSB_VTOTAL(PSB_PIPE_C);
		vblank_reg = PSB_VBLANK(PSB_PIPE_C);
		vsync_reg = PSB_VSYNC(PSB_PIPE_C);
		dspsize_reg = PSB_DSPSIZE(PSB_PIPE_C);
		dsppos_reg = PSB_DSPPOS(PSB_PIPE_C);
		pipesrc_reg = PSB_PIPESRC(PSB_PIPE_C);
		pipeconf = &dev_priv->pipeconf2;
		dspcntr = &dev_priv->dspcntr2;
		break;
	default:
		DRM_ERROR("Illegal Pipe Number. \n");
		return 0;
	}

	PSB_DEBUG_ENTRY("adjusted_hdisplay = %d\n",
		 adjusted_mode->hdisplay);
	PSB_DEBUG_ENTRY("adjusted_vdisplay = %d\n",
		 adjusted_mode->vdisplay);
	PSB_DEBUG_ENTRY("adjusted_hsync_start = %d\n",
		 adjusted_mode->hsync_start);
	PSB_DEBUG_ENTRY("adjusted_hsync_end = %d\n",
		 adjusted_mode->hsync_end);
	PSB_DEBUG_ENTRY("adjusted_htotal = %d\n",
		 adjusted_mode->htotal);
	PSB_DEBUG_ENTRY("adjusted_vsync_start = %d\n",
		 adjusted_mode->vsync_start);
	PSB_DEBUG_ENTRY("adjusted_vsync_end = %d\n",
		 adjusted_mode->vsync_end);
	PSB_DEBUG_ENTRY("adjusted_vtotal = %d\n",
		 adjusted_mode->vtotal);
	PSB_DEBUG_ENTRY("adjusted_clock = %d\n",
		 adjusted_mode->clock);
	PSB_DEBUG_ENTRY("hdisplay = %d\n",
		 mode->hdisplay);
	PSB_DEBUG_ENTRY("vdisplay = %d\n",
		 mode->vdisplay);

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
		return 0;

	memcpy(&psb_intel_crtc->saved_mode, mode, sizeof(struct drm_display_mode));
	memcpy(&psb_intel_crtc->saved_adjusted_mode, adjusted_mode, sizeof(struct drm_display_mode));

	list_for_each_entry(connector, &mode_config->connector_list, head) {
		if(!connector)
			continue;
			
		encoder = connector->encoder;
		
		if(!encoder)
			continue;

		if (encoder->crtc != crtc)
			continue;

		psb_intel_output = to_psb_intel_output(connector);
		
		PSB_DEBUG_ENTRY("output->type = 0x%x \n", psb_intel_output->type);

		switch (psb_intel_output->type) {
		case INTEL_OUTPUT_LVDS:
			is_lvds = true;
			break;
		case INTEL_OUTPUT_TVOUT:
			is_tv = true;
			break;
		case INTEL_OUTPUT_ANALOG:
			is_crt = true;
			break;
		case INTEL_OUTPUT_MIPI:
			is_mipi = true;
			break;
		case INTEL_OUTPUT_MIPI2:
			is_mipi2 = true;
			break;
		case INTEL_OUTPUT_HDMI:
			is_hdmi = true;
			break;
		}
	}

	/* Disable the VGA plane that we never use */
	REG_WRITE(VGACNTRL, VGA_DISP_DISABLE);

	/* Disable the panel fitter if it was on our pipe */
	if (psb_intel_panel_fitter_pipe(dev) == pipe)
		REG_WRITE(PFIT_CONTROL, 0);

	/* pipesrc and dspsize control the size that is scaled from,
	 * which should always be the user's requested size.
	 */
	if (pipe == 1) {
		/* FIXME: To make HDMI display with 864x480 (TPO), 480x864 (PYR) or 480x854 (TMD), set the sprite 
		 * width/height and souce image size registers with the adjusted mode for pipe B. */

		/* The defined sprite rectangle must always be completely contained within the displayable 
		 * area of the screen image (frame buffer). */
		REG_WRITE(dspsize_reg, ((MIN(mode->crtc_vdisplay, adjusted_mode->crtc_vdisplay) - 1) << 16) 
				| (MIN(mode->crtc_hdisplay, adjusted_mode->crtc_hdisplay) - 1));
		/* Set the CRTC with encoder mode. */
		REG_WRITE(pipesrc_reg, ((mode->crtc_hdisplay - 1) << 16)
				 | (mode->crtc_vdisplay - 1));
	} else {
		REG_WRITE(dspsize_reg, ((mode->crtc_vdisplay - 1) << 16) | (mode->crtc_hdisplay - 1));
		REG_WRITE(pipesrc_reg, ((mode->crtc_hdisplay - 1) << 16) | (mode->crtc_vdisplay - 1));
	}

	REG_WRITE(dsppos_reg, 0);

	if (psb_intel_output)
		drm_connector_property_get_value(&psb_intel_output->base,
			dev->mode_config.scaling_mode_property, &scalingType);

	if (scalingType == DRM_MODE_SCALE_NO_SCALE) {
		/*Moorestown doesn't have register support for centering so we need to
		  mess with the h/vblank and h/vsync start and ends to get centering*/
		int offsetX = 0, offsetY = 0;

		offsetX = (adjusted_mode->crtc_hdisplay - mode->crtc_hdisplay) / 2;
		offsetY = (adjusted_mode->crtc_vdisplay - mode->crtc_vdisplay) / 2;

		REG_WRITE(htot_reg, (mode->crtc_hdisplay - 1) |
			((adjusted_mode->crtc_htotal - 1) << 16));
		REG_WRITE(vtot_reg, (mode->crtc_vdisplay - 1) |
			((adjusted_mode->crtc_vtotal - 1) << 16));
		REG_WRITE(hblank_reg, (adjusted_mode->crtc_hblank_start - offsetX - 1) |
			((adjusted_mode->crtc_hblank_end - offsetX - 1) << 16));
		REG_WRITE(hsync_reg, (adjusted_mode->crtc_hsync_start - offsetX - 1) |
			((adjusted_mode->crtc_hsync_end - offsetX - 1) << 16));
		REG_WRITE(vblank_reg, (adjusted_mode->crtc_vblank_start - offsetY - 1) |
			((adjusted_mode->crtc_vblank_end - offsetY - 1) << 16));
		REG_WRITE(vsync_reg, (adjusted_mode->crtc_vsync_start - offsetY - 1) |
			((adjusted_mode->crtc_vsync_end - offsetY - 1) << 16));
	} else {
		REG_WRITE(htot_reg, (adjusted_mode->crtc_hdisplay - 1) |
			((adjusted_mode->crtc_htotal - 1) << 16));
		REG_WRITE(vtot_reg, (adjusted_mode->crtc_vdisplay - 1) |
			((adjusted_mode->crtc_vtotal - 1) << 16));
		REG_WRITE(hblank_reg, (adjusted_mode->crtc_hblank_start - 1) |
			((adjusted_mode->crtc_hblank_end - 1) << 16));
		REG_WRITE(hsync_reg, (adjusted_mode->crtc_hsync_start - 1) |
			((adjusted_mode->crtc_hsync_end - 1) << 16));
		REG_WRITE(vblank_reg, (adjusted_mode->crtc_vblank_start - 1) |
			((adjusted_mode->crtc_vblank_end - 1) << 16));
		REG_WRITE(vsync_reg, (adjusted_mode->crtc_vsync_start - 1) |
			((adjusted_mode->crtc_vsync_end - 1) << 16));
	}

	/* Flush the plane changes */
	{
		struct drm_crtc_helper_funcs *crtc_funcs =
		    crtc->helper_private;
		crtc_funcs->mode_set_base(crtc, x, y, old_fb);
	}

	/* setup pipeconf */
	*pipeconf = PIPEACONF_ENABLE; /* FIXME_JLIU7 REG_READ(pipeconf_reg); */

	/* Set up the display plane register */
 	*dspcntr = REG_READ(dspcntr_reg);
	*dspcntr |= pipe << DISPPLANE_SEL_PIPE_POS;
	*dspcntr |= DISPLAY_PLANE_ENABLE;
/* MDFLD_PO_JLIU7	dspcntr |= DISPPLANE_BOTTOM; */
/* MDFLD_PO_JLIU7	dspcntr |= DISPPLANE_GAMMA_ENABLE; */

	if (is_mipi2)
	{
		goto mrst_crtc_mode_set_exit;
	}
/* FIXME JLIU7 Add MDFLD HDMI supports */
/* FIXME_MDFLD JLIU7 DSIPLL clock *= 8? */
/* FIXME_MDFLD JLIU7 need to revist for dual MIPI supports */
	clk = adjusted_mode->clock;

	if (is_hdmi) {
		if ((dev_priv->ksel == KSEL_CRYSTAL_19) || (dev_priv->ksel == KSEL_BYPASS_19))
		{
			refclk = 19200;

			if (is_mipi || is_mipi2)
			{
				clk_n = 1, clk_p2 = 8;
			} else if (is_hdmi) {
				clk_n = 1, clk_p2 = 10;
			}
		} else if (dev_priv->ksel == KSEL_BYPASS_25) { 
			refclk = 25000;

			if (is_mipi || is_mipi2)
			{
				clk_n = 1, clk_p2 = 8;
			} else if (is_hdmi) {
				clk_n = 1, clk_p2 = 10;
			}
		} else if ((dev_priv->ksel == KSEL_BYPASS_83_100) && (dev_priv->core_freq == 166)) {
			refclk = 83000;

			if (is_mipi || is_mipi2)
			{
				clk_n = 4, clk_p2 = 8;
			} else if (is_hdmi) {
				clk_n = 4, clk_p2 = 10;
			}
		} else if ((dev_priv->ksel == KSEL_BYPASS_83_100) &&
			   (dev_priv->core_freq == 100 || dev_priv->core_freq == 200)) {
			refclk = 100000;
			if (is_mipi || is_mipi2)
			{
				clk_n = 4, clk_p2 = 8;
			} else if (is_hdmi) {
				clk_n = 4, clk_p2 = 10;
			}
		}

		if (is_mipi)
			clk_byte = dev_priv->bpp / 8;
		else if (is_mipi2)
			clk_byte = dev_priv->bpp2 / 8;
	
		clk_tmp = clk * clk_n * clk_p2 * clk_byte;

		PSB_DEBUG_ENTRY("clk = %d, clk_n = %d, clk_p2 = %d. \n", clk, clk_n, clk_p2);
		PSB_DEBUG_ENTRY("adjusted_mode->clock = %d, clk_tmp = %d. \n", adjusted_mode->clock, clk_tmp);

		ok = mdfldFindBestPLL(crtc, clk_tmp, refclk, &clock);
		dev_priv->tmds_clock_khz = clock.dot / (clk_n * clk_p2 * clk_byte);

		if (!ok) {
#if 0				/* FIXME JLIU7 */
			DRM_ERROR("Couldn't find PLL settings for mode!\n");
			return;
#endif				/* FIXME JLIU7 */
			DRM_ERROR
			    ("mdfldFindBestPLL fail in mdfld_crtc_mode_set. \n");
		} else {
			m_conv = mdfld_m_converts[(clock.m - MDFLD_M_MIN)];

			PSB_DEBUG_ENTRY("dot clock = %d,"
				 "m = %d, p1 = %d, m_conv = %d. \n", clock.dot, clock.m,
				 clock.p1, m_conv);
		}

		dpll = REG_READ(dpll_reg);

		if (dpll & DPLL_VCO_ENABLE) {
			dpll &= ~DPLL_VCO_ENABLE;
			REG_WRITE(dpll_reg, dpll);
			REG_READ(dpll_reg);

			/* FIXME jliu7 check the DPLL lock bit PIPEACONF[29] */
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);

			/* reset M1, N1 & P1 */
			REG_WRITE(fp_reg, 0);
			dpll &= ~MDFLD_P1_MASK;
			REG_WRITE(dpll_reg, dpll);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);
		}

		/* When ungating power of DPLL, needs to wait 0.5us before enable the VCO */
		if (dpll & MDFLD_PWR_GATE_EN) {
			dpll &= ~MDFLD_PWR_GATE_EN;
			REG_WRITE(dpll_reg, dpll);
			/* FIXME_MDFLD PO - change 500 to 1 after PO */
			udelay(500);
		}	

		dpll = 0; 

#if 0 /* FIXME revisit later */
		if ((dev_priv->ksel == KSEL_CRYSTAL_19) || (dev_priv->ksel == KSEL_BYPASS_19) || (dev_priv->ksel == KSEL_BYPASS_25)) {
			dpll &= ~MDFLD_INPUT_REF_SEL;	
		} else if (dev_priv->ksel == KSEL_BYPASS_83_100) { 
			dpll |= MDFLD_INPUT_REF_SEL;	
		}
#endif /* FIXME revisit later */

		if (is_hdmi)
			dpll |= MDFLD_VCO_SEL;	

		fp = (clk_n / 2) << 16;
		fp |= m_conv; 

		/* compute bitmask from p1 value */
		dpll |= (1 << (clock.p1 - 2)) << 17;

#if 0 /* 1080p30 & 720p */
        	dpll = 0x00050000;
        	fp = 0x000001be;
#endif 
#if 0 /* 480p */
        	dpll = 0x02010000;
        	fp = 0x000000d2;
#endif 
	} else {
#if 0 /*DBI_TPO_480x864*/
		dpll = 0x00020000;
		fp = 0x00000156; 
#endif /* DBI_TPO_480x864 */ /* get from spec. */

        	dpll = 0x00800000;
	        fp = 0x000000c1;
}

	REG_WRITE(fp_reg, fp);
	REG_WRITE(dpll_reg, dpll);
	/* FIXME_MDFLD PO - change 500 to 1 after PO */
	udelay(500);

	dpll |= DPLL_VCO_ENABLE;
	REG_WRITE(dpll_reg, dpll);
	REG_READ(dpll_reg);

	/* wait for DSI PLL to lock */
	while ((timeout < 20000) && !(REG_READ(pipeconf_reg) & PIPECONF_DSIPLL_LOCK)) {
		udelay(150);
		timeout ++;
	}

	if (is_mipi)
		goto mrst_crtc_mode_set_exit;

	PSB_DEBUG_ENTRY("is_mipi = 0x%x \n", is_mipi);

	REG_WRITE(pipeconf_reg, *pipeconf);
	REG_READ(pipeconf_reg);

	/* Wait for for the pipe enable to take effect. */
//FIXME_JLIU7 HDMI	mrstWaitForPipeEnable(dev);

	REG_WRITE(dspcntr_reg, *dspcntr);
	psb_intel_wait_for_vblank(dev);

mrst_crtc_mode_set_exit:

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return 0;
}

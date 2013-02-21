/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2007-2008 Dave Airlie
 * Copyright © 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The DRM mode setting helper functions are common code for drivers to use if
 * they wish.  Drivers are not forced to use this code in their
 * implementations but it would be useful if they code they do use at least
 * provides a consistent interface and operation to userspace
 */

#ifndef __DRM_CRTC_HELPER_H__
#define __DRM_CRTC_HELPER_H__

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/idr.h>

#include <linux/fb.h>

enum mode_set_atomic {
	LEAVE_ATOMIC_MODE_SET,
	ENTER_ATOMIC_MODE_SET,
};

struct drm_crtc_helper_funcs {
	/*
	 * Control power levels on the CRTC.  If the mode passed in is
	 * unsupported, the provider must use the next lowest power level.
	 */
	void (*dpms)(struct drm_crtc *crtc, int mode);
	void (*prepare)(struct drm_crtc *crtc);
	void (*commit)(struct drm_crtc *crtc);

	/* Provider can fixup or change mode timings before modeset occurs */
	bool (*mode_fixup)(struct drm_crtc *crtc,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	/* Actually set the mode */
	int (*mode_set)(struct drm_crtc *crtc, struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode, int x, int y,
			struct drm_framebuffer *old_fb);

	/* Move the crtc on the current fb to the given position *optional* */
	int (*mode_set_base)(struct drm_crtc *crtc, int x, int y,
			     struct drm_framebuffer *old_fb);
	int (*mode_set_base_atomic)(struct drm_crtc *crtc,
				    struct drm_framebuffer *fb, int x, int y,
				    enum mode_set_atomic);

	/* reload the current crtc LUT */
	void (*load_lut)(struct drm_crtc *crtc);

	/* disable crtc when not in use - more explicit than dpms off */
	void (*disable)(struct drm_crtc *crtc);
};

struct drm_encoder_helper_funcs {
	void (*dpms)(struct drm_encoder *encoder, int mode);
	void (*save)(struct drm_encoder *encoder);
	void (*restore)(struct drm_encoder *encoder);

	bool (*mode_fixup)(struct drm_encoder *encoder,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	void (*prepare)(struct drm_encoder *encoder);
	void (*commit)(struct drm_encoder *encoder);
	void (*mode_set)(struct drm_encoder *encoder,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);
	struct drm_crtc *(*get_crtc)(struct drm_encoder *encoder);
	/* detect for DAC style encoders */
	enum drm_connector_status (*detect)(struct drm_encoder *encoder,
					    struct drm_connector *connector);
	/* disable encoder when not in use - more explicit than dpms off */
	void (*disable)(struct drm_encoder *encoder);
};

struct drm_connector_helper_funcs {
	int (*get_modes)(struct drm_connector *connector);
	int (*mode_valid)(struct drm_connector *connector,
			  struct drm_display_mode *mode);
	struct drm_encoder *(*best_encoder)(struct drm_connector *connector);
};

extern int drm_helper_probe_single_connector_modes(struct drm_connector *connector, uint32_t maxX, uint32_t maxY);
extern void drm_helper_disable_unused_functions(struct drm_device *dev);
extern int drm_crtc_helper_set_config(struct drm_mode_set *set);
extern bool drm_crtc_helper_set_mode(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     int x, int y,
				     struct drm_framebuffer *old_fb);
extern bool drm_helper_crtc_in_use(struct drm_crtc *crtc);
extern bool drm_helper_encoder_in_use(struct drm_encoder *encoder);

extern void drm_helper_connector_dpms(struct drm_connector *connector, int mode);

extern void drm_helper_get_fb_bpp_depth(uint32_t format, unsigned int *depth,
					int *bpp);
extern int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
					  struct drm_mode_fb_cmd2 *mode_cmd);

static inline void drm_crtc_helper_add(struct drm_crtc *crtc,
				       const struct drm_crtc_helper_funcs *funcs)
{
	crtc->helper_private = (void *)funcs;
}

static inline void drm_encoder_helper_add(struct drm_encoder *encoder,
					  const struct drm_encoder_helper_funcs *funcs)
{
	encoder->helper_private = (void *)funcs;
}

static inline void drm_connector_helper_add(struct drm_connector *connector,
					    const struct drm_connector_helper_funcs *funcs)
{
	connector->helper_private = (void *)funcs;
}

extern int drm_helper_resume_force_mode(struct drm_device *dev);
extern void drm_kms_helper_poll_init(struct drm_device *dev);
extern void drm_kms_helper_poll_fini(struct drm_device *dev);
extern void drm_helper_hpd_irq_event(struct drm_device *dev);

extern void drm_kms_helper_poll_disable(struct drm_device *dev);
extern void drm_kms_helper_poll_enable(struct drm_device *dev);

extern int drm_format_num_planes(uint32_t format);
extern int drm_format_plane_cpp(uint32_t format, int plane);
extern int drm_format_horz_chroma_subsampling(uint32_t format);
extern int drm_format_vert_chroma_subsampling(uint32_t format);
extern int drm_framebuffer_check(const struct drm_mode_fb_cmd2 *r,
				 const uint64_t sizes[4]);

/**
 * drm_region - two dimensional region
 * @x1: horizontal starting coordinate (inclusive)
 * @x2: horizontal ending coordinate (exclusive)
 * @y1: vertical starting coordinate (inclusive)
 * @y2: vertical ending coordinate (exclusive)
 */
struct drm_region {
	int x1, y1, x2, y2;
};

extern void drm_region_adjust_size(struct drm_region *r, int x, int y);
extern void drm_region_translate(struct drm_region *r, int x, int y);
extern void drm_region_subsample(struct drm_region *r, int hsub, int vsub);
extern int drm_region_width(const struct drm_region *r);
extern int drm_region_height(const struct drm_region *r);
extern bool drm_region_visible(const struct drm_region *r);
extern bool drm_region_clip(struct drm_region *r,
			    const struct drm_region *clip);
extern bool drm_region_clip_scaled(struct drm_region *src,
				   struct drm_region *dst,
				   const struct drm_region *clip,
				   int hscale, int vscale);
extern int drm_calc_hscale(struct drm_region *src, struct drm_region *dst,
			   int min_hscale, int max_hscale);
extern int drm_calc_vscale(struct drm_region *src, struct drm_region *dst,
			   int min_vscale, int max_vscale);
extern void drm_plane_opts_defaults(struct drm_plane_opts *opts);
extern void drm_chroma_phase_offsets(int *ret_xoff, int *ret_yoff,
				     int hsub, int vsub, uint8_t chroma_siting,
				     bool second_chroma_plane);

#endif

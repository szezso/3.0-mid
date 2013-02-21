/*
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include "drmP.h"
#include "drm_crtc.h"
#include "drm_fourcc.h"
#include "drm_crtc_helper.h"
#include "drm_fb_helper.h"

static bool drm_kms_helper_poll = true;
module_param_named(poll, drm_kms_helper_poll, bool, 0600);

static void drm_mode_validate_flag(struct drm_connector *connector,
				   int flags)
{
	struct drm_display_mode *mode, *t;

	if (flags == (DRM_MODE_FLAG_DBLSCAN | DRM_MODE_FLAG_INTERLACE))
		return;

	list_for_each_entry_safe(mode, t, &connector->modes, head) {
		if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
				!(flags & DRM_MODE_FLAG_INTERLACE))
			mode->status = MODE_NO_INTERLACE;
		if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
				!(flags & DRM_MODE_FLAG_DBLSCAN))
			mode->status = MODE_NO_DBLESCAN;
	}

	return;
}

/**
 * drm_helper_probe_single_connector_modes - get complete set of display modes
 * @dev: DRM device
 * @maxX: max width for modes
 * @maxY: max height for modes
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Based on @dev's mode_config layout, scan all the connectors and try to detect
 * modes on them.  Modes will first be added to the connector's probed_modes
 * list, then culled (based on validity and the @maxX, @maxY parameters) and
 * put into the normal modes list.
 *
 * Intended to be used either at bootup time or when major configuration
 * changes have occurred.
 *
 * FIXME: take into account monitor limits
 *
 * RETURNS:
 * Number of modes found on @connector.
 */
int drm_helper_probe_single_connector_modes(struct drm_connector *connector,
					    uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode, *t;
	struct drm_connector_helper_funcs *connector_funcs =
		connector->helper_private;
	int count = 0;
	int mode_flags = 0;

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s]\n", connector->base.id,
			drm_get_connector_name(connector));
	/* set all modes to the unverified state */
	list_for_each_entry_safe(mode, t, &connector->modes, head)
		mode->status = MODE_UNVERIFIED;

	if (connector->force) {
		if (connector->force == DRM_FORCE_ON)
			connector->status = connector_status_connected;
		else
			connector->status = connector_status_disconnected;
		if (connector->funcs->force)
			connector->funcs->force(connector);
	} else {
		connector->status = connector->funcs->detect(connector, true);
		drm_kms_helper_poll_enable(dev);
	}

	if (connector->status == connector_status_disconnected) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] disconnected\n",
			connector->base.id, drm_get_connector_name(connector));
		drm_mode_connector_update_edid_property(connector, NULL);
		goto prune;
	}

	count = (*connector_funcs->get_modes)(connector);
	if (count == 0 && connector->status == connector_status_connected)
		count = drm_add_modes_noedid(connector, 1024, 768);
	if (count == 0)
		goto prune;

	drm_mode_connector_list_update(connector);

	if (maxX && maxY)
		drm_mode_validate_size(dev, &connector->modes, maxX,
				       maxY, 0);

	if (connector->interlace_allowed)
		mode_flags |= DRM_MODE_FLAG_INTERLACE;
	if (connector->doublescan_allowed)
		mode_flags |= DRM_MODE_FLAG_DBLSCAN;
	drm_mode_validate_flag(connector, mode_flags);

	list_for_each_entry_safe(mode, t, &connector->modes, head) {
		if (mode->status == MODE_OK)
			mode->status = connector_funcs->mode_valid(connector,
								   mode);
	}

prune:
	drm_mode_prune_invalid(dev, &connector->modes, true);

	if (list_empty(&connector->modes))
		return 0;

	drm_mode_sort(&connector->modes);

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s] probed modes :\n", connector->base.id,
			drm_get_connector_name(connector));
	list_for_each_entry_safe(mode, t, &connector->modes, head) {
		mode->vrefresh = drm_mode_vrefresh(mode);

		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}

	return count;
}
EXPORT_SYMBOL(drm_helper_probe_single_connector_modes);

/**
 * drm_helper_encoder_in_use - check if a given encoder is in use
 * @encoder: encoder to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @encoders's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * True if @encoder is part of the mode_config, false otherwise.
 */
bool drm_helper_encoder_in_use(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_helper_encoder_in_use);

/**
 * drm_helper_crtc_in_use - check if a given CRTC is in a mode_config
 * @crtc: CRTC to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @crtc's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * True if @crtc is part of the mode_config, false otherwise.
 */
bool drm_helper_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;
	/* FIXME: Locking around list access? */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head)
		if (encoder->crtc == crtc && drm_helper_encoder_in_use(encoder))
			return true;
	return false;
}
EXPORT_SYMBOL(drm_helper_crtc_in_use);

static void
drm_encoder_disable(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;

	if (encoder_funcs->disable)
		(*encoder_funcs->disable)(encoder);
	else
		(*encoder_funcs->dpms)(encoder, DRM_MODE_DPMS_OFF);
}

/**
 * drm_helper_disable_unused_functions - disable unused objects
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * If an connector or CRTC isn't part of @dev's mode_config, it can be disabled
 * by calling its dpms function, which should power it off.
 */
void drm_helper_disable_unused_functions(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct drm_crtc *crtc;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;
		if (connector->status == connector_status_disconnected)
			connector->encoder = NULL;
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (!drm_helper_encoder_in_use(encoder)) {
			drm_encoder_disable(encoder);
			/* disconnector encoder from any connector */
			encoder->crtc = NULL;
		}
	}

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc->enabled = drm_helper_crtc_in_use(crtc);
		if (!crtc->enabled) {
			if (crtc_funcs->disable)
				(*crtc_funcs->disable)(crtc);
			else
				(*crtc_funcs->dpms)(crtc, DRM_MODE_DPMS_OFF);
			crtc->fb = NULL;
		}
	}
}
EXPORT_SYMBOL(drm_helper_disable_unused_functions);

/**
 * drm_encoder_crtc_ok - can a given crtc drive a given encoder?
 * @encoder: encoder to test
 * @crtc: crtc to test
 *
 * Return false if @encoder can't be driven by @crtc, true otherwise.
 */
static bool drm_encoder_crtc_ok(struct drm_encoder *encoder,
				struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct drm_crtc *tmp;
	int crtc_mask = 1;

	WARN(!crtc, "checking null crtc?\n");

	dev = crtc->dev;

	list_for_each_entry(tmp, &dev->mode_config.crtc_list, head) {
		if (tmp == crtc)
			break;
		crtc_mask <<= 1;
	}

	if (encoder->possible_crtcs & crtc_mask)
		return true;
	return false;
}

/*
 * Check the CRTC we're going to map each output to vs. its current
 * CRTC.  If they don't match, we have to disable the output and the CRTC
 * since the driver will have to re-route things.
 */
static void
drm_crtc_prepare_encoders(struct drm_device *dev)
{
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_encoder *encoder;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder_funcs = encoder->helper_private;
		/* Disable unused encoders */
		if (encoder->crtc == NULL)
			drm_encoder_disable(encoder);
		/* Disable encoders whose CRTC is about to change */
		if (encoder_funcs->get_crtc &&
		    encoder->crtc != (*encoder_funcs->get_crtc)(encoder))
			drm_encoder_disable(encoder);
	}
}

/**
 * drm_crtc_set_mode - set a mode
 * @crtc: CRTC to program
 * @mode: mode to use
 * @x: width of mode
 * @y: height of mode
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Try to set @mode on @crtc.  Give @crtc and its associated connectors a chance
 * to fixup or reject the mode prior to trying to set it.
 *
 * RETURNS:
 * True if the mode was set successfully, or false otherwise.
 */
bool drm_crtc_helper_set_mode(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode, saved_hwmode;
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	struct drm_encoder_helper_funcs *encoder_funcs;
	int saved_x, saved_y;
	struct drm_encoder *encoder;
	bool ret = true;

	crtc->enabled = drm_helper_crtc_in_use(crtc);
	if (!crtc->enabled)
		return true;

	adjusted_mode = drm_mode_duplicate(dev, mode);
	if (!adjusted_mode)
		return false;

	saved_hwmode = crtc->hwmode;
	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;

	/* Update crtc values up front so the driver can rely on them for mode
	 * setting.
	 */
	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	/* Pass our mode to the connectors and the CRTC to give them a chance to
	 * adjust it according to limitations or connector properties, and also
	 * a chance to reject the mode entirely.
	 */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		if (!(ret = encoder_funcs->mode_fixup(encoder, mode,
						      adjusted_mode))) {
			goto done;
		}
	}

	if (!(ret = crtc_funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		goto done;
	}
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	/* Prepare the encoders and CRTCs before setting the mode. */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		/* Disable the encoders as the first thing we do. */
		encoder_funcs->prepare(encoder);
	}

	drm_crtc_prepare_encoders(dev);

	crtc_funcs->prepare(crtc);

	/* Set up the DPLL and any encoders state that needs to adjust or depend
	 * on the DPLL.
	 */
	ret = !crtc_funcs->mode_set(crtc, mode, adjusted_mode, x, y, old_fb);
	if (!ret)
	    goto done;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		DRM_DEBUG_KMS("[ENCODER:%d:%s] set [MODE:%d:%s]\n",
			encoder->base.id, drm_get_encoder_name(encoder),
			mode->base.id, mode->name);
		encoder_funcs = encoder->helper_private;
		encoder_funcs->mode_set(encoder, mode, adjusted_mode);
	}

	/* Now enable the clocks, plane, pipe, and connectors that we set up. */
	crtc_funcs->commit(crtc);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		encoder_funcs = encoder->helper_private;
		encoder_funcs->commit(encoder);

	}

	/* Store real post-adjustment hardware mode. */
	crtc->hwmode = *adjusted_mode;

	/* Calculate and store various constants which
	 * are later needed by vblank and swap-completion
	 * timestamping. They are derived from true hwmode.
	 */
	drm_calc_timestamping_constants(crtc);

	/* FIXME: add subpixel order */
done:
	drm_mode_destroy(dev, adjusted_mode);
	if (!ret) {
		crtc->hwmode = saved_hwmode;
		crtc->mode = saved_mode;
		crtc->x = saved_x;
		crtc->y = saved_y;
	}

	return ret;
}
EXPORT_SYMBOL(drm_crtc_helper_set_mode);


/**
 * drm_crtc_helper_set_config - set a new config from userspace
 * @crtc: CRTC to setup
 * @crtc_info: user provided configuration
 * @new_mode: new mode to set
 * @connector_set: set of connectors for the new config
 * @fb: new framebuffer
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Setup a new configuration, provided by the user in @crtc_info, and enable
 * it.
 *
 * RETURNS:
 * Zero. (FIXME)
 */
int drm_crtc_helper_set_config(struct drm_mode_set *set)
{
	struct drm_device *dev;
	struct drm_crtc *save_crtcs, *new_crtc, *crtc;
	struct drm_encoder *save_encoders, *new_encoder, *encoder;
	struct drm_framebuffer *old_fb = NULL;
	bool mode_changed = false; /* if true do a full mode set */
	bool fb_changed = false; /* if true and !mode_changed just do a flip */
	struct drm_connector *save_connectors, *connector;
	struct drm_connector *hdmiconnector = NULL;
	int count = 0, ro, fail = 0;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret = 0;
	int i;
	/* true if this is triggerred from hotplug event*/
	bool in_hotplug = false;

	DRM_DEBUG_KMS("\n");

	if (!set)
		return -EINVAL;

	if (!set->crtc)
		return -EINVAL;

	if (!set->crtc->helper_private)
		return -EINVAL;

	crtc_funcs = set->crtc->helper_private;

	if (!set->mode)
		set->fb = NULL;

	if (set->fb) {
		DRM_DEBUG_KMS("[CRTC:%d] [FB:%d] #connectors=%d (x y) (%i %i)\n",
				set->crtc->base.id, set->fb->base.id,
				(int)set->num_connectors, set->x, set->y);
	} else {
		DRM_DEBUG_KMS("[CRTC:%d] [NOFB]\n", set->crtc->base.id);
		set->mode = NULL;
		set->num_connectors = 0;
	}

	dev = set->crtc->dev;

	/* Allocate space for the backup of all (non-pointer) crtc, encoder and
	 * connector data. */
	save_crtcs = kzalloc(dev->mode_config.num_crtc *
			     sizeof(struct drm_crtc), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	save_encoders = kzalloc(dev->mode_config.num_encoder *
				sizeof(struct drm_encoder), GFP_KERNEL);
	if (!save_encoders) {
		kfree(save_crtcs);
		return -ENOMEM;
	}

	save_connectors = kzalloc(dev->mode_config.num_connector *
				sizeof(struct drm_connector), GFP_KERNEL);
	if (!save_connectors) {
		kfree(save_crtcs);
		kfree(save_encoders);
		return -ENOMEM;
	}

	/* Copy data. Note that driver private data is not affected.
	 * Should anything bad happen only the expected state is
	 * restored, not the drivers personal bookkeeping.
	 */
	count = 0;
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		save_crtcs[count++] = *crtc;
	}

	count = 0;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		save_encoders[count++] = *encoder;
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		save_connectors[count++] = *connector;
	}

	/* We should be able to check here if the fb has the same properties
	 * and then just flip_or_move it */
	if (set->crtc->fb != set->fb) {
		/* If we have no fb then treat it as a full mode set */
		if (set->crtc->fb == NULL) {
			DRM_DEBUG_KMS("crtc has no fb, full mode set\n");
			mode_changed = true;
		} else if (set->fb == NULL) {
			mode_changed = true;
		} else
			fb_changed = true;
	}

	if (set->x != set->crtc->x || set->y != set->crtc->y)
		fb_changed = true;

	if (set->mode && !drm_mode_equal(set->mode, &set->crtc->mode)) {
		DRM_DEBUG_KMS("modes are different, full mode set\n");
		drm_mode_debug_printmodeline(&set->crtc->mode);
		drm_mode_debug_printmodeline(set->mode);
		mode_changed = true;
	}

	/* a) traverse passed in connector list and get encoders for them */
	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct drm_connector_helper_funcs *connector_funcs =
			connector->helper_private;
		new_encoder = connector->encoder;
		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector) {
				new_encoder = connector_funcs->best_encoder(connector);
				/* if we can't get an encoder for a connector
				   we are setting now - then fail */
				if (new_encoder == NULL)
					/* don't break so fail path works correct */
					fail = 1;
				break;
			}
		}

		if (new_encoder != connector->encoder) {
			DRM_DEBUG_KMS("encoder changed, full mode switch\n");
			mode_changed = true;
			/* If the encoder is reused for another connector, then
			 * the appropriate crtc will be set later.
			 */
			if (connector->encoder)
				connector->encoder->crtc = NULL;
			connector->encoder = new_encoder;
		}
	}

	if (fail) {
		ret = -EINVAL;
		goto fail;
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;

		if (connector->encoder->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = connector->encoder->crtc;

		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector)
				new_crtc = set->crtc;
		}

		/* Make sure the new CRTC will work with the encoder */
		if (new_crtc &&
		    !drm_encoder_crtc_ok(connector->encoder, new_crtc)) {
			ret = -EINVAL;
			goto fail;
		}
		if (new_crtc != connector->encoder->crtc) {
			DRM_DEBUG_KMS("crtc changed, full mode switch\n");
			mode_changed = true;
			connector->encoder->crtc = new_crtc;
		}
		if (new_crtc) {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [CRTC:%d]\n",
				connector->base.id, drm_get_connector_name(connector),
				new_crtc->base.id);
		} else {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [NOCRTC]\n",
				connector->base.id, drm_get_connector_name(connector));
		}
	}

	/* mode_set_base is not a required function */
	if (fb_changed && !crtc_funcs->mode_set_base)
		mode_changed = true;

	if (mode_changed) {
		set->crtc->enabled = drm_helper_crtc_in_use(set->crtc);
		/* check whether the crtc is used for HDMI output */
		list_for_each_entry(connector,
			&dev->mode_config.connector_list, head) {
			if (!connector->encoder)
				continue;
			if ((connector->encoder->crtc == set->crtc) &&
				(connector->connector_type
					== DRM_MODE_CONNECTOR_DVID)) {
				hdmiconnector = connector;
				break;
			}
		}

		/* check whether this is from HDMI hotplug */
		if (set->fb) {
			struct drm_fb_helper *fb_helper =
				(struct drm_fb_helper *)set->fb->helper_private;
			if (fb_helper)
				in_hotplug = fb_helper->hotplug;
		}

		/* FIXME: This is workaround of i2c failure when setup local
		 * MIPI in hotplug sequence.
		 * We only set mode for HDMI pipe in HDMI hotplug sequence.
		 * This should be reverted after fix the bug.
		 */
		if (set->crtc->enabled &&
				(hdmiconnector || !in_hotplug)) {
			DRM_DEBUG_KMS("attempting to set mode from"
					" userspace\n");
			drm_mode_debug_printmodeline(set->mode);
			old_fb = set->crtc->fb;
			set->crtc->fb = set->fb;
			if (!drm_crtc_helper_set_mode(set->crtc, set->mode,
						      set->x, set->y,
						      old_fb)) {
				DRM_ERROR("failed to set mode on [CRTC:%d]\n",
					  set->crtc->base.id);
				set->crtc->fb = old_fb;
				ret = -EINVAL;
				goto fail;
			}
			DRM_DEBUG_KMS("Setting connector DPMS state to on\n");
			for (i = 0; i < set->num_connectors; i++) {
				DRM_DEBUG_KMS("\t[CONNECTOR:%d:%s] set DPMS on\n", set->connectors[i]->base.id,
					      drm_get_connector_name(set->connectors[i]));
				set->connectors[i]->dpms = DRM_MODE_DPMS_ON;
			}
		}
		drm_helper_disable_unused_functions(dev);
	} else if (fb_changed) {
		set->crtc->x = set->x;
		set->crtc->y = set->y;

		old_fb = set->crtc->fb;
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		ret = crtc_funcs->mode_set_base(set->crtc,
						set->x, set->y, old_fb);
		if (ret != 0) {
			set->crtc->fb = old_fb;
			goto fail;
		}
	}

	kfree(save_connectors);
	kfree(save_encoders);
	kfree(save_crtcs);
	return 0;

fail:
	/* Restore all previous data. */
	count = 0;
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		*crtc = save_crtcs[count++];
	}

	count = 0;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		*encoder = save_encoders[count++];
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		*connector = save_connectors[count++];
	}

	kfree(save_connectors);
	kfree(save_encoders);
	kfree(save_crtcs);
	return ret;
}
EXPORT_SYMBOL(drm_crtc_helper_set_config);

static int drm_helper_choose_encoder_dpms(struct drm_encoder *encoder)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

static int drm_helper_choose_crtc_dpms(struct drm_crtc *crtc)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = crtc->dev;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder && connector->encoder->crtc == crtc)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

/**
 * drm_helper_connector_dpms
 * @connector affected connector
 * @mode DPMS mode
 *
 * Calls the low-level connector DPMS function, then
 * calls appropriate encoder and crtc DPMS functions as well
 */
void drm_helper_connector_dpms(struct drm_connector *connector, int mode)
{
	struct drm_encoder *encoder = connector->encoder;
	struct drm_crtc *crtc = encoder ? encoder->crtc : NULL;
	int old_dpms;

	if (mode == connector->dpms)
		return;

	old_dpms = connector->dpms;
	connector->dpms = mode;

	/* from off to on, do crtc then encoder */
	if (mode < old_dpms) {
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
	}

	/* from on to off, do encoder then crtc */
	if (mode > old_dpms) {
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}

	return;
}
EXPORT_SYMBOL(drm_helper_connector_dpms);

/*
 * Just need to support RGB formats here for compat with code that doesn't
 * use pixel formats directly yet.
 */
void drm_helper_get_fb_bpp_depth(uint32_t format, unsigned int *depth,
				 int *bpp)
{
	switch (format) {
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
		*depth = 8;
		*bpp = 8;
		break;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_BGRA5551:
		*depth = 15;
		*bpp = 16;
		break;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		*depth = 16;
		*bpp = 16;
		break;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		*depth = 24;
		*bpp = 24;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		*depth = 24;
		*bpp = 32;
		break;
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		*depth = 30;
		*bpp = 32;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
		*depth = 32;
		*bpp = 32;
		break;
	default:
		DRM_DEBUG_KMS("unsupported pixel format\n");
		*depth = 0;
		*bpp = 0;
		break;
	}
}
EXPORT_SYMBOL(drm_helper_get_fb_bpp_depth);

int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
				   struct drm_mode_fb_cmd2 *mode_cmd)
{
	int i;

	fb->width = mode_cmd->width;
	fb->height = mode_cmd->height;

	for (i = 0; i < 4; i++) {
		fb->pitches[i] = mode_cmd->pitches[i];
		fb->offsets[i] = mode_cmd->offsets[i];
	}

	drm_helper_get_fb_bpp_depth(mode_cmd->pixel_format, &fb->depth,
				    &fb->bits_per_pixel);
	fb->pixel_format = mode_cmd->pixel_format;

	return 0;
}
EXPORT_SYMBOL(drm_helper_mode_fill_fb_struct);

int drm_helper_resume_force_mode(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {

		if (!crtc->enabled)
			continue;

		ret = drm_crtc_helper_set_mode(crtc, &crtc->mode,
					       crtc->x, crtc->y, crtc->fb);

		if (ret == false)
			DRM_ERROR("failed to set mode on crtc %p\n", crtc);

		/* Turn off outputs that were already powered off */
		if (drm_helper_choose_crtc_dpms(crtc)) {
			list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

				if(encoder->crtc != crtc)
					continue;

				encoder_funcs = encoder->helper_private;
				if (encoder_funcs->dpms)
					(*encoder_funcs->dpms) (encoder,
								drm_helper_choose_encoder_dpms(encoder));
			}

			crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}
	/* disable the unused connectors while restoring the modesetting */
	drm_helper_disable_unused_functions(dev);
	return 0;
}
EXPORT_SYMBOL(drm_helper_resume_force_mode);

#define DRM_OUTPUT_POLL_PERIOD (10*HZ)
static void output_poll_execute(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct drm_device *dev = container_of(delayed_work, struct drm_device, mode_config.output_poll_work);
	struct drm_connector *connector;
	enum drm_connector_status old_status;
	bool repoll = false, changed = false;
	struct drm_property property;

	if (!drm_kms_helper_poll)
		return;

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {

		/* if this is HPD or polled don't check it -
		   TV out for instance */
		if (!connector->polled)
			continue;

		else if (connector->polled & (DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT))
			repoll = true;

		old_status = connector->status;
		/* if we are connected and don't want to poll for disconnect
		   skip it */
		if (old_status == connector_status_connected &&
		    !(connector->polled & DRM_CONNECTOR_POLL_DISCONNECT) &&
		    !(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		connector->status = connector->funcs->detect(connector, false);
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] status updated from %d to %d\n",
			      connector->base.id,
			      drm_get_connector_name(connector),
			      old_status, connector->status);
		if (old_status != connector->status)
			changed = true;
	}

	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {
		/* send a uevent + call fbdev */
		drm_sysfs_hotplug_event(dev);
		if (dev->mode_config.funcs->output_poll_changed)
			dev->mode_config.funcs->output_poll_changed(dev);
	}

	list_for_each_entry(connector,
			    &dev->mode_config.connector_list,
			    head) {
		if (connector->funcs->set_property) {
			strncpy(property.name, "hdmi-send-uevent",
				sizeof("hdmi-send-uevent"));
			connector->funcs->set_property(connector,
						       &property,
						       connector->status);
		}
	}

	if (repoll)
		queue_delayed_work(system_nrt_wq, delayed_work, DRM_OUTPUT_POLL_PERIOD);
}

void drm_kms_helper_poll_disable(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled)
		return;
	cancel_delayed_work_sync(&dev->mode_config.output_poll_work);
}
EXPORT_SYMBOL(drm_kms_helper_poll_disable);

void drm_kms_helper_poll_enable(struct drm_device *dev)
{
	bool poll = false;
	struct drm_connector *connector;

	if (!dev->mode_config.poll_enabled || !drm_kms_helper_poll)
		return;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (connector->polled)
			poll = true;
	}

	if (poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, DRM_OUTPUT_POLL_PERIOD);
}
EXPORT_SYMBOL(drm_kms_helper_poll_enable);

void drm_kms_helper_poll_init(struct drm_device *dev)
{
	INIT_DELAYED_WORK(&dev->mode_config.output_poll_work, output_poll_execute);
	dev->mode_config.poll_enabled = true;

	drm_kms_helper_poll_enable(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_init);

void drm_kms_helper_poll_fini(struct drm_device *dev)
{
	drm_kms_helper_poll_disable(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_fini);

void drm_helper_hpd_irq_event(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled)
		return;

	/* kill timer and schedule immediate execution, this doesn't block */
	cancel_delayed_work(&dev->mode_config.output_poll_work);
	if (drm_kms_helper_poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, 0);
}
EXPORT_SYMBOL(drm_helper_hpd_irq_event);


/**
 * drm_format_num_planes - get the number of planes for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * RETURNS:
 * The number of planes used by the specified pixel format.
 */
int drm_format_num_planes(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
		return 3;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_num_planes);

/**
 * drm_format_plane_cpp - determine the bytes per pixel value
 * @format: pixel format (DRM_FORMAT_*)
 * @plane: plane index
 *
 * RETURNS:
 * The bytes per pixel value for the specified plane.
 */
int drm_format_plane_cpp(uint32_t format, int plane)
{
	unsigned int depth;
	int bpp;

	if (plane >= drm_format_num_planes(format))
		return 0;

	switch (format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return 2;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		return plane ? 2 : 1;
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
		return 1;
	default:
		drm_helper_get_fb_bpp_depth(format, &depth, &bpp);
		return bpp >> 3;
	}
}
EXPORT_SYMBOL(drm_format_plane_cpp);

/**
 * drm_format_horz_chroma_subsampling - get the horizontal chroma subsampling factor
 * @format: pixel format (DRM_FORMAT_*)
 *
 * RETURNS:
 * The horizontal chroma subsampling factor for the
 * specified pixel format.
 */
int drm_format_horz_chroma_subsampling(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YUV410:
		return 4;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_horz_chroma_subsampling);

/**
 * drm_format_vert_chroma_subsampling - get the vertical chroma subsampling factor
 * @format: pixel format (DRM_FORMAT_*)
 *
 * RETURNS:
 * The vertical chroma subsampling factor for the
 * specified pixel format.
 */
int drm_format_vert_chroma_subsampling(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV410:
		return 4;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_vert_chroma_subsampling);

/**
 * drm_framebuffer_check - check the framebuffer layout
 * @r: cmd from ioctl
 * @sizes: sizes of the used BOs
 *
 * For each handle in the handles[] array of @r, the size of the
 * corresponding BO must be passed in @sizes using the same index.
 *
 * RETURNS:
 * Zero on success, error code on failure.
 */
int drm_framebuffer_check(const struct drm_mode_fb_cmd2 *r, const uint64_t sizes[4])
{
	struct {
		unsigned int start, end;
	} ranges[4];
	unsigned int req_sizes[4] = {};
	int hsub = drm_format_horz_chroma_subsampling(r->pixel_format);
	int vsub = drm_format_vert_chroma_subsampling(r->pixel_format);
	int num_planes = drm_format_num_planes(r->pixel_format);
	int i, j;

	if (r->width == 0 || r->height == 0)
		return -EINVAL;

	/* Keep things safe for s15.16 fixed point math. */
	if (r->width > 0x7fff || r->height > 0x7fff)
		return -ERANGE;

	if (r->width % hsub || r->height % vsub)
		return -EINVAL;

	for (i = 0; i < num_planes; i++) {
		unsigned int height = r->height / (i != 0 ? vsub : 1);
		unsigned int width = r->width / (i != 0 ? hsub : 1);
		unsigned int size = r->pitches[i] * height;
		unsigned int cpp = drm_format_plane_cpp(r->pixel_format, i);
		unsigned int min_pitch = cpp * width;

		if (!r->handles[i])
			return -EINVAL;

		if (size < r->pitches[i] || size < height)
			return -ERANGE;

		if (min_pitch < width || min_pitch < cpp)
			return -ERANGE;

		if (r->pitches[i] < min_pitch)
			return -EINVAL;

		ranges[i].start = r->offsets[i];
		ranges[i].end = ranges[i].start + size;

		if (ranges[i].end < ranges[i].start)
			return -ERANGE;

		/* update all indexes of req_sizes that match this handle */
		for (j = 0; j < num_planes; j++) {
			if (r->handles[i] == r->handles[j])
				req_sizes[j] = max(req_sizes[j], ranges[i].end);
		}
	}

	/* Check that the passed BO sizes are sufficient */
	for (i = 0; i < num_planes; i++) {
		if (sizes[i] < req_sizes[i])
			return -ENOSPC;
	}

	/* Check for overlapping ranges within the same BO */
	/* FIXME what about formats with interleaved planes (eg. IMC2/IMC4)? */
	for (i = 0; i < num_planes; i++) {
		for (j = i + 1; j < num_planes; j++) {
			if (r->handles[i] == r->handles[j] &&
			    ranges[i].start < ranges[j].end &&
			    ranges[j].start < ranges[i].end)
				return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_framebuffer_check);


/**
 * drm_region_adjust_size - adjust the size of the region
 * @r: region to be adjusted
 * @x: horizontal adjustment
 * @y: vertical adjustment
 *
 * Change the size of region @r by @x in the horizontal direction,
 * and by @y in the vertical direction, while keeping the center
 * of @r stationary.
 *
 * Positive @x and @y increase the size, negative values decrease it.
 */
void drm_region_adjust_size(struct drm_region *r, int x, int y)
{
	r->x1 -= x >> 1;
	r->y1 -= y >> 1;
	r->x2 += (x + 1) >> 1;
	r->y2 += (y + 1) >> 1;
}
EXPORT_SYMBOL(drm_region_adjust_size);

/**
 * drm_region_translate - translate the region
 * @r: region to be tranlated
 * @x: horizontal translation
 * @y: vertical translation
 *
 * Move region @r by @x in the horizontal direction,
 * and by @y in the vertical direction.
 */
void drm_region_translate(struct drm_region *r, int x, int y)
{
	r->x1 += x;
	r->y1 += y;
	r->x2 += x;
	r->y2 += y;
}
EXPORT_SYMBOL(drm_region_translate);

/**
 * drm_region_subsample - subsample a region
 * @r: region to be subsampled
 * @hsub: horizontal subsampling factor
 * @vsub: vertical subsampling factor
 *
 * Divide the coordinates of region @r by @hsub and @vsub.
 */
void drm_region_subsample(struct drm_region *r, int hsub, int vsub)
{
	r->x1 /= hsub;
	r->y1 /= vsub;
	r->x2 /= hsub;
	r->y2 /= vsub;
}
EXPORT_SYMBOL(drm_region_subsample);

/**
 * drm_region_width - determine the region width
 * @r: region whose width is returned
 *
 * RETURNS:
 * The width of the region.
 */
int drm_region_width(const struct drm_region *r)
{
	return r->x2 - r->x1;
}
EXPORT_SYMBOL(drm_region_width);

/**
 * drm_region_height - determine the region height
 * @r: region whose height is returned
 *
 * RETURNS:
 * The height of the region.
 */
int drm_region_height(const struct drm_region *r)
{
	return r->y2 - r->y1;
}
EXPORT_SYMBOL(drm_region_height);

/**
 * drm_region_visible - determine if the the region is visible
 * @r: region whose visibility is returned
 *
 * RETURNS:
 * @true if the region is visible, @false otherwise.
 */
bool drm_region_visible(const struct drm_region *r)
{
	return drm_region_width(r) > 0 && drm_region_height(r) > 0;
}
EXPORT_SYMBOL(drm_region_visible);

/**
 * drm_region_clip - clip one region by another region
 * @r: region to be clipped
 * @clip: clip region
 *
 * Clip region @r by region @clip.
 *
 * RETURNS:
 * @true if the region is still visible after being clipped,
 * @false otherwise.
 */
bool drm_region_clip(struct drm_region *r, const struct drm_region *clip)
{
	r->x1 = max(r->x1, clip->x1);
	r->y1 = max(r->y1, clip->y1);
	r->x2 = min(r->x2, clip->x2);
	r->y2 = min(r->y2, clip->y2);

	return drm_region_visible(r);
}
EXPORT_SYMBOL(drm_region_clip);

/**
 * drm_region_clip_scaled - perform a scaled clip operation
 * @src: source window region
 * @dst: destination window region
 * @clip: clip region
 * @hscale: horizontal scaling factor
 * @vscale: vertical scaling factor
 *
 * Clip region @dst by region @clip. Clip region @src by the same
 * amounts multiplied by @hscale and @vscale.
 *
 * RETUTRNS:
 * @true if region @dst is still visible after being clipped,
 * @false otherwise
 */
bool drm_region_clip_scaled(struct drm_region *src, struct drm_region *dst,
			    const struct drm_region *clip,
			    int hscale, int vscale)
{
	int diff;

	diff = clip->x1 - dst->x1;
	if (diff > 0)
		src->x1 += diff * hscale;
	diff = clip->y1 - dst->y1;
	if (diff > 0)
		src->y1 += diff * vscale;
	diff = dst->x2 - clip->x2;
	if (diff > 0)
		src->x2 -= diff * hscale;
	diff = dst->y2 - clip->y2;
	if (diff > 0)
		src->y2 -= diff * vscale;

	return drm_region_clip(dst, clip);
}
EXPORT_SYMBOL(drm_region_clip_scaled);

/**
 * drm_calc_hscale - calculate the horizontal scaling factor
 * @src: source window region
 * @dst: destination window region
 * @min_hscale: minimum allowed horizontal scaling factor
 * @max_hscale: maximum allowed horizontal scaling factor
 *
 * Calculate the horizontal scaling factor as
 * (@src width) / (@dst width).
 *
 * If the calculated scaling factor is below @min_hscale,
 * decrease the width of region @dst to compensate.
 *
 * If the calculcated scaling factor is above @max_hscale,
 * decrease the width of region @src to compensate.
 *
 * RETURNS:
 * The horizontal scaling factor.
 */
int drm_calc_hscale(struct drm_region *src, struct drm_region *dst,
		    int min_hscale, int max_hscale)
{
	int src_w = drm_region_width(src);
	int dst_w = drm_region_width(dst);
	int hscale;

	if (dst_w <= 0)
		return 0;

	hscale = src_w / dst_w;

	if (hscale < min_hscale) {
		int max_dst_w = src_w / min_hscale;

		drm_region_adjust_size(dst, max_dst_w - dst_w, 0);

		return min_hscale;
	}

	if (hscale > max_hscale) {
		int max_src_w = dst_w * max_hscale;

		drm_region_adjust_size(src, max_src_w - src_w, 0);

		return max_hscale;
	}

	return hscale;
}
EXPORT_SYMBOL(drm_calc_hscale);

/**
 * drm_calc_vscale - calculate the vertical scaling factor
 * @src: source window region
 * @dst: destination window region
 * @min_vscale: minimum allowed vertical scaling factor
 * @max_vscale: maximum allowed vertical scaling factor
 *
 * Calculate the vertical scaling factor as
 * (@src height) / (@dst height).
 *
 * If the calculated scaling factor is below @min_vscale,
 * decrease the height of region @dst to compensate.
 *
 * If the calculcated scaling factor is above @max_vscale,
 * decrease the height of region @src to compensate.
 *
 * RETURNS:
 * The vertical scaling factor.
 */
int drm_calc_vscale(struct drm_region *src, struct drm_region *dst,
		    int min_vscale, int max_vscale)
{
	int src_h = drm_region_height(src);
	int dst_h = drm_region_height(dst);
	int vscale;

	if (dst_h <= 0)
		return 0;

	vscale = src_h / dst_h;

	if (vscale < min_vscale) {
		int max_dst_h = src_h / min_vscale;

		drm_region_adjust_size(dst, 0, max_dst_h - dst_h);

		return min_vscale;
	}

	if (vscale > max_vscale) {
		int max_src_h = dst_h * max_vscale;

		drm_region_adjust_size(src, 0, max_src_h - src_h);

		return max_vscale;
	}

	return vscale;
}
EXPORT_SYMBOL(drm_calc_vscale);

/**
 * drm_plane_opts_defaults - fill the plane opts with default values
 */
void drm_plane_opts_defaults(struct drm_plane_opts *opts)
{
	memset(opts, 0, sizeof *opts);

	opts->brightness = 0x8000;
	opts->contrast = 0x8000;
	opts->hue = 0x8000;
	opts->saturation = 0x8000;

	/* disable source color keying */
	opts->src_key_low = ~0ULL;

	opts->const_alpha = 0xffff;
}
EXPORT_SYMBOL(drm_plane_opts_defaults);

/**
 * drm_chroma_phase_offsets - calculate the chroma phase offsets
 * @ret_xoff: returned horizontal offset (16.16)
 * @ret_yoff: returned vertical offset (16.16)
 * @hsub: horizontal chroma subsampling factor
 * @vsub: vertical chroma subsampling factor
 * @chroma: chroma siting information
 * @second_chroma_plane: first or second chroma plane?
 *
 * Calculates the phase offset between chroma and luma pixel centers,
 * based on infromation provided in @chroma, @hsub, @vsub, and
 * @second_chroma_plane.
 *
 * RETURNS:
 * The chroma phase offsets in 16.16 format. The returned
 * phase offsets are in chroma (ie. subsampled) coordinate space.
 */
void drm_chroma_phase_offsets(int *ret_xoff, int *ret_yoff,
			      int hsub, int vsub, uint8_t chroma_siting,
			      bool second_chroma_plane)
{
	*ret_xoff = 0;
	*ret_yoff = 0;

	switch (chroma_siting & 0x3) {
	case DRM_CHROMA_SITING_HORZ_LEFT:
		break;
	case DRM_CHROMA_SITING_HORZ_CENTER:
		*ret_xoff -= (hsub - 1) * 0x8000 / hsub;
		break;
	}

	switch (chroma_siting & 0xc0) {
	case DRM_CHROMA_SITING_VERT_TOP:
		break;
	case DRM_CHROMA_SITING_VERT_CENTER:
		*ret_yoff -= (vsub - 1) * 0x8000 / vsub;
		break;
	}

	/* Chroma planes out of phase by 0.5 chroma lines? */
	if (second_chroma_plane &&
	    (chroma_siting & DRM_CHROMA_SITING_MISALIGNED_PLANES))
		*ret_yoff -= 0x8000;
}
EXPORT_SYMBOL(drm_chroma_phase_offsets);

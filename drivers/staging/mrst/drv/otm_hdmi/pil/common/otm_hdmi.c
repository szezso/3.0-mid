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


#include <linux/time.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"

#include "hdmi_internal.h"
#include "hdmi_timings.h"
#ifdef OTM_HDMI_HDCP_ENABLE
#include "hdcp_api.h"
#endif
#include "edid.h"
#include "ps_hdmi.h"
#include "ips_hdmi.h"
#include "infoframes_api.h"

/* TODO: Leave it here or move to some .h? */
#define OTM_HDMI_NAME "OTM HDMI"

/*
 * Table of attributes
 */
otm_hdmi_attribute_t otm_hdmi_attributes_table
		[OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES];

/* Placeholder for all TX supported modes */
static const otm_hdmi_timing_t *g_video_modes[MAX_TIMINGS];
/* Placeholder for all TX supported modes per CEA 861E spec used by EDID parser
 */
static const otm_hdmi_timing_t *g_video_modes_ref[MAX_TIMINGS];
static otm_hdmi_ret_t otm_hdmi_attr_set_validate(otm_hdmi_attribute_id_t id,
							void *value);
static otm_hdmi_ret_t otm_hdmi_attr_get_validate(otm_hdmi_attribute_id_t id);

static otm_hdmi_ret_t __pd_attr_declare(otm_hdmi_attribute_t *table,
				otm_hdmi_attribute_id_t id,
				otm_hdmi_attribute_type_t type,
				otm_hdmi_attribute_flag_t flags,
				char *name,
				void *value,
				unsigned int min,
				unsigned int max);

static otm_hdmi_ret_t __program_clocks(hdmi_context_t *ctx,
					 unsigned int dclk)
					__attribute__((unused));


static unsigned int g_gpio = GPIO_MIN;
static unsigned int g_dtv;
static unsigned int g_dc = 1;

#define EDID_SIGNATURE 0x00FFFFFFFFFFFF00ull

static hdmi_context_t *g_context;

/* This table preserves the special timings for DTV models */
static const otm_hdmi_timing_t *static_dtv_modes[] = {
	&MODE_1920x1080p60__PANEL,
	&MODE_1920x1080p50__PANEL,
	&MODE_1920x1080p48__PANEL,
	&MODE_1280x720p60__PANEL_FS,
	&MODE_1280x720p50__PANEL_FS,
	&MODE_1920x540p60__PANEL_FS,
	&MODE_1920x540p50__PANEL_FS,
	&MODE_920x1080p60__PANEL_FS,
	&MODE_920x1080p50__PANEL_FS,
	&MODE_1920x1080p30__PANEL_FS,
	&MODE_1920x1080p25__PANEL_FS,
	&MODE_1920x1080p24__PANEL_FS,
};

/* This table contains list of audio timings supported by Intel CE Media
 * Processors and used in the situations when EDID is not available
 *
 * Note: Do *NOT* add declaration WMA in here as we dont have approval for that
 */
static otm_hdmi_audio_cap_t static_audio_modes[] = {
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_PCM, 8, ALL_SF, ALL_SS),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_AC3, 8, ALL_SF, 640 / 8),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DTS, 8, ALL_SF, 1536 / 8),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DDP, 8, ALL_SF, 0),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DTSHD, 8, ALL_SF, 0),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_MLP, 8, ALL_SF, 0),
};

static otm_hdmi_ret_t __program_clocks(hdmi_context_t *ctx, unsigned int dclk)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	return rc;
}

/**
 * This function called by edid_print tool internally
 * @ctx		: hdmi context handle
 * @edid		: edid information
 *
 * Returns nothing. Called by edid_print tool to print
 * edid information to dmesg for debugging purposes
 */
static void __hdmi_report_edid(hdmi_context_t *ctx, edid_info_t *edid)
{
	int i = 0;

	LOG_PRINT(LOG_LEVEL_HIGH, "----------------------\n");
	LOG_PRINT(LOG_LEVEL_HIGH, "Name     : %s\n", edid->product_name);
	LOG_PRINT(LOG_LEVEL_HIGH, "Year     : %d\n", edid->product_year);
	LOG_PRINT(LOG_LEVEL_HIGH, "SN       : %d\n", edid->product_sn);
	LOG_PRINT(LOG_LEVEL_HIGH, "Type     : %s\n",
			edid->hdmi ? "HDMI" : "DVI");
	LOG_PRINT(LOG_LEVEL_HIGH, "YCbCr444 : %s\n",
			edid->ycbcr444 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "YCbCr422 : %s\n",
			edid->ycbcr422 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "30 bpp   : %s\n",
			edid->dc_30 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "36 bpp   : %s\n",
			edid->dc_36 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "48 bpp   : %s\n",
			edid->dc_48 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "DC_YUV   : %s\n",
			edid->dc_y444 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "Max CLK  : %d\n",
			edid->max_tmds_clock);
	LOG_PRINT(LOG_LEVEL_HIGH, "Lip sync : %s\n",
			edid->latency_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "ILip sync: %s\n",
			edid->latency_int_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "Vid lat  : %d\n",
			edid->latency_video);
	LOG_PRINT(LOG_LEVEL_HIGH, "Aud lat  : %d\n",
			edid->latency_audio);
	LOG_PRINT(LOG_LEVEL_HIGH, "IVid lat : %d\n",
			edid->latency_video_interlaced);
	LOG_PRINT(LOG_LEVEL_HIGH, "IAud lat : %d\n",
			edid->latency_audio_interlaced);
	LOG_PRINT(LOG_LEVEL_HIGH, "HDMI VID : %s\n",
			edid->hdmi_video_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "HDMI 3D  : %s\n",
			edid->enabled_3d ? "Y" : "N");

	LOG_PRINT(LOG_LEVEL_HIGH, "SPA      : %d.%d.%d.%d\n",
		  (edid->spa & 0xF000) >> 12,
		  (edid->spa & 0x0F00) >> 8,
		  (edid->spa & 0x00F0) >> 4, (edid->spa & 0x000F) >> 0);

	LOG_PRINT(LOG_LEVEL_HIGH, "Supported timings [%d]:\n",
		  edid->num_timings);

	for (i = 0; i < edid->num_timings; i++)
		print_pd_timing(&edid->timings[i], edid->order[i]);

	LOG_PRINT(LOG_LEVEL_HIGH, "Audio capabilities:\n");
	for (i = 0; i < edid->num_caps; i++)
		print_audio_capability(&edid->audio_caps[i]);

	print_speaker_layout(edid->speaker_map);
	LOG_PRINT(LOG_LEVEL_HIGH, "----------------------\n");
}

/**
 * This function overrides the edid information with static timings
 * @ctx		: hdmi context handle
 * @edid		: edid information
 * @safe		: boolean for edid option
 *
 * Returns OTM_HDMI_SUCCESS or OTM_HDMI_ERR_INTERNAL
 *
 * This function overrides the edid information with static timings
 */
static otm_hdmi_ret_t __hdmi_edid_override(hdmi_context_t *ctx,
				edid_info_t *edid,
				bool safe)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	unsigned int i, n = 0;
	bool hdmi;
	bool dc_30, dc_36;
	const otm_hdmi_timing_t **modes = NULL;
	bool hdcp = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]);
	unsigned int n_modes_dtv = NUM_ENTRIES_IN(static_dtv_modes);

	/* Verify pointers */
	if (!(edid)) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	/* Save device type and DC caps */
	hdmi = !ctx->dtv && (safe ? edid->hdmi : true);
	dc_30 = hdmi && edid->dc_30;
	dc_36 = hdmi && edid->dc_36;

	/* Clear EDID */
	memset(edid, 0, sizeof(edid_info_t));

	/* Set device type */
	edid->hdmi = hdmi;

	/* Pick caps table based on whether we are HDMI TX or DTV */
	modes = ctx->dtv ? static_dtv_modes : g_video_modes;
	n = ctx->dtv ? n_modes_dtv : ctx->n_modes_tx;

	/* Add all supported video modes */
	for (i = edid->num_timings = 0; i < n; i++) {
		edid->timings[edid->num_timings++] = *modes[i];

		/* Do NOT advertise 3D modes in DVI mode unless we are in DTV
		 * mode which means always use DTV static table
		 */
		if (!ctx->dtv && !hdmi &&
			modes[i]->stereo_type != OTM_HDMI_STEREO_NONE) {
			edid->num_timings--;
		}
	}

	/* Set HDCP based on DTV indicator */
	PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]) =
				ctx->dtv ? false : hdcp;

	/* Dont bother with HDMI caps if we are in DVI mode */
	if (!(hdmi))
		goto exit;

	/* Add all supported audio modes */
	edid->num_caps = NUM_ENTRIES_IN(static_audio_modes);
	for (i = 0; i < edid->num_caps; i++)
		edid->audio_caps[i] = static_audio_modes[i];

	/* Enable all possible speaker allocation maps */
	edid->speaker_map |= 0x3ff;

	/* Indicate support of deep color and YCbCr output */
	edid->ycbcr444 = true;
	edid->ycbcr422 = true;
	edid->dc_30 = safe ? dc_30 : true;
	edid->dc_36 = safe ? dc_36 : true;

exit:
	return rc;
}

/**
 * otm_hdmi_edid_parse() - fill capability table
 * @context:      hdmi context
 * @use_edid: True or False
 *
 * Returns - check otm_hdmi_ret_t
 *
 * This routine files capability table.
 */
otm_hdmi_ret_t otm_hdmi_edid_parse(void *context, otm_hdmi_use_edid_t use_edid)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	i2c_read_t edid_foo = ps_hdmi_i2c_edid_read;
	bool cable = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_CABLE_STATUS]);
	edid_info_t *edid;
	unsigned int i;
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	/* Verify pointers */
	if (!ctx) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	/* Init locals */
	edid = &ctx->edid_int;

	/* Begin EDID update protection */
	mutex_lock(&ctx->modes_sema);

	/* Clear EDID */
	memset(edid, 0, sizeof(edid_info_t));

	/* Setup reference table for parsing */
	edid->num_ref_timings = ctx->n_modes_ref;
	edid->ref_timings = g_video_modes_ref;

	/* DTV mode will use static DTV timings directly */
	if (ctx->dtv)
		goto edid_override;

	switch (use_edid) {
	case OTM_HDMI_USE_EDID_REAL:
		/* Try reading EDID. If reading failed pick overriding strategy
		 * based on cable status
		*/
		rc = edid_parse(edid, edid_foo, ctx);
		if (rc != OTM_HDMI_SUCCESS) {
			pr_debug("Failed to read EDID info\n");
			use_edid = cable ? OTM_HDMI_USE_EDID_SAFE :
				OTM_HDMI_USE_EDID_NONE;
		}
		break;
	case OTM_HDMI_USE_EDID_SAFE:
		/* In safe mode we still need real EDID */
		edid_parse(edid, edid_foo, ctx);
		break;
	case OTM_HDMI_USE_EDID_NONE:
		/* In full override mode we dont care of real EDID
		 * so do nothing
		 */
		break;
	default:
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	/* Dont bother with static timings if we are using real EDID */
	if (use_edid == OTM_HDMI_USE_EDID_REAL)
		goto twin_caps;

edid_override:
	/* Use static timings */
	__hdmi_edid_override(ctx, edid, use_edid == OTM_HDMI_USE_EDID_SAFE);

	/* Insertion of twin entries below is done right in the parsed table of
	 * modes without knowledge of its maximum size. Be extra careful about
	 * it and check that MAX_TIMINGS is big enough; This needs to be fixed
	 * in long run
	 */
twin_caps:
	/* Insert 59.94 entries */
	hdmi_timing_add_twin_entries(edid, OTM_HDMI_REFRESH_60,
					OTM_HDMI_REFRESH_59_94);

	/* Insert 29.97 entries */
	hdmi_timing_add_twin_entries(edid, OTM_HDMI_REFRESH_30,
					OTM_HDMI_REFRESH_29_97);

	/* Insert 23.98 entries */
	hdmi_timing_add_twin_entries(edid, OTM_HDMI_REFRESH_24,
					OTM_HDMI_REFRESH_23_98);

	/* Insert 47.96 entries */
	hdmi_timing_add_twin_entries(edid, OTM_HDMI_REFRESH_48,
					OTM_HDMI_REFRESH_47_96);

	/* Adjust received timings */
	for (i = 0; i < edid->num_timings; i++)
		hdmi_timing_edid_to_vdc(&edid->timings[i]);

	/* Print warning message in case there are no timings */
	if (ctx->edid_int.num_timings == 0) {
		LOG_ERROR
		    ("----------------- WARNING -----------------------\n");
		LOG_ERROR
		    ("-- TV timings are not available		--\n");
		LOG_ERROR
		    ("-- To resolve this switch to static TV timings --\n");
	}
	/* Update EDID availability indicator */
	PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_USE_EDID]) = use_edid;

	/* End EDID update protection */
	mutex_unlock(&ctx->modes_sema);

exit:
	return rc;
}

/**
 * otm_hdmi_timing_from_cea_modes() - get timings for cea modes
 * @buffer: the extension block buffer
 * @timings: the result CEA timings extacted from the buffer
 *
 * Returns - the number of modes in the timings
 */
int otm_hdmi_timing_from_cea_modes(unsigned char *buffer,
				   otm_hdmi_timing_t *timings)
{
	edid_info_t *edid  = NULL;

	if (buffer == NULL)
		return 0;

	if (timings == NULL)
		return 0;

	if (g_context == NULL)
		return 0;

	edid = &g_context->edid_int;
	if (edid == NULL)
		return 0;

	edid->num_ref_timings = g_context->n_modes_ref;
	edid->ref_timings = g_video_modes_ref;

	return edid_parse_pd_timing_from_cea_block(edid, buffer, timings);
}

/**
 * otm_hdmi_get_mode_timings() - get timings of a mode, given:
 * @context : HDMI context
 * @hdisplay: mode width
 * @vdisplay: mode height
 * @vrefresh: mode refresh rate
 *
 * Returns matching mode, NULL otherwise.
 */
const otm_hdmi_timing_t *otm_hdmi_get_mode_timings(void *context,
						int hdisplay,
						int vdisplay,
						int vrefresh)
{
	const otm_hdmi_timing_t *mode = NULL;
	int i, refresh_rate;

	if (hdisplay < 0 || vdisplay < 0 || vrefresh < 0)
		goto exit;

	for (i = 0; i < MAX_TIMINGS; i++) {
		mode = g_video_modes[i];
		refresh_rate = ((mode->dclk * 1000) /
					(mode->htotal * mode->vtotal));
		if (hdisplay == mode->width &&
			vdisplay == mode->height &&
			vrefresh == refresh_rate)
			return mode;
	}
exit:
	return NULL;
}

/**
 * This function fills the given table with timings
 * @unit_id	: hdmi unit revision id
 * @table	: handle to table to be filled
 * @max_size	: max number of entries in the table
 * @reference	: is this table reference table?
 *
 * This routine fills given table with timings according to current unit version
 * and subsequent use of table
 */
static int __init_tx_modes(hdmi_unit_revision_id_t unit_id,
			   const otm_hdmi_timing_t **table,
			   unsigned int max_size, bool reference)
{
	int i = 0;

#define __ADD_MODE(mode)    \
	do { \
		if (i < max_size) \
			table[i++] = mode;  \
		else {		       \
		    i = -1;	     \
		    goto exit;	  \
		}	\
	} while (0);

	/* The following 2D modes are supported on all unit revisions */
	__ADD_MODE(&MODE_640x480p5994_60);
	__ADD_MODE(&MODE_720_1440x576i50);
	__ADD_MODE(&MODE_720_1440x480i5994_60);
	__ADD_MODE(&MODE_720x576p50);
	__ADD_MODE(&MODE_720x480p5994_60);
	__ADD_MODE(&MODE_1280x720p50);
	__ADD_MODE(&MODE_1280x720p5994_60);
	__ADD_MODE(&MODE_1920x1080i50);
	__ADD_MODE(&MODE_1920x1080i5994_60);
	__ADD_MODE(&MODE_1920x1080p24);
	__ADD_MODE(&MODE_1920x1080p25);
	__ADD_MODE(&MODE_1920x1080p30);
	__ADD_MODE(&MODE_1920x1080p50);
	__ADD_MODE(&MODE_1920x1080p5994_60);

	/* The following 3D modes are supported on all unit revisions */
	__ADD_MODE(&MODE_1280x720p50__SBSH2);
	__ADD_MODE(&MODE_1280x720p5994_60__SBSH2);
	__ADD_MODE(&MODE_1920x1080i50__SBSH2);
	__ADD_MODE(&MODE_1920x1080i5994_60__SBSH2);
	__ADD_MODE(&MODE_1920x1080p24__SBSH2);
	__ADD_MODE(&MODE_1920x1080p50__SBSH2);
	__ADD_MODE(&MODE_1920x1080p5994_60__SBSH2);
	__ADD_MODE(&MODE_1280x720p50__TBH2);
	__ADD_MODE(&MODE_1280x720p5994_60__TBH2);
	__ADD_MODE(&MODE_1920x1080p24__TBH2);
	__ADD_MODE(&MODE_1920x1080p30__TBH2);
	__ADD_MODE(&MODE_1920x1080p50__TBH2);
	__ADD_MODE(&MODE_1920x1080p5994_60__TBH2);
	__ADD_MODE(&MODE_1280x720p50__FP2);
	__ADD_MODE(&MODE_1920x1080p30__FP2);

	/* The following modes are only included if the table is used as a
	 * reference set for EDID parsing
	 */
	if (reference) {
		__ADD_MODE(&MODE_720_1440x576i50__16by9);
		__ADD_MODE(&MODE_720_1440x480i5994_60__16by9);
		__ADD_MODE(&MODE_720x576p50__16by9);
		__ADD_MODE(&MODE_720x480p5994_60__16by9);
	}
	/* The following mode are supported only on CE4200 B0 and further */
	if (unit_id >= HDMI_PCI_REV_CE4200_B0) {
		__ADD_MODE(&MODE_1280x720p50__FP);
		__ADD_MODE(&MODE_1280x720p5994_60__FP);
		__ADD_MODE(&MODE_1920x1080i50__FP);
		__ADD_MODE(&MODE_1920x1080i5994_60__FP);
		__ADD_MODE(&MODE_1920x1080p24__FP);
		__ADD_MODE(&MODE_1920x1080p30__FP);
	}
#undef __ADD_MODE

exit:
	return i;
}

/**
 * otm_hdmi_phy_enable() - PHY power programming wrapper
 * @context:    hdmi context
 * @status: status
 *
 * Returns - check otm_hdmi_ret_t
 *
 * This routine is PHY power programming wrapper
 */
#ifdef OTM_HDMI_FIXME
static otm_hdmi_ret_t __hdmi_phy_enable(void *context, bool status)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	bool cable = PD_ATTR_BOOL(ATTRS
				[OTM_HDMI_ATTR_ID_CABLE_STATUS]);
#ifdef SOC_HDCP_ENABLE
	bool mute = PD_ATTR_BOOL(ATTRS
				[OTM_HDMI_ATTR_ID_HDCP_AUTO_MUTE]);
#endif
	/* Safety checks */
	if (ctx == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}

	/* Override given setting based on current mode, cable status and
	 * hot plug processing state
	 * TODO - Fix timerisset
	 */
	status = status && ctx->mode_set
	    && cable /* && !timerisset(&ctx->phy_time) */ ;

	/* It's gonna take a while for HDCP to re-authenticate after phy
	 * enabling hence mute output to prevent premium content going out
	 * unencrypted
	 */
#ifdef SOC_HDCP_ENABLE
	if (PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]) && status && mute)
		hdmi_mute(ctx, OTM_HDMI_MUTE_VIDEO, MUTE_SOURCE_HDCP);
#endif

	pr_debug("About to call hdmi_phy_enable\n");
	/* Program HW and update status indicator */

	rc = hdmi_phy_enable(&ctx->dev, status);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;
	ctx->phy_status = status;

	/* Debug print */
	LOG_PRINT(LOG_LEVEL_HIGH, "PHY -> %s\n", status ? "on" : "off");

exit:
	return rc;
}

/**
 * __hdmi_enable - enable HDMI HW device
 * @ctx:    hdmi context
 *
 * Returns - check otm_hdmi_ret_t
 *
 * Enable hdmi HW device
 */
static otm_hdmi_ret_t __hdmi_enable(hdmi_context_t *ctx)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	/* Check context */
	if (ctx == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}

	/* Disable PHY */
	rc = __hdmi_phy_enable(ctx, false);
	pr_debug("phy_enable returned with %d\n", rc);

	/* Setup and enable I2C subunit */
	rc = hdmi_i2c_reset_init_enable(ctx);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;

	/* Activate interrupts of interest */
	hdmi_interrupts_set_mask(&ctx->dev, HDMI_INTERRUPTS);

	/* Bring device to a known state */
	hdmi_general_pixel_clock_enable(&ctx->dev);
	hdmi_general_tdms_clock_enable(&ctx->dev);
	hdmi_general_audio_clock_enable(&ctx->dev);
	hdmi_general_hdcp_clock_enable(&ctx->dev);
	hdmi_general_5V_enable(&ctx->dev);

#ifdef SOC_HDCP_ENABLE
	hdmi_hdcp_disable(ctx);
#endif

exit:
	return rc;
}
#endif /*OTM_HDMI_FIXME*/

static void log_entry(void *uhandle, char *foo)
{
#ifdef __HDMI_HAL_TRACE__
	PD_PRINT("%s: Entering %s\n", PD_NAME, foo);
#endif
}

static void log_exit(void *uhandle, char *foo, int rc)
{
#ifdef __HDMI_HAL_TRACE__
	PD_PRINT("%s: Exiting %s with %d\n", PD_NAME, foo, rc);
#endif
}

/* Microseconds domain timer initialization */
static void __poll_start(void *poll_timer)
{
	do_gettimeofday(poll_timer);
}

/* Microseconds domain timeout verification */
static bool __poll_timeout(void *poll_timer)
{
	struct timeval tv_stop;
	do_gettimeofday(&tv_stop);
	return TIME_DIFF(tv_stop, *((struct timeval *) poll_timer)) >
				I2C_SW_TIMEOUT;
}

/**
 * This function initializes hdmi_context
 * @context	: opaque hdmi_context
 * @pdev		: pci_device
 *
 * Returns check otm_hdmi_ret_t
 * Initializes hdmi_context
 */
static otm_hdmi_ret_t __hdmi_context_init(void *context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;
	unsigned int isr_address;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	rc = ps_hdmi_pci_dev_init(ctx, pdev);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;

#ifndef OTM_HDMI_FIXME
/* FIXME: this should come from IPIL */
#define	HDMI_INTERRUPT_STATUS 0x100C
#endif
	/* Register for interrupt routine invocation at given interrupt */
	isr_address = (unsigned int)ctx->io_address + HDMI_INTERRUPT_STATUS;

	pr_debug("About to call initialize HAL members and io_address "
			"is 0x%x\n",
			(unsigned int)ctx->io_address);

	/* Initialize HAL; It's important that ALL entries are initialized!!! */
	ctx->dev.log_entry = log_entry;
	ctx->dev.log_exit = log_exit;
	ctx->dev.poll_timer = &ctx->hal_timer;
	ctx->dev.poll_start = __poll_start;
	ctx->dev.poll_timeout = __poll_timeout;
	ctx->dev.io_address = (unsigned int)ctx->io_address;
	ctx->dev.io_address = (unsigned int)ctx->io_address;
#ifdef OTM_HDMI_FIXME
	ctx->dev.io_read = ior;
	ctx->dev.io_write = iow;
#endif

	ctx->dev.uhandle = ctx->io_address;
	/* Create modes table sharing protection semaphore */
	mutex_init (&ctx->modes_sema);

	/* Create execution protection semaphore */
	mutex_init (&ctx->exec_sema);

	/* Create HPD protection semaphore */
	mutex_init (&ctx->hpd_sema);

	/* Create server thread synchronization semaphore */
	mutex_init (&ctx->srv_sema);

	/* Create server thread synchronization semaphore */
	mutex_init (&ctx->i2c_sema);

	/* Create AV mute synchronization semaphore */
	mutex_init (&ctx->mute_sema);

exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * otm_hdmi_deinit - deinit called during shutdown
 * @context	:opaque hdmi_context
 *
 * Returns nothing. de-initializes and frees pointers
 * Called during power down
 */
void otm_hdmi_deinit(void *context)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = context;

#ifdef OTM_HDMI_FIXME
	/* Disable interrupts and unregister interrupts handling routine */
	hdmi_interrupts_set_mask(&ctx->dev, 0);
	free_irq(ctx->irq_number, ctx);
#endif
	/* Destroy semaphores */
	mutex_destroy(&ctx->modes_sema);
	mutex_destroy(&ctx->exec_sema);
	mutex_destroy(&ctx->hpd_sema);
	mutex_destroy(&ctx->srv_sema);
	mutex_destroy(&ctx->i2c_sema);
	mutex_destroy(&ctx->mute_sema);

	/* Bring device to a known state */
#ifdef OTM_HDMI_FIXME
#ifdef SOC_HDMI_VIDEO_ENABLE
	hdmi_video_set_pixel_source(&ctx->dev, false);
#endif
#ifdef SOC_HDMI_INFOFRAMES_ENABLE
	hdmi_infoframe_disable_all(&ctx->dev);
#endif
#endif
	ipil_hdmi_general_unit_disable(&ctx->dev);
#ifdef OTM_HDMI_FIXME
	/* Disable PHY */
	rc = __hdmi_phy_enable(ctx, false);
	pr_debug("__hdmi_phy_enable returned with %d\n", rc);
#endif
	ipil_hdmi_general_hdcp_clock_disable(&ctx->dev);
	ipil_hdmi_general_5V_disable(&ctx->dev);
	ipil_hdmi_general_audio_clock_disable(&ctx->dev);
	ipil_hdmi_general_pixel_clock_disable(&ctx->dev);
	ipil_hdmi_general_tdms_clock_disable(&ctx->dev);
	ipil_hdmi_i2c_disable(&ctx->dev);

	/* Clearing audio information */
	ipil_hdmi_audio_deinit(ctx);

	/* Unmap IO region, Disable the PCI devices
	 */
	rc = ps_hdmi_pci_dev_deinit(ctx);

	/* Free context */
	kfree(ctx);

	pr_debug("Exiting deinit with error code %d\n", rc);
exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return;
}

#ifndef OTM_HDMI_FIXME
otm_hdmi_ret_t hdmi_phy_enable(hdmi_device_t *dev, bool status)
{
	return OTM_HDMI_SUCCESS;
}

otm_hdmi_ret_t hdmi_timing_add_twin_entries(edid_info_t *edid,
					    otm_hdmi_refresh_t src,
					    otm_hdmi_refresh_t dst)
{
	return OTM_HDMI_SUCCESS;
}

void hdmi_timing_edid_to_vdc(otm_hdmi_timing_t *t)
{
	return;
}
#endif

/* turn HDMI power rails on */
bool otm_hdmi_power_rails_on(void)
{
	return ipil_hdmi_power_rails_on();
}

/**
 * get pixel clock range
 * @pc_min	: min pixel clock
 * @pc_max	: max pixel clock
 *
 * Returns check otm_hdmi_ret_t
 * This functions returns the minimum and maximum
 * pixel clock values
 */
otm_hdmi_ret_t otm_hdmi_get_pixel_clock_range(unsigned int *pc_min,
						unsigned int *pc_max)
{
	if (!pc_min || !pc_max)
		return OTM_HDMI_ERR_FAILED;

	*pc_min = IPIL_MIN_PIXEL_CLOCK;
	*pc_max = IPIL_MAX_PIXEL_CLOCK;
	return OTM_HDMI_SUCCESS;
}

/**
 * hdmi interrupt handler (upper half).
 * @irq:	irq number
 * @data:	data for the interrupt handler
 *
 * Returns:	IRQ_HANDLED on NULL input arguments, and if the
 *			interrupt is not HDMI HPD interrupts.
 *		IRQ_WAKE_THREAD if this is a HDMI HPD interrupt.
 * hdmi interrupt handler (upper half). handles the interrupts
 * by reading hdmi status register and waking up bottom half if needed.
 */
static irqreturn_t __hdmi_irq_handler(int irq, void *data)
{
	if (g_context != NULL)
		return ipil_hdmi_irq_handler(&g_context->dev);

	return IRQ_HANDLED;
}

/**
 * otm_hdmi_setup_irq	-       install HPD IRQ call back
 * @context: hdmi device context
 * @pdev: pci device
 * @phdmi_irq_cb: function pointer for hotplug/unplug IRQ callbacks.
 * @data: data for irq callback
 *
 * Perform HPD IRQ call back initialization
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_setup_irq(void *context, struct pci_dev *pdev,
				irqreturn_t (*phdmi_irq_cb) (int, void*),
				void *data)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;
	int ret;

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	/* Setup interrupt handler */
	if (phdmi_irq_cb != NULL && data != NULL) {
		ret = request_threaded_irq(ctx->irq_number,
				__hdmi_irq_handler, phdmi_irq_cb,
				 IRQF_SHARED,OTM_HDMI_NAME, (void *)data);
		if (ret) {
			pr_debug("\nregister irq interrupt failed\n");
			rc = OTM_HDMI_ERR_INTERNAL;
			/*
			* TODO: ignore failure as PCI is not working on DV1.
			*/
			goto exit;
		}
	}
exit:
	return rc;

}

/**
 * otm_hdmi_device_init	-	init hdmi device driver
 * @context: hdmi device context
 * @pdev: pci device
 *
 * Perform HDMI device initialization which includes 3 steps:
 * 1) otm context create,
 * 2) os specific context init,
 * 3) device enable
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_device_init(void **context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;
	int n;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}

	/* Create and clear context */
	g_context = ctx =
	    (hdmi_context_t *) kmalloc(sizeof(hdmi_context_t), GFP_KERNEL);
	if (ctx == NULL) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	memset(ctx, 0, sizeof(hdmi_context_t));

	pr_debug("HDMI Context created = 0x%08x\n", (unsigned int)ctx);

	/* Init HDMI context */
	rc = __hdmi_context_init(ctx, pdev);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nHDMI Context init failed\n");
		goto exit;
	}

	rc = otm_hdmi_declare_attributes(__pd_attr_declare, __pd_attr_get_name);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nHDMI declare attributes table failed\n");
		goto exit;
	}

	ipil_hdmi_set_hdmi_dev(&ctx->dev);

	/* Decide on I2C HW acceleration */
	ipil_hdmi_decide_I2C_HW(ctx);

	/* Save the output mode as DTV or HDMT tx */
	ctx->dtv = g_dtv;

	/* Save the deep color enable flag */
	ctx->dc = g_dc;

	/* Explicitly disable 5V before enabling it later as transition from
	 * off to on seems to be responsible to hot plug generation during
	 * starup */
	ipil_hdmi_general_5V_disable(&ctx->dev);

	/* Save active GPIO number */
	ctx->gpio = g_gpio;

	/* Save context */
	*context = ctx;

	/* Setup all external clocks */
	rc = ipil_hdmi_set_program_clocks(ctx, 27000);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;

	/* Fill in static timing table */
	n = __init_tx_modes(ctx->dev.id, g_video_modes, MAX_TIMINGS,
				false);
	if (n < 0) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	ctx->n_modes_tx = n;

	/* Fill EDID parser reference timing table */
	n = __init_tx_modes(ctx->dev.id, g_video_modes_ref, MAX_TIMINGS,
			    true);
	if (n < 0) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	ctx->n_modes_ref = n;

	/* Fill in advertised timings table */
	otm_hdmi_edid_parse(ctx, OTM_HDMI_USE_EDID_NONE);

#ifdef OTM_HDMI_FIXME
	/* Enable HDMI unit */
	rc = __hdmi_enable(ctx);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;
#endif
	rc = ipil_hdmi_audio_init(ctx);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;

exit:
	/* Clean up if appropriate */
	if ((rc != OTM_HDMI_SUCCESS) && (ctx != NULL))
		otm_hdmi_deinit((void *)ctx);

	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * Returns if the given values is preferred mode or not
 * @hdisplay	: width
 * @vdisplay	: height
 * @refresh	: refresh rate
 *
 * Returns true if preferred mode else false
 */
bool otm_hdmi_is_preferred_mode(int hdisplay, int vdisplay, int refresh)
{
	if (hdisplay == IPIL_PREFERRED_HDISPLAY &&
	    vdisplay == IPIL_PREFERRED_VDISPLAY &&
	    refresh == IPIL_PREFERRED_REFRESH_RATE)
		return true;
	else
		return false;
}

/**
 * Set raw edid to the hdmi context
 * @context	: opaque hdmi_context
 * @raw_edid	: raw edid information
 *
 * Returns - check otm_hdmi_ret_t
 * Copy raw edid to the hdmi context
 */
otm_hdmi_ret_t otm_hdmi_set_raw_edid(void *context, char *raw_edid)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL)
		return OTM_HDMI_ERR_FAILED;

	/* TODO: need more flexiable way which should be edid size-aware copy */
	memcpy(ctx->edid_raw, raw_edid, MAX_EDID_BLOCKS * SEGMENT_SIZE);

	return OTM_HDMI_SUCCESS;
}

/**
 * Get raw edid to the hdmi context
 * @context	: opaque hdmi_context
 * @raw_edid	: raw edid information
 *
 * Returns - check otm_hdmi_ret_t
 * Retrieves raw edid in the hdmi context
 */
otm_hdmi_ret_t otm_hdmi_get_raw_edid(void *context, char **raw_edid)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL || raw_edid == NULL)
		return OTM_HDMI_ERR_FAILED;

	*raw_edid = (char *)ctx->edid_raw;

	return OTM_HDMI_SUCCESS;
}

/**
 * Check if monitor connected is hdmi
 * @context	: opaque hdmi_context
 *
 * Returns true if hdmi else false
 * Check if monitor connected is hdmi
 */
bool otm_hdmi_is_monitor_hdmi(void *context)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL)
		return true; /* default to HDMI */

	return ctx->edid_int.hdmi;
}

/**
 * HDMI video mute handling
 * @ctx		: hdmi_context
 * @type	: mute
 * @source	: mute source
 *
 * Returns true if hdmi else false
 * HDMI video mute handling
 */
static void hdmi_mute(hdmi_context_t *ctx,
			otm_hdmi_mute_t type,
			mute_source_t source)
{
#ifdef OTM_HDMI_FIXME
	unsigned int color = PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_BG_COLOR]);
	hdmi_video_set_pixel_color(&ctx->dev, color);
#endif
	mutex_lock(&ctx->mute_sema);
	if (type & OTM_HDMI_MUTE_VIDEO) {
		/*
		 * Update mute list and mute video
		 */
		ctx->mute_source |= source;
#ifdef OTM_HDMI_FIXME
		hdmi_video_set_pixel_source(&ctx->dev, false);
#endif
	} else {
		/*
		 * Update mute list and unmute video [if possible]
		 */
		ctx->mute_source &= ~source;
#ifdef OTM_HDMI_FIXME
		hdmi_video_set_pixel_source(&ctx->dev, ctx->mute_source == 0);
#endif
	}
	mutex_unlock(&ctx->mute_sema);
}

/**
 * Checks the output pixel depth
 * @depth	: pixel depth
 * @depth_30	: ycbcr422 boolean
 * @depth_36	: ycbcr444 boolean
 *
 * Returns check otm_hdmi_ret_t. Checks the output pixel depth
 */
static otm_hdmi_ret_t __check_opd_support(otm_hdmi_output_pixel_depth_t depth,
							bool depth_30,
							bool depth_36)
{
	otm_hdmi_ret_t rc = OTM_HDMI_ERR_INTERNAL;

	switch (depth) {
	case OTM_HDMI_OPD_24BIT:
		rc = OTM_HDMI_SUCCESS;
		break;
	case OTM_HDMI_OPD_30BIT:
		rc = depth_30 ? OTM_HDMI_SUCCESS : rc;
		break;
	case OTM_HDMI_OPD_36BIT:
		rc = depth_36 ? OTM_HDMI_SUCCESS : rc;
		break;
	default:
		rc = OTM_HDMI_ERR_INTERNAL;
		break;
	}

	return rc;
}

/**
 * Checks the pixel depth configuration
 * @opf		: output pixel format
 * @opd		: output pixel depth
 *
 * Returns check otm_hdmi_ret_t. Checks the pixel depth config
 */
static otm_hdmi_ret_t __check_pixel_depth_cfg(
				otm_hdmi_output_pixel_format_t opf,
				otm_hdmi_output_pixel_depth_t opd)
{
	bool rc = (opf != OTM_HDMI_OPF_YUV422)
	    || (opd == OTM_HDMI_OPD_24BIT);
	return rc ? OTM_HDMI_SUCCESS : OTM_HDMI_ERR_INTERNAL;
}

/**
 * Checks the output pixel format
 * @format	: output pixel format
 * @yuv444	: is pixel format yuv444
 * @yuv422	: is pixel format yuv422
 *
 * Returns check otm_hdmi_ret_t. Checks the output pixel format
 */
static otm_hdmi_ret_t __check_opf_support(otm_hdmi_output_pixel_format_t format,
						bool yuv444,
						bool yuv422)
{
	otm_hdmi_ret_t rc = OTM_HDMI_ERR_INTERNAL;
	bool ext_color =
	    PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT]);

	switch (format) {
	case OTM_HDMI_OPF_YUV444:
		rc = yuv444 ? OTM_HDMI_SUCCESS : rc;
		break;
	case OTM_HDMI_OPF_YUV422:
		rc = yuv422 ? OTM_HDMI_SUCCESS : rc;
		break;
	case OTM_HDMI_OPF_RGB444:
		rc = (!ext_color) ? OTM_HDMI_SUCCESS : rc;
		break;
	default:
		rc = OTM_HDMI_ERR_INTERNAL;
		break;
	}

	return rc;
}

/**
 * set color space conversion
 * @ctx	: hdmi_context
 *
 * Returns check otm_hdmi_ret_t. Sets the color space
 * conversion attribute
 */
static otm_hdmi_ret_t __set_attr_csc(hdmi_context_t *ctx)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int in, out;

	if (NULL == ctx) {
		LOG_ERROR("Invalid argument passed (ctx)\n");
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	/*
	 * Set CSC appropriately
	 */
	out = PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]);
	in = PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_COLOR_SPACE_INPUT]);
#ifdef OTM_HDMI_FIXME
	rc = hdmi_configure_csc(&ctx->dev, in, out, &ctx->mode);
#endif

exit:
	return rc;
}

/**
 * validates the attribute to be written or not.
 * @id	: attribute id
 * @value  : value to be set to the attribute
 *
 * Write's the attributes value.
 * validates the attribute to be written or not.
 *
 * Returns -
 *	OTM_HDMI_SUCCESS - if the attribute value is writable.
 *	OTM_HDMI_ERR_INTERNAL - if the attribute value is non-writable.
 *	OTM_HDMI_ERR_FAILED - if the attribute is not in range.
 */
static otm_hdmi_ret_t otm_hdmi_attr_set_validate(otm_hdmi_attribute_id_t id,
						void *value)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	bool _bool;
	unsigned int _uint;
	char *_string = NULL;
	int str_len = 0;

	if (id < 0 || id >= OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES) {
		LOG_ERROR("Invalid argument passed (id): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	if (NULL == value) {
		LOG_ERROR("Invalid argument passed (value): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	if ((OTM_HDMI_ATTR_FLAG_WRITE & ATTRS[id].flags) != 1) {
		LOG_ERROR("Attribute id: %d is read-only\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	/*
	 * Based on attribute type perform appropriate check
	 */
	switch (ATTRS[id].type) {
	case OTM_HDMI_ATTR_TYPE_BOOLEAN:
		_bool = *(bool *) value;
		if ((_bool != true) && (_bool != false))
			rc = OTM_HDMI_ERR_FAILED;
		break;

	case OTM_HDMI_ATTR_TYPE_UINT:
		_uint = *(unsigned int *)value;
		if ((_uint < ATTRS[id].content._uint.value_min)
		    || (_uint > ATTRS[id].content._uint.value_max)) {
			rc = OTM_HDMI_ERR_FAILED;
		}
		break;

	case OTM_HDMI_ATTR_TYPE_STRING:
		_string = (char *) value;
		str_len = strlen(_string);
		if (str_len < 1 || str_len > OTM_HDMI_MAX_STRING_LENGTH) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
		break;
	default:
		LOG_ERROR("Invalid attribute id (%d)\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		break;
	}

exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/*
 * Setting given attribute
 * @param [in] context	: port driver specific information
 * @param [in] id       : attribute id
 * @param [in] data     : user provided buffer with attribute value
 * @param [in] internal : internal [driver] or external [app] call
 *
 * Setting given attribute
 * Note: Some attributes settings can not be applied until mode change was done.
 * Hence such attributes are set logically and the actual HW will be set during
 * mode change.
 */
otm_hdmi_ret_t otm_hdmi_set_attribute(void *context,
					otm_hdmi_attribute_id_t id,
					void *data,
					bool internal)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *) context;
#ifdef OTM_HDMI_FIXME
	gdl_display_id_t pipe =
	    PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_DISPLAY_PIPE]);
	otm_hdmi_ret_t rc1, rc2;
	bool hdcp_ctrl = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]);
	otm_hdmi_hdcp_status_t hdcp_status =
		PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_HDCP_STATUS]);
#endif
	bool pwr = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_POWER]);
	edid_info_t *edid;
	bool abool;
	unsigned int auint, out, depth;
	otm_hdmi_attribute_flag_t flags;
	LOG_ENTRY(LOG_LEVEL_HIGH);

	if (NULL == ctx) {
		LOG_ERROR("Invalid argument passed (context): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	flags =
	    OTM_HDMI_ATTR_FLAG_WRITE |
		(internal ? OTM_HDMI_ATTR_FLAG_INTERNAL : 0);

	rc = otm_hdmi_attr_set_validate(id, data);
	if (OTM_HDMI_SUCCESS != rc)
		goto exit;
	abool = *(bool *) (data);
	auint = *(unsigned int *) (data);
	LOG_PRINT(LOG_LEVEL_HIGH, "ATTR[%d] -> %d\n", id, auint);

#ifdef OTM_HDMI_FIXME
	/*
	 * If not on pipe 0 all requests
	 * [except new pipe setting] must be rejected
	 */
	if (pipe == GDL_DISPLAY_ID_UNDEFINED
	    && id != OTM_HDMI_ATTR_ID_DISPLAY_PIPE) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	rc = __update_edid(ctx);
	if (rc != OTM_HDMI_SUCCESS) {
		LOG_ERROR("ERR: %s at %d code = %d\n",
				__func__, __LINE__, rc);
		goto exit;
	}
#endif
	edid = &ctx->edid_ext;

	switch (id) {
	case OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT:
		out = auint;
		depth =
			ctx->dc ? PD_ATTR_UINT(ATTRS
				[OTM_HDMI_ATTR_ID_PIXEL_DEPTH]) :
				OTM_HDMI_OPD_24BIT;
		rc = __check_opf_support(out, edid->ycbcr444, edid->ycbcr422);
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;
		rc = __check_pixel_depth_cfg(out, depth);
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;
#ifdef OTM_HDMI_FIXME
		rc = ctx->support->pd_setmode_required(pipe);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_PIXEL_DEPTH:
		if (false == ctx->dc) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
		depth = auint;
		out = PD_ATTR_UINT(ATTRS
				[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]);

		rc = __check_opd_support(depth, edid->dc_30, edid->dc_36);
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;

		rc = __check_pixel_depth_cfg(out, depth);
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;

#ifdef OTM_HDMI_FIXME
		rc = ctx->support->pd_setmode_required(pipe);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_HDCP:
		if (true == ctx->dtv) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
#ifdef OTM_HDMI_FIXME
		rc = (abool != hdcp_ctrl) ? service_hdcp_set(ctx, abool,
						!abool) :
						OTM_HDMI_SUCCESS;
#endif
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;
		break;
#ifdef OTM_HDMI_FIXME
	case OTM_HDMI_ATTR_ID_BG_COLOR:
		rc  = hdmi_video_set_pixel_color(&ctx->dev, auint);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
		break;
#endif
	case OTM_HDMI_ATTR_ID_USE_EDID:
		if (true == ctx->dtv) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}

		rc = otm_hdmi_edid_parse(ctx, auint);
		if (OTM_HDMI_SUCCESS != rc)
			goto exit;
#ifdef OTM_HDMI_FIXME
		rc  = ctx->support->pd_setmode_required(pipe);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_POWER:
		if (abool != pwr) {
			/*
			 * When shutting down the PHY also disable HDCP
			 * Some sinks are picky to PHY going off
			 * when input is encrypted PHY enabling expects
			 * HDCP to be off for unmute to happen
			 */
			if (!abool) {
#ifdef OTM_HDMI_FIXME
				rc = service_hdcp_set(ctx, false,
							true);
				if (rc != OTM_HDMI_SUCCESS) {
					LOG_ERROR("ERR: %s at %d code :%d\n",
						 __func__, __LINE__, rc);
					goto exit;
				}
#endif
			}
			rc = hdmi_phy_enable(&(ctx->dev), abool);
			if (OTM_HDMI_SUCCESS != rc)
				goto exit;
#ifdef OTM_HDMI_FIXME
			ctx->support->pd_submit_event
				(GDL_APP_EVENT_PHY_STATUS_CHANGE);
#endif
		}
		break;

	case OTM_HDMI_ATTR_ID_SLOW_DDC:
#ifdef OTM_HDMI_FIXME
		rc = hdmi_i2c_set_ddc_speed(&ctx->dev, abool);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT:
		if (abool) {
			/*
			 * xvYCC colorimetry can only be set
			 * if we are outputing YCbCr
			 */
			out = PD_ATTR_UINT(ATTRS
				[OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT]);
			if (OTM_HDMI_OPF_RGB444 == out) {
				rc = OTM_HDMI_ERR_FAILED;
				goto exit;
			}
#ifdef OTM_HDMI_FIXME
			/*
			 * xvYCC colorimetry can only be set
			 * if we are sending gamut packets
			 */
			rc1 =
			    hdmi_packet_check_type(&ctx->pi_0.packet,
						   HDMI_PACKET_GAMUT);
			rc2 =
			    hdmi_packet_check_type(&ctx->pi_1.packet,
						   HDMI_PACKET_GAMUT);
#endif
		}
		if (false == ctx->usr_avi) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
		break;

	case OTM_HDMI_ATTR_ID_DISPLAY_PIPE:
#ifdef OTM_HDMI_FIXME
		if (GDL_DISPLAY_ID_1 == auint) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
		if (pipe == pipe)
			goto exit;
		rc = __switch_pipe(ctx);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_MUTE:
		/*
		 * Audio mute is handled indirectly
		 * during writes based on attribute value
		 * Video mute needs to be handled directly
		 */
		hdmi_mute(ctx, auint, MUTE_SOURCE_APP);
		break;

	case OTM_HDMI_ATTR_ID_PURE_VIDEO:
#ifdef OTM_HDMI_FIXME
		rc = hdmi_video_set_video_indicator(&ctx->dev, abool);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_OUTPUT_DITHER:
#ifdef OTM_HDMI_FIXME
		rc = ctx->support->pd_setmode_required(pipe);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_HDCP_ENCRYPT:
#ifdef OTM_HDMI_FIXME
		rc = hdmi_hdcp_set_enc(ctx, abool && HDCP_OFF != hdcp_status);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_DVI:
#ifdef OTM_HDMI_FIXME
		rc = ctx->support->pd_setmode_required(pipe);
		if (rc != OTM_HDMI_SUCCESS) {
			LOG_ERROR("ERR: %s at %d code = %d\n",
					__func__, __LINE__, rc);
			goto exit;
		}
#endif
		break;

	case OTM_HDMI_ATTR_ID_PAR:
	case OTM_HDMI_ATTR_ID_FAR:
		if (false == ctx->usr_avi) {
			rc = OTM_HDMI_ERR_FAILED;
			goto exit;
		}
		break;
	case OTM_HDMI_ATTR_ID_DEBUG:
		ATTRS[id].content._uint.value = auint;
		break;
	default:
		break;
	}

#ifdef OTM_HDMI_FIXME
	rc = ctx->support->pd_attr_set(ATTRS, id, data, flags);
	if (rc != OTM_HDMI_SUCCESS) {
		LOG_ERROR("ERR: %s at %d code = %d\n",
				__func__, __LINE__, rc);
		goto exit;
	}
#endif
	/*
	 * Post processing [here we call routines
	 * that rely on the value in the ATTR
	 * table which are not yet available
	 * during the switch statement above
	 */
	switch (id) {
	case OTM_HDMI_ATTR_ID_COLOR_SPACE_INPUT:
		rc = (ctx->mode_set) ? __set_attr_csc(ctx) : rc;
		break;

	case OTM_HDMI_ATTR_ID_OUTPUT_CLAMP:
		rc = (ctx->mode_set) ? __set_attr_csc(ctx) : rc;
		if (false == ctx->usr_avi)
			goto exit;

	case OTM_HDMI_ATTR_ID_PAR:
	case OTM_HDMI_ATTR_ID_FAR:
	case OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT:
		rc = (ctx->mode_set) ?
			otm_hdmi_infoframes_set_avi(ctx,
			&ctx->mode) : rc;
		break;

	default:
		break;
	}

exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}
EXPORT_SYMBOL(otm_hdmi_set_attribute);

/**
 * validates the attribute to be read or not.
 * @id	: attribute id to be validated
 *
 * Read's the attributes flag value.
 *
 * Returns -
 *	OTM_HDMI_SUCCESS - if the attribute is readable.
 *	OTM_HDMI_ERR_INTERNAL -	if the attribute is non-readable.
 *	OTM_HDMI_ERR_FAILED - if the attribute is not in range.
 */
static otm_hdmi_ret_t otm_hdmi_attr_get_validate(otm_hdmi_attribute_id_t id)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	if (id < 0 || id >= OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES) {
		LOG_ERROR("Invalid argument passed (id): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	/*
	 * Based on attribute type perform appropriate check
	*/
	if (OTM_HDMI_ATTR_FLAG_WRITE & ATTRS[id].flags) {
		return rc;
	} else if (OTM_HDMI_ATTR_FLAG_SUPPORTED & ATTRS[id].flags) {
		/*
		 * Needs a Fix.
		 */
		return rc;
	} else if (OTM_HDMI_ATTR_FLAG_INTERNAL & ATTRS[id].flags) {
		rc = OTM_HDMI_ERR_INTERNAL;
		return rc;
	} else {
		LOG_ERROR("Invalid attribute accessed: (%d)\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		return rc;
	}
exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * Getting given attribute
 * @context		:opaque hdmi context
 * @id			: attribute id
 * @attribute		:user provided buffer for attribute details
 * @log			: a hint wether port driver should log the call
 *
 * Returns otm_hdmi_ret_t check. Getting given attribute values
 */
otm_hdmi_ret_t otm_hdmi_get_attribute(void *context,
					otm_hdmi_attribute_id_t id,
					otm_hdmi_attribute_t *attribute,
					bool log)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	LOG_ENTRY((log) ? LOG_LEVEL_HIGH : LOG_LEVEL_VBLANK);

	rc = otm_hdmi_attr_get_validate(id);
	if (OTM_HDMI_SUCCESS != rc)
		goto exit;
	if (NULL == attribute || NULL == context) {
		LOG_ERROR("Invalid argument passed (attribute): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	*attribute = ATTRS[id];
exit:
	LOG_EXIT((log) ? LOG_LEVEL_HIGH : LOG_LEVEL_VBLANK, rc);
	return rc;
}
EXPORT_SYMBOL(otm_hdmi_get_attribute);

/**
 * Attribute name getting routine
 * @id		: attribute id
 *
 * Returns attribute name corresponding to id
 */
char *__pd_attr_get_name(otm_hdmi_attribute_id_t id)
{
	otm_hdmi_attribute_t *table = otm_hdmi_attributes_table;

	if ((0 <= id) && (id < OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES))
		return table[id].name;
	else
		return NULL;
}
EXPORT_SYMBOL(__pd_attr_get_name);

/**
 * Generic attribute declaration routine
 * @table	: attribute table to be updated
 * @id		: id to be updated to the table
 * @type		: attribute type
 * @flags	: attribute flags
 * @name		: attribute name
 * @value	: attribute default value
 * @min		: min value possible for the attribute
 * @max		: max value possible for the attribute
 *
 * Returns check otm_hdmi_ret_t
 */
static otm_hdmi_ret_t __pd_attr_declare(otm_hdmi_attribute_t *table,
				otm_hdmi_attribute_id_t id,
				otm_hdmi_attribute_type_t type,
				otm_hdmi_attribute_flag_t flags,
				char *name,
				void *value,
				unsigned int min,
				unsigned int max)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	if ((id < 0) || (id > OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES))
		return  OTM_HDMI_ERR_FAILED;

	if ((name != NULL) && (strlen(name) <= OTM_HDMI_MAX_STRING_LENGTH))
		strcpy(table[id].name, name);
	else
		LOG_ERROR("set default name\n");
		/* TODO: set default name */

	table[id].flags = flags;

	switch (type) {
	case OTM_HDMI_ATTR_TYPE_UINT:
			table[id].content._uint.value_default =
						(unsigned int) value;
			table[id].content._uint.value_min = min;
			table[id].content._uint.value_max = max;
			break;
	case OTM_HDMI_ATTR_TYPE_BOOLEAN:
			table[id].content._bool.value_default =
						(bool) value;
			break;
	case OTM_HDMI_ATTR_TYPE_STRING:
			if ((value != NULL) &&
			strlen(value) <= OTM_HDMI_MAX_STRING_LENGTH)
				strcpy(table[id].content.string.value,
					(char *) value);
			else
				rc = OTM_HDMI_ERR_FAILED;
			break;
	default:
			break;
	}
	return rc;
}

/**
 * otm_hdmi_declare_attributes - init hdmi global attributes table
 * @declare	: declare attribute
 * @get_name	: name of the attribute
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_declare_attributes(pd_attr_declare_t declare,
						pd_attr_get_name_t get_name)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	otm_hdmi_attribute_t *table = otm_hdmi_attributes_table;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/*
	 * declare(table, OTM_HDMI_ATTR_ID_NAME,
	 * OTM_HDMI_ATTR_TYPE_STRING, PD_ATTR_FLAGS_RS,
	 * get_name(OTM_HDMI_ATTR_ID_NAME),
	 * (void *) PD_NAME, 0, 0);
	 */

	PD_DECLARE_ATTRIBUTE_STRING(declare, table,
		OTM_HDMI_ATTR_ID_NAME,
		PD_ATTR_FLAGS_RS, get_name, PD_NAME);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_CABLE_STATUS,
		PD_ATTR_FLAGS_RS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_POWER,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_MUTE,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_STATUS,
		PD_ATTR_FLAGS_RS, get_name,
		OTM_HDMI_HDCP_STATUS_OFF,
		OTM_HDMI_HDCP_STATUS_OFF,
		OTM_HDMI_HDCP_STATUS_ON);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_ENCRYPT,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_COLOR_SPACE_INPUT,
		PD_ATTR_FLAGS_RWSI,
		get_name,
		OTM_HDMI_COLOR_SPACE_RGB,
		0, OTM_HDMI_COLOR_SPACE_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT,
		PD_ATTR_FLAGS_RWS, get_name,
		OTM_HDMI_OPF_RGB444, 0, OTM_HDMI_OPF_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PIXEL_DEPTH,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_OPD_24BIT,
		0, OTM_HDMI_PIXEL_DEPTH_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_BG_COLOR,
		PD_ATTR_FLAGS_RWS,
		get_name, 0xFF0000, 0x000000, 0xFFFFFF);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_USE_EDID,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_USE_EDID_REAL,
		OTM_HDMI_USE_EDID_NONE,
		OTM_HDMI_USE_EDID_SAFE);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_DEBUG,
		PD_ATTR_FLAGS_RWS,
		get_name,
		LOG_LEVEL_ERROR,
		__LOG_LEVEL_MIN, __LOG_LEVEL_MAX);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_VERSION_MAJOR,
		PD_ATTR_FLAGS_RS, get_name, 0, 0, 100);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_VERSION_MINOR,
		PD_ATTR_FLAGS_RS, get_name, 4, 0, 9);

	PD_DECLARE_ATTRIBUTE_STRING(declare, table,
		OTM_HDMI_ATTR_ID_BUILD_DATE,
		PD_ATTR_FLAGS_RS, get_name, __DATE__);

	PD_DECLARE_ATTRIBUTE_STRING(declare, table,
		OTM_HDMI_ATTR_ID_BUILD_TIME,
		PD_ATTR_FLAGS_RS, get_name, __TIME__);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_DISPLAY_PIPE,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_DISPLAY_ID_0,
		OTM_HDMI_DISPLAY_ID_0,
		OTM_HDMI_DISPLAY_ID_UNDEFINED);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PAR,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_PAR_NO_DATA,
		OTM_HDMI_PAR_NO_DATA,
		OTM_HDMI_PAR_16_9);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_FAR,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_FAR_SAME_AS_PAR,
		OTM_HDMI_FAR_16_9_TOP,
		OTM_HDMI_FAR_16_9_SP_4_3);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_SLOW_DDC,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_AUDIO_CLOCK,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_AUDIO_CLOCK_36,
		OTM_HDMI_AUDIO_CLOCK_24,
		OTM_HDMI_AUDIO_CLOCK_16);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_AUDIO_STATUS,
		PD_ATTR_FLAGS_RS, get_name, false);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_TMDS_DELAY,
		PD_ATTR_FLAGS_RWS, get_name, 400, 100, 500);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_OUTPUT_CLAMP,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_OUTPUT_DITHER,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_1P1,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_MUTE,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_MUTE_OFF,
		OTM_HDMI_MUTE_OFF, OTM_HDMI_MUTE_BOTH);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_PURE_VIDEO,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_PHY,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_RETRY,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_DVI,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_RI_RETRY,
		PD_ATTR_FLAGS_RWS, get_name, 40, 0, 50);

	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}
EXPORT_SYMBOL(otm_hdmi_declare_attributes);

/**
 * Description: crtc mode set function for hdmi.
 *
 * @context		:hdmi_context
 * @mode		:mode requested
 * @adjusted_mode	:adjusted mode
 * @fb_width		:allocated frame buffer dimensions
 * @fb_height		:allocated frame buffer dimensions
 * @pclk_khz:		tmds clk value for the best pll and is needed for audio.
 *			This field has to be moved into OTM audio
 *			interfaces when implemented
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
/*
 * TODO: Revisit scaling type enums:
 * 0 - scale_none
 * 1 - full screen
 * 2 - center
 * 3 - scale aspect
 */
otm_hdmi_ret_t otm_hdmi_crtc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode, int fb_width,
			int fb_height, uint32_t *pclk_khz)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	int scalingtype = IPIL_TIMING_SCALE_NONE;

	/* NULL checks */
	if (context == NULL || mode == NULL || adjusted_mode == NULL
						|| pclk_khz == NULL) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* prepare for crtc mode set. This includes any hdmi unit reset etc. */
	rc = ipil_hdmi_crtc_mode_set_prepare(&ctx->dev);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed in preparing for mode set\n");
		return rc;
	}

	/* get the preferred scaling type for the platform */
	rc = ps_hdmi_get_pref_scalingtype(ctx, &scalingtype);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to get preferred scaling type\n");
		return rc;
	}
	/* TODO: For phone, these registers are taken care in
	 * pipe_set_base call. Need to combine both the solutions
	 * into a common function.
	 */
#ifndef MFLD_HDMI_PR3
	/* program display related registers: dpssize and pipesrc, pfit */
	rc = ipil_hdmi_crtc_mode_set_program_dspregs(&ctx->dev, scalingtype,
						mode, adjusted_mode,
						fb_width, fb_height);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to set display registers\n");
		return rc;
	}
#endif

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_timings(&ctx->dev,
						scalingtype, mode,
						adjusted_mode);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to set timing registers\n");
		return rc;
	}

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_dpll(&ctx->dev,
						adjusted_mode->dclk);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to program dpll\n");
		return rc;
	}
	*pclk_khz = ctx->dev.clock_khz;

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_pipeconf(&ctx->dev);
	if (rc != OTM_HDMI_SUCCESS)
		pr_debug("\nfailed to program pipeconf\n");

	return rc;
}

/**
 * encoder mode set function for hdmi
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 * encoder mode set function for hdmi. enables phy.
 * set correct polarity for the current mode.
 */
otm_hdmi_ret_t otm_hdmi_enc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	bool is_monitor_hdmi;

	/* NULL checks */
	if (context == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_INVAL;
	}

	is_monitor_hdmi = otm_hdmi_is_monitor_hdmi(ctx);
	pr_debug("\nMonitor Mode: %s\n", is_monitor_hdmi ? "HDMI" : "DVI");

	/* handle encoder mode set */
	rc = ipil_hdmi_enc_mode_set(&ctx->dev, mode, adjusted_mode,
					is_monitor_hdmi);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed in programing enc mode set\n");
		return rc;
	}

	/* Enable AVI infoframes for HDMI mode */
	if (is_monitor_hdmi) {
		rc = otm_hdmi_infoframes_set_avi(context, mode);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to program avi infoframe\n");
	} else {/* Disable all inofoframes for DVI mode */
		rc = otm_hdmi_disable_all_infoframes(context);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to disable all infoframes\n");
	}

	return rc;
}

/**
 * Restore HDMI registers and enable the display
 * @context	:hdmi_context
 * @connected	:hdmi connected or not
 *
 * Returns:	none
 * Restore HDMI registers and enable the display
 */
void otm_hdmi_restore_and_enable_display(void *context, bool connected)
{
	if (NULL != context) {
		if (connected) {
			ipil_hdmi_restore_and_enable_display(
					&((hdmi_context_t *)context)->dev);
			/* restore data island packets */
			if (otm_hdmi_is_monitor_hdmi(context)) {
				ipil_hdmi_restore_data_island(
					&((hdmi_context_t *)context)->dev);
			}
		} else {
			ipil_hdmi_destroy_saved_data(
					&((hdmi_context_t *)context)->dev);
		}
#ifdef OTM_HDMI_HDCP_ENABLE
		/* inform HDCP about resume */
		if (otm_hdmi_hdcp_set_power_save(context, false)
						== false)
			pr_debug("failed to resume hdcp\n");
#endif
	}
}

/**
 * save HDMI display registers
 * @context	:hdmi_context
 * @connected	:hdmi connected or not
 *
 * Returns:	none
 * save HDMI display registers
 */
void otm_hdmi_save_display_registers(void *context, bool connected)
{
	if (NULL != context) {
		if (connected) {
			ipil_hdmi_save_display_registers(
					&((hdmi_context_t *)context)->dev);
			/* save data island packets */
			if (otm_hdmi_is_monitor_hdmi(context)) {
				ipil_hdmi_save_data_island(
					&((hdmi_context_t *)context)->dev);
			}
		} else {
			ipil_hdmi_destroy_saved_data(
					&((hdmi_context_t *)context)->dev);
		}
	}
}

/**
 * disable HDMI display
 * @context:	hdmi_context
 *
 * Returns:	none
 * disable HDMI display
 */
void otm_disable_hdmi(void *context)
{
	if (NULL != context) {
#ifdef OTM_HDMI_HDCP_ENABLE
		/* inform HDCP about suspend */
		if (otm_hdmi_hdcp_set_power_save(context, true)
						== false)
			pr_debug("failed to suspend hdcp\n");
#endif
		/* disable HDMI */
		ipil_disable_hdmi(&((hdmi_context_t *)context)->dev);
	}
}

/*
 *
 * Internal scripts wrapper functions.
 *
 */

/* Starting this off, but all scripts/unit test helpers should move
 * to another file.
 */

#ifdef OTM_HDMI_UNIT_TEST

/**
 * test_otm_hdmi_report_edid() - Report current EDID information
 *
 * This routine simply dumps the EDID information
 *
 * Returns - nothing
 */
void test_otm_hdmi_report_edid(void)
{
	edid_info_t *edid = NULL;
	if (NULL == g_context) {
		LOG_PRINT(LOG_LEVEL_HIGH,
			     "Cant print EDID, Initialize otm_hdmi first!\n");
		return;
	}
	edid = &g_context->edid_int;
	if (NULL == edid) {
		LOG_PRINT(LOG_LEVEL_HIGH,
			     "EDID not initialized in driver.\n");
		return;
	}
	__hdmi_report_edid(g_context, edid);
}
EXPORT_SYMBOL_GPL(test_otm_hdmi_report_edid);
#endif

#ifdef OTM_HDMI_UNIT_TEST

/**
 * get_hdmi_context() - Getting hdmi_context
 *
 * This routine gives a handle to hdmi_context
 * to be used with other function calls like
 * set_attribute which requires hdmi_context
 * as one of the params
 *
 * Returns - hdmi_context
 */
void *otm_hdmi_get_context(void)
{
	return (void *)g_context;
}
EXPORT_SYMBOL_GPL(otm_hdmi_get_context);
#endif

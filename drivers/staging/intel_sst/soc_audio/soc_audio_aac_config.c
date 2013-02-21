/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

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

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.
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

#include <linux/string.h>
#include "soc_debug.h"
#include "soc_audio_pipeline.h"
#include "soc_audio_api.h"
#include "soc_audio_bu_config.h"
#include "soc_audio_aac_config.h"
#include "soc_audio_aac_defs.h"
#include "soc_ipc.h"

#ifdef INCLUDE_AAC_DECODE

/* This function is used for AAC to check decode parameters */
enum soc_result audio_aac_check_dec_params(struct
					   soc_audio_codec_params
					   *param_value)
{
	enum soc_result result = SOC_SUCCESS;
	enum soc_audio_aac_downmixer b_aac_downmixer;
	enum soc_audio_aac_mono_to_stereo to_stereo;
	enum soc_audio_aac_surround_mono_to_stereo
	    b_aac_surround_mono_to_stereo;
	int32_t enable_prl;
	int32_t enable_drc;
	int32_t target_level;
	int32_t drc_cut_fac;
	int32_t drc_boost_fac;

	SOC_ENTER();

	/* copy the parameter value of Decoder Config Parameters */
	to_stereo = param_value->config.aac_params.to_stereo;
	b_aac_downmixer = param_value->config.aac_params.b_aac_downmixer;
	b_aac_surround_mono_to_stereo =
	    param_value->config.aac_params.b_aac_surround_mono_to_stereo;
	enable_prl = param_value->config.aac_params.enable_prl;
	enable_drc = param_value->config.aac_params.enable_drc;
	target_level = param_value->config.aac_params.target_level;
	drc_cut_fac = param_value->config.aac_params.drc_cut_fac;
	drc_boost_fac = param_value->config.aac_params.drc_boost_fac;

	/* Check if Downmixer Disable */
	if ((b_aac_downmixer != SOC_AUDIO_AAC_DOWNMIXER_DISABLE) &&
	    (b_aac_downmixer != SOC_AUDIO_AAC_DOWNMIXER_ENABLE)) {
		soc_debug_print(ERROR_LVL, "aac: AAC_DOWNMIXER_ON_OFF param "
				"failed");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check if the channel is to stereo */
	if ((to_stereo != SOC_AUDIO_AAC_MONO_TO_STEREO_SINGLE_CHANNEL) &&
	    (to_stereo != SOC_AUDIO_AAC_MONO_TO_STEREO_2_CHANNELS)) {
		soc_debug_print(ERROR_LVL, "aac: MONO_TO_STEREO param "
				"invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* check if the channel is AAC surround to stereo */
	if ((b_aac_surround_mono_to_stereo !=
	     SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_SINGLE_CHANNEL) &&
	    (b_aac_surround_mono_to_stereo ==
	     SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_2_CHANNELS)) {
		soc_debug_print(ERROR_LVL, "aac: SURROUND_MONO_TO_STEREO param "
				"invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check the boundary for enable_prl */
	if (enable_prl != false && enable_prl != true) {
		soc_debug_print(ERROR_LVL, "aac: SOC_AUDIO_AAC_PRL_ENABLE "
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}
	/* Check the boundary for enable dynamic range compression */
	if (enable_drc != false && enable_drc != true) {
		soc_debug_print(ERROR_LVL, "aac: SOC_AUDIO_AAC_DRC_ENABLE "
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check the target between upper and lower boundary */
	if (target_level < SOC_AUDIO_AAC_TARGET_LEVEL_MIN ||
	    target_level > SOC_AUDIO_AAC_TARGET_LEVEL_MAX) {
		soc_debug_print(ERROR_LVL, "aac: SOC_AUDIO_AAC_TARGET_LEVEL "
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check AAC compression range */
	if (drc_cut_fac < SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MIN ||
	    drc_cut_fac > SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MAX) {
		soc_debug_print(ERROR_LVL, "aac: "
				"SOC_AUDIO_AAC_DRC_COMPRESS_FAC param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check DRC boost range */
	if (drc_boost_fac < SOC_AUDIO_AAC_DRC_BOOST_FAC_MIN ||
	    drc_boost_fac > SOC_AUDIO_AAC_DRC_BOOST_FAC_MAX) {
		soc_debug_print(ERROR_LVL, "aac: "
				"SOC_AUDIO_AAC_DRC_BOOST_FAC param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

error_exit:
	SOC_EXIT();
	return result;
}

/* This function is used to set a specific AAC decoder parameter */
enum soc_result audio_aac_set_dec_param(struct soc_audio_input_wl *input_wl,
					int param_id, void *param_value)
{
	enum soc_result result = SOC_SUCCESS;
	enum soc_audio_aac_downmixer *t_aac_downmixer;
	enum soc_audio_aac_mono_to_stereo *mono_stereo;
	enum soc_audio_aac_surround_mono_to_stereo
	*t_aac_surround_mono_to_stereo;
	struct soc_audio_codec_params *codec_params;

	int host_params;

	SOC_ENTER();
	codec_params =
	    &input_wl->pipeline->
	    stages[SOC_AUDIO_DEC_DECODE_STAGE].stage_params.decoder.host;

	switch (param_id) {
	case SOC_AUDIO_AAC_DOWNMIXER_ON_OFF:
		t_aac_downmixer = (enum soc_audio_aac_downmixer *)param_value;

		if ((SOC_AUDIO_AAC_DOWNMIXER_DISABLE == *t_aac_downmixer) ||
		    (SOC_AUDIO_AAC_DOWNMIXER_ENABLE == *t_aac_downmixer)) {
			codec_params->config.aac_params.b_aac_downmixer =
			    *t_aac_downmixer;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: AAC_DOWNMIXER_ON_OFF param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_MONO_TO_STEREO_MODE:
		mono_stereo = (enum soc_audio_aac_mono_to_stereo *)param_value;

		if ((SOC_AUDIO_AAC_MONO_TO_STEREO_SINGLE_CHANNEL ==
		     *mono_stereo)
		    || (SOC_AUDIO_AAC_MONO_TO_STEREO_2_CHANNELS ==
			*mono_stereo)) {
			codec_params->config.aac_params.to_stereo =
			    *mono_stereo;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
					"aac: MONO_TO_STEREO param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_MODE:
		t_aac_surround_mono_to_stereo =
		    (enum soc_audio_aac_surround_mono_to_stereo *)param_value;

		if ((SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_SINGLE_CHANNEL ==
		     *t_aac_surround_mono_to_stereo) ||
		    (SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_2_CHANNELS ==
		     *t_aac_surround_mono_to_stereo)) {
			codec_params->config.aac_params.
			    b_aac_surround_mono_to_stereo =
			    *t_aac_surround_mono_to_stereo;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: SURROUND_MONO_TO_STEREO param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_PRL_ENABLE:
		host_params = *((int *)param_value);
		if ((false == host_params) || (true == host_params)) {
			codec_params->config.aac_params.enable_prl =
			    host_params;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
					"aac: AAC_PRL_ENABLE param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_DRC_ENABLE:
		host_params = *((int *)param_value);
		if ((false == host_params) || (true == host_params)) {
			codec_params->config.aac_params.enable_drc =
			    host_params;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
					"aac: AAC_DRC_ENABLE param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_TARGET_LEVEL:
		host_params = *((int *)param_value);
		if (host_params >= SOC_AUDIO_AAC_TARGET_LEVEL_MIN &&
		    host_params <= SOC_AUDIO_AAC_TARGET_LEVEL_MAX) {
			codec_params->config.aac_params.target_level =
			    host_params;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
					"aac: AAC_TARGET_LEVEL param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_DRC_COMPRESS_FAC:
		host_params = *((int *)param_value);
		if (host_params >= SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MIN &&
		    host_params <= SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MAX) {
			codec_params->config.aac_params.drc_cut_fac =
			    (int)((host_params << 23) / 100);
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: AAC_DRC_COMPRESS_FAC param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_DRC_BOOST_FAC:
		host_params = *((int *)param_value);
		if (host_params >= SOC_AUDIO_AAC_DRC_BOOST_FAC_MIN
		    && host_params <= SOC_AUDIO_AAC_DRC_BOOST_FAC_MAX) {
			codec_params->config.aac_params.drc_boost_fac =
			    (int)((host_params << 23) / 100);
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: AAC_DRC_BOOST_FAC param invalid!");
		}
		break;
	case SOC_AUDIO_AAC_PCM_WD_SZ:
		host_params = *((int *)param_value);
		if ((host_params == 16) || (host_params == 24)) {
			codec_params->config.aac_params.pcm_wdsz = host_params;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: SOC_AUDIO_AAC_PCM_WD_SZ invalid!");
		}
		break;
	case SOC_AUDIO_AAC_PCM_SAMPLE_RATE:
		host_params = *((int *)param_value);
		if (soc_audio_core_is_valid_sample_rate(host_params)) {
			codec_params->config.aac_params.externalsr
				= host_params;
		} else {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"aac: SOC_AUDIO_AAC_PCM_SAMPLE_RATE invalid!");
		}
		break;
	default:
		result = SOC_ERROR_INVALID_PARAMETER;
		soc_debug_print(ERROR_LVL, "aac: invalid config param type!");
		break;
	}

	SOC_EXIT();
	return result;
}

/* This function is used to set the Decoder Config Parameters */
void audio_aac_set_default_dec_params(enum soc_audio_format format,
				     struct soc_audio_codec_params
				      *dec_params)
{
	SOC_ENTER();
	memset(dec_params, 0, sizeof(struct soc_audio_codec_params));

	/* Params able to be modified by the user. */
	dec_params->config.aac_params.sbr_signaling = 2;

	/* Default values */
	dec_params->config.aac_params.to_stereo =
	    SOC_AUDIO_AAC_MONO_TO_STEREO_2_CHANNELS;
	dec_params->config.aac_params.b_aac_surround_mono_to_stereo =
	    SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_2_CHANNELS;

	/* Set the decoder param value to default */
	dec_params->config.aac_params.pcm_wdsz = 24;
	dec_params->config.aac_params.downmix = 0;
	dec_params->config.aac_params.bdownsample = 0;
	dec_params->config.aac_params.externalsr = 44100;
	dec_params->config.aac_params.zero_unused_chans = 0;
	dec_params->config.aac_params.outnchans = SOC_AUDIO_MAX_OUTPUT_CHANNELS;

	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_LEFT_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_LEFT_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_RIGHT_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_RIGHT_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_LFE_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_LFE_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_CENTER_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_CENTER_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_LEFT_SUR_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_L_SUR_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_RIGHT_SUR_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_R_SUR_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_LEFT_REAR_SUR_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_L_REAR_SUR_CH_ROUTE;
	dec_params->config.aac_params.
	    chanrouting[SOC_AUDIO_RIGHT_REAR_SUR_CHAN_ROUTE_INDEX] =
	    SOC_AUDIO_DEFAULT_R_REAR_SUR_CH_ROUTE;
	if (format == SOC_AUDIO_MEDIA_FMT_AAC_LOAS) {
		dec_params->config.aac_params.bsformat =
		    SOC_AUDIO_MEDIA_FMT_AAC_LOAS;
	} else if (format == SOC_AUDIO_MEDIA_FMT_AAC) {
		dec_params->config.aac_params.bsformat =
		    SOC_AUDIO_MEDIA_FMT_AAC;
	}
	/* set decoder config parameters to default */
	dec_params->config.aac_params.enable_drc = false;
	dec_params->config.aac_params.enable_prl = false;
	dec_params->config.aac_params.drc_cut_fac = 0;
	dec_params->config.aac_params.drc_boost_fac = 0;
	dec_params->config.aac_params.target_level = 124;

	SOC_EXIT();
	return;
}

#endif

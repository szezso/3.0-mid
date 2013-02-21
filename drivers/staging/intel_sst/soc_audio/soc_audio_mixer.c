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

#include <linux/mutex.h>
#include "soc_audio_api.h"
#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_ipc.h"
#include "soc_debug.h"
#include "soc_audio_defs.h"
#include "soc_audio_pipeline_specific.h"

/* Change user gain to coeff index */
static enum soc_result
audio_user_gain_to_coeff_index(int user_gain, int *coeff_index)
{
	enum soc_result result = SOC_ERROR_INVALID_PARAMETER;

	if ((user_gain >= SOC_AUDIO_GAIN_MUTE)
	    && (user_gain <= SOC_AUDIO_GAIN_MAX)) {
		*coeff_index =
		    SOC_AUDIO_MIX_COEFF_INDEX_0_DB -
		    ((user_gain * SOC_AUDIO_MIX_COEFF_STEPS_PER_DB) / 10);
		result = SOC_SUCCESS;
	}
	return result;
}

/*
Enable or disable processing of mixing metadata in this processor.
*/
enum soc_result
soc_audio_enable_mix_metadata(uint32_t processor_h, bool enable)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_mixer_params *mixer;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	SOC_AUDIO_CHK_PROC_EARGS(processor_h);

	/* Get the associated context */
	ctx = &(soc_audio_processor[processor_h]);

	mutex_lock(&ctx->processor_lock);
	{
		SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx);

		mixer =
		    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.
		    mixer;
		mixer->host.use_stream_metadata = enable;
		/* xzhou: Just follow the logic of previous implementation
		 * Not sure why only increase coeff_changed
		 * when enable is false. */
		if (!enable)
			mixer->host.coeff_changed++;

		/* Commit the changes back to the mixer
		 * if the post ATC pipe is running. */
		if (ctx->pipeline->configured)
			result =
			    soc_ipc_stage_configure(ctx, ctx->pipeline,
						    SOC_AUDIO_MIXER_STAGE);
	}
	mutex_unlock(&ctx->processor_lock);
	SOC_EXIT();
	return result;
}

/*
Set the channel configuration mode of the mixer.
*/
enum soc_result
soc_audio_set_mixing_channel_config(uint32_t processor_h,
				    enum soc_audio_mix_ch_cfg_mode
				    ch_cfg_mode, int ch_cfg)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_mixer_params *mixer;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	SOC_AUDIO_CHK_PROC_EARGS(processor_h);

	/* Get the associated context */
	ctx = &(soc_audio_processor[processor_h]);

	mutex_lock(&ctx->processor_lock);
	SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx);

	switch (ch_cfg_mode) {
	case SOC_AUDIO_MIX_CH_CFG_MODE_AUTO:
	case SOC_AUDIO_MIX_CH_CFG_MODE_PRIMARY:
		break;
	case SOC_AUDIO_MIX_CH_CFG_MODE_FIXED:
		if (!soc_audio_core_is_valid_ch_config(ch_cfg))
			result = SOC_ERROR_INVALID_PARAMETER;

		break;
	default:
		result = SOC_ERROR_INVALID_PARAMETER;
		break;
	}
	if (result != SOC_SUCCESS)
		goto exit_unlock;

	mixer =
	    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.mixer;
	mixer->host.ch_config_mode = ch_cfg_mode;
	mixer->host.ch_config_mode_fixed_ch_cfg = 0xFFFFFFFF;
	if (SOC_AUDIO_MIX_CH_CFG_MODE_FIXED == ch_cfg_mode) {
		/* Assume the validation of ch_cfg has been check */
		mixer->host.ch_config_mode_fixed_ch_cfg = ch_cfg;
	}
	/*Commit changes back to the mixer if the post ATC pipe is running.*/
	if (ctx->pipeline->configured) {
		result =
		    soc_ipc_stage_configure(ctx, ctx->pipeline,
					    SOC_AUDIO_MIXER_STAGE);
	}
exit_unlock:
	mutex_unlock(&ctx->processor_lock);
	SOC_EXIT();
	return result;
}

/*
Use this optional function call to set the sampling rate mode of the mixer.
*/
enum soc_result
soc_audio_set_mixing_sample_rate(uint32_t processor_h,
				 enum soc_audio_mix_sample_rate_mode
				 sample_rate_mode, int sample_rate)
{
	int32_t i;
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_pipeline_src_params *src;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	SOC_AUDIO_CHK_PROC_EARGS(processor_h);

	/* Get the associated context */
	ctx = &(soc_audio_processor[processor_h]);

	switch (sample_rate_mode) {
	case SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO:
	case SOC_AUDIO_MIX_SAMPLE_RATE_MODE_PRIMARY:
		break;
	case SOC_AUDIO_MIX_SAMPLE_RATE_MODE_FIXED:
		if (!soc_audio_core_is_valid_sample_rate(sample_rate)) {
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit;
		}
		break;
	default:
		result = SOC_ERROR_INVALID_PARAMETER;
		goto exit;
	}
	mutex_lock(&ctx->processor_lock);
	SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx);
	if (ctx->pipeline->configured) {
		/* Get current state of the mixer parameters */
		result = soc_ipc_stage_get_params(ctx, ctx->pipeline,
						  SOC_AUDIO_SRC_STAGE);
		if (result != SOC_SUCCESS) {
			soc_debug_print(ERROR_LVL,
				"Failed to get the mixer parameters!\n");
			goto exit_unlock;
		}
	}
	src = &ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].stage_params.src;
	src->mix_src_mode = sample_rate_mode;
	if (sample_rate_mode == SOC_AUDIO_MIX_SAMPLE_RATE_MODE_FIXED) {
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++)
			src->output_sample_rate[i] = sample_rate;
	}
	/* Commit the changes back to the mixer */
	if (ctx->pipeline->configured) {
		result =
		    soc_ipc_stage_configure(ctx, ctx->pipeline,
					    SOC_AUDIO_SRC_STAGE);
	}
exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/*
Use this optional function call to get the sampling rate mode of the mixer.
*/
enum soc_result
soc_audio_get_mixing_sample_rate(uint32_t processor_h,
				 uint32_t mixer_input_index,
				 enum soc_audio_mix_sample_rate_mode
				 *sample_rate_mode, int *sample_rate)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_pipeline_src_params *src;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	SOC_AUDIO_CHK_PROC_EARGS(processor_h);
	SOC_AUDIO_CHK_EARGS(sample_rate_mode
			    && sample_rate
			    && (mixer_input_index < SOC_AUDIO_MAX_INPUTS));

	/* Get the associated context */
	ctx = &(soc_audio_processor[processor_h]);

	mutex_lock(&ctx->processor_lock);
	SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx);

	if (ctx->pipeline->configured) {
		/* Get current state of the mixer parameters */
		result = soc_ipc_stage_get_params(ctx, ctx->pipeline,
						  SOC_AUDIO_SRC_STAGE);
		if (result != SOC_SUCCESS) {
			soc_debug_print(ERROR_LVL,
				"Failed to get the mixer parameters!\n");
			goto exit;
		}
	}
	src = &ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].stage_params.src;
	*sample_rate_mode = src->mix_src_mode;
	*sample_rate = src->output_sample_rate[mixer_input_index];
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/*
Use to update the gain of all channels of the Audio Processor.
*/
static enum soc_result
audio_set_channel_gain(struct soc_audio_processor_context *ctx,
		       int32_t *mix_ch_gain)
{
	int32_t i;
	struct soc_audio_mixer_params *mixer;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	mixer =
	    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.mixer;
	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++)
		mixer->host.output_ch_gains[i] = mix_ch_gain[i];

	mixer->host.output_ch_gains_id = 0x76543210;
	mixer->host.coeff_changed++;
	/* Commit the changes back to the mixer */
	if (ctx->pipeline->configured)
		result = soc_ipc_stage_configure(ctx, ctx->pipeline,
						 SOC_AUDIO_MIXER_STAGE);
	SOC_EXIT();
	return result;
}

/*
Set destination channel location and signal strength of input channels.
*/
enum soc_result
soc_audio_input_set_channel_mix(uint32_t processor_h,
				uint32_t input_index,
				struct soc_audio_channel_mix_config
				*ch_mix_config)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_channel_mix_config mixer_config;
	struct soc_audio_mixer_params *mixer_params;
	uint32_t input_ch, output_ch, ch_id;
	int coeff_index;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);
	SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx);

	for (input_ch = 0; input_ch < SOC_AUDIO_MAX_INPUT_CHANNELS;
		input_ch++) {
		ch_id =
		    ch_mix_config->input_channels_map[input_ch].
		    input_channel_id;
		if (ch_id >= SOC_AUDIO_MAX_INPUT_CHANNELS) {
			soc_debug_print(ERROR_LVL,
			"Mixer gain input_channel_id is out of range!\n");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit;
		}
		mixer_config.input_channels_map[input_ch].input_channel_id =
		    ch_id;
		for (output_ch = 0; output_ch < SOC_AUDIO_MAX_OUTPUT_CHANNELS;
		     output_ch++) {
			result =
			    audio_user_gain_to_coeff_index
			    (ch_mix_config->input_channels_map
			     [input_ch].output_channels_gain[output_ch],
			     &coeff_index);
			if (result != SOC_SUCCESS) {
				soc_debug_print(ERROR_LVL,
				"Mixer gain parameters (%d) is out of range!\n",
				ch_mix_config->input_channels_map
				[input_ch].output_channels_gain
				[output_ch]);
				result = SOC_ERROR_INVALID_PARAMETER;
				goto exit;
			}
			mixer_config.input_channels_map[input_ch].
			    output_channels_gain[output_ch] = coeff_index;
		}
	}
	mixer_params =
	    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.mixer;

	/* If the pipe is running, need to update just the host side.
	 * Else we need update the DSP copy which gets picked up when the
	 * pipe starts. This is because we just keep 1 copy of the mixer coeff
	 * in the host params to save some major space. */
	if (ctx->pipeline->configured) {
		memcpy(&mixer_params->host.channel_mix_config, &mixer_config,
		       sizeof(struct soc_audio_channel_mix_config));
		mixer_params->host.input_index = input_index;
		if (!ctx->muted) {
			result =
			    soc_ipc_stage_configure(ctx, ctx->pipeline,
						    SOC_AUDIO_MIXER_STAGE);
			if (result != SOC_SUCCESS)
				soc_debug_print(ERROR_LVL,
				"soc_ipc_stage_configure failed\n");
		}
	} else {
		memcpy(&mixer_params->
		       input_config[input_index].channel_mix_config,
		       &mixer_config,
		       sizeof(struct soc_audio_channel_mix_config));
	}
	/*Need to tell mixer we got coefficients from the user.(out of band)*/
	mixer_params->host.oob_gains_set = true;
	mixer_params->host.coeff_changed++;
exit:
	mutex_unlock(&ctx->processor_lock);
	SOC_EXIT();
	return result;
}

/* Enable and set master volume of the mixed output of the Audio Processor.*/
enum soc_result
soc_audio_set_master_volume(uint32_t processor_h, int32_t master_volume)
{
	int32_t i, volume;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);
	ctx->master_volume_enable = true;
	ctx->master_volume = SOC_AUDIO_ENSURE_VALID_GAIN(master_volume);
	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		if (ctx->per_channel_volume_enable) {
			volume =
			    SOC_AUDIO_ENSURE_VALID_GAIN(ctx->master_volume +
							ctx->per_channel_volume
							[i]);
		} else {
			volume = ctx->master_volume;
		}
		result = audio_user_gain_to_coeff_index(volume,
							&ctx->post_mix_ch_gain
							[i]);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Mixer gain parameters is out of range!\n");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit_unlock;
		}
	}
	if (!ctx->muted)
		result = audio_set_channel_gain(ctx, ctx->post_mix_ch_gain);
exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Retrieve the current master volume setting. */
enum soc_result
soc_audio_get_master_volume(uint32_t processor_h,
			    int32_t *master_volume)
{
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if ((SOC_SUCCESS != result) || !master_volume)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);
	*master_volume = ctx->master_volume;
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Disable master volume control on the Audio Processor. */
enum soc_result soc_audio_disable_master_volume(uint32_t processor_h)
{
	int32_t i;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);
	ctx->master_volume_enable = false;
	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		if (ctx->per_channel_volume_enable) {
			result =
			    audio_user_gain_to_coeff_index
			    (ctx->per_channel_volume[i],
			     &ctx->post_mix_ch_gain[i]);
		} else {
			ctx->post_mix_ch_gain[i] =
			    SOC_AUDIO_MIX_COEFF_INDEX_0_DB;
		}
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Mixer gain parameters is out of range!\n");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit_unlock;
		}
	}
	if (!ctx->muted)
		result = audio_set_channel_gain(ctx, ctx->post_mix_ch_gain);
exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Mute or unmute the mixed output of the Audio Processor. */
enum soc_result soc_audio_mute(uint32_t processor_h, bool mute)
{
	int32_t i;
	int32_t mix_ch_gain[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);

	if (ctx->muted == mute)
		goto exit_unlock;
	ctx->muted = mute;

	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		mix_ch_gain[i] = mute ? SOC_AUDIO_MIX_COEFF_INDEX_MUTE :
		    ctx->post_mix_ch_gain[i];
	}
	result = audio_set_channel_gain(ctx, mix_ch_gain);
exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Enable and set the volume of each channel of the mixed output of the audio
 * processor. */
enum soc_result
soc_audio_set_per_channel_volume(uint32_t processor_h,
				 struct soc_audio_per_channel_volume
				 channel_volume)
{
	int32_t i, volume;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);

	ctx->per_channel_volume_enable = true;
	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		switch (i) {
		case SOC_AUDIO_CHANNEL_LEFT:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				front_left);
			break;
		case SOC_AUDIO_CHANNEL_RIGHT:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				front_right);
			break;
		case SOC_AUDIO_CHANNEL_CENTER:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				center);
			break;
		case SOC_AUDIO_CHANNEL_LFE:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				lfe);
			break;
		case SOC_AUDIO_CHANNEL_LEFT_SUR:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				surround_left);
			break;
		case SOC_AUDIO_CHANNEL_RIGHT_SUR:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				surround_right);
			break;
		case SOC_AUDIO_CHANNEL_LEFT_REAR_SUR:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				rear_surround_left);
			break;
		case SOC_AUDIO_CHANNEL_RIGHT_REAR_SUR:
			ctx->per_channel_volume[i]
				= SOC_AUDIO_ENSURE_VALID_GAIN(channel_volume.\
				rear_surround_right);
			break;
		default:
			result = SOC_FAILURE;
			goto exit_unlock;
		}
	}

	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		if (ctx->master_volume_enable) {
			volume =
			    SOC_AUDIO_ENSURE_VALID_GAIN(ctx->per_channel_volume
							[i] +
							ctx->master_volume);
		} else
			volume = ctx->per_channel_volume[i];

		result = audio_user_gain_to_coeff_index(volume,
							&ctx->post_mix_ch_gain
							[i]);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Mixer gain parameters is out of range!\n");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit_unlock;
		}
	}
	if (!ctx->muted)
		result = audio_set_channel_gain(ctx, ctx->post_mix_ch_gain);

exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Retrieve the per-channel volume control settings of the mixed output
 * of the audio processor */
enum soc_result soc_audio_get_per_channel_volume(uint32_t processor_h,
				struct soc_audio_per_channel_volume
				 *channel_volume)
{
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;
	int i;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if ((SOC_SUCCESS != result) || !channel_volume)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);

	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
		switch (i) {
		case SOC_AUDIO_CHANNEL_LEFT:
			channel_volume->front_left =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_RIGHT:
			channel_volume->front_right =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_CENTER:
			channel_volume->center =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_LFE:
			channel_volume->lfe =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_LEFT_SUR:
			channel_volume->surround_left =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_RIGHT_SUR:
			channel_volume->surround_right =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_LEFT_REAR_SUR:
			channel_volume->rear_surround_left =
				ctx->per_channel_volume[i];
			break;
		case SOC_AUDIO_CHANNEL_RIGHT_REAR_SUR:
			channel_volume->rear_surround_right =
				ctx->per_channel_volume[i];
			break;
		default:
			result = SOC_FAILURE;
		}
	}

	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/* Disable per-channel volume control of the mixed output */
enum soc_result soc_audio_disable_per_channel_volume(uint32_t processor_h)
{
	int32_t i, volume;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto exit;

	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);

	ctx->per_channel_volume_enable = false;
	if (ctx->master_volume_enable) {
		result =
		    audio_user_gain_to_coeff_index(ctx->master_volume, &volume);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Mixer gain parameters is out of range!\n");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto exit_unlock;
		}
	} else {
		volume = SOC_AUDIO_MIX_COEFF_INDEX_0_DB;
	}
	for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++)
		ctx->post_mix_ch_gain[i] = volume;
	if (!ctx->muted)
		result = audio_set_channel_gain(ctx, ctx->post_mix_ch_gain);

exit_unlock:
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

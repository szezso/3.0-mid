/*
* File Name: soc_audio_inputs.c
*/

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

/******************************************************************************/
/* Header Files */
/******************************************************************************/

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "soc_audio_api.h"
#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_ipc.h"
#include "soc_debug.h"
#include "soc_audio_defs.h"
#include "soc_audio_mpeg_config.h"
#include "soc_audio_aac_config.h"
/*
#include "soc_audio_truehd_config.h"
#include "soc_audio_ac3_config.h"
#include "soc_audio_ddplus_config.h"
#include "soc_audio_dts_config.h"
#include "soc_audio_dts_hd_config.h"
#include "soc_audio_dts_lbr_config.h"
*/
/******************************************************************************/
/* Audio Input Private function declarations */
/******************************************************************************/
static enum soc_result
audio_input_set_to_play(struct soc_audio_processor_context *ctx,
			struct soc_audio_input_wl *input_wl);
static enum soc_result
audio_input_set_to_stop(struct soc_audio_processor_context *ctx,
			struct soc_audio_input_wl *input_wl);
static enum soc_result
audio_input_set_to_pause(struct soc_audio_processor_context *ctx,
			 struct soc_audio_input_wl *input_wl);
static bool audio_input_valid_sample_size(int sample_size);

/******************************************************************************/
/* Audio Input API function Implementation */
/******************************************************************************/

/*
* This API is used to add an input
*/
enum soc_result soc_audio_input_add(uint32_t processor_h, void **handle)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_input_wl *new_input = NULL;
	struct soc_audio_input_wl *prev_input = NULL;
	struct soc_audio_input_wl *curr_input = NULL;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get the audio processor context and get the lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{
		/* Processor should be initialized */
		if (ctx->ref_count == 0) {
			soc_debug_print(ERROR_LVL, "Processor uninitialized");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}

		/* Allocate and initialize the new input_wl structure */
		new_input = (struct soc_audio_input_wl *)
		    kmalloc(sizeof(struct soc_audio_input_wl), GFP_KERNEL);
		if (NULL == new_input) {
			soc_debug_print(ERROR_LVL, "Can not alloc new input");
			result = SOC_ERROR_NO_RESOURCES;
			goto EXIT_UNLOCK;
		}
		memset(new_input, 0, sizeof(struct soc_audio_input_wl));
		new_input->state = SOC_DEV_STATE_DISABLE;
		new_input->ctx = ctx;
		new_input->format = SOC_AUDIO_MEDIA_FMT_PCM;
		new_input->next_input = NULL;

		/* Add this input to our processor */
		prev_input = ctx->inputs;
		curr_input = ctx->inputs;
		if (NULL == curr_input) {
			ctx->inputs = new_input;
		} else {	/* traverse and add at end of list */
			while (curr_input) {
				prev_input = curr_input;
				curr_input = curr_input->next_input;
			}
			prev_input->next_input = new_input;
		}

		/* Update internal structures to reflect new input port */
		ctx->input_count++;

		result = SOC_SUCCESS;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	/* Return handle only if success */
	if (SOC_SUCCESS == result)
		*handle = (void *)new_input;
	SOC_EXIT();
	return result;
}

/*
This API is used to set the state of an input
 */
enum soc_result soc_audio_input_set_state(uint32_t processor_h,
					  void *handle,
					  enum soc_dev_state state)
{
	enum soc_result result = SOC_SUCCESS;
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* Check if processor handle and states are valid */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;
	if (state != SOC_DEV_STATE_PAUSE &&
	state != SOC_DEV_STATE_STOP && state != SOC_DEV_STATE_PLAY) {
		soc_debug_print(ERROR_LVL, "Invalid state: %d", state);
		result = SOC_FAILURE;
		goto EXIT;
	}

	/* Get the audio processor context and get the lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{
		/* Get the input workload */
		input_wl = (struct soc_audio_input_wl *)handle;
		/* Validate input workload */
		result = audio_input_workload_validation(ctx, input_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* The application must call set_data_format() before reaching
		 * here. The way to tell it is to varify the format in input
		 * workload. */
		if (SOC_AUDIO_MEDIA_FMT_INVALID == input_wl->format) {
			soc_debug_print(ERROR_LVL, "Invalid format. Needs to "
					"set the data format.\n ");
			result = SOC_FAILURE;
			goto EXIT_UNLOCK;
		}

		/* Ignore this operation if no state changes  */
		if (state == input_wl->state)
			goto EXIT_UNLOCK;

		/* BU specific actions. */
		/* this function must be placed before set_to_play. */
		result = soc_bu_audio_input_set_state(ctx, input_wl, state);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"soc_bu_audio_input_set_state failed.\n ");
		}

		/* Call the state specific functions */
		switch (state) {
		case SOC_DEV_STATE_PAUSE:
			result = audio_input_set_to_pause(ctx, input_wl);
			break;
		case SOC_DEV_STATE_STOP:
			result = audio_input_set_to_stop(ctx, input_wl);
			break;
		case SOC_DEV_STATE_PLAY:
			result = audio_input_set_to_play(ctx, input_wl);
			break;
		case SOC_DEV_STATE_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			break;
		}

		/* Set the new state */
		if (SOC_SUCCESS == result) {
			input_wl->state = state;
		} else {
			soc_debug_print(ERROR_LVL,
					"Set state: %d failed with %d\n", state,
					result);
		}
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/*
* This API removes an input from the currently playing stream. This
* configuration command can be issued at any time. It will remove a previously
* added input stream and not disrupt the playback of any other input streams.
*/
enum soc_result soc_audio_input_remove(uint32_t processor_h, void *handle)
{

	enum soc_result result = SOC_FAILURE;
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get the audio processor context and lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{
		/* Get the input workload */
		input_wl = (struct soc_audio_input_wl *)handle;
		/* Validate input workload */
		result = audio_input_workload_validation(ctx, input_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Remove the input workload */
		result = audio_input_remove(ctx, input_wl);
		if (SOC_SUCCESS != result) {
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}

		/* If it is the last input, we need to stop the post processing
		pipeline also. post processing pipeline is the post processing
		pipeline and is required for all other audio format. */
		if (0 == ctx->input_count) {
			/* Stop the post processing pipeline */
			if (ctx->pipeline->started) {
				result = soc_ipc_stop_pipe(ctx, ctx->pipeline);
				if (SOC_SUCCESS != result) {
					soc_debug_print(ERROR_LVL,
				"Cannot stop post processing pipeline\n");
					goto EXIT_UNLOCK;
				}
				ctx->pipeline->started = false;
				soc_ipc_flush_pipe(ctx, ctx->pipeline);
				if (SOC_SUCCESS != result) {
					soc_debug_print(ERROR_LVL,
					"Cannot flush post processing"
					" pipeline\n");
					goto EXIT_UNLOCK;
				}
			}
		}
		result = SOC_SUCCESS;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/* Remove input workload.
 * This function need to be protected by processor lock.
 * Caller function must make sure the arguments are valid. */
enum soc_result
audio_input_remove(struct soc_audio_processor_context *ctx,
		   struct soc_audio_input_wl *input_wl)
{
	enum soc_result result = SOC_SUCCESS;
	struct soc_audio_input_wl *prev_input = NULL;
	struct soc_audio_input_wl *curr_input = NULL;

	SOC_ENTER();

	/* Find the input */
	prev_input = ctx->inputs;
	curr_input = ctx->inputs;
	while (input_wl != curr_input) {
		prev_input = curr_input;
		curr_input = curr_input->next_input;
	}

	/* remove the input from the current input list */
	/* Decode pipeline is removed only if it is not a PCM input */
	if (curr_input->pipeline && (
#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
		(PIPE_OPS_CAPTURE == curr_input->pipeline->op_type) ||
#endif
		(curr_input->format != SOC_AUDIO_MEDIA_FMT_PCM))) {
		soc_debug_print(DEBUG_LVL_1, "audio_input_remove: %d PCM:%d, "
				"%d OPS: %d\n",
			   curr_input->format, SOC_AUDIO_MEDIA_FMT_PCM,
			   curr_input->pipeline->op_type, PIPE_OPS_CAPTURE);
		/* Stop and free the decode pipe */
		if (curr_input->pipeline->started) {
			result = soc_ipc_stop_pipe(ctx, curr_input->pipeline);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
				"Cannot stop input pipeline\n");
				/* Failure, try to go ahead to
				   clean as much as possible */
			}
			curr_input->pipeline->started = false;
			result = soc_ipc_flush_pipe(ctx, curr_input->pipeline);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
				"Cannot flush decode pipeline\n");
				/* Failure, try to go ahead to
				   clean as much as possible */
			}
		}

		result = soc_ipc_free_pipe(ctx, curr_input->pipeline);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Cannot ipc free decode pipeline\n");
			/* Failure, try to go ahead to
			   clean as much as possible */
		}
		result = soc_audio_pipeline_delete(ctx, &curr_input->pipeline);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Cannot delete decode pipeline\n");
			/* Failure, try to go ahead to
			   clean as much as possible */
		}
	}
	/* Take out the input workload from list */
	if (curr_input == ctx->inputs)
		ctx->inputs = ctx->inputs->next_input;
	else
		prev_input->next_input = curr_input->next_input;

	/* Update internal structures to reflect the removal */
	kfree(curr_input);
	ctx->input_count--;
	if (0 == ctx->input_count)
		ctx->inputs = NULL;
	SOC_EXIT();
	return result;
}

/*
This API is used to set the data format of an input
 */
enum soc_result soc_audio_input_set_data_format(uint32_t processor_h,
						void *handle,
						enum soc_audio_ops_type type,
						enum soc_audio_format format)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_audio_format previous_format = SOC_AUDIO_MEDIA_FMT_INVALID;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get processor context and input work load, get the lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{
		input_wl = (struct soc_audio_input_wl *)handle;
		/* Validate input workload */
		result = audio_input_workload_validation(ctx, input_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* We can not set format while playing */
		if (SOC_DEV_STATE_PLAY == input_wl->state) {
			soc_debug_print(ERROR_LVL,
			"Setting up input data format while in 'PLAY' state");
			result = SOC_FAILURE;
			goto EXIT_UNLOCK;
		}

		/* If same format is already set, do not reconfigure */
		previous_format = input_wl->format;
		if ((previous_format == format)
#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
			&& (PIPE_OPS_PLAYBACK == type)
#endif
			) {
			result = SOC_SUCCESS;
			goto EXIT_UNLOCK;
		}

		/* Here is the place we setup format of a input workload */
		input_wl->format = format;

		/*
		 * If the format is not PCM, then tear the old decode pipe if
		 * one exists. If the pipeline does not exit, the delete
		 * pipeline will just return with no effect.
		 */
		soc_audio_pipeline_delete(ctx, &input_wl->pipeline);

		/*
		 * Setup appropriate pipeline. For PCM we do not need a decode
		 * pipeline. For all other formats, we need a decode pipeline.
		 * (TBD: maybe decode_pcm pipe is needed)
		 */
		if (SOC_AUDIO_MEDIA_FMT_PCM == input_wl->format) {
#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
			if (PIPE_OPS_CAPTURE == type) {
				result = audio_capture_pipe_setup(input_wl);
				if (SOC_SUCCESS != result) {
					soc_debug_print(ERROR_LVL,
					"Capture pipeline setup failed");
				}
			} else
				result = SOC_SUCCESS;
#else
				result = SOC_SUCCESS;
#endif
		} else {
			result = audio_dec_pipe_setup(input_wl);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
				"Decode pipeline setup failed");
			}
		}
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/*
This API is used set a specific decoder parameter.
*/
enum soc_result soc_audio_input_set_decoder_param(uint32_t processor_h,
						  void *input_handle,
						  uint32_t param_type,
						  void *param_value)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_audio_format format;
	struct soc_audio_pipeline *pipe_instance = NULL;

	SOC_ENTER();

	/* Should not have NULL param */
	if (NULL == param_value) {
		result = SOC_ERROR_INVALID_PARAMETER;
		soc_debug_print(ERROR_LVL, "NULL param\n");
		WARN_ON(1);
		goto EXIT;
	}

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get processor context and lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{
		/* Get input workload */
		input_wl = (struct soc_audio_input_wl *)input_handle;
		/* Validate input workload */
		result = audio_input_workload_validation(ctx, input_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Get format */
		/* If DTS core is in the input stream, but only DTSHD decoder,
		 * need to force format to DTSHD since we will use the DTSHD
		 * decoder. */
		if (input_wl->format == SOC_AUDIO_MEDIA_FMT_DTS)
			format = SOC_AUDIO_MEDIA_FMT_DTS_HD;
		else
			format = input_wl->format;

		/* TBD: !!! ms10 format not implemented yet !!!! */

		/* Check the parameters according to their formats */
		switch (format) {
		case SOC_AUDIO_MEDIA_FMT_PCM:
		case SOC_AUDIO_MEDIA_FMT_DVD_PCM:
		case SOC_AUDIO_MEDIA_FMT_BLURAY_PCM:
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL, "Codec parameters not set,"
				"format is PCM!");
			break;
		case SOC_AUDIO_MEDIA_FMT_DD:
#ifdef INCLUDE_AC3_DECODE
			result =
			    audio_ac3_set_dec_param(input_wl, param_type,
						    param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_DD_PLUS:
#ifdef INCLUDE_DDPLUS_DECODE
			result =
			    audio_ddplus_set_dec_param(input_wl, param_type,
						       param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_DTS:
#ifdef INCLUDE_DTS_DECODE
			result =
			    audio_dts_set_dec_param(input_wl, param_type,
						    param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_DTS_LBR:
#ifdef INCLUDE_DTS_LBR_DECODE
			result =
			    audio_dts_lbr_set_dec_param(input_wl, param_type,
							param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_DTS_BC:
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		case SOC_AUDIO_MEDIA_FMT_MPEG:
#ifdef INCLUDE_MPEG_DECODE
			result =
			    audio_mpeg_set_dec_param(input_wl, param_type,
						     param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_WM9:
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		case SOC_AUDIO_MEDIA_FMT_TRUE_HD:
#ifdef INCLUDE_TRUEHD_DECODE
			result =
			    audio_truehd_set_dec_param(input_wl, param_type,
						       param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_DTS_HD:
		case SOC_AUDIO_MEDIA_FMT_DTS_HD_HRA:
		case SOC_AUDIO_MEDIA_FMT_DTS_HD_MA:
#ifdef INCLUDE_DTS_HD_DECODE
			result =
			    audio_dts_hd_set_dec_param(input_wl, param_type,
						       param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		case SOC_AUDIO_MEDIA_FMT_AAC:
		case SOC_AUDIO_MEDIA_FMT_AAC_LOAS:
#ifdef INCLUDE_AAC_DECODE
			result =
			    audio_aac_set_dec_param(input_wl, param_type,
						    param_value);
#else
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
			break;
		default:
		case SOC_AUDIO_MEDIA_FMT_COUNT:
		case SOC_AUDIO_MEDIA_FMT_INVALID:
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL,
				"No decoder is setup on the input!");
			break;
		}
		if (SOC_ERROR_FEATURE_NOT_SUPPORTED == result) {
			soc_debug_print(ERROR_LVL, "Format %d not supported",
					format);
			goto EXIT_UNLOCK;
		} else if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Can not write codec parameter to pipeline");
			goto EXIT_UNLOCK;
		}

	}

	pipe_instance = input_wl->pipeline;
	ctx = input_wl->ctx;

	if (pipe_instance->started) {
		soc_debug_print(ERROR_LVL,
			"Trying to change params of a pipe in progress,"
			"not configuring now\n");
		result = SOC_SUCCESS;
		goto EXIT_UNLOCK;
	}

	pipe_instance->configured = false;
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/*
This API is used to set input PCM sampling rate,
sampling size and channel config.
*/
enum soc_result soc_audio_input_set_pcm_stream_info(void *in_handle, struct
						    soc_audio_stream_info
						    *stream_info)
{
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate input handle and stream info */
	if (!in_handle || !stream_info) {
		soc_debug_print(ERROR_LVL, "Invalid input handle");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto exit;
	}
	input_wl = (struct soc_audio_input_wl *)in_handle;

	/* Get processor context */
	ctx = input_wl->ctx;

	/* Validate the sample rate, sample size, and channel config */
	if (!soc_audio_core_is_valid_sample_rate(stream_info->sample_rate)) {
		result = SOC_ERROR_INVALID_PARAMETER;
		goto exit;
	}
	if (!audio_input_valid_sample_size(stream_info->sample_size)) {
		result = SOC_ERROR_INVALID_PARAMETER;
		goto exit;
	}
	if (!soc_audio_core_is_valid_ch_config(stream_info->channel_config)) {
		soc_debug_print(WARNING_LVL, "Warning: Invalid channel "
			"configuration supplied, assuming 2ch default!\n");
		stream_info->channel_config = SOC_AUDIO_CHANNEL_CONFIG_2_CH;
	}
	stream_info->channel_count =
	    soc_audio_core_get_channel_cnt_from_config
	    (stream_info->channel_config);

	if (SOC_DEV_STATE_PLAY == input_wl->state) {
		soc_debug_print(ERROR_LVL,
		"Setting up input sample rate while in 'PLAY' state\n");
		result = SOC_FAILURE;
		goto exit;
	}
	mutex_lock(&ctx->processor_lock);
	input_wl->sample_rate = stream_info->sample_rate;
	input_wl->sample_size = stream_info->sample_size;
	input_wl->channel_config = stream_info->channel_config;
	input_wl->channel_count = stream_info->channel_count;
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/*
Get bit rate, sample rate, sample size, channel config and audio format
(algorithm) for the incoming stream returned all in one structure.
*/
enum soc_result
soc_audio_input_get_stream_info(void *in_handle,
				struct soc_audio_stream_info *stream_info)
{
	struct soc_audio_format_specific_info fmt_info;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	result = soc_audio_input_get_format_specific_info(in_handle, &fmt_info);
	if (result == SOC_SUCCESS)
		*stream_info = fmt_info.basic_stream_info;
	SOC_EXIT();
	return result;
}

/*
Get extened format specific stream information of the current input stream.
*/
enum soc_result soc_audio_input_get_format_specific_info(void *in_handle,
	struct soc_audio_format_specific_info  *fmt_info)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_pipeline *pipeline = NULL;
	struct soc_audio_stream_info *stream_info =
	    &fmt_info->basic_stream_info;
	uint32_t stage_handle = 0;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	if (!in_handle || !stream_info) {
		soc_debug_print(ERROR_LVL, "Invalid args!");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto exit;
	}
	/* Get the associated context */
	input_wl = (struct soc_audio_input_wl *)in_handle;

	ctx = input_wl->ctx;

	mutex_lock(&ctx->processor_lock);
	switch (input_wl->format) {
	case SOC_AUDIO_MEDIA_FMT_PCM:
		stream_info->algo = SOC_AUDIO_MEDIA_FMT_PCM;
		stream_info->sample_rate = input_wl->sample_rate;
		stream_info->sample_size = input_wl->sample_size;
		stream_info->channel_config = input_wl->channel_config;
		stream_info->channel_count = input_wl->channel_count;
		stream_info->bitrate =
		    input_wl->sample_size * input_wl->sample_rate *
		    input_wl->channel_count;
		break;
	case SOC_AUDIO_MEDIA_FMT_DVD_PCM:
	case SOC_AUDIO_MEDIA_FMT_BLURAY_PCM:
		break;
	default:
		break;
	}
	if (pipeline) {
		result = soc_ipc_stage_get_params((void *)ctx, pipeline,
						stage_handle);
		if (result == SOC_SUCCESS) {
			switch (stage_handle) {
			case SOC_AUDIO_DEC_DECODE_STAGE:
			default:
				break;
			}
		}
	}
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/*
This api flushes the stream out of the pipeline after it is stopped
*/
enum soc_result
soc_audio_input_flush_stream(uint32_t processor_h, void *handle)
{

	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_input_wl *input_wl = NULL;
	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();
	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get the audio processor context and lock */
	ctx = &soc_audio_processor[processor_h];
	mutex_lock(&ctx->processor_lock);
	{

		/* Get the input workload */
		input_wl = (struct soc_audio_input_wl *)handle;
		/* Validate input workload */
		result = audio_input_workload_validation(ctx, input_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Flush the decode pipe if one is present */
		if (input_wl->format != SOC_AUDIO_MEDIA_FMT_PCM) {
			result = soc_ipc_flush_pipe(ctx, input_wl->pipeline);

			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
				"Cannot flush input pipeline\n");
				goto EXIT_UNLOCK;
			}
		}
		/* Flush the post proc pipe */
		result = soc_ipc_flush_pipe(ctx, ctx->pipeline);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"Cannot flush input pipeline\n");
			goto EXIT_UNLOCK;
		}

	}

EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);

EXIT:
	SOC_EXIT();
	return result;

}

/*
Sets the input to be the primary input and will be the source for outputs
set in pass through mode.
*/
enum soc_result
soc_audio_input_set_as_primary(void *in_handle,
			       int32_t input_index, bool pass_through)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_input_wl *input_wl = NULL;
#ifdef INCLUDE_MIXER_STAGE
	struct soc_audio_mixer_params *mixer_params;
#endif
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	if (!in_handle) {
		soc_debug_print(ERROR_LVL, "Invalid args!");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto exit;
	}
	/* Get the associated context */
	input_wl = (struct soc_audio_input_wl *)in_handle;

	ctx = input_wl->ctx;

	/* Now only deal with Mixer stage */
	mutex_lock(&ctx->processor_lock);
#ifdef INCLUDE_MIXER_STAGE
	mixer_params =
	    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.mixer;
	if (mixer_params->host.primary_index != input_index) {
		mixer_params->host.primary_index = input_index;
		if (ctx->pipeline->configured) {
			result =
			    soc_ipc_stage_configure(ctx, ctx->pipeline,
						    SOC_AUDIO_MIXER_STAGE);
		}
	}
#endif
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/*
Sets the input to be the secondary input on the processor.
*/
enum soc_result
soc_audio_input_set_as_secondary(void *in_handle, int32_t input_index)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_input_wl *input_wl = NULL;
#ifdef INCLUDE_MIXER_STAGE
	struct soc_audio_mixer_params *mixer_params;
#endif
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();
	if (!in_handle) {
		soc_debug_print(ERROR_LVL, "Invalid args!");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto exit;
	}
	/* Get the associated context */
	input_wl = (struct soc_audio_input_wl *)in_handle;

	ctx = input_wl->ctx;

	/* Now only deal with Mixer stage */
	mutex_lock(&ctx->processor_lock);
#ifdef INCLUDE_MIXER_STAGE
	mixer_params =
	    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].stage_params.mixer;
	if (mixer_params->host.secondary_index != input_index) {
		mixer_params->host.secondary_index = input_index;
		if (ctx->pipeline->configured) {
			result =
			    soc_ipc_stage_configure(ctx, ctx->pipeline,
						    SOC_AUDIO_MIXER_STAGE);
		}
	}
#endif
	mutex_unlock(&ctx->processor_lock);
exit:
	SOC_EXIT();
	return result;
}

/******************************************************************************/
/* Audio Input Private function definition */
/******************************************************************************/

/*
* This is used to set an input to play.
* Please take a Lock and check input arguents before calling this function.
* While driver reach here, we know that we had post procesing pipeline
* allocated, mabye configured, and the decode pipeline configured.
*/
static enum soc_result audio_input_set_to_play(struct
					       soc_audio_processor_context
					       *ctx, struct
					       soc_audio_input_wl
					       *input_wl)
{
	enum soc_result result = SOC_SUCCESS;
	SOC_ENTER();

	/* Do nothing if the pipeline has already started */
	if ((input_wl->state == SOC_DEV_STATE_PAUSE) ||
	    (input_wl->state == SOC_DEV_STATE_PLAY))
		goto EXIT;

	/* If pipeline is attached to the input, configure the pipeline */
	if (input_wl->pipeline) {
		struct soc_audio_pipeline *pipe_instance = input_wl->pipeline;
		if (!input_wl->pipeline->configured) {
			/* Configure the input pipeline */
			result = soc_ipc_config_pipe(ctx, pipe_instance);
			if (result != SOC_SUCCESS) {
				soc_audio_pipeline_delete(ctx, &pipe_instance);
				soc_debug_print(ERROR_LVL,
					"configure input pipeline failed.\n");
				goto EXIT;
			}
			pipe_instance->configured = true;
		}
	}

	/* post processing pipeline should always be configured here and
	 * started here. The reason is because we need to wait add
	 * output finised to get necessary information for setting up
	 * post processing pipeline. While you hit the play the second
	 * time, the post processing pipeline should be configured
	 * already, and we only need to notify FW to start the pipe.*/
	if (!ctx->pipeline->configured) {
		result = audio_post_proc_pipe_setup(ctx);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
					"Cannot configure post pipe\n");
			result = SOC_FAILURE;
			goto EXIT;
		}
	}

	/* Ask FW do clock switching according to
	the current configuration */
	soc_ipc_switch_clock(ctx, ctx->pipeline);

	/* Start the input pipeline if attached*/
	if (input_wl->pipeline) {
		struct soc_audio_pipeline *pipe_instance = input_wl->pipeline;
		/* The input pipeline cannot be in the start state. */
		if (pipe_instance->started) {
			soc_debug_print(ERROR_LVL,
					"input Pipeline was started");
			result = SOC_FAILURE;
			goto EXIT;
		}
		/* start the pipeline */
		result = soc_ipc_start_pipe(ctx, pipe_instance);
		if (result != SOC_SUCCESS) {
			soc_ipc_free_pipe(ctx, pipe_instance);

			/* bu specific diconnect called from pipeline_delete */
			soc_audio_pipeline_delete(ctx, &pipe_instance);
			soc_debug_print(ERROR_LVL,
					"start input pipeline failed\n");
			goto EXIT;
		}
		pipe_instance->started = true;
	}

	/* Start the post processing pipeline if it is necessary. post
	 * processing pipeline maybe already enabled for other input.*/
	if (input_wl->pipeline
		&& (PIPE_OPS_CAPTURE == input_wl->pipeline->op_type)) {
		/* Since currently there is no connection between capture and
		 * post processing pipeline, we return without starting the
		 * post processing pipeline. */
			goto EXIT;
	} else {
		/* Start the pipeline */
		if (!ctx->pipeline->started) {
			result = audio_post_proc_pipe_start(ctx);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
						"Cannot start post pipeline\n");
				result = SOC_FAILURE;
				goto EXIT;
			}
		}
	}
EXIT:
	SOC_EXIT();
	return result;
}

/*
* This is used to check if this is the last input to be stopped
* This is done by iterating through the input linked list and
* counting the number of inputs that are in SOC_DEV_STATE_PLAY state
*/
static bool is_this_the_last_input_to_be_stopped(struct
						 soc_audio_processor_context
						 *ctx)
{
	bool is_last = false;
	int i, active_input_count = 0;
	struct soc_audio_input_wl *temp_input_wl = ctx->inputs;
	for (i = 0; i < ctx->input_count; i++) {
		/* Don't count capture inputs as this function is used
		 * for PCM playback inputs only */
		if (temp_input_wl->pipeline &&
		    PIPE_OPS_CAPTURE == temp_input_wl->pipeline->op_type) {
			temp_input_wl = temp_input_wl->next_input;
			continue;
		}
		if ((temp_input_wl->state == SOC_DEV_STATE_PLAY) ||
		    (temp_input_wl->state == SOC_DEV_STATE_PAUSE))
			active_input_count++;
		temp_input_wl = temp_input_wl->next_input;
	}

	if (active_input_count == 1)
		is_last = true;

	return is_last;
}

/*
* This is used to set an input to stop
* Please take a Lock and check input arguents before calling this function.
*/
static enum soc_result audio_input_set_to_stop(struct
					       soc_audio_processor_context
					       *ctx, struct
					       soc_audio_input_wl
					       *input_wl)
{
	enum soc_result result = SOC_SUCCESS;
	SOC_ENTER();

	if (input_wl->state == SOC_DEV_STATE_STOP)
		goto EXIT;
	/* Stop input pipeline if attached */
	if (input_wl->pipeline && input_wl->pipeline->started) {
		result = soc_ipc_stop_pipe(ctx, input_wl->pipeline);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL, "Cannot stop pipe\n");
				goto EXIT;
		}
		input_wl->pipeline->started = false;
		soc_ipc_flush_pipe(ctx, input_wl->pipeline);
	}

	/* stop the post processing pipeline if it is necessary. post
	 * processing pipeline maybe already enabled for other input.*/
	if (input_wl->pipeline
		&& (PIPE_OPS_CAPTURE == input_wl->pipeline->op_type)) {
		/* Don't stop post processing pipeline since there is no
		 * connection to capture pipeline. */
		goto EXIT;
	} else {
		if (ctx->pipeline->started
				&& is_this_the_last_input_to_be_stopped(ctx)) {
			result = soc_ipc_stop_pipe(ctx, ctx->pipeline);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
						"Cannot stop pipe\n");
				goto EXIT;
			}
			ctx->pipeline->started = false;
			soc_ipc_flush_pipe(ctx, ctx->pipeline);
		}
	}

EXIT:
	/* Ask FW do clock switching according to the current configuration */
	soc_ipc_switch_clock(ctx, ctx->pipeline);
	SOC_EXIT();
	return result;
}

/*
* This is used to set an input to pause
* Please take a Lock and check input arguents before calling this function.
*/
static enum soc_result audio_input_set_to_pause(struct
						soc_audio_processor_context
						*ctx, struct
						soc_audio_input_wl
						*input_wl)
{
	enum soc_result result = SOC_SUCCESS;
	SOC_ENTER();
	ctx = ctx;
	input_wl = input_wl;

	SOC_EXIT();
	return result;
}

static bool audio_input_valid_sample_size(int sample_size)
{
	bool result = false;

	switch (sample_size) {
	case 8:
	case 16:
	case 20:
	case 24:
	case 32:
		result = true;
		break;
	default:
		soc_debug_print(ERROR_LVL,
				"Sample size (%d) not supported!", sample_size);
		break;
	}
	return result;
}

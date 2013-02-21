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
/****************************************************************************/
/* Header Files */
/****************************************************************************/

#include <linux/string.h>
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
static enum soc_result
audio_pipe_set_default_decoder_param(struct soc_audio_input_wl *input_wl);

/**
This function allocates resources needed for a pipeline and initializes it.
 */
struct soc_audio_pipeline *soc_audio_pipeline_add(uint32_t
						  stages, uint32_t type)
{
	struct soc_audio_pipeline *pipe_instance = NULL;
	uint32_t mem_size = 0, i = 0;
	void *context = NULL;

	SOC_ENTER();
	if (stages < 1) {
		soc_debug_print(ERROR_LVL, "pipeline without any stages\n");
		goto soc_pipeline_add_exit;
	}

	/* find the size of structure to be allocated
	   soc_audio_pipeline structure already has one stage(to follow
	   array notation) so allocate n-1 stage memory */

	mem_size = sizeof(struct soc_audio_pipeline) +
	    (sizeof(struct soc_audio_pipeline_stage) * (stages - 1));

	soc_debug_print(DEBUG_LVL_2, "Memory requested %d\n", mem_size);

	/* we allocate the memory from bu specific memory allocater */
	pipe_instance = (struct soc_audio_pipeline *)
	    soc_audio_g_init_data.alloc_mem(mem_size, &context);

	if (pipe_instance == NULL) {
		soc_debug_print(ERROR_LVL,
		"Memory allocation failure for pipe.\n");
		goto soc_pipeline_add_exit;
	}
	/* zero out initial allocation */
	memset(pipe_instance, 0, mem_size);

	pipe_instance->started = false;
	pipe_instance->configured = false;
	/* Afix the bu specified pointer in pipeline, later we
	   call the free with this pointer */
	pipe_instance->context = context;
	pipe_instance->type = type;
	pipe_instance->num_stages = stages;

	/* Initialize all stages */
	for (i = 0; i < stages; i++) {
		pipe_instance->stages[i].in_use = false;
		pipe_instance->stages[i].inputs_count = 0;
		pipe_instance->stages[i].outputs_count = 0;
	}

soc_pipeline_add_exit:
	SOC_EXIT();

	return pipe_instance;
}

/**
This function de-allocates all the resources for a pipeline. It should be
called after the pipeline has been stopped.
 */
enum soc_result soc_audio_pipeline_delete(struct
					  soc_audio_processor_context
					  *ctx, struct soc_audio_pipeline
					  **pipe_instance)
{
	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();

	if (pipe_instance && *pipe_instance) {
		/*Proceed to deallocate stage buffers only if there
		is a output present. As the last output removal would
		have de-allocated all the buffers
		in pipe reconfigure.*/
		if (0 != ctx->output_count)
			soc_bu_audio_pipe_disconnect(*pipe_instance);
		(*pipe_instance)->configured = false;
		/* Freeing memory context */
		soc_audio_g_init_data.free_mem((*pipe_instance)->context);
		*pipe_instance = NULL;
		result = SOC_SUCCESS;
	}
	SOC_EXIT();
	return result;
}

/**
This function calculates number of outputs needed for a pipeline
 */
void audio_pipeline_read_output_configs(struct
					soc_audio_processor_context
					*ctx, struct soc_audio_pipe_out
					*out_cfg)
{
	struct soc_audio_output_wl *curr_output = NULL;
	uint32_t loop = 0;
	SOC_ENTER();

	out_cfg->count = 0;
	out_cfg->ac3_encoded = false;
	out_cfg->dts_encoded = false;
	out_cfg->matrix_dec_enabled = false;

	curr_output = ctx->outputs;
	while (curr_output != NULL) {
		switch (curr_output->out_mode) {
		case SOC_AUDIO_OUTPUT_PCM:
		case SOC_AUDIO_OUTPUT_PASSTHROUGH:
			/* no action required */
			break;
		case SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL:
			out_cfg->ac3_encoded = true;
			break;
		case SOC_AUDIO_OUTPUT_ENCODED_DTS:
			out_cfg->dts_encoded = true;
			break;
		case SOC_AUDIO_OUTPUT_INVALID:
		default:
			soc_debug_print(ERROR_LVL, "Invalid output mode\n");
			break;
		}
		out_cfg->cfg[loop].output_handle = loop;
		out_cfg->cfg[loop].in_use = true;
		out_cfg->cfg[loop].ch_config = curr_output->channel_config;
		out_cfg->cfg[loop].sample_size = curr_output->sample_size;
		out_cfg->cfg[loop].sample_rate = curr_output->sample_rate;
		out_cfg->cfg[loop].channel_count = curr_output->channel_count;

		/*Get the dmix standard from the output*/
		out_cfg->cfg[loop].out_mode = curr_output->out_mode;
		out_cfg->cfg[loop].dmix_mode = curr_output->dmix_mode;

		out_cfg->cfg[loop].ch_map = curr_output->ch_map;

		curr_output = curr_output->next_output;
		out_cfg->count++;
		loop++;
	}
	/*If we have no outputs,setup a default pipe so data will keep flowing*/
	if (out_cfg->count == 0) {
		soc_debug_print(WARNING_LVL,
			"No output associated , adding one by default\n");
		out_cfg->count = 1;
	}

	SOC_EXIT();
	return;
}

/* CHEN: TBD: should move to soc_audio_processor.c*/
/**
This function adds the basic stages needed in a pipeline
 */
enum soc_result audio_post_proc_pipe_set_post_mix_stages(const struct
							 soc_audio_pipe_out
							 *output_cfg, struct
							 soc_audio_pipeline
							 *pipe_instance)
{
	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();
#ifdef INCLUDE_DOWNMIXER_STAGE
	/* Downmix stage */
	pipe_instance->stages[SOC_AUDIO_DOWNMIX_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_DOWNMIX_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_DOWNMIXER_STAGE */

#ifdef INCLUDE_POSTSRC_STAGE
	/* Post SRC stage */
	pipe_instance->stages[SOC_AUDIO_POST_SRC_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_POST_SRC_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_POSTSRC_STAGE */

#ifdef INCLUDE_QUALITY_STAGE
	/* Audio Quality stage */
	pipe_instance->stages[SOC_AUDIO_QUALITY_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_QUALITY_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_QUALITY_STAGE */

#ifdef INCLUDE_INTERLEAVER_STAGE
	/* Need to preform an interleave */
	pipe_instance->stages[SOC_AUDIO_INTERLVR_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_INTERLVR_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_INTERLEAVER_STAGE */

#ifdef INCLUDE_PER_OUTPUT_DELAY_STAGE
	/* per output delay stage */
	pipe_instance->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_PER_OUTPUT_DELAY_STAGE */

#ifdef INCLUDE_OUTPUT_STAGE
	pipe_instance->stages[SOC_AUDIO_OUTPUT_STAGE].inputs_count =
	    output_cfg->count;
	pipe_instance->stages[SOC_AUDIO_OUTPUT_STAGE].outputs_count =
	    output_cfg->count;
#endif /* INCLUDE_OUTPUT_STAGE */

	/* Do BU specific parameter settings for each stage */
	result = soc_bu_audio_post_proc_pipe_init_stages(pipe_instance,
							 output_cfg);

	SOC_EXIT();
	return result;
}

/**
This function adds the basic stages needed in a pipeline
 */
static enum soc_result audio_dec_pipe_enable_stages(struct soc_audio_pipeline
						    *pipe_instance)
{

	SOC_ENTER();

#ifdef INCLUDE_INPUT_STAGE
	pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_IN;
	pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].outputs_count = 1;
#endif /* INCLUDE_INPUT_STAGE */
#ifdef INCLUDE_DECODE_STAGE
	pipe_instance->stages[SOC_AUDIO_DEC_DECODE_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_DECODE;
	pipe_instance->stages[SOC_AUDIO_DEC_DECODE_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_DEC_DECODE_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_DEC_DECODE_STAGE].outputs_count = 1;
#endif /* INCLUDE DECODE STAGE */
#ifdef INCLUDE_PRESRC_STAGE
	pipe_instance->stages[SOC_AUDIO_DEC_PRESRC_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_PRESRC;
	pipe_instance->stages[SOC_AUDIO_DEC_PRESRC_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_DEC_PRESRC_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_DEC_PRESRC_STAGE].outputs_count = 1;
#endif /* INCLUDE PRESRC STAGE */
#ifdef INCLUDE_OUTPUT_STAGE
	pipe_instance->stages[SOC_AUDIO_DEC_OUTPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_OUT;
	pipe_instance->stages[SOC_AUDIO_DEC_OUTPUT_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_DEC_OUTPUT_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_DEC_OUTPUT_STAGE].outputs_count = 1;
#endif /* INCLUDE_OUTPUT_STAGE */

	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This function sets up a post processing pipe
	1. add required stages
	2. connect the stages
	3. configures the pipeline
	4. starts the pipeline
 */
enum soc_result audio_post_proc_pipe_setup(
		struct soc_audio_processor_context *ctx)
{
	struct soc_audio_pipeline *pipe_instance = ctx->pipeline;
	struct soc_audio_pipe_out out_cfg;
	enum soc_result result;

	SOC_ENTER();
	memset(&out_cfg, 0, sizeof(struct soc_audio_pipe_out));
	/* read all the output and consolidate it at one place */
	audio_pipeline_read_output_configs(ctx, &out_cfg);

	pipe_instance->op_type = PIPE_OPS_PLAYBACK;
	/* Add Required Stages*/
	result =
	    audio_post_proc_pipe_set_post_mix_stages(&out_cfg, pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"post_proc_pipe_enable_stages failed result = %d\n",
			result);
		return result;
	}
	/* Connect the Stages*/
	result = soc_bu_audio_pipe_update_connect(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"BU specific function to connect stages failed\n");
		return result;
	}
	/* Configure the pipeleine */
	result = soc_ipc_config_pipe(ctx, pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_audio_pipeline_delete(ctx, &pipe_instance);
		soc_debug_print(ERROR_LVL,
			"IPC sending to BU specific function to connect "
			"stages failed\n");
		return result;
	}
	pipe_instance->configured = true;

	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This function sets up a decode pipe
	1. allocate the pipe
	2. add required stages
	3. connect the stages
	4. configures the pipeline
 */
enum soc_result audio_dec_pipe_setup(struct soc_audio_input_wl
				     *input_wl)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_pipeline *pipe_instance;
	enum soc_result result;

	SOC_ENTER();
	/* Get processor context */
	ctx = input_wl->ctx;

	pipe_instance = input_wl->pipeline =
	    soc_audio_pipeline_add(SOC_AUDIO_DEC_STAGE_COUNT,
				   SOC_AUDIO_DEC_PIPE);
	if (pipe_instance == NULL) {
		soc_debug_print(ERROR_LVL,
				"pipeline creation failed for decode\n");

		return SOC_ERROR_NO_RESOURCES;
	}

	pipe_instance->op_type = PIPE_OPS_PLAYBACK;
	/* Allocate decode pipe */
	result = soc_ipc_alloc_pipe(ctx, pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL, "pipeline addition failed\n");
		return result;
	}

	/* Set default decoder param */
	result = audio_pipe_set_default_decoder_param(input_wl);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
				"set_default_decoder_param failed\n");
		return result;
	}

	/* Add decode stages */
	result = audio_dec_pipe_enable_stages(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"pipeline_add_dec_stage failed with result = %d\n",
			result);
		return result;
	}

	/* Decode BU specific stage initializations */
	result = soc_bu_audio_dec_pipe_init_stages(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
		"soc_bu_audio_dec_pipe_init_stages failed result = %d\n",
		result);
		return result;
	}

	/* Connect decode stages */
	result = soc_bu_audio_pipe_update_connect(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL, "BU pipe connect failed\n");
		return result;
	}

	SOC_EXIT();
	return result;
}

/***********************************************************/
/* Audio pipeline Private function definition */
/***********************************************************/

/*
This is used to set default decoder paramaters
 */
static enum soc_result
audio_pipe_set_default_decoder_param(struct soc_audio_input_wl *input_wl)
{

	enum soc_result result = SOC_FAILURE;

#if defined(INCLUDE_DECODE_STAGE) || defined(INCLUDE_ENCODE_STAGE)
	enum soc_audio_format format;
	struct soc_audio_codec_params *params;

	SOC_ENTER();
	/* Get the params from decode stage parameters */
	params =
	    &input_wl->pipeline->stages[SOC_AUDIO_DEC_DECODE_STAGE].
	    stage_params.decoder.host;

	/* If DTS core is in the input stream, but only DTSHD decoder,
	 * need to force format to DTSHD since we will use the DTSHD
	 * decoder. */
	if (input_wl->format == SOC_AUDIO_MEDIA_FMT_DTS)
		format = SOC_AUDIO_MEDIA_FMT_DTS_HD;
	else
		format = input_wl->format;
	/* Call the format specific setup function to setup stage parameters */
	switch (format) {
	case SOC_AUDIO_MEDIA_FMT_PCM:
	case SOC_AUDIO_MEDIA_FMT_DVD_PCM:
	case SOC_AUDIO_MEDIA_FMT_BLURAY_PCM:
		result = SOC_ERROR_INVALID_PARAMETER;
		soc_debug_print(ERROR_LVL,
				"Codec parameters not set, format is PCM!");
		break;
	case SOC_AUDIO_MEDIA_FMT_DD:
#ifdef INCLUDE_AC3_DECODE
		audio_ac3_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_DD_PLUS:
#ifdef INCLUDE_DDPLUS_DECODE
		audio_ddplus_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_DTS:
#ifdef INCLUDE_DTS_DECODE
		audio_dts_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_DTS_LBR:
#ifdef INCLUDE_DTS_LBR_DECODE
		audio_dts_lbr_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_DTS_BC:
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
		break;
	case SOC_AUDIO_MEDIA_FMT_MPEG:
#ifdef INCLUDE_MPEG_DECODE
		audio_mpeg_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_WM9:
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
		break;
	case SOC_AUDIO_MEDIA_FMT_TRUE_HD:
#ifdef INCLUDE_TRUEHD_DECODE
		audio_truehd_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_DTS_HD:
	case SOC_AUDIO_MEDIA_FMT_DTS_HD_HRA:
	case SOC_AUDIO_MEDIA_FMT_DTS_HD_MA:
#ifdef INCLUDE_DTS_HD_DECODE
		audio_dts_hd_set_default_dec_params(params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_AAC:
	case SOC_AUDIO_MEDIA_FMT_AAC_LOAS:
#ifdef INCLUDE_AAC_DECODE
		audio_aac_set_default_dec_params(format, params);
		result = SOC_SUCCESS;
#else
		result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
#endif
		break;
	case SOC_AUDIO_MEDIA_FMT_COUNT:
	case SOC_AUDIO_MEDIA_FMT_INVALID:
	default:
		result = SOC_ERROR_INVALID_PARAMETER;
		soc_debug_print(ERROR_LVL, "No decoder is setup on the input!");
		break;
	}

	/* Call the BU function to set up BU specific parameters */
	if (SOC_SUCCESS == result) {
		result = soc_bu_audio_pipe_set_default_decoder_param(format,
								     params);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL, "bu set default decodeer "
					"param failed");
		}
	} else if (SOC_ERROR_FEATURE_NOT_SUPPORTED == result) {
		soc_debug_print(ERROR_LVL, "Format %d not supported", format);
	} else {
		soc_debug_print(ERROR_LVL,
				"Can not write codec parameter to pipeline");
	}

	SOC_EXIT();
	return result;
#else
	return result;
#endif
}

#if (defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE))

static enum soc_result
audio_capture_pipe_enable_stages(struct soc_audio_pipeline *pipe_instance)
{
	SOC_ENTER();
	pipe_instance->stages[SOC_AUDIO_CAPTURE_INPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_IN;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_INPUT_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_INPUT_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_INPUT_STAGE].outputs_count = 1;

#if 0
	pipe_instance->stages[SOC_AUDIO_CAPTURE_SRC_STAGE].task =
	    xxx;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_SRC_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_SRC_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_SRC_STAGE].outputs_count = 1;
#endif

	pipe_instance->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_OUT;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].in_use = true;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].inputs_count = 1;
	pipe_instance->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].outputs_count = 1;

	SOC_EXIT();
	return SOC_SUCCESS;
}
#endif

#if (defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE))
enum soc_result audio_capture_pipe_setup(struct soc_audio_input_wl *input_wl)
{
	enum soc_result result = SOC_SUCCESS;
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_pipeline *pipe_instance = NULL;

	SOC_ENTER();
	/* Get processor context */
	ctx = input_wl->ctx;

	pipe_instance = input_wl->pipeline =
	    soc_audio_pipeline_add(SOC_AUDIO_CAPTURE_STAGE_COUNT,
				   SOC_AUDIO_CPR_PIPE);

	if (pipe_instance == NULL) {
		soc_debug_print(ERROR_LVL,
				"pipeline creation failed for capture\n");
		result = SOC_ERROR_NO_RESOURCES;
		goto audio_capture_pipe_setup_exit;
	}

	pipe_instance->op_type = PIPE_OPS_CAPTURE;

	/* Allocate capture pipe */
	result = soc_ipc_alloc_pipe(ctx, pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL, "pipeline addition failed\n");
		goto audio_capture_pipe_setup_exit;
	}

	/* Add capture stages */
	result = audio_capture_pipe_enable_stages(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"pipeline_add_capture_stage failed result = %d\n",
			result);
		goto audio_capture_pipe_setup_exit;
	}

	/* Capture BU specific stage initializations */
	result = soc_bu_audio_capture_pipe_init_stages(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
		"soc_bu_audio_capture_pipe_init_stages failed result = %d\n",
		result);
		goto audio_capture_pipe_setup_exit;
	}

	/* Connect capture stages */
	result = soc_bu_audio_pipe_update_connect(pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL, "BU pipe connect failed\n");
		goto audio_capture_pipe_setup_exit;
	}

	/* Configure pipeline */
	result = soc_ipc_config_pipe(ctx, pipe_instance);
	if (result != SOC_SUCCESS) {
		soc_audio_pipeline_delete(ctx, &pipe_instance);
		soc_debug_print(ERROR_LVL, "Capture pipe config failed\n");
		goto audio_capture_pipe_setup_exit;
	}

	pipe_instance->configured = true;

audio_capture_pipe_setup_exit:

	SOC_EXIT();
	return result;
}
#endif

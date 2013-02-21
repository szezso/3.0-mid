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
#include <linux/slab.h>
#include <linux/mutex.h>
#include "soc_audio_api.h"
#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_ipc.h"
#include "soc_debug.h"
#include "soc_audio_defs.h"

/*
 * Private Audio Output function declarations
 */
static int16_t audio_output_ch_config_to_count(enum
					       soc_audio_channel_config
					       ch_config);

/*
 * Audio Ouput API functions
 */

/**
This API is used to add an output
 */
enum soc_result soc_audio_output_add(uint32_t processor_h,
				     struct soc_audio_output_config config,
				     void **output_h)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_output_wl *new_output = NULL,
	    *curr_output = NULL, *prev_output = NULL;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;
	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		if (ctx->ref_count == 0) {
			soc_debug_print(ERROR_LVL, "Invalid args");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}
		/* Allocate and initialize new output port */
		new_output = (struct soc_audio_output_wl *)
		    kmalloc(sizeof(struct soc_audio_output_wl), GFP_KERNEL);
		if (NULL == new_output) {
			soc_debug_print(ERROR_LVL, "Out of memory");
			result = SOC_ERROR_NO_RESOURCES;
			goto EXIT_UNLOCK;
		}
		/* Initialize the output wl with the config passed in */
		memset(new_output, 0, sizeof(struct soc_audio_output_wl));
		new_output->ctx = ctx;
		new_output->sample_rate = config.sample_rate;
		new_output->sample_size = config.sample_size;
		new_output->channel_config = config.ch_config;
		new_output->ch_map = config.ch_map;
		new_output->out_mode = config.out_mode;
		new_output->channel_count =
		    audio_output_ch_config_to_count(new_output->channel_config);

		/* Set defaults */
		new_output->dmix_mode = SOC_AUDIO_DOWNMIX_INVALID;

		/* Set the reconfigure flag true */
		if (ctx->pipeline->configured)
			new_output->reconfig_post_proc_pipe = true;

		/* Add newly created output to ctx */
		ctx->output_count++;

		prev_output = curr_output = ctx->outputs;
		if (NULL == curr_output) {
			ctx->outputs = new_output;
		} else {
			/* traverse to end of list */
			while (curr_output) {
				prev_output = curr_output;
				curr_output = curr_output->next_output;
			}
			prev_output->next_output = new_output;
		}

#ifdef INCLUDE_QUALITY_STAGE
		/* Add a ACE system for the new output */
		result = audio_add_ACE_system(ctx);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
				"ACE System alloc failed for ouput[%d]\n",
				ctx->output_count);
			goto EXIT_UNLOCK;
		}
#endif
		/* Return handle to user */
		*output_h = (void *)new_output;
		result = SOC_SUCCESS;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/**
This API is used to remove an output
 */
enum soc_result soc_audio_output_remove(uint32_t processor_h,
					void *output_h)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_output_wl *output_wl = NULL;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;
	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		output_wl = (struct soc_audio_output_wl *)output_h;

		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Do the removal */
		result = audio_output_remove(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/*4 . Reconfigure the pipe
		 *TODO : Validate this */
		result = audio_post_proc_pipe_reconfig(ctx);
		if (SOC_SUCCESS != result)
			soc_debug_print(ERROR_LVL,
				"Reconfig pipe on output removal failed");

	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/**
This API is used to enable an output
 */
enum soc_result soc_audio_output_enable(uint32_t processor_h, void *output_h)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		output_wl = (struct soc_audio_output_wl *)output_h;

		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;
		/* Set the enable flag in the output_wl */
		output_wl->enabled = true;

		/*If the reconfig flag in output_wl is true
		 * Reconfigure the pipeline */
		if (true == output_wl->reconfig_post_proc_pipe)
			result = audio_post_proc_pipe_reconfig(ctx);

		if (result == SOC_SUCCESS)
			output_wl->reconfig_post_proc_pipe = false;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;

}

/**
This API is used to disable the output
*/
enum soc_result soc_audio_output_disable(uint32_t processor_h,
					 void *output_h)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx;
	enum soc_result result;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Check if the output is not already disabled */
		if (false == output_wl->enabled) {
			soc_debug_print(DEBUG_LVL_2,
					"Output Disable : Already disabled");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}
		output_wl->enabled = false;
	}

EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This API set the channel config ofthe output
*/
enum soc_result soc_audio_output_set_channel_config(uint32_t processor_h,
						    void *output_h, enum
						    soc_audio_channel_config
						    ch_config)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		if ((ch_config != output_wl->channel_config)
		    && (ctx->pipeline->configured))
			output_wl->reconfig_post_proc_pipe = true;
		output_wl->channel_config = ch_config;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return SOC_SUCCESS;

}

/**
This API set the channel config ofthe output
*/
enum soc_result soc_audio_output_set_sample_size(uint32_t processor_h,
						 void *output_h,
						 uint32_t sample_size)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate input workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		if ((sample_size != output_wl->sample_size)
		    && (ctx->pipeline->configured))
			output_wl->reconfig_post_proc_pipe = true;
		output_wl->sample_size = sample_size;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This API set the sample rate of the output_wl
*/
enum soc_result soc_audio_output_set_sample_rate(uint32_t processor_h,
						 void *output_h,
						 uint32_t sample_rate)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		if ((sample_rate != output_wl->sample_rate)
		    && (ctx->pipeline->configured))
			output_wl->reconfig_post_proc_pipe = true;

		if ((output_wl->out_mode ==
		     SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL)
		    && (sample_rate != 48000)) {
			result = SOC_ERROR_FEATURE_NOT_SUPPORTED;
			soc_debug_print(ERROR_LVL,
			"DD encoded output only supports 48kHz output!");
		} else {
			output_wl->sample_rate = sample_rate;
		}
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This API set the sample output mode of the output_wl
*/

enum soc_result soc_audio_output_set_mode(uint32_t processor_h,
					  void *output_h,
					  enum soc_audio_output_mode mode)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl and processor handle */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result)
			goto EXIT_UNLOCK;

		/* Check if the output is not already enabled */
		if (true == output_wl->enabled) {
			soc_debug_print(ERROR_LVL,
					"Output set mode : Cannot change mode "
					"of enabled output");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}

		/* If the mode is the same, do nothing */
		if (output_wl->out_mode != mode) {
			result = SOC_SUCCESS;
			goto EXIT_UNLOCK;
		}

		/*First figure out what the previous mode was and clean up */
		switch (output_wl->out_mode) {
		case SOC_AUDIO_OUTPUT_PCM:
			break;
		case SOC_AUDIO_OUTPUT_PASSTHROUGH:
		/*TODO For passthrough implementation add a flag passthrough*/
			break;
		case SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL:
		case SOC_AUDIO_OUTPUT_ENCODED_DTS:
		/*TODO render related flags not present in soc_audio_output_wl*/
			break;
		case SOC_AUDIO_OUTPUT_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
			break;
		}

		/*Now reconfigure the output with the new mode */
		switch (mode) {
		case SOC_AUDIO_OUTPUT_PCM:
		/*TODO render related flags not present in soc_audio_output_wl*/
			break;
		case SOC_AUDIO_OUTPUT_PASSTHROUGH:
		/*TODO For passthrough implementation add a flag passthrough */
			break;
		case SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL:
		/*TODO render related flags not present in soc_audio_output_wl*/
			break;
		case SOC_AUDIO_OUTPUT_ENCODED_DTS:
		/*TODO render related flags not present in soc_audio_output_wl*/
			break;
		case SOC_AUDIO_OUTPUT_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
			break;
		}
		/* Finally setup the output mode */
		output_wl->out_mode = mode;
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return SOC_SUCCESS;
}

/* Use to specify a specific downmix to be transmitted via an output. */
enum soc_result soc_audio_output_set_downmix_mode(uint32_t processor_h,
						  void *output_h,
						  enum soc_audio_downmix_mode
						  dmix_mode)
{
	struct soc_audio_output_wl *output_wl;
	struct soc_audio_processor_context *ctx = NULL;
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result) {
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}
	/* Derive the processor context */
	ctx = &soc_audio_processor[processor_h];

	mutex_lock(&ctx->processor_lock);
	{
		/* Derive the output_wl and processor handle */
		output_wl = (struct soc_audio_output_wl *)output_h;
		/* Validate output workload */
		result = audio_output_workload_validation(ctx, output_wl);
		if (SOC_SUCCESS != result) {
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}
		/* Check if the output is not already enabled */
		if (true == output_wl->enabled) {
			soc_debug_print(ERROR_LVL, "Output is enabled,"
			"please disable before trying to set downmix mode");
			result = SOC_ERROR_INVALID_PARAMETER;
			goto EXIT_UNLOCK;
		}

		switch (dmix_mode) {
		case SOC_AUDIO_DOWNMIX_DEFAULT:
		case SOC_AUDIO_DOWNMIX_1_0:
		case SOC_AUDIO_DOWNMIX_1_0_LFE:
		case SOC_AUDIO_DOWNMIX_2_0:
		case SOC_AUDIO_DOWNMIX_2_0_NO_SCALE:
		case SOC_AUDIO_DOWNMIX_2_0_LFE:
		case SOC_AUDIO_DOWNMIX_2_1:
		case SOC_AUDIO_DOWNMIX_2_1_LFE:
		case SOC_AUDIO_DOWNMIX_2_0_LTRT:
		case SOC_AUDIO_DOWNMIX_2_0_LTRT_NO_SCALE:
		case SOC_AUDIO_DOWNMIX_2_0_DOLBY_PRO_LOGIC_II:
		case SOC_AUDIO_DOWNMIX_2_0_DOLBY_PRO_LOGIC_II_NO_SCALE:
		case SOC_AUDIO_DOWNMIX_2_0_DVB_AAC:
		case SOC_AUDIO_DOWNMIX_3_0:
		case SOC_AUDIO_DOWNMIX_3_0_LFE:
		case SOC_AUDIO_DOWNMIX_3_1:
		case SOC_AUDIO_DOWNMIX_3_1_LFE:
		case SOC_AUDIO_DOWNMIX_2_2:
		case SOC_AUDIO_DOWNMIX_2_2_LFE:
		case SOC_AUDIO_DOWNMIX_3_2:
		case SOC_AUDIO_DOWNMIX_3_2_LFE:
		case SOC_AUDIO_DOWNMIX_3_0_1:
		case SOC_AUDIO_DOWNMIX_3_0_1_LFE:
		case SOC_AUDIO_DOWNMIX_2_2_1:
		case SOC_AUDIO_DOWNMIX_2_2_1_LFE:
		case SOC_AUDIO_DOWNMIX_3_2_1:
		case SOC_AUDIO_DOWNMIX_3_2_1_LFE:
		case SOC_AUDIO_DOWNMIX_3_0_2:
		case SOC_AUDIO_DOWNMIX_3_0_2_LFE:
		case SOC_AUDIO_DOWNMIX_2_2_2:
		case SOC_AUDIO_DOWNMIX_2_2_2_LFE:
		case SOC_AUDIO_DOWNMIX_3_2_2:
		case SOC_AUDIO_DOWNMIX_3_2_2_LFE:
			output_wl->dmix_mode = dmix_mode;
			break;
		case SOC_AUDIO_DOWNMIX_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			break;
		}
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/**
This API is used to get output sample rate
*/
enum soc_result soc_audio_output_get_sample_rate(uint32_t processor_h,
						 void *handle,
						 int *sample_rate)
{

	struct soc_audio_output_wl *output_wl;
	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();

	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Validate the arguments */
	if (!handle || !sample_rate) {
		soc_debug_print(ERROR_LVL, "Invalid input handle");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto EXIT;
	}
	output_wl = (struct soc_audio_output_wl *)handle;

	*sample_rate = output_wl->sample_rate;
EXIT:
	SOC_EXIT();
	return result;
}

/* Remove output workload.
 * This function need processor lock protection, and caller need to make sure
 * the arguments are valid */
enum soc_result
audio_output_remove(struct soc_audio_processor_context *ctx,
		    struct soc_audio_output_wl *output_wl)
{
	enum soc_result result = SOC_FAILURE;
	struct soc_audio_output_wl *rem_output = NULL;
	struct soc_audio_output_wl *prev_output = NULL;
	uint32_t ACE_sys_index = 0;

	SOC_ENTER();
	/* Remove output from lint */
	prev_output = rem_output = ctx->outputs;
	ACE_sys_index = 0;
	if (output_wl == ctx->outputs) {
		ctx->outputs = ctx->outputs->next_output;
	} else {
		/* traverse to find output, rem_output points to the
		 * output to remove */
		while (rem_output && (output_wl != rem_output)) {
			prev_output = rem_output;
			rem_output = rem_output->next_output;
			ACE_sys_index++;
		}
		/* remove output from list if a match found for
		 * in_output_h */
		if (rem_output)
			prev_output->next_output = rem_output->next_output;

	}
	/* free the memory if we find the output workload */
	if (rem_output) {
		ctx->output_count--;
#ifdef INCLUDE_QUALITY_STAGE
		result = audio_remove_ACE_system(ctx, ACE_sys_index);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
			"ACE System removal failed for ouput[%d]",
			ACE_sys_index);
			goto EXIT;
		}
#endif
		kfree(rem_output);
		if (0 == ctx->output_count)
			ctx->outputs = NULL;
		result = SOC_SUCCESS;
	} else {
		/* output_h doesn't match any output ports */
		result = SOC_ERROR_INVALID_PARAMETER;
		soc_debug_print(ERROR_LVL, "output_h doesn't match");
		goto EXIT;
	}
EXIT:
	SOC_EXIT();
	return result;
}

/************************************************************************/
/* Private Audio Output functions                                       */
/************************************************************************/

/**
This function returns the channel count
for given channel configuration.
*/
static int16_t audio_output_ch_config_to_count(enum
					       soc_audio_channel_config
					       ch_config)
{
	int16_t ch_count = 0;

	switch (ch_config) {
	case SOC_AUDIO_STEREO:
	case SOC_AUDIO_DUAL_MONO:
		ch_count = 2;
		break;
	case SOC_AUDIO_5_1:
		ch_count = 6;
		break;
	case SOC_AUDIO_7_1:
		ch_count = 8;
		break;
	case SOC_AUDIO_CHAN_CONFIG_INVALID:
	default:
		ch_count = -1;	/* Invalid config */
		soc_debug_print(ERROR_LVL, "Invalid output channel config!");
		break;
	}

	return ch_count;
}

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

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include "soc_audio_api.h"
#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_debug.h"
#include "soc_ipc.h"
#include "soc_audio_defs.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_audio_pipeline_defs.h"

/**************************************************************************/
/* Audio Processor global variables  */
/**************************************************************************/

/*  For now, we suport only a fixed number of processors */
struct soc_audio_processor_context
	soc_audio_processor[SOC_AUDIO_MAX_PROCESSORS];
/* Global init data, hook into BU provided init function */
struct soc_audio_init_data soc_audio_g_init_data;
/* Debug level in the code */
uint32_t soc_audio_debug_lvl = WARNING_LVL;

/* General initialization done */
bool soc_audio_init_done;
/*  Global processor handle */
static int32_t soc_audio_gbl_proc_hndl = SOC_BAD_HANDLE;

/******************************************************************************/
/* Audio Processor Private function declarations */
/******************************************************************************/
static enum soc_result
audio_post_proc_pipe_init(struct soc_audio_processor_context *ctx);
static void soc_audio_mixer_config_set_gain(struct soc_audio_channel_mix_config
					    *channel_mix_config,
					    int32_t one_to_one_val,
					    int32_t n_by_m_value);

/******************************************************************************/
/* Audio Processor API function Implementation */
/******************************************************************************/

/*!
This API is used to initialize state and structures internal to this layer.
It is the very first API needs to be called, if not done all subsequent
API's will be failed
 */
enum soc_result soc_init(struct soc_audio_init_data *init_data)
{
	enum soc_result result = SOC_FAILURE;
	uint32_t handle = 0;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* if init already done return success */
	if (true == soc_audio_init_done) {
		result = SOC_SUCCESS;
		goto EXIT;
	}

	/* Cache allignment check */
	result = soc_bu_audio_cache_align_check();
	if (SOC_SUCCESS != result) {
		soc_debug_print(ERROR_LVL,
				"audio_cache_align_check failed\n");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}

	/* Iterate all the processor and initilze lock and re_count */
	for (handle = 0; handle < SOC_AUDIO_MAX_PROCESSORS; ++handle) {
		ctx = &(soc_audio_processor[handle]);
		/* We init usage count of semaphore to 1 means that it is a
		 * mutex now. */
		mutex_init(&ctx->processor_lock);
		ctx->ref_count = 0;
	}

	soc_audio_gbl_proc_hndl = SOC_BAD_HANDLE;

	init_waitqueue_head(&soc_audio_g_init_data.wait_queue);
	soc_audio_g_init_data.condition = 0;
	/* Initialze work queue and ipc queue */
	mutex_init(&init_data->list_lock);
	INIT_LIST_HEAD(&init_data->ipc_dispatch_list);
	soc_ipc_init_work_queue(init_data);

	/* Mark Init done */
	soc_audio_init_done = true;

	result = SOC_SUCCESS;
EXIT:
	SOC_EXIT();
	return result;
}

/*!
This API is used to de-initialize state and structures internal to this layer.
This should be called during system shutdown
 */
void soc_destroy(void)
{

	uint32_t handle = 0;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* if init not done, exit out */
	if (false == soc_audio_init_done) {
		SOC_EXIT();
		return;
	}
	soc_audio_init_done = false;

	/* Iterate all the processor and close them */
	for (handle = 0; handle < SOC_AUDIO_MAX_PROCESSORS; ++handle) {
		ctx = &(soc_audio_processor[handle]);
		soc_audio_close_processor(handle);
		/* destroy the mutex now. */
		mutex_destroy(&ctx->processor_lock);
	}

	soc_audio_gbl_proc_hndl = SOC_BAD_HANDLE;

	/* Free queued messages and destroy work queue */
	soc_ipc_destroy_work_queue(&soc_audio_g_init_data);
	mutex_destroy(&soc_audio_g_init_data.list_lock);

	SOC_EXIT();
	return;
}

/**
This API opens and returns a handle to an Audio Processor if an instance
is available. The handle will be used to manage the audio processing and
allows the user to add inputs and outputs to the instance. Only a single
global processor instance can be opened on the system. If the global
processor is already created, the handle to the already created global
processor is returned.
If other audio processors are already open, then the physical inputs and
outputs assigned to other processors cannot be added to this processor.
 */
enum soc_result soc_audio_open_processor(bool is_global,
					 uint32_t *processor_h)
{
	enum soc_result result = SOC_FAILURE;
	uint32_t loop, handle = 0;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();

	/* Validate arguments */
	if (processor_h == NULL || (!(is_global == true || is_global == false))
	    || soc_audio_init_done == false) {
		/* invalid argunents passed */
		soc_debug_print(ERROR_LVL, "Invalid arguments\n");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto EXIT;
	}

	*processor_h = SOC_BAD_HANDLE;

	/* If global processor context is requested, check if one is
	   allocated and return that. Mark that conext is use by increasing
	   its reference count */
	if ((true == is_global) &&
	    (soc_audio_gbl_proc_hndl != SOC_BAD_HANDLE)) {
		ctx = &soc_audio_processor[soc_audio_gbl_proc_hndl];
		/* under lock check that context is marked as global processor
		   (to avoid race) only if it marked as global processor
		   increase the ref count */
		mutex_lock(&ctx->processor_lock);
		{
			if (ctx->is_global_processor == true
			    && ctx->ref_count > 0) {
				++ctx->ref_count;
				*processor_h = soc_audio_gbl_proc_hndl;
				result = SOC_SUCCESS;
			} else {
				result = SOC_FAILURE;
			}
		}
		mutex_unlock(&ctx->processor_lock);
		goto EXIT;
	}

	/* Create a processor */
	/* Check for an available processor instance, and return the handle. */
	for (handle = 0; handle < SOC_AUDIO_MAX_PROCESSORS; ++handle) {

		/* Check if processor available */
		ctx = &(soc_audio_processor[handle]);
		mutex_lock(&ctx->processor_lock);
		if (0 != ctx->ref_count) {
			mutex_unlock(&ctx->processor_lock);
			continue;
		}
		/* Initialze wait queue for ipc send & wait */
		init_waitqueue_head(&ctx->wait_queue);

		/* Alloc a pipeline in IA and send IPC alloc message to FW. */
		/* Set up default values of stage parameters. */
		result = audio_post_proc_pipe_init(ctx);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL,
					"Processor %x initialization failed\n",
					handle);
			mutex_unlock(&ctx->processor_lock);
			break;
		}

		ctx->is_global_processor = is_global;
		ctx->ref_count = 1;
		ctx->input_count = 0;
		ctx->output_count = 0;

		/* Init volume control related parameters */
		ctx->muted = false;
		ctx->master_volume = SOC_AUDIO_GAIN_0_DB;
		ctx->master_volume_enable = false;
		ctx->per_channel_volume_enable = false;
		for (loop = 0; loop < SOC_AUDIO_MAX_OUTPUT_CHANNELS; loop++) {
			ctx->per_channel_volume[loop] = SOC_AUDIO_GAIN_0_DB;
			ctx->post_mix_ch_gain[loop]
			    = SOC_AUDIO_MIX_COEFF_INDEX_0_DB;
		}

		/* If global processor requested, make this context as global
		   processor */
		if (true == is_global)
			soc_audio_gbl_proc_hndl = handle;

		*processor_h = handle;
		result = SOC_SUCCESS;
		mutex_unlock(&ctx->processor_lock);
		/* To break the loop */
		break;
	}
	/* Reach here means all processors are been used */
EXIT:
	SOC_EXIT();
	return result;
}

/**
This API frees up the instance of the processor specifed by the handle.
It cleans up any resources associated with the instance.
A call to close processor will also remove and inputs or outputs connected.
 */
enum soc_result soc_audio_close_processor(uint32_t processor_h)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_input_wl *curr_input = NULL, *prev_input = NULL;
	struct soc_audio_output_wl *curr_output = NULL, *prev_output = NULL;
	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();
	/* Validate processor handle */
	result = audio_processor_handle_validation(processor_h);
	if (SOC_SUCCESS != result)
		goto EXIT;

	/* Get the associated context */
	ctx = &(soc_audio_processor[processor_h]);
	mutex_lock(&ctx->processor_lock);
	{
		if (ctx->ref_count == 0) {
			result = SOC_ERROR_INVALID_PARAMETER;
			soc_debug_print(ERROR_LVL, "Try to remove processor "
					"not in use\n");
			goto EXIT_UNLOCK;
		}

		/* Remove a user */
		--ctx->ref_count;
		result = SOC_SUCCESS;

		/* Do not remove the processor if it still has users */
		if (ctx->ref_count != 0) {
			result = SOC_SUCCESS;
			goto EXIT_UNLOCK;
		}

		/* We need to free this processor context */
		/* If this is the global processor, release the global handle */
		if (true == ctx->is_global_processor) {
			soc_audio_gbl_proc_hndl = SOC_BAD_HANDLE;
			ctx->is_global_processor = false;
		}

		/* Free input workloads */
		curr_input = prev_input = ctx->inputs;
		while (curr_input) {
			prev_input = curr_input;
			curr_input = curr_input->next_input;
			/* Stop and free the input pipeline if input
			   pipeline available */
			result = audio_input_remove(ctx, prev_input);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
						"Cannot remove input\n");
				/* Failure, try to go ahead to
				   clean as much as possible */
			}
		}

		/* Free output workloads */
		prev_output = curr_output = ctx->outputs;
		while (curr_output) {
			prev_output = curr_output;
			curr_output = curr_output->next_output;
			result = audio_output_remove(ctx, prev_output);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
						"Cannot remove output\n");
			}
			/* Since the output is removed, we reconfigure the
			 pipe. */
			result = audio_post_proc_pipe_reconfig(ctx);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
				"Reconfig pipe after output removal failed\n");
			}
		}
		/* Stop and free the basic pipeline */
		if (ctx->pipeline == NULL) {
			result = SOC_FAILURE;
			soc_debug_print(ERROR_LVL, "Post processing pipeline "
					"does not exist\n");
			goto EXIT_UNLOCK;
		}

		/* stop if the base pipeline is already started. */
		if (ctx->pipeline->started) {
			result = soc_ipc_stop_pipe(ctx, ctx->pipeline);
			if (SOC_SUCCESS != result) {
				soc_debug_print(ERROR_LVL,
						"IPC stop post processing "
						"pipeline failed\n");
			}
			ctx->pipeline->started = false;
		}

		/* Remove the pipe flush, post processing above did that for
		 any started pipe. */

		/* Free the post processing pipeline after remove all the
		 * input/output workloads */
		result = soc_ipc_free_pipe(ctx, ctx->pipeline);
		if (SOC_SUCCESS != result)
			soc_debug_print(ERROR_LVL, "IPC free pipe failed\n");

		/* De-Allocate post processing pipeline */
		result = soc_audio_pipeline_delete(ctx, &ctx->pipeline);
		if (SOC_SUCCESS != result) {
			soc_debug_print(ERROR_LVL, "Delete post processing "
					"pipeline failed\n");
		}
	}
EXIT_UNLOCK:
	mutex_unlock(&ctx->processor_lock);
EXIT:
	SOC_EXIT();
	return result;
}

/******************************************************************************/
/* Audio Processor Private function definition */
/******************************************************************************/

/*
* Check the processor handle
*/
enum soc_result audio_processor_handle_validation(uint32_t processor_h)
{
	enum soc_result result = SOC_SUCCESS;
	/* Validate processor */
	if (processor_h >= SOC_AUDIO_MAX_PROCESSORS) {
		soc_debug_print(ERROR_LVL, " Invalid processor:%d ",
				processor_h);
		result = SOC_ERROR_INVALID_HANDLE;
		goto EXIT;
	}
	if (!soc_audio_init_done) {
		soc_debug_print(ERROR_LVL,
				" Not initialize the audio module yet!\n ");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}

EXIT:
	return result;
}

/*
* Reset only SOME params of certain stages
* However here we are trying to initialize all the params and will validate the
* the result. */
static void audio_reset_stage_params(struct
			      soc_audio_processor_context
			      *ctx)
{

#ifdef INCLUDE_SRC_STAGE
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].inputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].outputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	{
		struct soc_audio_pipeline_src_params *src;
		uint32_t i = 0;
		src =
		    &ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].
		    stage_params.src;
		src->primary_index = SOC_BAD_HANDLE;
		src->mix_src_mode = SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO;
		src->auto_output_sample_rate = SOC_AUDIO_OUTPUT_SAMPLE_RATE;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			src->vsrc_set_var_on[i] = 0;
			src->output_sample_rate[i] = 0;
			src->state[i] = SOC_AUDIO_STATUS_NO_DATA_AVAIL;
			src->vsrc_init_flag[i] = 0;
		}

	}
#endif /*INCLUDE_SRC_STAGE */

#ifdef INCLUDE_DOWNMIXER_STAGE
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		int dolby_dmix_echo = false;
		struct soc_audio_downmix_params *downmixer;
		downmixer =
		    &ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].
		    stage_params.downmixer;
		dolby_dmix_echo = false;
		/* The Downmix stage can have maximum output number of inputs
		   After Downmix stage , the number of outputs is derived
		   from the output_cfg */
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++) {
			downmixer->inputs[i].dolby_cert_mode = dolby_dmix_echo;
			downmixer->inputs[i].dmix_mode =
			    SOC_AUDIO_DOWNMIX_INVALID;
		}
	}
#endif /* INCLUDE_DOWNMIXER_STAGE */

#ifdef INCLUDE_POSTSRC_STAGE
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		struct soc_audio_pipeline_src_params *src;
		src =
		    &ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].
		    stage_params.src;
		src->primary_index = SOC_BAD_HANDLE;
		src->mix_src_mode = SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO;
		src->auto_output_sample_rate = 48000;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			src->output_sample_rate[i] = 0;
			src->state[i] = SOC_AUDIO_STATUS_NO_DATA_AVAIL;
		}

	}
#endif /* INCLUDE_POSTSRC_STAGE */

#ifdef INCLUDE_QUALITY_STAGE
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].outputs_count = 0;
#endif /* INCLUDE_QUALITY_STAGE */

#ifdef INCLUDE_INTERLEAVER_STAGE
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		struct soc_audio_interleaver_params *interleaver;
		interleaver =
		    &ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].
		    stage_params.interleaver;
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++) {
			/* Initialize */
			interleaver->inputs[i].output_ch_count = 0;
			interleaver->inputs[i].reorder_for_hw = true;
			interleaver->inputs[i].output_ch_map =
			    SOC_AUDIO_INVALID_CHANNEL_CONFIG;
		}

	}
#endif /* INCLUDE_INTERLEAVER_STAGE */

#ifdef INCLUDE_PER_OUTPUT_DELAY_STAGE
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].inputs_count =
	    0;
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].outputs_count =
	    0;
	{
		uint32_t i = 0;
		int32_t buffer_size = 0;
		struct soc_audio_per_output_delay_params *per_output_delay;
		per_output_delay =
		    &ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].
		    stage_params.per_output_delay;
		buffer_size = 0;
		if (buffer_size > 0) {
			per_output_delay->max_delay_ms =
			    buffer_size / (192 * 8 * 4);
			per_output_delay->buffer_size = buffer_size;
			per_output_delay->enabled = true;
		} else {
			per_output_delay->enabled = false;
			per_output_delay->max_delay_ms = 0;
			per_output_delay->buffer_size = 0;
		}
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++)
			per_output_delay->output_params[i].buffer_reset = true;
	}
#endif /* INCLUDE_PER_OUTPUT_DELAY_STAGE */

#ifdef INCLUDE_OUTPUT_STAGE
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].outputs_count = 0;
#endif /* INCLUDE_OUTPUT_STAGE */

}

/*
* Reconfigure the post processing pipeline after new output is added and enabled
* Caller has to make sure that this is called within the processor lock
* or removed.This will trigger following:
* 0. Check if the pipe is configured
* 1A. Stop pipe IPC to FW
* 1B. Flush the data path jobs out of the pipe
* 2. Deallocate stage param buffers
* 3. Reset params of some stages
* 4. Re-initialize stages according to new output_cfg
* 5. Re-allocate stage param buffers
* 6. Connect the stages
* 7. Start the new reconfigured pipeline
*/

enum soc_result audio_post_proc_pipe_reconfig(struct
					      soc_audio_processor_context
					      *ctx)
{
	enum soc_result result;
	struct soc_audio_pipe_out out_cfg;
	SOC_ENTER();

	/* Proceed only if the pipe is configured */
	if (!ctx->pipeline->configured) {
		soc_debug_print(ERROR_LVL,
				"Trying to reconfigure a non-configured pipe");
		return SOC_ERROR_INVALID_PARAMETER;
	}
	/* Stop pipe IPC to FW */
	if (ctx->pipeline->started) {
		soc_ipc_stop_pipe(ctx, ctx->pipeline);
		ctx->pipeline->started = false;
		/* Flush the data path jobs out of the pipe */
		soc_ipc_flush_pipe(ctx, ctx->pipeline);
	}

	/* Deallocate stage param buffers */
	result = soc_bu_audio_pipe_disconnect(ctx->pipeline);

	if (0 == ctx->output_count) {
		soc_debug_print(DEBUG_LVL_2,
			" Reconfigure following the removal of last output"
			"so exiting without restarting the pipeline");
		goto EXIT;
	}
	/* Reset params of post mixer stages */
	audio_pipeline_read_output_configs(ctx, &out_cfg);

	audio_reset_stage_params(ctx);

	/* Re-initialize stages according to new output_cfg */
	result =
	    audio_post_proc_pipe_set_post_mix_stages(&out_cfg, ctx->pipeline);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"post_proc_pipe_enable_stages failed result = %d\n",
			result);
		return result;
	}
	/* Re-allocate stage param buffers */
	/* Connect the stages */
	result = soc_bu_audio_pipe_update_connect(ctx->pipeline);
	if (result != SOC_SUCCESS) {
		soc_debug_print(ERROR_LVL,
			"BU specific function to connect stages failed\n");
		return result;
	}
	/* Start the new reconfigured pipeline */
	result = soc_ipc_start_pipe(ctx, ctx->pipeline);
	if (result != SOC_SUCCESS) {
		soc_ipc_free_pipe(ctx, ctx->pipeline);

		/* bu specific diconnect is called from pipeline_delete */
		soc_audio_pipeline_delete(ctx, &ctx->pipeline);
		soc_debug_print(ERROR_LVL,
				"IPC sending to BU specific function to start "
				"pipe failed\n");
		return result;
	}
	ctx->pipeline->started = true;

EXIT:
	SOC_EXIT();
	return result;

}

/*
* Start the post processing pipe
* We should always check pipeline configured before the call
* This function assumes post processing pipeline already configured
*/
enum soc_result audio_post_proc_pipe_start(struct
					   soc_audio_processor_context
					   *ctx)
{
	enum soc_result result;
	SOC_ENTER();

	/* start the pipeline */
	result = soc_ipc_start_pipe(ctx, ctx->pipeline);
	if (result != SOC_SUCCESS) {
		soc_ipc_free_pipe(ctx, ctx->pipeline);

		/* bu specific diconnect is called from pipeline_delete */
		soc_audio_pipeline_delete(ctx, &ctx->pipeline);
		soc_debug_print(ERROR_LVL,
				"IPC sending to BU specific function to start "
				"pipe failed\n");
		return result;
	}
	ctx->pipeline->started = true;
	SOC_EXIT();
	return result;
}

/*
* Caller need to make sure to take a processor lock before the call.
* Check the input workload. The input workload must not NULL and inside the
* input list of the processor context.
*/
enum soc_result
audio_input_workload_validation(struct soc_audio_processor_context *ctx,
				struct soc_audio_input_wl *input_wl)
{
	struct soc_audio_input_wl *curr_input = NULL;
	enum soc_result result = SOC_SUCCESS;
	/* Check the process context */
	if (!ctx) {
		soc_debug_print(ERROR_LVL, " Invalid process context ");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto EXIT;
	}
	/* This processor should be in use */
	if (ctx->ref_count == 0) {
		soc_debug_print(ERROR_LVL, " Processor not in use.\n ");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}
	/* Validate input handle */
	if (!input_wl) {
		soc_debug_print(ERROR_LVL, " Invalid input handle ");
		result = SOC_ERROR_INVALID_HANDLE;
		goto EXIT;
	}
	/* See if the input_wl is in the processor */
	curr_input = ctx->inputs;
	while (curr_input && curr_input != input_wl)
		curr_input = curr_input->next_input;

	if (curr_input != input_wl) {
		soc_debug_print(ERROR_LVL,
				" Can not find input workload from "
				" processor context ");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}
EXIT:
	return result;
}

/*
 * initial post processing pipeline. Allocate pipeline and initialize stage
 * parameters. Stage buffer are NOT allocated here.
 */
static enum soc_result
audio_post_proc_pipe_init(struct soc_audio_processor_context *ctx)
{

	enum soc_result result = SOC_FAILURE;

	SOC_ENTER();
	/* Add the main pipeline */
	ctx->pipeline = soc_audio_pipeline_add(SOC_AUDIO_STAGE_COUNT,
					       SOC_AUDIO_MAIN_PIPE);
	if (ctx->pipeline == NULL) {
		soc_debug_print(ERROR_LVL, "pipeline creation failed\n");
		result = SOC_FAILURE;
		goto EXIT;
	}

	/* Inform the FW about the new pipeline */
	result = soc_ipc_alloc_pipe(ctx, ctx->pipeline);
	if (SOC_SUCCESS != result) {
		soc_debug_print(ERROR_LVL, "pipeline creation IPC failed\n");
		soc_audio_pipeline_delete(ctx, &ctx->pipeline);
		result = SOC_FAILURE;
		goto EXIT;
	}

/* Note EVERY stage up to the input of the mixer needs to attempt to service
 * every input to avoid tearing down the PSM pipe on removal and addition of
 * each input, so put input/output count to max so each stage will attempt
 * to service each input if its available.  Here each stage before the mixer
 * is set to MAX input and outputs for that reason. Each of these stages
 * are 1 input and 1 output that is matched. So input/output is the same. */
/* Enable the basic stages required for post processing pipeline others we
 * will add based based on input format */

#ifdef INCLUDE_INPUT_STAGE
	/* Input stage */
	ctx->pipeline->stages[SOC_AUDIO_INPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_IN;
	ctx->pipeline->stages[SOC_AUDIO_INPUT_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_INPUT_STAGE].inputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	ctx->pipeline->stages[SOC_AUDIO_INPUT_STAGE].outputs_count =
	    SOC_AUDIO_MAX_INPUTS;
#endif /* INCLUDE_INPUT_STAGE */

#ifdef INCLUDE_DEINTERLEAVER_STAGE
	/* Need to preform a de-interleave */
	ctx->pipeline->stages[SOC_AUDIO_DEINTERLVR_STAGE].inputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	ctx->pipeline->stages[SOC_AUDIO_DEINTERLVR_STAGE].outputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	ctx->pipeline->stages[SOC_AUDIO_DEINTERLVR_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_DEINTERLEAVER;
	ctx->pipeline->stages[SOC_AUDIO_DEINTERLVR_STAGE].in_use = true;
#endif /* INCLUDE_DEINTERLEAVER_STAGE */

#ifdef INCLUDE_SRC_STAGE
	/* SRC stage */
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_SRC;
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].inputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].outputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	{
		struct soc_audio_pipeline_src_params *src;
		uint32_t i = 0;
		src =
		    &ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].
		    stage_params.src;
		src->primary_index = SOC_BAD_HANDLE;
		src->mix_src_mode = SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO;
		src->auto_output_sample_rate = 48000;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			src->vsrc_set_var_on[i] = 0;
			src->output_sample_rate[i] = 0;
			src->state[i] = SOC_AUDIO_STATUS_NO_DATA_AVAIL;
			src->vsrc_init_flag[i] = 0;
		}

	}
#endif /* INCLUDE_SRC_STAGE */

#ifdef INCLUDE_MIXER_STAGE
	/* Mixer stage */
	ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_MIX;
	ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].inputs_count =
	    SOC_AUDIO_MAX_INPUTS;
	/* Mixer will have only one output */
	ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].outputs_count = 1;
	{
		uint32_t i;
		struct soc_audio_mixer_params *mixer;
		mixer =
		    &ctx->pipeline->stages[SOC_AUDIO_MIXER_STAGE].
		    stage_params.mixer;

		mixer->host.coeff_changed = 0;
		mixer->host.operate_in_default_mode = false;
		mixer->host.primary_index = -1;	/* No primary stream
						 initially. */
		mixer->host.secondary_index = -1;
		/* No secondary stream with mixing metadata initially. */
		mixer->host.output_ch_config = 0x76543210;
		mixer->host.output_sample_size = 32;
		/* Mixer currenty operates only on 32-bit samples. */
		mixer->host.use_stream_metadata = false;
		/* Use or don't mixer metadata embedded in streams */
		mixer->host.ch_config_mode = SOC_AUDIO_MIX_CH_CFG_MODE_AUTO;
		mixer->host.ch_config_mode_fixed_ch_cfg = 0xFFFFFFFF;
		/* Default mode is AUTO, dont specify a valid config */
		mixer->host.output_ch_gains_id = 0x76543210;
		/* Added for output_ch_gains_id initialization for per channel
		 *              volume control */
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUT_CHANNELS; i++) {
			mixer->host.output_ch_gains[i] =
			    SOC_AUDIO_MIX_COEFF_INDEX_0_DB;
		}
		/* Ensure all mixer inputs have valid default gain values. */
		soc_audio_mixer_config_set_gain(&
						(mixer->
						 host.channel_mix_config),
						SOC_AUDIO_MIX_COEFF_INDEX_0_DB,
						SOC_AUDIO_MIX_COEFF_INDEX_MUTE);
		mixer->host.input_index = SOC_BAD_HANDLE;

		/* Updating the DSP copy here since this is called in open
		 processor, and will be committed before running the pipe. */
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			soc_audio_mixer_config_set_gain(&(mixer->input_config
						[i].channel_mix_config),
						SOC_AUDIO_MIX_COEFF_INDEX_0_DB,
						SOC_AUDIO_MIX_COEFF_INDEX_MUTE);
		}
	}
#endif

#ifdef INCLUDE_WATERMARK_STAGE
	/* Watermark stage */
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_WATERMARK;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].inputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].outputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].stage_params.
	    watermark.enabled = false;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].stage_params.
	    watermark.handle = SOC_BAD_HANDLE;
	ctx->pipeline->stages[SOC_AUDIO_WATERMARK_STAGE].stage_params.
	    watermark.host_notified = false;
#endif /* INCLUDE_WATERMARK_STAGE */

#ifdef INCLUDE_BASS_MANAGEMENT_STAGE
	/* Bass management stage */
	ctx->pipeline->stages[SOC_AUDIO_BASS_MAN_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_BM;
	ctx->pipeline->stages[SOC_AUDIO_BASS_MAN_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_BASS_MAN_STAGE].inputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_BASS_MAN_STAGE].outputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_BASS_MAN_STAGE].stage_params.
	    bass_man.enabled = false;
#endif /* INCLUDE_BASS_MANAGEMENT_STAGE */

#ifdef INCLUDE_DELAY_MANAGEMENT_STAGE
	/*Delay Managament Stage */
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_DM;
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].in_use = true;
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].inputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].outputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].
	    stage_params.delay_mgmt.enabled = false;
	ctx->pipeline->stages[SOC_AUDIO_DELAY_MAN_STAGE].
	    stage_params.delay_mgmt.buffer_reset = false;
#endif /* INCLUDE_DELAY_MANAGEMENT_STAGE */

#ifdef INCLUDE_DOWNMIXER_STAGE
	/* Downmix stage */
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_DOWNMIX;
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].in_use = true;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		int dolby_dmix_echo = false;
		struct soc_audio_downmix_params *downmixer;
		downmixer =
		    &ctx->pipeline->stages[SOC_AUDIO_DOWNMIX_STAGE].
		    stage_params.downmixer;
		dolby_dmix_echo = false;
		/* The Downmix stage can have maximum output number of inputs
		   After Downmix stage , the number of outputs is derived
		   from the output_cfg */
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++) {
			downmixer->inputs[i].dolby_cert_mode = dolby_dmix_echo;
			downmixer->inputs[i].dmix_mode =
			    SOC_AUDIO_DOWNMIX_INVALID;
		}
	}
#endif /* INCLUDE_DOWNMIXER_STAGE */

#ifdef INCLUDE_POSTSRC_STAGE
	/* Post SRC stage */
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_SRC;
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].in_use = true;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		struct soc_audio_pipeline_src_params *src;
		src =
		    &ctx->pipeline->stages[SOC_AUDIO_POST_SRC_STAGE].
		    stage_params.src;
		src->primary_index = SOC_BAD_HANDLE;
		src->mix_src_mode = SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO;
		src->auto_output_sample_rate = 48000;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			src->output_sample_rate[i] = 0;
			src->state[i] = SOC_AUDIO_STATUS_NO_DATA_AVAIL;
		}

	}
#endif /* INCLUDE_POSTSRC_STAGE */

#ifdef INCLUDE_QUALITY_STAGE
	/* Audio Quality stage */
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_AUDIO_QUALITY;
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].in_use = true;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		/* Initialize is context pointers' array for ACE systems.
		 * We have the assumption that the number of non-NULL elements
		 * in array equals to the number of the output, which further
		 * implies that the Nth ACE system is assocated to the Nth
		 * output.
		 * This context array need to be updated according to the
		 * current number of output. */
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++) {
			ctx->pipeline->
			    stages[SOC_AUDIO_QUALITY_STAGE].stage_params.
			    audio_quality.context[i] = NULL;
			/* set connection metadata which is the output to ACE
			 * system mapping. Assumption here is the first output
			 * always uses first ACE system and so on. */
			ctx->pipeline->stages[SOC_AUDIO_QUALITY_STAGE].bu_stage.
			    conn_mda.output[i] = i;
		}
	}
#endif /* INCLUDE_QUALITY_STAGE */

#ifdef INCLUDE_INTERLEAVER_STAGE
	/* Need to preform an interleave */
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_INTERLEAVER;
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].in_use = true;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].outputs_count = 0;
	{
		uint32_t i = 0;
		struct soc_audio_interleaver_params *interleaver;
		interleaver =
		    &ctx->pipeline->stages[SOC_AUDIO_INTERLVR_STAGE].
		    stage_params.interleaver;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			/* Initialize */
			interleaver->inputs[i].output_ch_count = 0;
			interleaver->inputs[i].reorder_for_hw = true;
			interleaver->inputs[i].output_ch_map =
			    SOC_AUDIO_INVALID_CHANNEL_CONFIG;
		}

	}
#endif /* INCLUDE_INTERLEAVER_STAGE */

#ifdef INCLUDE_PER_OUTPUT_DELAY_STAGE
	/* per output delay stage */
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_PER_OUTPUT_DELAY;
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].in_use = false;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].inputs_count =
	    0;
	ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].outputs_count =
	    0;
	{
		uint32_t i = 0;
		int32_t buffer_size = 0;
		struct soc_audio_per_output_delay_params *per_output_delay;
		per_output_delay =
		    &ctx->pipeline->stages[SOC_AUDIO_PER_OUTPUT_DELAY_STAGE].
		    stage_params.per_output_delay;
		buffer_size = 0;
		if (buffer_size > 0) {
			per_output_delay->max_delay_ms =
			    buffer_size / (192 * 8 * 4);
			per_output_delay->buffer_size = buffer_size;
			per_output_delay->enabled = true;
		} else {
			per_output_delay->enabled = false;
			per_output_delay->max_delay_ms = 0;
			per_output_delay->buffer_size = 0;
		}
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++)
			per_output_delay->output_params[i].buffer_reset = true;
	}
#endif /* INCLUDE_PER_OUTPUT_DELAY_STAGE */

#ifdef INCLUDE_OUTPUT_STAGE
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_OUT;
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].in_use = true;
	/* Updated in connect stages */
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].inputs_count = 0;
	ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].outputs_count = 0;
#endif /* INCLUDE_OUTPUT_STAGE */


#if 0
#ifdef INCLUDE_ENCODE_STAGE
	/* Encode stage */
	ctx->pipeline->stages[SOC_AUDIO_ENCODE_STAGE].task =
	    SOC_AUDIO_PIPELINE_TASK_ENCODE;
	ctx->pipeline->stages[SOC_AUDIO_ENCODE_STAGE].in_use = false;
	ctx->pipeline->stages[SOC_AUDIO_ENCODE_STAGE].inputs_count = 1;
	ctx->pipeline->stages[SOC_AUDIO_ENCODE_STAGE].outputs_count = 1;
	if (output_cfg->ac3_encoded)
		ctx->pipeline->stages[SOC_AUDIO_ENCODE_STAGE].in_use = true;
#endif /* INCLUDE_ENCODE_STAGE */

#endif
	{
		uint32_t i = 0;
		for (i = 0; i < ctx->pipeline->num_stages; i++) {
			soc_debug_print(DEBUG_LVL_2,
			"add_stages use %d, stage %d input %d outputt %d\n",
			ctx->pipeline->stages[i].in_use, i,
			ctx->pipeline->stages[i].inputs_count,
			ctx->pipeline->stages[i].outputs_count);
		}
	}

	result = SOC_SUCCESS;
EXIT:
	SOC_EXIT();
	return result;
}

/*
 * Set up the mixer config
 */
static void soc_audio_mixer_config_set_gain(struct soc_audio_channel_mix_config
					    *channel_mix_config,
					    int32_t one_to_one_val,
					    int32_t n_by_m_value)
{
	uint32_t input_channel, output_channel;

	for (input_channel = 0; input_channel < SOC_AUDIO_MAX_INPUT_CHANNELS;
	     input_channel++) {
		channel_mix_config->input_channels_map[input_channel].
		    input_channel_id = input_channel;
		for (output_channel = 0;
		     output_channel < SOC_AUDIO_MAX_OUTPUT_CHANNELS;
		     output_channel++) {
			if (input_channel == output_channel) {
				channel_mix_config->input_channels_map
				    [input_channel].output_channels_gain
				    [output_channel] = one_to_one_val;
			} else {
				channel_mix_config->input_channels_map
				    [input_channel].output_channels_gain
				    [output_channel] = n_by_m_value;
			}
		}
	}
}

/*
* Caller need to make sure to take a processor lock before the call.
* Check the output workload. The output workload must not NULL and inside the
* output list of the processor context.
*/
enum soc_result
audio_output_workload_validation(struct soc_audio_processor_context *ctx,
				 struct soc_audio_output_wl *output_wl)
{
	struct soc_audio_output_wl *curr_output = NULL;
	enum soc_result result = SOC_SUCCESS;
	/* Check the process context */
	if (!ctx) {
		soc_debug_print(ERROR_LVL, " Invalid process context ");
		result = SOC_ERROR_INVALID_PARAMETER;
		WARN_ON(1);
		goto EXIT;
	}
	/* This processor should be in use */
	if (ctx->ref_count == 0) {
		soc_debug_print(ERROR_LVL, " Processor not in use\n ");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto EXIT;
	}
	/* Validate output handle */
	if (!output_wl) {
		soc_debug_print(ERROR_LVL, " Invalid output handle ");
		result = SOC_ERROR_INVALID_HANDLE;
		goto EXIT;
	}
	/* See if the output_wl is in the processor */
	curr_output = ctx->outputs;
	while (curr_output && curr_output != output_wl)
		curr_output = curr_output->next_output;

	if (curr_output != output_wl) {
		soc_debug_print(ERROR_LVL,
				" Can not find output workload from "
				" processor context ");
		result = SOC_ERROR_INVALID_HANDLE;
		goto EXIT;
	}
EXIT:
	return result;
}

/*
 * Recover the related pipelines to resume the FW.
 */
enum soc_result soc_audio_pm_recover_pipe(uint32_t proc_h)
{
	enum soc_result result = SOC_SUCCESS;
	struct soc_audio_processor_context *ctx = NULL;

	SOC_ENTER();
	/* Validate processor handle */
	result = audio_processor_handle_validation(proc_h);
	if (SOC_SUCCESS != result)
		return result;

	/* Get the audio processor context and get the lock */
	ctx = &soc_audio_processor[proc_h];

	soc_debug_print(DEBUG_LVL_1,
		"Redo the pipe alloc and config after fw re-download\n");

#ifdef INCLUDE_SRC_STAGE
	{
		/* Reset vsrsc_init_flag to reinitialize aux_buffer in
		 VSRCInit() */
		struct soc_audio_pipeline_src_params *src;
		uint32_t i = 0;

		src = &ctx->pipeline->stages[SOC_AUDIO_SRC_STAGE].
		    stage_params.src;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++)
			src->vsrc_init_flag[i] = 0;
	}
#endif

	result = soc_ipc_alloc_pipe(ctx, ctx->pipeline);
	if (SOC_SUCCESS == result && ctx->pipeline->configured)
		result = soc_ipc_config_pipe(ctx, ctx->pipeline);

	SOC_EXIT();
	return result;
}

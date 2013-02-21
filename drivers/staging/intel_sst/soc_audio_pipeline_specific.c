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
  Santa Clara, CA  97052

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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_debug.h"
#include "soc_ipc.h"
#include "intel_sst.h"
#include "intel_sst_common.h"
#include "soc_audio_api.h"
#include "soc_audio_pipeline_specific.h"
#ifdef INCLUDE_LOOPBACK_IF
#include "intel_sst_loopback.h"
#endif

/**
This function does piplene initialialization specific to a bu

param  pipe_instance pipeline
param output configuration

retval none
 */
enum soc_result soc_bu_audio_post_proc_pipe_init_stages(struct
						soc_audio_pipeline
						*pipe_instance, const struct
						soc_audio_pipe_out
						*output_cfg)
{
	uint32_t i;

	SOC_ENTER();
	if (pipe_instance == NULL) {
		pr_err("Invalid argument passed\n");
		return SOC_ERROR_INVALID_PARAMETER;
	}
#ifdef INCLUDE_OUTPUT_STAGE
	if (true == pipe_instance->stages[SOC_AUDIO_OUTPUT_STAGE].in_use) {
		struct soc_audio_bu_specific_output_params *output;
		output =
		    &pipe_instance->stages[SOC_AUDIO_OUTPUT_STAGE].
		    stage_params.output.params[0];
#ifdef INCLUDE_LOOPBACK_IF
		output->loopback = loopback_if.user;
		output->max_streams = SND_SST_MAX_AUDIO_DEVICES;
		pr_debug("loopback is enabled %d mem %p max_streams %d\n",
				output->loopback, loopback_if.mem,
				output->max_streams);
		output->ring_buffer_address = virt_to_phys(loopback_if.mem);
		output->ring_buffer_size = loopback_if.size;
#endif
		output->num_chan = SOC_AUDIO_MAX_OUTPUT_CHANNELS;
	}
#endif
#ifdef INCLUDE_SRC_STAGE
	if (true == pipe_instance->stages[SOC_AUDIO_SRC_STAGE].in_use) {
		struct soc_audio_pipeline_src_params *src;
		bool same_output_sample_rate;
		src =
		    &pipe_instance->stages[SOC_AUDIO_SRC_STAGE].
		    stage_params.src;

		/* Judge whether all output ports have the same sample rate */
		same_output_sample_rate = true;
		for (i = 1; i < output_cfg->count; i++) {
			if (output_cfg->cfg[i].sample_rate
			    != output_cfg->cfg[0].sample_rate) {
				same_output_sample_rate = false;
				break;
			}
		}
		/* May has been set to other mode */
		if (src->mix_src_mode ==
			SOC_AUDIO_MIX_SAMPLE_RATE_MODE_INVALID)
			src->mix_src_mode = SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO;
		src->auto_output_sample_rate = SOC_AUDIO_OUTPUT_SAMPLE_RATE;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			/* set output sample rate */
			src->output_sample_rate[i] =
			    SOC_AUDIO_OUTPUT_SAMPLE_RATE;
			if (src->mix_src_mode ==
			    SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO) {
				if (same_output_sample_rate) {
					src->output_sample_rate[i] =
					    output_cfg->cfg[0].sample_rate;
				}
			}
		}
	}
#endif
#ifdef INCLUDE_MIXER_STAGE
	if (true == pipe_instance->stages[SOC_AUDIO_MIXER_STAGE].in_use) {
		struct soc_audio_mixer_params *mixer;
		mixer =
		    &pipe_instance->stages[SOC_AUDIO_MIXER_STAGE].
		    stage_params.mixer;
		/* Always use 2 channels */
		mixer->host.ch_config_mode = SOC_AUDIO_MIX_CH_CFG_MODE_FIXED;
		mixer->host.ch_config_mode_fixed_ch_cfg =
		    SOC_AUDIO_CHANNEL_CONFIG_2_CH;
	}
#endif
#ifdef INCLUDE_INTERLEAVER_STAGE
	if (true == pipe_instance->stages[SOC_AUDIO_INTERLVR_STAGE].in_use) {
		for (i = 0; i < SOC_AUDIO_MAX_OUTPUTS; i++) {
			/* Set the parameters according to the
			   output configuration */
			pipe_instance->
			    stages[SOC_AUDIO_INTERLVR_STAGE].stage_params.
			    interleaver.inputs[i].output_ch_count =
			    output_cfg->cfg[i].channel_count;
			pipe_instance->
			    stages[SOC_AUDIO_INTERLVR_STAGE].stage_params.
			    interleaver.inputs[i].output_smpl_size =
			    output_cfg->cfg[i].sample_size;
		}
	}
#endif /** INCLUDE_INTERLEAVER_STAGE */

	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This function allocates buffer for stages and connects prev stage
inputs to newly allocated buffers. It does output scaling for src stage.

param handle audio processor context given to upper layer
param pipe_instance pipeline

retval none
 */
enum soc_result soc_bu_audio_pipe_update_connect(struct soc_audio_pipeline
						 *pipe_instance)
{
	/* For decode pipeline we don't need to do anything here, because all
	 * buffers are static allocated in FW */
	uint32_t i = 0;
	struct soc_audio_pipeline_stage *stage;

	SOC_ENTER();
	for (i = 0; i < pipe_instance->num_stages; i++) {
		stage = &(pipe_instance->stages[i]);
		/* find whether stage is configured or not */
		if (stage->in_use != true) {
			soc_debug_print(DEBUG_LVL_2,
					"stage not in use %d\n", i);
			continue;
		}
		stage->bu_stage.handle = i;
	}

	SOC_EXIT();
	return SOC_SUCCESS;
}

/**
This function frees all the pipeline buffers. This should be called after
pipeline is deleted.

param handle audio processor context given to upper layer
param pipe_instance pipeline

retval none
 */
enum soc_result soc_bu_audio_pipe_disconnect(struct soc_audio_pipeline
					*pipe_instance __attribute__ ((unused)))
{
	return SOC_SUCCESS;
}

/* Used to indicate whether the space of is allocated */
static int pipeline_allocated[SOC_AUDIO_NUM_PIPELINES] = { 0, };

/* The IA shadow of pipelines which are stored in DSP DRAM */
char dram_pipelines_shadow[SOC_AUDIO_DEC_PIPE_SIZE
			   * (SOC_AUDIO_NUM_PIPELINES - 1)];

/**
This function allocates memory from SRAM or DRAM in DSP for pipelines.
param size size of memory to be alocated
param conext opaque handle to mmeory allocated. has ismd buffer attr
retval virual address of mmeory if sucessful otherwise none
 */
void *allocate_shared_memory(uint32_t size, void **context)
{
	int i, pipe_size;
	void *pipe_addr;

	pr_debug("SIZE requested =%d", size);
	pr_debug("SOC_AUDIO_PCM_PIPE_SIZE =%d", SOC_AUDIO_PCM_PIPE_SIZE);
	pr_debug("SOC_AUDIO_DEC_PIPE_SIZE =%d\n", SOC_AUDIO_DEC_PIPE_SIZE);
	pr_debug("Printing the decoder stage size =%d",
		 sizeof(struct soc_audio_pipeline_decode_params));
	pr_debug("Printing the mixer stage size =%d",
		 sizeof(struct soc_audio_mixer_params));
	pr_debug("Printing the src stage size =%d",
		 sizeof(struct soc_audio_pipeline_src_params));
	pr_debug("Printing the soc_audio_pipeline_decode_config_params=%d",
		 sizeof(union soc_audio_pipeline_decode_config_params));
	pr_debug("Printing the soc_audio_pipeline_stage_params =%d",
		 sizeof(union soc_audio_pipeline_stage_params));
	pr_debug("Printing the soc_audio_pipeline_bu_stage =%d",
		 sizeof(struct soc_audio_pipeline_bu_stage));

	/* There are total four pipelines:
	 * one capture, two decoders and one post processing.
	 */
	*context = NULL;
	for (i = 0; i < SOC_AUDIO_NUM_PIPELINES; i++) {
		if (i == 0) {
			pipe_size = SOC_AUDIO_PCM_PIPE_SIZE;
			pipe_addr = (void *)SOC_AUDIO_PCM_PIPE_ADDR;
		} else {
			pipe_size = SOC_AUDIO_DEC_PIPE_SIZE;
			pipe_addr = (void *)(dram_pipelines_shadow
			    + SOC_AUDIO_DEC_PIPE_SIZE * (i - 1));
		}
		if ((size <= pipe_size) && !pipeline_allocated[i]) {
			*context = pipe_addr;
			pipeline_allocated[i] = 1;
			break;
		}
	}
	return *context;
}

/**
This function frees pipelines' memory.

param buffer opaque handle returned during memory allocation
retval None
 */
void free_shared_memory(void *buffer)
{
	int i;
	void *pipe_addr;
	for (i = 0; i < SOC_AUDIO_NUM_PIPELINES; i++) {
		if (i == 0) {
			pipe_addr = (void *)SOC_AUDIO_PCM_PIPE_ADDR;
		} else {
			pipe_addr = dram_pipelines_shadow
			    + SOC_AUDIO_DEC_PIPE_SIZE * (i - 1);
		}
		if (buffer == pipe_addr) {
			pipeline_allocated[i] = 0;
			break;
		}
	}
	return;
}

/**
This function invokes appropriate send routine in IA->dsp dispatcher
param  message IPC message from soc audio
retval SOC_SUCCESS or appropriate failure code
 */
enum soc_result soc_audio_ipc_send_message(struct ipc_post *message)
{
	enum soc_result result = SOC_FAILURE;
	switch (soc_ipc_get_msg_id(message->header)) {
	case SOC_IPC_IA_ALLOC_PIPE:
	case SOC_IPC_IA_CONFIG_PIPE:
	case SOC_IPC_IA_START_PIPE:
	case SOC_IPC_IA_STOP_PIPE:
	case SOC_IPC_IA_FLUSH_PIPE:
	case SOC_IPC_IA_FREE_PIPE:
	case SOC_IPC_IA_GET_STREAM_INFO:
	case SOC_IPC_IA_GET_STAGE_PARAMS:
	case SOC_IPC_IA_STAGE_CONFIGURE:
	case SOC_IPC_IA_INPUT_JOB_AVAILABLE:
	case SOC_IPC_IA_SWITCH_CLOCK:
#ifdef _AUDIO_FW_PROFILING_
	case SOC_IPC_IA_FW_MESSAGE_PROFILE_DUMP:
#endif
		{
			result = sst_post_message(message);
			break;
		}
	default:
		pr_err("unknown message id %d\n",
			 soc_ipc_get_msg_id(message->header));
	}
	return result;
}

enum soc_result soc_bu_audio_pipe_set_default_decoder_param(enum
						 soc_audio_format
						 format,
						 struct soc_audio_codec_params
						 *params)
{
	enum soc_result result = SOC_SUCCESS;
	if ((format == SOC_AUDIO_MEDIA_FMT_DTS_HD_HRA) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DTS_HD_MA)) {
		/* The DTS-HD decoder doesn't distinguish between MA and HRA
		 * formats, so we can use the common DTSHD format to simplify
		 * firmware type checking
		 */
		params->bu_specific.algo = SOC_AUDIO_MEDIA_FMT_DTS_HD;
	} else
		params->bu_specific.algo = format;

	if ((format == SOC_AUDIO_MEDIA_FMT_DTS) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DTS_HD) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DTS_HD_HRA) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DTS_HD_MA) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DD) ||
	    (format == SOC_AUDIO_MEDIA_FMT_DD_PLUS) ||
	    (format == SOC_AUDIO_MEDIA_FMT_AAC) ||
	    (format == SOC_AUDIO_MEDIA_FMT_AAC_LOAS)) {
		int output;
		for (output = 0; output < SOC_AUDIO_MAX_OUTPUTS; output++) {
			params->bu_specific.outputs[output].ch_count = 0;
			params->bu_specific.outputs[output].dmix_mode
			    = SOC_AUDIO_DOWNMIX_INVALID;
		}
	}

	return result;
}

/**
This function processes all the pointers before sending the message to FW
param handle audio processor context given to upper layer
param pipe_instance pipeline
retval none */
void soc_audio_config_preprocess(struct soc_audio_pipeline *pipe,
				 void *dsp_addr_base_stage)
{
	uint32_t stage_loop = 0;
	struct soc_audio_pipeline_stage *curr_stage;
	struct soc_audio_pipeline_stage *prev_stage;
	struct soc_audio_pipeline_stage *base_stage;
	uint32_t input_cnt;

	prev_stage = NULL;
	base_stage = curr_stage = (pipe->stages);

	while (curr_stage != NULL && stage_loop < pipe->num_stages) {
		if (prev_stage == NULL) {
			prev_stage = curr_stage;
			stage_loop++;
			curr_stage = &(pipe->stages[stage_loop]);
			continue;
		}
		if (curr_stage->in_use == false) {
			stage_loop++;
			curr_stage = &(pipe->stages[stage_loop]);
			continue;
		}
		prev_stage->bu_stage.dsp_phys_next = dsp_addr_base_stage +
		    (int)((void *)curr_stage - (void *)base_stage);
		for (input_cnt = 0;
		     input_cnt < curr_stage->inputs_count; input_cnt++) {
			curr_stage->bu_stage.dsp_phys_inputs[input_cnt] =
			    dsp_addr_base_stage + (int)((void *)
				&(prev_stage->bu_stage.outputs[input_cnt])
					- (void *)base_stage);
			curr_stage->bu_stage.dsp_phys_inputs_orig[input_cnt] =
			    curr_stage->bu_stage.dsp_phys_inputs[input_cnt];
		}
		stage_loop++;
		prev_stage = curr_stage;
		curr_stage = &(pipe->stages[stage_loop]);
	}
}

enum soc_result audio_set_dma_registers(void *handle,
					struct stream_info *stream)
{
	struct soc_audio_input_wl *input_wl = NULL;
	struct soc_audio_pipeline *pipe_instance;
	struct soc_audio_pipeline *pcm_pipe_instance;
	struct soc_audio_bu_specific_input_params *input, *pcm_input;
	struct soc_audio_processor_context *ctx;
	enum soc_result result = SOC_SUCCESS;
	int i;

	SOC_ENTER();
	/* Get the input workload */
	input_wl = (struct soc_audio_input_wl *)handle;

	/* Get the audio processor context and get the lock */
	ctx = input_wl->ctx;
	/* Validate input workload */
	result = audio_input_workload_validation(ctx, input_wl);
	if (SOC_SUCCESS != result)
		goto EXIT;

	pcm_pipe_instance = input_wl->ctx->pipeline;
	pipe_instance = input_wl->pipeline;
	if (!pcm_pipe_instance || !pipe_instance) {
		pr_err("invalid parameter\n");
		goto EXIT;
	}

	if (!pcm_pipe_instance->started || !pipe_instance->started) {
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
						(struct soc_audio_input_wl *)
						stream->input_wl,
						SOC_DEV_STATE_PLAY);
		if (result != SOC_SUCCESS) {
			pr_err("Error in setting the dec pipe to play");
			goto EXIT;
		}
	}

	/* Setup the post prossesing pipeline input BU parameters. */
	pcm_input =
	    &pcm_pipe_instance->stages[SOC_AUDIO_INPUT_STAGE].stage_params.
	    input.params[stream->soc_input_id];

	pr_debug("PCM buf size %x, period count %d\n",
		pcm_input->ring_buffer_size, pcm_input->period_count);
	pr_debug("addr %x, rate %d sample size %d ch_count %d started =%d\n",
		pcm_input->ring_buffer_address,
		pcm_input->sample_rate, pcm_input->sample_size,
		pcm_input->channel_count, pcm_pipe_instance->started);

	/* Setup the decode pipeline input bu parameters */
	for (i = 0; i < pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].
		inputs_count; i++) {
		input = &pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].
			stage_params.input.params[i];
		if (input->pcm_input_id == stream->soc_input_id)
			break;
	}
	if (i == pipe_instance->
		stages[SOC_AUDIO_DEC_INPUT_STAGE].inputs_count) {
		pr_err("input not found in the decode pipeline!\n");
		goto EXIT;
	}

	/* Note that offload use the ring_buffer_address,
	 * ring_buffer_size from pcm pipeline input stage.*/
	pcm_input->ring_buffer_address = stream->ring_buffer_addr;
	pcm_input->ring_buffer_size = stream->ring_buffer_size;

	result = soc_ipc_input_job_available(input_wl->ctx, pipe_instance);
	if (result != SOC_SUCCESS)
		pr_err("sending input job available failed\n");
EXIT:
	SOC_EXIT();
	return result;
}

enum soc_result soc_bu_audio_input_set_state(struct soc_audio_processor_context
					     *ctx,
					     struct soc_audio_input_wl
					     *input_wl,
					     enum soc_dev_state state)
{
	enum soc_dev_state prev_state;
	struct soc_audio_pipeline *pipe_instance;
	enum soc_result result = SOC_SUCCESS;
	int i, stream_id;
	int frame_size, frame_count;
	struct ipc_ia_time_stamp_t __iomem *fw_tstamp;

	SOC_ENTER();

	/* Get the state now */
	prev_state = input_wl->state;
	if (state == prev_state)
		return result;

	/* Get the stream id */
	for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
		stream_id = sst_drv_ctx->active_streams[i];
		if (SND_SST_DEVICE_NONE == stream_id)
			continue;
		if (input_wl == sst_drv_ctx->streams[stream_id].input_wl)
			break;
	}
	if (i == SOC_AUDIO_MAX_INPUTS) {
		pr_err("%s : No active streams/devices\n", __func__);
		result = SOC_FAILURE;
		goto EXIT;
	}
	pr_debug("%s : stream_id %d -> %d\n", __func__, stream_id, state);

	if (STREAM_OPS_PLAYBACK == sst_drv_ctx->streams[stream_id].ops) {
		struct soc_audio_pipeline_stage *input_stage;
		struct soc_audio_bu_specific_input_params *input;
		struct soc_audio_buffer_attr *buf_attr;

		pipe_instance = ctx->pipeline;
		input_stage = &pipe_instance->stages[SOC_AUDIO_INPUT_STAGE];
		input = &input_stage->stage_params.input.params[i];

		switch (state) {
		case SOC_DEV_STATE_PAUSE:
			input->enable = false;
			break;
		case SOC_DEV_STATE_STOP:
			input->enable = false;
			/*Only need to switch clock for non-last inputs
			 * Refer to audio_input_set_to_stop @ soc_audio_inputs.c
			 */
			break;
		case SOC_DEV_STATE_PLAY:
			/* The logic point of set up stream information is
			 * set-to-play time, since it is called for each
			 * stream, and the param whould not change during
			 * the playback. Note even for encoded stream,
			 * the stream info is written to PCM pipeline input
			 * stage, not decode pipeline. */
			if (input_wl->format == SOC_AUDIO_MEDIA_FMT_PCM)
				input->in_type = SOC_AUDIO_BU_IN_PCM;
			else
				input->in_type = SOC_AUDIO_BU_IN_PCM_FROM_DEC;

			input->period_count =
				sst_drv_ctx->streams[stream_id].period_count;
			input->ring_buffer_address =
				sst_drv_ctx->streams[stream_id].
				ring_buffer_addr;
			input->ring_buffer_size =
				sst_drv_ctx->streams[stream_id].
				ring_buffer_size;
			input->sample_rate =
				sst_drv_ctx->streams[stream_id].sfreq;
			input->sample_size =
				sst_drv_ctx->streams[stream_id].pcm_wd_sz / 8;
			input->channel_count =
				sst_drv_ctx->streams[stream_id].num_chan;

			/* 1: X_ms_size must aligned to frame size */
			frame_size = input->sample_size * input->channel_count;
			frame_count =
				(input->sample_rate * SOC_AUDIO_BUFFER_TIME_MS)
				/ 1000;
			/* Firmware deinterleaver optimization on even number
			*  of frames. */
			frame_count = (frame_count / 2) * 2;
			/* Make it frame aligned */
			input->x_ms_size = (frame_count * frame_size);
			/* Reset some DMA flags */
			input->flag_started_ddr_dma = false;

			/* Reset sg_list so that the new info will be used */
			memset(&(input->sg_list), 0, sizeof(struct sg_list_t));

			/* update the time stamp */
			fw_tstamp = (struct ipc_ia_time_stamp_t *)
				(sst_drv_ctx->mailbox
				 + SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET);
			fw_tstamp +=
				sst_drv_ctx->streams[stream_id].soc_input_id;
			memset(fw_tstamp, 0,
				sizeof(struct ipc_ia_time_stamp_t));

			buf_attr = (struct soc_audio_buffer_attr *)
			&input_stage->bu_stage.outputs[i].metadata;
			buf_attr->sample_rate = input->sample_rate;
			/* bytes to bits */
			buf_attr->sample_size = input->sample_size * 8;
			buf_attr->channel_count = input->channel_count;
			switch (buf_attr->channel_count) {
			case 1:
				buf_attr->channel_config =
					SOC_AUDIO_CHANNEL_CONFIG_1_CH;
				break;
			case 2:
				buf_attr->channel_config =
					SOC_AUDIO_CHANNEL_CONFIG_2_CH;
				break;
			case 6:
				buf_attr->channel_config =
					SOC_AUDIO_CHANNEL_CONFIG_6_CH;
				break;
			case 8:
				buf_attr->channel_config =
					SOC_AUDIO_CHANNEL_CONFIG_8_CH;
				break;
			default:
				buf_attr->channel_config =
				SOC_AUDIO_INVALID_CHANNEL_CONFIG;
				break;
			}
			buf_attr->discontinuity = false;

			/* Finally enable the input */
			input->enable = true;
			pr_debug("%s : buf size is %d, addr %x, rate %d",
				__func__,
				input->ring_buffer_size,
				input->ring_buffer_address, input->sample_rate);
			pr_debug("x mes %d sample size %d ch_count %d\n",
				input->x_ms_size,
				input->sample_size, input->channel_count);
			pr_debug("SET_TO_PLAY .......");
			break;
		case SOC_DEV_STATE_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			break;
		}
	}
#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
	else if (STREAM_OPS_CAPTURE == sst_drv_ctx->streams[stream_id].ops) {
		struct soc_audio_bu_specific_output_params *output;
		pipe_instance = input_wl->pipeline;
		output = &pipe_instance->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].
			stage_params.output.params[0];
		switch (state) {
		case SOC_DEV_STATE_PAUSE:
			output->enable = false;
			break;
		case SOC_DEV_STATE_STOP:
			output->enable = false;
			break;
		case SOC_DEV_STATE_PLAY:
			if (input_wl->format == SOC_AUDIO_MEDIA_FMT_PCM) {
				output->period_count =
					sst_drv_ctx->streams[stream_id].
					period_count;
				output->sample_rate =
					sst_drv_ctx->streams[stream_id].sfreq;
				output->sample_size =
					sst_drv_ctx->streams[stream_id].
					pcm_wd_sz / 8;
				output->channel_count =
					sst_drv_ctx->streams[stream_id].
					num_chan;
				output->ring_buffer_address =
					sst_drv_ctx->streams[stream_id].
					ring_buffer_addr;
				output->ring_buffer_size =
					sst_drv_ctx->streams[stream_id].
					ring_buffer_size;

				/* update the time stamp */
				fw_tstamp = (struct ipc_ia_time_stamp_t *)
					(sst_drv_ctx->mailbox
					 + SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET);
				fw_tstamp +=
				sst_drv_ctx->streams[stream_id].soc_input_id;
				memset(fw_tstamp, 0,
					sizeof(struct ipc_ia_time_stamp_t));

				output->enable = true;

				pr_debug("ringbuf size=%d, addr=%x, period=%d",
				       output->ring_buffer_size,
				       output->ring_buffer_address,
				       output->period_count);
				pr_debug("rate=%d sample size=%d ch_count %d\n",
				       output->sample_rate,
				       output->sample_size,
				       output->channel_count);
				pr_debug("SET_TO_CAPTURE .......");
			}

			break;
		case SOC_DEV_STATE_INVALID:
		default:
			result = SOC_ERROR_INVALID_PARAMETER;
			break;
		}
	}
#endif
	SOC_EXIT();
EXIT:
	return result;
}

enum soc_result soc_bu_audio_dec_pipe_init_stages(struct soc_audio_pipeline
						  *pipe_instance)
{
	SOC_ENTER();
#ifdef INCLUDE_INPUT_STAGE
	if (true == pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].in_use) {
		struct soc_audio_bu_specific_input_params *input;
		struct stream_info *stream;
		int i, stream_id;

		/* Find the corresponding stream */
		/* xzhou: We can add stream info to avoid the while loop,
		   but I don't want to change the common data
		   structure currently. */
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			stream_id = sst_drv_ctx->active_streams[i];
			if (SND_SST_DEVICE_NONE == stream_id)
				continue;
			stream = &sst_drv_ctx->streams[stream_id];

			if (NULL ==
			    (struct soc_audio_input_wl *)stream->input_wl)
				continue;

			if (((struct soc_audio_input_wl *)stream->
			     input_wl)->pipeline != pipe_instance)
				continue;
			if (stream->status == STREAM_UN_INIT) {
				pr_err("%s: offload stream uninitialized\n",
				       __func__);
				return SOC_FAILURE;
			}

			input =
			    &pipe_instance->stages[SOC_AUDIO_DEC_INPUT_STAGE].
			    stage_params.input.params[0];
			    /* index is hardcoded to 0 since we can have only
			     * one input in the decode pipeline.
			     * We simulate a encoded stream as a raw pcm mono
			     * pcm stream with 1 byte smaple size. Ring buffer
			     * will be assigned later on to pcm pipeline input,
			     * and then updated to decode pipeline in FW. */
			input->period_count = 0;
			input->ring_buffer_address = 0;
			input->ring_buffer_size = 0;
			input->sample_rate = 0;
			input->sample_size = 1;
			input->channel_count = 1;
			input->in_type = SOC_AUDIO_BU_IN_DEC;
			pr_debug("offload buf size is %x, addr %x, rate %x\n",
				input->ring_buffer_size,
				input->ring_buffer_address, input->sample_rate);
			pr_debug("size %x ch_count %x\n",
				input->sample_size, input->channel_count);
			/* xzhou: store the stream id so we know which input
			   should be connected to when configure the decode
			   pipeline */
			input->pcm_input_id = i;
		}
	}
#endif /* INCLUDE_INPUT_STAGE */

	SOC_EXIT();
	return SOC_SUCCESS;
}

/*
* audio_find_pcm_slot - Find SSP slot
*
* @device: Device to be checked
* @num_ch: Number of channels queried
*
* This checks the deivce against the map and calculates pcm_slot value
*/
static uint32_t audio_find_pcm_slot(uint32_t device, uint32_t num_chan)
{
	uint32_t pcm_slot = 0;

	if (device == SND_SST_DEVICE_VIRTUAL_HEADSET)
		device = SND_SST_DEVICE_HEADSET;
	if (device == SND_SST_DEVICE_VIRTUAL_IHF)
		device = SND_SST_DEVICE_IHF;

	if (device == SND_SST_DEVICE_VIBRA && num_chan == 1)
		pcm_slot = 0x10;
	else if (device == SND_SST_DEVICE_HAPTIC && num_chan == 1)
		pcm_slot = 0x20;
	else if (device == SND_SST_DEVICE_IHF && num_chan == 1)
		pcm_slot = 0x04;
	else if (device == SND_SST_DEVICE_IHF && num_chan == 2)
		pcm_slot = 0x0C;
	else if (device == SND_SST_DEVICE_IHF && num_chan == 6)
		pcm_slot = 0x0C;
	else if (device == SND_SST_DEVICE_HEADSET && num_chan == 1)
		pcm_slot = 0x01;
	else if (device == SND_SST_DEVICE_HEADSET && num_chan == 2)
		pcm_slot = 0x03;
	else if (device == SND_SST_DEVICE_HEADSET && num_chan == 6)
		pcm_slot = 0x03;
	else if (device == SND_SST_DEVICE_CAPTURE && num_chan == 1)
		pcm_slot = 0x01;
	else if (device == SND_SST_DEVICE_CAPTURE && num_chan == 2)
		pcm_slot = 0x03;
	else if (device == SND_SST_DEVICE_CAPTURE && num_chan == 3)
		pcm_slot = 0x07;
	else if (device == SND_SST_DEVICE_CAPTURE && num_chan == 4)
		pcm_slot = 0x0F;
	else if (device == SND_SST_DEVICE_CAPTURE && num_chan > 4)
		pcm_slot = 0x1F;
	else if ((device == SND_SST_DEVICE_COMPRESSED_PLAY) ||
		 (device == SND_SST_DEVICE_COMPRESSED_CAPTURE))
		pcm_slot = sst_drv_ctx->compressed_slot;
	else {
		pr_err("device %d num_chan %d\n", device, num_chan);
	}

	pr_debug("returning slot =%x for device =%d\n", pcm_slot, device);
	return pcm_slot;
}

/*
* soc_audio_set_output_pcm_slot - Set output SSP slot
*
* @proc: handle to processor
* @device: Device to be checked
* @num_ch: Number of channels queried
*/
enum soc_result
soc_audio_set_output_pcm_slot(uint32_t proc_hdl, uint32_t device,
				       uint32_t num_chan)
{
	struct soc_audio_processor_context *ctx = NULL;
	struct soc_audio_bu_specific_output_params *output;
	uint32_t retval = 0;

	ctx = &soc_audio_processor[proc_hdl];
	output = &ctx->pipeline->stages[SOC_AUDIO_OUTPUT_STAGE].
		stage_params.output.params[0];

	retval = audio_find_pcm_slot(device, num_chan);
	if (retval == 0) {
		pr_err("slot 0 for device %d num_chan %d\n",
		       device, num_chan);
		return SOC_FAILURE;
	}

	output->time_slot = retval;
	return SOC_SUCCESS;
}

/* The channel layout assumption is
* 0: Front Left
* 1: Front Right
* 2: Front Center
* 3: Low Frequency
* 4: Back Left
* 5: Back Right
* 6: ..... (others maybe supported in the future)
* which is NOT the sequence in soc_audio_channel. The reason
* is that the numbers in soc_audio_channel are used as index of
* the input_gain_index table in mixer, and the current number value
* of soc_audio_channel will cause indexing out of boundary in case
* of AUDIO_MAX_OUTPUT_CHANNELS < 8.
*/
void generate_ch_mix_config(struct soc_audio_channel_mix_config *ch_mix_config,
			    int32_t volume, uint8_t num_chan)
{
	int input_ch = 0;
	int output_ch = 0;
	int gain = 0;
	int gain_mute = SOC_AUDIO_GAIN_MUTE;
	int gain_0db = SOC_AUDIO_GAIN_0_DB;
	int gain_user = volume;
	int gain_half = gain_user - 96;
	/* Note that the gain is in the context of user gain.
	   Check the boundary values */
	if (SOC_AUDIO_GAIN_MAX < gain_user)
		gain_user = SOC_AUDIO_GAIN_MAX;
	else if (SOC_AUDIO_GAIN_MUTE > gain_half)
		gain_user = gain_half = SOC_AUDIO_GAIN_MUTE;

	for (input_ch = 0;
		input_ch < SOC_AUDIO_MAX_INPUT_CHANNELS;
		input_ch++) {
		ch_mix_config->input_channels_map[input_ch].input_channel_id =
		    input_ch;
		for (output_ch = 0;
		     output_ch < SOC_AUDIO_MAX_OUTPUT_CHANNELS; output_ch++) {
			switch (SOC_AUDIO_OUTPUT_CHANNELS) {
			case 1:
				/* Mono ouput, map every channel to the sink */
				gain = gain_0db;
				break;
			case 2:
				/* Stereo output */
				switch (input_ch) {
				case 0:	/* Front Left */
					if (0 == output_ch)
						/* left */
						gain = gain_user;
					else
						/* right */
						gain = (num_chan == 1) ?
							gain_user : gain_mute;
					break;
				case 1:	/* Front Right */
					if (0 == output_ch)
						gain = gain_mute;
					else
						gain = gain_user;
					break;
				case 2:	/* Front Center */
					if (0 == output_ch)
						gain = gain_half;
					else
						gain = gain_half;
					break;
				case 3:	/* Low Frequency */
					if (0 == output_ch)
						gain = gain_mute;
					else
						gain = gain_mute;
					break;
				case 4:	/* Back Left */
					if (0 == output_ch)
						gain = gain_user;
					else
						gain = gain_half;
					break;
				case 5:	/* Back Right */
					if (0 == output_ch)
						gain = gain_half;
					else
						gain = gain_user;
					break;
				default:
					pr_err("%s : not supported"
						"input no of channels =%d\n",
						__func__, input_ch);
					gain = gain_mute;
				}
				break;
			default:
				pr_err("%s : not supported"
					"output number of channels =%d\n",
					__func__, SOC_AUDIO_OUTPUT_CHANNELS);
				gain = gain_mute;
			}
			/* Finally set the gain */
			ch_mix_config->
			    input_channels_map[input_ch].output_channels_gain
			    [output_ch] = gain;
		}		/*end of for */
	}			/*end of for */
}

#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
enum soc_result
soc_bu_audio_capture_pipe_init_stages(struct soc_audio_pipeline *pipe)
{
	enum soc_result result = SOC_SUCCESS;
	uint32_t retval = 0;
	SOC_ENTER();

	if (true == pipe->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].in_use) {
		int i, stream_id;
		struct stream_info *stream;
		struct soc_audio_bu_specific_input_params *input;
		struct soc_audio_bu_specific_output_params *output;

		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			stream_id = sst_drv_ctx->active_streams[i];
			if (SND_SST_DEVICE_NONE == stream_id)
				continue;
			stream = &sst_drv_ctx->streams[stream_id];

			if (!(struct soc_audio_input_wl *)stream->input_wl)
				continue;

			if (((struct soc_audio_input_wl *)stream->input_wl)->
			    pipeline != pipe)
				continue;

			if (stream->status == STREAM_UN_INIT) {
				pr_err("%s : capture stream uninitialized\n",
				       __func__);
				result = SOC_FAILURE;
				break;
			}

			input = &pipe->stages[SOC_AUDIO_CAPTURE_INPUT_STAGE].
				stage_params.input.params[0];
			output = &pipe->stages[SOC_AUDIO_CAPTURE_OUTPUT_STAGE].
				stage_params.output.params[0];

			/* Capture input parameters */
			input->sample_rate = SOC_AUDIO_CAPTURE_SAMPLE_RATE;
			input->channel_count = stream->num_chan;
			input->sample_size   = 2;
			input->period_count = stream->period_count;
			input->in_type   = SOC_AUDIO_BU_IN_CPR;
			retval = audio_find_pcm_slot(stream->device,
						     stream->num_chan);
			if (retval == 0) {
				pr_err("slot 0 for stream %d\n", stream_id);
				result = SOC_FAILURE;
				break;
			}
			input->time_slot = retval;

			/* Capture output parameters */
			output->sample_rate = stream->sfreq;
			output->sample_size = stream->pcm_wd_sz / 8;
			output->channel_count = stream->num_chan;
			output->period_count = stream->period_count;
			output->flag_started_ddr_dma = 0;
			output->ring_buffer_size = stream->ring_buffer_size;
			output->ring_buffer_address = stream->ring_buffer_addr;
			output->str_id = i;
		}

	}

	SOC_EXIT();
	return result;
}
#endif

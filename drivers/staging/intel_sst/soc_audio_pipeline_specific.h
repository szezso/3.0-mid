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
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, CLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.  NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, DIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (CLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER  CONTRACT, STRICT LIABILITY, OR TORT
  (CLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _SOC_AUDIO_PIPELE_SPECIFIC_H
#define _SOC_AUDIO_PIPELE_SPECIFIC_H
#include "soc_ipc_defs.h"
#include "soc_audio_processor_defs.h"
#include "soc_audio_bu_config.h"

enum soc_result soc_bu_audio_post_proc_pipe_init_stages(
	struct soc_audio_pipeline *pipe_instance,
	const struct soc_audio_pipe_out *in_output_cfg);
enum soc_result soc_bu_audio_dec_pipe_init_stages(
		struct soc_audio_pipeline *pipe_instance);
enum soc_result soc_bu_audio_pipe_disconnect(
	struct soc_audio_pipeline *pipe_instance);
enum soc_result soc_bu_audio_pipe_update_connect(
	struct soc_audio_pipeline *pipe_instance);
void *allocate_shared_memory(uint32_t size, void **context);
void free_shared_memory(void *buffer);
enum soc_result soc_audio_ipc_send_message(struct ipc_post *message);
bool soc_bu_audio_input_valid_sample_rate(int sample_rate);
bool soc_bu_audio_input_valid_channel_config(int channel_config);
bool soc_bu_audio_input_valid_sample_size(int sample_size);
int soc_bu_audio_get_channel_cnt_from_config(int channel_config);
enum soc_result soc_bu_audio_pipe_set_default_decoder_param(
		enum soc_audio_format format,
		 struct soc_audio_codec_params *params);
void *soc_bu_audio_stage_params_buffer_allocate(
	struct soc_audio_stage_params_buf_desc *params_buf_desc,
	uint32_t size);
enum soc_result soc_bu_audio_stage_params_buffer_free(
	struct soc_audio_stage_params_buf_desc *params_buf_desc);

void soc_audio_config_preprocess(struct soc_audio_pipeline *pipe,
				 void *dsp_addr_base_stage);

static inline enum soc_result soc_bu_audio_cache_align_check(void)
{
	return SOC_SUCCESS;
}

enum soc_result soc_bu_audio_input_set_state(
	struct soc_audio_processor_context *ctx,
	struct soc_audio_input_wl *input_wl,
	enum soc_dev_state state);

enum soc_result soc_audio_set_output_pcm_slot(
	uint32_t proc_hdl,
	uint32_t device,
	uint32_t num_chan);

void generate_ch_mix_config(struct soc_audio_channel_mix_config *ch_mix_config,
			    int32_t volume, uint8_t num_chan);
enum soc_result sst_post_message(struct ipc_post *msg);

#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
enum soc_result soc_bu_audio_capture_pipe_init_stages(struct soc_audio_pipeline
						  *pipe_instance);
#endif

#endif /*  _SOC_AUDIO_PIPELE_SPECIFIC_H */

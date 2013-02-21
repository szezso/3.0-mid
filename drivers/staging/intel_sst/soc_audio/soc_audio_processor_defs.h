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

#ifndef _SOC_AUDIO_PROCESSOR_DEFS_H
#define _SOC_AUDIO_PROCESSOR_DEFS_H


#include <linux/sched.h>
#include <linux/mutex.h>
#include "soc_audio_common.h"

struct soc_audio_ipc_condition {
	int condition;	/* condition for blocking check */
	int32_t result;		/* ret code when block is released */
	int32_t data;		/* data to be appended for block if any */
};

/* Audio Processor Output Port */
struct soc_audio_output_wl {
	bool enabled;
	struct soc_audio_processor_context *ctx; /* Pointer to parent
					 processor. */
	uint32_t sample_rate;
	bool reconfig_post_proc_pipe;
	uint32_t sample_size;
	uint16_t channel_count;
	uint16_t ch_map;
	enum soc_audio_channel_config channel_config;
	enum soc_audio_output_mode out_mode;
	enum soc_audio_downmix_mode dmix_mode;
	/* Link to next output */
	struct soc_audio_output_wl *next_output;
};

/* Audio Processor Input Port */
/* to do: pointer proc_handle should not be a uint32_t */
struct soc_audio_input_wl {
	enum soc_dev_state state;
	struct soc_audio_processor_context *ctx; /* Pointer to parent
					 processor. */
	bool disabled;

	/* Stream information */
	uint32_t sample_size;
	uint8_t channel_config;
	uint8_t channel_count;
	uint32_t sample_rate;
	enum soc_audio_format format;

	struct soc_audio_pipeline *pipeline;

	/* Link to next input_wl */
	struct soc_audio_input_wl *next_input;
};

/* Audio Processor */
struct soc_audio_processor_context {
	/* Global */
	struct mutex processor_lock;	/* Mutex lock for audio processor. */
	bool is_global_processor;
	bool muted;
	uint16_t ref_count;
	uint32_t handle; /* Index for self, context given to upper layer */
	struct soc_audio_ipc_condition ipc_block;
	wait_queue_head_t wait_queue;

	/* Pipeline */
	bool pipe_reconfigure_needed;
	struct soc_audio_pipeline *pipeline;

	/* I/O Config */
	uint8_t input_count;
	uint8_t output_count;
	struct soc_audio_input_wl *inputs;
	struct soc_audio_output_wl *outputs;

	/* Volume control */
	bool master_volume_enable;
	bool per_channel_volume_enable;
	int32_t master_volume;
	int32_t per_channel_volume[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	int32_t post_mix_ch_gain[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
};

/* Array of processor contexts */
extern struct soc_audio_processor_context
	soc_audio_processor[SOC_AUDIO_MAX_PROCESSORS];

/* Global init data */
extern struct soc_audio_init_data soc_audio_g_init_data;
extern bool soc_audio_init_done;

#endif /* _SOC_AUDIO_PROCESSOR_DEFS_H */

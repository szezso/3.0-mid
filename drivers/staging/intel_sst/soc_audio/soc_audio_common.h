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
#ifndef _SOC_AUDIO_COMMON_H
#define _SOC_AUDIO_COMMON_H


#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include "soc_audio_core.h"
#include "soc_audio_bu_config.h"
#include "soc_audio_defs.h"
#include "soc_audio_pipeline_defs.h"
#include "soc_ipc_defs.h"

typedef void *(*soc_alloc_shared_memory_t) (uint32_t, void **);
typedef void (*soc_free_shared_memory_t) (void *);
typedef enum soc_result (*soc_ipc_message_response_t) (void *, uint8_t *);
typedef int32_t(*soc_ipc_download_library_t) (void *, uint8_t, uint32_t);

struct soc_audio_init_data {
	struct workqueue_struct *wq;
	soc_audio_callback_t cb_underrun;
	soc_alloc_shared_memory_t alloc_mem;
	soc_free_shared_memory_t free_mem;
	struct mutex list_lock;	/* lock for IPC list */
	struct list_head ipc_dispatch_list;	/* list of messages */
	struct work_struct work;
	soc_ipc_message_response_t message_response;
	soc_ipc_download_library_t dnld_lib;
	wait_queue_head_t wait_queue;
	int condition;	/* condition for blocking check */
	int32_t result;		/* ret code when block is released */
	int32_t data;		/* data to be appended for block if any */
};
/**
   Output configuration parameters.
*/
struct soc_audio_internal_output {
	int32_t output_handle;	/* output index */
	bool in_use;		/* output in use or not */
	int32_t stream_delay;
				/**< Set the delay in miliseconds of output
				 stream.
				 Range 0-255 ms (1 ms step).*/
	int32_t sample_size;	/**< Set the output audio sample size.*/
	enum soc_audio_channel_config ch_config; /**< Set the output channel
						configuration*/
	enum soc_audio_output_mode out_mode; /**< Set the output mode.
			(i.e. SOC_AUDIO_OUTPUT_PCM or
			SOC_AUDIO_OUTPUT_PASSTHROUGH) */
	int32_t sample_rate; /**< Set the output stream's sample rate.*/
	int32_t ch_map;	/**<Set the output channel mapping
		(applicable in SW output only), 0 = use default driver
		channel mapping (disabled)*/
	uint16_t channel_count;
	int32_t dmix_mode;
};

struct soc_audio_pipe_out {
	uint32_t count;
	bool dts_encoded;
	bool ac3_encoded;
	bool matrix_dec_enabled;
	struct soc_audio_internal_output cfg[SOC_AUDIO_MAX_OUTPUTS];
};
#endif /* SOC_AUDIO_COMMON */

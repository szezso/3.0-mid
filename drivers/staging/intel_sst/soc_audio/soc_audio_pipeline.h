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

#ifndef _SOC_AUDIO_PIPELINE_H
#define _SOC_AUDIO_PIPELINE_H

#include "soc_audio_pipeline_defs.h"
#include "soc_audio_processor.h"
#include "soc_audio_defs.h"

struct soc_audio_pipeline *soc_audio_pipeline_add(uint32_t stages,
						  uint32_t type);
enum soc_result soc_audio_pipeline_delete(struct
					  soc_audio_processor_context
					  *ctx, struct soc_audio_pipeline
					  **pipe_instance);
enum soc_result audio_post_proc_pipe_setup(struct
					   soc_audio_processor_context
					   *ctx);
enum soc_result audio_dec_pipe_setup(struct
				     soc_audio_input_wl
				     *input_wl);
enum soc_result audio_post_proc_pipe_start(struct
					   soc_audio_processor_context
					   *ctx);
enum soc_result audio_post_proc_pipe_set_post_mix_stages(const struct
							 soc_audio_pipe_out
							 *output_cfg, struct
							 soc_audio_pipeline
							 *pipe_instance);
void audio_pipeline_read_output_configs(struct
					soc_audio_processor_context
					*ctx, struct soc_audio_pipe_out
					*out_cfg);

#if (defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE))
enum soc_result audio_capture_pipe_setup(struct soc_audio_input_wl *input_wl);
#endif

#endif /* _SOC_AUDIO_PIPELINE_H */

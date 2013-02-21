#ifndef __SOC_IPC_H__
#define __SOC_IPC_H__
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
  A PARTICULAR PURPOSE ARE DISCLAIMED. NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* soc_ipc.h
 * SOC IPC Layer APIs.
 * author Lomesh Agarwal, Arun Kannan
 * version 1.0
 *
 * SOC IPC Layer unifies the message format and flow between IA and audio DSP
 * across multiple platforms.
 * Messages are sent between audio DSP and IA using a command register and
 * two 1K mailboxes (one for IA to DSP and other for DSP to IA).
 * A typical audio pipeline creation and destruction message flow will look
 * like -
 *
 *	ALLOC_PIPE -> CONFIG_PIPE -> START_PIPE -> DROP_PIPE -> FREE_PIPE
 *
 * There can be two types of messages -
 *	-# Large message - these messages contain a paylod
 *	-# Short message - these message doesn't contain any paylod
 * This structure is sent as payload with ALLOC_PIPE message
 */

#include "soc_ipc_defs.h"
#include "soc_audio_pipeline.h"
#include "soc_audio_processor_defs.h"

/**
 * sets the msg_id field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param msg msg ID
 * return
 *	- NONE
 */
static inline void soc_ipc_set_msg_id(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~0xff;
	/* set up the new value */
	*header |= (value & 0xff);
	return;
}
/**
 * gets the msg_id field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- msg_id value in the header
 */
static inline uint32_t soc_ipc_get_msg_id(uint32_t header)
{
	return header & 0xff;
}

/**
 * sets the fw_handler field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param msg fw_handler
 * return
 *	- NONE
 */
static inline void soc_ipc_set_fw_handle(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~(0xf << 8);
	/* set up the new value */
	*header |= ((value & 0xf) << 8);
	return;
}

/**
 * gets the fw_handler field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- fw_handler fw_handler value in the header
 */
static inline uint32_t soc_ipc_get_fw_handle(uint32_t header)
{
	return (header & (0xf << 8)) >> 8;
}

/**
 * sets the type field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param msg type
 * return
 *	- NONE
 */
static inline void soc_ipc_set_type(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~(0x0f << 12);
	/* set up the new value */
	*header |= ((value & 0x0f) << 12);
	return;
}
/**
 * gets the type field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- type type value in the header
 */
static inline uint32_t soc_ipc_get_type(uint32_t header)
{
	return (header & (0x0f << 12)) >> 12;
}

/**
 * sets the size field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param data data
 * return
 *	- NONE
 */
static inline void soc_ipc_set_size(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~(0x3fff << 16);
	/* set up the new value */
	*header |= ((value & 0x3fff) << 16);
	return;
}

/**
 * gets the size field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- data data value in the header
 */
static inline uint32_t soc_ipc_get_size(uint32_t header)
{
	return (header & (0x3fff << 16)) >> 16;
}

/**
 * sets the done field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param msg done_bit
 * return
 *	- NONE
 */
static inline void soc_ipc_set_done(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~(0x1 << 30);
	/* set up the new value */
	*header |= ((value & 0x1) << 30);
	return;
}

/**
 * gets the done field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- done done value in the header
 */
static inline uint32_t soc_ipc_get_done(uint32_t header)
{
	return (header & (0x1 << 30)) >> 30 ;
}
/**
 * sets the busy field of a header
 * param header ipc header.
 * Assuming little endian machine
 * param msg busy_bit
 * return
 *	- NONE
 */
static inline void soc_ipc_set_busy(uint32_t *header, uint32_t value)
{
	/* clear the fields */
	*header &= ~(0x1 << 31);
	/* set up the new value */
	*header |= ((value & 0x1) << 31);
	return;
}

/**
 * gets the busy field from a header
 * param header ipc header.
 * Assuming little endian machine
 * return
 *	- busy value in the header
 */
static inline uint32_t soc_ipc_get_busy(uint32_t header)
{
	return	(header & (0x1 << 31)) >> 31;
}

enum soc_result soc_ipc_alloc_pipe(struct soc_audio_processor_context *ctx,
				   struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_config_pipe(struct soc_audio_processor_context *ctx,
				    struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_start_pipe(struct soc_audio_processor_context *ctx,
				   struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_stop_pipe(struct soc_audio_processor_context *ctx,
				  struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_flush_pipe(struct soc_audio_processor_context *ctx,
				   struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_free_pipe(struct soc_audio_processor_context *ctx,
				  struct soc_audio_pipeline *pipeline);

enum soc_result soc_ipc_send_fw_profile_dump(
					struct soc_audio_processor_context *ctx,
					struct soc_audio_pipeline *pipeline);

enum soc_result
soc_ipc_stage_get_params(struct soc_audio_processor_context *ctx,
			 struct soc_audio_pipeline *pipeline,
			 uint32_t stage_handle);

enum soc_result
soc_ipc_stage_configure(struct soc_audio_processor_context *ctx,
			struct soc_audio_pipeline *pipeline,
			uint32_t stage_handle);

enum soc_result
soc_ipc_set_input_volume(struct soc_audio_processor_context *ctx,
			 struct soc_audio_pipeline *pipeline,
			 uint32_t volume, uint32_t stream_id);

enum soc_result soc_ipc_input_job_available(struct
					    soc_audio_processor_context * ctx,
					    struct soc_audio_pipeline
					    *pipeline);

enum soc_result soc_ipc_switch_clock(struct soc_audio_processor_context *ctx,
				   struct soc_audio_pipeline *pipeline);
enum soc_result soc_ipc_process_reply(void *header, uint8_t * mailbox);

void soc_ipc_init_work_queue(struct soc_audio_init_data *init_data);

void soc_ipc_destroy_work_queue(struct soc_audio_init_data *init_data);

enum soc_result soc_ipc_waittimeout(struct soc_audio_init_data *init_data);

#endif /* __SOC_IPC_H__ */

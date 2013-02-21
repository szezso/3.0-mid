#ifndef __SOC_IPC_DEFS_H__
#define __SOC_IPC_DEFS_H__
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

/*! \file soc_ipc_defs.h
 * \brief SOC IPC Layer data structures.
 * \author Lomesh Agarwal, Arun Kannan
 * \version 1.0
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
 *
 * \warning <b>All IPC message body must be multiple of 32 bits.</b>
 */


#include <linux/list.h>
#include "soc_audio_defs.h"
#include "soc_audio_pipeline_defs.h"
#include "soc_audio_bu_config.h"

/*! \def INVALID_FW_HANDLE
 * Invalid FW handle value. FW handle is used to identify pipeline across IA
 * and DSP
 * it is 4 bit field in IPC header.
 * Invalid value is set to max value 15.
 */
#define INVALID_FW_HANDLE		15

/** @} */

/*! \def ipc_header
 * \brief IPC Message Header
 *
 * The IPC header is defined as a 32 bits field with the following format:
 * uint32_t ipc_header {
 *         msg_id:8;    < Message ID - Max 256 Message Types >
 *         fw_handle:4; < FW Handle  returned by FW >
 *         type:4;      < Pipe type can be used for routing >
 *         size:14;     < Size of message payload>
 *         done:1;      < bit 30 >
 *         busy:1;      < bit 31 >
 * };
 * The set and get methods are provided through helper macros, which are
 * definded in soc_ipc.h
 */

/*! \struct ipc_post
 * \brief IPC Message from IA to DSP
 *
 * IPC layer maintains a list of msgs. each msg is added to the list and
 * IP specific function is called to send the msg to DSP. this function will
 *  deque the msg from list and send it if DSP is free. otherwise
 * function will retunr. Driver will call this function again when driver
 *  receives DSP free interrupt and then the msg will be sent.
 */
struct ipc_post {
	struct list_head node;	/*!< for msg list management */
	uint32_t header;	/*!< msg header */
	int8_t *mailbox_data;	/*!< msg specific data. max size is 1K */
};

/*! \struct ipc_pipe_alloc
 * \brief ALLOC_PIPE msg payload
 *
 * This structure is sent as payload with ALLOC_PIPE message
 */
struct ipc_alloc_params {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/*! \struct ipc_pipe_alloc_response
 * \brief ALLOC_PIPE msg payload
 *
 * This structure is sent as payload with ALLOC_PIPE message
 */
struct ipc_alloc_params_response {
	uint16_t result;
	uint16_t fw_id;
	void *ctx;		/*!< context to identify the response. */
};

/*! \struct ipc_pipe_start
 * \brief START_PIPE msg payload
 *
 * This structure is sent as payload with START_PIPE message
 */
struct ipc_pipe_start {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/* struct ipc_pipe_response
 * brief msg payload
 *
 * This structure handles the following messages
 * payloads
 * start pipe message payload
 * stop pipe message payload
 * free pipe message payload
 * fw responds in case of config pipe
 * stream info response
 * stage configure response
 * clock switch response
 * fw profile dump response
 * input job available response
 */
struct ipc_pipe_response {
	uint16_t result;
	uint16_t reserved;
	void *ctx;              /*!< context to identify the response. */
};

/*! \struct ipc_pipe_stop
 * \brief STOP_PIPE msg payload
 *
 * This structure is sent as payload with STOP_PIPE message
 */
struct ipc_pipe_stop {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/*! \struct ipc_pipe_free
 * \brief FREE_PIPE msg payload
 *
 * This structure is sent as payload with FREE_PIPE message
 */
struct ipc_pipe_free {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/*! \struct ipc_config_params
 * \brief CONFIG_PIPE msg payload
 *
 * This structure is sent as payload with CONFIG_PIPE message
 */
struct ipc_config_params {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
	struct soc_audio_pipeline *pipe;
};

/* !\struct ipc_get_stream_info
 * \brief GET_STREAM_INFO msg payload
 *
 * This structure is sent as payload with GET_STREAM_INFO message
 */
struct ipc_get_stream_info {
	uint32_t stage_handle;
	void *ctx;		/*!< context to identify the response. */
};

/* !\struct ipc_stage_configure
 * \brief STAGE_CONFIGURE msg payload
 *
 * This structure is sent as payload with STAGE_CONFIGURE message
 */
struct ipc_stage_configure {
	uint32_t stage_handle;
	void *ctx;		/*!< context to identify the response. */
};

/* !\struct ipc_input_job_available
 * \brief INPUT_JOB_AVAILABLE msg payload
 *
 * This structure is sent as payload with STAGE_CONFIGURE message
 */
struct ipc_input_job_available {
	uint32_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/* !\struct ipc_fw_msg_profile_dump
 * \brief FW_MSG_PROFILE_DUMP msg payload
 *
 * This structure is sent as payload with FW_PROFILE_MSG_DUMP message
 */
struct ipc_fw_msg_profile_dump {
	uint16_t result;
	uint16_t reserved;
	void *ctx;              /*!< context to identify the response. */
};

/* !\struct ipc_switch_clock
 * \brief IPC_SWITCH_CLOCK msg payload
 *
 * This structure is sent as payload with SWITCH_CLOCK message
 */
struct ipc_switch_clock {
	uint16_t result;
	uint16_t reserved;
	void *ctx;		/*!< context to identify the response. */
};

/* Stream type params for decode pipelines */
struct snd_sst_str_type {
	uint8_t codec_type;	/* Codec type */
	uint8_t str_type;	/* 1 = voice 2 = music */
	uint8_t operation;	/* Playback or Capture */
	uint8_t protected_str;	/* 0=Non DRM, 1=DRM */
	uint8_t time_slots;
	uint8_t reserved;	/* Reserved */
	uint16_t result;	/* Result used for acknowledgment */
};

/* Library info structure */
struct module_info {
	uint32_t lib_version;
	uint32_t lib_type;
	uint32_t media_type;
	uint32_t lib_caps;
	uint8_t lib_name[12];
	uint8_t b_date[16];	/* Lib build date */
	uint8_t b_time[16];	/* Lib build time */
};

/* Library slot info */
struct lib_slot_info {
	uint32_t slot_num;	/* 1 or 2 */
	uint32_t iram_size;	/* slot size in IRAM */
	uint32_t dram_size;	/* slot size in DRAM */
	uint32_t iram_offset;	/* starting offset of slot in IRAM */
	uint32_t dram_offset;	/* starting offset of slot in DRAM */
};

struct snd_sst_lib_download {
	struct module_info lib_info; /* library info type, capabilities etc */
	struct lib_slot_info slot_info;	/* slot info to be downloaded */
	uint32_t mod_entry_pt;
};

struct snd_sst_lib_download_info {
	struct snd_sst_lib_download dload_lib;
	uint16_t result;	/* Result used for acknowledgment */
	uint8_t pvt_id;		/* Private ID */
	uint8_t reserved;	/* for alignment */
};
#endif /* __SOC_IPC_DEFS_H__ */

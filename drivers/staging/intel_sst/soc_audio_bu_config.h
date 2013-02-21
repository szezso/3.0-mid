/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2006-2010 Intel Corporation. All rights reserved.

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

  Copyright(c) 2006-2010 Intel Corporation. All rights reserved.
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

#ifndef _SOC_AUDIO_BU_CONFIG_H
#define _SOC_AUDIO_BU_CONFIG_H

#include "soc_audio_defs.h"

/* This file defines BU specific parameters for the converged audio driver */
#define _ENABLE_CLOCK_SWITCHING_
/*#define _AUDIO_FW_PROFILING_*/

/* Define to 1 to report out each stage performance via SVEN */
#define PIPE_STAGE_PERFORMANCE		0

#define SOC_AUDIO_MAX_PROCESSORS	2

/* 0 disable, 1 enable */
#define SOC_CACHELINE_PADDING		0

#define SOC_MAX_DSPS			1

#define SOC_AUDIO_MAX_INPUTS		3
#define SOC_AUDIO_MAX_OUTPUTS		1
#define SOC_AUDIO_MAX_DEC_PIPES		2
#define SOC_AUDIO_MAX_CAPTURE_PIPES	1

/* AAC decoder needs to support 8 channles. Use this MACRO for it. */
#define SOC_AUDIO_MAX_CHANNELS		8

#define SOC_AUDIO_MAX_INPUT_CHANNELS	6
#define SOC_AUDIO_MAX_OUTPUT_CHANNELS	2
#define SOC_AUDIO_MAX_INPUT_SAMPLE_RATE	48000
#define SOC_AUDIO_CAPTURE_SAMPLE_RATE	48000
#define SOC_AUDIO_OUTPUT_SAMPLE_RATE	48000
#define SOC_AUDIO_MAX_SAMPLE_WIDTH	2
#define SOC_AUDIO_OUTPUT_CHANNELS	2
#define SOC_AUDIO_BUFFER_TIME_MS	5

#define SOC_AUDIO_QUALITY_MAX_FILTERS  40
#define SOC_AUDIO_QUALITY_MAX_CONTROLS 16

#define SOC_AUDIO_MAIN_PIPE		0
#define SOC_AUDIO_DEC_PIPE		1
#define SOC_AUDIO_CPR_PIPE		2


#define MAX_NUM_SCATTER_BUFFERS 8

#define SOC_AUDIO_DSP_DRAM_ADDR		0x400000
#define SOC_AUDIO_DSP_SRAM_ADDR		0xFFFE1000
#define SOC_AUDIO_DSP_SRAM_SIZE		(64*1024)

#define SOC_AUDIO_DSP_MAILBOX_ADDR	0xFFFF1000
#define SOC_AUDIO_DSP_MAILBOX_SIZE	0x2000
/*! \def SOC_AUDIO_DSP_MAILBOX_SIZE
 * MAILBOX Size is 16K
 * 256 bytes for sending the message
 * 256 bytes for receiving the message
 * 512 bytes timestamp
 * 1K SRAM Checkpoint
 * 6K pipeline
 */

/*! \def SOC_IPC_TIMEOUT
 * Maximum time allowed to DSP to respond to IA message.
 * it is set to 5000 ms.
 */
#define SOC_IPC_TIMEOUT			5000

#define SOC_AUDIO_MAILBOX_SIZE_IPCX_SEND 0x080
#define SOC_AUDIO_MAILBOX_SIZE_IPCX_RCV	 0x080
#define SOC_AUDIO_MAILBOX_SIZE_IPCD_RCV	 0x100
#define SOC_AUDIO_MAILBOX_SIZE_TIMESTAMP 0x200
#define SOC_AUDIO_MAILBOX_SIZE_CHKPOINT	 0x400
#define SOC_AUDIO_MAILBOX_SIZE_PIPELINE	 0x1800

#define SOC_AUDIO_MAILBOX_SEND_OFFSET	0x0000
#define SOC_AUDIO_MAILBOX_IPCX_RCV_OFFSET (SOC_AUDIO_MAILBOX_SEND_OFFSET +\
					 SOC_AUDIO_MAILBOX_SIZE_IPCX_SEND)
#define SOC_AUDIO_MAILBOX_IPCD_RCV_OFFSET (SOC_AUDIO_MAILBOX_IPCX_RCV_OFFSET +\
					 SOC_AUDIO_MAILBOX_SIZE_IPCX_RCV)
#define SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET (SOC_AUDIO_MAILBOX_IPCD_RCV_OFFSET +\
					   SOC_AUDIO_MAILBOX_SIZE_IPCD_RCV)
#define SOC_AUDIO_MAILBOX_CHKPOINT_OFFSET (SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET +\
					   SOC_AUDIO_MAILBOX_SIZE_TIMESTAMP)
#define SOC_AUDIO_MAILBOX_PIPELINE_OFFSET (SOC_AUDIO_MAILBOX_CHKPOINT_OFFSET + \
					   SOC_AUDIO_MAILBOX_SIZE_CHKPOINT)

/* There are total 4 pipelines in our design: one post processing pipeline,
 * two decode pipelines and one capture pipeline. As there is only 6K in SRAM
 * to contain pipelines, only post processing pipeline is placed in SRAM
 * and all the decode pipelines are placed in DSP DRAM (8k).
 */
#define SOC_AUDIO_NUM_PIPELINES		(SOC_AUDIO_MAX_DEC_PIPES + \
					 SOC_AUDIO_MAX_CAPTURE_PIPES + 1)
#define SOC_AUDIO_DRAM_PIPELINE_OFFSET	0x5E000
#define SOC_AUDIO_DRAM_PIPELINE_SIZE	0x2000

/* Supposed to be the only pipeline in Mailbox */
#define SOC_AUDIO_PCM_PIPE_SIZE		SOC_AUDIO_MAILBOX_SIZE_PIPELINE
#define SOC_AUDIO_DEC_PIPE_SIZE		0xA00
#define SOC_AUDIO_PCM_PIPE_ADDR		(sst_drv_ctx->mailbox + \
					SOC_AUDIO_MAILBOX_PIPELINE_OFFSET)
#define SOC_AUDIO_DEC_PIPE_ADDR	(sst_drv_ctx->dram + \
				SOC_AUDIO_DRAM_PIPELINE_OFFSET)
/* Boundary check */
#if (SOC_AUDIO_DEC_PIPE_SIZE * (SOC_AUDIO_NUM_PIPELINES - 1)) > 8*1024
#error DRAM3 violation.
#endif

/* The IA shadow of pipelines which are stored in DSP DRAM */
extern char dram_pipelines_shadow[SOC_AUDIO_DEC_PIPE_SIZE
				  * (SOC_AUDIO_NUM_PIPELINES - 1)];

#define SOC_AUDIO_DSP_ISRX		0x18
#define SOC_AUDIO_DSP_IMRX		0x28
#define SOC_AUDIO_DSP_IPCD		0x40
#define SOC_AUDIO_DSP_IPCX		0x38
#define SOC_AUDIO_DSP_IPCD_INTR		0x02
#define SOC_AUDIO_DSP_IPCX_INTR		0x01
#define SOC_AUDIO_DSP_BUSY_DONE_MASK	0x3fffffff
#define SOC_AUDIO_DSP_IPCD_HOST_DONE	0x40000000
#define SOC_AUDIO_DSP_IPCX_HOST_DONE	0x40000000

/* supported decode and encode formats */
#define INCLUDE_MPEG_DECODE
#define INCLUDE_AAC_DECODE

/* supported pipeline stages */
#define INCLUDE_INPUT_STAGE
#define INCLUDE_DEINTERLEAVER_STAGE
#define INCLUDE_SRC_STAGE
#define INCLUDE_MIXER_STAGE
#define INCLUDE_INTERLEAVER_STAGE
#define INCLUDE_OUTPUT_STAGE
#define INCLUDE_DECODE_STAGE

#define SOC_AUDIO_BUFFER_SIZE (8*1024)
#define SOC_AUDIO_DECODE_AUX_BUFFER_SIZE (384*1024)
#define SOC_AUDIO_DECODE_BUFFER_SIZE SOC_AUDIO_BUFFER_SIZE

#define INCLUDE_CPR_INPUT_STAGE
#define INCLUDE_CPR_OUTPUT_STAGE
#define INCLUDE_LOOPBACK_IF

enum soc_audio_available_pipeline_stages {
	SOC_AUDIO_INPUT_STAGE = 0,
	SOC_AUDIO_DEINTERLVR_STAGE,
	SOC_AUDIO_SRC_STAGE,
	SOC_AUDIO_MIXER_STAGE,
	SOC_AUDIO_INTERLVR_STAGE,
	SOC_AUDIO_OUTPUT_STAGE,
	/*Add more stages indexes here */
	SOC_AUDIO_STAGE_COUNT
};

enum soc_audio_dec_pipeline_stages {
	SOC_AUDIO_DEC_INPUT_STAGE = 0,
	SOC_AUDIO_DEC_DECODE_STAGE,
	SOC_AUDIO_DEC_OUTPUT_STAGE,
	SOC_AUDIO_DEC_STAGE_COUNT
};

struct soc_audio_tags_desc {
	int32_t tag_buffer_id_start;
	/*First buffer that has tags assciated to this buffer */
	int32_t tag_buffer_id_end;
	/*Last buffer that has tags associated to this buffer. */
};

/** @defgroup ipc_msgs Message IDs for IPC messages
 *  @{
 *    Bits B7: DSP or IA ; B6-B4: Msg Category; B3-B0: Msg Type
 *
 *    IA to DSP pipeline control msgs
 *    (0-127) request (127-255) response
 *   }
 **/
#define SOC_IPC_IA_ALLOC_PIPE                   0x21
#define SOC_IPC_IA_FREE_PIPE                    0x22
#define SOC_IPC_IA_START_PIPE                   0x23
#define SOC_IPC_IA_STOP_PIPE                    0x24
#define SOC_IPC_IA_CFG_PARAM_SET                0x25
#define SOC_IPC_IA_CFG_PARAM_GET                0x26
#define SOC_IPC_IA_INPUT_JOB_AVAILABLE          0x27
#define SOC_IPC_IA_CONFIG_PIPE                  0x28
#define SOC_IPC_IA_BUFFER_AVAILABLE             0x2A
#define SOC_IPC_IA_FLUSH_PIPE                   0x2B
#define SOC_IPC_IA_GET_CODEC_VER_STRING         0x2D
#define SOC_IPC_IA_STAGE_CONFIGURE              0x2E
#define SOC_IPC_IA_INIT_INFO                    0x2F
#define SOC_IPC_IA_ENABLE_SVEN_DEBUG            0x30
#define SOC_IPC_IA_GET_STREAM_INFO              0x31
#define SOC_IPC_IA_GET_STAGE_PARAMS             0x32
#define SOC_IPC_IA_PLAY_FRAMES                  0x33
#define SOC_IPC_IA_CAPT_FRAMES                  0x34
#define SOC_IPC_IA_PLAY_VOICE                   0x35
#define SOC_IPC_IA_CAPT_VOICE                   0x36
#define SOC_IPC_IA_DECODE_FRAMES                0x37
#define IPC_IA_SET_RUNTIME_PARAMS		0x38
#define SOC_IPC_IA_SWITCH_CLOCK                 0x39
#define SOC_IPC_IA_FW_MESSAGE_PROFILE_DUMP      0x40
/* DSP to IA Msg */
#define SOC_IPC_IA_ALLOC_PIPE_DONE              0x81
#define SOC_IPC_IA_CONFIG_PIPE_DONE             0x82
#define SOC_IPC_IA_START_PIPE_DONE              0x83
#define SOC_IPC_IA_STOP_PIPE_DONE               0x84
#define SOC_IPC_IA_FREE_PIPE_DONE               0x85
#define SOC_IPC_IA_GET_STREAM_INFO_DONE         0x86
#define SOC_IPC_IA_GET_STAGE_PARAMS_DONE        0x87
#define SOC_IPC_IA_STAGE_CONFIGURE_DONE         0x88
#define SOC_IPC_IA_FW_INIT_CMPLT                0x89
#define SOC_IPC_IA_GETTING_STALLED              0x8A
#define SOC_IPC_IA_UNSTALLED                    0x8B
#define SOC_IPC_IA_DSP_FREE                     0x8C
#define SOC_IPC_IA_OUTPUT_JOB_AVAILABLE         0x8D
#define SOC_IPC_IA_INPUT_JOB_CONSUMED           0x8E
#define SOC_IPC_IA_STAGE_EXE_ERROR              0x8F
#define SOC_IPC_IA_MAILBOX_HAS_MSG              0x90
#define SOC_IPC_IA_NOTIFICATION_EVENT           0x91
#define SOC_IPC_IA_CODEC_AVAILABLE              0x92
#define SOC_IPC_IA_CODEC_VER                    0x93
#define SOC_IPC_IA_FLUSH_PIPE_DONE              0x94
#define SOC_IPC_IA_INPUT_JOB_AVAILABLE_DONE	0x95
#define SOC_IPC_IA_SWITCH_CLOCK_DONE            0x96
#define SOC_IPC_IA_FW_MESSAGE_PROFILE_DUMP_DONE 0x97
#define SOC_IPC_MAX_MESSAGE_COUNT               0xFF

/**
   This structure is used to specify in-band buffer metadata attributes.
   This data will be in the "attributes" field of soc_buffer_descriptor_t
   structures associated with buffers read from an audio output port.
*/
struct soc_audio_buffer_attr {
	enum soc_audio_format audio_format;
	uint32_t sample_size;
	uint32_t channel_count;
	uint32_t sample_rate;
	uint32_t channel_config;
	uint32_t bitrate;
	uint32_t discontinuity;
	/**< If true, this buffer is the first buffer after a
	discontinuity has been detected. */
	int32_t opt_metadata_offset;
	/**< Offset of optional metadata in the buffer payload,
	 or OPT_METADATA_INVALID when not present */

	/* For codec only */
	struct soc_audio_tags_desc tags;
	uint32_t ad_valid;
	uint32_t ad_fade_byte;
	uint32_t ad_pan_byte;
	uint32_t acmod;
#ifdef INCLUDE_AAC_DECODE
	uint32_t aac_downmix_metadata;
#endif
};

/* Buffer Descriptor for stage parameters buffers */
struct soc_audio_stage_params_buf_desc {
	void *dsp_phys_address;
	/* Address is always specified in DSP address range.
	 IA_phys_add - remap_address */
};

struct soc_audio_codec_buf_desc {
	uint32_t size;
	void *dsp_phys_address;
};

struct soc_audio_metadata_node {
	int32_t frame_id;
	int32_t input_range_begin;
	int32_t input_range_end;
	uint32_t eos;
	uint32_t tags_only;
	struct soc_audio_buffer_attr metadata;
};

/* Max number of frames that the decoder can keep internally. */
#define SOC_AUDIO_MAX_INFLIGHT_FRAMES 2

struct soc_audio_metadata_association {
	int32_t input_index;	/* association queue input pointer */
	int32_t output_index;	/* association queue output pointer */
	struct soc_audio_metadata_node
	 association_queue[SOC_AUDIO_MAX_INFLIGHT_FRAMES];
};

struct soc_audio_description {
	int32_t transmit_count;
	uint32_t valid;
	uint32_t fade;
	uint32_t pan;
	uint32_t pad;
};

struct dma_buf_desc {
	uint8_t *base_ptr;	/* the base address of the buffer */
	uint8_t *rd_ptr;	/* the current read address */
	uint8_t *wr_ptr;	/* the current write address */
	uint32_t size_total;	/* size of the buffer in bytes */
	uint32_t size_valid_data;	/* valid data available in bytes */
	uint32_t size_empty;	/* empty space available to write in bytes */
	uint32_t mem_mgr_idx;	/* Buffer index returned by memory manager */
	uint32_t dummy;
};

/* Buffer Descriptor */
struct soc_audio_decode_buf_desc {
	void *dsp_phys_address;
	/* Address is always specified in DSP address range.
	IA_phys_add - remap_address */
	uint32_t size;
	uint32_t level;		/* Level of the valid data inside the buffer. */
	uint32_t eos;		/* eos tag. */
};

/* Buffer Descriptor */
struct soc_audio_pipeline_buf_desc {
	void *dsp_phys_address;
	/* Address is always specified in DSP address range.
	 IA_phys_add - remap_address */
	uint32_t size;
	uint32_t level;
	/* Level of the valid data inside the buffer. */
	uint32_t eos;
	/* eos tag. */
	uint32_t tags;
	/* buffer has tags */
	struct soc_audio_buffer_attr metadata;
};

/** Information the codec needs about the output configurations. */
struct soc_audio_output_info {
	uint32_t ch_count;
	enum soc_audio_downmix_mode dmix_mode;
};

/* Address and size info of a frame buffer in DDR */
struct sst_address_info {
	uint32_t addr;		/* Address at IA */
	uint32_t size;		/* Size of the buffer */
};

/* Frame info to play or capture */
struct sst_frame_info {
	uint32_t num_entries;	/* number of entries to follow */
	struct sst_address_info addr[MAX_NUM_SCATTER_BUFFERS];
};
/** Variables used by the codec stage that are changed
by the host and picked up by the DSP.  */
struct soc_audio_bu_specific_decode_config_params {
	int32_t pri_dec_stage_addr;
	/** Host physical address of primary decoder.
	 Used for mix metadata purposes. */
	uint32_t ignore_eos;
	/** Flag to force codec to ignore eos (don't deinit) but propagate.
	 Used for post processing stages i.e. encoders. */
	enum soc_audio_format algo;
	/** Codec to be used for processing in the stage. */
	struct soc_audio_output_info outputs[SOC_AUDIO_MAX_OUTPUTS];
	/** Contains the downmix mode of each output to the processor.
	 Used for downmix coeff. */
};

struct soc_audio_bu_specific_decode_params {
	/* Host initializes these variables once,
	then they are used by the DSP during runtime. */
	struct soc_audio_codec_buf_desc aux_buffer;
	/** Used to store tables, scratch, persistant mem
	needed by codecs. */
	struct soc_audio_codec_buf_desc encode_buffer;
	/** Used as a temporary memory location to packetize
	 the encoder's output*/
	struct soc_audio_codec_buf_desc mda_buffer;
	/** Meta-data association buffer storage..*/
	struct soc_audio_decode_buf_desc decode_buffer;
	/** Decoder input buffer, data waiting to be decoded stored here. */
	struct soc_audio_description ad;
	/** Audio description metadata container */

	/* These variables are only read and written to by the DSP. */
	void *xa_process_api;
	/** Tensilica decoder API object.*/
	void *xa_process_handle;
	/** Tensilica function pointer.*/
	void *error_info;
	/** Tensilica error info strucute pointer. */
	struct soc_audio_metadata_association *md_associate;
	/** Pointer to metadata association queue memory. */
	uint32_t codec_init_done;
	/** Used by codec wrapper to know if it has been initialized yet.*/
	uint32_t new_input;
	/** Used by codec wrapper to know if a new input buffer has arrived.*/
	uint32_t metadata_changed;
	/** Used to specify if stream in-band metadata has changed. */
	uint32_t input_requested;
	/** Flag to show know how much the codec requests
	before it starts operating.*/
	uint32_t input_buf_offset;
	/** Offset into the input buffer to know how much has been
	used already.*/
	uint32_t reinit_after_eos;
	/** Used to reinit the decoder after EOS is encountered.*/
	uint32_t processing_eos;
	/** Flag used to know if the decoder is in the middle of
	processing EOS. */
	uint32_t out_ch_count;
	/** Use to store output channel count from the decoder. */
	uint32_t out_samp_freq;
	/** Use to store output sample freq from the decoder. */
	uint32_t out_data_rate;
	/** Use to store output bit rate from the decoder. */
	uint32_t out_ch_config;
	/** Use to store output ch config from the decoder. */
	uint32_t prev_ch_map;
	/** Use to keep track of the previous channel configuration
	 to determine when it changes. */
	uint32_t prev_sample_rate;
	/** Use to keep track of the previous sample rate to
	determine when it changes.*/
	int32_t acmod;
	int prev_exe_status;
	/** Use to keep track of the previous DO_EXECUTE status.
	 value could be DECODE_ERROR, SYNC_LOST, SYNC_FOUND */
	uint32_t ms10dec_enabled;
	/** Flag to tell the DSP this DECODE is for MS10 specific purpose */
	uint32_t ms10dec_main;
	/** Flag to tell the DSP this is ms10 main DCV decoder */
	int32_t ms10dec_ddc_sync;
	/** Hosst physical address of ddc frame sync stage. */
};

struct soc_audio_pipeline_connection_metadata {
	int output[SOC_AUDIO_MAX_OUTPUTS];
	int input[SOC_AUDIO_MAX_INPUTS];
};

struct soc_audio_pipeline_stage_bu_params {
	struct soc_audio_pipeline_buf_desc params_buffer;
};

struct soc_audio_pipeline_stage;

typedef int (*bu_stage_func_t) (struct soc_audio_pipeline_stage *stage,
				int wl_id);

struct soc_audio_pipeline_bu_stage {
	int handle;
	/* Usually the index into the array to which this element belongs*/
	void *dsp_phys_inputs[SOC_AUDIO_MAX_INPUTS];
	void *dsp_phys_inputs_orig[SOC_AUDIO_MAX_INPUTS];

	struct soc_audio_pipeline_buf_desc outputs[SOC_AUDIO_MAX_INPUTS];

	bu_stage_func_t func;
	bu_stage_func_t flush_func;

	struct soc_audio_pipeline_connection_metadata conn_mda;

	/* Important flag. In general, "done" indicates a stage has
	consumed its input buffer.
	ex - The decoder has copied the input data into the cubby dec buffer.
	Not necessarily decoded the buffer but it has consumed it.
	Most other stages always consume the given buffer but the
	decode stage acts differently. If the cubby decode buffer
	is full the dec stage can consume part or nothing of the input buffer.
	In the case of input and output stages this flag takes on a
	slightly different meaning.
	Input Stage - There was an input job in the input queue.
		False means there is no input job in the input job queue.
	Output Stage - The output buffers were successfully
		enqueued into the output job queue.
	   False means there is no space in the output job queue. */
	uint32_t done;

	void *dsp_phys_next;
	/* Used by DSP  next stage pointer */
};

#define SOC_AUDIO_CODEC_COUNT (SOC_AUDIO_MEDIA_FMT_COUNT + 1)

struct soc_audio_codecs_available {
	uint32_t codec_exists[SOC_AUDIO_CODEC_COUNT];
	uint32_t codec_ms10_ddt;
	uint32_t codec_ms10_ddc;
	uint32_t done_flag;
};

/*memory block structure with address and size*/
struct memory_block {
	uint32_t address;	/*source or destination address */
	uint32_t size;		/*transfer size */
};

/* DDR scatter-gather buffers information and associated information.
 * lpe_address_info_t will be defined in ipc.h */
struct sg_list_node_t {
	struct memory_block sg_addr;
	struct sg_list_node_t *next_buf;
};

struct sg_list_t {
	struct sg_list_node_t head;
	struct sg_list_node_t *cur_dma_entry;
	uint32_t cur_entry_rdwr_ptr;
	uint32_t num_elements;
	uint32_t total_buf_size;
};

/* Time stamp */
/* There is no IPC for time stamp, it will be made available in
	SRAM fixed location; it is dynamically updated by Firmware
	for each stream */
struct ipc_ia_time_stamp_t {
	uint32_t hw_pointer_delay;
	/* HW pointer delay */
	uint32_t samples_rendered;
	/* playback - data rendered */
	uint32_t samples_processed;
	/* capture - data in DDR */
	uint32_t data_consumed;
	/* Data has been consumed and samples processed
	is updated */
	uint32_t bytes_processed;
	/* number of bytes processed(DMA transferred)*/
};

enum {
	SOC_AUDIO_BU_IN_PCM,
	/* Input/Output of PCM pipeline */
	SOC_AUDIO_BU_IN_DEC,
	/* Input/Output of DEC pipeline */
	SOC_AUDIO_BU_IN_PCM_FROM_DEC,
	/* Input of PCM pipeline from DEC pipeline */
	SOC_AUDIO_BU_IN_CPR
	/* Input/output of Capture pipeline */
};

enum {
	/* Callback to middleware that next write id OK */
	SOC_AUDIO_WRITE_OK,
	/* Callback to middleware that pause is OK */
	SOC_AUDIO_PAUSE_OK
};

struct soc_audio_bu_specific_input_params {
	struct sg_list_t sg_list;
	uint32_t ring_buffer_address;
	uint32_t ring_buffer_size;
	struct dma_buf_desc buffer;
	uint32_t sample_rate;
	uint32_t sample_size;
	uint32_t channel_count;
	uint32_t x_ms_size;
	uint32_t period_count;
	bool flag_started_ddr_dma;
	struct ipc_ia_time_stamp_t *str_time_stamp_ptr;
	uint32_t enable;
	uint32_t in_type;
	/* Used for decode pipeline to show which input it connect to. */
	uint32_t pcm_input_id;
	uint32_t time_slot;
};
struct soc_audio_bu_specific_output_params {
	uint32_t num_chan;
	/*Desired output channel (Mono, stereo) */
	uint32_t time_slot;
	uint32_t enable;
	/* For capture */
	uint32_t sample_rate;
	uint32_t sample_size;
	uint32_t channel_count;
	uint32_t period_count;
	struct sg_list_t sg_list;
	uint32_t ring_buffer_size;
	uint32_t ring_buffer_address;
	uint32_t flag_started_ddr_dma;
	struct ipc_ia_time_stamp_t *str_time_stamp_ptr;
	uint32_t str_id;
#ifdef INCLUDE_LOOPBACK_IF
	uint32_t loopback;
	uint32_t max_streams;
#endif
};

#define SOC_AUDIO_CHANNEL_0_VALUE (0 << 8)
#define SOC_AUDIO_CHANNEL_1_VALUE (1 << 8)
#define SOC_AUDIO_CHANNEL_2_VALUE (2 << 8)
#define SOC_AUDIO_CHANNEL_3_VALUE (3 << 8)
#define SOC_AUDIO_CHANNEL_4_VALUE (4 << 8)
#define SOC_AUDIO_CHANNEL_5_VALUE (5 << 8)
#define SOC_AUDIO_CHANNEL_6_VALUE (6 << 8)
#define SOC_AUDIO_CHANNEL_7_VALUE (7 << 8)

#define SOC_AUDIO_LEFT_CHAN_ROUTE_INDEX             0
#define SOC_AUDIO_RIGHT_CHAN_ROUTE_INDEX            2
#define SOC_AUDIO_LFE_CHAN_ROUTE_INDEX              5
#define SOC_AUDIO_CENTER_CHAN_ROUTE_INDEX           1
#define SOC_AUDIO_LEFT_SUR_CHAN_ROUTE_INDEX         3
#define SOC_AUDIO_RIGHT_SUR_CHAN_ROUTE_INDEX        4
#define SOC_AUDIO_LEFT_REAR_SUR_CHAN_ROUTE_INDEX    6
#define SOC_AUDIO_RIGHT_REAR_SUR_CHAN_ROUTE_INDEX   7

#define SOC_AUDIO_AAC_TARGET_LEVEL_MAX 127
#define SOC_AUDIO_AAC_TARGET_LEVEL_MIN 0
#define SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MAX 100
#define SOC_AUDIO_AAC_DRC_COMPRESS_FAC_MIN 0
#define SOC_AUDIO_AAC_DRC_BOOST_FAC_MAX 100
#define SOC_AUDIO_AAC_DRC_BOOST_FAC_MIN 0

#define SOC_AUDIO_DEFAULT_LEFT_CH_ROUTE       SOC_AUDIO_CHANNEL_0_VALUE | \
					SOC_AUDIO_LEFT_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_RIGHT_CH_ROUTE      SOC_AUDIO_CHANNEL_1_VALUE | \
					SOC_AUDIO_RIGHT_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_LFE_CH_ROUTE        SOC_AUDIO_CHANNEL_2_VALUE | \
					SOC_AUDIO_LFE_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_CENTER_CH_ROUTE     SOC_AUDIO_CHANNEL_3_VALUE | \
					SOC_AUDIO_CENTER_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_L_SUR_CH_ROUTE      SOC_AUDIO_CHANNEL_4_VALUE | \
					SOC_AUDIO_LEFT_SUR_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_R_SUR_CH_ROUTE      SOC_AUDIO_CHANNEL_5_VALUE | \
					SOC_AUDIO_RIGHT_SUR_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_L_REAR_SUR_CH_ROUTE SOC_AUDIO_CHANNEL_6_VALUE | \
				SOC_AUDIO_LEFT_REAR_SUR_CHAN_ROUTE_INDEX;
#define SOC_AUDIO_DEFAULT_R_REAR_SUR_CH_ROUTE SOC_AUDIO_CHANNEL_7_VALUE | \
				SOC_AUDIO_RIGHT_REAR_SUR_CHAN_ROUTE_INDEX;

/* Mpeg Default Parameters */
#define MPEG_CRC_CHECK  false
#define MPEG_PCM_WD_SZ  16
#define MPEG_MCH_ENABLE false

/** Channel Maps used by the firmware is different.
 * See SOC_AUDIO_CH_CONFIG_HW_OUT_XCH.
 * This is the defule value if user does not give any perference.
 * Note: the number here is actually a index to Mixer gain table.
 * So, the number should never be larger then the actual number of
 * channels you have (or FW supported) */
enum {
	/** Invalid channel configuration. No channels are present.*/
	SOC_AUDIO_INVALID_CHANNEL_CONFIG = 0xFFFFFFFF,
	/** Eample of a common mono channel config, specifies Left only */
	SOC_AUDIO_CHANNEL_CONFIG_1_CH = 0xFFFFFFF0,
	/** Eample of a common stereo channel config.*/
	SOC_AUDIO_CHANNEL_CONFIG_2_CH = 0xFFFFFF10,
	/** Eample of a common 5.1 channel config */
	SOC_AUDIO_CHANNEL_CONFIG_6_CH = 0xFF543210,
	/** Eample of a common 7.1 channel config.*/
	SOC_AUDIO_CHANNEL_CONFIG_8_CH = 0x76543210
};

#define SOC_AUDIO_CH_CONFIG_HW_OUT_2CH 0xFFFFFF10
#define SOC_AUDIO_CH_CONFIG_HW_OUT_6CH 0xFF543210
#define SOC_AUDIO_CH_CONFIG_HW_OUT_8CH 0x76543210

#if defined(INCLUDE_CPR_INPUT_STAGE) && defined(INCLUDE_CPR_OUTPUT_STAGE)
enum soc_audio_capture_pipeline_stages {
	SOC_AUDIO_CAPTURE_INPUT_STAGE = 0,
	SOC_AUDIO_CAPTURE_OUTPUT_STAGE,
	SOC_AUDIO_CAPTURE_STAGE_COUNT
};
#endif

/* AUDIO_DSP_MESSAGE_HTOD_PIPE_CONFIGURE */
struct audio_htod_pipe_conf_t {
	uint32_t result;
	void *ctx;
	void *cur_in_job;
	void *prev_in_job;
	void *cur_out_job;
	void *prev_out_job;
	uint32_t num_stages;
	struct soc_audio_pipeline_stage *psm_stages;
};

#endif /*_SOC_AUDIO_BU_CONFIG_H */

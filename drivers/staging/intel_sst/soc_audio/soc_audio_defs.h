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

#ifndef _SOC_AUDIO_DEFS_H
#define _SOC_AUDIO_DEFS_H

#include "soc_audio_aac_defs.h"

#define SOC_BAD_HANDLE				-1
/* 8 steps per dB (0.125dB increments) */
#define SOC_AUDIO_MIX_COEFF_STEPS_PER_DB	8
/* Index of max gain (+18dB) */
#define SOC_AUDIO_MIX_COEFF_INDEX_MAX		0
#define SOC_AUDIO_MIX_COEFF_INDEX_0_DB		144    /* Index of 0 db gain */
/* Index of Mute (negative infinity) */
#define SOC_AUDIO_MIX_COEFF_INDEX_MUTE		1304
#define SOC_AUDIO_MAX_GAIN_TABLE		2

enum soc_result {
	SOC_SUCCESS = 0,
	SOC_ERROR_FEATURE_NOT_IMPLEMENTED = 1,
	SOC_ERROR_FEATURE_NOT_SUPPORTED = 2,
	SOC_ERROR_INVALID_VERBOSITY_LEVEL = 3,
	SOC_ERROR_INVALID_PARAMETER = 4,
	SOC_ERROR_INVALID_HANDLE = 5,
	SOC_ERROR_NO_RESOURCES = 6,
	SOC_ERROR_INVALID_RESOURCE = 7,
	SOC_ERROR_INVALID_QUEUE_TYPE = 8,
	SOC_ERROR_NO_DATA_AVAILABLE = 9,

	SOC_ERROR_NO_SPACE_AVAILABLE = 10,
	SOC_ERROR_TIMEOUT = 11,
	SOC_ERROR_EVENT_BUSY = 12,
	SOC_ERROR_OBJECT_DELETED = 13,

	SOC_ERROR_ALREADY_INITIALIZED = 17,
	SOC_ERROR_IOCTL_FAILED = 18,
	SOC_ERROR_INVALID_BUFFER_TYPE = 19,
	SOC_ERROR_INVALID_FRAME_TYPE = 20,

	SOC_ERROR_QUEUE_BUSY = 21,
	SOC_ERROR_NOT_FOUND = 22,
	SOC_FAILURE = 23,
	SOC_ERROR_PORT_BUSY = 24,

	SOC_ERROR_NULL_POINTER = 25,
	SOC_ERROR_INVALID_REQUEST = 26,
	SOC_ERROR_OUT_OF_RANGE = 27,
	SOC_ERROR_NOT_DONE = 28,
	SOC_ERROR_SUSPENDED = 29,	/* Low-power state */

	SOC_LAST_NORMAL_ERROR_MARKER,

	SOC_ERROR_UNSPECIFIED = 99
};

enum soc_dev_state {
	SOC_DEV_STATE_INVALID = 0,
	SOC_DEV_STATE_STOP = 1,
	SOC_DEV_STATE_PAUSE = 2,
	SOC_DEV_STATE_PLAY = 3,
	SOC_DEV_STATE_DISABLE = 4,
	SOC_DEV_STATE_ENABLE = 5
};

/* Defines a list of possible audio stream formats.
\enum soc_audio_format
*/
enum soc_audio_format {
	SOC_AUDIO_MEDIA_FMT_INVALID, /*Invalid or Unknown algorithm*/
	SOC_AUDIO_MEDIA_FMT_PCM, /*stream is linear PCM data*/
	/*stream is linear PCM data coming from a DVD (Program Stream)*/
	SOC_AUDIO_MEDIA_FMT_DVD_PCM,
	/*stream is linear PCM data coming from a BD (Transport Stream)*/
	SOC_AUDIO_MEDIA_FMT_BLURAY_PCM,
	/*stream uses mpeg 1 or mpeg 2 bc algorithm*/
	SOC_AUDIO_MEDIA_FMT_MPEG,
	/*stream uses mpeg 2 or mpeg 4 aac algorithm*/
	SOC_AUDIO_MEDIA_FMT_AAC,
	/*stream uses MPEG-2 or MPEG-4 AAC algorithm with LOAS header format*/
	SOC_AUDIO_MEDIA_FMT_AAC_LOAS,
	SOC_AUDIO_MEDIA_FMT_DD,	/*stream uses ac3 decode algorithm*/
	SOC_AUDIO_MEDIA_FMT_DD_PLUS, /*stream uses ac3+ algorithm*/
	SOC_AUDIO_MEDIA_FMT_TRUE_HD, /*stream uses dolby TrueHD algorithm*/
	SOC_AUDIO_MEDIA_FMT_DTS_HD, /*stream uses ac3 encode algorithm*/
	SOC_AUDIO_MEDIA_FMT_DTS_HD_HRA,
	SOC_AUDIO_MEDIA_FMT_DTS_HD_MA,
	SOC_AUDIO_MEDIA_FMT_DTS, /*stream uses dts encode algorithm*/
	/*stream uses dts low bitrate encode algorithm*/
	SOC_AUDIO_MEDIA_FMT_DTS_LBR,
	/*stream uses dts broadcast decode algorithm*/
	SOC_AUDIO_MEDIA_FMT_DTS_BC,
	SOC_AUDIO_MEDIA_FMT_WM9, /*stream uses WindowMedia 9 algorithm*/
	/*audio capture format is compressed*/
	SOC_AUDIO_MEDIA_FMT_IEC61937_COMPRESSED,
	SOC_AUDIO_ENCODE_FMT_AC3,
	SOC_AUDIO_ENCODE_FMT_DTS,
	SOC_AUDIO_ENCODE_FMT_TRUEHD_MAT,
	SOC_AUDIO_MATRIX_DECODER_DTS_NEO6,
	SOC_AUDIO_MEDIA_FMT_COUNT /*Maximum number of Encoding Algorithms*/
};

enum soc_audio_mix_ch_cfg_mode {
	SOC_AUDIO_MIX_CH_CFG_MODE_INVALID,
	SOC_AUDIO_MIX_CH_CFG_MODE_AUTO,
	SOC_AUDIO_MIX_CH_CFG_MODE_FIXED,
	SOC_AUDIO_MIX_CH_CFG_MODE_PRIMARY
};

enum soc_audio_mix_sample_rate_mode {
	SOC_AUDIO_MIX_SAMPLE_RATE_MODE_INVALID,
	SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO,
	SOC_AUDIO_MIX_SAMPLE_RATE_MODE_FIXED,
	SOC_AUDIO_MIX_SAMPLE_RATE_MODE_PRIMARY
};

enum soc_audio_status {
	SOC_AUDIO_STATUS_OKAY = 0,
	SOC_AUDIO_STATUS_NO_DATA_AVAIL = 1 << 0,
	SOC_AUDIO_STATUS_DECODE_ERROR = 1 << 1,
	SOC_AUDIO_STATUS_UNSUPPORTED_FORMAT = 1 << 2,
	SOC_AUDIO_STATUS_UNSUPPORTED_CONVERSION = 1 << 3,
};

enum soc_audio_downmix_mode {
	SOC_AUDIO_DOWNMIX_INVALID,
	SOC_AUDIO_DOWNMIX_DEFAULT,
	SOC_AUDIO_DOWNMIX_1_0,
	SOC_AUDIO_DOWNMIX_1_0_LFE,
	SOC_AUDIO_DOWNMIX_2_0,
	SOC_AUDIO_DOWNMIX_2_0_NO_SCALE,
	SOC_AUDIO_DOWNMIX_2_0_LFE,
	SOC_AUDIO_DOWNMIX_2_0_LTRT,
	SOC_AUDIO_DOWNMIX_2_0_LTRT_NO_SCALE,
	SOC_AUDIO_DOWNMIX_2_0_DOLBY_PRO_LOGIC_II,
	SOC_AUDIO_DOWNMIX_2_0_DOLBY_PRO_LOGIC_II_NO_SCALE,
	SOC_AUDIO_DOWNMIX_2_0_DVB_AAC,
	SOC_AUDIO_DOWNMIX_2_1,
	SOC_AUDIO_DOWNMIX_2_1_LFE,
	SOC_AUDIO_DOWNMIX_3_0,
	SOC_AUDIO_DOWNMIX_3_0_LFE,
	SOC_AUDIO_DOWNMIX_3_1,
	SOC_AUDIO_DOWNMIX_3_1_LFE,
	SOC_AUDIO_DOWNMIX_2_2,
	SOC_AUDIO_DOWNMIX_2_2_LFE,
	SOC_AUDIO_DOWNMIX_3_2,
	SOC_AUDIO_DOWNMIX_3_2_LFE,
	SOC_AUDIO_DOWNMIX_3_0_1,
	SOC_AUDIO_DOWNMIX_3_0_1_LFE,
	SOC_AUDIO_DOWNMIX_2_2_1,
	SOC_AUDIO_DOWNMIX_2_2_1_LFE,
	SOC_AUDIO_DOWNMIX_3_2_1,
	SOC_AUDIO_DOWNMIX_3_2_1_LFE,
	SOC_AUDIO_DOWNMIX_3_0_2,
	SOC_AUDIO_DOWNMIX_3_0_2_LFE,
	SOC_AUDIO_DOWNMIX_2_2_2,
	SOC_AUDIO_DOWNMIX_2_2_2_LFE,
	SOC_AUDIO_DOWNMIX_3_2_2,
	SOC_AUDIO_DOWNMIX_3_2_2_LFE
};

enum soc_audio_ops_type {
	PIPE_OPS_PLAYBACK = 0,	/* Decode */
	PIPE_OPS_CAPTURE,	/* Encode */
	PIPE_OPS_PLAYBACK_DRM,	/* Play Audio/Voice */
	PIPE_OPS_PLAYBACK_ALERT,	/* Play Audio/Voice */
	PIPE_OPS_CAPTURE_VOICE_CALL,	/* CSV Voice recording */
	PIPE_OPS_POSTPROCESS
};

enum soc_audio_pipeline_op {
	PIPE_TYPE_MUSIC = 1,
	PIPE_TYPE_VOICE
};

/* ------------------------------------------------------------------------- */
/* Tasks and their corresponding structures. Each task would require different
   information as input, hence different structures. */
/* ------------------------------------------------------------------------- */
enum soc_audio_pipeline_stage_task {
	SOC_AUDIO_PIPELINE_TASK_UNKNOWN,	/* 0 */
	SOC_AUDIO_PIPELINE_TASK_ECHO,	/* 1 */
	SOC_AUDIO_PIPELINE_TASK_IN,	/* 2 */
	SOC_AUDIO_PIPELINE_TASK_OUT,	/* 3 */
	SOC_AUDIO_PIPELINE_TASK_DECODE,	/* 4 */
	SOC_AUDIO_PIPELINE_TASK_ENCODE,	/* 5 */
	SOC_AUDIO_PIPELINE_TASK_MIX,	/* 6 */
	SOC_AUDIO_PIPELINE_TASK_SRC,	/* 7 */
	SOC_AUDIO_PIPELINE_TASK_DOWNMIX,	/* 8 */
	SOC_AUDIO_PIPELINE_TASK_INTERLEAVER,	/* 9 */
	SOC_AUDIO_PIPELINE_TASK_DEINTERLEAVER,	/* 10 */
	SOC_AUDIO_PIPELINE_TASK_BM,	/* 11 */
	SOC_AUDIO_PIPELINE_TASK_DM,	/* 12 */
	SOC_AUDIO_PIPELINE_TASK_DATA_DIV,	/* 13 */
	SOC_AUDIO_PIPELINE_TASK_PCM_DECODE,	/* 14 */
	SOC_AUDIO_PIPELINE_TASK_PACKETIZE_ENCODED_DATA,	/* 15 */
	SOC_AUDIO_PIPELINE_TASK_DATA_ACCUMULATOR,	/* 16 */
	SOC_AUDIO_PIPELINE_TASK_PRESRC,	/* 17 */
	SOC_AUDIO_PIPELINE_TASK_WATERMARK,	/* 18 */
	SOC_AUDIO_PIPELINE_TASK_BIT_DEPTH_CONVERTER,	/* 19 */
	SOC_AUDIO_PIPELINE_TASK_AUDIO_QUALITY,	/* 20 */
	SOC_AUDIO_PIPELINE_TASK_PER_OUTPUT_DELAY,	/* 21 */
	SOC_AUDIO_PIPELINE_TASK_MS10_DDC_SYNC,	/* 22 */
	SOC_AUDIO_PIPELINE_TASK_WAIT_IO_EVENT,	/* 23 */
	SOC_AUDIO_PIPELINE_TASK_PIPE_TIME,	/* 24 */
};

/**
This enumeration contains the supported crossover frequencies.
\enum soc_audio_crossover_freq
*/
enum soc_audio_crossover_freq {
	SOC_AUDIO_CROSSOVER_FREQ_INVALID,
	SOC_AUDIO_CROSSOVER_FREQ_40HZ,
	SOC_AUDIO_CROSSOVER_FREQ_60HZ,
	SOC_AUDIO_CROSSOVER_FREQ_80HZ,
	SOC_AUDIO_CROSSOVER_FREQ_90HZ,
	SOC_AUDIO_CROSSOVER_FREQ_100HZ,
	SOC_AUDIO_CROSSOVER_FREQ_110HZ,
	SOC_AUDIO_CROSSOVER_FREQ_120HZ,
	SOC_AUDIO_CROSSOVER_FREQ_160HZ,
	SOC_AUDIO_CROSSOVER_FREQ_200HZ,
};

typedef void (*soc_audio_callback_t) (void *);

/* Since soc_audio_format does not differentiate between MPEG 1/2 or Layer 2/3
this enum is there to provide that differentiation. */
enum soc_audio_mpeg_layer {
	SOC_AUDIO_MPEG_LAYER_UNKNOWN = 0,
	SOC_AUDIO_MPEG1_LAYER_LII = 1,
	SOC_AUDIO_MPEG1_LAYER_LIII = 2,
	SOC_AUDIO_MPEG2_LAYER_LII = 3,
};

/** Use these values to specify a channel in channel configurations.
   4 bits of a 32 bit integer are used to specify a channel configuration.
   See \ref SOC_AUDIO_CHAN_CONFIG_8_CH for an example channel configuration.
   These values are used while specifying a channel configuration of an PCM
   input while calling the \ref soc_audio_input_set_pcm_format or while
   filling out the ch_map member of \ref soc_audio_output_config.
*/
enum soc_audio_channel {	/* Do NOT change the ordering of this enum.*/
	SOC_AUDIO_CHANNEL_LEFT = 0,
	SOC_AUDIO_CHANNEL_CENTER,	/* 1 */
	SOC_AUDIO_CHANNEL_RIGHT,	/* 2 */
	SOC_AUDIO_CHANNEL_LEFT_SUR,	/* 3 */
	SOC_AUDIO_CHANNEL_RIGHT_SUR,	/* 4 */
	SOC_AUDIO_CHANNEL_LEFT_REAR_SUR,	/* 5 */
	SOC_AUDIO_CHANNEL_RIGHT_REAR_SUR,	/* 6  */
	SOC_AUDIO_CHANNEL_LFE,	/* 7  */
	SOC_AUDIO_CHANNEL_INVALID = 0xF,
};

/**
This enumeration defines the output channel configurations supported.
\enum soc_audio_channel_config
*/
enum soc_audio_channel_config {
	SOC_AUDIO_CHAN_CONFIG_INVALID,
	SOC_AUDIO_STEREO, /**< Right and Left channel data*/
	SOC_AUDIO_DUAL_MONO, /**< Right channel data*/
	SOC_AUDIO_5_1, /**< 6 channel data*/
	SOC_AUDIO_7_1, /**< 8 channel data*/
};

/*
This enumeration defines output format of audio stream.
\enum soc_audio_destination
*/
enum soc_audio_output_mode {
	SOC_AUDIO_OUTPUT_INVALID,
	SOC_AUDIO_OUTPUT_PCM,
	SOC_AUDIO_OUTPUT_PASSTHROUGH,
	SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL,
	SOC_AUDIO_OUTPUT_ENCODED_DTS
};

/* Types to differentiate audio inputs. */
enum soc_audio_input_type {
	SOC_AUDIO_INPUT_TYPE_OTHER = 0,
	SOC_AUDIO_INPUT_TYPE_PRIMARY = 1,
	SOC_AUDIO_INPUT_TYPE_SECONDARY = 2
};

#define SOC_AUDIO_AC3_PERIOD			6144
#define SOC_AUDIO_MPEG1_LAYER1_PERIOD		1536
#define SOC_AUDIO_MPEG1_LAYER2_3_PERIOD		4608
#define SOC_AUDIO_MPEG2_WITHOUT_EXTN_PERIOD	4608
#define SOC_AUDIO_MPEG2_WITH_EXTN_PERIOD	4608
#define SOC_AUDIO_MPEG2_AAC_PERIOD		4096
#define SOC_AUDIO_MPEG2_LAYER1_PERIOD		3072
#define SOC_AUDIO_MPEG2_LAYER2_PERIOD		9216
#define SOC_AUDIO_MPEG2_LAYER3_PERIOD		4608
#define SOC_AUDIO_DTS_TYPE1_PERIOD		2048
#define SOC_AUDIO_DTS_TYPE2_PERIOD		4096
#define SOC_AUDIO_DTS_TYPE3_PERIOD		8192
#define SOC_AUDIO_ATRAC_PERIOD			2048
#define SOC_AUDIO_ATRAC_2_3_PERIOD		4096
#define SOC_AUDIO_ATRAC_X_PERIOD		8192
#define SOC_AUDIO_DTS_TYPE4_0_PERIOD		2048
#define SOC_AUDIO_DTS_TYPE4_1_PERIOD		4096
#define SOC_AUDIO_DTS_TYPE4_2_PERIOD		8192
#define SOC_AUDIO_DTS_TYPE4_3_PERIOD		16384
#define SOC_AUDIO_DTS_TYPE4_4_PERIOD		32768
#define SOC_AUDIO_DTS_TYPE4_5_PERIOD		65536
#define SOC_AUDIO_WMA_TYPE1_PERIOD		8192
#define SOC_AUDIO_WMA_TYPE2_PERIOD		8192
#define SOC_AUDIO_WMA_TYPE3_PERIOD		4096
#define SOC_AUDIO_WMA_TYPE4_PERIOD		2048
#define SOC_AUDIO_MPEG2_AAC_LSF_0_PERIOD	8192
#define SOC_AUDIO_MPEG2_AAC_LSF_1_PERIOD	16384
#define SOC_AUDIO_MPEG4_AAC_0_PERIOD		4096
#define SOC_AUDIO_MPEG4_AAC_1_PERIOD		8192
#define SOC_AUDIO_MPEG4_AAC_2_PERIOD		16384
#define SOC_AUDIO_MPEG4_AAC_3_PERIOD		2048
#define SOC_AUDIO_EAC3_PERIOD			24576
#define SOC_AUDIO_MPEG4_AAC_LC_0_PERIOD		4096
#define SOC_AUDIO_MPEG4_AAC_LC_1_PERIOD		3840
#define SOC_AUDIO_MPEG4_AAC_HE_0_PERIOD		8192
#define SOC_AUDIO_MPEG4_AAC_HE_1_PERIOD		7680

/**
User gain value that can be used to specify gain in dB with a resolution of
0.1dB. Valid values are  +180 to -1450, representing gains of +18.0dB to
-145.0dB.
*/
/** Maximum gain value of 18.0dB */
#define SOC_AUDIO_GAIN_MAX   180
/** No gain 0.0dB */
#define SOC_AUDIO_GAIN_0_DB  0
/** Minimum gain of -145.0dB */
#define SOC_AUDIO_GAIN_MUTE  (-1450)

/** Min ramp value in ms used when ramping up/down from a certain level.*/
#define SOC_AUDIO_RAMP_MS_MIN 0
/** MAX ramp value in ms used when ramping up/down from a certain level.*/
#define SOC_AUDIO_RAMP_MS_MAX 20000

#define SOC_AUDIO_ENSURE_VALID_RAMP(ramp_ms) \
			(((ramp_ms) < SOC_AUDIO_RAMP_MS_MIN) \
			? SOC_AUDIO_RAMP_MS_MIN : \
			(((ramp_ms) > SOC_AUDIO_RAMP_MS_MAX) \
			? SOC_AUDIO_RAMP_MS_MAX : (ramp_ms)))

#define SOC_AUDIO_ENSURE_VALID_GAIN(gain) ((gain) > SOC_AUDIO_GAIN_MAX ? \
			SOC_AUDIO_GAIN_MAX : \
			((gain) < SOC_AUDIO_GAIN_MUTE ? \
			SOC_AUDIO_GAIN_MUTE : (gain)))

#define SOC_AUDIO_FLOOR_CEIL(value, min, max) ((value) > max ? \
					      max : ((value) < min ? \
					      min : (value)))

/** The maximum per channel delay allowed in units of 0.1ms.
 * See \ref soc_audio_set_per_channel_delay. */
#define SOC_AUDIO_PER_CHANNEL_MAX_DELAY_MS 830
/** The minimum per channel delay allowed in units of 0.1ms.
 * See \ref soc_audio_set_per_channel_delay. */
#define SOC_AUDIO_PER_CHANNEL_MIN_DELAY_MS 0

#endif

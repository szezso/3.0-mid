#ifndef __INTEL_SST_IOCTL_H__
#define __INTEL_SST_IOCTL_H__
/*
 *  intel_sst_ioctl.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file defines all sst ioctls
 */

/* codec and post/pre processing related info */

#include <linux/types.h>

enum sst_codec_types {
/*  AUDIO/MUSIC	CODEC Type Definitions */
	SST_CODEC_TYPE_UNKNOWN = 0,
	SST_CODEC_TYPE_PCM,	/* Pass through Audio codec */
	SST_CODEC_TYPE_MP3,
	SST_CODEC_TYPE_MP24,
	SST_CODEC_TYPE_AAC,
	SST_CODEC_TYPE_AACP,
	SST_CODEC_TYPE_eAACP,
	SST_CODEC_TYPE_WMA9,
	SST_CODEC_TYPE_WMA10,
	SST_CODEC_TYPE_WMA10P,
	SST_CODEC_TYPE_RA,
	SST_CODEC_TYPE_DDAC3,
	SST_CODEC_TYPE_STEREO_TRUE_HD,
	SST_CODEC_TYPE_STEREO_HD_PLUS,

	/*  VOICE CODEC Type Definitions */
	SST_CODEC_TYPE_VOICE_PCM = 0x21, /* Pass through voice codec */
};

enum snd_sst_stream_ops {
	STREAM_OPS_PLAYBACK = 0,	/* Decode */
	STREAM_OPS_CAPTURE,		/* Encode */
	STREAM_OPS_PLAYBACK_DRM,	/* Play Audio/Voice */
	STREAM_OPS_PLAYBACK_ALERT,	/* Play Audio/Voice */
	STREAM_OPS_CAPTURE_VOICE_CALL,	/* CSV Voice recording */
	STREAM_OPS_COMPRESSED_PATH,	/* CSV Voice recording */
};

enum stream_mode {
	SST_STREAM_MODE_NONE = 0,
	SST_STREAM_MODE_DNR = 1,
	SST_STREAM_MODE_FNF = 2,
	SST_STREAM_MODE_CAPTURE = 3
};

enum stream_type {
	SST_STREAM_TYPE_NONE = 0,
	SST_STREAM_TYPE_MUSIC = 1,
	SST_STREAM_TYPE_NORMAL = 2,
	SST_STREAM_TYPE_LONG_PB = 3,
	SST_STREAM_TYPE_LOW_LATENCY = 4,
};

enum snd_sst_audio_device_type {
	SND_SST_DEVICE_NONE = 0,
	SND_SST_DEVICE_HEADSET,
	SND_SST_DEVICE_IHF,
	SND_SST_DEVICE_VIBRA,
	SND_SST_DEVICE_HAPTIC,
	SND_SST_DEVICE_CAPTURE,
	SND_SST_DEVICE_VIRTUAL_HEADSET,
	SND_SST_DEVICE_VIRTUAL_IHF,
	SND_SST_DEVICE_COMPRESSED_PLAY,
	SND_SST_DEVICE_COMPRESSED_CAPTURE,
	SND_SST_MAX_AUDIO_DEVICES,
};

/* Port info structure */
struct snd_sst_port_info {
	__u16 port_type;
	__u16 reserved;
};

/* PCM Parameters */
struct snd_pcm_params {
	__u16 codec;	/* codec type */
	__u8 num_chan;	/* 1=Mono, 2=Stereo */
	__u8 pcm_wd_sz;	/* 16/24 - bit*/
	__u32 reserved;	/* Bitrate in bits per second */
	__u32 sfreq;	/* Sampling rate in Hz */
	__u32 ring_buffer_size;
	__u32 period_count;	/* period elapsed in samples*/
	__u32 ring_buffer_addr;
};

/* MP3 Music Parameters Message */
struct snd_mp3_params {
	__u16 codec;
	__u8  num_chan;	/* 1=Mono, 2=Stereo	*/
	__u8  pcm_wd_sz; /* 16/24 - bit*/
	__u32 brate; /* Use the hard coded value. */
	__u32 sfreq; /* Sampling freq eg. 8000, 441000, 48000 */
	__u8  crc_check; /* crc_check - disable (0) or enable (1) */
	__u8  op_align; /* op align 0- 16 bit, 1- MSB, 2 LSB*/
	__u16 reserved;	/* Unused */
};

#define AAC_BIT_STREAM_ADTS		0
#define AAC_BIT_STREAM_ADIF		1
#define AAC_BIT_STREAM_RAW		2

/* AAC Music Parameters Message */
struct snd_aac_params {
	__u16 codec;
	__u8 num_chan; /* 1=Mono, 2=Stereo*/
	__u8 pcm_wd_sz; /* 16/24 - bit*/
	__u32 brate;
	__u32 sfreq; /* Sampling freq eg. 8000, 441000, 48000 */
	__u32 aac_srate;	/* Plain AAC decoder operating sample rate */
	__u8 mpg_id; /* 0=MPEG-2, 1=MPEG-4 */
	__u8 bs_format; /* input bit stream format adts=0, adif=1, raw=2 */
	__u8 aac_profile; /* 0=Main Profile, 1=LC profile, 3=SSR profile */
	__u8 ext_chl; /* No.of external channels */
	__u8 aot; /* Audio object type. 1=Main , 2=LC , 3=SSR, 4=SBR*/
	__u8 op_align; /* output alignment 0=16 bit , 1=MSB, 2= LSB align */
	__u8 brate_type; /* 0=CBR, 1=VBR */
	__u8 crc_check; /* crc check 0= disable, 1=enable */
	__s8 bit_stream_format[8]; /* input bit stream format adts/adif/raw */
	__u8 jstereo; /* Joint stereo Flag */
	__u8 sbr_present; /* 1 = SBR Present, 0 = SBR absent, for RAW */
	__u8 downsample;       /* 1 = Downsampling ON, 0 = Downsampling OFF */
	__u8 num_syntc_elems; /* 1- Mono/stereo, 0 - Dual Mono, 0 - for raw */
	__s8 syntc_id[2]; /* 0 for ID_SCE(Dula Mono), -1 for raw */
	__s8 syntc_tag[2]; /* raw - -1 and 0 -16 for rest of the streams */
	__u8 pce_present; /* Flag. 1- present 0 - not present, for RAW */
	__u8 sbr_type;		/* sbr_type: 0-plain aac, 1-aac-v1, 2-aac-v2 */
	__u8 outchmode;  /*0- mono, 1-stereo, 2-dual mono 3-Parametric stereo */
	__u8 ps_present;
};

/* WMA Music Parameters Message */
struct snd_wma_params {
	__u16 codec;
	__u8  num_chan;	/* 1=Mono, 2=Stereo */
	__u8  pcm_wd_sz;	/* 16/24 - bit*/
	__u32 brate;	/* Use the hard coded value. */
	__u32 sfreq;	/* Sampling freq eg. 8000, 441000, 48000 */
	__u32 channel_mask;  /* Channel Mask */
	__u16 format_tag;	/* Format Tag */
	__u16 block_align;	/* packet size */
	__u16 wma_encode_opt;/* Encoder option */
	__u8 op_align;	/* op align 0- 16 bit, 1- MSB, 2 LSB */
	__u8 pcm_src;	/* input pcm bit width */
};

/* Codec params struture */
union  snd_sst_codec_params {
	struct snd_pcm_params pcm_params;
	struct snd_mp3_params mp3_params;
	struct snd_aac_params aac_params;
	struct snd_wma_params wma_params;
};


struct snd_sst_stream_params {
	union snd_sst_codec_params uc;
};

struct snd_sst_params {
	__u32 result;
	__u32 stream_id;
	__u8 codec;
	__u8 ops;
	__u8 stream_type;
	__u8 device_type;
	struct snd_sst_stream_params sparams;
};

struct snd_sst_vol {
	__u32	stream_id;
	__s32	volume;
	__u32	ramp_duration;
	__u32	ramp_type;		/* Ramp type, default=0 */
};

struct snd_sst_mute {
	__u32	stream_id;
	__u32	mute;
};

/* ioctl related stuff here */
struct snd_sst_pmic_config {
	__u32  sfreq;                /* Sampling rate in Hz */
	__u16  num_chan;             /* Mono =1 or Stereo =2 */
	__u16  pcm_wd_sz;            /* Number of bits per sample */
};

struct snd_sst_get_stream_params {
	struct snd_sst_params codec_params;
	struct snd_sst_pmic_config pcm_params;
};

enum snd_sst_target_type {
	SND_SST_TARGET_PMIC = 1,
	SND_SST_TARGET_LPE,
	SND_SST_TARGET_MODEM,
	SND_SST_TARGET_BT,
	SND_SST_TARGET_FM,
	SND_SST_TARGET_NONE,
};

enum snd_sst_device_type {
	SND_SST_DEVICE_SSP = 1,
	SND_SST_DEVICE_PCM,
	SND_SST_DEVICE_OTHER,
};

enum snd_sst_device_mode {

	SND_SST_DEV_MODE_PCM_MODE1 = 1, /*(16-bit word, bit-length frame sync)*/
	SND_SST_DEV_MODE_PCM_MODE2,
	SND_SST_DEV_MODE_PCM_MODE3,
	SND_SST_DEV_MODE_PCM_MODE4_RIGHT_JUSTIFIED,
	SND_SST_DEV_MODE_PCM_MODE4_LEFT_JUSTIFIED,
	SND_SST_DEV_MODE_PCM_MODE4_I2S, /*(I2S mode, 16-bit words)*/
	SND_SST_DEV_MODE_PCM_MODE5,
	SND_SST_DEV_MODE_PCM_MODE6,
};

enum snd_sst_port_action {
	SND_SST_PORT_PREPARE = 1,
	SND_SST_PORT_ACTIVATE,
};

enum stream_param_type {
	SET_TIME_SLOT = 0,
	SWITCH_DEVICE = 1,
};

/* Target selection per device structure */
struct snd_sst_slot_info {
	__u8 mix_enable;	/* Mixer enable or disable */
	__u8 device_type;
	__u8 device_instance;	/* 0, 1, 2 */
	__u8 target_device;
	__u16 target_sink;
	__u8 slot[2];
	__u8 master;
	__u8 action;
	__u8 device_mode;
	__u8 reserved;
	struct snd_sst_pmic_config pcm_params;
};

#define SST_MAX_TARGET_DEVICES 3
/* Target device list structure */
struct snd_sst_target_device  {
	__u32 device_route;
	struct snd_sst_slot_info devices[SST_MAX_TARGET_DEVICES];
};

struct snd_sst_buff_entry {
	void *buffer;
	unsigned int size;
};

struct snd_sst_buffs {
	unsigned int entries;
	__u8 type;
	struct snd_sst_buff_entry *buff_entry;
};

struct snd_sst_dbufs  {
	unsigned long long input_bytes_consumed;
	unsigned long long output_bytes_produced;
	struct snd_sst_buffs *ibufs;
	struct snd_sst_buffs *obufs;
};

/*IOCTL defined here */
/*SST MMF IOCTLS only */
#define SNDRV_SST_STREAM_SET_PARAMS _IOR('L', 0x00, \
					struct snd_sst_stream_params *)
#define SNDRV_SST_STREAM_GET_PARAMS _IOWR('L', 0x01, \
					struct snd_sst_get_stream_params *)
#define SNDRV_SST_STREAM_GET_TSTAMP _IOWR('L', 0x02, __u64 *)
#define SNDRV_SST_STREAM_START	_IO('A', 0x42)
#define SNDRV_SST_STREAM_DROP	_IO('A', 0x43)
#define SNDRV_SST_STREAM_DRAIN	_IO('A', 0x44)
#define SNDRV_SST_STREAM_PAUSE	_IOW('A', 0x45, int)
#define SNDRV_SST_STREAM_RESUME _IO('A', 0x47)
#define SNDRV_SST_PROCESS_PID _IOW('L', 0x07, unsigned long *)
/*SST common ioctls */
#define SNDRV_SST_SET_VOL	_IOW('L', 0x11, struct snd_sst_vol *)
#define SNDRV_SST_GET_VOL	_IOW('L', 0x12, struct snd_sst_vol *)
#define SNDRV_SST_MUTE		_IOW('L', 0x13, struct snd_sst_mute *)
/*AM Ioctly only */
#define SNDRV_SST_SET_TARGET_DEVICE _IOW('L', 0x21, \
					struct snd_sst_target_device *)

#endif /* __INTEL_SST_IOCTL_H__ */

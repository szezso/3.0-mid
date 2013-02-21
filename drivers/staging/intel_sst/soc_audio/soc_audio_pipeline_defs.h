/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

  This program is free software;you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY;without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program;if not, write to the Free Software
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
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;LOSS OF USE,
  DATA, OR PROFITS;OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _SOC_AUDIO_PIPELINE_DEFS_H
#define _SOC_AUDIO_PIPELINE_DEFS_H

#include "soc_audio_defs.h"
#include "soc_audio_bu_config.h"
#include "soc_audio_quality_defs.h"

#define PKT_OPT
/* Structures */

/* Use this structure to specify volume levels of each audio channel.
\struct  soc_audio_per_channel_volume*/
struct soc_audio_per_channel_volume {
	int32_t front_left;
	int32_t front_right;
	int32_t center;
	int32_t lfe;
	int32_t surround_left;
	int32_t surround_right;
	int32_t rear_surround_left;
	int32_t rear_surround_right;
};

/*
Defines the bass management setting applicable to each channel in a stream.
\enum soc_audio_speaker_setting
*/
enum soc_audio_speaker_setting {
	SOC_AUDIO_SPEAKER_INVALID, /*Invalid value*/
	SOC_AUDIO_SPEAKER_LARGE, /*Speaker is large (contains all frequencies)*/
	/*Speaker is small (contains no frequencies below the specified
	crossover freq.)*/
	SOC_AUDIO_SPEAKER_SMALL
};

/*
Use this stucture to define the speaker setting (Small or Large)
of each individual channel/speaker of the audio stream. LFE not included.
\struct soc_audio_per_speaker_setting
*/
struct soc_audio_per_speaker_setting {
	enum soc_audio_speaker_setting front_left;
	enum soc_audio_speaker_setting front_right;
	enum soc_audio_speaker_setting center;
	enum soc_audio_speaker_setting surround_left;
	enum soc_audio_speaker_setting surround_right;
	enum soc_audio_speaker_setting rear_surround_left;
	enum soc_audio_speaker_setting rear_surround_right;
};

/* Use this structure to specify a delay value for each audio channel.
\struct  soc_audio_per_channel_delay*/
struct soc_audio_per_channel_delay {
	int32_t front_left;
	int32_t front_right;
	int32_t center;
	int32_t lfe;
	int32_t surround_left;
	int32_t surround_right;
	int32_t rear_surround_left;
	int32_t rear_surround_right;
};

/* Decoder Config Parameters */
struct soc_audio_pipeline_decode_aac_config_params {
	int32_t pcm_wdsz;
	int32_t bdownsample;
	int32_t downmix;
	int32_t externalsr;
	int32_t bsformat;
	int32_t outnchans;
	int32_t sbr_signaling;
	int32_t chanrouting[SOC_AUDIO_MAX_CHANNELS];
	int32_t to_stereo;
	int32_t zero_unused_chans;
	int32_t aac_format;
	int32_t num_channels;
	int32_t samp_freq;
	int32_t aac_samplerate;
	int32_t data_rate;
	int32_t acmod;
	int32_t sbr_type;
	int32_t chanmap;
	bool b_aac_surround_mono_to_stereo;
	bool b_aac_downmixer;
	int32_t enable_drc;
	int32_t enable_prl;
	int32_t drc_cut_fac;
	int32_t drc_boost_fac;
	int32_t target_level;
};

struct soc_audio_pipeline_decode_wm9_config_params {
	int32_t samp_freq;
	int32_t num_channels;
	int32_t w_format_tag;
	int32_t pcm_wdsz;
	int32_t channel_mask;	/*currently not used.*/
	int32_t block_align;
	int32_t wma_encode_opt;
	int32_t avg_bytes_per_sec;
};

struct soc_audio_pipeline_decode_mpeg_config_params {
	int32_t pcm_wd_sz;
	int32_t crc_check;
	int32_t mch_enable;
};

struct soc_audio_pipeline_decode_ac3_config_params {
	int32_t karac;
	int32_t dynrng_mode;
	int32_t lfe_out;
	int32_t ocfg;
	int32_t num_och;
	int32_t pcm_scale;
	int32_t stereo_mode;
	int32_t mono_rep;
	int32_t dyn_cut_hi;
	int32_t dyn_boost_lo;
	int32_t chan_map[6];
	int32_t ext_dnmixtab[36];
	int32_t block_repeat;
};

struct soc_audio_pipeline_decode_ddplus_config_params {
	int32_t chanrouting[8];
	int32_t num_och;
	int32_t lfe_out;
	int32_t ocfg;
	int32_t stereo_mode;
	int32_t mono_rep;
	int32_t karac;
	int32_t pcm_scale;
	int32_t dynrng_mode;
	int32_t dyn_cut_hi;
	int32_t dyn_boost_lo;
	int32_t quitonerr;
	int32_t framestart;
	int32_t frameend;
};

struct soc_audio_pipeline_decode_truehd_config_params {
	int32_t decmode;
	int32_t zero_unused_chans;
	int32_t blockcrc;
	int32_t lossless;
	int32_t drc_enable;
	int32_t drc_cut;
	int32_t drc_boost;
	int32_t dialref;
	int32_t fbachannelorder;
	bool dialrefsupplied;
};

struct soc_audio_pipeline_decode_dts_config_params {
	int32_t spkrout;
	int32_t drcpercent;
	int32_t num_och;
	int32_t lfeindex;
	bool secondary_audio_present;
	int32_t bitspersample;
	int32_t downmixmode;
};

/* Configuration parameters for the DTS Broadcast decoder. */
struct soc_audio_pipeline_decode_dts_bc_config_params {
	uint32_t pcmsize; /*Bit depth of input bitstream.*/
	int32_t spkrout; /*Channel config of generated output.*/
	bool lfednmix; /*Flag to enable Lfe downmix on two front channels.*/
	bool dwnmixmode; /*Flag to specify internal or external downmix mode.*/
	/*Flag to specify internal or external speaker remapper mode.*/
	bool spkrremapmode;
	int32_t playermixoutmode; /*Type of secondary audio mixing enabled.*/
	int32_t drcpercent; /*Dynamic range control setting.*/
	/*Flag to signal to the decoder if disable dialknorm is enabled.*/
	bool disdialnorm_flag;
	bool disable_dialnorm;/*Flag to disable dialnorm enabled in bitstream.*/
	/*Flag to explicitly enable user defined DRC percent rather than the
	one embedded in the bitstream.*/
	bool drc_prsent;
};

struct soc_audio_pipeline_codec_ac3_enc_config_params {
	int32_t acmod;
	int32_t lofreqeffon;
	int32_t lfefiltinuse;
	int32_t num_ich;
	int32_t data_rate_code;
	int32_t pcm_wdsz;
	int32_t compchar;
	int32_t compchar2;
	int32_t testmodeon;
	int32_t samp_freq;
	int32_t chanrouting[6];
	bool packetize;
	/* Flag to tell encoder to packetize output or not */
};

struct soc_audio_pipeline_codec_dts_enc_config_params {
	int32_t num_input_ch;	/*Max of 5*/
	int32_t lfe_present;	/*0 present, 1 not present*/
	int32_t total_num_input_ch;
	int32_t sample_size;	/*16 or 24*/
	int32_t sample_rate;	/*Only supports 48kHz*/
	int32_t encode_format;	/* 1, 2, 3*/
	bool packetize;	/* Flag to tell encoder to packetize output or not */
};

struct soc_audio_pipeline_decode_dts_lbr_config_params {
	uint32_t ui16BitDepth;
	uint32_t ui16StereoDwnMix;
	uint32_t ui32NewSampleRate;
};

struct soc_audio_pipeline_codec_dts_hd_config_params {
	int32_t sample_width;
	int32_t spkrout;
	int32_t drc_percent;
	int32_t lfe_index;
	int32_t decode_type;
	int32_t downmix_mode;
	int32_t spkr_remap_mode;
	bool multi_asset_decoding;
	bool enable_dts_es_matrix_sound_track;
	bool disable_dialogue_normalization;
	bool no_default_speaker_remapping;
	uint32_t audiopresentindex;
	uint32_t replacementsetconfig[3];
	int32_t number_repchsets;
	bool secondary_audio_present;
};

struct soc_audio_neo6_management_params {
	uint32_t samplebitdepth;
	uint32_t samplerate;
	uint32_t samplesperframe;
	bool input_over;
	int32_t cgain;
	int32_t mode;
	int32_t channels;
};

/* MS10 DDC decode configuration parameters.*/
struct soc_audio_pipeline_decode_ddc_config_params {
	bool assoc_audio_enable;
	/* whether associated audio was enabled or not. To be deprecated */
	bool main_audio_dd_enabled;/* DD output was enabled or not */
	bool pcm_enable;	    /* PCM output was enabled or not */
	bool packetize;		    /* Enable packetizer */
	int32_t chanrouting[2];		/* DDC: force to [0:L, 1:R] */
	int32_t num_och;		/* DDC: force to 2 */
	int32_t ocfg;			/* DDC: force to 2 */
	int32_t lfe_out;		/* DDC: force to 0 */
	int32_t stereo_mode;
	int32_t mono_rep;
	int32_t karac;
	int32_t pcm_scale;
	int32_t dynrng_mode;
	int32_t dyn_cut_hi;
	int32_t dyn_boost_lo;
	int32_t quitonerr;
	int32_t framestart;
	int32_t frameend;
	int32_t acmod;
	int32_t substream_id;
	int32_t input_over;
};

/* MS10 DDT configuration parameters.*/
struct soc_audio_pipeline_decode_ddt_pp_config_params {
	int32_t RfMode;
	int32_t LoRoDownmix;
	int32_t drcCutFac;
	int32_t drcBoostFac;
	int32_t dualMode;
};

/* MS10 DSP pipeline configuration parameters.*/
struct soc_audio_pipeline_decode_ddt_config_params {
	bool assoc_audio_ddt_enabled;
	bool main_audio_ddt_enabled;
	struct soc_audio_pipeline_decode_aac_config_params ddt_aac;
	struct soc_audio_pipeline_decode_ddt_pp_config_params ddt_pp;
	struct soc_audio_pipeline_codec_ac3_enc_config_params ddt_ac3_enc;
};

union soc_audio_pipeline_decode_config_params {
#ifdef INCLUDE_AC3_DECODE
	struct soc_audio_pipeline_decode_ac3_config_params ac3_params;
#endif
#ifdef INCLUDE_DDPLUS_DECODE
	struct soc_audio_pipeline_decode_ddplus_config_params ddplus_params;
#endif
#ifdef INCLUDE_TRUEHD_DECODE
	struct soc_audio_pipeline_decode_truehd_config_params truehd_params;
#endif
#ifdef INCLUDE_MPEG_DECODE
	struct soc_audio_pipeline_decode_mpeg_config_params mpeg_params;
#endif
#ifdef INCLUDE_AAC_DECODE
	struct soc_audio_pipeline_decode_aac_config_params aac_params;
#endif
#ifdef INCLUDE_WMA_DECODE
	struct soc_audio_pipeline_decode_wm9_config_params wma_params;
#endif
#ifdef INCLUDE_DTS_DECODE
	struct soc_audio_pipeline_decode_dts_config_params dts_params;
#endif
#ifdef INCLUDE_DTS_LBR_DECODE
	struct soc_audio_pipeline_decode_dts_lbr_config_params dts_lbr_params;
#endif
#ifdef INCLUDE_AC3_ENCODE
	struct soc_audio_pipeline_codec_ac3_enc_config_params ac3_enc_params;
#endif
#ifdef INCLUDE_DTS_ENCODE
	struct soc_audio_pipeline_codec_dts_enc_config_params dts_enc_params;
#endif
#ifdef INCLUDE_NEO6_DECODE
	struct soc_audio_neo6_management_params neo6_params;
#endif
#ifdef INCLUDE_DTS_HD_DECODE
	struct soc_audio_pipeline_codec_dts_hd_config_params dts_hd_params;
#endif
#ifdef INCLUDE_DTS_BC_DECODE
	struct soc_audio_pipeline_decode_dts_bc_config_params dts_bc_params;
#endif
#ifdef INCLUDE_DDC_DECODE
	struct soc_audio_pipeline_decode_ddc_config_params ddc_params;
#endif
#ifdef INCLUDE_DDT_DECODE
	struct soc_audio_pipeline_decode_ddt_config_params ddt_params;
#endif
};

struct soc_audio_codec_params {
	union soc_audio_pipeline_decode_config_params config;
	struct soc_audio_bu_specific_decode_config_params bu_specific;
#if SOC_CACHELINE_PADDING
	uint8_t cacheline_pad[44];
#endif
};

/*
This struct defines the stream information
/struct soc_audio_stream_info
*/
struct soc_audio_stream_info {
	uint32_t bitrate;
	uint32_t sample_rate;
	uint32_t sample_size;
	int32_t channel_config;
	int32_t channel_count;
	enum soc_audio_format algo;
};

/* Dolby specific downmix structure */
struct soc_audio_dolby_dmixlevs {
	int16_t cmixlev;
	int16_t surmixlev;
};

struct soc_audio_format_info_ac3 {
	/* This is the channel configuration (acmod) of the input bit stream.
	* Valid values are 0 through 7.
	* 0 indicates 2 full bandwidth channels (1/0 + 1/0),
	* 1 <96> 1/0 (C)
	* 2 <96> 2/0 (L, R)
	* 3 <96> 3/0 (L, C, R)
	* 4 <96> 2/1 (L, R, l)
	* 5 <96> 3/1 (L, C, R, l)
	* 6 <96> 2/2 (L, R, l, r)
	* 7 <96> 3/2 (L, C, R, l, r) (default) */
	uint32_t acmod;

	/* 0 = not present 1= present*/
	int32_t lfe_present;

	/* This the byte-order of the input bit stream. Any non zero value
	means * the application must byte swap the AC3 stream before feeding
	it to the driver.*/
	int32_t input_byte_swap;

	/* extended bitstream information 1 */
	int16_t bsid;		   /* Bitstream identification */
	int16_t bsmod;		   /* Bitstream mode */
	int16_t lfeon;		   /* Low freq effects chan flag */
	int16_t xbsi1e;		   /* Extra BSI1 info exists */

	/* downmix coefficients */
	struct soc_audio_dolby_dmixlevs legacy_dmixlevs;
	/* Legacy downmix levels */
	struct soc_audio_dolby_dmixlevs ltrt_dmixlevs;
	/* Lt/Rt downmix levels  */
	struct soc_audio_dolby_dmixlevs loro_dmixlevs;
	/* Lo/Ro downmix levels */
};

struct soc_audio_format_info_aac {
	/* The transport format for the encoded input bit stream:
		0 -Unknown, 1-adif, 2-adts, 6-raw */
	int aac_format;

	/* Information about the audio coding mode of the input bit stream
	describing the encoded channel configuration.
	* 0 - Undefined,
	* 1 - mono (1/0),
	* 2 -parametric stereo,
	* 3 - dual mono,
	* 4 -stereo(2/0),
	* 5 -L,C,R(3/0)
	* 6 <96> L, R, l (2/1),
	* 7 <96> L, R, l, r (2/2),
	* 8 <96> L, C, R, Cs (3/0/1)
	* 9 <96> L, C, R, l, r (3/2)
	* 10 <96> L, C, R, l, r, Cs (3/2/1)
	* 11 <96> L, C, R, l, r, Sbl, Sbr (3/2/2)
	* 12 <96> L, R, LFE (2/0.1)
	* 13 <96> L, C, R, LFE (3/0.1)
	* 14 <96> L, R, Cs, LFE (2/0/1.1)
	* 15 <96> L, R, Ls, Rs, LFE (2/2.1)
	* 16 <96> L, C, R, Cs, LFE (3/0/1.1)
	* 17 <96> L, C, R, l, r, LFE (5.1 mode)
	* 18 <96> L, C, R, l, r, Cs, LFE (3/2/1.1)
	* 19 <96> L, C, R, l, r, Sbl, Sbr, LFE (7.1 mode)*/
	int acmod;

	/* The decoded Audio Object Type.
	* 0 - plain AAC-LC (low complexity) object type
	* 1 - aacPlus object type containing SBR element.
	* 2 - aacPlusv2 object type containing PS object type*/
	uint8_t sbr_type;
};

struct soc_audio_format_info_mpeg {
	/* This indicates if an extension substream is present in the stream.
		0 = not present 1= present*/
	int extn_substream_present;

	/* 0 = not present 1= present*/
	int lfe_present;

	/* This is the number of extra full-bandwidth channels in addition
	to the main audio channels.
	* This number does not include the LFE channel.
	* Valid values: 0 through 3*/
	int num_xchan;

	/* This is the channel mode information
	* bits 1:0 <96> 0 (stereo),
	* 1 (joint32_t stereo),
	* 2 (dual mono),
	* 3 (mono),
	* bit 4: intensity stereo, if set to 1
	* bit 5: mid-side stereo, if set to 1*/
	int32_t ch_mode_info;
};

struct soc_audio_format_info_ddplus {
	int16_t strmtyp; /* Stream type */
	int16_t substreamid; /* Sub-stream identification */
	int16_t frmsiz;	/* Frame size (in 16-bit words) */
	int16_t fscod2;	/* Sample rate code 2 (halfrate) */
	int16_t chanmape; /* Channel map exists flag */
	int16_t chanmap; /* Channel map data */
	int16_t mixmdate; /* Mixing metadata exists flag */
	int16_t lfemixlevcode; /* LFE Mix Level Code exists flag */
	int16_t lfemixlevcod; /* LFE Mix Level Code */
	int16_t pgmscle[2]; /* Program scale factor exists flags */
	int16_t pgmscl[2]; /* Program scale factor */
	int16_t extpgmscle; /* External program scale factor exists flags */
	int16_t extpgmscl; /* External program scale factor exists */
	int16_t mixdef;	/* Mix control type */
	int16_t mixdeflen; /* Length of mixing parameter data field */
	int16_t mixdata2e; /* Mixing data 2 exists */
	int16_t premixcmpsel; /* Premix compression word select */
	/* Dynamic range control word source (external or current) */
	int16_t drcsrc;
	int16_t premixcmpscl; /* Premix compression word scale factor */
	int16_t extpgmlscle; /* External program left scale factor exists */
	int16_t extpgmcscle; /* External program center scale factor exists */
	int16_t extpgmrscle; /* External program right scale factor exists */
	/* External program left surround scale factor exists */
	int16_t extpgmlsscle;
	/* External program right surround scale factor exists */
	int16_t extpgmrsscle;
	int16_t extpgmlfescle; /* External program LFE scale factor exists */
	int16_t extpgmlscl; /* External program left scale factor */
	int16_t extpgmcscl; /* External program center scale factor */
	int16_t extpgmrscl; /* External program right scale factor */
	int16_t extpgmlsscl; /* External program left surround scale factor */
	int16_t extpgmrsscl; /* External program right surround scale factor */
	int16_t extpgmlfescl; /* External program LFE scale factor */
	int16_t dmixscle; /* Downmix scale factor exists */
	int16_t dmixscl; /* Downmix scale factor */
	int16_t addche;	/* Additional scale factors exist */
	/* External program 1st auxiliary channel scale factor exists */
	int16_t extpgmaux1scle;
	/* External program 1st auxiliary channel scale factor */
	int16_t extpgmaux1scl;
	/* External program 2nd auxiliary channel scale factor exists */
	int16_t extpgmaux2scle;
	/* External program 2nd auxiliary channel scale factor */
	int16_t extpgmaux2scl;
	/* Frame mixing configuration information exists flag */
	int16_t frmmixcfginfoe;
	/* Block mixing configuration information exists flag */
	int16_t blkmixcfginfoe;
	int16_t blkmixcfginfo[6]; /* Block mixing configuration information */
	int16_t paninfoe[2]; /* Pan information exists flag */
	int16_t panmean[2]; /* Pan mean angle data */
	int16_t paninfo[2]; /* Pan information */
	int16_t infomdate; /* Information Meta-Data exists flag */
	int16_t sourcefscod; /* Source sample rate code */
	int16_t convsync; /* Converter synchronization flag */
	int16_t blkid; /* Block identification */

	/* extended bitstream information 1 */
	int16_t bsid; /* Bitstream identification */
	int16_t bsmod; /* Bitstream mode */
	int16_t acmod; /* Audio coding mode */
	int16_t lfeon; /* Low freq effects chan flag */
	int16_t nfchans; /* Derived # of full bw chans */
	int16_t nchans;	/* Derived # of channels */
	int16_t niprgms; /* Derived # of independent programs */
	int16_t xbsi1e;	/* Extra BSI1 info exists */

	/* downmix coefficients */
	/* Legacy downmix levels */
	struct soc_audio_dolby_dmixlevs legacy_dmixlevs;
	/* Lt/Rt downmix levels  */
	struct soc_audio_dolby_dmixlevs ltrt_dmixlevs;
	/* Lo/Ro downmix levels */
	struct soc_audio_dolby_dmixlevs loro_dmixlevs;
};

struct soc_audio_format_info_dts_lbr {
	int32_t channel_mask;
	bool mix_metadata_enabled;
	int32_t mix_metadata_adj_level;
	int32_t bits_for_mixout_mask;
	int32_t num_mixout_configs;
	int32_t mixout_chmask[4];
	int32_t mix_metadata_present;
	int32_t external_mixflag;
	int32_t postmix_gainadj_code;
	int32_t control_mixer_drc;
	int32_t limit_embeded_drc;
	int32_t custom_drc_code;
	int32_t enbl_perch_mainaudio_scale;
	int32_t onetoone_mixflag;
	int32_t mix_out_config;
	/*unsigned char ui8MainAudioScaleCode[4][16];*/
	uint8_t ui8MainAudioScaleCode[16];
	/*unsigned short ui16MixMapMask[4][3][6];*/
	uint32_t ui16MixMapMask[6];
	/*unsigned char ui8MixCoeffs[4][3][6][16];*/
	uint8_t ui8MixCoeffs[6][16];
};

struct soc_audio_format_info_dvd_pcm {
	bool audio_emphasis_flag;
	bool audio_mute_flag;
	int32_t dynamic_range_control;
};

struct soc_audio_format_info_truhd {
	uint8_t active_channel_count;
};

/* Codec specific stream information. Dolby Digital Plus mixing data fields */
struct soc_audio_format_info_ddc {
	int16_t dmixmod; /* Preferred downmix mode */
	int16_t cmixlev; /* Center downmix level */
	int16_t surmixlev; /* Surround downmix level */
	int16_t ltrtcmixlev; /* Lt/Rt center downmix level */
	int16_t ltrtsurmixlev; /* Lt/Rt surround downmix level */
	int16_t lorocmixlev; /* Lo/Ro center downmix level */
	int16_t lorosurmixlev; /* Lo/Ro surround downmix level */
	int16_t pgmscl[2]; /* Program scale factor */
	int16_t extpgmscl; /* External program scale factor  */
	int16_t paninfo[2]; /* Panning information */
	int16_t panmean[2]; /* Pan mean angle data */
	int16_t lfemixlevcod; /* LFE mix level code */
	int16_t premixcmpsel; /* Premix compression word select */
	/* Dynamic range control word source (external or current) */
	int16_t drcsrc;
	int16_t premixcmpscl; /* Premix compression word scale factor */
	int16_t extpgmlscl; /* External program left scale factor */
	int16_t extpgmcscl; /* External program center scale factor */
	int16_t extpgmrscl; /* External program right scale factor */
	int16_t extpgmlsscl; /* External program left surround scale factor */
	int16_t extpgmrsscl; /* External program right surround scale factor */
	int16_t extpgmlfescl; /* External program LFE scale factor */
	int16_t dmixscl; /* Downmix scale factor */
	/* External program 1st auxiliary channel scale factor */
	int16_t extpgmaux1scl;
	/* External program 2nd auxiliary channel scale factor */
	int16_t extpgmaux2scl;
	int16_t mixdatavalid; /* introduced for valid mixdata */
};

union soc_audio_format_info {
#ifdef INCLUDE_DDPLUS_DECODE
	struct soc_audio_format_info_ddplus ddplus;
#endif
#ifdef INCLUDE_AC3_DECODE
	struct soc_audio_format_info_ac3 ac3;
#endif
#ifdef INCLUDE_MPEG_DECODE
	struct soc_audio_format_info_mpeg mpeg;
#endif
#ifdef INCLUDE_AAC_DECODE
	struct soc_audio_format_info_aac aac;
#endif
#ifdef INCLUDE_DVD_PCM_DECODE
	struct soc_audio_format_info_dvd_pcm dvd_pcm;
#endif
#ifdef INCLUDE_TRUEHD_DECODE
	struct soc_audio_format_info_truhd truehd;
#endif
#ifdef INCLUDE_DTS_LBR_DECODE
	struct soc_audio_format_info_dts_lbr dts_lbr;
#endif
#ifdef INCLUDE_DDC_DECODE
	struct soc_audio_format_info_ddc ddc;
#endif
};

struct soc_audio_format_specific_info {
	struct soc_audio_stream_info basic_stream_info;
	union soc_audio_format_info format;
};

/*
 Specifies how an input channel will be mixed into the various output channels.
*/
struct soc_audio_channel_mapping {
	int32_t input_channel_id; /*< ID of the input channel being mapped */
	/* Array indicating the relative gain to be included in each output
	channel */
	int32_t output_channels_gain[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
};

/* Specifies a complete mapping of input channels to output channels. */
struct soc_audio_channel_mix_config {
	/*< Each entry in the array controls the mixing of a
	single input channel. */
	struct soc_audio_channel_mapping
	input_channels_map[SOC_AUDIO_MAX_INPUT_CHANNELS];
};

/*
   Output configuration parameters.
   \struct soc_audio_output_config
*/
struct soc_audio_output_config {
	int32_t stream_delay;
	/*< Set the delay in miliseconds of output stream.
	Range 0-255 ms (1 ms step).*/
	int32_t sample_size;/*< Set the output audio sample size.*/
	enum soc_audio_channel_config ch_config;
	/*< Set the output channel configuration*/
	enum soc_audio_output_mode out_mode;
	/*< Set the output mode.
	(i.e. SOC_AUDIO_OUTPUT_PCM or SOC_AUDIO_OUTPUT_PASSTHROUGH) */
	int32_t sample_rate;/*< Set the output stream's sample rate.*/
	int32_t ch_map;
	/*<Set the output channel mapping (applicable in SW output only),
	0 = use default driver channel mapping (disabled)*/
};

/*Some mpeg clips have PTS every frame and give small buffers.*/
struct soc_audio_pipeline_decode_params {
	struct soc_audio_codec_params host;
#ifdef MAINTAIN_DSP_COPY
	struct soc_audio_codec_params dsp;
	/* DSP's copy of the host updatable parameters.*/
#endif
	/* Parameters supplied to configure the codec's knobs. */
	struct soc_audio_bu_specific_decode_params bu_specific;
	struct soc_audio_format_specific_info stream_info;
};

struct soc_audio_pipeline_src_params {
	int32_t primary_index;
	enum soc_audio_mix_sample_rate_mode mix_src_mode;
	uint32_t auto_output_sample_rate;
	int32_t vsrc_init_flag[SOC_AUDIO_MAX_INPUTS];
	int32_t vsrc_set_var_on[SOC_AUDIO_MAX_INPUTS];
	uint32_t output_sample_rate[SOC_AUDIO_MAX_INPUTS];
	struct soc_audio_pipeline_buf_desc
	 aux_buffer[SOC_AUDIO_MAX_INPUTS];
	/* Used to store SRC history. */
	enum soc_audio_status state[SOC_AUDIO_MAX_INPUTS];
};

/********** Start Mixer Parameters ****************/

enum soc_audio_scale_type {
	SOC_AUDIO_SCALE_TYPE_NONE = 0,
	/* Invalid or not present. */
	SOC_AUDIO_SCALE_TYPE_DEFAULT = 1,
	/* Gains for input scaled straight-through to output (1-to-1). */
	SOC_AUDIO_SCALE_TYPE_MONO = 2
	/* Gains for a mono independently scaled to each output channel
	(for panning, etc.). */
};

/*Han added for mixer coefficient smoothing using aux_buffer*/
struct soc_audio_mixer_internal_mem_layout {
	struct soc_audio_channel_mix_config stream_channel_mix_config_pri;
	/* Channel mix configuration for primary*/
	struct soc_audio_channel_mix_config stream_channel_mix_config_sec;
	/* Channel mix configuration for sec*/
	int32_t stream_output_ch_gains[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/*output channel gains to be applied for individual
	channel volume control*/
	int32_t delta[SOC_AUDIO_MAX_INPUTS]
	    [SOC_AUDIO_MAX_INPUT_CHANNELS][SOC_AUDIO_MAX_OUTPUT_CHANNELS];
		/* For coefficient smoothing*/
	int32_t master_delta[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/* For coefficient smoothing*/
	int32_t new_gain_index;
	int32_t coeff_changed;
	/* Counter to tell us whether there was change
	in the mixer coeffiecients.*/
	int16_t
	input_gain_index[SOC_AUDIO_MAX_GAIN_TABLE][SOC_AUDIO_MAX_INPUTS]
	    [SOC_AUDIO_MAX_INPUT_CHANNELS][SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	int16_t master_gain_index[SOC_AUDIO_MAX_GAIN_TABLE]
	    [SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	int16_t sub_flag[SOC_AUDIO_MAX_INPUTS]
	    [SOC_AUDIO_MAX_INPUT_CHANNELS][SOC_AUDIO_MAX_OUTPUT_CHANNELS];
		/* For coefficient smoothing*/
	int16_t number_of_input_channels[SOC_AUDIO_MAX_INPUTS];
	int16_t master_sub_flag[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/* For coefficient smoothing*/
	uint32_t stream_input_config_valid_pri;
	uint32_t stream_input_config_valid_sec;
	uint32_t stream_output_ch_gains_valid;
	enum soc_audio_scale_type type;
};

/* Mixer input configuration */
struct soc_audio_mixer_input {
	struct soc_audio_channel_mix_config channel_mix_config;
	/* Channel mix configuration of this input*/
	uint32_t valid;
	int32_t *buffer;	/* Pointer to input data*/
	uint32_t num_channels;	/* Number of channels in this input*/
	int32_t sample_size;
	/* Sample size of this input. This must be 32!*/
	int32_t input_ch_config;	/* Input channel ordering*/
	uint32_t copy_to_old_metadata_array;
	/* Flag to tell us to copy*/
};

/* Mixer stage parameters configurable from the HOST.*/
struct soc_audio_mixer_params_host {
	int32_t coeff_changed;
	/* Counter to tell us whether there was change in
	the mixer coeffiecients. Which causes the mixer to smoothly
	transition. */
	int32_t output_ch_gains_id;
	/* channel id for output channel gains
	to be applied for individual channel volume control */
	int32_t output_sample_size;
	/* Sample size of output data. */
	uint32_t operate_in_default_mode;
	/* Flag to tell us to operate in default mix mode (1:1)*/
	int32_t primary_index;
	/* index of the primary stream in the input_config array,
	 this one may have downmixing metadata*/
	int32_t secondary_index;
	/* index of the secondary stream in the input_config array,
	 this one may have mixing metadata*/
	uint32_t use_stream_metadata;
	/* Flag to tell us to use the mix metadata in the stream
	or the user-specified mix coefficients.*/
	int32_t oob_gains_set;
	/* Flag to tell us out of band, (user) mixer
	coefficients have been specified.*/
	int32_t output_ch_config;
	/* Output channel ordering*/
	int32_t ch_config_mode;
	/* User sets this to control the output channel
	configuration at the mixer. */
	int32_t ch_config_mode_fixed_ch_cfg;
	/* When ch_config_mode is set to FIXED need to
	use this variable as the output channel config for the mixer.*/
	int32_t output_ch_gains[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/* output channel gains to be applied for individual
	channel volume control*/
	/* Since we are only updating one input's coeff at
	a time no need to store the entire array. */
	struct soc_audio_channel_mix_config channel_mix_config;
	/* Channel mix configuration of the input being updated.*/
	uint32_t input_index;
	/* The index of the current input config being updated.*/
#if SOC_CACHELINE_PADDING
	uint8_t cacheline_pad[16];
#endif
};

/* Mixer stage parameters */
struct soc_audio_mixer_params {

	/* The parameters the host updates */
	struct soc_audio_mixer_params_host host;

	/* The parameters the DSP looks at only updated
	when host notifies of changes. */
#ifdef MAINTAIN_DSP_COPY
	struct soc_audio_mixer_params_host dsp;
#endif

	/* Used to store internal state of the mixer,
	allocated by host but then used by the DSP from then on. */
	struct soc_audio_pipeline_buf_desc aux_buffer;

	/* These parameters are used by the DSP ONLY.*/
	struct soc_audio_mixer_input input_config[SOC_AUDIO_MAX_INPUTS];
	/*Input configuration for each input.*/
	uint32_t input_buffer_count;	/* Number of inputs*/
	int32_t *output_buffer;
	/* Pointer to the output (mixed) buffer*/
	uint32_t num_input_samples;
	/* Number of samples in each input buffer*/
	uint32_t num_output_channels;
	/* Number of output channels.*/
};

/********** END Mixer Parameters ****************/

/* Used for the DTS-HD packetizer*/
enum soc_audio_sample_rate_scale {
	SOC_AUDIO_SAMPLE_RATE_NORMAL = 0,
	SOC_AUDIO_SAMPLE_RATE_SCALE_TWO = 2,
	SOC_AUDIO_SAMPLE_RATE_SCALE_FOUR = 4,
	SOC_AUDIO_SAMPLE_RATE_SCALE_EIGHT = 8,
	SOC_AUDIO_SAMPLE_RATE_SCALE_SIXTEEN = 16,
};

#define SOC_AUDIO_DMIX_COEFF_CNT_DTS 64

struct soc_audio_dolby_downmix_coeff {
	int32_t bsid;
	/* Bitstream identification: bsid = 6 (Annex D bit-stream id),
	 bsid = 16 (DDPlus bit-stream id) */
	int32_t acmod;		/* acmod */
	bool xbsi1e;
	/* Flag to indicate Extra BSI1 info exists. For AC3 packet,
	if xbsile is set to TRUE, ltrt_dmixlevs/loro_dmixlevs
	should be used */
	bool mixmdate;
	/* Mixing metadata exists flag for DDPlus packet */
	struct soc_audio_dolby_dmixlevs dd_dmixlevs;
	/* Legacy AC3 downmix levels */
	struct soc_audio_dolby_dmixlevs ltrt_dmixlevs;
	/* Lt/Rt downmix levels */
	struct soc_audio_dolby_dmixlevs loro_dmixlevs;
	/* Lo/Ro downmix levels */
	int32_t ocfg;
	/*for dolby codec, put the ocfg value here.
	downmixer stage need this parameter */
};

struct soc_audio_aac_downmix_metadata {
	int32_t acmod;		/* acmod */
	bool upmix_decoder_done;
	/* flag to indicate if upmix is done in decoder stage */
	bool dmix_decoder_done;
	/* flag to indicate if downmix is done in decoder stage */
	/*MPEG downmix metadata */
	int32_t aac_id;
	/* tell the AAC stream is MPEG2 or MPEG4 */
	int32_t matrix_mixdown_idx_present;
	/* if matrix_mixdown_idx is present in the stream */
	int32_t matrix_mixdown_idx;
	/* index to choose downmix coefficent */
	int32_t PCE_present;
	/* flag to indicate if PCE is present in the stream */
	bool pseudo_surround_enable;
	/* flag to indicate if pseudo_surround is enabled */
	/*DVB downmix metadata */
	int32_t center_mix_level;
	int32_t surround_mix_level;
};

struct soc_audio_output_mix_config {
	enum soc_audio_scale_type type;
	int16_t coeff_index[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/*Index to the gain coefficient in our custom mixer
	coefficient table. */
};

struct soc_audio_output_sec_mix_config {
	enum soc_audio_scale_type type;
	int32_t valid_mono_ch;
	int32_t secondary_audio_additional_scaling;	/*Rev2Aux support*/
	int16_t coeff_index[SOC_AUDIO_MAX_OUTPUT_CHANNELS];
	/*Index to the gain coefficient in our custom mixer
	coefficient table. */
};

struct soc_audio_downmix_metadata_context {
	uint32_t num_input_channels;/* Number of input channels */
	uint32_t num_output_channels;/* Number of output channels */
	uint32_t ch_map_after_dmix;
	/* The channel map of the output of the DTS downmixer */
	uint8_t scale_lfe;
	/*Flag to indicate if we should perform LFE scaling or not.*/
	uint8_t smooth_coeff;
	/* Flag to disable smoothing of downmix coefficients
	on first samples PCM (since old coefficients are 0) */
	union {
		int32_t downmixcoeff[SOC_AUDIO_DMIX_COEFF_CNT_DTS];
	/* Populated by DTS Library on
	XA_DTSHDDEC_CONFIG_PARAM_DOWNMIXCOEFF */
		struct soc_audio_dolby_downmix_coeff ddp_coeff;
		/* Populated by DD/DDPlus Library */
		struct soc_audio_aac_downmix_metadata aac_dmix;
	};
	int32_t output_dmix_mode;
	/* The downmix mode needed on the specific output. */
	enum soc_audio_format algo;/* Indicates the downmix codec type */
};

struct soc_audio_output_downmix_config {
	int32_t dec_dmix_mode;
	/* Indicates if the downmix is happening internal to the
	decoder or to use the external downmixer.*/
	struct soc_audio_downmix_metadata_context
			context[SOC_AUDIO_MAX_OUTPUTS];
	/* Downmix metadata information for each downmix context */
	int32_t downmixcoeff71to51[4][6];
	/* Populated by DTS Library on
	XA_DTSHDDEC_CONFIG_PARAM_DOWNMIXCOEFF_71_51 */
};

/* * Optional metadata in the buffer payload.  This can be found in the
buffer payload at base + opt_metadata_offset.  It is separate from the
standard metadata becuase it is not always used and it can be quite large.
 */
struct soc_audio_opt_metadata {
	struct soc_audio_output_mix_config pri_scale;
	/* Per-channel scaling for primary stream applied during mixing. */
	struct soc_audio_output_sec_mix_config sec_scale;
	/* Per-channel scaling for secondary stream applied during mixing. */
	struct soc_audio_output_mix_config master_scale;
	/* Per-channel scaling applied post-mix. */
	struct soc_audio_output_downmix_config dmix_config;
};

struct soc_audio_downmixer_input {
	uint32_t *pointer_to_inp_pcm_buffer;
	uint32_t *pointer_to_out_downmixed_pcm_buffer;
	int32_t i_num_inp_channels;
	int32_t i_num_out_channels;
	uint32_t *pointer_to_downmix_coefficients;
	uint32_t *pointer_to_downmix_coefficients_c2;
	/*2nd set of downmixer coeff for dolby stereo downmix*/
	uint32_t
	pointer_to_downmix_coefficients_old[SOC_AUDIO_DMIX_COEFF_CNT_DTS];
	/*Number of coefficients from decoder.*/

	int32_t i_inp_bitwidth;
	int32_t i_out_bitwidth;
	int32_t i_number_of_samples_in_single_channel;
	int32_t i_flag_noninterleaved;
	int32_t ch_map_config;
	int32_t acmod;
	int32_t ch_indexes[SOC_AUDIO_MAX_INPUT_CHANNELS];
	int32_t dolby_cert_mode;
	int32_t b_dtsado_scale_LFE;
	int32_t output_ch_config;
	int32_t dmix_mode;
	int32_t input_buffer_level;
	int32_t final_output_num_ch;
};

struct soc_audio_downmix_params {
	struct soc_audio_downmixer_input inputs[SOC_AUDIO_MAX_OUTPUTS];
	struct soc_audio_pipeline_buf_desc aux_buffer;
	/* Auxilliary buffer for storing downnmix
	coeffients while ramping up or down.*/
};

struct soc_audio_time_scale_params {
	struct soc_audio_pipeline_buf_desc aux_buffer;
};

enum soc_audio_dts_hd_substream_type {
	SOC_AUDIO_DTS_HD_CORE_SUBSTREAM,
	SOC_AUDIO_DTS_HD_EXTENSION_SUBSTREAM,
};

struct soc_audio_packetizer_ddp_params {
	bool first_syncframe;
	bool sub2_exist;
	int32_t substream1_num;
	int32_t substream2_num;
	int32_t substream1_cnt;
	int32_t substream2_cnt;
	bool sub1_complete;
	bool sub2_complete;
	bool found_sub1;
	bool found_sub2;
	int32_t strmtyp;
	int32_t substreamid;
	int32_t numblkscod;
	int32_t frame_payload;
	int32_t sub_frame_size;
	int32_t bsid;
	int32_t dd_exist;
};

enum soc_audio_dts_hd_frame_payload_structure {
	SOC_AUDIO_DTS_HD_CORE_SUBSTREAM_EXIST = 1,
	SOC_AUDIO_DTS_HD_EXTENSION_SUBSTREAM_ONLY = 2,
};

struct soc_audio_packetizer_dts_hd_params {
	bool found_core;
	bool found_extension;
	bool pack_complete;
	int32_t first_ext_index;
	int32_t core_syncword_byte_index;
	int32_t ext_syncword_byte_index;
	enum soc_audio_dts_hd_substream_type first_sub_type;
	int32_t current_ext_index;
	int32_t frame_payload;
	bool first_sub;
	int32_t frame_size;
	int32_t core_numblk;
	int32_t extension_frame_duration;
	int32_t core_sample_rate;
	int32_t extension_sample_rate;
	enum soc_audio_sample_rate_scale sample_rate_scale;
	enum soc_audio_format stream_type;
	enum soc_audio_dts_hd_frame_payload_structure payload_structure;
};

#define SOC_AUDIO_MAX_BYTES_PER_SAMPLE ((SOC_AUDIO_MAX_INPUT_CHANNELS * 32)/8)

struct soc_audio_pcm_decode_params {
	int32_t out_buffer_level;
	int32_t curr_bytes_till_pts;
	int32_t input_bytes_copied;
	int32_t sample_size;
	int32_t sample_rate;
	int32_t channel_count;
	int32_t channel_config;
	int32_t prev_channel_config;
	/*used to keep track of change for event firing*/
	int32_t prev_sample_rate;
	/*used to keep track of change for event firing*/
	int32_t offset;
	int32_t output_sample_size;	/*Optional output formatting.*/
	bool output_byte_swap;	/*Optional output formatting.*/
	int32_t partial_sample_bytes;
	int8_t partial_sample[SOC_AUDIO_MAX_BYTES_PER_SAMPLE];

	struct soc_audio_pipeline_buf_desc aux_buffer;
	struct soc_audio_format_specific_info stream_info;
};

enum soc_audio_packetize_result {
	SOC_AUDIO_PACKETIZE_SUCCESS = 0,
	SOC_AUDIO_PACKETIZE_ERROR_OPERATION_FAILED,
	SOC_AUDIO_PACKETIZE_ERROR_NULL_POINTER,
	SOC_AUDIO_PACKETIZE_ERROR_NOT_INITIALIZED,
	SOC_AUDIO_PACKETIZE_ERROR_NOT_AVAILABLE = 99
};

struct soc_audio_packetize_stream_info {
	int32_t sample_rate_content;
	int32_t sample_rate_transmit;
	int32_t channel_count_transmit;
	int32_t sample_size_transmit;
};

enum soc_audio_pass_through_mode {
	SOC_AUDIO_PASS_THROUGH_MODE_INVALID, /* Invalid passthrough mode.*/
	SOC_AUDIO_PASS_THROUGH_MODE_DIRECT,
	/* Pass through input will be output as is, no formatting.*/
	SOC_AUDIO_PASS_THROUGH_MODE_IEC61937
	/* Pass through input will be output formatted to the
	IEC61937 specification. (default)*/
};

struct soc_audio_input_pass_through_config {
	bool is_pass_through;
	 /*< Flag to indicate whether or not the input is in pass
\	through mode, if false, all other members in this struct
	will be ignored.*/
	bool dts_or_dolby_convert;
      /*< Flag to tell if conversion is needed from DTS-HD
	to-> DTS-5.1 or DD+ to-> AC3-5.1. Will inspect the current
	stream's algo to perform the conversion.*/
	enum soc_audio_format supported_formats[SOC_AUDIO_MEDIA_FMT_COUNT];
	/*< List of supported audio formats allowed to
	passthrough at this input.*/
	int supported_format_count;
	/*< Number of elements in the supported_fmts
	array member in this structure.*/
};

/* Function pointers for common packetizer functions. */
typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_lib_info_func_t)
	(void *lib_info_string);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_persistant_size_func_t)
(int32_t *size);

typedef enum soc_audio_packetize_result
(*soc_audio_packetize_mem_set_persistant_ptr_func_t)(void
						*persistant_base_addr);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_input_bytes_req_func_t)
(int32_t *in_bytes_req);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_mem_set_input_ptr_func_t)
(void *in_buf_ptr);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_mem_set_output_ptr_func_t)
(void *out_buf_ptr);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_set_output_size_func_t)
(int32_t output_buffer_size);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_set_input_bytes_func_t)
(int32_t in_buf_bytes);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_set_input_over_func_t)
(void);

typedef enum soc_audio_packetize_result
(*soc_audio_packetize_get_input_bytes_consumed_func_t) (int32_t *
							 in_bytes_consumed);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_do_execute_func_t) (void);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_output_bytes_func_t)
(int32_t *output_bytes);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_exe_done_query_func_t)
						(int32_t *exe_done);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_set_byte_swap_func_t)
						(bool byte_swap);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_set_expand_to_32b_func_t)
						(bool expand);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_stream_position_func_t)
						(int32_t *stream_pos);

typedef enum
soc_audio_packetize_result
(*soc_audio_packetize_get_frame_start_stream_position_func_t)
						(int32_t *stream_pos);

typedef enum
soc_audio_packetize_result(*soc_audio_packetize_get_stream_info_func_t)
		(struct soc_audio_packetize_stream_info *stream_info);

#ifdef PKT_OPT
typedef void (*soc_audio_packetize_set_circ_info_t)(int32_t start,
						     int32_t size);
#endif

struct soc_audio_packetizer_internal {
	uint32_t init_done;
	uint32_t format;
	uint32_t eos;
	uint32_t new_input;
	uint32_t input_bytes_copied;
	uint32_t sync_found;
	uint32_t pass_through_sample_rate;
	struct soc_audio_metadata_association *md_associate;
	enum soc_audio_pass_through_mode pass_through_mode;

	soc_audio_packetize_get_lib_info_func_t get_lib_info;
	soc_audio_packetize_get_persistant_size_func_t get_persistant_size;
	soc_audio_packetize_mem_set_persistant_ptr_func_t set_persistant_ptr;
	soc_audio_packetize_get_input_bytes_req_func_t get_input_bytes_req;
	soc_audio_packetize_mem_set_input_ptr_func_t set_input_ptr;
	soc_audio_packetize_mem_set_output_ptr_func_t set_output_ptr;
	soc_audio_packetize_set_output_size_func_t set_output_size;
	soc_audio_packetize_set_input_bytes_func_t set_input_bytes;
	soc_audio_packetize_set_input_over_func_t set_input_over;
	soc_audio_packetize_get_input_bytes_consumed_func_t
	    get_input_bytes_consumed;
	soc_audio_packetize_do_execute_func_t do_execute;
	soc_audio_packetize_get_output_bytes_func_t get_output_bytes;
	soc_audio_packetize_exe_done_query_func_t exe_done_query;
	soc_audio_packetize_set_byte_swap_func_t set_byte_swap;
	soc_audio_packetize_set_expand_to_32b_func_t set_expand_to_32b;
	soc_audio_packetize_get_stream_position_func_t get_stream_position;
	soc_audio_packetize_get_frame_start_stream_position_func_t
		get_frame_start_stream_position;
	soc_audio_packetize_get_stream_info_func_t get_stream_info;
#ifdef PKT_OPT
	int32_t circ_aux_start;
	soc_audio_packetize_set_circ_info_t set_circ_info;
#endif
};

#define SOC_AUDIO_MAX_ES_HEADER_BYTES 20

struct soc_audio_packetizer_params {
	uint32_t format;
	uint32_t init_done;
	uint32_t eos;
	int32_t expand_for_hw;
	uint32_t bytes_till_next_syncword;
	uint32_t syncword_byte_index;
	uint32_t input_bytes_copied;
	int32_t found_sync_need_frame_info;
	int32_t header_bytes_found;
	int32_t input_pts_buffer_used;
	uint32_t sample_rate;
	uint32_t new_input;
	uint8_t header[SOC_AUDIO_MAX_ES_HEADER_BYTES];
	struct soc_audio_packetizer_internal internal_params;
	struct soc_audio_pipeline_buf_desc aux_buffer;
	struct soc_audio_pipeline_buf_desc persistant_buffer;
	struct soc_audio_pipeline_buf_desc mda_buffer;
	/* Meta-data association buffer storage.. */
	struct soc_audio_packetizer_ddp_params ddp_param;
	struct soc_audio_packetizer_dts_hd_params dtshd_param;
};

struct soc_audio_interleaver_input {
	uint32_t output_smpl_size;
	uint32_t output_ch_count;
	int32_t output_ch_map;
	uint32_t reorder_for_hw;
};

struct soc_audio_interleaver_params {
	struct soc_audio_interleaver_input inputs[SOC_AUDIO_MAX_OUTPUTS];
};

struct soc_audio_chunk_divider {
	uint32_t out_buffer_level;
	uint32_t input_bytes_copied;
};

struct soc_audio_chunk_accumulator {
	uint32_t out_buffer_level;
};

struct soc_audio_delay_buffering_info {
	struct soc_audio_pipeline_buf_desc aux_buffer;
	uint32_t delay_ms;	/* store customer delay input */
	void *start_address;	/* circular buffer start address */
	void *end_address;	/* circular buffer end address */
	void *read_ptr;
	void *write_ptr;
	/* current buffering level of circular buffer */
	uint32_t bytes_buffered;
	/* delay bytes required calculated from customer input */
	uint32_t delay_bytes_count;
	/* silence bytes need to insert int32_to output */
	uint32_t silence_bytes_count;
};

struct soc_audio_delay_management_params {
	struct soc_audio_delay_buffering_info buf_mgmt[8];
	int32_t per_channel_delay[8];
	bool enabled;
	bool buffer_reset;
	bool delay_change;
};

#define SOC_AUDIO_WM_MAX_PARAMS_LEN 1472

struct soc_audio_watermark_management_params {
	uint8_t params[SOC_AUDIO_WM_MAX_PARAMS_LEN];
	uint32_t handle;
	uint32_t wl_id;
	int32_t detector;	/* the detector object */
	const int32_t *notif_events;
	struct soc_audio_pipeline_buf_desc aux_buffer;
	bool detector_exe_started;
	bool callback_needed;
	bool host_notified;
	bool enabled;
};

#define BIT_DEPTH_CONVERT_PASSTHROUGH -1

struct soc_audio_bit_depth_converter_context {
	int32_t forced_hw_output_ch_count;
	uint32_t output_sample_size;
};

struct soc_audio_bit_depth_converter_params {
	struct soc_audio_bit_depth_converter_context
	 context[SOC_AUDIO_MAX_OUTPUTS];
};

struct soc_audio_per_output_delay_output_info {
	bool buffer_reset;
	bool delay_change;
	uint32_t new_delay;
	struct soc_audio_pipeline_buf_desc aux_buffer;
	int32_t current_delay;
	int8_t *start_address;	/* circular buffer start address */
	int8_t *end_address;	/* circular buffer end address */
	int8_t *read_ptr;
	int8_t *write_ptr;
	uint32_t bytes_buffered;/* current buffering level of circular
				 buffer */
	uint32_t delay_bytes_count;	/* delay bytes required calculated
					 from customer input */
	uint32_t silence_bytes_count;	/* silence bytes need to insert into */
	int32_t prev_sample_rate;
	int32_t prev_sample_size;
	int32_t prev_channel_count;

};

struct soc_audio_per_output_delay_params {
	bool enabled;
	int32_t max_delay_ms;
	uint32_t buffer_size;
	struct soc_audio_per_output_delay_output_info
	 output_params[SOC_AUDIO_MAX_OUTPUTS];
};

struct soc_audio_bass_management_params {
	uint32_t enabled;
	struct soc_audio_per_speaker_setting speaker_settings;
	int32_t init_done;
	enum soc_audio_crossover_freq crossover_freq;
	struct soc_audio_pipeline_buf_desc aux_buffer;
};

struct soc_audio_quality_state {
	bool init_done;
	bool in_use;
	bool bypass_change;
	bool flt_param_change;
	bool ctl_param_change;
	bool config_change;
	bool avl_config_change;
#if SOC_CACHELINE_PADDING
	uint8_t cacheline_pad[57];	/* 64 byte alligned. */
#endif
};

struct soc_audio_quality_context {
	struct soc_audio_quality_state state;
	struct soc_audio_acesystem ace_system;
};

struct soc_audio_quality_params {
	struct soc_audio_quality_context *context[SOC_AUDIO_MAX_OUTPUTS];
	struct soc_audio_stage_params_buf_desc aq_buffer[SOC_AUDIO_MAX_OUTPUTS];
};

enum soc_audio_ms10_ddc_codec_status {
	SOC_AUDIO_MS10_DDC_FRAME_DECODED = 0, /* frame decoded successfully */
	SOC_AUDIO_MS10_DDC_FRAME_PENDING, /* frame pending on more input data */
	SOC_AUDIO_MS10_DDC_FRAME_ERROR,	      /* frame decoding error */
};

struct soc_audio_ms10_ddc_sync_params {
	bool sync_enable;
	int32_t main_ddc_sync;/* keep the main pipeline's ddc_sync stage
				 address */
	uint32_t frame_index_main;/* decoded frame count since new input
				 from main audio */
	uint32_t frame_index_assoc;/* decoded frame count since new input
				 from associate audio */
	enum soc_audio_ms10_ddc_codec_status cur_main_status;/* current
				 decoded frame status from main */
	enum soc_audio_ms10_ddc_codec_status cur_assoc_status;/* current
				 decoded frame status from assoc */

	uint32_t sample_rate;
	uint32_t blk_per_frame;
	int32_t convsync_main_in_associated;
	int32_t repeat_or_misordered_main;
	int32_t repeat_or_misordered_assoc;
};
struct soc_audio_input_params {
	struct soc_audio_bu_specific_input_params params[SOC_AUDIO_MAX_INPUTS];
};
struct soc_audio_output_params {
	struct soc_audio_bu_specific_output_params
	 params[SOC_AUDIO_MAX_OUTPUTS];
};

union soc_audio_pipeline_stage_params {
#ifdef INCLUDE_INPUT_STAGE
	struct soc_audio_input_params input;
#endif
#ifdef INCLUDE_OUTPUT_STAGE
	struct soc_audio_output_params output;
#endif
#ifdef INCLUDE_MIXER_STAGE
	struct soc_audio_mixer_params mixer;
#endif
#ifdef INCLUDE_PCM_DECODE_STAGE
	struct soc_audio_pcm_decode_params pcm_dec;
#endif
#if defined(INCLUDE_DECODE_STAGE) || defined(INCLUDE_ENCODE_STAGE)
	struct soc_audio_pipeline_decode_params decoder;
#endif
#if defined(INCLUDE_SRC_STAGE) || defined(INCLUDE_POSTSRC_STAGE) ||\
		 defined(INCLUDE_PRESRC_STAGE)
	struct soc_audio_pipeline_src_params src;
#endif
#ifdef INCLUDE_WATERMARK_STAGE
	struct soc_audio_watermark_management_params watermark;
#endif
#ifdef INCLUDE_DOWNMIXER_STAGE
	struct soc_audio_downmix_params downmixer;
#endif

#ifdef INCLUDE_QUALITY_STAGE
	struct soc_audio_quality_params audio_quality;
#endif

#ifdef INCLUDE_INTERLEAVER_STAGE
	struct soc_audio_interleaver_params interleaver;
#endif
#ifdef INCLUDE_PACKETIZER_STAGE
	struct soc_audio_packetizer_params packetizer;
#endif
#ifdef INCLUDE_DATA_DIVIDER_STAGE
	struct soc_audio_chunk_divider chunk_div;
#endif
#ifdef INCLUDE_BASS_MANAGEMENT_STAGE
	struct soc_audio_bass_management_params bass_man;
#endif
#ifdef INCLUDE_DELAY_MANAGEMENT_STAGE
	struct soc_audio_delay_management_params delay_mgmt;
#endif
#ifdef INCLUDE_DATA_ACCUMULATOR_STAGE
	struct soc_audio_chunk_accumulator acumultr;
#endif
#ifdef INCLUDE_BIT_DEPTH_CONVERTER_STAGE
	struct soc_audio_bit_depth_converter_params bit_depth_converter;
#endif
#ifdef INCLUDE_PER_OUTPUT_DELAY_STAGE
	struct soc_audio_per_output_delay_params per_output_delay;
#endif
#ifdef INCLUDE_MS10_DDC_SYNC_STAGE
	struct soc_audio_ms10_ddc_sync_params ms10_ddc_sync;
#endif
};

/* Use this structure to store metadata about inputs/outputs
connected to the stage. */
struct soc_audio_pipeline_stage_connection_metadata {
	int32_t output[SOC_AUDIO_MAX_OUTPUTS];
	int32_t input[SOC_AUDIO_MAX_INPUTS];
};

/* -------------------------------------------------------------------------*/
/* This structure describes a single stage in the DSP pipe. A Stage could
   be a decode stage, a mix stage etc. Sequence of stages make a pipe. */
/* -------------------------------------------------------------------------*/
struct soc_audio_pipeline_stage {
	uint32_t inputs_count;
	uint32_t outputs_count;
	uint32_t in_use;
	/* the task that should be executed for during that stage. */
	enum soc_audio_pipeline_stage_task task;
	union soc_audio_pipeline_stage_params stage_params;

	struct soc_audio_pipeline_bu_stage bu_stage;
#if SOC_CACHELINE_PADDING
#if PIPE_STAGE_PERFORMANCE
	uint8_t cacheline_pad[24];
#else
	uint8_t cacheline_pad[40];
#endif
#endif
};

/* ----------------------------------------------------*/
/* Structure represents a pipeline in a DSP */
/* ----------------------------------------------------*/
struct soc_audio_pipeline {
	/* Each wl is associated with workload in the fw. This handle is used
	 when passing messages to fw. */
	uint32_t fw_handle;
	/* Is set to true once the pipe is added and is in a started state. */
	uint32_t started;
	uint32_t configured;	/* Pipe needs to be configured. */
	uint32_t type;		/* type of pipe */
	uint32_t num_stages;
	enum soc_audio_pipeline_op op;	/* voice or music */
	enum soc_audio_ops_type op_type;	/* Playback or capture */
	void *context;		/* memory context to be freed */
#if SOC_CACHELINE_PADDING
	uint8_t cacheline_pad[32];
#endif
	/* Variable length array based on number of stages */
	struct soc_audio_pipeline_stage stages[1];
};

#endif /* _SOC_AUDIO_PIPELINE_DEFS_H */

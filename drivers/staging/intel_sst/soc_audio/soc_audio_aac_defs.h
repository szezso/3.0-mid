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

#ifndef _SOC_AUDIO_AAC_DEF_H
#define _SOC_AUDIO_AAC_DEF_H

/* Use these defines to change AAC decoder parameters */

/* Flag to specify duplication of mono signals to stereo. */
#define SOC_AUDIO_AAC_MONO_TO_STEREO_MODE		0xaac00001

/* Flag to dynamically enable/disbale AAC downmixing. */
#define SOC_AUDIO_AAC_DOWNMIXER_ON_OFF			0xaac00002

/* Flag to specify duplication (and scaling by 0.707) of /S into Ls and Rs. */
#define SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_MODE	0xaac00003

/* Flag to enable/disable PRL functionality. By default, PRL is disabled.
Valid value: 0(disable), 1(enable) */
#define SOC_AUDIO_AAC_PRL_ENABLE			0xaac00004

/* Flag to enable/disable PRL functionality. By default, DRC is disabled.
Valid value: 0(disable), 1(enable) */
#define SOC_AUDIO_AAC_DRC_ENABLE			0xaac00005

/* Parameter to set the desried target level of the decoder output.
This is only applicable if the program reference level is available
in the bit-stream. If the program reference level information is NOT
available in the stream, then the output does not undergo any scaling.
The output of the decoder is scaled to the desired target output level
only if the parameter SOC_AUDIO_AAC_PRL_ENABLE is enabled.
target_level valid value: 0~127, which could be calculated by
target_value = (-4 * desired_output_level_in_db) */
#define SOC_AUDIO_AAC_TARGET_LEVEL			0xaac00006

/* Parameter to control the scaling of PCM data so that they are
compressed as mentioned in ISO/IEC14496-3.
Valid range: 0 ~ 100, denoting DRC_cut_percentage of 0 ~1.0.
The scaling is applied only if the apply_drc flag */
#define SOC_AUDIO_AAC_DRC_COMPRESS_FAC			0xaac00008

/* Parameter to control the scaling of PCM data so that they are
boosted as mentioned in ISO/IEC14496-3.
Valid range: 0 ~ 100, denoting DRC_boost_percentage of 0 ~1.0.
The scaling is applied only if the apply_drc flag */
#define SOC_AUDIO_AAC_DRC_BOOST_FAC			0xaac00009

/* Parameter of the PCM sample size in bits.
 * Valid value: 16, 24 (default).
 */
#define SOC_AUDIO_AAC_PCM_WD_SZ				0xaac0000a

/* Parameter of the PCM sample rate.
 * Valid range: 8000 ~ 96000Hz. Default is 44100.
 */
#define SOC_AUDIO_AAC_PCM_SAMPLE_RATE			0xaac0000b

/* Valid values to specify for decode param mono_stereo */
enum soc_audio_aac_mono_to_stereo {
	/* Mono streams are presented as a single channel routed to C */
	SOC_AUDIO_AAC_MONO_TO_STEREO_SINGLE_CHANNEL,
	/* Mono streams are presented as 2 identical channels (default)
	L = R = 0.707xC */
	SOC_AUDIO_AAC_MONO_TO_STEREO_2_CHANNELS
};

/* Valid values to specify for decode param b_aac_downmixer */
enum soc_audio_aac_downmixer {
	SOC_AUDIO_AAC_DOWNMIXER_DISABLE, /* Disable AAC downmixer */
	SOC_AUDIO_AAC_DOWNMIXER_ENABLE	/* Enable AAC downmixer */
};

/* Valid values to specify for decode param drc_boost_fac */
enum soc_audio_aac_surround_mono_to_stereo {
	/** Ls = /S */
	SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_SINGLE_CHANNEL,
	/** Ls = Rs = 0.707x /S */
	SOC_AUDIO_AAC_SURROUND_MONO_TO_STEREO_2_CHANNELS
};

#endif /* _SOC_AUDIO_AAC_DEF_H */

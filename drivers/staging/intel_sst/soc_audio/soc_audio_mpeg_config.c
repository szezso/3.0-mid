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
#include "soc_audio_common.h"
#include "soc_audio_defs.h"
#include "soc_debug.h"
#include "soc_audio_mpeg_config.h"

#ifdef INCLUDE_MPEG_DECODE

/******************************************************************************/
/* Audio MPEG Private function definition */
/******************************************************************************/

/*
This function check for mpeg decode parameters
*/
enum soc_result audio_mpeg_check_dec_params(struct soc_audio_codec_params
					    *param_value)
{
	enum soc_result result = SOC_SUCCESS;

	SOC_ENTER();

	/* Check the boundary for MPEG CRC check */
	if (!(param_value->config.mpeg_params.crc_check == true ||
	      param_value->config.mpeg_params.crc_check == false)) {
		soc_debug_print(ERROR_LVL, "MPEG: SOC_AUDIO_MPEG_CRC_CHECK"
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check the boundary for MPEG MCH enable */
	if (!(param_value->config.mpeg_params.mch_enable == true ||
	      param_value->config.mpeg_params.mch_enable == false)) {
		soc_debug_print(ERROR_LVL, "MPEG: SOC_AUDIO_MPEG_MCH_ENABLE"
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

	/* Check the boundary for PCM WD SZ */
	if (param_value->config.mpeg_params.pcm_wd_sz < 0 ||
	    param_value->config.mpeg_params.pcm_wd_sz > 32) {
		soc_debug_print(ERROR_LVL, "MPEG: SOC_AUDIO_PCM_WD_SZ"
				"param invalid!");
		result = SOC_ERROR_INVALID_PARAMETER;
		goto error_exit;
	}

error_exit:
	SOC_EXIT();
	return result;
}

/*
This function is used to set a specific MPEG decode parameter.
*/
enum soc_result audio_mpeg_set_dec_param(struct soc_audio_input_wl *input_wl,
					 int param_type,
					 void *param_value)
{
	enum soc_result result = SOC_SUCCESS;
	struct soc_audio_codec_params *codec_params;

	SOC_ENTER();
	codec_params =
	    &input_wl->pipeline->
	    stages[SOC_AUDIO_DEC_DECODE_STAGE].stage_params.decoder.host;

	switch (param_type) {

	case SOC_AUDIO_MPEG_PCM_WD_SZ:
		codec_params->config.mpeg_params.pcm_wd_sz =
		    *(uint32_t *) param_value;
		break;

	case SOC_AUDIO_MPEG_MCH_ENABLE:
		codec_params->config.mpeg_params.mch_enable =
		    *(uint32_t *) param_value;
		break;

	default:
		result = SOC_FAILURE;
	}

	SOC_EXIT();

	return result;
}

/*
This function is used to set mpeg decode parameters
*/
void audio_mpeg_set_default_dec_params(struct soc_audio_codec_params
				       *param)
{
	SOC_ENTER();
	param->config.mpeg_params.crc_check = MPEG_CRC_CHECK;
	/*24 packed into 32bits. */
	param->config.mpeg_params.pcm_wd_sz = MPEG_PCM_WD_SZ;
	param->config.mpeg_params.mch_enable = MPEG_MCH_ENABLE;

	SOC_EXIT();
}

#endif

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

#include "soc_debug.h"
#include "soc_audio_defs.h"
#include "soc_audio_pipeline_defs.h"
#include "soc_audio_core.h"

int soc_audio_core_get_channel_cnt_from_config(int channel_config)
{
	int loop = 0;
	int channel_count = 0;
	SOC_ENTER();
	for (loop = 0; loop < SOC_AUDIO_MAX_INPUT_CHANNELS; loop++) {
		if (((channel_config >> (loop * 4)) & 0xF) !=
		    SOC_AUDIO_CHANNEL_INVALID) {
			channel_count++;
		}
	}
	SOC_EXIT();
	return channel_count;
}

bool soc_audio_core_is_valid_ch_config(int channel_config)
{
	int loop = 0;
	int curr_channel = 0;
	bool ret_value = true;
	SOC_ENTER();
	for (loop = 0; loop < SOC_AUDIO_MAX_INPUT_CHANNELS; loop++) {
		curr_channel = ((channel_config >> (loop * 4)) & 0xF);
		if ((curr_channel != SOC_AUDIO_CHANNEL_INVALID)
		    && ((curr_channel < 0) || (curr_channel > 7))) {
			ret_value = false;
			break;
		}
	}
	SOC_EXIT();
	return ret_value;
}

bool soc_audio_core_is_valid_sample_rate(int sample_rate)
{
	bool result = false;
	switch (sample_rate) {
	case 8000:
	case 11025:
	case 12000:
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 176400:
	case 192000:
		result = true;
		break;
	default:
		soc_debug_print(ERROR_LVL,
				"Sample rate (%d) not supported!", sample_rate);
		break;
	}
	return result;
}

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

#ifndef _SOC_DEBUG_H
#define _SOC_DEBUG_H


#include <linux/kernel.h>
#include <linux/mutex.h>

enum debuglvl {
	SILENT_LVL = 0,
	ERROR_LVL,
	WARNING_LVL,
	DEBUG_LVL_1,
	DEBUG_LVL_2,
	DEBUG_LVL_3,
};

extern uint32_t soc_audio_debug_lvl;
#define soc_debug_print(debuglvl, ...) \
	do { \
		if (debuglvl < soc_audio_debug_lvl + 1) \
			pr_debug(__VA_ARGS__); \
	} while (0)

#define SOC_ENTER() \
	soc_debug_print(DEBUG_LVL_2, "%s: %s enter\n", __FILE__, __func__)

#define SOC_EXIT() \
	soc_debug_print(DEBUG_LVL_2, "%s: %s exit\n", __FILE__, __func__)

/* Call after SOC_ENTER for arguments */
#define SOC_AUDIO_CHK_EARGS(x) \
	do { \
		if (!(x)) { \
			soc_debug_print(ERROR_LVL, "Invalid arguments!\n"); \
			SOC_EXIT(); \
			WARN_ON(1); \
			return SOC_ERROR_INVALID_PARAMETER; \
		} \
	} while (0)
/* Call this after calling SOC_ENTER to check soc_audio_init_done
and processor_h */
#define SOC_AUDIO_CHK_PROC_EARGS(processor_h) \
	do { \
		if (processor_h >= SOC_AUDIO_MAX_PROCESSORS) { \
			soc_debug_print(ERROR_LVL,\
				"Invalid processor handle!\n"); \
			SOC_EXIT(); \
			return SOC_ERROR_INVALID_PARAMETER; \
		} \
		if (!soc_audio_init_done) { \
			soc_debug_print(ERROR_LVL,\
				"audio module uninitialized!\n"); \
			SOC_EXIT(); \
			return SOC_ERROR_INVALID_PARAMETER; \
		} \
	} while (0)

/* Call this after getting the lock to check the ref_count */
#define SOC_AUDIO_CHK_REF_COUNT_EARGS(ctx) \
	do { \
		if (ctx->ref_count == 0) { \
			soc_debug_print(ERROR_LVL, "No processor yet!\n"); \
			mutex_unlock(&ctx->processor_lock); \
			SOC_EXIT(); \
			return SOC_ERROR_INVALID_PARAMETER; \
		} \
	} while (0)

#endif

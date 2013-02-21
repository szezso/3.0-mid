/*
 *	intel_alsa_ssp_hw_interface.h
 *
 *  Copyright (C) 2010 Intel Corp
 *  Authors:	Selma Bensaid <selma.bensaid@intel.com>
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
 */

#ifndef _INTEL_ALSA_SSP_HW_INTERFACE_H
#define _INTEL_ALSA_SSP_HW_INTERFACE_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <sound/core.h>


#define INTEL_ALSA_SSP_PLAYBACK 0
#define INTEL_ALSA_SSP_CAPTURE  1

#define INTEL_ALSA_SSP_SND_CARD_MAX_DEVICES     2

#define INTEL_ALSA_BT_DEVICE_ID      0
#define INTEL_ALSA_FM_DEVICE_ID      1

enum intel_alsa_ssp_stream_status {
	INTEL_ALSA_SSP_STREAM_INIT = 0,
	INTEL_ALSA_SSP_STREAM_STARTED,
	INTEL_ALSA_SSP_STREAM_RUNNING,
	INTEL_ALSA_SSP_STREAM_PAUSED,
	INTEL_ALSA_SSP_STREAM_DROPPED,
};

enum intel_alsa_ssp_control_id {
	INTEL_ALSA_SSP_CTRL_SND_OPEN = 0x1000,
	INTEL_ALSA_SSP_CTRL_SND_PAUSE = 0x1001,
	INTEL_ALSA_SSP_CTRL_SND_RESUME = 0x1002,
	INTEL_ALSA_SSP_CTRL_SND_CLOSE = 0x1003,
};

/**
 * @period_req_index: ALSA index ring buffer updated by the DMA transfer
 *		request goes from 0 .. (period_size -1)
 * @period_cb_index: ALSA index ring buffer updated by the DMA transfer
 *		callback goes from 0 .. (period_size -1)
 * @period_index_max : ALSA ring Buffer number of periods
 * @addr: Virtual address of the DMA transfer
 * @length: length in bytes of the DMA transfer
 * @dma_running: Status of DMA transfer
 */
struct intel_alsa_ssp_dma_buf {
	u32 period_req_index;
	u32 period_cb_index;
	u32 period_index_max;
	u8 *addr;
	int length;
};

struct intel_alsa_ssp_stream_info {
	struct intel_alsa_ssp_dma_buf dma_slot;
	struct snd_pcm_substream *substream;
	ssize_t dbg_cum_bytes;
	unsigned long stream_status;
	unsigned int device_id;
	unsigned int stream_dir;
	unsigned int device_offset;
	unsigned int stream_index;
};

struct intel_alsa_stream_status {
	void *ssp_handle;
	spinlock_t lock;
	unsigned long stream_info;

};
int intel_alsa_ssp_control(int command, struct intel_alsa_ssp_stream_info *pl_str_info);
int intel_alsa_ssp_transfer_data(struct intel_alsa_ssp_stream_info *str_info);
int intel_alsa_ssp_dma_capture_complete(void *param);
int intel_alsa_ssp_dma_playback_complete(void *param);
void intel_alsa_reset_ssp_status(void);

#endif /* _INTEL_ALSA_SSP_HW_INTERFACE_H */

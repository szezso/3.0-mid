/*
 *  intel_sst_stream.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
 *  Authors:	Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Anurag Kansal <anurag.kansal@intel.com>
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
 *  This file contains the stream operations of SST driver
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "intel_sst.h"
#include "intel_sst_common.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_audio_api.h"

/**
* sst_set_vol - This fuction allows to set the premix gain or gain of a stream
*
* @set_vol:	this holds the volume structure that needs to be set
*
* This function is called when premix gain or stream gain is requested to be set
*/
int sst_set_vol(struct snd_sst_vol *set_vol)
{
	int retval = 0;
	int input_volume = set_vol->volume;
	uint32_t str_id = set_vol->stream_id;
	enum soc_result result;
	struct soc_audio_channel_mix_config ch_mix_config;
	uint8_t num_chan = sst_drv_ctx->streams[str_id].num_chan;

	sst_drv_ctx->volume[str_id] = input_volume;
	generate_ch_mix_config(&ch_mix_config, input_volume, num_chan);
	result = soc_audio_input_set_channel_mix(sst_drv_ctx->proc_hdl,
				sst_drv_ctx->streams[str_id].soc_input_id,
				&ch_mix_config);
	if (SOC_SUCCESS == result)
		retval = 0;
	else {
		pr_err("soc_audio_input_set_channel_mix failed!");
		retval = -EIO;
	}
	return retval;
}

/**
* sst_set_mute - This fuction sets premix mute or soft mute of a stream
*
* @set_mute:	this holds the mute structure that needs to be set
*
* This function is called when premix mute or stream mute requested to be set
*/
int sst_set_mute(struct snd_sst_mute *set_mute)
{
	int retval = 0;
	uint32_t str_id = set_mute->stream_id;
	uint32_t input_volume = SOC_AUDIO_GAIN_MUTE;

	enum soc_result result;
	struct soc_audio_channel_mix_config ch_mix_config;
	uint8_t num_chan = sst_drv_ctx->streams[str_id].num_chan;

	sst_drv_ctx->volume[str_id] = SOC_AUDIO_GAIN_MUTE;
	generate_ch_mix_config(&ch_mix_config, input_volume, num_chan);
	result = soc_audio_input_set_channel_mix(
			sst_drv_ctx->proc_hdl,
			sst_drv_ctx->streams[str_id].soc_input_id,
			&ch_mix_config);
	if (SOC_SUCCESS == result)
		retval = 0;
	else {
		pr_err("soc_audio_input_set_channel_mix failed!");
		retval = -EIO;
	}
	return retval;
}

/* This function creates the scatter gather list to be sent to firmware to
capture/playback data*/
static int sst_create_sg_list(struct stream_info *stream,
		struct sst_frame_info *sg_list)
{
	struct sst_stream_bufs *kbufs = NULL;
	int i = 0;

	list_for_each_entry(kbufs, &stream->bufs, node) {
		if (kbufs->in_use == false) {
			if (stream->ops != STREAM_OPS_PLAYBACK_DRM) {
				sg_list->addr[i].addr = virt_to_phys((void *)
								     kbufs->
								     addr +
								     kbufs->
								     offset);
				sg_list->addr[i].size = kbufs->size;
				pr_debug("phyaddr[%d]:0x%x Size:0x%x\n", i,
					 sg_list->addr[i].addr, kbufs->size);
				/* TODO determine a right check */
				if ((stream->device ==
				     SND_SST_DEVICE_COMPRESSED_PLAY)
				    || (stream->device ==
					SND_SST_DEVICE_COMPRESSED_CAPTURE)) {
					stream->ring_buffer_size = kbufs->size;
					stream->ring_buffer_addr =
					    virt_to_phys((void *)
							 kbufs->addr +
							 kbufs->offset);
					pr_debug("Setting the Ring buffer"
					"Address 0x%x Size:0x%x\n",
					     stream->ring_buffer_addr,
					     stream->ring_buffer_size);
				}
			}
			stream->curr_bytes += sg_list->addr[i].size;
			kbufs->in_use = true;
			i++;
		}
		if (i >= MAX_NUM_SCATTER_BUFFERS)
			break;
	}

	sg_list->num_entries = i;
	pr_debug("sg list entries = %d\n", sg_list->num_entries);
	return i;
}


/**
 * sst_play_frame - Send msg for sending stream frames
 *
 * @str_id:	ID of stream
 *
 * This function is called to send data to be played out
 * to the firmware
 */
int sst_play_frame(int str_id)
{
	int retval = 0;
	struct sst_frame_info sg_list = { 0 };
	struct sst_stream_bufs *kbufs = NULL, *_kbufs;
	struct stream_info *stream;

	pr_debug("play frame for %d\n", str_id);
	retval = sst_validate_strid(str_id);
	if (retval)
		return retval;

	stream = &sst_drv_ctx->streams[str_id];

	/* clear prev sent buffers */
	list_for_each_entry_safe(kbufs, _kbufs, &stream->bufs, node) {
		if (kbufs->in_use == true) {
			list_del(&kbufs->node);
			kfree(kbufs);
		}
	}
	/* update bytes sent */
	stream->cumm_bytes += stream->curr_bytes;
	stream->curr_bytes = 0;
	if (list_empty(&stream->bufs)) {
		/* no user buffer available */
		stream->status = STREAM_INIT;
		return 0;
	}

	/* create list */
	sst_create_sg_list(stream, &sg_list);
	return 0;

}

/**
 * sst_capture_frame - Send msg for sending stream frames
 *
 * @str_id:	ID of stream
 *
 * This function is called to capture data from the firmware
 */
int sst_capture_frame(int str_id)
{
	int retval = 0;
	struct sst_frame_info sg_list = { 0 };
	struct sst_stream_bufs *kbufs = NULL, *_kbufs;
	struct stream_info *stream;

	pr_debug("capture frame for %d\n", str_id);
	retval = sst_validate_strid(str_id);
	if (retval)
		return retval;

	stream = &sst_drv_ctx->streams[str_id];

	/* clear prev sent buffers */
	list_for_each_entry_safe(kbufs, _kbufs, &stream->bufs, node) {
		if (kbufs->in_use == true) {
			list_del(&kbufs->node);
			kfree(kbufs);
			pr_debug("del node\n");
		}
	}
	if (list_empty(&stream->bufs)) {
		/* no user buffer available */
		stream->status = STREAM_INIT;
		return 0;
	}
	/* create new sg list */
	sst_create_sg_list(stream, &sg_list);

	/*update bytes recevied*/
	stream->cumm_bytes += stream->curr_bytes;
	stream->curr_bytes = 0;

	pr_debug("Cum bytes  = %d\n", stream->cumm_bytes);
	return 0;
}

/*
 *  intel_sst_interface.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
 *  Authors:	Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Anurag Kansal <anurag.kansal@intel.com>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
 *  This driver exposes the audio engine functionalities to the ALSA
 *	and middleware.
 *  Upper layer interfaces (MAD driver, MMF) to SST driver
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <linux/pm_runtime.h>
#include <linux/poll.h>
#include "intel_sst.h"
#include "intel_sst_ioctl.h"
#include "soc_audio_bu_config.h"
#include "intel_sst_common.h"
#include "soc_audio_api.h"
#include "soc_ipc.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_audio_mpeg_config.h"
#ifdef INCLUDE_LOOPBACK_IF
#include "intel_sst_loopback.h"
#endif

/*
 * sst_download_fw - download the audio firmware to DSP
 *
 * This function is called when the FW needs to be downloaded to SST DSP engine
 */
int sst_download_fw(void)
{
	int retval = 0;

	char name[20];
	enum soc_result result;
	uint32_t soc_hdl;
	struct soc_audio_output_config config;
	void *output;

	if (sst_drv_ctx->fw_present)
		return retval;

	memset_io(sst_drv_ctx->mailbox, 0, SOC_AUDIO_DSP_MAILBOX_SIZE);
	pr_debug("Shared SRAM cleared\n");
	snprintf(name, sizeof(name), "%s%04x%s", "fw_sst_",
					sst_drv_ctx->pci_id, ".bin");

	pr_debug("Downloading %s FW now...\n", name);
	if (!sst_drv_ctx->fw) {
		retval = request_firmware(&sst_drv_ctx->fw, name,
				 &sst_drv_ctx->pci->dev);
		if (retval) {
			pr_err("request fw failed %d\n", retval);
			return retval;
		}
	}
	retval = sst_load_fw(sst_drv_ctx->fw, NULL);
	if (retval) {
		release_firmware(sst_drv_ctx->fw);
		sst_drv_ctx->fw = NULL;
		return retval;
	}
	/* Wait for formware download to complete */
	result = soc_ipc_waittimeout(&soc_audio_g_init_data);
	if (result != SOC_SUCCESS) {
		pr_err("fw download failed %d\n", result);
		release_firmware(sst_drv_ctx->fw);
		sst_drv_ctx->fw = NULL;
		return -EIO;
	}

	/* open global processor */
	result = soc_audio_open_processor(true, &soc_hdl);
	if (result != SOC_SUCCESS) {
		pr_err("open processor failed\n");
		return -EIO;
	}
	/* add output */
	config.stream_delay = 0;
	config.sample_size = SNDRV_PCM_FMTBIT_S16_LE;
	config.ch_config = SOC_AUDIO_STEREO;
	config.sample_rate = SOC_AUDIO_OUTPUT_SAMPLE_RATE;
	config.out_mode = SOC_AUDIO_OUTPUT_PCM;
	config.ch_map = 0;
	result = soc_audio_output_add(soc_hdl, config, &output);
	if (result != SOC_SUCCESS) {
		pr_err("sst_add_output failed %d\n", result);
		soc_audio_close_processor(soc_hdl);
		return -EIO;
	}
	sst_drv_ctx->proc_hdl = soc_hdl;
	sst_drv_ctx->output_wl = output;
	sst_drv_ctx->fw_present = 1;

	return retval;
}

/*
 * sst_download_codec - download the audio codec to DSP
 * This function is called when the Codec needs to be downloaded
 * to SST DSP engine
 */
int sst_download_codec(int format, int operation)
{
	int retval;
	const struct firmware *sst_lib;
	char name[256];
	char ops[20];
	char codec[20];

	switch (operation) {
	case STREAM_OPS_PLAYBACK:
		strncpy(ops, "dec", 20);
		break;
	case STREAM_OPS_CAPTURE:
		strncpy(ops, "enc", 20);
		break;
	default:
		pr_err("sst_download_codec : wrong operation %d", operation);
		return -EPERM;
	}

	switch (format) {
	case SST_CODEC_TYPE_MP3:
		strncpy(codec, "mp3", 20);
		break;
	case SST_CODEC_TYPE_AAC:
		strncpy(codec, "aac", 20);
		break;
	default:
		pr_err("sst_download_codec : wrong format %d", format);
		return -EPERM;
	}

	snprintf(name, sizeof(name), "%s_%s_%d.bin", codec, ops, 1);

	pr_debug("Downloading %s Codec Library now...\n", name);
	retval = request_firmware(&sst_lib, name, &sst_drv_ctx->pci->dev);
	if (retval) {
		pr_err("request fw failed %d\n", retval);
		return retval;
	}

	retval = sst_load_codec(sst_lib, NULL);

	release_firmware(sst_lib);

	return retval;
}

/* This function adds the device/stream to
 * the list of active streams
 * */
static int add_to_active_streams(int str_id)
{
	int i;
	/* Look for a free slot to store the
	 * stream/device id */
	for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
		if (SND_SST_DEVICE_NONE == sst_drv_ctx->active_streams[i])
			break;
	}

	if (i == SOC_AUDIO_MAX_INPUTS) {
		pr_err("SOC_AUDIO max stream limit reached\n");
		return -1;
	}
	pr_debug("add_to_active_streams: str_id: %d, "
		"soc input id: %d\n", str_id, i);
	sst_drv_ctx->active_streams[i] = str_id;
	sst_drv_ctx->streams[str_id].soc_input_id = i;
	return 0;
}

/* This function deletes the device/stream from
 * the list of active streams
 * */
static int delete_from_active_streams(int str_id)
{
	int i = sst_drv_ctx->streams[str_id].soc_input_id;
	if (i < SOC_AUDIO_MAX_INPUTS && i >= 0) {
		sst_drv_ctx->active_streams[i] = SND_SST_DEVICE_NONE;
		sst_drv_ctx->streams[str_id].soc_input_id = -1;
	} else {
		pr_err("Failed to find device: %d, in active_strems\n", i);
		return -1;
	}
	pr_debug("delete_from_active_streams: str_id: %d, "
		"soc input id: %d\n", str_id, i);
	return 0;
}

void free_stream_context(unsigned int str_id)
{
	struct stream_info *stream;
	struct sst_stream_bufs *bufs = NULL, *_bufs;

	if (!sst_validate_strid(str_id)) {
		stream = &sst_drv_ctx->streams[str_id];
		stream->status = STREAM_UN_INIT;
		list_for_each_entry_safe(bufs, _bufs, &stream->bufs, node) {
			list_del(&bufs->node);
			kfree(bufs);
		}
		delete_from_active_streams(str_id);
		if (stream->ops == STREAM_OPS_PLAYBACK ||
				stream->ops == STREAM_OPS_PLAYBACK_DRM)
			sst_drv_ctx->pb_streams--;
		else if (stream->ops == STREAM_OPS_CAPTURE)
			sst_drv_ctx->cp_streams--;
	}
}

/*
 * sst_close_pcm_stream - Close PCM interface
 *
 * @str_id: stream id to be closed
 *
 * This function is called by MID sound card driver to close
 * an existing pcm interface
 */
static int sst_close_pcm_stream(unsigned int str_id)
{
	int i;
	struct stream_info *stream;

	pr_debug("sst_close_pcm_stream called, str_id = %d\n", str_id);
	if (str_id == 0xFF) {
		pr_debug("str_id is wrong\n");
		return 0;
	}

	if (sst_validate_strid(str_id))
		return -EINVAL;

	stream = &sst_drv_ctx->streams[str_id];

	/* Flush the workqueue before we close the stream. */
	flush_workqueue(sst_drv_ctx->mad_wq);
	mutex_lock(&sst_drv_ctx->stream_lock);

	/* Set the input state here as we can't make sure
	 * sst_process_mad_ops happens before this function.
	 * This is a condition for which DROP is not explicitly
	 * called by the alsa. We shall ideally not enter this
	 * condition. */
	if (((struct soc_audio_input_wl *)stream->input_wl)->state
	    == SOC_DEV_STATE_PLAY) {
		soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
					  stream->input_wl, SOC_DEV_STATE_STOP);
	}
	soc_audio_input_remove(sst_drv_ctx->proc_hdl, stream->input_wl);

	free_stream_context(str_id);
	stream->pcm_substream = NULL;
	stream->period_elapsed = NULL;
	sst_drv_ctx->stream_cnt--;

#ifdef INCLUDE_LOOPBACK_IF
	if (0 != loopback_if.user) {
		/* Check if there exist any open streams.
		 * Exit loopback only if none of the streams
		 * are active */
		int stream_id;
		for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
			stream_id = sst_drv_ctx->active_streams[i];
			if (SND_SST_DEVICE_NONE == stream_id)
				continue;
			if (sst_drv_ctx->streams[stream_id].status
				== STREAM_INIT)
				break;
		}

		if (i == SOC_AUDIO_MAX_INPUTS) {
			/* None of the devices/streams are active
			 * closing loopback */
			unsigned long flags;
			pr_debug("Exiting Loopback\n");
			mutex_lock(&loopback_if.lock);
			spin_lock_irqsave(&loopback_if.spin_lock, flags);
			loopback_if.user = 0;
			loopback_if.flag = 1;
			wake_up(&loopback_if.sleep);
			spin_unlock_irqrestore(&loopback_if.spin_lock, flags);
			mutex_unlock(&loopback_if.lock);
		}
	}
#endif
	mutex_unlock(&sst_drv_ctx->stream_lock);
	pr_debug("doing rtpm_put now\n");
	sst_pm_runtime_put(&sst_drv_ctx->pci->dev);
	return 0;
}

/*
 * sst_set_stream_params - this function sets the stream parameters into the
 * stream_info array of sst_drv_ctx
 * @str_param :stream params
 * stream_id : the stream index to update paramters format
 *
 */
static int sst_set_stream_params(struct snd_sst_params *str_param,
					int stream_id)
{

	enum soc_audio_format format;
	switch (str_param->codec) {

	case SST_CODEC_TYPE_PCM:
		sst_drv_ctx->streams[stream_id].period_count =
		    str_param->sparams.uc.pcm_params.period_count;
		sst_drv_ctx->streams[stream_id].ring_buffer_size =
		    str_param->sparams.uc.pcm_params.ring_buffer_size;
		sst_drv_ctx->streams[stream_id].ring_buffer_addr =
		    str_param->sparams.uc.pcm_params.ring_buffer_addr;
		sst_drv_ctx->streams[stream_id].pcm_wd_sz =
		    str_param->sparams.uc.pcm_params.pcm_wd_sz;
		sst_drv_ctx->streams[stream_id].num_chan =
		    str_param->sparams.uc.pcm_params.num_chan;
		sst_drv_ctx->streams[stream_id].sfreq =
		    str_param->sparams.uc.pcm_params.sfreq;
		sst_drv_ctx->streams[stream_id].device = str_param->device_type;
		format = SOC_AUDIO_MEDIA_FMT_PCM;
		break;

	case SST_CODEC_TYPE_MP3:
		sst_drv_ctx->streams[stream_id].pcm_wd_sz =
		    str_param->sparams.uc.mp3_params.pcm_wd_sz;
		sst_drv_ctx->streams[stream_id].num_chan =
		    str_param->sparams.uc.mp3_params.num_chan;
		sst_drv_ctx->streams[stream_id].sfreq =
		    str_param->sparams.uc.mp3_params.sfreq;
		sst_drv_ctx->streams[stream_id].device = str_param->device_type;
		format = SOC_AUDIO_MEDIA_FMT_MPEG;
		break;
	case SST_CODEC_TYPE_AAC:
		sst_drv_ctx->streams[stream_id].pcm_wd_sz =
		    str_param->sparams.uc.aac_params.pcm_wd_sz;
		sst_drv_ctx->streams[stream_id].num_chan =
		    str_param->sparams.uc.aac_params.num_chan;
		sst_drv_ctx->streams[stream_id].sfreq =
		    str_param->sparams.uc.aac_params.sfreq;
		sst_drv_ctx->streams[stream_id].device = str_param->device_type;
		/* TODO: more parameters for AAC? */
		format = SOC_AUDIO_MEDIA_FMT_AAC;
		break;
	default:
		pr_err("sst_set_stream_params : unsupported format =%d",
			 str_param->codec);
		format = SOC_AUDIO_MEDIA_FMT_INVALID;
		return format;

	}

	pr_debug("set_params- rate %d size %d ch_count %d for stream =%d\n",
	     sst_drv_ctx->streams[stream_id].sfreq,
	     sst_drv_ctx->streams[stream_id].pcm_wd_sz,
	     sst_drv_ctx->streams[stream_id].num_chan, stream_id);
	return format;

}

static enum soc_audio_ops_type audio_ops_type(enum snd_sst_stream_ops ops_type)
{
	enum soc_audio_ops_type type = PIPE_OPS_PLAYBACK;

	switch (ops_type) {
	case STREAM_OPS_PLAYBACK:
		type = PIPE_OPS_PLAYBACK;
		break;
	case STREAM_OPS_CAPTURE:
		type = PIPE_OPS_CAPTURE;
		break;
	case STREAM_OPS_PLAYBACK_DRM:
		type = PIPE_OPS_PLAYBACK_DRM;
		break;
	case STREAM_OPS_PLAYBACK_ALERT:
		type = PIPE_OPS_PLAYBACK_ALERT;
		break;
	case STREAM_OPS_CAPTURE_VOICE_CALL:
		type = PIPE_OPS_CAPTURE_VOICE_CALL;
		break;
	case STREAM_OPS_COMPRESSED_PATH:
		break;
	}

	return type;
}
/*
 * sst_get_stream_allocated - this function gets a stream allocated with
 * the given params
 *
 * @str_param : stream params
 * @lib_dnld : pointer to pointer of lib downlaod struct
 *
 * This creates new stream id for a stream, in case lib is to be downloaded to
 * DSP, it downloads that
 */
int sst_get_stream_allocated(struct snd_sst_params *str_param,
			     struct snd_sst_lib_download **lib_dnld,
			     unsigned int poll_mode)
{
	int result = 0;
	struct stream_info *stream;
	enum soc_audio_format format = 0;
	struct soc_audio_channel_mix_config ch_mix_config;
	uint8_t num_chan = str_param->sparams.uc.pcm_params.num_chan;

	pr_debug("sst_get_stream_allocated: %d\n", str_param->device_type);
	stream = &sst_drv_ctx->streams[str_param->device_type];

	if (stream->status != STREAM_UN_INIT) {
		pr_err("failed to get stream allocated: %d\n",
			str_param->device_type);
		result = -1;
		goto alloc_exit;
	}

	result = add_to_active_streams(str_param->device_type);
	if (result < 0) {
		pr_err("sst_get_stream_allocated failed\n");
		goto alloc_exit;
	}

	sst_init_stream(stream, str_param->codec,
		str_param->ops, 0, str_param->device_type);
	stream->poll_mode = poll_mode;

	result = soc_audio_input_add(sst_drv_ctx->proc_hdl,
		&stream->input_wl);
	if (result != SOC_SUCCESS) {
		pr_err("sst_add_input failed %d %d\n",
			str_param->device_type, result);
		result = -1;
		goto alloc_fail;
	}
	/* This is for multiple channel input. We need to set the mixer
	 * gain table to mix all channels into left and right channels.
	 */
	generate_ch_mix_config(&ch_mix_config,
		sst_drv_ctx->volume[str_param->device_type], num_chan);
	result = soc_audio_input_set_channel_mix(sst_drv_ctx->proc_hdl,
		stream->soc_input_id, &ch_mix_config);
	if (result != SOC_SUCCESS)
		pr_err("soc_audio_input_set_channel_mix failed!");

	soc_audio_input_set_as_primary(stream->input_wl,
		str_param->device_type, false);
	if (result != SOC_SUCCESS)
		pr_err("soc_audio_input_set_as_primary failed!");

	format = sst_set_stream_params(str_param, str_param->device_type);
	if (format == SOC_AUDIO_MEDIA_FMT_INVALID) {
		result = -ENOMEM;
		goto alloc_fail;
	}

	result = soc_audio_input_set_data_format(sst_drv_ctx->proc_hdl,
			stream->input_wl, audio_ops_type(str_param->ops),
			format);

	if (result != SOC_SUCCESS) {
		pr_err("sst_data_format failed %d %d\n", str_param->device_type,
			   result);
		result = -1;
		goto alloc_fail;
	}

	if (str_param->ops != STREAM_OPS_CAPTURE) {
		result = soc_audio_set_output_pcm_slot(sst_drv_ctx->proc_hdl,
				str_param->device_type,
				str_param->sparams.uc.pcm_params.num_chan);
		if (result != SOC_SUCCESS) {
			pr_err("soc_audio_set_pcm_slot failed\n");
			result = -1;
			goto alloc_fail;
		}
	}
#ifdef INCLUDE_DECODE_STAGE
	if (format == SOC_AUDIO_MEDIA_FMT_MPEG) {
		int mch_enable = false;
		soc_audio_input_set_decoder_param(sst_drv_ctx->proc_hdl,
			stream->input_wl, SOC_AUDIO_MPEG_PCM_WD_SZ,
			&(stream->pcm_wd_sz));
		soc_audio_input_set_decoder_param(sst_drv_ctx->proc_hdl,
			stream->input_wl, SOC_AUDIO_MPEG_MCH_ENABLE,
			&mch_enable);
	}
	if (format == SOC_AUDIO_MEDIA_FMT_AAC) {
		soc_audio_input_set_decoder_param(sst_drv_ctx->proc_hdl,
				stream->input_wl, SOC_AUDIO_AAC_PCM_WD_SZ,
				&(stream->pcm_wd_sz));
		soc_audio_input_set_decoder_param(sst_drv_ctx->proc_hdl,
				stream->input_wl, SOC_AUDIO_AAC_PCM_SAMPLE_RATE,
				&(stream->sfreq));
		/* Need add more if necessary */
	}
#endif

	result = str_param->device_type;
alloc_exit:
	return result;

alloc_fail:
	stream->status = STREAM_UN_INIT;
	delete_from_active_streams(str_param->device_type);
	return result;
}

/*
 * sst_get_stream - this function prepares for stream allocation
 *
 * @str_param : stream param
 */
int sst_get_stream(struct snd_sst_params *str_param, unsigned int poll_mode)
{
	int i = 0, retval = 0;
	struct snd_sst_lib_download *lib_dnld;
	int codec_change = 0;

	if (str_param->codec == SST_CODEC_TYPE_UNKNOWN)
		pr_err("get_stream called for unknown codec type");

	if ((sst_drv_ctx->codec != str_param->codec) &&
	    str_param->codec != SST_CODEC_TYPE_PCM)
		codec_change = 1;

	/* We download codec when the one already installed is not matching
	 * the one which the str_param want to get stream for
	 */
	if (codec_change) {
		pr_debug("Downloading codec lib as codec now is = %d",
			 sst_drv_ctx->streams[i].codec);
		retval = sst_download_codec(str_param->codec, str_param->ops);

		if (retval) {
			pr_err("Codec download fail %x, abort\n", retval);
			return retval;
		}
		sst_drv_ctx->codec = str_param->codec;
		pr_debug("Codec downloaded successfully for codec =%d !",
		       sst_drv_ctx->codec);
	}

	/* stream is not allocated, we are allocating */
	retval = sst_get_stream_allocated(str_param, &lib_dnld, poll_mode);

	if (str_param->ops == STREAM_OPS_PLAYBACK ||
			str_param->ops == STREAM_OPS_PLAYBACK_DRM) {
		sst_drv_ctx->pb_streams++;
	} else if (str_param->ops == STREAM_OPS_CAPTURE) {
		sst_drv_ctx->cp_streams++;
	}

	return retval;
}

int sst_prepare_fw(void)
{
	int retval = 0;

	/* FW is not downloaded */
	pr_debug("DSP Downloading FW now...\n");
	retval = sst_download_fw();
	if (retval) {
		pr_err("FW download fail %x\n", retval);
		pr_debug("doing rtpm_put\n");
		sst_pm_runtime_put(&sst_drv_ctx->pci->dev);
	}
	return retval;
}

void sst_process_mad_ops(struct work_struct *work)
{

	struct mad_ops_wq *mad_ops = container_of(work, struct mad_ops_wq, wq);
	int str_id = mad_ops->stream_id;
	enum soc_dev_state state;
	enum soc_result result;

	mutex_lock(&sst_drv_ctx->streams[str_id].lock);
	pr_debug("sst_process_mad_ops: ops= %d", mad_ops->control_op);
	switch (mad_ops->control_op) {
	case SST_SND_PAUSE:
		pr_debug("SST Debug: in mad_ops pause stream\n");
		state = SOC_DEV_STATE_PAUSE;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
						   sst_drv_ctx->
						   streams[str_id].input_wl,
						   state);

		if (result != SOC_SUCCESS)
			pr_err("SST_SND_PAUSE: pause failed %d\n", result);

		break;
	case SST_SND_RESUME:
		pr_debug("SST Debug: in mad_ops resume stream\n");
		state = SOC_DEV_STATE_ENABLE;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
						   sst_drv_ctx->
						   streams[str_id].input_wl,
						   state);

		if (result != SOC_SUCCESS)
			pr_err("SST_SND_RESUME: resume failed\n");

		break;
	case SST_SND_DROP:
		pr_debug("SST Debug: in mad_ops drop stream\n");
		state = SOC_DEV_STATE_STOP;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
						   sst_drv_ctx->
						   streams[str_id].input_wl,
						   state);

		if (result != SOC_SUCCESS)
			pr_err("SST_SND_DROP: stop failed\n");
		break;
	case SST_SND_START:
		state = SOC_DEV_STATE_PLAY;
		pr_debug("mad_ops start stream, str_id = %d, ops: %d\n",
			 str_id, sst_drv_ctx->streams[str_id].ops);
		result =
		    soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
					      sst_drv_ctx->
					      streams[str_id].input_wl, state);

		if (result != SOC_SUCCESS)
			pr_err("start failed\n");
		break;
	case SST_SND_DEVICE_RESUME:
	case SST_SND_DEVICE_RESUME_SYNC:
		pr_debug("SST_SND_DEVICE_RESUME / _SYNC, doing rtpm_get\n");
		sst_pm_runtime_get_sync(&sst_drv_ctx->pci->dev);
		sst_prepare_fw();
		break;
	case SST_SND_DEVICE_SUSPEND:
		pr_debug("SST_SND_DEVICE_SUSPEND doing rtpm_put\n");
		sst_pm_runtime_put(&sst_drv_ctx->pci->dev);
		break;
	default:
		pr_err("wrong control_ops reported\n");
	}
	mutex_unlock(&sst_drv_ctx->streams[str_id].lock);
	kfree(mad_ops);
	return;
}

static void sst_fill_compressed_slot(unsigned int device_type)
{
	if (device_type == SND_SST_DEVICE_HEADSET)
		sst_drv_ctx->compressed_slot = 0x03;
	else if (device_type == SND_SST_DEVICE_IHF)
		sst_drv_ctx->compressed_slot = 0x0C;

	pr_debug("sst_fill_compressed_slot : compressed_slot =%d",
		 sst_drv_ctx->compressed_slot);
}


/*
 * sst_open_pcm_stream - Open PCM interface
 *
 * @str_param: parameters of pcm stream
 *
 * This function is called by MID sound card driver to open
 * a new pcm interface
 */
static int sst_open_pcm_stream(struct snd_sst_params *str_param)
{
	struct stream_info *str_info;
	int retval;

	pr_debug("open_pcm, doing rtpm_get, usage count =%d\n",
		 atomic_read(&((&sst_drv_ctx->pci->dev)->power.usage_count)));

	sst_pm_runtime_get_sync(&sst_drv_ctx->pci->dev);
	sst_prepare_fw();
	if (!str_param) {
		pr_debug("sst_open_pcm_stream, doing rtpm_put\n");
		sst_pm_runtime_put(&sst_drv_ctx->pci->dev);
		return -EINVAL;
	}

	if (str_param->ops == STREAM_OPS_COMPRESSED_PATH) {
		sst_fill_compressed_slot(str_param->device_type);
		/* We need to update the pcm slot so that the
		 * right tdm_slot is selected for the new device*/
		retval = soc_audio_set_output_pcm_slot(sst_drv_ctx->proc_hdl,
					      SND_SST_DEVICE_COMPRESSED_PLAY,
					      str_param->sparams.uc.
					      pcm_params.num_chan);
		if (retval != SOC_SUCCESS) {
			pr_err("soc_audio_set_pcm_slot failed\n");
			return retval;
		} else {
			/* 0xFF as we want to close this stream later on */
			return 0xFF;
		}
	}

	/* Flush the workqueue before we open the stream. */
	flush_workqueue(sst_drv_ctx->mad_wq);
	mutex_lock(&sst_drv_ctx->stream_lock);
	retval = sst_get_stream(str_param, 0);
	if (retval > 0) {
		sst_drv_ctx->stream_cnt++;

		str_info = &sst_drv_ctx->streams[retval];
		str_info->src = MAD_DRV;
	}
	mutex_unlock(&sst_drv_ctx->stream_lock);
	return retval;
}

/*
 * sst_device_control - Set Control params
 *
 * @cmd: control cmd to be set
 * @arg: command argument
 *
 * This function is called by MID sound card driver to set
 * SST/Sound card controls for an opened stream.
 * This is registered with MID driver
 */
static int sst_device_control(int cmd, void *arg)
{
	int retval = 0;

	switch (cmd) {
	case SST_SND_DROP:
	case SST_SND_PAUSE:
	case SST_SND_RESUME:
	case SST_SND_START:
	case SST_SND_DEVICE_RESUME:
	case SST_SND_DEVICE_RESUME_SYNC:
	case SST_SND_DEVICE_SUSPEND: {
			struct mad_ops_wq *work =
			    kzalloc(sizeof(*work), GFP_ATOMIC);
			if (work == NULL)
				return -ENOMEM;
			INIT_WORK(&work->wq, sst_process_mad_ops);
			work->control_op = cmd;
			work->stream_id = *(int *)arg;
			queue_work(sst_drv_ctx->mad_wq, &work->wq);
			break;
		}
	case SST_SND_STREAM_INIT: {
			struct pcm_stream_info *str_info;
			struct stream_info *stream;

			pr_debug("stream init called\n");
			str_info = (struct pcm_stream_info *)arg;
			retval = sst_validate_strid(str_info->str_id);
			if (retval)
				break;

			stream = &sst_drv_ctx->streams[str_info->str_id];
			pr_debug("setting the period ptrs\n");
			stream->pcm_substream = str_info->mad_substream;
			stream->period_elapsed = str_info->period_elapsed;
			/*
			stream->sfreq = str_info->sfreq;
			 */
			stream->status = STREAM_INIT;
			break;
		}
	case SST_SND_BUFFER_POINTER:{
			struct pcm_stream_info *stream_info;
			struct ipc_ia_time_stamp_t __iomem *fw_tstamp;
			struct stream_info *stream;

			stream_info = (struct pcm_stream_info *)arg;
			retval = sst_validate_strid(stream_info->str_id);
			if (retval)
				break;

			stream = &sst_drv_ctx->streams[stream_info->str_id];

			if (!stream->pcm_substream) {
				pr_err("not a pcm sub stream\n");
				break;
			}

			fw_tstamp = (struct ipc_ia_time_stamp_t *)
				(sst_drv_ctx->mailbox
				 + SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET);
			fw_tstamp += stream->soc_input_id;

			if (stream->ops == STREAM_OPS_PLAYBACK)
				stream_info->buffer_ptr =
				    fw_tstamp->samples_rendered;
			else
				stream_info->buffer_ptr =
				    fw_tstamp->samples_processed;
			stream_info->pcm_delay = fw_tstamp->hw_pointer_delay;
			/* pr_debug("buffer ptr %llu pcm_delay %llu\n",
				 stream_info->buffer_ptr,
				 stream_info->pcm_delay); */
			break;
		}
	case SST_ENABLE_RX_TIME_SLOT:{
			int status = *(int *)arg;
			sst_drv_ctx->rx_time_slot_status = status;
			break;
		}
	default:
		/* Illegal case */
		pr_warn("illegal req\n");
		return -EINVAL;
	}

	return retval;
}

static struct intel_sst_pcm_control pcm_ops = {
	.open = sst_open_pcm_stream,
	.device_control = sst_device_control,
	.close = sst_close_pcm_stream,
};

static struct intel_sst_card_ops sst_pmic_ops = {
	.pcm_control = &pcm_ops,
};

/*
 * register_sst_card - function for sound card to register
 *
 * @card: pointer to structure of operations
 *
 * This function is called card driver loads and is ready for registration
 */
int register_sst_card(struct intel_sst_card_ops *card)
{
	pr_debug("driver register card %p\n", sst_drv_ctx);
	if (!sst_drv_ctx) {
		pr_err("No SST driver register card reject\n");
		return -ENODEV;
	}

	if (!card) {
		pr_err("Null Pointer Passed\n");
		return -EINVAL;
	}
	sst_drv_ctx->pmic_vendor = card->vendor_id;
	sst_drv_ctx->rx_time_slot_status = 0; /*default AMIC*/
	card->pcm_control = sst_pmic_ops.pcm_control;
	return 0;
}
EXPORT_SYMBOL_GPL(register_sst_card);

/*
 * unregister_sst_card- function for sound card to un-register
 *
 * @card: pointer to structure of operations
 *
 * This function is called when card driver unloads
 */
void unregister_sst_card(struct intel_sst_card_ops *card)
{
	return;
}
EXPORT_SYMBOL_GPL(unregister_sst_card);

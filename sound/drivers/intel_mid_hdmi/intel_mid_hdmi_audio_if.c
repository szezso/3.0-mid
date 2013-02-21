/*
 *   intel_mid_hdmi_audio_if.c - Intel HDMI audio driver for MID
 *
 *  Copyright (C) 2010 Intel Corp
 *  Authors:	Sailaja Bandarupalli <sailaja.bandarupalli@intel.com>
 *		Ramesh Babu K V <ramesh.babu@intel.com>
 *		Vaibhav Agarwal <vaibhav.agarwal@intel.com>
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
 * ALSA driver for Intel MID HDMI audio controller.  This file contains
 * interface functions exposed to HDMI Display driver and code to register
 * with ALSA framework..
 */

#define pr_fmt(fmt)		"had: " fmt

#include <linux/jiffies.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include "intel_mid_hdmi_audio.h"

static int flag_en_allbufs;

/**
 * hdmi_audio_suspend - power management suspend function
 *
 *@haddata: pointer to HAD private data
 *@event: pm event for which this method is invoked
 *
 * This function is called by client driver to suspend the
 * hdmi audio.
 */
int hdmi_audio_suspend(void *haddata, pm_event_t event)
{
	int retval = 0;
	struct snd_intelhad *intelhaddata = (struct snd_intelhad *)haddata;
	pr_debug("Enter:%s", __func__);

	if (intelhaddata->playback_cnt > 0) {
		pr_err("audio stream is active\n");
		return -EBUSY;
	}

	mutex_lock(&intelhaddata->had_lock);
	if (intelhaddata->drv_status == HAD_DRV_DISCONNECTED) {
		pr_debug("had not connected\n");
		mutex_unlock(&intelhaddata->had_lock);
		return retval;
	}

	intelhaddata->drv_status = HAD_DRV_SUSPENDED;
	retval = snd_intelhad_stop_silence(intelhaddata);
	mutex_unlock(&intelhaddata->had_lock);
	return retval;
}

/**
 * hdmi_audio_resume - power management resume function
 *
 *@haddata: pointer to HAD private data
 *
 * This function is called by client driver to resume the
 * hdmi audio.
 */
int hdmi_audio_resume(void *haddata)
{
	int caps, retval = 0;
	struct snd_intelhad *intelhaddata = (struct snd_intelhad *)haddata;
	pr_debug("Enter:%s", __func__);

	mutex_lock(&intelhaddata->had_lock);
	if (HAD_DRV_DISCONNECTED == intelhaddata->drv_status) {
		pr_debug("had not connected\n");
		mutex_unlock(&intelhaddata->had_lock);
		return 0;
	}

	if (HAD_DRV_SUSPENDED != intelhaddata->drv_status) {
		pr_err("had is not in suspended state\n");
		mutex_unlock(&intelhaddata->had_lock);
		return 0;
	}

	if (SNDRV_DEFAULT_IDX1 ==  intelhaddata->card_index)
		intelhaddata->drv_status = HAD_DRV_DISCONNECTED;
	else {
		intelhaddata->drv_status = HAD_DRV_CONNECTED;
		caps = HDMI_AUDIO_UNDERRUN | HDMI_AUDIO_BUFFER_DONE;
		retval = intelhaddata->query_ops.hdmi_audio_set_caps(
				HAD_SET_ENABLE_AUDIO_INT, &caps);
		retval = intelhaddata->query_ops.hdmi_audio_set_caps(
				HAD_SET_ENABLE_AUDIO, NULL);
		retval = snd_intelhad_configure_silence(intelhaddata);
		retval = snd_intelhad_start_silence(intelhaddata);
	}
	mutex_unlock(&intelhaddata->had_lock);
	return retval;
}

/**
 * had_event_handler - Call back function to handle events
 *
 * @event_type: Event type to handle
 * @data: data related to the event_type
 *
 * This function is invoked to handle HDMI events from client driver.
 */
int had_event_handler(enum had_event_type event_type, void *data)
{
	int caps, i = 0, retval = 0;
	struct snd_intelhad *intelhaddata = data;
	enum intel_had_aud_buf_type buf_id;
	struct pcm_stream_info *stream;
	u32 buf_size;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	struct hdmi_audio_registers_ops reg_ops;
	struct hdmi_audio_query_set_ops query_ops;
	u32 hdmi_status;
	u32 msecs, rate, channels;

	substream = intelhaddata->stream_info.had_substream;
	buf_id = intelhaddata->curr_buf;
	reg_ops = intelhaddata->reg_ops;
	query_ops = intelhaddata->query_ops;
	switch (event_type) {
	case HAD_EVENT_AUDIO_BUFFER_DONE:
		stream = &intelhaddata->stream_info;
		pr_debug("called _event_handler with BUFFER DONE event for"
							"\tBuf#%d\n", buf_id);
		buf_size =  intelhaddata->buf_info[buf_id].buf_size;
		if (intelhaddata->flag_start) {
			intelhaddata->stream_info.buffer_rendered += buf_size;
			stream->period_elapsed(stream->had_substream);
		}

		intelhaddata->buf_info[buf_id].is_valid = true;
		if (intelhaddata->valid_buf_cnt-1 == buf_id) {
			if (intelhaddata->flag_start)
				intelhaddata->curr_buf = HAD_BUF_TYPE_A;
			else	/* Use only silence buffers C & D */
				intelhaddata->curr_buf = HAD_BUF_TYPE_C;
		} else
			intelhaddata->curr_buf++;

		if (intelhaddata->send_data)
			return retval;

		if (flag_en_allbufs &&
				intelhaddata->curr_buf == HAD_BUF_TYPE_A) {
			pr_debug("special case:enable all bufs\n");
			for (i = 0; i < intelhaddata->valid_buf_cnt; i++) {
				pr_debug("Enabling buf[%d]\n", i);
				reg_ops.hdmi_audio_write_register(
					AUD_BUF_A_LENGTH +
					(i * HAD_REG_WIDTH), buf_size);
				reg_ops.hdmi_audio_write_register(
				AUD_BUF_A_ADDR+(i * HAD_REG_WIDTH),
				intelhaddata->buf_info[i].buf_addr |
				BIT(0) | BIT(1));
			}
			flag_en_allbufs = 0;
		} else {
			/*Reprogram the registers with addr and length*/
			reg_ops.hdmi_audio_write_register(
					AUD_BUF_A_LENGTH +
					(buf_id * HAD_REG_WIDTH), buf_size);
			reg_ops.hdmi_audio_write_register(
					AUD_BUF_A_ADDR+(buf_id * HAD_REG_WIDTH),
					intelhaddata->buf_info[buf_id].buf_addr|
					BIT(0) | BIT(1));
		}
	break;

	case HAD_EVENT_AUDIO_BUFFER_UNDERRUN:
		stream = &intelhaddata->stream_info;
		pr_debug("called _event_handler with _UNDERRUN event for"
							"\tBuf #%d\n", buf_id);
		if (intelhaddata->flag_start) {
			/* Report UNDERRUN error to above layers */
			intelhaddata->flag_underrun = 1;
			stream->period_elapsed(stream->had_substream);
		} else
			pr_debug("_UNDERRUN occured during _STOP\n");

		/* Handle Underrun interrupt within Audio Unit */
		reg_ops.hdmi_audio_write_register(
				AUD_CONFIG, 0);
		/**
		  * FIXME: shouldn't use while loop here. However, the interrupt
		  * status 'sticky' bits might not be cleared by setting '1' to
		  * that bit once...
		  */
		do { /* clear bit30, 31 AUD_HDMI_STATUS */
			reg_ops.hdmi_audio_read_register(
					AUD_HDMI_STATUS, &hdmi_status);
			pr_debug("HDMI status =0x%x\n", hdmi_status);
			if (hdmi_status & AUD_CONFIG_MASK_UNDERRUN) {
				i++;
				hdmi_status &= (AUD_CONFIG_MASK_SRDBG |
						AUD_CONFIG_MASK_FUNCRST);
				hdmi_status |= ~AUD_CONFIG_MASK_UNDERRUN;
				reg_ops.hdmi_audio_write_register(
						AUD_HDMI_STATUS, hdmi_status);
			} else
				break;
		} while (i < MAX_CNT);
		if (i >= MAX_CNT)
			pr_err("Unable to clear UNDERRUN bits\n");
	break;

	case HAD_EVENT_HOT_PLUG:
		pr_err("called _event_handler with _HOT_PLUG event\n");
		mutex_lock(&intelhaddata->had_lock);
		intelhaddata->drv_status = HAD_DRV_CONNECTED;
		buf_size =  intelhaddata->buf_info[buf_id].buf_size;
		buf_id = intelhaddata->curr_buf;
		intelhaddata->send_data = 0;
		caps = HDMI_AUDIO_UNDERRUN | HDMI_AUDIO_BUFFER_DONE;
		retval = query_ops.hdmi_audio_set_caps(
				HAD_SET_ENABLE_AUDIO_INT, &caps);
		retval = query_ops.hdmi_audio_set_caps(
				HAD_SET_ENABLE_AUDIO, NULL);

		if (!intelhaddata->stream_info.str_id) {
			retval = snd_intelhad_configure_silence(intelhaddata);
			retval = snd_intelhad_start_silence(intelhaddata);
			mutex_unlock(&intelhaddata->had_lock);
			pr_err("nothing to do in hot plug\n");
			return retval;
		}

		mutex_unlock(&intelhaddata->had_lock);
		cancel_delayed_work(&intelhaddata->dummy_audio);

		/* Reset HW within Audio unit */
		reg_ops.hdmi_audio_write_register(
				AUD_CONFIG, 0);
		reg_ops.hdmi_audio_write_register(
				AUD_HDMI_STATUS, 1);
		reg_ops.hdmi_audio_write_register(
				AUD_HDMI_STATUS, 0);

		/*Reprogram the registers with addr and length*/
		for (i = buf_id; i < intelhaddata->valid_buf_cnt; i++) {
			pr_debug("Enabling buf[%d]\n", i);
			reg_ops.hdmi_audio_write_register(
					AUD_BUF_A_LENGTH +
					(buf_id * HAD_REG_WIDTH), buf_size);
			reg_ops.hdmi_audio_write_register(
					AUD_BUF_A_ADDR+(buf_id * HAD_REG_WIDTH),
					intelhaddata->buf_info[buf_id].buf_addr|
					BIT(0) | BIT(1));
		}
		if (substream) { /* continue transfer */
			snd_intelhad_init_audio_ctrl(substream,
					intelhaddata, 0);
			hdmi_audio_mode_change(substream);
		}
		flag_en_allbufs = 1;

	break;

	case HAD_EVENT_HOT_UNPLUG:
		pr_debug("called _event_handler with _HOT_UNPLUG event\n");
		mutex_lock(&intelhaddata->had_lock);
		if (intelhaddata->stream_info.str_id) {
			buf_size =  intelhaddata->buf_info[buf_id].buf_size;
			if (substream) { /* substream is active */
				runtime = substream->runtime;
				rate = runtime->rate;
				channels = runtime->channels;
				msecs = (buf_size*1000)/(rate*channels*4);
				intelhaddata->timer = msecs_to_jiffies(msecs);
				intelhaddata->send_data = 1;
				schedule_delayed_work(
						&intelhaddata->dummy_audio,
						intelhaddata->timer);
			}
		} else {
			caps = HDMI_AUDIO_UNDERRUN | HDMI_AUDIO_BUFFER_DONE;
			retval = query_ops.hdmi_audio_set_caps(
					HAD_SET_DISABLE_AUDIO_INT, &caps);
			retval = query_ops.hdmi_audio_set_caps(
					HAD_SET_DISABLE_AUDIO, NULL);
			retval = snd_intelhad_stop_silence(intelhaddata);
		}
		intelhaddata->drv_status = HAD_DRV_DISCONNECTED;
		mutex_unlock(&intelhaddata->had_lock);
	break;

	case HAD_EVENT_MODE_CHANGING:
		pr_debug(" called _event_handler with _MODE_CHANGE event\n");
		mutex_lock(&intelhaddata->had_lock);
		if (intelhaddata->stream_info.str_id && substream)
			retval = hdmi_audio_mode_change(substream);
		mutex_unlock(&intelhaddata->had_lock);
	break;

	default:
		pr_debug("error un-handled event !!\n");
		retval = -EINVAL;
	break;

	}
	return retval;
}

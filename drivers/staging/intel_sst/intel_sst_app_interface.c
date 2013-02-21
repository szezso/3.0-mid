/*
 *  intel_sst_interface.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
 *  Authors:	Lomesh Agarwal <lomesh.agarwal@intel.com>
 *  Anurag Kansal <anurag.kansal@intel.com>
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

#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/aio.h>
#include <linux/pm_runtime.h>
#include <linux/ioctl.h>
#include <linux/poll.h>
#include "intel_sst.h"
#include "intel_sst_ioctl.h"
#include "intel_sst_common.h"
#include "soc_audio_api.h"
#include "soc_audio_bu_config.h"
#include "soc_audio_pipeline_defs.h"
#include "soc_audio_pipeline_specific.h"

/**
 * intel_sst_open - opens a handle to driver
 *
 * @i_node:	inode structure
 * @file_ptr:pointer to file
 *
 * This function is called by OS when a user space component
 * tries to get a driver handle. Only one handle at a time
 * will be allowed
 */
int intel_sst_open(struct inode *i_node, struct file *file_ptr)
{
	unsigned int retval = 0;

	pr_debug("sst open called\n");
	mutex_lock(&sst_drv_ctx->stream_lock);
	if (sst_drv_ctx->encoded_cnt < MAX_ENC_STREAM) {
		struct ioctl_pvt_data *data =
			kzalloc(sizeof(struct ioctl_pvt_data), GFP_KERNEL);
		if (!data) {
			mutex_unlock(&sst_drv_ctx->stream_lock);
			return -ENOMEM;
		}
		sst_drv_ctx->encoded_cnt++;
		sst_drv_ctx->stream_cnt++;
		data->str_id = 0;
		if (file_ptr->f_flags & O_NONBLOCK)
			pr_debug("poll mode set\n");
		file_ptr->private_data = (void *)data;
	} else
		retval = -EUSERS;
	mutex_unlock(&sst_drv_ctx->stream_lock);

	return retval;
}

/**
 * intel_sst_open_cntrl - opens a handle to driver
 *
 * @i_node:	inode structure
 * @file_ptr:pointer to file
 *
 * This function is called by OS when a user space component
 * tries to get a driver handle to /dev/intel_sst_control.
 * Only one handle at a time will be allowed
 * This is for control operations only
 */
int intel_sst_open_cntrl(struct inode *i_node, struct file *file_ptr)
{
	unsigned int retval = 0;

	/* audio manager open */
	mutex_lock(&sst_drv_ctx->stream_lock);
	if (sst_drv_ctx->am_cnt < MAX_AM_HANDLES) {
		sst_drv_ctx->am_cnt++;
		pr_debug("AM handle opened...\n");
		file_ptr->private_data = NULL;
	} else
		retval = -EACCES;
	mutex_unlock(&sst_drv_ctx->stream_lock);

	return retval;
}

/**
 * intel_sst_release - releases a handle to driver
 *
 * @i_node:	inode structure
 * @file_ptr:	pointer to file
 *
 * This function is called by OS when a user space component
 * tries to release a driver handle.
 */
int intel_sst_release(struct inode *i_node, struct file *file_ptr)
{
	struct ioctl_pvt_data *data = file_ptr->private_data;
	struct stream_info *stream;
	enum soc_result result = SOC_FAILURE;

	pr_debug("sst release called, closing app handle\n");
	mutex_lock(&sst_drv_ctx->stream_lock);
	sst_drv_ctx->encoded_cnt--;
	sst_drv_ctx->stream_cnt--;
	if (!data->str_id)
		goto EXIT;

	stream = &sst_drv_ctx->streams[data->str_id];

	/*
	 * Disable asynchronous notifications for this device-file's descriptor.
	 */
	fasync_helper(-1, file_ptr, 0, &stream->async_queue);
	pr_debug("closed async notifications\n");

	result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
					   stream->input_wl,
					   SOC_DEV_STATE_STOP);
	if (result != SOC_SUCCESS)
		pr_err("intel_sst_release set stop failed\n");

	pr_debug("Sending the input_remove");
	free_stream_context(data->str_id);
	result = soc_audio_input_remove(sst_drv_ctx->proc_hdl,
			       sst_drv_ctx->
			       streams[data->str_id].input_wl);

	if (result != SOC_SUCCESS)
		pr_err("intel_sst_release input remove failed\n");

EXIT:
	mutex_unlock(&sst_drv_ctx->stream_lock);

	kfree(data);
	return result;
}

int intel_sst_release_cntrl(struct inode *i_node, struct file *file_ptr)
{
	/* audio manager close */
	mutex_lock(&sst_drv_ctx->stream_lock);
	sst_drv_ctx->am_cnt--;
	mutex_unlock(&sst_drv_ctx->stream_lock);
	pr_debug("AM handle closed\n");
	return 0;
}

/*sets user data buffers to play/capture*/
static int intel_sst_play_capture(struct stream_info *stream, int str_id)
{
	if (stream->status == STREAM_INIT) {
		/* stream is started */
		stream->status = STREAM_RUNNING;
	}

	if (stream->status == STREAM_RUNNING) {
		/* stream is started */
		if (stream->ops == STREAM_OPS_PLAYBACK ||
				stream->ops == STREAM_OPS_PLAYBACK_DRM) {
			if (sst_play_frame(str_id) < 0) {
				pr_err("play frames failed\n");
				return -EIO;
			}
		} else if (stream->ops == STREAM_OPS_CAPTURE) {
			if (sst_capture_frame(str_id) < 0) {
				pr_err("capture frames failed\n ");
				return -EIO;
			}
		}
	} else {
		pr_err("Stream isn't in started state %d\n", stream->status);
		return -EIO;
	}
	return 0;
}

/* fills kernel list with buffer addresses for SST DSP driver to process*/
static int snd_sst_fill_kernel_list(struct stream_info *stream,
			const struct iovec *iovec, unsigned long nr_segs,
			struct list_head *copy_to_list)
{
	struct sst_stream_bufs *stream_bufs;
	unsigned long index, data_not_copied, mmap_len;
	unsigned char __user *bufp;
	unsigned long size, copied_size;
	int retval = 0, add_to_list = 0;
	static int sent_offset;
	static unsigned long sent_index;

	stream_bufs = kzalloc(sizeof(*stream_bufs), GFP_KERNEL);
	if (!stream_bufs)
		return -ENOMEM;
	stream_bufs->addr = sst_drv_ctx->mmap_mem;
	/* Use different buffer to support multiple streams */
	mmap_len = sst_drv_ctx->mmap_len / MAX_ENC_STREAM;
	stream_bufs->addr = sst_drv_ctx->mmap_mem
	    + mmap_len *
		((stream->device == SND_SST_DEVICE_COMPRESSED_PLAY) ? 0 : 1);
	bufp = stream->cur_ptr;

	copied_size = 0;

	if (!stream->sg_index)
		sent_index = sent_offset = 0;

	for (index = stream->sg_index; index < nr_segs; index++) {
		stream->sg_index = index;
		if (!stream->cur_ptr)
			bufp = iovec[index].iov_base;

		size = ((unsigned long)iovec[index].iov_base
			+ iovec[index].iov_len) - (unsigned long) bufp;

		if ((copied_size + size) > mmap_len)
			size = mmap_len - copied_size;


		if (stream->ops == STREAM_OPS_PLAYBACK) {
			data_not_copied = copy_from_user(
				(void *)(stream_bufs->addr + copied_size),
				bufp, size);
			if (data_not_copied > 0) {
				/* Clean up the list and return error code */
				retval = -EFAULT;
				break;
			}
		} else if (stream->ops == STREAM_OPS_CAPTURE) {
			struct snd_sst_user_cap_list *entry =
				kzalloc(sizeof(*entry), GFP_KERNEL);

			if (!entry) {
				kfree(stream_bufs);
				return -ENOMEM;
			}
			entry->iov_index = index;
			entry->iov_offset = (unsigned long) bufp -
					(unsigned long)iovec[index].iov_base;
			entry->offset = copied_size;
			entry->size = size;
			list_add_tail(&entry->node, copy_to_list);
		}

		stream->cur_ptr = bufp + size;

		if (((unsigned long)iovec[index].iov_base
				+ iovec[index].iov_len) <
				((unsigned long)iovec[index].iov_base)) {
			pr_err("Buffer overflows");
			kfree(stream_bufs);
			return -EINVAL;
		}

		if (((unsigned long)iovec[index].iov_base
					+ iovec[index].iov_len) ==
					(unsigned long)stream->cur_ptr) {
			stream->cur_ptr = NULL;
			stream->sg_index++;
		}

		copied_size += size;
		pr_debug("copied_size - %lx\n", copied_size);
		if ((copied_size >= mmap_len) ||
				(stream->sg_index == nr_segs)) {
			add_to_list = 1;
		}

		if (add_to_list) {
			stream_bufs->in_use = false;
			stream_bufs->size = copied_size;
			list_add_tail(&stream_bufs->node, &stream->bufs);
			break;
		}
	}
	return retval;
}

/* This function copies the captured data returned from SST DSP engine
 * to the user buffers*/
static int snd_sst_copy_userbuf_capture(struct stream_info *stream,
			const struct iovec *iovec,
			struct list_head *copy_to_list)
{
	struct snd_sst_user_cap_list *entry, *_entry;
	struct sst_stream_bufs *kbufs = NULL, *_kbufs;
	int retval = 0;
	unsigned long data_not_copied;

	/* copy sent buffers */
	pr_debug("capture stream copying to user now...\n");
	list_for_each_entry_safe(kbufs, _kbufs, &stream->bufs, node) {
		if (kbufs->in_use == true) {
			/* copy to user */
			list_for_each_entry_safe(entry, _entry,
						copy_to_list, node) {
				data_not_copied = copy_to_user(
					iovec[entry->iov_index].iov_base +
					entry->iov_offset,
					kbufs->addr + entry->offset,
					entry->size);
				if (data_not_copied > 0) {
					/* Clean up the list and return error */
					retval = -EFAULT;
					break;
				}
				list_del(&entry->node);
				kfree(entry);
			}
		}
	}
	pr_debug("end of cap copy\n");
	return retval;
}

/*
 * snd_sst_userbufs_play_cap - constructs the list from user buffers
 *
 * @iovec:pointer to iovec structure
 * @nr_segs:number entries in the iovec structure
 * @str_id:stream id
 * @stream:pointer to stream_info structure
 *
 * This function will traverse the user list and copy the data to the kernel
 * space buffers.
 */
static int snd_sst_userbufs_play_cap(const struct iovec *iovec,
			unsigned long nr_segs, unsigned int str_id,
			struct stream_info *stream)
{
	int retval;
	LIST_HEAD(copy_to_list);


	retval = snd_sst_fill_kernel_list(stream, iovec, nr_segs,
		       &copy_to_list);
	if (retval < 0)
		return retval;

	retval = intel_sst_play_capture(stream, str_id);
	if (retval < 0)
		return retval;

	if (stream->ops == STREAM_OPS_CAPTURE) {
		retval = snd_sst_copy_userbuf_capture(stream, iovec,
				&copy_to_list);
	}
	return retval;
}

/* This function is common function across read/write
  for user buffers called from system calls*/
static int intel_sst_read_write(unsigned int str_id, char __user *buf,
					size_t count)
{
	int retval, result;
	struct stream_info *stream;
	struct iovec iovec;
	unsigned long nr_segs;

	retval = sst_validate_strid(str_id);
	if (retval)
		return -EINVAL;

	stream = &sst_drv_ctx->streams[str_id];

	if (!count)
		return -EINVAL;

	stream->curr_bytes = 0;
	stream->cumm_bytes = 0;
	/* copy user buf details */
	pr_debug("intel_sst_read_write - new buffers %p, copy size %d,"
		"status %d\n", buf, (int)count, (int)stream->status);

	iovec.iov_base = buf;
	iovec.iov_len  = count;
	nr_segs = 1;

	do {
		retval =
		    snd_sst_userbufs_play_cap(&iovec, nr_segs, str_id, stream);
		if (retval < 0)
			break;

	} while (stream->sg_index < nr_segs);

	stream->sg_index = 0;
	stream->cur_ptr = NULL;
	if (retval >= 0)
		retval = stream->cumm_bytes;

	result = audio_set_dma_registers(stream->input_wl, stream);

	if (SOC_SUCCESS != result) {
		pr_err("Failed to set the dma registers, retval =%d",
		       retval);
	}
	retval = stream->cumm_bytes;

	pr_debug("end of play/rec bytes = %d!!\n", retval);
	return retval;
}

/***
 * intel_sst_write - This function is called when user tries to play out data
 *
 * @file_ptr:pointer to file
 * @buf:user buffer to be played out
 * @count:size of tthe buffer
 * @offset:offset to start from
 *
 * writes the encoded data into DSP
 */
int intel_sst_write(struct file *file_ptr, const char __user *buf,
			size_t count, loff_t *offset)
{
	struct ioctl_pvt_data *data = file_ptr->private_data;
	int str_id = data->str_id;
	struct stream_info *stream;
	int retval = 0;

	stream = &sst_drv_ctx->streams[str_id];
	mutex_lock(&stream->lock);

	pr_debug("write called for %d with num_bytes =%d\n", str_id,
		 count);
	if (stream->status == STREAM_UN_INIT) {
		retval = -EBADRQC;
		goto EXIT;
	}

	retval = intel_sst_read_write(str_id, (char __user *)buf, count);
EXIT:
	mutex_unlock(&stream->lock);
	return retval;
}

/*
 * intel_sst_aio_write - write buffers
 *
 * @kiocb:pointer to a structure containing file pointer
 * @iov:list of user buffer to be played out
 * @nr_segs:number of entries
 * @offset:offset to start from
 *
 * This function is called when user tries to play out multiple data buffers
 */
ssize_t intel_sst_aio_write(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr_segs, loff_t  offset)
{
	int retval;
	struct ioctl_pvt_data *data = kiocb->ki_filp->private_data;
	int str_id = data->str_id;
	struct stream_info *stream;

	pr_debug("entry - %ld\n", nr_segs);

	if (is_sync_kiocb(kiocb) == false)
		return -EINVAL;

	pr_debug("called for str_id %d\n", str_id);
	retval = sst_validate_strid(str_id);
	if (retval)
		return -EINVAL;

	stream = &sst_drv_ctx->streams[str_id];
	mutex_lock(&stream->lock);

	if (stream->status == STREAM_UN_INIT) {
		retval = -EBADRQC;
		goto EXIT;
	}

	stream->curr_bytes = 0;
	stream->cumm_bytes = 0;
	pr_debug("new segs %ld, offset %d, status %d\n" ,
			nr_segs, (int) offset, (int) stream->status);
	do {
		retval = snd_sst_userbufs_play_cap(iov, nr_segs,
						str_id, stream);
		if (retval < 0)
			break;

	} while (stream->sg_index < nr_segs);

	stream->sg_index = 0;
	stream->cur_ptr = NULL;
	if (retval >= 0)
		retval = stream->cumm_bytes;

	retval = audio_set_dma_registers(stream->input_wl, stream);

	if (SOC_SUCCESS != retval) {
		pr_err("Failed to set the dma registers, retval =%d",
		       retval);
	}
EXIT:
	mutex_unlock(&stream->lock);

	return retval;
}

/*
 * intel_sst_read - read the encoded data
 *
 * @file_ptr: pointer to file
 * @buf: user buffer to be filled with captured data
 * @count: size of tthe buffer
 * @offset: offset to start from
 *
 * This function is called when user tries to capture data
 */
int intel_sst_read(struct file *file_ptr, char __user *buf,
			size_t count, loff_t *offset)
{
	struct ioctl_pvt_data *data = file_ptr->private_data;
	int str_id = data->str_id;
	struct stream_info *stream;
	int retval;

	stream = &sst_drv_ctx->streams[str_id];
	mutex_lock(&stream->lock);

	pr_debug("intel_sst_read called for %d\n", str_id);
	if (stream->status == STREAM_UN_INIT) {
		retval =  -EBADRQC;
		goto EXIT;
	}
	retval = intel_sst_read_write(str_id, buf, count);
EXIT:
	mutex_unlock(&stream->lock);
	return retval;
}

/*
 * intel_sst_aio_read - aio read
 *
 * @kiocb: pointer to a structure containing file pointer
 * @iov: list of user buffer to be filled with captured
 * @nr_segs: number of entries
 * @offset: offset to start from
 *
 * This function is called when user tries to capture out multiple data buffers
 */
ssize_t intel_sst_aio_read(struct kiocb *kiocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t offset)
{
	int retval;
	struct ioctl_pvt_data *data = kiocb->ki_filp->private_data;
	int str_id = data->str_id;
	struct stream_info *stream;

	pr_debug("entry - %ld\n", nr_segs);

	if (is_sync_kiocb(kiocb) == false) {
		pr_debug("aio_read from user space is not allowed\n");
		return -EINVAL;
	}

	pr_debug("called for str_id %d\n", str_id);
	retval = sst_validate_strid(str_id);
	if (retval)
		return -EINVAL;

	stream = &sst_drv_ctx->streams[str_id];
	mutex_lock(&stream->lock);

	if (stream->status == STREAM_UN_INIT) {
		retval = -EBADRQC;
		goto EXIT;
	}
	stream->curr_bytes = 0;
	stream->cumm_bytes = 0;

	pr_debug("new segs %ld, offset %d, status %d\n" ,
			nr_segs, (int) offset, (int) stream->status);
	do {
		retval = snd_sst_userbufs_play_cap(iov, nr_segs,
						str_id, stream);
		if (retval < 0)
			break;

	} while (stream->sg_index < nr_segs);

	stream->sg_index = 0;
	stream->cur_ptr = NULL;
	if (retval >= 0)
		retval = stream->cumm_bytes;
EXIT:
	mutex_unlock(&stream->lock);
	pr_debug("end of play/rec bytes = %d!!\n", retval);
	return retval;
}

unsigned int intel_sst_poll(struct file *file, poll_table * wait)
{
	struct ioctl_pvt_data *data = NULL;
	struct stream_info *str_info;
	int str_id = 0, retval = 0;

	pr_debug("intel_sst_poll called\n");
	data = file->private_data;
	if (data)
		str_id = data->str_id;
	else
		return -ENXIO;

	retval = sst_validate_strid(str_id);
	if (retval)
		return -ENXIO;

	str_info = &sst_drv_ctx->streams[str_id];
	mutex_lock(&str_info->lock);

	pr_debug("intel_sst_poll for stream %d\n", str_id);
	/* check if stream is supported for poll */
	if (!str_info->poll_mode) {
		pr_err("poll mode not set in stream %d\n", str_id);
		retval = -POLLNVAL;
		goto EXIT;
	}

	pr_debug("polling for stream %d\n", str_id);

#if 0
	poll_wait(file, &str_info->sleep, wait);
#else
	wait_event(str_info->sleep, (str_info->flag != 0));
	str_info->flag = 0;
#endif

	pr_debug("polling done for stream %d\n", str_id);
	pr_debug("polling status =%d and  ops =%d\n", str_info->status,
		 str_info->ops);
	/* based on wake type, return the mask */
	/* TODO Check if you need to poll in some other state */
	if (str_info->status == STREAM_RUNNING) {
		if (str_info->ops == STREAM_OPS_PLAYBACK) {
			pr_debug("polling POLLOUT\n");
			retval = POLLOUT | POLLWRNORM;
			goto EXIT;
		} else if (str_info->ops == STREAM_OPS_CAPTURE) {
			pr_debug("polling POLLIN\n");
			retval = POLLIN | POLLWRNORM;
			goto EXIT;
		}
	}
	pr_debug("polling POLLERR\n");
EXIT:
	mutex_unlock(&str_info->lock);
	return retval;
}

/**
 * intel_sst_ioctl - recieves the device ioctl's
 * @file_ptr:pointer to file
 * @cmd:Ioctl cmd
 * @arg:data
 *
 * This function is called by OS when a user space component
 * sends an Ioctl to SST driver
 */
long intel_sst_ioctl(struct file *file_ptr, unsigned int cmd,
		     unsigned long arg)
{
	int retval = 0;
	struct ioctl_pvt_data *data;
	int str_id = 0;
	enum soc_dev_state state;
	enum soc_result result;
	struct stream_info *str_info = NULL;

	data = file_ptr->private_data;
	if (!data)
		return -EINVAL;

	str_id = data->str_id;
	if (!str_id && _IOC_NR(cmd) != _IOC_NR(SNDRV_SST_STREAM_SET_PARAMS))
		return -EINVAL;

	/* FIXME: what guarantees the str_id we get stays valid */
	if (str_id) {
		retval = sst_validate_strid(str_id);
		if (retval)
			return -EINVAL;

		str_info = &sst_drv_ctx->streams[str_id];
		mutex_lock(&str_info->lock);
	}

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_SST_STREAM_PAUSE):
		pr_debug("IOCTL_PAUSE recieved for %d!\n", str_id);
		state = SOC_DEV_STATE_STOP;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
					str_info->input_wl, state);

		if (result != SOC_SUCCESS) {
			pr_err("soc_audio_input_set_state() failed\n");
			retval = -EBADRQC;
		}
		break;

	case _IOC_NR(SNDRV_SST_STREAM_RESUME):
		pr_debug("SNDRV_SST_IOCTL_RESUME recieved!\n");
		state = SOC_DEV_STATE_PLAY;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
					str_info->input_wl, state);

		if (result != SOC_SUCCESS) {
			pr_err("soc_audio_input_set_state() failed\n");
			retval = -EBADRQC;
		}

		break;

	case _IOC_NR(SNDRV_SST_STREAM_SET_PARAMS):{
			struct snd_sst_params __user *usr_param =
				(struct snd_sst_params __user *)arg;
			struct snd_sst_params str_param;

			pr_debug("IOCTL_SET_PARAMS recieved!\n");
			if (str_id) {
				retval = -EINVAL;
				break;
			}
			retval = copy_from_user(&str_param,
						usr_param, sizeof(str_param));
			if (retval) {
				retval = -EFAULT;
				break;
			}
			retval = sst_get_stream(&str_param,
					file_ptr->f_flags & O_NONBLOCK ? 1 : 0);
			if (retval > 0) {
				str_id = retval;
				str_info = &sst_drv_ctx->streams[str_id];

				mutex_lock(&str_info->lock);
				data->str_id = retval;
				str_info->src = SST_DRV;
				retval = copy_to_user(&usr_param->stream_id,
							 &retval,
							 sizeof(__u32));
			} else
				retval = -EINVAL;
			break;
		}
	case _IOC_NR(SNDRV_SST_SET_VOL):{
			struct snd_sst_vol set_vol;
			struct snd_sst_vol __user *rec_vol =
				(struct snd_sst_vol __user *)arg;
			retval =
			    copy_from_user(&set_vol, rec_vol, sizeof(set_vol));
			if (retval) {
				retval = -EFAULT;
				break;
			}
			set_vol.stream_id = str_id;
			pr_debug("SET_VOLUME recieved for %d!\n",
				set_vol.stream_id);
			retval = sst_set_vol(&set_vol);
			if (retval)
				pr_err("sst_set_vol failed\n");
			break;
	    }
	case _IOC_NR(SNDRV_SST_MUTE):{
			struct snd_sst_mute set_mute;
			struct snd_sst_vol __user *rec_mute =
				(struct snd_sst_vol __user *)arg;
			retval = copy_from_user(&set_mute, rec_mute,
					   sizeof(set_mute));
			if (retval) {
				retval = -EFAULT;
				break;
			}
			set_mute.stream_id = str_id;
			pr_debug("SNDRV_SST_SET_MUTE recieved for %d!\n",
				set_mute.stream_id);
			retval = sst_set_mute(&set_mute);
			if (retval)
				pr_err("sst_set_mute failed\n");
			break;
		}

	case _IOC_NR(SNDRV_SST_STREAM_DROP):
		pr_debug("SNDRV_SST_IOCTL_DROP recieved!\n");
		state = SOC_DEV_STATE_STOP;
		result = soc_audio_input_set_state(sst_drv_ctx->proc_hdl,
				str_info->input_wl, state);

		if (result != SOC_SUCCESS) {
			pr_err("stop failed\n");
			retval = -EBADRQC;
		}
		break;
	case _IOC_NR(SNDRV_SST_STREAM_START):
		pr_debug("SNDRV_SST_STREAM_START recieved for"
			"device %d!\n", str_id);

		if (str_info->status == STREAM_INIT) {
			str_info->status = STREAM_RUNNING;
			if (str_info->ops == STREAM_OPS_PLAYBACK ||
			    str_info->ops == STREAM_OPS_PLAYBACK_DRM) {
				retval = sst_play_frame(str_id);
			} else if (str_info->ops == STREAM_OPS_CAPTURE)
				retval = sst_capture_frame(str_id);
			else {
				retval = -EINVAL;
				break;
			}
			if (retval < 0) {
				str_info->status = STREAM_INIT;
				break;
			}
		} else
			retval = -EINVAL;
		break;
	default:
		retval = -EINVAL;
	}

	if (str_id)
		mutex_unlock(&str_info->lock);

	return retval;
}

int intel_sst_fasync(int fd, struct file *filep, int mode)
{
	int retval = -EINVAL;
	struct ioctl_pvt_data *data = NULL;
	int str_id = 0;
	struct stream_info *str_info = NULL;

	pr_debug("fasync registered from user\n");
	data = filep->private_data;
	if (data)
		str_id = data->str_id;
	else
		goto EXIT;

	if (str_id) {
		if (sst_validate_strid(str_id))
			goto EXIT;

		str_info = &sst_drv_ctx->streams[str_id];
	} else
		goto EXIT;

	mutex_lock(&str_info->lock);
	retval = fasync_helper(fd, filep, mode, &str_info->async_queue);
	mutex_unlock(&str_info->lock);

EXIT:
	return retval;
}


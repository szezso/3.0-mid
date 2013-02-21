/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHANY WARRANTY; without even the implied warranty of
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
  A PARTICULAR PURPOSE ARE DISCLAIMED. NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING ANY WAY OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* Header Files */
#include <linux/list.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include "soc_audio_api.h"
#include "soc_audio_processor.h"
#include "soc_audio_pipeline.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_ipc.h"
#include "soc_debug.h"

/******************************************************************************/
/* IPC Private function declarations */
/******************************************************************************/
static enum soc_result ipc_create_msg(struct ipc_post **arg,
				      uint32_t size);
static void ipc_post_msg(struct work_struct *work);
static void ipc_msg_list_add_tail(struct ipc_post *msg);
static enum soc_result ipc_wait_reply(struct soc_audio_processor_context
				      *ctx);

/*
* this function is an wrapper function that sets the header before sending a
* message
*/
static inline void ipc_fill_header(uint32_t *header,
				   uint32_t msg,
				   uint32_t fw_handle,
				   uint32_t type,
				   uint32_t size,
				   uint32_t done, uint32_t busy)
{
	soc_ipc_set_msg_id(header, msg);
	soc_ipc_set_fw_handle(header, fw_handle);
	soc_ipc_set_type(header, type);
	soc_ipc_set_size(header, size);
	soc_ipc_set_done(header, done);
	soc_ipc_set_busy(header, busy);
}

/******************************************************************************/
/* IPC message function APIs */
/******************************************************************************/

/*
* soc_ipc_init_work_queue - Initialize the IPC work queue
* This function is called at the initialization time
*/
void soc_ipc_init_work_queue(struct soc_audio_init_data *init_data)
{
	/* Initialize the work queue for waiting IPC reply messages
	   which are processor independent */
	init_data->wq = create_workqueue("soc_audio_wq");
	/* coasi related initialization */
	INIT_WORK(&init_data->work, ipc_post_msg);
	/* Register the IPC function for processing all recevived IPC message */
	init_data->message_response = soc_ipc_process_reply;
}

/*
* soc_ipc_destroy_work_queue - Clear the IPC work queue
* This function is called at the shutdown time
*/
void soc_ipc_destroy_work_queue(struct soc_audio_init_data *init_data)
{
	/* Destroy the work queue */
	destroy_workqueue(init_data->wq);
	init_data->message_response = NULL;
}

/**
* soc_ipc_alloc_pipe - Send msg for a new pipeline ID
* This function is called by any function which wants to allocate
* a new pipe.
*/
enum soc_result
soc_ipc_alloc_pipe(struct soc_audio_processor_context *ctx,
		   struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_alloc_params params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_alloc_params));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_ALLOC_PIPE,
			INVALID_FW_HANDLE, pipeline->type,
			sizeof(struct ipc_alloc_params), false, true);

	params.reserved = 0;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params,
		     sizeof(struct ipc_alloc_params));

	ipc_msg_list_add_tail(msg);

	retval = ipc_wait_reply(ctx);
	if (SOC_SUCCESS != retval)
		goto EXIT;
	pipeline->fw_handle = ctx->ipc_block.data;
EXIT:
	SOC_EXIT();

	return retval;
}

/**
* soc_ipc_config_pipe - Send msg to FW. FW will update its pipeline information
* This function is called by any function which wants to config
* a new pipe.
*/
enum soc_result
soc_ipc_config_pipe(struct soc_audio_processor_context *ctx,
		    struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_config_params params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_config_params));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_CONFIG_PIPE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_config_params), false, true);
	params.reserved = 0;
	params.ctx = ctx;
	params.pipe = pipeline;
	memcpy(msg->mailbox_data, &params,
		     sizeof(struct ipc_config_params));

	ipc_msg_list_add_tail(msg);

	retval = ipc_wait_reply(ctx);
EXIT:
	SOC_EXIT();

	return retval;
}

/**
* soc_ipc_start_pipe - Send msg to start a  pipeline
* This function is called by any function which wants to start a new pipe.
*/
enum soc_result
soc_ipc_start_pipe(struct soc_audio_processor_context *ctx,
		   struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_pipe_start params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_pipe_start));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_START_PIPE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_pipe_start), false, true);
	params.reserved = 0;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params, sizeof(struct ipc_pipe_start));

	ipc_msg_list_add_tail(msg);

	retval = ipc_wait_reply(ctx);
	if (SOC_SUCCESS != retval)
		goto EXIT;
	pipeline->started = true;
EXIT:
	SOC_EXIT();

	return retval;
}

/*
* soc_ipc_free_pipe - Send msg to free pipe
* This function is called by any function which wants to free pipeline.
*/
enum soc_result
soc_ipc_free_pipe(struct soc_audio_processor_context *ctx,
		  struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_pipe_free params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_pipe_free));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_FREE_PIPE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_pipe_free), false, true);
	params.reserved = 0;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params, sizeof(struct ipc_pipe_free));

	ipc_msg_list_add_tail(msg);

	retval = ipc_wait_reply(ctx);
EXIT:
	SOC_EXIT();

	return retval;
}

enum soc_result
soc_ipc_send_fw_profile_dump(struct soc_audio_processor_context *ctx,
			     struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_fw_msg_profile_dump params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_fw_msg_profile_dump));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_FW_MESSAGE_PROFILE_DUMP,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_pipe_free), false, true);
	params.reserved = 0;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params,
		 sizeof(struct ipc_fw_msg_profile_dump));

	ipc_msg_list_add_tail(msg);
	queue_work(soc_audio_g_init_data.wq, &soc_audio_g_init_data.work);
EXIT:
	SOC_EXIT();

	return retval;
}

/**
* soc_ipc_stop_pipe - Send msg to stop pipe
* This function is called by any function which wants to stop pipeline.
*/
enum soc_result
soc_ipc_stop_pipe(struct soc_audio_processor_context *ctx,
		  struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_pipe_stop params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_pipe_stop));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_STOP_PIPE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_pipe_stop), false, true);
	params.reserved = 0;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params, sizeof(struct ipc_pipe_stop));

	ipc_msg_list_add_tail(msg);

	retval = ipc_wait_reply(ctx);
	if (SOC_SUCCESS != retval)
		goto EXIT;
	pipeline->started = false;
EXIT:
	/* There is a potential problem to do profiling dump in free pipe ipc.
	 * Right after free pipeline, we do run time power suspend/resume and
	 * inside we power-off the dsp. The profile dump fw messages may still
	 * going on and got cut-off half-way through. */
#ifdef _AUDIO_FW_PROFILING_
	soc_ipc_send_fw_profile_dump(ctx, pipeline);
#endif
	SOC_EXIT();

	return retval;
}

/**
* soc_ipc_flush_pipe - Send msg to flush pipe
* This function is called by any function which wants to flush pipeline.
*/
enum soc_result
soc_ipc_flush_pipe(struct soc_audio_processor_context *ctx,
		   struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_pipe_stop params;
	int i, num_retries = 10;

	SOC_ENTER();

	/* Add retry to confirm the flush complete including DMA interrupts
	 *  as interrupts could occur after the pipe flushed */
	for (i = 0; i < num_retries; i++) {
		soc_debug_print(DEBUG_LVL_3, "flush pipe, try %d\n", i);
		/* send msg to FW to allocate a pipe */
		retval = ipc_create_msg(&msg, sizeof(struct ipc_pipe_stop));
		if (SOC_SUCCESS != retval) {
			retval = SOC_ERROR_NO_RESOURCES;
			goto EXIT;
		}
		ipc_fill_header(&msg->header, SOC_IPC_IA_FLUSH_PIPE,
				pipeline->fw_handle, pipeline->type,
				sizeof(struct ipc_pipe_stop), false, true);
		params.reserved = 0;
		params.ctx = ctx;
		memcpy(msg->mailbox_data, &params,
				sizeof(struct ipc_pipe_stop));

		ipc_msg_list_add_tail(msg);

		retval = ipc_wait_reply(ctx);
		if (SOC_SUCCESS == retval)
			break;
		msleep_interruptible(1);
	}
EXIT:
	SOC_EXIT();

	return retval;
}

/**
* soc_ipc_stage_get_params
* - Send msg to get stream info of an paticular stage.
* This function is called by any function which wants to get stage's stream
* info.
*/
enum soc_result
soc_ipc_stage_get_params(struct soc_audio_processor_context *ctx,
			      struct soc_audio_pipeline *pipeline,
			      uint32_t stage_handle)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_get_stream_info params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_get_stream_info));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_GET_STREAM_INFO,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_get_stream_info), false, true);

	params.stage_handle = stage_handle;
	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params,
		     sizeof(struct ipc_get_stream_info));

	ipc_msg_list_add_tail(msg);
	retval = ipc_wait_reply(ctx);

EXIT:
	SOC_EXIT();
	return retval;
}

/**
* soc_ipc_stage_configure
* - Configure a stage on the DSP pipe.
*/
enum soc_result
soc_ipc_stage_configure(struct soc_audio_processor_context *ctx,
			struct soc_audio_pipeline *pipeline,
			uint32_t stage_handle)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_stage_configure params;

	SOC_ENTER();
	/* send msg to FW to allocate a pipe */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_stage_configure));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_STAGE_CONFIGURE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_stage_configure), false, true);

	params.stage_handle = stage_handle;
	params.ctx = ctx;
	memcpy(msg->mailbox_data,
		     &params, sizeof(struct ipc_stage_configure));

	ipc_msg_list_add_tail(msg);
	retval = ipc_wait_reply(ctx);

EXIT:
	SOC_EXIT();
	return retval;
}

/**
* soc_ipc_input_job_available
* - Send Job available to firmware to
* start the dma engine only on fresh data
*/
enum soc_result
soc_ipc_input_job_available(struct soc_audio_processor_context *ctx,
			    struct soc_audio_pipeline *pipeline)
{
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_input_job_available params;

	SOC_ENTER();
	/* send msg to FW to inform input_job_available */
	retval = ipc_create_msg(&msg, sizeof(struct ipc_input_job_available));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_INPUT_JOB_AVAILABLE,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_input_job_available), false, true);

	params.ctx = ctx;
	memcpy(msg->mailbox_data,
		     &params, sizeof(struct ipc_input_job_available));
	ipc_msg_list_add_tail(msg);
	queue_work(soc_audio_g_init_data.wq, &soc_audio_g_init_data.work);
EXIT:
	SOC_EXIT();
	return retval;
}

/**
* soc_ipc_switch_clock -
* * Notify FW to check if a clock switch is required
*/
enum soc_result
soc_ipc_switch_clock(struct soc_audio_processor_context *ctx,
					struct soc_audio_pipeline *pipeline)
{
#ifdef _ENABLE_CLOCK_SWITCHING_
	struct ipc_post *msg = NULL;
	enum soc_result retval = SOC_FAILURE;
	struct ipc_switch_clock params;

	SOC_ENTER();
	retval = ipc_create_msg(&msg, sizeof(struct ipc_switch_clock));
	if (SOC_SUCCESS != retval) {
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	ipc_fill_header(&msg->header, SOC_IPC_IA_SWITCH_CLOCK,
			pipeline->fw_handle, pipeline->type,
			sizeof(struct ipc_switch_clock), false, true);

	params.ctx = ctx;
	memcpy(msg->mailbox_data, &params,
		     sizeof(struct ipc_switch_clock));

	ipc_msg_list_add_tail(msg);
	queue_work(soc_audio_g_init_data.wq, &soc_audio_g_init_data.work);
EXIT:
	SOC_EXIT();

	return retval;
#else
	return SOC_SUCCESS;
#endif
}

/**
* soc_ipc_process_reply - process reply from bu specific interrupt handler
* This function is called by driver when firmware responds to message
*/
enum soc_result soc_ipc_process_reply(void *in_header, uint8_t * mailbox)
{
	enum soc_result result = SOC_SUCCESS;
	uint32_t *header = in_header;

	SOC_ENTER();

	switch (soc_ipc_get_msg_id(*header)) {
	case SOC_IPC_IA_ALLOC_PIPE_DONE:{
			struct ipc_alloc_params_response *rsp =
			    (struct ipc_alloc_params_response *)(mailbox);
			struct soc_audio_processor_context *ctx;
			ctx = (struct soc_audio_processor_context *)rsp->ctx;
			ctx->ipc_block.data = rsp->fw_id;
			ctx->ipc_block.result = rsp->result;
			result = rsp->result;
			ctx->ipc_block.condition = 1;
			wake_up(&ctx->wait_queue);
		}
		break;
	case SOC_IPC_IA_CONFIG_PIPE_DONE:
	case SOC_IPC_IA_START_PIPE_DONE:
	case SOC_IPC_IA_STOP_PIPE_DONE:
	case SOC_IPC_IA_FLUSH_PIPE_DONE:
	case SOC_IPC_IA_FREE_PIPE_DONE:
	case SOC_IPC_IA_STAGE_CONFIGURE_DONE:
	case SOC_IPC_IA_GET_STREAM_INFO_DONE:
	case SOC_IPC_IA_GET_STAGE_PARAMS_DONE:{
			struct ipc_pipe_response *rsp =
				(struct ipc_pipe_response *)mailbox;
			struct soc_audio_processor_context *ctx;
			ctx = (struct soc_audio_processor_context *)rsp->ctx;
			ctx->ipc_block.result = rsp->result;
			result = rsp->result;
			ctx->ipc_block.condition = 1;
			wake_up(&ctx->wait_queue);
		}
		break;
	case SOC_IPC_IA_INPUT_JOB_AVAILABLE_DONE:
	case SOC_IPC_IA_SWITCH_CLOCK_DONE:
	case SOC_IPC_IA_FW_MESSAGE_PROFILE_DUMP_DONE:{
			struct ipc_pipe_response *rsp =
				(struct ipc_pipe_response *)mailbox;
			result = rsp->result;
		}
		break;
	case SOC_IPC_IA_FW_INIT_CMPLT:
		soc_debug_print(ERROR_LVL, "FW built on %s\n", mailbox);
		soc_audio_g_init_data.condition = 1;
		wake_up(&soc_audio_g_init_data.wait_queue);
		break;
	case SOC_IPC_IA_MAILBOX_HAS_MSG:
		soc_debug_print(ERROR_LVL, "FW: %s\n", mailbox);
		break;
	case SOC_IPC_IA_NOTIFICATION_EVENT:
		soc_debug_print(ERROR_LVL, "Notification event %d\n",
			*((int *)mailbox));
		break;
	default:
		result = SOC_FAILURE;
		soc_debug_print(ERROR_LVL, "IPC replying unidentified "
			"message id: 0x%x\n", soc_ipc_get_msg_id(*header));
		break;
	}

	if (SOC_SUCCESS != result) {
		soc_debug_print(ERROR_LVL, "IPC process reply failed. "
			"message id: 0x%x\n", soc_ipc_get_msg_id(*header));
	}
	SOC_EXIT();
	return result;
}

/*
* soc_ipc_waittimeout - Wait till the firmware replies
* This function is called while waiting for fw download
*/
enum soc_result soc_ipc_waittimeout(struct soc_audio_init_data *init_data)
{
	enum soc_result retval = SOC_FAILURE;

	/* We are assuming only one control message will be pending
	 * for a processor at any given time since we have acquired the
	 * lock and are not releaseing it till timeout happens or firmware
	 * responds */
	SOC_ENTER();
	wait_event_timeout(init_data->wait_queue,
				   init_data->condition
				   ,  msecs_to_jiffies(SOC_IPC_TIMEOUT));
	if (init_data->condition) {
		/* event wake */
		soc_debug_print(DEBUG_LVL_3, "initial wait succeeded\n");
		/* We are good */
		retval = SOC_SUCCESS;
	} else {
		soc_debug_print(ERROR_LVL, "initial wait failed\n");
		retval = SOC_FAILURE;
	}
	/* reset the event for next time */
	init_data->condition = 0;

	SOC_EXIT();
	return retval;
}

/******************************************************************************/
/* IPC Private function definition */
/******************************************************************************/

/**
 * soc_ipc_post_message - Posts message to DSP
 * work:        Pointer to work structure
 *
 * This function is called in response to workqueue scheduled by any
 * component in driver which wants to send an IPC message to DSP.
 * */
static void ipc_post_msg(struct work_struct *work __attribute__ ((unused)))
{
	struct ipc_post *msg;
	enum soc_result result;

	SOC_ENTER();
	/* No ipc can issue unless the current ipc is consumed or failed. */
	mutex_lock(&soc_audio_g_init_data.list_lock);
	{
		/* check list */
		if (list_empty(
			&soc_audio_g_init_data.ipc_dispatch_list)) {
			/*In some cases post msg can be called even while
			 * list is empty. For example, flush msg list may
			 * tries to call this function and send out messages
			 * if possible.*/
			soc_debug_print(DEBUG_LVL_2,
					"IPC Dispatch List is Empty\n");
			goto EXIT;
		}

		/* copy msg from list */
		msg = list_entry(
				soc_audio_g_init_data.ipc_dispatch_list.next,
				struct ipc_post, node);
		/* Send message */
		result = soc_audio_ipc_send_message(msg);
		if (result != SOC_SUCCESS) {
			/* Resend later while we schedule the queue again */
			soc_debug_print(ERROR_LVL, "IPC sending failed\n");
		} else {
			/* clean up the message */
			list_del(&msg->node);
			kfree(msg->mailbox_data);
			kfree(msg);
		}
	}
EXIT:
	mutex_unlock(&soc_audio_g_init_data.list_lock);
	SOC_EXIT();
	return;
}

/*
* This function add the message to global ipc dispatch list
*/
static void ipc_msg_list_add_tail(struct ipc_post *msg)
{
	mutex_lock(&soc_audio_g_init_data.list_lock);
	list_add_tail(&msg->node,
	&soc_audio_g_init_data.ipc_dispatch_list);
	mutex_unlock(&soc_audio_g_init_data.list_lock);
}

/*
* Driver will Sleep until the corresponding IPC message been processed, or
* time.
*/
static enum soc_result ipc_wait_reply(struct soc_audio_processor_context
				      *ctx)
{
	enum soc_result retval = SOC_FAILURE;

	SOC_ENTER();
	ctx->ipc_block.condition = 0;
	queue_work(soc_audio_g_init_data.wq, &soc_audio_g_init_data.work);
	/* We are assuming only one control message will be pending
	 * for a processor at any gien time since we have acquired the
	 * lock and are not releaseing it till timeout happens or firmware
	 * responds */
	wait_event_timeout(ctx->wait_queue,
		ctx->ipc_block.condition,  msecs_to_jiffies(SOC_IPC_TIMEOUT));
	if (ctx->ipc_block.condition) {
		/* event wake */
		if (!ctx->ipc_block.result) {
			/* We are good */
			retval = SOC_SUCCESS;
		} else {
			soc_debug_print(ERROR_LVL, "IPC failed\n");
			retval = SOC_FAILURE;
		}
	} else {
		soc_debug_print(ERROR_LVL, "Wait timed-out\n");
		retval = SOC_FAILURE;
	}

	SOC_EXIT();
	return retval;
}

/* Allocate a ipc message with variable size of payload */
static enum soc_result ipc_create_msg(struct ipc_post **arg,
				      uint32_t size)
{
	struct ipc_post *msg;
	enum soc_result retval = SOC_SUCCESS;

	SOC_ENTER();
	/* Allocate the message */
	msg = kmalloc(sizeof(struct ipc_post), GFP_ATOMIC);
	if (!msg) {
		soc_debug_print(ERROR_LVL,
			"kmalloc msg failed\n");
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	/* Allocate the payload part of the message */
	if (size > SOC_AUDIO_DSP_MAILBOX_SIZE) {
		soc_debug_print(ERROR_LVL,
			"kmalloc msg size too large\n");
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	}
	msg->mailbox_data = kmalloc(size, GFP_ATOMIC);
	if (!msg->mailbox_data) {
		kfree(msg);
		soc_debug_print(ERROR_LVL,
			"kmalloc mailbox_data failed\n");
		retval = SOC_ERROR_NO_RESOURCES;
		goto EXIT;
	};
	*arg = msg;
EXIT:
	SOC_EXIT();
	return retval;
}

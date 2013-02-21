/*
 *  intel_sst_ipc.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
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
 *  This file defines all ipc functions
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/firmware.h>
#include "intel_sst.h"
#include "soc_audio_bu_config.h"
#include "intel_sst_common.h"
#include "soc_audio_api.h"
#include "soc_ipc.h"
#include "soc_audio_pipeline_specific.h"
/**
 * sst_post_message - Posts message to SST
 * @msg: IPC message to be posted
 *
 * This function is called by any component in driver which
 * wants to send an IPC message. This will post message only if
 * busy bit is free
 *
 * The caller must ensure that this is single threaded.
 */
enum soc_result sst_post_message(struct ipc_post *msg)
{
	uint32_t header;

	pr_debug("post message called, ID = %x\n",
		 soc_ipc_get_msg_id(msg->header));

	/* check busy bit */
	header = sst_shim_read(sst_drv_ctx->shim, SST_IPCX);
	if (soc_ipc_get_busy(header) || soc_ipc_get_done(header)) {
		/* busy, unmask */
		pr_err("Busy not free...\n");
		return SOC_ERROR_NO_RESOURCES;
	}
	if (soc_ipc_get_size(msg->header) > SOC_AUDIO_MAILBOX_SIZE_IPCX_SEND) {
		pr_err("mailbox data size is large %d\n",
		       soc_ipc_get_size(msg->header));
		return SOC_ERROR_INVALID_PARAMETER;
	}
	if (SOC_IPC_IA_CONFIG_PIPE == soc_ipc_get_msg_id(msg->header)) {
		struct audio_htod_pipe_conf_t conf_msg;
		struct ipc_config_params *ipc_config_msg;
		struct soc_audio_pipeline *pipe;
		/* For sending config message realign all the pointers */
		ipc_config_msg = (struct ipc_config_params *)
		&(msg->mailbox_data[0]);
		conf_msg.ctx = ipc_config_msg->ctx;
		pipe = ipc_config_msg->pipe;
		conf_msg.num_stages = pipe->num_stages;
		if (SOC_AUDIO_MAIN_PIPE == pipe->type) {
			/* Post processing pipeline: in SRAM */
			conf_msg.psm_stages =
				(struct soc_audio_pipeline_stage *)
				(SOC_AUDIO_DSP_MAILBOX_ADDR
				+ ((void *)&pipe->stages[0] -
				sst_drv_ctx->mailbox));
		} else {
			/* Other pipelines: in DSP DRAM */
			conf_msg.psm_stages =
			(struct soc_audio_pipeline_stage *)
			    (SOC_AUDIO_DSP_DRAM_ADDR +
			    SOC_AUDIO_DRAM_PIPELINE_OFFSET +
			    ((void *)&pipe->stages[0] -
			     (void *)dram_pipelines_shadow));
		}
		soc_ipc_set_size(&msg->header, sizeof(conf_msg));
		memcpy_toio(sst_drv_ctx->mailbox +
			    SOC_AUDIO_MAILBOX_SEND_OFFSET, &conf_msg,
			    soc_ipc_get_size(msg->header));

		soc_audio_config_preprocess(pipe, (void *)conf_msg.psm_stages);

		if (SOC_AUDIO_MAIN_PIPE != pipe->type) {
			pr_debug("sst_post_message: pipe_shadow"
					 "%p psm_stages %p\n",
					 dram_pipelines_shadow,
					 conf_msg.psm_stages);
			sst_pause_dsp();
			memcpy_toio(sst_drv_ctx->dram +
				    SOC_AUDIO_DRAM_PIPELINE_OFFSET +
				    ((void *)pipe -
				     (void *)dram_pipelines_shadow),
				    (void *)pipe, SOC_AUDIO_DEC_PIPE_SIZE);
			sst_resume_dsp();
		}
		sst_shim_write(sst_drv_ctx->shim, SST_IPCX, msg->header);

	} else {
		memcpy_toio(sst_drv_ctx->mailbox +
			    SOC_AUDIO_MAILBOX_SEND_OFFSET, msg->mailbox_data,
			    soc_ipc_get_size(msg->header));
		sst_shim_write(sst_drv_ctx->shim, SST_IPCX, msg->header);
	}

	return SOC_SUCCESS;
}

/*
 * sst_clear_interrupt - clear the SST FW interrupt
 *
 * This function clears the interrupt register after the interrupt
 * bottom half is complete allowing next interrupt to arrive
 */
void sst_clear_ipcx_interrupt(void)
{
	union interrupt_reg isr;
	union interrupt_reg imr;
	uint32_t clear_ipc;

	imr.full = sst_shim_read(sst_drv_ctx->shim, SST_IMRX);
	isr.full = sst_shim_read(sst_drv_ctx->shim, SST_ISRX);
	/*  write 1 to clear  */
	isr.part.ipcx = 1;
	sst_shim_write(sst_drv_ctx->shim, SST_ISRX, isr.full);
	/* Set IA done bit */
	clear_ipc = sst_shim_read(sst_drv_ctx->shim, SST_IPCX);
	soc_ipc_set_busy(&clear_ipc, 0);
	soc_ipc_set_done(&clear_ipc, 0);
	sst_shim_write(sst_drv_ctx->shim, SST_IPCX, clear_ipc);
	/* un mask busy interrupt */
	imr.full &= ~(0x3);
	sst_shim_write(sst_drv_ctx->shim, SST_IMRX, imr.full);
}

/*
 * sst_clear_interrupt - clear the SST FW interrupt
 *
 * This function clears the interrupt register after the interrupt
 * bottom half is complete allowing next interrupt to arrive
 */
void sst_clear_ipcd_interrupt(void)
{
	union interrupt_reg isr;
	union interrupt_reg imr;
	uint32_t clear_ipc;

	imr.full = sst_shim_read(sst_drv_ctx->shim, SST_IMRX);
	isr.full = sst_shim_read(sst_drv_ctx->shim, SST_ISRX);
	/*  write 1 to clear  */ ;
	isr.part.ipcd = 1;
	sst_shim_write(sst_drv_ctx->shim, SST_ISRX, isr.full);
	/* Set IA done bit */
	clear_ipc = sst_shim_read(sst_drv_ctx->shim, SST_IPCD);
	soc_ipc_set_busy(&clear_ipc, 0);
	/* There is no return message from IA to DSP
	 * In general all DSP to IA message are one way message */
	soc_ipc_set_done(&clear_ipc, 0);
	sst_shim_write(sst_drv_ctx->shim, SST_IPCD, clear_ipc);

	/* un mask busy interrupt */
	imr.full &= ~(0x3);
	sst_shim_write(sst_drv_ctx->shim, SST_IMRX, imr.full);
}
/**
* sst_process_message - Processes message from SST
*
* @work:	Pointer to work structure
*
* This function is scheduled by ISR
* It take a msg from process_queue and does action based on msg
*/
void sst_process_message(struct work_struct *work)
{
	uint32_t msg_id;
	enum soc_result result;
	struct sst_ipc_msg_wq *msg =
	    container_of(work, struct sst_ipc_msg_wq, wq);
	msg_id = soc_ipc_get_msg_id(msg->header);

	pr_debug("process msg for %x, ID = %x\n", msg->header, msg_id);

	result = soc_audio_g_init_data.message_response(
	&msg->header, msg->mailbox);
	sst_clear_ipcd_interrupt();
	return;
}

/**
* sst_process_reply - Processes reply message from SST
*
* @work:	Pointer to work structure
*
* This function is scheduled by ISR
* It take a reply msg from response_queue and
* does action based on msg
*/
void sst_process_reply(struct work_struct *work)
{
	uint32_t msg_id;
	enum soc_result result;
	struct sst_ipc_msg_wq *msg =
	    container_of(work, struct sst_ipc_msg_wq, wq);
	msg_id = soc_ipc_get_msg_id(msg->header);

	pr_debug("process reply for %x, ID = %x\n", msg->header, msg_id);

	result = soc_audio_g_init_data.message_response(
	&msg->header, msg->mailbox);

	sst_clear_ipcx_interrupt();
	/* Reply to IPCX indicates DSP can take next message */
	queue_work(soc_audio_g_init_data.wq, &soc_audio_g_init_data.work);
	return;
}

void sst_print_sram(void)
{
	int i, data[SOC_AUDIO_MAILBOX_SIZE_CHKPOINT/4] = { 0 };
	/* msleep(3); */
	pr_err("FW checkpoints\n");
	memcpy_fromio(data,
		      sst_drv_ctx->mailbox + SOC_AUDIO_MAILBOX_CHKPOINT_OFFSET,
		      SOC_AUDIO_MAILBOX_SIZE_CHKPOINT);
	for (i = 0; i < SOC_AUDIO_MAILBOX_SIZE_CHKPOINT/4;) {
		pr_err
		("[%02X] = %08X %08X %08X %08X %08X %08X %08X %08X\n",
			i, data[i], data[i + 1], data[i + 2], data[i + 3],
			data[i + 4], data[i + 5], data[i + 6], data[i + 7]);
		i += 8;
	}
	return;
}
EXPORT_SYMBOL_GPL(sst_print_sram);

void sst_reset_sram(void)
{
	memset_io(sst_drv_ctx->mailbox + SOC_AUDIO_MAILBOX_CHKPOINT_OFFSET,
		  0, SOC_AUDIO_MAILBOX_SIZE_CHKPOINT);
	return;
}
EXPORT_SYMBOL_GPL(sst_reset_sram);

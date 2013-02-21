/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#ifndef QUEUE_H
#define QUEUE_H


#if defined(__cplusplus)
extern "C" {
#endif

#define UPDATE_QUEUE_ROFF(psQueue, ui32Size)						\
	psQueue->ui32ReadOffset = (psQueue->ui32ReadOffset + ui32Size)	\
	& (psQueue->ui32QueueSize - 1);

 typedef struct _COMMAND_COMPLETE_DATA_
 {
	int			bInUse;

	u32			ui32DstSyncCount;
	u32			ui32SrcSyncCount;
	PVRSRV_SYNC_OBJECT	*psDstSync;
	PVRSRV_SYNC_OBJECT	*psSrcSync;
	u32			ui32AllocSize;
 }COMMAND_COMPLETE_DATA, *PCOMMAND_COMPLETE_DATA;

#if !defined(USE_CODE)
void QueueDumpDebugInfo(void);


PVRSRV_ERROR PVRSRVProcessQueues (u32	ui32CallerID,
								  int		bFlush);

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#include <linux/seq_file.h>
off_t
QueuePrintQueues (char * buffer, size_t size, off_t off);

#ifdef PVR_PROC_USE_SEQ_FILE
void* ProcSeqOff2ElementQueue(struct seq_file * sfile, loff_t off);
void ProcSeqShowQueue(struct seq_file *sfile,void* el);
#endif

#endif



PVRSRV_ERROR  PVRSRVCreateCommandQueueKM(u32 ui32QueueSize,
													 PVRSRV_QUEUE_INFO **ppsQueueInfo);

PVRSRV_ERROR  PVRSRVDestroyCommandQueueKM(PVRSRV_QUEUE_INFO *psQueueInfo);


PVRSRV_ERROR  PVRSRVInsertCommandKM(PVRSRV_QUEUE_INFO	*psQueue,
												PVRSRV_COMMAND		**ppsCommand,
												u32			ui32DevIndex,
												u16			CommandType,
												u32			ui32DstSyncCount,
												PVRSRV_KERNEL_SYNC_INFO	*apsDstSync[],
												u32			ui32SrcSyncCount,
												PVRSRV_KERNEL_SYNC_INFO	*apsSrcSync[],
												u32			ui32DataByteSize );


PVRSRV_ERROR  PVRSRVGetQueueSpaceKM(PVRSRV_QUEUE_INFO *psQueue,
												u32 ui32ParamSize,
												void **ppvSpace);


PVRSRV_ERROR  PVRSRVSubmitCommandKM(PVRSRV_QUEUE_INFO *psQueue,
												PVRSRV_COMMAND *psCommand);


void PVRSRVCommandCompleteKM(void * hCmdCookie, int bScheduleMISR);

void PVRSRVCommandCompleteCallbacks(void);


PVRSRV_ERROR PVRSRVRegisterCmdProcListKM(u32		ui32DevIndex,
										 PFN_CMD_PROC	*ppfnCmdProcList,
										 u32		ui32MaxSyncsPerCmd[][2],
										 u32		ui32CmdCount);

PVRSRV_ERROR PVRSRVRemoveCmdProcListKM(u32	ui32DevIndex,
									   u32	ui32CmdCount);

#endif


#if defined (__cplusplus)
}
#endif

#endif


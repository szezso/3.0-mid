/**********************************************************************
 *
 * Copyright (c) 2009-2010 Intel Corporation.
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

/* FIXME MLD */
/*
#ifdef INTEL_D3_FLUSH
#include "pal.h"
#endif
*/

#include "perproc.h"
#include "sgxinfokm.h"

#define CCB_OFFSET_IS_VALID(type, psCCBMemInfo, psCCBKick, offset) \
	((sizeof(type) <= (psCCBMemInfo)->ui32AllocSize) && \
	((psCCBKick)->offset <= (psCCBMemInfo)->ui32AllocSize - sizeof(type)))

#define	CCB_DATA_FROM_OFFSET(type, psCCBMemInfo, psCCBKick, offset) \
	((type *)(((char *)(psCCBMemInfo)->pvLinAddrKM) + \
		(psCCBKick)->offset))



void SGXTestActivePowerEvent(PVRSRV_DEVICE_NODE	*psDeviceNode,
								 u32			ui32CallerID);


PVRSRV_ERROR SGXScheduleCCBCommand(PVRSRV_SGXDEV_INFO 	*psDevInfo,
								   SGXMKIF_CMD_TYPE		eCommandType,
								   SGXMKIF_COMMAND		*psCommandData,
								   u32			ui32CallerID,
								   u32			ui32PDumpFlags);

PVRSRV_ERROR SGXScheduleCCBCommandKM(PVRSRV_DEVICE_NODE		*psDeviceNode,
									 SGXMKIF_CMD_TYPE		eCommandType,
									 SGXMKIF_COMMAND		*psCommandData,
									 u32				ui32CallerID,
									 u32				ui32PDumpFlags);


PVRSRV_ERROR SGXScheduleProcessQueuesKM(PVRSRV_DEVICE_NODE *psDeviceNode);


int SGXIsDevicePowered(PVRSRV_DEVICE_NODE *psDeviceNode);


void * SGXRegisterHWRenderContextKM(void *				psDeviceNode,
										IMG_DEV_VIRTADDR		*psHWRenderContextDevVAddr,
										PVRSRV_PER_PROCESS_DATA *psPerProc);


void * SGXRegisterHWTransferContextKM(void *				psDeviceNode,
										  IMG_DEV_VIRTADDR			*psHWTransferContextDevVAddr,
										  PVRSRV_PER_PROCESS_DATA	*psPerProc);


void SGXFlushHWRenderTargetKM(void * psSGXDevInfo, IMG_DEV_VIRTADDR psHWRTDataSetDevVAddr);


PVRSRV_ERROR SGXUnregisterHWRenderContextKM(void * hHWRenderContext);


PVRSRV_ERROR SGXUnregisterHWTransferContextKM(void * hHWTransferContext);

#if defined(SGX_FEATURE_2D_HARDWARE)

void * SGXRegisterHW2DContextKM(void *				psDeviceNode,
									IMG_DEV_VIRTADDR		*psHW2DContextDevVAddr,
									PVRSRV_PER_PROCESS_DATA *psPerProc);


PVRSRV_ERROR SGXUnregisterHW2DContextKM(void * hHW2DContext);
#endif

u32 SGXConvertTimeStamp(PVRSRV_SGXDEV_INFO	*psDevInfo,
							   u32			ui32TimeWraps,
							   u32			ui32Time);

void SGXCleanupRequest(PVRSRV_DEVICE_NODE	*psDeviceNode,
							IMG_DEV_VIRTADDR	*psHWDataDevVAddr,
							u32			ui32CleanupType);



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

#ifndef __PVR_BRIDGE_KM_H_
#define __PVR_BRIDGE_KM_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include "pvr_bridge.h"
#include "perproc.h"

PVRSRV_ERROR LinuxBridgeInit(void);
void LinuxBridgeDeInit(void);



PVRSRV_ERROR  PVRSRVEnumerateDevicesKM(u32 *pui32NumDevices,
												   PVRSRV_DEVICE_IDENTIFIER *psDevIdList);


PVRSRV_ERROR  PVRSRVAcquireDeviceDataKM(u32			uiDevIndex,
													PVRSRV_DEVICE_TYPE	eDeviceType,
													void *			*phDevCookie);


PVRSRV_ERROR  PVRSRVCreateCommandQueueKM(u32 ui32QueueSize,
													 PVRSRV_QUEUE_INFO **ppsQueueInfo);


PVRSRV_ERROR  PVRSRVDestroyCommandQueueKM(PVRSRV_QUEUE_INFO *psQueueInfo);


PVRSRV_ERROR  PVRSRVGetDeviceMemHeapsKM(void * hDevCookie,
													PVRSRV_HEAP_INFO *psHeapInfo);


PVRSRV_ERROR  PVRSRVCreateDeviceMemContextKM(void *					hDevCookie,
														 PVRSRV_PER_PROCESS_DATA	*psPerProc,
														 void *					*phDevMemContext,
														 u32					*pui32ClientHeapCount,
														 PVRSRV_HEAP_INFO			*psHeapInfo,
														 int					*pbCreated,
														 int					*pbShared);



PVRSRV_ERROR  PVRSRVDestroyDeviceMemContextKM(void * hDevCookie,
														  void * hDevMemContext,
														  int *pbDestroyed);



PVRSRV_ERROR  PVRSRVGetDeviceMemHeapInfoKM(void *				hDevCookie,
															void *			hDevMemContext,
															u32			*pui32ClientHeapCount,
															PVRSRV_HEAP_INFO	*psHeapInfo,
															int 			*pbShared
					);



PVRSRV_ERROR  _PVRSRVAllocDeviceMemKM(void *					hDevCookie,
												 PVRSRV_PER_PROCESS_DATA	*psPerProc,
												 void *					hDevMemHeap,
												 u32					ui32Flags,
												 u32					ui32Size,
												 u32					ui32Alignment,
												 PVRSRV_KERNEL_MEM_INFO		**ppsMemInfo);


#if defined(PVRSRV_LOG_MEMORY_ALLOCS)
	#define PVRSRVAllocDeviceMemKM(devCookie, perProc, devMemHeap, flags, size, alignment, memInfo, logStr) \
		(PVR_TRACE(("PVRSRVAllocDeviceMemKM(" #devCookie ", " #perProc ", " #devMemHeap ", " #flags ", " #size \
			", " #alignment "," #memInfo "): " logStr " (size = 0x%;x)", size)),\
			_PVRSRVAllocDeviceMemKM(devCookie, perProc, devMemHeap, flags, size, alignment, memInfo))
#else
	#define PVRSRVAllocDeviceMemKM(devCookie, perProc, devMemHeap, flags, size, alignment, memInfo, logStr) \
			_PVRSRVAllocDeviceMemKM(devCookie, perProc, devMemHeap, flags, size, alignment, memInfo)
#endif




PVRSRV_ERROR  PVRSRVFreeDeviceMemKM(void *			hDevCookie,
												PVRSRV_KERNEL_MEM_INFO	*psMemInfo);



PVRSRV_ERROR  PVRSRVDissociateDeviceMemKM(void *			hDevCookie,
												PVRSRV_KERNEL_MEM_INFO	*psMemInfo);


PVRSRV_ERROR  PVRSRVReserveDeviceVirtualMemKM(void *		hDevMemHeap,
														 IMG_DEV_VIRTADDR	*psDevVAddr,
														 u32			ui32Size,
														 u32			ui32Alignment,
														 PVRSRV_KERNEL_MEM_INFO	**ppsMemInfo);


PVRSRV_ERROR  PVRSRVFreeDeviceVirtualMemKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);


PVRSRV_ERROR  PVRSRVMapDeviceMemoryKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
												  PVRSRV_KERNEL_MEM_INFO	*psSrcMemInfo,
												  void *				hDstDevMemHeap,
												  PVRSRV_KERNEL_MEM_INFO	**ppsDstMemInfo);


PVRSRV_ERROR  PVRSRVUnmapDeviceMemoryKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);


PVRSRV_ERROR  PVRSRVWrapExtMemoryKM(void *				hDevCookie,
												PVRSRV_PER_PROCESS_DATA	*psPerProc,
												void *				hDevMemContext,
												u32 				ui32ByteSize,
												u32				ui32PageOffset,
												int				bPhysContig,
												IMG_SYS_PHYADDR	 		*psSysAddr,
												void 				*pvLinAddr,
												u32				ui32Flags,
												PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);


PVRSRV_ERROR  PVRSRVUnwrapExtMemoryKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);


PVRSRV_ERROR PVRSRVEnumerateDCKM(PVRSRV_DEVICE_CLASS DeviceClass,
								 u32 *pui32DevCount,
								 u32 *pui32DevID );


PVRSRV_ERROR PVRSRVOpenDCDeviceKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
								  u32				ui32DeviceID,
								  void * 				hDevCookie,
								  void * 				*phDeviceKM);


PVRSRV_ERROR PVRSRVCloseDCDeviceKM(void * hDeviceKM, int bResManCallback);


PVRSRV_ERROR PVRSRVEnumDCFormatsKM(void * hDeviceKM,
								   u32 *pui32Count,
								   DISPLAY_FORMAT *psFormat);


PVRSRV_ERROR PVRSRVEnumDCDimsKM(void * hDeviceKM,
								DISPLAY_FORMAT *psFormat,
								u32 *pui32Count,
								DISPLAY_DIMS *psDim);


PVRSRV_ERROR PVRSRVGetDCSystemBufferKM(void * hDeviceKM,
									   void * *phBuffer);


PVRSRV_ERROR PVRSRVGetDCInfoKM(void * hDeviceKM,
							   DISPLAY_INFO *psDisplayInfo);

PVRSRV_ERROR PVRSRVCreateDCSwapChainKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
									   void *				hDeviceKM,
									   u32				ui32Flags,
									   DISPLAY_SURF_ATTRIBUTES	*psDstSurfAttrib,
									   DISPLAY_SURF_ATTRIBUTES	*psSrcSurfAttrib,
									   u32				ui32BufferCount,
									   u32				ui32OEMFlags,
									   void *				*phSwapChain,
									   u32				*pui32SwapChainID);

PVRSRV_ERROR PVRSRVDestroyDCSwapChainKM(void *	hSwapChain);

PVRSRV_ERROR PVRSRVSetDCDstRectKM(void *	hDeviceKM,
								  void *	hSwapChain,
								  IMG_RECT	*psRect);

PVRSRV_ERROR PVRSRVSetDCSrcRectKM(void *	hDeviceKM,
								  void *	hSwapChain,
								  IMG_RECT	*psRect);

PVRSRV_ERROR PVRSRVSetDCDstColourKeyKM(void *	hDeviceKM,
									   void *	hSwapChain,
									   u32	ui32CKColour);

PVRSRV_ERROR PVRSRVSetDCSrcColourKeyKM(void *	hDeviceKM,
									void *		hSwapChain,
									u32		ui32CKColour);

PVRSRV_ERROR PVRSRVGetDCBuffersKM(void *	hDeviceKM,
								  void *	hSwapChain,
								  u32	*pui32BufferCount,
								  void *	*phBuffer);

PVRSRV_ERROR PVRSRVSwapToDCBufferKM(void *	hDeviceKM,
									void *	hBuffer,
									u32	ui32SwapInterval,
									void *	hPrivateTag,
									u32	ui32ClipRectCount,
									IMG_RECT	*psClipRect);

PVRSRV_ERROR PVRSRVSwapToDCSystemKM(void *	hDeviceKM,
									void *	hSwapChain);


PVRSRV_ERROR PVRSRVOpenBCDeviceKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
								  u32				ui32DeviceID,
								  void *				hDevCookie,
								  void *				*phDeviceKM);

PVRSRV_ERROR PVRSRVCloseBCDeviceKM(void * hDeviceKM, int bResManCallback);


PVRSRV_ERROR PVRSRVGetBCInfoKM(void *	hDeviceKM,
							   BUFFER_INFO	*psBufferInfo);

PVRSRV_ERROR PVRSRVGetBCBufferKM(void *	hDeviceKM,
								 u32	ui32BufferIndex,
								 void *	*phBuffer);



PVRSRV_ERROR  PVRSRVMapDeviceClassMemoryKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
													   void *				hDevMemContext,
													   void *				hDeviceClassBuffer,
													   PVRSRV_KERNEL_MEM_INFO	**ppsMemInfo,
													   void *				*phOSMapInfo);


PVRSRV_ERROR  PVRSRVUnmapDeviceClassMemoryKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);


PVRSRV_ERROR  PVRSRVGetFreeDeviceMemKM(u32 ui32Flags,
												   u32 *pui32Total,
												   u32 *pui32Free,
												   u32 *pui32LargestBlock);

PVRSRV_ERROR  PVRSRVAllocSyncInfoKM(void *					hDevCookie,
												void *					hDevMemContext,
												PVRSRV_KERNEL_SYNC_INFO	**ppsKernelSyncInfo);

PVRSRV_ERROR  PVRSRVFreeSyncInfoKM(PVRSRV_KERNEL_SYNC_INFO	*psKernelSyncInfo);


PVRSRV_ERROR  PVRSRVGetMiscInfoKM(PVRSRV_MISC_INFO *psMiscInfo);

PVRSRV_ERROR PVRSRVGetFBStatsKM(u32	*pui32Total,
								u32	*pui32Available);

 PVRSRV_ERROR
PVRSRVAllocSharedSysMemoryKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
							 u32 				ui32Flags,
							 u32 				ui32Size,
							 PVRSRV_KERNEL_MEM_INFO		**ppsKernelMemInfo);

 PVRSRV_ERROR
PVRSRVFreeSharedSysMemoryKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);

 PVRSRV_ERROR
PVRSRVDissociateMemFromResmanKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);

#ifdef INTEL_D3_CHANGES

PVRSRV_ERROR PVRSRVWaitForWriteOpSyncKM(PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo);
#endif

#if defined (__cplusplus)
}
#endif

#endif


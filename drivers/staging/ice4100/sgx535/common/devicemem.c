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

#include <stddef.h>

#include "services_headers.h"
#include "buffer_manager.h"
#include "pdump_km.h"
#include "pvr_bridge_km.h"

static PVRSRV_ERROR AllocDeviceMem(void *hDevCookie,
				   void *hDevMemHeap,
				   u32 ui32Flags,
				   u32 ui32Size,
				   u32 ui32Alignment,
				   PVRSRV_KERNEL_MEM_INFO ** ppsMemInfo);

typedef struct _RESMAN_MAP_DEVICE_MEM_DATA_ {

	PVRSRV_KERNEL_MEM_INFO *psMemInfo;

	PVRSRV_KERNEL_MEM_INFO *psSrcMemInfo;
} RESMAN_MAP_DEVICE_MEM_DATA;

PVRSRV_ERROR PVRSRVGetDeviceMemHeapsKM(void *hDevCookie,
				       PVRSRV_HEAP_INFO * psHeapInfo)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	u32 i;

	if (hDevCookie == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetDeviceMemHeapsKM: hDevCookie invalid"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDeviceNode = (PVRSRV_DEVICE_NODE *) hDevCookie;

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	for (i = 0; i < ui32HeapCount; i++) {

		psHeapInfo[i].ui32HeapID = psDeviceMemoryHeap[i].ui32HeapID;
		psHeapInfo[i].hDevMemHeap = psDeviceMemoryHeap[i].hDevMemHeap;
		psHeapInfo[i].sDevVAddrBase =
		    psDeviceMemoryHeap[i].sDevVAddrBase;
		psHeapInfo[i].ui32HeapByteSize =
		    psDeviceMemoryHeap[i].ui32HeapSize;
		psHeapInfo[i].ui32Attribs = psDeviceMemoryHeap[i].ui32Attribs;
	}

	for (; i < PVRSRV_MAX_CLIENT_HEAPS; i++) {
		memset(psHeapInfo + i, 0, sizeof(*psHeapInfo));
		psHeapInfo[i].ui32HeapID = (u32) PVRSRV_UNDEFINED_HEAP_ID;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVCreateDeviceMemContextKM(void *hDevCookie,
					    PVRSRV_PER_PROCESS_DATA * psPerProc,
					    void **phDevMemContext,
					    u32 * pui32ClientHeapCount,
					    PVRSRV_HEAP_INFO * psHeapInfo,
					    int *pbCreated, int *pbShared)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount, ui32ClientHeapCount = 0;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	void *hDevMemContext;
	void *hDevMemHeap;
	IMG_DEV_PHYADDR sPDDevPAddr;
	u32 i;

	if (hDevCookie == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVCreateDeviceMemContextKM: hDevCookie invalid"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDeviceNode = (PVRSRV_DEVICE_NODE *) hDevCookie;

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	hDevMemContext = BM_CreateContext(psDeviceNode,
					  &sPDDevPAddr, psPerProc, pbCreated);
	if (hDevMemContext == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVCreateDeviceMemContextKM: Failed BM_CreateContext"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	for (i = 0; i < ui32HeapCount; i++) {
		switch (psDeviceMemoryHeap[i].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			{

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    psDeviceMemoryHeap[i].hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
#if defined(PVR_SECURE_HANDLES)
				pbShared[ui32ClientHeapCount] = 1;
#endif
				ui32ClientHeapCount++;
				break;
			}
		case DEVICE_MEMORY_HEAP_PERCONTEXT:
			{
				hDevMemHeap = BM_CreateHeap(hDevMemContext,
							    &psDeviceMemoryHeap
							    [i]);

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
#if defined(PVR_SECURE_HANDLES)
				pbShared[ui32ClientHeapCount] = 0;
#endif

				ui32ClientHeapCount++;
				break;
			}
		}
	}

	*pui32ClientHeapCount = ui32ClientHeapCount;
	*phDevMemContext = hDevMemContext;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVDestroyDeviceMemContextKM(void *hDevCookie,
					     void *hDevMemContext,
					     int *pbDestroyed)
{
	return BM_DestroyContext(hDevMemContext, pbDestroyed);
}

PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfoKM(void *hDevCookie,
					  void *hDevMemContext,
					  u32 * pui32ClientHeapCount,
					  PVRSRV_HEAP_INFO * psHeapInfo,
					  int *pbShared)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount, ui32ClientHeapCount = 0;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	void *hDevMemHeap;
	u32 i;

	if (hDevCookie == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetDeviceMemHeapInfoKM: hDevCookie invalid"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDeviceNode = (PVRSRV_DEVICE_NODE *) hDevCookie;

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	for (i = 0; i < ui32HeapCount; i++) {
		switch (psDeviceMemoryHeap[i].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			{

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    psDeviceMemoryHeap[i].hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
#if defined(PVR_SECURE_HANDLES)
				pbShared[ui32ClientHeapCount] = 1;
#endif
				ui32ClientHeapCount++;
				break;
			}
		case DEVICE_MEMORY_HEAP_PERCONTEXT:
			{
				hDevMemHeap = BM_CreateHeap(hDevMemContext,
							    &psDeviceMemoryHeap
							    [i]);

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
#if defined(PVR_SECURE_HANDLES)
				pbShared[ui32ClientHeapCount] = 0;
#endif

				ui32ClientHeapCount++;
				break;
			}
		}
	}

	*pui32ClientHeapCount = ui32ClientHeapCount;

	return PVRSRV_OK;
}

static PVRSRV_ERROR AllocDeviceMem(void *hDevCookie,
				   void *hDevMemHeap,
				   u32 ui32Flags,
				   u32 ui32Size,
				   u32 ui32Alignment,
				   PVRSRV_KERNEL_MEM_INFO ** ppsMemInfo)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	BM_HANDLE hBuffer;

	PVRSRV_MEMBLK *psMemBlock;
	int bBMError;

	*ppsMemInfo = NULL;

#ifdef INTEL_D3_PAD
	while (ui32Alignment % 64) {
		ui32Alignment <<= 1;
	}
#endif

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(PVRSRV_KERNEL_MEM_INFO),
		       (void **)&psMemInfo, NULL,
		       "Kernel Memory Info") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "AllocDeviceMem: Failed to alloc memory for block"));
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
	}

	memset(psMemInfo, 0, sizeof(*psMemInfo));

	psMemBlock = &(psMemInfo->sMemBlk);

	psMemInfo->ui32Flags = ui32Flags | PVRSRV_MEM_RAM_BACKED_ALLOCATION;

	bBMError = BM_Alloc(hDevMemHeap,
			    NULL,
			    ui32Size,
			    &psMemInfo->ui32Flags,
			    IMG_CAST_TO_DEVVADDR_UINT(ui32Alignment), &hBuffer);

	if (!bBMError) {
		PVR_DPF((PVR_DBG_ERROR, "AllocDeviceMem: BM_Alloc Failed"));
		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(PVRSRV_KERNEL_MEM_INFO), psMemInfo, NULL);

		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *)hBuffer;

	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);

	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;

	psMemInfo->ui32AllocSize = ui32Size;

	psMemInfo->pvSysBackupBuffer = NULL;

	*ppsMemInfo = psMemInfo;

	return (PVRSRV_OK);
}

static PVRSRV_ERROR FreeDeviceMem2(PVRSRV_KERNEL_MEM_INFO *psMemInfo, int bFromAllocator)
{
	BM_HANDLE		hBuffer;

	if (!psMemInfo)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	hBuffer = psMemInfo->sMemBlk.hBuffer;


	if (bFromAllocator)
		BM_Free(hBuffer, psMemInfo->ui32Flags);
	else
		BM_FreeExport(hBuffer, psMemInfo->ui32Flags);


	if ((psMemInfo->pvSysBackupBuffer) && bFromAllocator)
	{

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, psMemInfo->ui32AllocSize, psMemInfo->pvSysBackupBuffer, NULL);
		psMemInfo->pvSysBackupBuffer = NULL;
	}

	if (psMemInfo->ui32RefCount == 0)
		OSFreeMem(PVRSRV_PAGEABLE_SELECT, sizeof(PVRSRV_KERNEL_MEM_INFO), psMemInfo, NULL);


	return(PVRSRV_OK);
}

static PVRSRV_ERROR FreeDeviceMem(PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	BM_HANDLE hBuffer;

	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	hBuffer = psMemInfo->sMemBlk.hBuffer;

	BM_Free(hBuffer, psMemInfo->ui32Flags);

	if (psMemInfo->pvSysBackupBuffer) {

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, psMemInfo->ui32AllocSize,
			  psMemInfo->pvSysBackupBuffer, NULL);
		psMemInfo->pvSysBackupBuffer = NULL;
	}

	OSFreeMem(PVRSRV_PAGEABLE_SELECT, sizeof(PVRSRV_KERNEL_MEM_INFO),
		  psMemInfo, NULL);

	return (PVRSRV_OK);
}

PVRSRV_ERROR PVRSRVAllocSyncInfoKM(void *hDevCookie,
				   void *hDevMemContext,
				   PVRSRV_KERNEL_SYNC_INFO ** ppsKernelSyncInfo)
{
	void *hSyncDevMemHeap;
	DEVICE_MEMORY_INFO *psDevMemoryInfo;
	BM_CONTEXT *pBMContext;
	PVRSRV_ERROR eError;
	PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo;
	PVRSRV_SYNC_DATA *psSyncData;

	eError = OSAllocMem(PVRSRV_PAGEABLE_SELECT,
			    sizeof(PVRSRV_KERNEL_SYNC_INFO),
			    (void **)&psKernelSyncInfo, NULL,
			    "Kernel Synchronization Info");
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVAllocSyncInfoKM: Failed to alloc memory"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psKernelSyncInfo->ui32RefCount = 0;

	pBMContext = (BM_CONTEXT *) hDevMemContext;
	psDevMemoryInfo = &pBMContext->psDeviceNode->sDevMemoryInfo;

	hSyncDevMemHeap =
	    psDevMemoryInfo->psDeviceMemoryHeap[psDevMemoryInfo->
						ui32SyncHeapID].hDevMemHeap;

	eError = AllocDeviceMem(hDevCookie,
				hSyncDevMemHeap,
				PVRSRV_MEM_CACHE_CONSISTENT,
				sizeof(PVRSRV_SYNC_DATA),
				sizeof(u32),
				&psKernelSyncInfo->psSyncDataMemInfoKM);

	if (eError != PVRSRV_OK) {

		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVAllocSyncInfoKM: Failed to alloc memory"));
		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(PVRSRV_KERNEL_SYNC_INFO), psKernelSyncInfo,
			  NULL);

		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psKernelSyncInfo->psSyncData =
	    psKernelSyncInfo->psSyncDataMemInfoKM->pvLinAddrKM;
	psSyncData = psKernelSyncInfo->psSyncData;

	psSyncData->ui32WriteOpsPending = 0;
	psSyncData->ui32WriteOpsComplete = 0;
	psSyncData->ui32ReadOpsPending = 0;
	psSyncData->ui32ReadOpsComplete = 0;
	psSyncData->ui32LastOpDumpVal = 0;
	psSyncData->ui32LastReadOpDumpVal = 0;

#if defined(PDUMP)
	PDUMPMEM(psKernelSyncInfo->psSyncDataMemInfoKM->pvLinAddrKM,
		 psKernelSyncInfo->psSyncDataMemInfoKM,
		 0,
		 psKernelSyncInfo->psSyncDataMemInfoKM->ui32AllocSize,
		 PDUMP_FLAGS_CONTINUOUS,
		 MAKEUNIQUETAG(psKernelSyncInfo->psSyncDataMemInfoKM));
#endif

	psKernelSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr =
	    psKernelSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
	    offsetof(PVRSRV_SYNC_DATA, ui32WriteOpsComplete);
	psKernelSyncInfo->sReadOpsCompleteDevVAddr.uiAddr =
	    psKernelSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
	    offsetof(PVRSRV_SYNC_DATA, ui32ReadOpsComplete);

	psKernelSyncInfo->psSyncDataMemInfoKM->psKernelSyncInfo = NULL;

	*ppsKernelSyncInfo = psKernelSyncInfo;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVFreeSyncInfoKM(PVRSRV_KERNEL_SYNC_INFO * psKernelSyncInfo)
{
	PVRSRV_ERROR eError;

	if (psKernelSyncInfo->ui32RefCount != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "oops: sync info ref count not zero at destruction"));

		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	eError = FreeDeviceMem(psKernelSyncInfo->psSyncDataMemInfoKM);
	(void)OSFreeMem(PVRSRV_PAGEABLE_SELECT, sizeof(PVRSRV_KERNEL_SYNC_INFO),
			psKernelSyncInfo, NULL);

	return eError;
}

static void freeWrapped(PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	void *hOSWrapMem = psMemInfo->sMemBlk.hOSWrapMem;

	if(psMemInfo->sMemBlk.psIntSysPAddr)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(IMG_SYS_PHYADDR), psMemInfo->sMemBlk.psIntSysPAddr, NULL);
		psMemInfo->sMemBlk.psIntSysPAddr = NULL;
	}

	if(hOSWrapMem)
	{
		OSReleasePhysPageAddr(hOSWrapMem);
	}
}

static PVRSRV_ERROR FreeMemCallBackCommon(PVRSRV_KERNEL_MEM_INFO *psMemInfo, u32 ui32Param, int bFromAllocator)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	psMemInfo->ui32RefCount--;

	if ((psMemInfo->ui32Flags & PVRSRV_MEM_EXPORTED) &&
			(bFromAllocator == 1)) {
		void *hMemInfo = NULL;

		eError = PVRSRVFindHandle(KERNEL_HANDLE_BASE,
					  &hMemInfo,
					  psMemInfo,
					  PVRSRV_HANDLE_TYPE_MEM_INFO);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "FreeMemCallBackCommon: can't find exported meminfo in the global handle list"));
			return eError;
		}

		eError = PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
					     hMemInfo,
					     PVRSRV_HANDLE_TYPE_MEM_INFO);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "FreeMemCallBackCommon: PVRSRVReleaseHandle failed for exported meminfo"));
			return eError;
		}
	}

	if (psMemInfo->ui32RefCount == 0) {
		switch(psMemInfo->memType)
	{

			case PVRSRV_MEMTYPE_WRAPPED:
				freeWrapped(psMemInfo);
			case PVRSRV_MEMTYPE_DEVICE:
				if (psMemInfo->psKernelSyncInfo)
				{
					psMemInfo->psKernelSyncInfo->ui32RefCount--;
					if (psMemInfo->psKernelSyncInfo->ui32RefCount == 0)
					{
						eError = PVRSRVFreeSyncInfoKM(psMemInfo->psKernelSyncInfo);
					}
				}
			case PVRSRV_MEMTYPE_DEVICECLASS:
				break;
			default:
				PVR_DPF((PVR_DBG_ERROR, "FreeMemCallBackCommon: Unknown memType"));
				return PVRSRV_ERROR_GENERIC;
		}
	}

	return FreeDeviceMem2(psMemInfo, bFromAllocator);
}

static PVRSRV_ERROR FreeDeviceMemCallBack(void *pvParam,
										  u32 ui32Param)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = (PVRSRV_KERNEL_MEM_INFO *)pvParam;

	return FreeMemCallBackCommon(psMemInfo, ui32Param, 1);
}

PVRSRV_ERROR PVRSRVFreeDeviceMemKM(void *hDevCookie,
				   PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	PVRSRV_ERROR eError;

	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (psMemInfo->sMemBlk.hResItem != NULL) {
		eError = ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
	} else {

		eError = FreeDeviceMemCallBack(psMemInfo, 0);
	}

	return eError;
}

PVRSRV_ERROR _PVRSRVAllocDeviceMemKM(void *hDevCookie,
				     PVRSRV_PER_PROCESS_DATA * psPerProc,
				     void *hDevMemHeap,
				     u32 ui32Flags,
				     u32 ui32Size,
				     u32 ui32Alignment,
				     PVRSRV_KERNEL_MEM_INFO ** ppsMemInfo)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	PVRSRV_ERROR eError;
	BM_HEAP *psBMHeap;
	void *hDevMemContext;

	if (!hDevMemHeap || (ui32Size == 0)) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32Flags & PVRSRV_HAP_CACHETYPE_MASK) {
		if (((ui32Size % HOST_PAGESIZE()) != 0) ||
		    ((ui32Alignment % HOST_PAGESIZE()) != 0)) {
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	eError = AllocDeviceMem(hDevCookie,
				hDevMemHeap,
				ui32Flags, ui32Size, ui32Alignment, &psMemInfo);

	if (eError != PVRSRV_OK) {
		return eError;
	}

	if (ui32Flags & PVRSRV_MEM_NO_SYNCOBJ) {
		psMemInfo->psKernelSyncInfo = NULL;
	} else {

		psBMHeap = (BM_HEAP *) hDevMemHeap;
		hDevMemContext = (void *)psBMHeap->pBMContext;
		eError = PVRSRVAllocSyncInfoKM(hDevCookie,
					       hDevMemContext,
					       &psMemInfo->psKernelSyncInfo);
		if (eError != PVRSRV_OK) {
			goto free_mainalloc;
		}
		psMemInfo->psKernelSyncInfo->ui32RefCount++;
	}

	*ppsMemInfo = psMemInfo;

	if (ui32Flags & PVRSRV_MEM_NO_RESMAN) {
		psMemInfo->sMemBlk.hResItem = NULL;
	} else {

		psMemInfo->sMemBlk.hResItem =
		    ResManRegisterRes(psPerProc->hResManContext,
				      RESMAN_TYPE_DEVICEMEM_ALLOCATION,
				      psMemInfo, 0, FreeDeviceMemCallBack);
		if (psMemInfo->sMemBlk.hResItem == NULL) {

			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto free_mainalloc;
		}
	}

	psMemInfo->ui32RefCount++;

	psMemInfo->memType = PVRSRV_MEMTYPE_DEVICE;

	return (PVRSRV_OK);

free_mainalloc:
	FreeDeviceMem(psMemInfo);

	return eError;
}

PVRSRV_ERROR PVRSRVDissociateDeviceMemKM(void *hDevCookie,
					 PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode = hDevCookie;

	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError =
	    ResManDissociateRes(psMemInfo->sMemBlk.hResItem,
				psDeviceNode->hResManContext);

	PVR_ASSERT(eError == PVRSRV_OK);

	return eError;
}

PVRSRV_ERROR PVRSRVGetFreeDeviceMemKM(u32 ui32Flags,
				      u32 * pui32Total,
				      u32 * pui32Free, u32 * pui32LargestBlock)
{

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVUnwrapExtMemoryKM(PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
}

static PVRSRV_ERROR UnwrapExtMemoryCallBack(void *pvParam, u32 ui32Param)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	return FreeMemCallBackCommon(psMemInfo, ui32Param, 1);
}

PVRSRV_ERROR PVRSRVWrapExtMemoryKM(void *hDevCookie,
				   PVRSRV_PER_PROCESS_DATA * psPerProc,
				   void *hDevMemContext,
				   u32 ui32ByteSize,
				   u32 ui32PageOffset,
				   int bPhysContig,
				   IMG_SYS_PHYADDR * psExtSysPAddr,
				   void *pvLinAddr,
				   u32 ui32Flags,
				   PVRSRV_KERNEL_MEM_INFO ** ppsMemInfo)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = NULL;
	DEVICE_MEMORY_INFO *psDevMemoryInfo;
	u32 ui32HostPageSize = HOST_PAGESIZE();
	void *hDevMemHeap = NULL;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	BM_HANDLE hBuffer;
	PVRSRV_MEMBLK *psMemBlock;
	int bBMError;
	BM_HEAP *psBMHeap;
	PVRSRV_ERROR eError;
	void *pvPageAlignedCPUVAddr;
	IMG_SYS_PHYADDR *psIntSysPAddr = NULL;
	void *hOSWrapMem = NULL;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	u32 ui32PageCount = 0;
	u32 i;

	psDeviceNode = (PVRSRV_DEVICE_NODE *) hDevCookie;
	PVR_ASSERT(psDeviceNode != NULL);

	if (psDeviceNode == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVWrapExtMemoryKM: invalid parameter"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#ifdef INTEL_D3_CHANGES
	if (pvLinAddr && psExtSysPAddr == NULL)
#else
	if (pvLinAddr)
#endif
	{

		ui32PageOffset = (u32) pvLinAddr & (ui32HostPageSize - 1);

		ui32PageCount =
		    HOST_PAGEALIGN(ui32ByteSize +
				   ui32PageOffset) / ui32HostPageSize;
		pvPageAlignedCPUVAddr =
		    (void *)((u32) pvLinAddr - ui32PageOffset);

		if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			       ui32PageCount * sizeof(IMG_SYS_PHYADDR),
			       (void **)&psIntSysPAddr, NULL,
			       "Array of Page Addresses") != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVWrapExtMemoryKM: Failed to alloc memory for block"));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		eError = OSAcquirePhysPageAddr(pvPageAlignedCPUVAddr,
					       ui32PageCount * ui32HostPageSize,
					       psIntSysPAddr,
					       &hOSWrapMem,
					       (ui32Flags != 0) ? 1 : 0);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVWrapExtMemoryKM: Failed to alloc memory for block"));
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto ErrorExitPhase1;
		}

		psExtSysPAddr = psIntSysPAddr;

		bPhysContig = 0;
	} else {

	}

	psDevMemoryInfo =
	    &((BM_CONTEXT *) hDevMemContext)->psDeviceNode->sDevMemoryInfo;
	psDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;
	for (i = 0; i < PVRSRV_MAX_CLIENT_HEAPS; i++) {
		if (HEAP_IDX(psDeviceMemoryHeap[i].ui32HeapID) ==
		    psDevMemoryInfo->ui32MappingHeapID) {
			if (psDeviceMemoryHeap[i].DevMemHeapType ==
			    DEVICE_MEMORY_HEAP_PERCONTEXT) {

				hDevMemHeap =
				    BM_CreateHeap(hDevMemContext,
						  &psDeviceMemoryHeap[i]);
			} else {
				hDevMemHeap =
				    psDevMemoryInfo->psDeviceMemoryHeap[i].
				    hDevMemHeap;
			}
			break;
		}
	}

	if (hDevMemHeap == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVWrapExtMemoryKM: unable to find mapping heap"));
		eError = PVRSRV_ERROR_GENERIC;
		goto ErrorExitPhase2;
	}

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(PVRSRV_KERNEL_MEM_INFO),
		       (void **)&psMemInfo, NULL,
		       "Kernel Memory Info") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVWrapExtMemoryKM: Failed to alloc memory for block"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExitPhase2;
	}

	memset(psMemInfo, 0, sizeof(*psMemInfo));
	psMemInfo->ui32Flags = ui32Flags;

	psMemBlock = &(psMemInfo->sMemBlk);

	bBMError = BM_Wrap(hDevMemHeap,
			   ui32ByteSize,
			   ui32PageOffset,
			   bPhysContig,
			   psExtSysPAddr,
			   NULL, &psMemInfo->ui32Flags, &hBuffer);
	if (!bBMError) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVWrapExtMemoryKM: BM_Wrap Failed"));
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto ErrorExitPhase3;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);
	psMemBlock->hOSWrapMem = hOSWrapMem;
	psMemBlock->psIntSysPAddr = psIntSysPAddr;

	psMemBlock->hBuffer = (void *)hBuffer;

	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);
	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = ui32ByteSize;

	psMemInfo->pvSysBackupBuffer = NULL;

	psBMHeap = (BM_HEAP *) hDevMemHeap;
	hDevMemContext = (void *)psBMHeap->pBMContext;
	eError = PVRSRVAllocSyncInfoKM(hDevCookie,
				       hDevMemContext,
				       &psMemInfo->psKernelSyncInfo);
	if (eError != PVRSRV_OK) {
		goto ErrorExitPhase4;
	}

	psMemInfo->psKernelSyncInfo->ui32RefCount++;

	psMemInfo->memType = PVRSRV_MEMTYPE_WRAPPED;

	psMemInfo->ui32RefCount++;

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICEMEM_WRAP, psMemInfo, 0,
			      UnwrapExtMemoryCallBack);

	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;

ErrorExitPhase4:
	if (psMemInfo) {
		FreeDeviceMem(psMemInfo);

		psMemInfo = NULL;
	}

ErrorExitPhase3:
	if (psMemInfo) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  sizeof(PVRSRV_KERNEL_MEM_INFO), psMemInfo, NULL);

	}

ErrorExitPhase2:
	if (psIntSysPAddr) {
		OSReleasePhysPageAddr(hOSWrapMem);
	}

ErrorExitPhase1:
	if (psIntSysPAddr) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  ui32PageCount * sizeof(IMG_SYS_PHYADDR),
			  psIntSysPAddr, NULL);

	}

	return eError;
}

PVRSRV_ERROR PVRSRVUnmapDeviceMemoryKM(PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
}

static PVRSRV_ERROR UnmapDeviceMemoryCallBack(void *pvParam, u32 ui32Param)
{
	PVRSRV_ERROR eError;
	RESMAN_MAP_DEVICE_MEM_DATA *psMapData = pvParam;

	if (psMapData->psMemInfo->sMemBlk.psIntSysPAddr) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(IMG_SYS_PHYADDR),
			psMapData->psMemInfo->sMemBlk.psIntSysPAddr, NULL);
		psMapData->psMemInfo->sMemBlk.psIntSysPAddr = NULL;
	}

	psMapData->psMemInfo->psKernelSyncInfo->ui32RefCount--;
	if (psMapData->psMemInfo->psKernelSyncInfo->ui32RefCount == 0) {
		eError =
		    PVRSRVFreeSyncInfoKM(psMapData->psMemInfo->
					 psKernelSyncInfo);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "UnmapDeviceMemoryCallBack: Failed to free sync info"));
			return eError;
		}
	}

	eError = FreeDeviceMem(psMapData->psMemInfo);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "UnmapDeviceMemoryCallBack: Failed to free DST meminfo"));
		return eError;
	}

	eError = FreeMemCallBackCommon(psMapData->psSrcMemInfo, 0, 0);

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(RESMAN_MAP_DEVICE_MEM_DATA),
		psMapData, NULL);

	return eError;
}

PVRSRV_ERROR PVRSRVMapDeviceMemoryKM(PVRSRV_PER_PROCESS_DATA * psPerProc,
				     PVRSRV_KERNEL_MEM_INFO * psSrcMemInfo,
				     void *hDstDevMemHeap,
				     PVRSRV_KERNEL_MEM_INFO ** ppsDstMemInfo)
{
	PVRSRV_ERROR eError;
	u32 i;
	u32 ui32PageCount, ui32PageOffset;
	u32 ui32HostPageSize = HOST_PAGESIZE();
	IMG_SYS_PHYADDR *psSysPAddr = NULL;
	IMG_DEV_PHYADDR sDevPAddr;
	BM_BUF *psBuf;
	IMG_DEV_VIRTADDR sDevVAddr;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = NULL;
	BM_HANDLE hBuffer;
	PVRSRV_MEMBLK *psMemBlock;
	int bBMError;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	void *pvPageAlignedCPUVAddr;
	RESMAN_MAP_DEVICE_MEM_DATA *psMapData = NULL;

	if (!psSrcMemInfo || !hDstDevMemHeap || !ppsDstMemInfo) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: invalid parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*ppsDstMemInfo = NULL;

	ui32PageOffset =
	    psSrcMemInfo->sDevVAddr.uiAddr & (ui32HostPageSize - 1);
	ui32PageCount =
	    HOST_PAGEALIGN(psSrcMemInfo->ui32AllocSize +
			   ui32PageOffset) / ui32HostPageSize;
	pvPageAlignedCPUVAddr =
	    (void *)((u32) psSrcMemInfo->pvLinAddrKM - ui32PageOffset);

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       ui32PageCount * sizeof(IMG_SYS_PHYADDR),
		       (void **)&psSysPAddr, NULL,
		       "Array of Page Addresses") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: Failed to alloc memory for block"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psBuf = psSrcMemInfo->sMemBlk.hBuffer;

	psDeviceNode = psBuf->pMapping->pBMHeap->pBMContext->psDeviceNode;

	sDevVAddr.uiAddr =
	    psSrcMemInfo->sDevVAddr.uiAddr -
	    IMG_CAST_TO_DEVVADDR_UINT(ui32PageOffset);
	for (i = 0; i < ui32PageCount; i++) {
		BM_GetPhysPageAddr(psSrcMemInfo, sDevVAddr, &sDevPAddr);

		psSysPAddr[i] =
		    SysDevPAddrToSysPAddr(psDeviceNode->sDevId.eDeviceType,
					  sDevPAddr);

		sDevVAddr.uiAddr += IMG_CAST_TO_DEVVADDR_UINT(ui32HostPageSize);
	}

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(RESMAN_MAP_DEVICE_MEM_DATA),
		       (void **)&psMapData, NULL,
		       "Resource Manager Map Data") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: Failed to alloc resman map data"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(PVRSRV_KERNEL_MEM_INFO),
		       (void **)&psMemInfo, NULL,
		       "Kernel Memory Info") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: Failed to alloc memory for block"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}

	memset(psMemInfo, 0, sizeof(*psMemInfo));
	psMemInfo->ui32Flags = psSrcMemInfo->ui32Flags;

	psMemBlock = &(psMemInfo->sMemBlk);

	bBMError = BM_Wrap(hDstDevMemHeap,
			   psSrcMemInfo->ui32AllocSize,
			   ui32PageOffset,
			   0,
			   psSysPAddr,
			   pvPageAlignedCPUVAddr,
			   &psMemInfo->ui32Flags, &hBuffer);

	if (!bBMError) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: BM_Wrap Failed"));
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto ErrorExit;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *)hBuffer;

	psMemBlock->psIntSysPAddr = psSysPAddr;

	psMemInfo->pvLinAddrKM = psSrcMemInfo->pvLinAddrKM;

	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = psSrcMemInfo->ui32AllocSize;
	psMemInfo->psKernelSyncInfo = psSrcMemInfo->psKernelSyncInfo;

	psMemInfo->psKernelSyncInfo->ui32RefCount++;

	psMemInfo->pvSysBackupBuffer = NULL;

	psMemInfo->ui32RefCount++;

	psSrcMemInfo->ui32RefCount++;

	BM_Export(psSrcMemInfo->sMemBlk.hBuffer);

	psMemInfo->memType = PVRSRV_MEMTYPE_MAPPED;

	psMapData->psMemInfo = psMemInfo;
	psMapData->psSrcMemInfo = psSrcMemInfo;

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICEMEM_MAPPING, psMapData, 0,
			      UnmapDeviceMemoryCallBack);

	*ppsDstMemInfo = psMemInfo;

	return PVRSRV_OK;

ErrorExit:

	if (psSysPAddr) {

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(IMG_SYS_PHYADDR),
			  psSysPAddr, NULL);

	}

	if (psMemInfo) {

		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(PVRSRV_KERNEL_MEM_INFO), psMemInfo, NULL);

	}

	if (psMapData) {

		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(RESMAN_MAP_DEVICE_MEM_DATA), psMapData, NULL);

	}

	return eError;
}

PVRSRV_ERROR PVRSRVUnmapDeviceClassMemoryKM(PVRSRV_KERNEL_MEM_INFO * psMemInfo)
{
	if (!psMemInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
}

static PVRSRV_ERROR UnmapDeviceClassMemoryCallBack(void *pvParam, u32 ui32Param)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	return FreeMemCallBackCommon(psMemInfo, ui32Param, 1);

}

PVRSRV_ERROR PVRSRVMapDeviceClassMemoryKM(PVRSRV_PER_PROCESS_DATA * psPerProc,
					  void *hDevMemContext,
					  void *hDeviceClassBuffer,
					  PVRSRV_KERNEL_MEM_INFO ** ppsMemInfo,
					  void **phOSMapInfo)
{
	PVRSRV_ERROR eError;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	PVRSRV_DEVICECLASS_BUFFER *psDeviceClassBuffer;
	IMG_SYS_PHYADDR *psSysPAddr;
	void *pvCPUVAddr, *pvPageAlignedCPUVAddr;
	int bPhysContig;
	BM_CONTEXT *psBMContext;
	DEVICE_MEMORY_INFO *psDevMemoryInfo;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	void *hDevMemHeap = NULL;
	u32 ui32ByteSize;
	u32 ui32Offset;
	u32 ui32PageSize = HOST_PAGESIZE();
	BM_HANDLE hBuffer;
	PVRSRV_MEMBLK *psMemBlock;
	int bBMError;
	u32 i;

	if (!hDeviceClassBuffer || !ppsMemInfo || !phOSMapInfo
	    || !hDevMemContext) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: invalid parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDeviceClassBuffer = (PVRSRV_DEVICECLASS_BUFFER *) hDeviceClassBuffer;

	eError =
	    psDeviceClassBuffer->pfnGetBufferAddr(psDeviceClassBuffer->
						  hExtDevice,
						  psDeviceClassBuffer->
						  hExtBuffer, &psSysPAddr,
						  &ui32ByteSize, &pvCPUVAddr,
						  phOSMapInfo, &bPhysContig);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: unable to get buffer address"));
		return PVRSRV_ERROR_GENERIC;
	}

	psBMContext = (BM_CONTEXT *) psDeviceClassBuffer->hDevMemContext;
	psDevMemoryInfo = &psBMContext->psDeviceNode->sDevMemoryInfo;
	psDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;
	for (i = 0; i < PVRSRV_MAX_CLIENT_HEAPS; i++) {
		if (HEAP_IDX(psDeviceMemoryHeap[i].ui32HeapID) ==
		    psDevMemoryInfo->ui32MappingHeapID) {
			if (psDeviceMemoryHeap[i].DevMemHeapType ==
			    DEVICE_MEMORY_HEAP_PERCONTEXT) {

				hDevMemHeap =
				    BM_CreateHeap(hDevMemContext,
						  &psDeviceMemoryHeap[i]);
			} else {
				hDevMemHeap =
				    psDevMemoryInfo->psDeviceMemoryHeap[i].
				    hDevMemHeap;
			}
			break;
		}
	}

	if (hDevMemHeap == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: unable to find mapping heap"));
		return PVRSRV_ERROR_GENERIC;
	}

	ui32Offset = ((u32) pvCPUVAddr) & (ui32PageSize - 1);
	pvPageAlignedCPUVAddr = (void *)((u32) pvCPUVAddr - ui32Offset);

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(PVRSRV_KERNEL_MEM_INFO),
		       (void **)&psMemInfo, NULL,
		       "Kernel Memory Info") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: Failed to alloc memory for block"));
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
	}

	memset(psMemInfo, 0, sizeof(*psMemInfo));

	psMemBlock = &(psMemInfo->sMemBlk);

	bBMError = BM_Wrap(hDevMemHeap,
			   ui32ByteSize,
			   ui32Offset,
			   bPhysContig,
			   psSysPAddr,
			   pvPageAlignedCPUVAddr,
			   &psMemInfo->ui32Flags, &hBuffer);

	if (!bBMError) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: BM_Wrap Failed"));
		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(PVRSRV_KERNEL_MEM_INFO), psMemInfo, NULL);

		return PVRSRV_ERROR_BAD_MAPPING;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *)hBuffer;

	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);

	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = ui32ByteSize;
	psMemInfo->psKernelSyncInfo = psDeviceClassBuffer->psKernelSyncInfo;

	psMemInfo->pvSysBackupBuffer = NULL;

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICECLASSMEM_MAPPING, psMemInfo, 0,
			      UnmapDeviceClassMemoryCallBack);

	psMemInfo->ui32RefCount++;

	psMemInfo->memType = PVRSRV_MEMTYPE_DEVICECLASS;

	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;
}

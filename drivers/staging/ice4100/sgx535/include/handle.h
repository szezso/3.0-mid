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

#ifndef __HANDLE_H__
#define __HANDLE_H__

#if defined (__cplusplus)
extern "C" {
#endif

#include "img_types.h"
#include "hash.h"
#include "resman.h"

typedef enum
{
	PVRSRV_HANDLE_TYPE_NONE = 0,
	PVRSRV_HANDLE_TYPE_PERPROC_DATA,
	PVRSRV_HANDLE_TYPE_DEV_NODE,
	PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT,
	PVRSRV_HANDLE_TYPE_DEV_MEM_HEAP,
	PVRSRV_HANDLE_TYPE_MEM_INFO,
	PVRSRV_HANDLE_TYPE_SYNC_INFO,
	PVRSRV_HANDLE_TYPE_DISP_INFO,
	PVRSRV_HANDLE_TYPE_DISP_SWAP_CHAIN,
	PVRSRV_HANDLE_TYPE_BUF_INFO,
	PVRSRV_HANDLE_TYPE_DISP_BUFFER,
	PVRSRV_HANDLE_TYPE_BUF_BUFFER,
	PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT,
	PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT,
	PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT,
	PVRSRV_HANDLE_TYPE_SHARED_PB_DESC,
	PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
	PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO,
	PVRSRV_HANDLE_TYPE_SHARED_EVENT_OBJECT,
	PVRSRV_HANDLE_TYPE_EVENT_OBJECT_CONNECT,
	PVRSRV_HANDLE_TYPE_MMAP_INFO,
	PVRSRV_HANDLE_TYPE_SOC_TIMER
} PVRSRV_HANDLE_TYPE;

typedef enum
{

	PVRSRV_HANDLE_ALLOC_FLAG_NONE = 		0,

	PVRSRV_HANDLE_ALLOC_FLAG_SHARED = 		0x01,

	PVRSRV_HANDLE_ALLOC_FLAG_MULTI = 		0x02,

	PVRSRV_HANDLE_ALLOC_FLAG_PRIVATE = 		0x04
} PVRSRV_HANDLE_ALLOC_FLAG;

struct _PVRSRV_HANDLE_BASE_;
typedef struct _PVRSRV_HANDLE_BASE_ PVRSRV_HANDLE_BASE;

#ifdef	PVR_SECURE_HANDLES
extern PVRSRV_HANDLE_BASE *gpsKernelHandleBase;

#define	KERNEL_HANDLE_BASE (gpsKernelHandleBase)

PVRSRV_ERROR PVRSRVAllocHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType, PVRSRV_HANDLE_ALLOC_FLAG eFlag);

PVRSRV_ERROR PVRSRVAllocSubHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType, PVRSRV_HANDLE_ALLOC_FLAG eFlag, void * hParent);

PVRSRV_ERROR PVRSRVFindHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType);

PVRSRV_ERROR PVRSRVLookupHandleAnyType(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, PVRSRV_HANDLE_TYPE *peType, void * hHandle);

PVRSRV_ERROR PVRSRVLookupHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType);

PVRSRV_ERROR PVRSRVLookupSubHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType, void * hAncestor);

PVRSRV_ERROR PVRSRVGetParentHandle(PVRSRV_HANDLE_BASE *psBase, void * *phParent, void * hHandle, PVRSRV_HANDLE_TYPE eType);

PVRSRV_ERROR PVRSRVLookupAndReleaseHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType);

PVRSRV_ERROR PVRSRVReleaseHandle(PVRSRV_HANDLE_BASE *psBase, void * hHandle, PVRSRV_HANDLE_TYPE eType);

PVRSRV_ERROR PVRSRVNewHandleBatch(PVRSRV_HANDLE_BASE *psBase, u32 ui32BatchSize);

PVRSRV_ERROR PVRSRVCommitHandleBatch(PVRSRV_HANDLE_BASE *psBase);

void PVRSRVReleaseHandleBatch(PVRSRV_HANDLE_BASE *psBase);

PVRSRV_ERROR PVRSRVSetMaxHandle(PVRSRV_HANDLE_BASE *psBase, u32 ui32MaxHandle);

u32 PVRSRVGetMaxHandle(PVRSRV_HANDLE_BASE *psBase);

PVRSRV_ERROR PVRSRVEnableHandlePurging(PVRSRV_HANDLE_BASE *psBase);

PVRSRV_ERROR PVRSRVPurgeHandles(PVRSRV_HANDLE_BASE *psBase);

PVRSRV_ERROR PVRSRVAllocHandleBase(PVRSRV_HANDLE_BASE **ppsBase);

PVRSRV_ERROR PVRSRVFreeHandleBase(PVRSRV_HANDLE_BASE *psBase);

PVRSRV_ERROR PVRSRVHandleInit(void);

PVRSRV_ERROR PVRSRVHandleDeInit(void);

#else

#define KERNEL_HANDLE_BASE NULL

static
PVRSRV_ERROR PVRSRVAllocHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType, PVRSRV_HANDLE_ALLOC_FLAG eFlag)
{
	*phHandle = pvData;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVAllocSubHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType, PVRSRV_HANDLE_ALLOC_FLAG eFlag, void * hParent)
{
	*phHandle = pvData;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVFindHandle(PVRSRV_HANDLE_BASE *psBase, void * *phHandle, void *pvData, PVRSRV_HANDLE_TYPE eType)
{
	*phHandle = pvData;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVLookupHandleAnyType(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, PVRSRV_HANDLE_TYPE *peType, void * hHandle)
{
	*peType = PVRSRV_HANDLE_TYPE_NONE;

	*ppvData = hHandle;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVLookupHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType)
{
	*ppvData = hHandle;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVLookupSubHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType, void * hAncestor)
{

	*ppvData = hHandle;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVGetParentHandle(PVRSRV_HANDLE_BASE *psBase, void * *phParent, void * hHandle, PVRSRV_HANDLE_TYPE eType)
{
	*phParent = NULL;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVLookupAndReleaseHandle(PVRSRV_HANDLE_BASE *psBase, void * *ppvData, void * hHandle, PVRSRV_HANDLE_TYPE eType)
{
	*ppvData = hHandle;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVReleaseHandle(PVRSRV_HANDLE_BASE *psBase, void * hHandle, PVRSRV_HANDLE_TYPE eType)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVNewHandleBatch(PVRSRV_HANDLE_BASE *psBase, u32 ui32BatchSize)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVCommitHandleBatch(PVRSRV_HANDLE_BASE *psBase)
{
	return PVRSRV_OK;
}

static
void PVRSRVReleaseHandleBatch(PVRSRV_HANDLE_BASE *psBase)
{
}

static
PVRSRV_ERROR PVRSRVSetMaxHandle(PVRSRV_HANDLE_BASE *psBase, u32 ui32MaxHandle)
{

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static
u32 PVRSRVGetMaxHandle(PVRSRV_HANDLE_BASE *psBase)
{
	return 0;
}

static
PVRSRV_ERROR PVRSRVEnableHandlePurging(PVRSRV_HANDLE_BASE *psBase)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVPurgeHandles(PVRSRV_HANDLE_BASE *psBase)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVAllocHandleBase(PVRSRV_HANDLE_BASE **ppsBase)
{
	*ppsBase = NULL;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVFreeHandleBase(PVRSRV_HANDLE_BASE *psBase)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVHandleInit(void)
{
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVHandleDeInit(void)
{
	return PVRSRV_OK;
}

#endif

#define PVRSRVAllocHandleNR(psBase, phHandle, pvData, eType, eFlag) \
	(void)PVRSRVAllocHandle(psBase, phHandle, pvData, eType, eFlag)

#define PVRSRVAllocSubHandleNR(psBase, phHandle, pvData, eType, eFlag, hParent) \
	(void)PVRSRVAllocSubHandle(psBase, phHandle, pvData, eType, eFlag, hParent)


#endif


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

#ifndef _MMU_H_
#define _MMU_H_

#include "sgxinfokm.h"

PVRSRV_ERROR
MMU_Initialise (PVRSRV_DEVICE_NODE *psDeviceNode, MMU_CONTEXT **ppsMMUContext, IMG_DEV_PHYADDR *psPDDevPAddr);

void
MMU_Finalise (MMU_CONTEXT *psMMUContext);


void
MMU_InsertHeap(MMU_CONTEXT *psMMUContext, MMU_HEAP *psMMUHeap);

MMU_HEAP *
MMU_Create (MMU_CONTEXT *psMMUContext,
			DEV_ARENA_DESCRIPTOR *psDevArena,
			RA_ARENA **ppsVMArena);

void
MMU_Delete (MMU_HEAP *pMMU);

int
MMU_Alloc (MMU_HEAP *pMMU,
           u32 uSize,
           u32 *pActualSize,
           u32 uFlags,
		   u32 uDevVAddrAlignment,
           IMG_DEV_VIRTADDR *pDevVAddr);

void
MMU_Free (MMU_HEAP *pMMU,
          IMG_DEV_VIRTADDR DevVAddr,
		  u32 ui32Size);

void
MMU_Enable (MMU_HEAP *pMMU);

void
MMU_Disable (MMU_HEAP *pMMU);

void
MMU_MapPages (MMU_HEAP *pMMU,
			  IMG_DEV_VIRTADDR devVAddr,
			  IMG_SYS_PHYADDR SysPAddr,
			  u32 uSize,
			  u32 ui32MemFlags,
			  void * hUniqueTag);

void
MMU_MapShadow (MMU_HEAP          * pMMU,
               IMG_DEV_VIRTADDR    MapBaseDevVAddr,
               u32          uSize,
               IMG_CPU_VIRTADDR    CpuVAddr,
               void *          hOSMemHandle,
               IMG_DEV_VIRTADDR  * pDevVAddr,
               u32          ui32MemFlags,
               void *          hUniqueTag);

void
MMU_UnmapPages (MMU_HEAP *pMMU,
             IMG_DEV_VIRTADDR dev_vaddr,
             u32 ui32PageCount,
             void * hUniqueTag);

void
MMU_MapScatter (MMU_HEAP *pMMU,
				IMG_DEV_VIRTADDR DevVAddr,
				IMG_SYS_PHYADDR *psSysAddr,
				u32 uSize,
				u32 ui32MemFlags,
				void * hUniqueTag);


IMG_DEV_PHYADDR
MMU_GetPhysPageAddr(MMU_HEAP *pMMUHeap, IMG_DEV_VIRTADDR sDevVPageAddr);


IMG_DEV_PHYADDR
MMU_GetPDDevPAddr(MMU_CONTEXT *pMMUContext);


#ifdef SUPPORT_SGX_MMU_BYPASS
void
EnableHostAccess (MMU_CONTEXT *psMMUContext);


void
DisableHostAccess (MMU_CONTEXT *psMMUContext);
#endif

void MMU_InvalidateDirectoryCache(PVRSRV_SGXDEV_INFO *psDevInfo);

PVRSRV_ERROR MMU_BIFResetPDAlloc(PVRSRV_SGXDEV_INFO *psDevInfo);

void MMU_BIFResetPDFree(PVRSRV_SGXDEV_INFO *psDevInfo);

#if defined(FIX_HW_BRN_22997) && defined(FIX_HW_BRN_23030) && defined(SGX_FEATURE_HOST_PORT)
PVRSRV_ERROR WorkaroundBRN22997Alloc(PVRSRV_SGXDEV_INFO *psDevInfo);

void WorkaroundBRN22997ReadHostPort(PVRSRV_SGXDEV_INFO *psDevInfo);

void WorkaroundBRN22997Free(PVRSRV_SGXDEV_INFO *psDevInfo);
#endif

#if defined(SUPPORT_EXTERNAL_SYSTEM_CACHE)
PVRSRV_ERROR MMU_MapExtSystemCacheRegs(PVRSRV_DEVICE_NODE *psDeviceNode);

PVRSRV_ERROR MMU_UnmapExtSystemCacheRegs(PVRSRV_DEVICE_NODE *psDeviceNode);
#endif

#endif

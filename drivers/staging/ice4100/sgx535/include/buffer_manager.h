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

#ifndef _BUFFER_MANAGER_H_
#define _BUFFER_MANAGER_H_

#include "img_types.h"
#include "ra.h"
#include "perproc.h"

#if defined(__cplusplus)
extern "C"{
#endif

typedef struct _BM_HEAP_ BM_HEAP;

struct _BM_MAPPING_
{
	enum
	{
		hm_wrapped = 1,
		hm_wrapped_scatter,
		hm_wrapped_virtaddr,
		hm_wrapped_scatter_virtaddr,
		hm_env,
		hm_contiguous
	} eCpuMemoryOrigin;

	BM_HEAP				*pBMHeap;
	RA_ARENA			*pArena;

	IMG_CPU_VIRTADDR	CpuVAddr;
	IMG_CPU_PHYADDR		CpuPAddr;
	IMG_DEV_VIRTADDR	DevVAddr;
	IMG_SYS_PHYADDR		*psSysAddr;
	u32			uSize;
    void *          hOSMemHandle;
	u32			ui32Flags;
};

typedef struct _BM_BUF_
{
	IMG_CPU_VIRTADDR	*CpuVAddr;
    void            *hOSMemHandle;
	IMG_CPU_PHYADDR		CpuPAddr;
	IMG_DEV_VIRTADDR	DevVAddr;

	BM_MAPPING			*pMapping;
	u32			ui32RefCount;
	u32			ui32ExportCount;
} BM_BUF;

struct _BM_HEAP_
{
	u32				ui32Attribs;
	BM_CONTEXT				*pBMContext;
	RA_ARENA				*pImportArena;
	RA_ARENA				*pLocalDevMemArena;
	RA_ARENA				*pVMArena;
	DEV_ARENA_DESCRIPTOR	sDevArena;
	MMU_HEAP				*pMMUHeap;

	struct _BM_HEAP_ 		*psNext;
	struct _BM_HEAP_ 		**ppsThis;
};

struct _BM_CONTEXT_
{
	MMU_CONTEXT	*psMMUContext;


	 BM_HEAP *psBMHeap;


	 BM_HEAP *psBMSharedHeap;

	PVRSRV_DEVICE_NODE *psDeviceNode;


	HASH_TABLE *pBufferHash;


	void * hResItem;

	u32 ui32RefCount;



	struct _BM_CONTEXT_ *psNext;
	struct _BM_CONTEXT_ **ppsThis;
};



typedef void *BM_HANDLE;

#define BP_POOL_MASK         0x7

#define BP_CONTIGUOUS			(1 << 3)
#define BP_PARAMBUFFER			(1 << 4)

#define BM_MAX_DEVMEM_ARENAS  2

void *
BM_CreateContext(PVRSRV_DEVICE_NODE			*psDeviceNode,
				 IMG_DEV_PHYADDR			*psPDDevPAddr,
				 PVRSRV_PER_PROCESS_DATA	*psPerProc,
				 int					*pbCreated);


PVRSRV_ERROR
BM_DestroyContext (void * hBMContext,
					int *pbCreated);


void *
BM_CreateHeap (void * hBMContext,
				DEVICE_MEMORY_HEAP_INFO *psDevMemHeapInfo);

void
BM_DestroyHeap (void * hDevMemHeap);


int
BM_Reinitialise (PVRSRV_DEVICE_NODE *psDeviceNode);

int
BM_Alloc (void *			hDevMemHeap,
			IMG_DEV_VIRTADDR	*psDevVAddr,
			u32			uSize,
			u32			*pui32Flags,
			u32			uDevVAddrAlignment,
			BM_HANDLE			*phBuf);

int
BM_Wrap (	void * hDevMemHeap,
		    u32 ui32Size,
			u32 ui32Offset,
			int bPhysContig,
			IMG_SYS_PHYADDR *psSysAddr,
			void *pvCPUVAddr,
			u32 *pui32Flags,
			BM_HANDLE *phBuf);

void
BM_Free (BM_HANDLE hBuf,
		u32 ui32Flags);


IMG_CPU_VIRTADDR
BM_HandleToCpuVaddr (BM_HANDLE hBuf);

IMG_DEV_VIRTADDR
BM_HandleToDevVaddr (BM_HANDLE hBuf);

IMG_SYS_PHYADDR
BM_HandleToSysPaddr (BM_HANDLE hBuf);

void *
BM_HandleToOSMemHandle (BM_HANDLE hBuf);

int
BM_ContiguousStatistics (u32 uFlags,
                         u32 *pTotalBytes,
                         u32 *pAvailableBytes);


void BM_GetPhysPageAddr(PVRSRV_KERNEL_MEM_INFO *psMemInfo,
								IMG_DEV_VIRTADDR sDevVPageAddr,
								IMG_DEV_PHYADDR *psDevPAddr);

PVRSRV_ERROR BM_GetHeapInfo(void * hDevMemHeap,
							PVRSRV_HEAP_INFO *psHeapInfo);

MMU_CONTEXT* BM_GetMMUContext(void * hDevMemHeap);

MMU_CONTEXT* BM_GetMMUContextFromMemContext(void * hDevMemContext);

void * BM_GetMMUHeap(void * hDevMemHeap);

PVRSRV_DEVICE_NODE* BM_GetDeviceNode(void * hDevMemContext);


void * BM_GetMappingHandle(PVRSRV_KERNEL_MEM_INFO *psMemInfo);

void BM_Export(BM_HANDLE hBuf);

void BM_FreeExport(BM_HANDLE hBuf, u32 ui32Flags);

#if defined(__cplusplus)
}
#endif

#endif


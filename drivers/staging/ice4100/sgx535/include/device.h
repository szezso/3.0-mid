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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "ra.h"
#include "resman.h"

typedef struct _BM_CONTEXT_ BM_CONTEXT;

typedef struct _MMU_HEAP_ MMU_HEAP;
typedef struct _MMU_CONTEXT_ MMU_CONTEXT;

#define PVRSRV_BACKINGSTORE_SYSMEM_CONTIG		(1<<(PVRSRV_MEM_BACKINGSTORE_FIELD_SHIFT+0))
#define PVRSRV_BACKINGSTORE_SYSMEM_NONCONTIG	(1<<(PVRSRV_MEM_BACKINGSTORE_FIELD_SHIFT+1))
#define PVRSRV_BACKINGSTORE_LOCALMEM_CONTIG		(1<<(PVRSRV_MEM_BACKINGSTORE_FIELD_SHIFT+2))
#define PVRSRV_BACKINGSTORE_LOCALMEM_NONCONTIG	(1<<(PVRSRV_MEM_BACKINGSTORE_FIELD_SHIFT+3))

typedef u32 DEVICE_MEMORY_HEAP_TYPE;
#define DEVICE_MEMORY_HEAP_PERCONTEXT		0
#define DEVICE_MEMORY_HEAP_KERNEL			1
#define DEVICE_MEMORY_HEAP_SHARED			2
#define DEVICE_MEMORY_HEAP_SHARED_EXPORTED	3

#define PVRSRV_DEVICE_NODE_FLAGS_PORT80DISPLAY	1
#define PVRSRV_DEVICE_NODE_FLAGS_MMU_OPT_INV	2

typedef struct _DEVICE_MEMORY_HEAP_INFO_
{

	u32				ui32HeapID;


	char				*pszName;


	char				*pszBSName;


	IMG_DEV_VIRTADDR		sDevVAddrBase;


	u32				ui32HeapSize;


	u32				ui32Attribs;


	DEVICE_MEMORY_HEAP_TYPE	DevMemHeapType;


	void *				hDevMemHeap;


	RA_ARENA				*psLocalDevMemArena;


	u32				ui32DataPageSize;

} DEVICE_MEMORY_HEAP_INFO;

typedef struct _DEVICE_MEMORY_INFO_
{

	u32				ui32AddressSpaceSizeLog2;




	u32				ui32Flags;


	u32				ui32HeapCount;


	u32				ui32SyncHeapID;


	u32				ui32MappingHeapID;


	DEVICE_MEMORY_HEAP_INFO	*psDeviceMemoryHeap;


    BM_CONTEXT				*pBMKernelContext;


    BM_CONTEXT				*pBMContext;

} DEVICE_MEMORY_INFO;


typedef struct DEV_ARENA_DESCRIPTOR_TAG
{
	u32				ui32HeapID;

	char				*pszName;

	IMG_DEV_VIRTADDR		BaseDevVAddr;

	u32 				ui32Size;

	DEVICE_MEMORY_HEAP_TYPE	DevMemHeapType;


	u32				ui32DataPageSize;

	DEVICE_MEMORY_HEAP_INFO	*psDeviceMemoryHeapInfo;

} DEV_ARENA_DESCRIPTOR;

typedef struct _SYS_DATA_TAG_ *PSYS_DATA;

typedef struct _PVRSRV_DEVICE_NODE_
{
	PVRSRV_DEVICE_IDENTIFIER	sDevId;
	u32					ui32RefCount;




	PVRSRV_ERROR			(*pfnInitDevice) (void*);

	PVRSRV_ERROR			(*pfnDeInitDevice) (void*);


	PVRSRV_ERROR			(*pfnInitDeviceCompatCheck) (struct _PVRSRV_DEVICE_NODE_*);


	PVRSRV_ERROR			(*pfnMMUInitialise)(struct _PVRSRV_DEVICE_NODE_*, MMU_CONTEXT**, IMG_DEV_PHYADDR*);
	void				(*pfnMMUFinalise)(MMU_CONTEXT*);
	void				(*pfnMMUInsertHeap)(MMU_CONTEXT*, MMU_HEAP*);
	MMU_HEAP*				(*pfnMMUCreate)(MMU_CONTEXT*,DEV_ARENA_DESCRIPTOR*,RA_ARENA**);
	void				(*pfnMMUDelete)(MMU_HEAP*);
	int				(*pfnMMUAlloc)(MMU_HEAP*pMMU,
										   u32 uSize,
										   u32 *pActualSize,
										   u32 uFlags,
										   u32 uDevVAddrAlignment,
										   IMG_DEV_VIRTADDR *pDevVAddr);
	void				(*pfnMMUFree)(MMU_HEAP*,IMG_DEV_VIRTADDR,u32);
	void 				(*pfnMMUEnable)(MMU_HEAP*);
	void				(*pfnMMUDisable)(MMU_HEAP*);
	void				(*pfnMMUMapPages)(MMU_HEAP *pMMU,
											  IMG_DEV_VIRTADDR devVAddr,
											  IMG_SYS_PHYADDR SysPAddr,
											  u32 uSize,
											  u32 ui32MemFlags,
											  void * hUniqueTag);
	void				(*pfnMMUMapShadow)(MMU_HEAP            *pMMU,
											   IMG_DEV_VIRTADDR    MapBaseDevVAddr,
											   u32          uSize,
											   IMG_CPU_VIRTADDR    CpuVAddr,
											   void *          hOSMemHandle,
											   IMG_DEV_VIRTADDR    *pDevVAddr,
											   u32 ui32MemFlags,
											   void * hUniqueTag);
	void				(*pfnMMUUnmapPages)(MMU_HEAP *pMMU,
												IMG_DEV_VIRTADDR dev_vaddr,
												u32 ui32PageCount,
												void * hUniqueTag);

	void				(*pfnMMUMapScatter)(MMU_HEAP *pMMU,
												IMG_DEV_VIRTADDR DevVAddr,
												IMG_SYS_PHYADDR *psSysAddr,
												u32 uSize,
												u32 ui32MemFlags,
												void * hUniqueTag);

	IMG_DEV_PHYADDR			(*pfnMMUGetPhysPageAddr)(MMU_HEAP *pMMUHeap, IMG_DEV_VIRTADDR sDevVPageAddr);
	IMG_DEV_PHYADDR			(*pfnMMUGetPDDevPAddr)(MMU_CONTEXT *pMMUContext);


	int				(*pfnDeviceISR)(void*);

	void				*pvISRData;

	u32 				ui32SOCInterruptBit;

	void				(*pfnDeviceMISR)(void*);


	void				(*pfnDeviceCommandComplete)(struct _PVRSRV_DEVICE_NODE_ *psDeviceNode);

	int				bReProcessDeviceCommandComplete;


	DEVICE_MEMORY_INFO		sDevMemoryInfo;


	void				*pvDevice;
	u32				ui32pvDeviceSize;


	PRESMAN_CONTEXT			hResManContext;


	PSYS_DATA				psSysData;


	RA_ARENA				*psLocalDevMemArena;

	u32				ui32Flags;

	struct _PVRSRV_DEVICE_NODE_	*psNext;
	struct _PVRSRV_DEVICE_NODE_	**ppsThis;
} PVRSRV_DEVICE_NODE;

PVRSRV_ERROR  PVRSRVRegisterDevice(PSYS_DATA psSysData,
											  PVRSRV_ERROR (*pfnRegisterDevice)(PVRSRV_DEVICE_NODE*),
											  u32 ui32SOCInterruptBit,
			 								  u32 *pui32DeviceIndex );

PVRSRV_ERROR  PVRSRVInitialiseDevice(u32 ui32DevIndex);
PVRSRV_ERROR  PVRSRVFinaliseSystem(int bInitSuccesful);

PVRSRV_ERROR  PVRSRVDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode);

PVRSRV_ERROR  PVRSRVDeinitialiseDevice(u32 ui32DevIndex);

#if !defined(USE_CODE)

PVRSRV_ERROR  PollForValueKM(volatile u32* pui32LinMemAddr,
												   u32 ui32Value,
												   u32 ui32Mask,
												   u32 ui32Waitus,
												   u32 ui32Tries);

#endif


#if defined (USING_ISR_INTERRUPTS)
PVRSRV_ERROR  PollForInterruptKM(u32 ui32Value,
								u32 ui32Mask,
								u32 ui32Waitus,
								u32 ui32Tries);
#endif

PVRSRV_ERROR  PVRSRVInit(PSYS_DATA psSysData);
void  PVRSRVDeInit(PSYS_DATA psSysData);
int  PVRSRVDeviceLISR(PVRSRV_DEVICE_NODE *psDeviceNode);
int  PVRSRVSystemLISR(void *pvSysData);
void  PVRSRVMISR(void *pvSysData);


#endif


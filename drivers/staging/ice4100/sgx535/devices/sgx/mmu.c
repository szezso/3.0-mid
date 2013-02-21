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

#include "sgxdefs.h"
#include "sgxmmu.h"
#include "services_headers.h"
#include "buffer_manager.h"
#include "hash.h"
#include "ra.h"
#include "pdump_km.h"
#include "sgxapi_km.h"
#include "sgxinfo.h"
#include "sgxinfokm.h"
#include "mmu.h"
#include "sgxconfig.h"

#define UINT32_MAX_VALUE	0xFFFFFFFFUL

#define SGX_MAX_PD_ENTRIES	(1<<(SGX_FEATURE_ADDRESS_SPACE_SIZE - SGX_MMU_PT_SHIFT - SGX_MMU_PAGE_SHIFT))

typedef struct _MMU_PT_INFO_ {

	void *hPTPageOSMemHandle;
	IMG_CPU_VIRTADDR PTPageCpuVAddr;
	u32 ui32ValidPTECount;
} MMU_PT_INFO;

struct _MMU_CONTEXT_ {

	PVRSRV_DEVICE_NODE *psDeviceNode;

	IMG_CPU_VIRTADDR pvPDCpuVAddr;
	IMG_DEV_PHYADDR sPDDevPAddr;

	void *hPDOSMemHandle;

	MMU_PT_INFO *apsPTInfoList[SGX_MAX_PD_ENTRIES];

	PVRSRV_SGXDEV_INFO *psDevInfo;

#if defined(PDUMP)
	u32 ui32PDumpMMUContextID;
#endif

	struct _MMU_CONTEXT_ *psNext;
};

struct _MMU_HEAP_ {

	MMU_CONTEXT *psMMUContext;

	u32 ui32PDBaseIndex;

	u32 ui32PageTableCount;

	u32 ui32PTETotal;

	u32 ui32PDEPageSizeCtrl;

	u32 ui32DataPageSize;

	u32 ui32DataPageBitWidth;

	u32 ui32DataPageMask;

	u32 ui32PTShift;

	u32 ui32PTBitWidth;

	u32 ui32PTMask;

	u32 ui32PTSize;

	u32 ui32PTECount;

	u32 ui32PDShift;

	u32 ui32PDBitWidth;

	u32 ui32PDMask;

	RA_ARENA *psVMArena;
	DEV_ARENA_DESCRIPTOR *psDevArena;
};

#if defined (SUPPORT_SGX_MMU_DUMMY_PAGE)
#define DUMMY_DATA_PAGE_SIGNATURE	0xDEADBEEF
#endif

#if defined(PDUMP)
static void
MMU_PDumpPageTables(MMU_HEAP * pMMUHeap,
		    IMG_DEV_VIRTADDR DevVAddr,
		    u32 uSize, int bForUnmap, void *hUniqueTag);
#endif

#define PAGE_TEST					0
#if PAGE_TEST
static void PageTest(void *pMem, IMG_DEV_PHYADDR sDevPAddr);
#endif

#define PT_DEBUG 0
#if PT_DEBUG
static void DumpPT(MMU_PT_INFO * psPTInfoList)
{
	u32 *p = (u32 *) psPTInfoList->PTPageCpuVAddr;
	u32 i;

	for (i = 0; i < 1024; i += 8) {
		PVR_DPF((PVR_DBG_WARNING,
			 "%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx %.8lx\n",
			 p[i + 0], p[i + 1], p[i + 2], p[i + 3],
			 p[i + 4], p[i + 5], p[i + 6], p[i + 7]));
	}
}

static void CheckPT(MMU_PT_INFO * psPTInfoList)
{
	u32 *p = (u32 *) psPTInfoList->PTPageCpuVAddr;
	u32 i, ui32Count = 0;

	for (i = 0; i < 1024; i++)
		if (p[i] & SGX_MMU_PTE_VALID)
			ui32Count++;

	if (psPTInfoList->ui32ValidPTECount != ui32Count) {
		PVR_DPF((PVR_DBG_WARNING,
			 "ui32ValidPTECount: %lu ui32Count: %lu\n",
			 psPTInfoList->ui32ValidPTECount, ui32Count));
		DumpPT(psPTInfoList);
		BUG();
	}
}
#else
/* FIXME MLD compiler warning temporary fix */
/* static void DumpPT(MMU_PT_INFO * psPTInfoList)
{
}
*/
static void CheckPT(MMU_PT_INFO * psPTInfoList)
{
}
#endif

#ifdef SUPPORT_SGX_MMU_BYPASS
void EnableHostAccess(MMU_CONTEXT * psMMUContext)
{
	u32 ui32RegVal;
	void *pvRegsBaseKM = psMMUContext->psDevInfo->pvRegsBaseKM;

	ui32RegVal = OSReadHWReg(pvRegsBaseKM, EUR_CR_BIF_CTRL);

	OSWriteHWReg(pvRegsBaseKM,
		     EUR_CR_BIF_CTRL,
		     ui32RegVal | EUR_CR_BIF_CTRL_MMU_BYPASS_HOST_MASK);

	PDUMPREG(EUR_CR_BIF_CTRL, EUR_CR_BIF_CTRL_MMU_BYPASS_HOST_MASK);
}

void DisableHostAccess(MMU_CONTEXT * psMMUContext)
{
	u32 ui32RegVal;
	void *pvRegsBaseKM = psMMUContext->psDevInfo->pvRegsBaseKM;

	OSWriteHWReg(pvRegsBaseKM,
		     EUR_CR_BIF_CTRL,
		     ui32RegVal & ~EUR_CR_BIF_CTRL_MMU_BYPASS_HOST_MASK);

	PDUMPREG(EUR_CR_BIF_CTRL, 0);
}
#endif

void MMU_InvalidateSystemLevelCache(PVRSRV_SGXDEV_INFO * psDevInfo)
{
#if defined(SGX_FEATURE_MP)
	psDevInfo->ui32CacheControl |= SGX_BIF_INVALIDATE_SLCACHE;
#endif
}

void MMU_InvalidateDirectoryCache(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	psDevInfo->ui32CacheControl |= SGX_BIF_INVALIDATE_PDCACHE;
#if defined(SGX_FEATURE_SYSTEM_CACHE)
	MMU_InvalidateSystemLevelCache(psDevInfo);
#endif
}

void MMU_InvalidatePageTableCache(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	psDevInfo->ui32CacheControl |= SGX_BIF_INVALIDATE_PTCACHE;
#if defined(SGX_FEATURE_SYSTEM_CACHE)
	MMU_InvalidateSystemLevelCache(psDevInfo);
#endif
}

static int
_AllocPageTableMemory(MMU_HEAP * pMMUHeap,
		      MMU_PT_INFO * psPTInfoList, IMG_DEV_PHYADDR * psDevPAddr)
{
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_CPU_PHYADDR sCpuPAddr;

	if (pMMUHeap->psDevArena->psDeviceMemoryHeapInfo->psLocalDevMemArena ==
	    NULL) {

		if (OSAllocPages
		    (PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
		     pMMUHeap->ui32PTSize, SGX_MMU_PAGE_SIZE,
		     (void **)&psPTInfoList->PTPageCpuVAddr,
		     &psPTInfoList->hPTPageOSMemHandle) != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "_AllocPageTableMemory: ERROR call to OSAllocPages failed"));
			return 0;
		}

		if (psPTInfoList->PTPageCpuVAddr) {
			sCpuPAddr =
			    OSMapLinToCPUPhys(psPTInfoList->PTPageCpuVAddr);
		} else {

			sCpuPAddr =
			    OSMemHandleToCpuPAddr(psPTInfoList->
						  hPTPageOSMemHandle, 0);
		}

		sDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);
	} else {
		IMG_SYS_PHYADDR sSysPAddr;

		if (RA_Alloc
		    (pMMUHeap->psDevArena->psDeviceMemoryHeapInfo->
		     psLocalDevMemArena, SGX_MMU_PAGE_SIZE, NULL, NULL, 0,
		     SGX_MMU_PAGE_SIZE, 0, &(sSysPAddr.uiAddr)) != 1) {
			PVR_DPF((PVR_DBG_ERROR,
				 "_AllocPageTableMemory: ERROR call to RA_Alloc failed"));
			return 0;
		}

		sCpuPAddr = SysSysPAddrToCpuPAddr(sSysPAddr);

		psPTInfoList->PTPageCpuVAddr = OSMapPhysToLin(sCpuPAddr,
							      SGX_MMU_PAGE_SIZE,
							      PVRSRV_HAP_WRITECOMBINE
							      |
							      PVRSRV_HAP_KERNEL_ONLY,
							      &psPTInfoList->
							      hPTPageOSMemHandle);
		if (!psPTInfoList->PTPageCpuVAddr) {
			PVR_DPF((PVR_DBG_ERROR,
				 "_AllocPageTableMemory: ERROR failed to map page tables"));
			return 0;
		}

		sDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

#if PAGE_TEST
		PageTest(psPTInfoList->PTPageCpuVAddr, sDevPAddr);
#endif
	}

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)
	{
		u32 *pui32Tmp;
		u32 i;

		pui32Tmp = (u32 *) psPTInfoList->PTPageCpuVAddr;

		for (i = 0; i < pMMUHeap->ui32PTECount; i++) {
			pui32Tmp[i] =
			    (pMMUHeap->psMMUContext->psDevInfo->
			     sDummyDataDevPAddr.
			     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
			    | SGX_MMU_PTE_VALID;
		}
	}
#else

	memset(psPTInfoList->PTPageCpuVAddr, 0, pMMUHeap->ui32PTSize);
#endif

	PDUMPMALLOCPAGETABLE(PVRSRV_DEVICE_TYPE_SGX,
			     psPTInfoList->PTPageCpuVAddr, pMMUHeap->ui32PTSize,
			     PDUMP_PT_UNIQUETAG);

	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, psPTInfoList->PTPageCpuVAddr,
		  pMMUHeap->ui32PTSize, 0, 1, PDUMP_PT_UNIQUETAG,
		  PDUMP_PT_UNIQUETAG);

	*psDevPAddr = sDevPAddr;

	return 1;
}

static void
_FreePageTableMemory(MMU_HEAP * pMMUHeap, MMU_PT_INFO * psPTInfoList)
{

	if (pMMUHeap->psDevArena->psDeviceMemoryHeapInfo->psLocalDevMemArena ==
	    NULL) {

		OSFreePages(PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
			    pMMUHeap->ui32PTSize,
			    psPTInfoList->PTPageCpuVAddr,
			    psPTInfoList->hPTPageOSMemHandle);
	} else {
		IMG_SYS_PHYADDR sSysPAddr;
		IMG_CPU_PHYADDR sCpuPAddr;

		sCpuPAddr = OSMapLinToCPUPhys(psPTInfoList->PTPageCpuVAddr);
		sSysPAddr = SysCpuPAddrToSysPAddr(sCpuPAddr);

		OSUnMapPhysToLin(psPTInfoList->PTPageCpuVAddr,
				 SGX_MMU_PAGE_SIZE,
				 PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY,
				 psPTInfoList->hPTPageOSMemHandle);

		RA_Free(pMMUHeap->psDevArena->psDeviceMemoryHeapInfo->
			psLocalDevMemArena, sSysPAddr.uiAddr, 0);
	}
}

static void
_DeferredFreePageTable(MMU_HEAP * pMMUHeap, u32 ui32PTIndex, int bOSFreePT)
{
	u32 *pui32PDEntry;
	u32 i;
	u32 ui32PDIndex;
	SYS_DATA *psSysData;
	MMU_PT_INFO **ppsPTInfoList;

	SysAcquireData(&psSysData);

	ui32PDIndex =
	    pMMUHeap->psDevArena->BaseDevVAddr.uiAddr >> pMMUHeap->ui32PDShift;

	ppsPTInfoList = &pMMUHeap->psMMUContext->apsPTInfoList[ui32PDIndex];

	{
#if PT_DEBUG
		if (ppsPTInfoList[ui32PTIndex]
		    && ppsPTInfoList[ui32PTIndex]->ui32ValidPTECount > 0) {
			DumpPT(ppsPTInfoList[ui32PTIndex]);

		}
#endif

		PVR_ASSERT(ppsPTInfoList[ui32PTIndex] == NULL
			   || ppsPTInfoList[ui32PTIndex]->ui32ValidPTECount ==
			   0);
	}

	PDUMPCOMMENT("Free page table (page count == %08X)",
		     pMMUHeap->ui32PageTableCount);
	if (ppsPTInfoList[ui32PTIndex]
	    && ppsPTInfoList[ui32PTIndex]->PTPageCpuVAddr) {
		PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX,
				   ppsPTInfoList[ui32PTIndex]->PTPageCpuVAddr,
				   pMMUHeap->ui32PTSize, PDUMP_PT_UNIQUETAG);
	}

	switch (pMMUHeap->psDevArena->DevMemHeapType) {
	case DEVICE_MEMORY_HEAP_SHARED:
	case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
		{

			MMU_CONTEXT *psMMUContext =
			    (MMU_CONTEXT *) pMMUHeap->psMMUContext->psDevInfo->
			    pvMMUContextList;

			while (psMMUContext) {

				pui32PDEntry =
				    (u32 *) psMMUContext->pvPDCpuVAddr;
				pui32PDEntry += ui32PDIndex;

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

				pui32PDEntry[ui32PTIndex] =
				    (psMMUContext->psDevInfo->sDummyPTDevPAddr.
				     uiAddr >> SGX_MMU_PDE_ADDR_ALIGNSHIFT)
				    | SGX_MMU_PDE_PAGE_SIZE_4K |
				    SGX_MMU_PDE_VALID;
#else

				if (bOSFreePT) {
					pui32PDEntry[ui32PTIndex] = 0;
				}
#endif

				PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
					  (void *)&pui32PDEntry[ui32PTIndex],
					  sizeof(u32), 0, 0, PDUMP_PT_UNIQUETAG,
					  PDUMP_PT_UNIQUETAG);

				psMMUContext = psMMUContext->psNext;
			}
			break;
		}
	case DEVICE_MEMORY_HEAP_PERCONTEXT:
	case DEVICE_MEMORY_HEAP_KERNEL:
		{

			pui32PDEntry =
			    (u32 *) pMMUHeap->psMMUContext->pvPDCpuVAddr;
			pui32PDEntry += ui32PDIndex;

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

			pui32PDEntry[ui32PTIndex] =
			    (pMMUHeap->psMMUContext->psDevInfo->
			     sDummyPTDevPAddr.
			     uiAddr >> SGX_MMU_PDE_ADDR_ALIGNSHIFT)
			    | SGX_MMU_PDE_PAGE_SIZE_4K | SGX_MMU_PDE_VALID;
#else

			if (bOSFreePT) {
				pui32PDEntry[ui32PTIndex] = 0;
			}
#endif

			PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
				  (void *)&pui32PDEntry[ui32PTIndex],
				  sizeof(u32), 0, 0, PDUMP_PD_UNIQUETAG,
				  PDUMP_PT_UNIQUETAG);
			break;
		}
	default:
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "_DeferredFreePagetable: ERROR invalid heap type"));
			return;
		}
	}

	if (ppsPTInfoList[ui32PTIndex] != NULL) {
		if (ppsPTInfoList[ui32PTIndex]->PTPageCpuVAddr != NULL) {
			u32 *pui32Tmp;

			pui32Tmp =
			    (u32 *) ppsPTInfoList[ui32PTIndex]->PTPageCpuVAddr;

			for (i = 0;
			     (i < pMMUHeap->ui32PTETotal)
			     && (i < pMMUHeap->ui32PTECount); i++) {
				pui32Tmp[i] = 0;
			}

			if (bOSFreePT) {
				_FreePageTableMemory(pMMUHeap,
						     ppsPTInfoList
						     [ui32PTIndex]);
			}

			pMMUHeap->ui32PTETotal -= i;
		} else {

			pMMUHeap->ui32PTETotal -= pMMUHeap->ui32PTECount;
		}

		if (bOSFreePT) {

			OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  sizeof(MMU_PT_INFO),
				  ppsPTInfoList[ui32PTIndex], NULL);
			ppsPTInfoList[ui32PTIndex] = NULL;
		}
	} else {

		pMMUHeap->ui32PTETotal -= pMMUHeap->ui32PTECount;
	}

	PDUMPCOMMENT("Finished free page table (page count == %08X)",
		     pMMUHeap->ui32PageTableCount);
}

static void _DeferredFreePageTables(MMU_HEAP * pMMUHeap)
{
	u32 i;

	for (i = 0; i < pMMUHeap->ui32PageTableCount; i++) {
		_DeferredFreePageTable(pMMUHeap, i, 1);
	}
	MMU_InvalidateDirectoryCache(pMMUHeap->psMMUContext->psDevInfo);
}

static int
_DeferredAllocPagetables(MMU_HEAP * pMMUHeap, IMG_DEV_VIRTADDR DevVAddr,
			 u32 ui32Size)
{
	u32 ui32PageTableCount;
	u32 ui32PDIndex;
	u32 i;
	u32 *pui32PDEntry;
	MMU_PT_INFO **ppsPTInfoList;
	SYS_DATA *psSysData;
	IMG_DEV_VIRTADDR sHighDevVAddr;

#if SGX_FEATURE_ADDRESS_SPACE_SIZE < 32
	PVR_ASSERT(DevVAddr.uiAddr < (1 << SGX_FEATURE_ADDRESS_SPACE_SIZE));
#endif

	SysAcquireData(&psSysData);

	ui32PDIndex = DevVAddr.uiAddr >> pMMUHeap->ui32PDShift;

	if ((UINT32_MAX_VALUE - DevVAddr.uiAddr)
	    < (ui32Size + pMMUHeap->ui32DataPageMask + pMMUHeap->ui32PTMask)) {

		sHighDevVAddr.uiAddr = UINT32_MAX_VALUE;
	} else {
		sHighDevVAddr.uiAddr = DevVAddr.uiAddr
		    + ui32Size
		    + pMMUHeap->ui32DataPageMask + pMMUHeap->ui32PTMask;
	}

	ui32PageTableCount = sHighDevVAddr.uiAddr >> pMMUHeap->ui32PDShift;

	ui32PageTableCount -= ui32PDIndex;

	pui32PDEntry = (u32 *) pMMUHeap->psMMUContext->pvPDCpuVAddr;
	pui32PDEntry += ui32PDIndex;

	ppsPTInfoList = &pMMUHeap->psMMUContext->apsPTInfoList[ui32PDIndex];

	PDUMPCOMMENT("Alloc page table (page count == %08X)",
		     ui32PageTableCount);
	PDUMPCOMMENT("Page directory mods (page count == %08X)",
		     ui32PageTableCount);

	for (i = 0; i < ui32PageTableCount; i++) {
		if (ppsPTInfoList[i] == NULL) {
			OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				   sizeof(MMU_PT_INFO),
				   (void **)&ppsPTInfoList[i], NULL,
				   "MMU Page Table Info");
			if (ppsPTInfoList[i] == NULL) {
				PVR_DPF((PVR_DBG_ERROR,
					 "_DeferredAllocPagetables: ERROR call to OSAllocMem failed"));
				return 0;
			}
			memset(ppsPTInfoList[i], 0, sizeof(MMU_PT_INFO));
		}

		if (ppsPTInfoList[i]->hPTPageOSMemHandle == NULL
		    && ppsPTInfoList[i]->PTPageCpuVAddr == NULL) {
			IMG_DEV_PHYADDR sDevPAddr;
#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)
			u32 *pui32Tmp;
			u32 j;
#else

			PVR_ASSERT(pui32PDEntry[i] == 0);
#endif

			if (_AllocPageTableMemory
			    (pMMUHeap, ppsPTInfoList[i], &sDevPAddr) != 1) {
				PVR_DPF((PVR_DBG_ERROR,
					 "_DeferredAllocPagetables: ERROR call to _AllocPageTableMemory failed"));
				return 0;
			}

			switch (pMMUHeap->psDevArena->DevMemHeapType) {
			case DEVICE_MEMORY_HEAP_SHARED:
			case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
				{

					MMU_CONTEXT *psMMUContext =
					    (MMU_CONTEXT *) pMMUHeap->
					    psMMUContext->psDevInfo->
					    pvMMUContextList;

					while (psMMUContext) {

						pui32PDEntry =
						    (u32 *) psMMUContext->
						    pvPDCpuVAddr;
						pui32PDEntry += ui32PDIndex;

						pui32PDEntry[i] =
						    (sDevPAddr.
						     uiAddr >>
						     SGX_MMU_PDE_ADDR_ALIGNSHIFT)
						    | pMMUHeap->
						    ui32PDEPageSizeCtrl |
						    SGX_MMU_PDE_VALID;

						PDUMPMEM2
						    (PVRSRV_DEVICE_TYPE_SGX,
						     (void *)&pui32PDEntry[i],
						     sizeof(u32), 0, 0,
						     PDUMP_PD_UNIQUETAG,
						     PDUMP_PT_UNIQUETAG);

						psMMUContext =
						    psMMUContext->psNext;
					}
					break;
				}
			case DEVICE_MEMORY_HEAP_PERCONTEXT:
			case DEVICE_MEMORY_HEAP_KERNEL:
				{

					pui32PDEntry[i] =
					    (sDevPAddr.
					     uiAddr >>
					     SGX_MMU_PDE_ADDR_ALIGNSHIFT)
					    | pMMUHeap->
					    ui32PDEPageSizeCtrl |
					    SGX_MMU_PDE_VALID;

					PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
						  (void *)&pui32PDEntry[i],
						  sizeof(u32), 0, 0,
						  PDUMP_PD_UNIQUETAG,
						  PDUMP_PT_UNIQUETAG);
					break;
				}
			default:
				{
					PVR_DPF((PVR_DBG_ERROR,
						 "_DeferredAllocPagetables: ERROR invalid heap type"));
					return 0;
				}
			}

#if !defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)

			MMU_InvalidateDirectoryCache(pMMUHeap->psMMUContext->
						     psDevInfo);
#endif
		} else {

			PVR_ASSERT(pui32PDEntry[i] != 0);
		}
	}

#if defined(SGX_FEATURE_SYSTEM_CACHE)
	MMU_InvalidateSystemLevelCache(pMMUHeap->psMMUContext->psDevInfo);
#endif

	return 1;
}

PVRSRV_ERROR
MMU_Initialise(PVRSRV_DEVICE_NODE * psDeviceNode, MMU_CONTEXT ** ppsMMUContext,
	       IMG_DEV_PHYADDR * psPDDevPAddr)
{
	u32 *pui32Tmp;
	u32 i;
	IMG_CPU_VIRTADDR pvPDCpuVAddr;
	IMG_DEV_PHYADDR sPDDevPAddr;
	IMG_CPU_PHYADDR sCpuPAddr;
	MMU_CONTEXT *psMMUContext;
	void *hPDOSMemHandle;
	SYS_DATA *psSysData;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	PVR_DPF((PVR_DBG_MESSAGE, "MMU_Initialise"));

	SysAcquireData(&psSysData);

	OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		   sizeof(MMU_CONTEXT),
		   (void **)&psMMUContext, NULL, "MMU Context");
	if (psMMUContext == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Initialise: ERROR call to OSAllocMem failed"));
		return PVRSRV_ERROR_GENERIC;
	}
	memset(psMMUContext, 0, sizeof(MMU_CONTEXT));

	psDevInfo = (PVRSRV_SGXDEV_INFO *) psDeviceNode->pvDevice;
	psMMUContext->psDevInfo = psDevInfo;

	psMMUContext->psDeviceNode = psDeviceNode;

	if (psDeviceNode->psLocalDevMemArena == NULL) {
		if (OSAllocPages
		    (PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
		     SGX_MMU_PAGE_SIZE, SGX_MMU_PAGE_SIZE, &pvPDCpuVAddr,
		     &hPDOSMemHandle) != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_Initialise: ERROR call to OSAllocPages failed"));
			return PVRSRV_ERROR_GENERIC;
		}

		if (pvPDCpuVAddr) {
			sCpuPAddr = OSMapLinToCPUPhys(pvPDCpuVAddr);
		} else {

			sCpuPAddr = OSMemHandleToCpuPAddr(hPDOSMemHandle, 0);
		}
		sPDDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

#if PAGE_TEST
		PageTest(pvPDCpuVAddr, sPDDevPAddr);
#endif

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		if (!psDevInfo->pvMMUContextList) {

			if (OSAllocPages
			    (PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
			     SGX_MMU_PAGE_SIZE, SGX_MMU_PAGE_SIZE,
			     &psDevInfo->pvDummyPTPageCpuVAddr,
			     &psDevInfo->hDummyPTPageOSMemHandle) !=
			    PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR call to OSAllocPages failed"));
				return PVRSRV_ERROR_GENERIC;
			}

			if (psDevInfo->pvDummyPTPageCpuVAddr) {
				sCpuPAddr =
				    OSMapLinToCPUPhys(psDevInfo->
						      pvDummyPTPageCpuVAddr);
			} else {

				sCpuPAddr =
				    OSMemHandleToCpuPAddr(psDevInfo->
							  hDummyPTPageOSMemHandle,
							  0);
			}
			psDevInfo->sDummyPTDevPAddr =
			    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX,
						  sCpuPAddr);

			if (OSAllocPages
			    (PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
			     SGX_MMU_PAGE_SIZE, SGX_MMU_PAGE_SIZE,
			     &psDevInfo->pvDummyDataPageCpuVAddr,
			     &psDevInfo->hDummyDataPageOSMemHandle) !=
			    PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR call to OSAllocPages failed"));
				return PVRSRV_ERROR_GENERIC;
			}

			if (psDevInfo->pvDummyDataPageCpuVAddr) {
				sCpuPAddr =
				    OSMapLinToCPUPhys(psDevInfo->
						      pvDummyDataPageCpuVAddr);
			} else {
				sCpuPAddr =
				    OSMemHandleToCpuPAddr(psDevInfo->
							  hDummyDataPageOSMemHandle,
							  0);
			}
			psDevInfo->sDummyDataDevPAddr =
			    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX,
						  sCpuPAddr);
		}
#endif
	} else {
		IMG_SYS_PHYADDR sSysPAddr;

		if (RA_Alloc(psDeviceNode->psLocalDevMemArena,
			     SGX_MMU_PAGE_SIZE,
			     NULL,
			     NULL,
			     0,
			     SGX_MMU_PAGE_SIZE, 0, &(sSysPAddr.uiAddr)) != 1) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_Initialise: ERROR call to RA_Alloc failed"));
			return PVRSRV_ERROR_GENERIC;
		}

		sCpuPAddr = SysSysPAddrToCpuPAddr(sSysPAddr);
		sPDDevPAddr =
		    SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sSysPAddr);
		pvPDCpuVAddr =
		    OSMapPhysToLin(sCpuPAddr, SGX_MMU_PAGE_SIZE,
				   PVRSRV_HAP_WRITECOMBINE |
				   PVRSRV_HAP_KERNEL_ONLY, &hPDOSMemHandle);
		if (!pvPDCpuVAddr) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_Initialise: ERROR failed to map page tables"));
			return PVRSRV_ERROR_GENERIC;
		}
#if PAGE_TEST
		PageTest(pvPDCpuVAddr, sPDDevPAddr);
#endif

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		if (!psDevInfo->pvMMUContextList) {

			if (RA_Alloc(psDeviceNode->psLocalDevMemArena,
				     SGX_MMU_PAGE_SIZE,
				     NULL,
				     NULL,
				     0,
				     SGX_MMU_PAGE_SIZE,
				     0, &(sSysPAddr.uiAddr)) != 1) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR call to RA_Alloc failed"));
				return PVRSRV_ERROR_GENERIC;
			}

			sCpuPAddr = SysSysPAddrToCpuPAddr(sSysPAddr);
			psDevInfo->sDummyPTDevPAddr =
			    SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX,
						  sSysPAddr);
			psDevInfo->pvDummyPTPageCpuVAddr =
			    OSMapPhysToLin(sCpuPAddr, SGX_MMU_PAGE_SIZE,
					   PVRSRV_HAP_WRITECOMBINE |
					   PVRSRV_HAP_KERNEL_ONLY,
					   &psDevInfo->hDummyPTPageOSMemHandle);
			if (!psDevInfo->pvDummyPTPageCpuVAddr) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR failed to map page tables"));
				return PVRSRV_ERROR_GENERIC;
			}

			if (RA_Alloc(psDeviceNode->psLocalDevMemArena,
				     SGX_MMU_PAGE_SIZE,
				     NULL,
				     NULL,
				     0,
				     SGX_MMU_PAGE_SIZE,
				     0, &(sSysPAddr.uiAddr)) != 1) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR call to RA_Alloc failed"));
				return PVRSRV_ERROR_GENERIC;
			}

			sCpuPAddr = SysSysPAddrToCpuPAddr(sSysPAddr);
			psDevInfo->sDummyDataDevPAddr =
			    SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX,
						  sSysPAddr);
			psDevInfo->pvDummyDataPageCpuVAddr =
			    OSMapPhysToLin(sCpuPAddr, SGX_MMU_PAGE_SIZE,
					   PVRSRV_HAP_WRITECOMBINE |
					   PVRSRV_HAP_KERNEL_ONLY,
					   &psDevInfo->
					   hDummyDataPageOSMemHandle);
			if (!psDevInfo->pvDummyDataPageCpuVAddr) {
				PVR_DPF((PVR_DBG_ERROR,
					 "MMU_Initialise: ERROR failed to map page tables"));
				return PVRSRV_ERROR_GENERIC;
			}
		}
#endif
	}

	PDUMPCOMMENT("Alloc page directory");
#ifdef SUPPORT_SGX_MMU_BYPASS
	EnableHostAccess(psMMUContext);
#endif

	PDUMPMALLOCPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, pvPDCpuVAddr,
			     SGX_MMU_PAGE_SIZE, PDUMP_PD_UNIQUETAG);

	if (pvPDCpuVAddr) {
		pui32Tmp = (u32 *) pvPDCpuVAddr;
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Initialise: pvPDCpuVAddr invalid"));
		return PVRSRV_ERROR_GENERIC;
	}

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

	for (i = 0; i < SGX_MMU_PD_SIZE; i++) {
		pui32Tmp[i] =
		    (psDevInfo->sDummyPTDevPAddr.
		     uiAddr >> SGX_MMU_PDE_ADDR_ALIGNSHIFT)
		    | SGX_MMU_PDE_PAGE_SIZE_4K | SGX_MMU_PDE_VALID;
	}

	if (!psDevInfo->pvMMUContextList) {

		pui32Tmp = (u32 *) psDevInfo->pvDummyPTPageCpuVAddr;
		for (i = 0; i < SGX_MMU_PT_SIZE; i++) {
			pui32Tmp[i] =
			    (psDevInfo->sDummyDataDevPAddr.
			     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
			    | SGX_MMU_PTE_VALID;
		}

		PDUMPCOMMENT("Dummy Page table contents");
		PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
			  psDevInfo->pvDummyPTPageCpuVAddr, SGX_MMU_PAGE_SIZE,
			  0, 1, PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);

		pui32Tmp = (u32 *) psDevInfo->pvDummyDataPageCpuVAddr;
		for (i = 0; i < (SGX_MMU_PAGE_SIZE / 4); i++) {
			pui32Tmp[i] = DUMMY_DATA_PAGE_SIGNATURE;
		}

		PDUMPCOMMENT("Dummy Data Page contents");
		PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
			  psDevInfo->pvDummyDataPageCpuVAddr, SGX_MMU_PAGE_SIZE,
			  0, 1, PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	}
#else

#if defined(INTEL_D3_P_CHANGES)
	if (pui32Tmp) {
#endif
		for (i = 0; i < SGX_MMU_PD_SIZE; i++) {

			pui32Tmp[i] = 0;
		}
#if defined(INTEL_D3_P_CHANGES)
	}
#endif
#endif

	PDUMPCOMMENT("Page directory contents");
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pvPDCpuVAddr, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);

#if defined(PDUMP)
	if (PDumpSetMMUContext(PVRSRV_DEVICE_TYPE_SGX,
			       "SGXMEM",
			       &psMMUContext->ui32PDumpMMUContextID,
			       2,
			       PDUMP_PT_UNIQUETAG, pvPDCpuVAddr) != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Initialise: ERROR call to PDumpSetMMUContext failed"));
		return PVRSRV_ERROR_GENERIC;
	}
#endif

	psMMUContext->pvPDCpuVAddr = pvPDCpuVAddr;
	psMMUContext->sPDDevPAddr = sPDDevPAddr;
	psMMUContext->hPDOSMemHandle = hPDOSMemHandle;

	*ppsMMUContext = psMMUContext;

	*psPDDevPAddr = sPDDevPAddr;

	psMMUContext->psNext = (MMU_CONTEXT *) psDevInfo->pvMMUContextList;
	psDevInfo->pvMMUContextList = (void *)psMMUContext;

#ifdef SUPPORT_SGX_MMU_BYPASS
	DisableHostAccess(psMMUContext);
#endif

	return PVRSRV_OK;
}

void MMU_Finalise(MMU_CONTEXT * psMMUContext)
{
	u32 *pui32Tmp, i;
	SYS_DATA *psSysData;
	MMU_CONTEXT **ppsMMUContext;
#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)
	PVRSRV_SGXDEV_INFO *psDevInfo =
	    (PVRSRV_SGXDEV_INFO *) psMMUContext->psDevInfo;
	MMU_CONTEXT *psMMUContextList =
	    (MMU_CONTEXT *) psDevInfo->pvMMUContextList;
#endif

	SysAcquireData(&psSysData);

	PDUMPCLEARMMUCONTEXT(PVRSRV_DEVICE_TYPE_SGX, "SGXMEM",
			     psMMUContext->ui32PDumpMMUContextID, 2);

	PDUMPCOMMENT("Free page directory");
	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, psMMUContext->pvPDCpuVAddr,
			   SGX_MMU_PAGE_SIZE, PDUMP_PT_UNIQUETAG);
#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)
	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX,
			   psDevInfo->pvDummyPTPageCpuVAddr, SGX_MMU_PAGE_SIZE,
			   PDUMP_PT_UNIQUETAG);
	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX,
			   psDevInfo->pvDummyDataPageCpuVAddr,
			   SGX_MMU_PAGE_SIZE, PDUMP_PT_UNIQUETAG);
#endif

	pui32Tmp = (u32 *) psMMUContext->pvPDCpuVAddr;

	for (i = 0; i < SGX_MMU_PD_SIZE; i++) {

		pui32Tmp[i] = 0;
	}

	if (psMMUContext->psDeviceNode->psLocalDevMemArena == NULL) {
		OSFreePages(PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
			    SGX_MMU_PAGE_SIZE,
			    psMMUContext->pvPDCpuVAddr,
			    psMMUContext->hPDOSMemHandle);

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		if (!psMMUContextList->psNext) {
			OSFreePages(PVRSRV_HAP_WRITECOMBINE |
				    PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				    psDevInfo->pvDummyPTPageCpuVAddr,
				    psDevInfo->hDummyPTPageOSMemHandle);
			OSFreePages(PVRSRV_HAP_WRITECOMBINE |
				    PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				    psDevInfo->pvDummyDataPageCpuVAddr,
				    psDevInfo->hDummyDataPageOSMemHandle);
		}
#endif
	} else {
		IMG_SYS_PHYADDR sSysPAddr;
		IMG_CPU_PHYADDR sCpuPAddr;

		sCpuPAddr = OSMapLinToCPUPhys(psMMUContext->pvPDCpuVAddr);
		sSysPAddr = SysCpuPAddrToSysPAddr(sCpuPAddr);

		OSUnMapPhysToLin(psMMUContext->pvPDCpuVAddr,
				 SGX_MMU_PAGE_SIZE,
				 PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY,
				 psMMUContext->hPDOSMemHandle);

		RA_Free(psMMUContext->psDeviceNode->psLocalDevMemArena,
			sSysPAddr.uiAddr, 0);

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		if (!psMMUContextList->psNext) {

			sCpuPAddr =
			    OSMapLinToCPUPhys(psDevInfo->pvDummyPTPageCpuVAddr);
			sSysPAddr = SysCpuPAddrToSysPAddr(sCpuPAddr);

			OSUnMapPhysToLin(psDevInfo->pvDummyPTPageCpuVAddr,
					 SGX_MMU_PAGE_SIZE,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 psDevInfo->hDummyPTPageOSMemHandle);

			RA_Free(psMMUContext->psDeviceNode->psLocalDevMemArena,
				sSysPAddr.uiAddr, 0);

			sCpuPAddr =
			    OSMapLinToCPUPhys(psDevInfo->
					      pvDummyDataPageCpuVAddr);
			sSysPAddr = SysCpuPAddrToSysPAddr(sCpuPAddr);

			OSUnMapPhysToLin(psDevInfo->pvDummyDataPageCpuVAddr,
					 SGX_MMU_PAGE_SIZE,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 psDevInfo->hDummyDataPageOSMemHandle);

			RA_Free(psMMUContext->psDeviceNode->psLocalDevMemArena,
				sSysPAddr.uiAddr, 0);
		}
#endif
	}

	PVR_DPF((PVR_DBG_MESSAGE, "MMU_Finalise"));

	ppsMMUContext =
	    (MMU_CONTEXT **) & psMMUContext->psDevInfo->pvMMUContextList;
	while (*ppsMMUContext) {
		if (*ppsMMUContext == psMMUContext) {

			*ppsMMUContext = psMMUContext->psNext;
			break;
		}

		ppsMMUContext = &((*ppsMMUContext)->psNext);
	}

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(MMU_CONTEXT), psMMUContext,
		  NULL);

}

void MMU_InsertHeap(MMU_CONTEXT * psMMUContext, MMU_HEAP * psMMUHeap)
{
	u32 *pui32PDCpuVAddr = (u32 *) psMMUContext->pvPDCpuVAddr;
	u32 *pui32KernelPDCpuVAddr =
	    (u32 *) psMMUHeap->psMMUContext->pvPDCpuVAddr;
	u32 ui32PDEntry;
#if !defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
	int bInvalidateDirectoryCache = 0;
#endif

	pui32PDCpuVAddr +=
	    psMMUHeap->psDevArena->BaseDevVAddr.uiAddr >> psMMUHeap->
	    ui32PDShift;
	pui32KernelPDCpuVAddr +=
	    psMMUHeap->psDevArena->BaseDevVAddr.uiAddr >> psMMUHeap->
	    ui32PDShift;

	PDUMPCOMMENT("Page directory shared heap range copy");
#ifdef SUPPORT_SGX_MMU_BYPASS
	EnableHostAccess(psMMUContext);
#endif

	for (ui32PDEntry = 0; ui32PDEntry < psMMUHeap->ui32PageTableCount;
	     ui32PDEntry++) {
#if !defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		PVR_ASSERT(pui32PDCpuVAddr[ui32PDEntry] == 0);
#endif

		pui32PDCpuVAddr[ui32PDEntry] =
		    pui32KernelPDCpuVAddr[ui32PDEntry];
		if (pui32PDCpuVAddr[ui32PDEntry]) {
			PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
				  (void *)&pui32PDCpuVAddr[ui32PDEntry],
				  sizeof(u32), 0, 0, PDUMP_PD_UNIQUETAG,
				  PDUMP_PT_UNIQUETAG);

#if !defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
			bInvalidateDirectoryCache = 1;
#endif
		}
	}

#ifdef SUPPORT_SGX_MMU_BYPASS
	DisableHostAccess(psMMUContext);
#endif

#if !defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
	if (bInvalidateDirectoryCache) {

		MMU_InvalidateDirectoryCache(psMMUContext->psDevInfo);
	}
#endif
}

static void
MMU_UnmapPagesAndFreePTs(MMU_HEAP * psMMUHeap,
			 IMG_DEV_VIRTADDR sDevVAddr,
			 u32 ui32PageCount, void *hUniqueTag)
{
	IMG_DEV_VIRTADDR sTmpDevVAddr;
	u32 i;
	u32 ui32PDIndex;
	u32 ui32PTIndex;
	u32 *pui32Tmp;
	int bInvalidateDirectoryCache = 0;

	sTmpDevVAddr = sDevVAddr;

	for (i = 0; i < ui32PageCount; i++) {
		MMU_PT_INFO **ppsPTInfoList;

		ui32PDIndex = sTmpDevVAddr.uiAddr >> psMMUHeap->ui32PDShift;

		ppsPTInfoList =
		    &psMMUHeap->psMMUContext->apsPTInfoList[ui32PDIndex];

		{

			ui32PTIndex =
			    (sTmpDevVAddr.uiAddr & psMMUHeap->
			     ui32PTMask) >> psMMUHeap->ui32PTShift;

			if (!ppsPTInfoList[0]) {
				PVR_DPF((PVR_DBG_MESSAGE,
					 "MMU_UnmapPagesAndFreePTs: Invalid PT for alloc at VAddr:0x%08lX (VaddrIni:0x%08lX AllocPage:%u) PDIdx:%u PTIdx:%u",
					 sTmpDevVAddr.uiAddr, sDevVAddr.uiAddr,
					 i, ui32PDIndex, ui32PTIndex));

				sTmpDevVAddr.uiAddr +=
				    psMMUHeap->ui32DataPageSize;

				continue;
			}

			pui32Tmp = (u32 *) ppsPTInfoList[0]->PTPageCpuVAddr;

			if (!pui32Tmp) {
				continue;
			}

			CheckPT(ppsPTInfoList[0]);

			if (pui32Tmp[ui32PTIndex] & SGX_MMU_PTE_VALID) {
				ppsPTInfoList[0]->ui32ValidPTECount--;
			} else {
				PVR_DPF((PVR_DBG_MESSAGE,
					 "MMU_UnmapPagesAndFreePTs: Page is already invalid for alloc at VAddr:0x%08lX (VAddrIni:0x%08lX AllocPage:%u) PDIdx:%u PTIdx:%u",
					 sTmpDevVAddr.uiAddr, sDevVAddr.uiAddr,
					 i, ui32PDIndex, ui32PTIndex));
			}

			PVR_ASSERT((s32) ppsPTInfoList[0]->ui32ValidPTECount >=
				   0);

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

			pui32Tmp[ui32PTIndex] =
			    (psMMUHeap->psMMUContext->psDevInfo->
			     sDummyDataDevPAddr.
			     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
			    | SGX_MMU_PTE_VALID;
#else

			pui32Tmp[ui32PTIndex] = 0;
#endif

			CheckPT(ppsPTInfoList[0]);
		}

		if (ppsPTInfoList[0]
		    && ppsPTInfoList[0]->ui32ValidPTECount == 0) {
			_DeferredFreePageTable(psMMUHeap,
					       ui32PDIndex -
					       psMMUHeap->ui32PDBaseIndex, 1);
			bInvalidateDirectoryCache = 1;
		}

		sTmpDevVAddr.uiAddr += psMMUHeap->ui32DataPageSize;
	}

	if (bInvalidateDirectoryCache) {
		MMU_InvalidateDirectoryCache(psMMUHeap->psMMUContext->
					     psDevInfo);
	} else {
		MMU_InvalidatePageTableCache(psMMUHeap->psMMUContext->
					     psDevInfo);
	}

#if defined(PDUMP)
	MMU_PDumpPageTables(psMMUHeap,
			    sDevVAddr,
			    psMMUHeap->ui32DataPageSize * ui32PageCount,
			    1, hUniqueTag);
#endif
}

void MMU_FreePageTables(void *pvMMUHeap,
			u32 ui32Start, u32 ui32End, void *hUniqueTag)
{
	MMU_HEAP *pMMUHeap = (MMU_HEAP *) pvMMUHeap;
	IMG_DEV_VIRTADDR Start;

	Start.uiAddr = ui32Start;

	MMU_UnmapPagesAndFreePTs(pMMUHeap, Start,
				 (ui32End - ui32Start) >> pMMUHeap->ui32PTShift,
				 hUniqueTag);
}

MMU_HEAP *MMU_Create(MMU_CONTEXT * psMMUContext,
		     DEV_ARENA_DESCRIPTOR * psDevArena, RA_ARENA ** ppsVMArena)
{
	MMU_HEAP *pMMUHeap;
	u32 ui32ScaleSize;

	PVR_ASSERT(psDevArena != NULL);

	if (psDevArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "MMU_Create: invalid parameter"));
		return NULL;
	}

	OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		   sizeof(MMU_HEAP), (void **)&pMMUHeap, NULL, "MMU Heap");
	if (pMMUHeap == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Create: ERROR call to OSAllocMem failed"));
		return NULL;
	}

	pMMUHeap->psMMUContext = psMMUContext;
	pMMUHeap->psDevArena = psDevArena;

	switch (pMMUHeap->psDevArena->ui32DataPageSize) {
	case 0x1000:
		ui32ScaleSize = 0;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_4K;
		break;
#if defined(SGX_FEATURE_VARIABLE_MMU_PAGE_SIZE)
	case 0x4000:
		ui32ScaleSize = 2;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_16K;
		break;
	case 0x10000:
		ui32ScaleSize = 4;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_64K;
		break;
	case 0x40000:
		ui32ScaleSize = 6;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_256K;
		break;
	case 0x100000:
		ui32ScaleSize = 8;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_1M;
		break;
	case 0x400000:
		ui32ScaleSize = 10;
		pMMUHeap->ui32PDEPageSizeCtrl = SGX_MMU_PDE_PAGE_SIZE_4M;
		break;
#endif
	default:
		PVR_DPF((PVR_DBG_ERROR, "MMU_Create: invalid data page size"));
		goto ErrorFreeHeap;
	}

	pMMUHeap->ui32DataPageSize = psDevArena->ui32DataPageSize;
	pMMUHeap->ui32DataPageBitWidth = SGX_MMU_PAGE_SHIFT + ui32ScaleSize;
	pMMUHeap->ui32DataPageMask = pMMUHeap->ui32DataPageSize - 1;

	pMMUHeap->ui32PTShift = pMMUHeap->ui32DataPageBitWidth;
	pMMUHeap->ui32PTBitWidth = SGX_MMU_PT_SHIFT - ui32ScaleSize;
	pMMUHeap->ui32PTMask =
	    SGX_MMU_PT_MASK & (SGX_MMU_PT_MASK << ui32ScaleSize);
	pMMUHeap->ui32PTSize = (1UL << pMMUHeap->ui32PTBitWidth) * sizeof(u32);

	if (pMMUHeap->ui32PTSize < 4 * sizeof(u32)) {
		pMMUHeap->ui32PTSize = 4 * sizeof(u32);
	}
	pMMUHeap->ui32PTECount = pMMUHeap->ui32PTSize >> 2;

	pMMUHeap->ui32PDShift =
	    pMMUHeap->ui32PTBitWidth + pMMUHeap->ui32PTShift;
	pMMUHeap->ui32PDBitWidth =
	    SGX_FEATURE_ADDRESS_SPACE_SIZE - pMMUHeap->ui32PTBitWidth -
	    pMMUHeap->ui32DataPageBitWidth;
	pMMUHeap->ui32PDMask =
	    SGX_MMU_PD_MASK & (SGX_MMU_PD_MASK >>
			       (32 - SGX_FEATURE_ADDRESS_SPACE_SIZE));

	if (psDevArena->BaseDevVAddr.uiAddr >
	    (pMMUHeap->ui32DataPageMask | pMMUHeap->ui32PTMask)) {

		PVR_ASSERT((psDevArena->BaseDevVAddr.uiAddr
			    & (pMMUHeap->ui32DataPageMask
			       | pMMUHeap->ui32PTMask)) == 0);
	}

	pMMUHeap->ui32PTETotal =
	    pMMUHeap->psDevArena->ui32Size >> pMMUHeap->ui32PTShift;

	pMMUHeap->ui32PDBaseIndex =
	    (pMMUHeap->psDevArena->BaseDevVAddr.uiAddr & pMMUHeap->
	     ui32PDMask) >> pMMUHeap->ui32PDShift;

	pMMUHeap->ui32PageTableCount =
	    (pMMUHeap->ui32PTETotal + pMMUHeap->ui32PTECount - 1)
	    >> pMMUHeap->ui32PTBitWidth;

	pMMUHeap->psVMArena = RA_Create(psDevArena->pszName,
					psDevArena->BaseDevVAddr.uiAddr,
					psDevArena->ui32Size,
					NULL,
					pMMUHeap->ui32DataPageSize,
					NULL,
					NULL, MMU_FreePageTables, pMMUHeap);

	if (pMMUHeap->psVMArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Create: ERROR call to RA_Create failed"));
		goto ErrorFreePagetables;
	}

	*ppsVMArena = pMMUHeap->psVMArena;

	return pMMUHeap;

ErrorFreePagetables:
	_DeferredFreePageTables(pMMUHeap);

ErrorFreeHeap:
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(MMU_HEAP), pMMUHeap, NULL);

	return NULL;
}

void MMU_Delete(MMU_HEAP * pMMUHeap)
{
	if (pMMUHeap != NULL) {
		PVR_DPF((PVR_DBG_MESSAGE, "MMU_Delete"));

		if (pMMUHeap->psVMArena) {
			RA_Delete(pMMUHeap->psVMArena);
		}
#ifdef SUPPORT_SGX_MMU_BYPASS
		EnableHostAccess(pMMUHeap->psMMUContext);
#endif
		_DeferredFreePageTables(pMMUHeap);
#ifdef SUPPORT_SGX_MMU_BYPASS
		DisableHostAccess(pMMUHeap->psMMUContext);
#endif

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(MMU_HEAP), pMMUHeap,
			  NULL);

	}
}

int
MMU_Alloc(MMU_HEAP * pMMUHeap,
	  u32 uSize,
	  u32 * pActualSize,
	  u32 uFlags, u32 uDevVAddrAlignment, IMG_DEV_VIRTADDR * psDevVAddr)
{
	int bStatus;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "MMU_Alloc: uSize=0x%x, flags=0x%x, align=0x%x",
		 uSize, uFlags, uDevVAddrAlignment));

	if ((uFlags & PVRSRV_MEM_USER_SUPPLIED_DEVVADDR) == 0) {
		u32 uiAddr;

		bStatus = RA_Alloc(pMMUHeap->psVMArena,
				   uSize,
				   pActualSize,
				   NULL, 0, uDevVAddrAlignment, 0, &uiAddr);
		if (!bStatus) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_Alloc: RA_Alloc of VMArena failed"));
			return bStatus;
		}

		psDevVAddr->uiAddr = IMG_CAST_TO_DEVVADDR_UINT(uiAddr);
	}
#ifdef SUPPORT_SGX_MMU_BYPASS
	EnableHostAccess(pMMUHeap->psMMUContext);
#endif

	bStatus = _DeferredAllocPagetables(pMMUHeap, *psDevVAddr, uSize);

#ifdef SUPPORT_SGX_MMU_BYPASS
	DisableHostAccess(pMMUHeap->psMMUContext);
#endif

	if (!bStatus) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_Alloc: _DeferredAllocPagetables failed"));
		if ((uFlags & PVRSRV_MEM_USER_SUPPLIED_DEVVADDR) == 0) {

			RA_Free(pMMUHeap->psVMArena, psDevVAddr->uiAddr, 0);
		}
	}

	return bStatus;
}

void MMU_Free(MMU_HEAP * pMMUHeap, IMG_DEV_VIRTADDR DevVAddr, u32 ui32Size)
{
	PVR_ASSERT(pMMUHeap != NULL);

	if (pMMUHeap == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "MMU_Free: invalid parameter"));
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "MMU_Free: mmu=%08X, dev_vaddr=%08X", pMMUHeap,
		 DevVAddr.uiAddr));

	if ((DevVAddr.uiAddr >= pMMUHeap->psDevArena->BaseDevVAddr.uiAddr) &&
	    (DevVAddr.uiAddr + ui32Size <=
	     pMMUHeap->psDevArena->BaseDevVAddr.uiAddr +
	     pMMUHeap->psDevArena->ui32Size)) {
		RA_Free(pMMUHeap->psVMArena, DevVAddr.uiAddr, 1);
		return;
	}

	PVR_DPF((PVR_DBG_ERROR,
		 "MMU_Free: Couldn't find DevVAddr %08X in a DevArena",
		 DevVAddr.uiAddr));
}

void MMU_Enable(MMU_HEAP * pMMUHeap)
{

}

void MMU_Disable(MMU_HEAP * pMMUHeap)
{

}

#if defined(PDUMP)
static void
MMU_PDumpPageTables(MMU_HEAP * pMMUHeap,
		    IMG_DEV_VIRTADDR DevVAddr,
		    u32 uSize, int bForUnmap, void *hUniqueTag)
{
	u32 ui32NumPTEntries;
	u32 ui32PTIndex;
	u32 *pui32PTEntry;

	MMU_PT_INFO **ppsPTInfoList;
	u32 ui32PDIndex;
	u32 ui32PTDumpCount;

	ui32NumPTEntries =
	    (uSize + pMMUHeap->ui32DataPageMask) >> pMMUHeap->ui32PTShift;

	ui32PDIndex = DevVAddr.uiAddr >> pMMUHeap->ui32PDShift;

	ppsPTInfoList = &pMMUHeap->psMMUContext->apsPTInfoList[ui32PDIndex];

	ui32PTIndex =
	    (DevVAddr.uiAddr & pMMUHeap->ui32PTMask) >> pMMUHeap->ui32PTShift;

	PDUMPCOMMENT("Page table mods (num entries == %08X) %s",
		     ui32NumPTEntries, bForUnmap ? "(for unmap)" : "");

	while (ui32NumPTEntries > 0) {
		MMU_PT_INFO *psPTInfo = *ppsPTInfoList++;

		if (ui32NumPTEntries <= pMMUHeap->ui32PTECount - ui32PTIndex) {
			ui32PTDumpCount = ui32NumPTEntries;
		} else {
			ui32PTDumpCount = pMMUHeap->ui32PTECount - ui32PTIndex;
		}

		if (psPTInfo) {
			pui32PTEntry = (u32 *) psPTInfo->PTPageCpuVAddr;
			PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX,
				  (void *)&pui32PTEntry[ui32PTIndex],
				  ui32PTDumpCount * sizeof(u32), 0, 0,
				  PDUMP_PT_UNIQUETAG, hUniqueTag);
		}

		ui32NumPTEntries -= ui32PTDumpCount;

		ui32PTIndex = 0;
	}

	PDUMPCOMMENT("Finished page table mods %s",
		     bForUnmap ? "(for unmap)" : "");
}
#endif

static void
MMU_MapPage(MMU_HEAP * pMMUHeap,
	    IMG_DEV_VIRTADDR DevVAddr,
	    IMG_DEV_PHYADDR DevPAddr, u32 ui32MemFlags)
{
	u32 ui32Index;
	u32 *pui32Tmp;
	u32 ui32MMUFlags = 0;
	MMU_PT_INFO **ppsPTInfoList;

	PVR_ASSERT((DevPAddr.uiAddr & pMMUHeap->ui32DataPageMask) == 0);

	if (((PVRSRV_MEM_READ | PVRSRV_MEM_WRITE) & ui32MemFlags) ==
	    (PVRSRV_MEM_READ | PVRSRV_MEM_WRITE)) {

		ui32MMUFlags = 0;
	} else if (PVRSRV_MEM_READ & ui32MemFlags) {

		ui32MMUFlags |= SGX_MMU_PTE_READONLY;
	} else if (PVRSRV_MEM_WRITE & ui32MemFlags) {

		ui32MMUFlags |= SGX_MMU_PTE_WRITEONLY;
	}

	if (PVRSRV_MEM_CACHE_CONSISTENT & ui32MemFlags) {
		ui32MMUFlags |= SGX_MMU_PTE_CACHECONSISTENT;
	}
#if !defined(FIX_HW_BRN_25503)

	if (PVRSRV_MEM_EDM_PROTECT & ui32MemFlags) {
		ui32MMUFlags |= SGX_MMU_PTE_EDMPROTECT;
	}
#endif

	ui32Index = DevVAddr.uiAddr >> pMMUHeap->ui32PDShift;

	ppsPTInfoList = &pMMUHeap->psMMUContext->apsPTInfoList[ui32Index];

	CheckPT(ppsPTInfoList[0]);

	ui32Index =
	    (DevVAddr.uiAddr & pMMUHeap->ui32PTMask) >> pMMUHeap->ui32PTShift;

	pui32Tmp = (u32 *) ppsPTInfoList[0]->PTPageCpuVAddr;

#if !defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

	if (pui32Tmp[ui32Index] & SGX_MMU_PTE_VALID) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_MapPage: Page is already valid for alloc at VAddr:0x%08lX PDIdx:%u PTIdx:%u",
			 DevVAddr.uiAddr,
			 DevVAddr.uiAddr >> pMMUHeap->ui32PDShift, ui32Index));
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_MapPage: Page table entry value: 0x%08lX",
			 pui32Tmp[ui32Index]));
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_MapPage: Physical page to map: 0x%08lX",
			 DevPAddr.uiAddr));
	}

	PVR_ASSERT((pui32Tmp[ui32Index] & SGX_MMU_PTE_VALID) == 0);
#endif

	ppsPTInfoList[0]->ui32ValidPTECount++;

	pui32Tmp[ui32Index] = ((DevPAddr.uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
			       & ((~pMMUHeap->ui32DataPageMask) >>
				  SGX_MMU_PTE_ADDR_ALIGNSHIFT))
	    | SGX_MMU_PTE_VALID | ui32MMUFlags;

	CheckPT(ppsPTInfoList[0]);
}

void
MMU_MapScatter(MMU_HEAP * pMMUHeap,
	       IMG_DEV_VIRTADDR DevVAddr,
	       IMG_SYS_PHYADDR * psSysAddr,
	       u32 uSize, u32 ui32MemFlags, void *hUniqueTag)
{
#if defined(PDUMP)
	IMG_DEV_VIRTADDR MapBaseDevVAddr;
#endif
	u32 uCount, i;
	IMG_DEV_PHYADDR DevPAddr;

	PVR_ASSERT(pMMUHeap != NULL);

#if defined(PDUMP)
	MapBaseDevVAddr = DevVAddr;
#endif

	for (i = 0, uCount = 0; uCount < uSize;
	     i++, uCount += pMMUHeap->ui32DataPageSize) {
		IMG_SYS_PHYADDR sSysAddr;

		sSysAddr = psSysAddr[i];

		PVR_ASSERT((sSysAddr.uiAddr & pMMUHeap->ui32DataPageMask) == 0);

		DevPAddr =
		    SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sSysAddr);

		MMU_MapPage(pMMUHeap, DevVAddr, DevPAddr, ui32MemFlags);
		DevVAddr.uiAddr += pMMUHeap->ui32DataPageSize;

		PVR_DPF((PVR_DBG_MESSAGE,
			 "MMU_MapScatter: devVAddr=%08X, SysAddr=%08X, size=0x%x/0x%x",
			 DevVAddr.uiAddr, sSysAddr.uiAddr, uCount, uSize));
	}

#if defined(PDUMP)
	MMU_PDumpPageTables(pMMUHeap, MapBaseDevVAddr, uSize, 0, hUniqueTag);
#endif
}

void
MMU_MapPages(MMU_HEAP * pMMUHeap,
	     IMG_DEV_VIRTADDR DevVAddr,
	     IMG_SYS_PHYADDR SysPAddr,
	     u32 uSize, u32 ui32MemFlags, void *hUniqueTag)
{
	IMG_DEV_PHYADDR DevPAddr;
#if defined(PDUMP)
	IMG_DEV_VIRTADDR MapBaseDevVAddr;
#endif
	u32 uCount;
	u32 ui32VAdvance;
	u32 ui32PAdvance;

	PVR_ASSERT(pMMUHeap != NULL);

	PVR_DPF((PVR_DBG_MESSAGE,
		 "MMU_MapPages: mmu=%08X, devVAddr=%08X, SysPAddr=%08X, size=0x%x",
		 pMMUHeap, DevVAddr.uiAddr, SysPAddr.uiAddr, uSize));

	ui32VAdvance = pMMUHeap->ui32DataPageSize;
	ui32PAdvance = pMMUHeap->ui32DataPageSize;

#if defined(PDUMP)
	MapBaseDevVAddr = DevVAddr;
#endif

	DevPAddr = SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, SysPAddr);

	PVR_ASSERT((DevPAddr.uiAddr & pMMUHeap->ui32DataPageMask) == 0);

#if defined(FIX_HW_BRN_23281)
	if (ui32MemFlags & PVRSRV_MEM_INTERLEAVED) {
		ui32VAdvance *= 2;
	}
#endif

	if (ui32MemFlags & PVRSRV_MEM_DUMMY) {
		ui32PAdvance = 0;
	}

	for (uCount = 0; uCount < uSize; uCount += ui32VAdvance) {
		MMU_MapPage(pMMUHeap, DevVAddr, DevPAddr, ui32MemFlags);
		DevVAddr.uiAddr += ui32VAdvance;
		DevPAddr.uiAddr += ui32PAdvance;
	}

#if defined(PDUMP)
	MMU_PDumpPageTables(pMMUHeap, MapBaseDevVAddr, uSize, 0, hUniqueTag);
#endif
}

void
MMU_MapShadow(MMU_HEAP * pMMUHeap,
	      IMG_DEV_VIRTADDR MapBaseDevVAddr,
	      u32 uByteSize,
	      IMG_CPU_VIRTADDR CpuVAddr,
	      void *hOSMemHandle,
	      IMG_DEV_VIRTADDR * pDevVAddr, u32 ui32MemFlags, void *hUniqueTag)
{
	u32 i;
	u32 uOffset = 0;
	IMG_DEV_VIRTADDR MapDevVAddr;
	u32 ui32VAdvance;
	u32 ui32PAdvance;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "MMU_MapShadow: %08X, 0x%x, %08X",
		 MapBaseDevVAddr.uiAddr, uByteSize, CpuVAddr));

	ui32VAdvance = pMMUHeap->ui32DataPageSize;
	ui32PAdvance = pMMUHeap->ui32DataPageSize;

	PVR_ASSERT(((u32) CpuVAddr & (SGX_MMU_PAGE_SIZE - 1)) == 0);
	PVR_ASSERT(((u32) uByteSize & pMMUHeap->ui32DataPageMask) == 0);
	pDevVAddr->uiAddr = MapBaseDevVAddr.uiAddr;

#if defined(FIX_HW_BRN_23281)
	if (ui32MemFlags & PVRSRV_MEM_INTERLEAVED) {
		ui32VAdvance *= 2;
	}
#endif

	if (ui32MemFlags & PVRSRV_MEM_DUMMY) {
		ui32PAdvance = 0;
	}

	MapDevVAddr = MapBaseDevVAddr;
	for (i = 0; i < uByteSize; i += ui32VAdvance) {
		IMG_CPU_PHYADDR CpuPAddr;
		IMG_DEV_PHYADDR DevPAddr;

		if (CpuVAddr) {
			CpuPAddr =
			    OSMapLinToCPUPhys((void *)((u32) CpuVAddr +
						       uOffset));
		} else {
			CpuPAddr = OSMemHandleToCpuPAddr(hOSMemHandle, uOffset);
		}
		DevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, CpuPAddr);

		PVR_ASSERT((DevPAddr.uiAddr & pMMUHeap->ui32DataPageMask) == 0);

		PVR_DPF((PVR_DBG_MESSAGE,
			 "0x%x: CpuVAddr=%08X, CpuPAddr=%08X, DevVAddr=%08X, DevPAddr=%08X",
			 uOffset,
			 (u32) CpuVAddr + uOffset,
			 CpuPAddr.uiAddr, MapDevVAddr.uiAddr, DevPAddr.uiAddr));

		MMU_MapPage(pMMUHeap, MapDevVAddr, DevPAddr, ui32MemFlags);

		MapDevVAddr.uiAddr += ui32VAdvance;
		uOffset += ui32PAdvance;
	}

#if defined(PDUMP)
	MMU_PDumpPageTables(pMMUHeap, MapBaseDevVAddr, uByteSize, 0,
			    hUniqueTag);
#endif
}

void
MMU_UnmapPages(MMU_HEAP * psMMUHeap,
	       IMG_DEV_VIRTADDR sDevVAddr, u32 ui32PageCount, void *hUniqueTag)
{
	u32 uPageSize = psMMUHeap->ui32DataPageSize;
	IMG_DEV_VIRTADDR sTmpDevVAddr;
	u32 i;
	u32 ui32PDIndex;
	u32 ui32PTIndex;
	u32 *pui32Tmp;

	sTmpDevVAddr = sDevVAddr;

	for (i = 0; i < ui32PageCount; i++) {
		MMU_PT_INFO **ppsPTInfoList;

		ui32PDIndex = sTmpDevVAddr.uiAddr >> psMMUHeap->ui32PDShift;

		ppsPTInfoList =
		    &psMMUHeap->psMMUContext->apsPTInfoList[ui32PDIndex];

		ui32PTIndex =
		    (sTmpDevVAddr.uiAddr & psMMUHeap->ui32PTMask) >> psMMUHeap->
		    ui32PTShift;

		if (!ppsPTInfoList[0]) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_UnmapPages: ERROR Invalid PT for alloc at VAddr:0x%08lX (VaddrIni:0x%08lX AllocPage:%u) PDIdx:%u PTIdx:%u",
				 sTmpDevVAddr.uiAddr, sDevVAddr.uiAddr, i,
				 ui32PDIndex, ui32PTIndex));

			sTmpDevVAddr.uiAddr += uPageSize;

			continue;
		}

		CheckPT(ppsPTInfoList[0]);

		pui32Tmp = (u32 *) ppsPTInfoList[0]->PTPageCpuVAddr;

		if (pui32Tmp[ui32PTIndex] & SGX_MMU_PTE_VALID) {
			ppsPTInfoList[0]->ui32ValidPTECount--;
		} else {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_UnmapPages: Page is already invalid for alloc at VAddr:0x%08lX (VAddrIni:0x%08lX AllocPage:%u) PDIdx:%u PTIdx:%u",
				 sTmpDevVAddr.uiAddr, sDevVAddr.uiAddr, i,
				 ui32PDIndex, ui32PTIndex));
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_UnmapPages: Page table entry value: 0x%08lX",
				 pui32Tmp[ui32PTIndex]));
		}

		PVR_ASSERT((s32) ppsPTInfoList[0]->ui32ValidPTECount >= 0);

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

		pui32Tmp[ui32PTIndex] =
		    (psMMUHeap->psMMUContext->psDevInfo->sDummyDataDevPAddr.
		     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
		    | SGX_MMU_PTE_VALID;
#else

		pui32Tmp[ui32PTIndex] = 0;
#endif

		CheckPT(ppsPTInfoList[0]);

		sTmpDevVAddr.uiAddr += uPageSize;
	}

	MMU_InvalidatePageTableCache(psMMUHeap->psMMUContext->psDevInfo);

#if defined(PDUMP)
	MMU_PDumpPageTables(psMMUHeap, sDevVAddr, uPageSize * ui32PageCount, 1,
			    hUniqueTag);
#endif
}

IMG_DEV_PHYADDR
MMU_GetPhysPageAddr(MMU_HEAP * pMMUHeap, IMG_DEV_VIRTADDR sDevVPageAddr)
{
	u32 *pui32PageTable;
	u32 ui32Index;
	IMG_DEV_PHYADDR sDevPAddr;
	MMU_PT_INFO **ppsPTInfoList;

	ui32Index = sDevVPageAddr.uiAddr >> pMMUHeap->ui32PDShift;

	ppsPTInfoList = &pMMUHeap->psMMUContext->apsPTInfoList[ui32Index];
	if (!ppsPTInfoList[0]) {
		PVR_DPF((PVR_DBG_ERROR,
			 "MMU_GetPhysPageAddr: Not mapped in at 0x%08x",
			 sDevVPageAddr.uiAddr));
		sDevPAddr.uiAddr = 0;
		return sDevPAddr;
	}

	ui32Index =
	    (sDevVPageAddr.uiAddr & pMMUHeap->ui32PTMask) >> pMMUHeap->
	    ui32PTShift;

	pui32PageTable = (u32 *) ppsPTInfoList[0]->PTPageCpuVAddr;

	sDevPAddr.uiAddr = pui32PageTable[ui32Index];

	sDevPAddr.uiAddr &=
	    ~(pMMUHeap->ui32DataPageMask >> SGX_MMU_PTE_ADDR_ALIGNSHIFT);

	sDevPAddr.uiAddr <<= SGX_MMU_PTE_ADDR_ALIGNSHIFT;

	return sDevPAddr;
}

IMG_DEV_PHYADDR MMU_GetPDDevPAddr(MMU_CONTEXT * pMMUContext)
{
	return (pMMUContext->sPDDevPAddr);
}

PVRSRV_ERROR SGXGetPhysPageAddrKM(void *hDevMemHeap,
				  IMG_DEV_VIRTADDR sDevVAddr,
				  IMG_DEV_PHYADDR * pDevPAddr,
				  IMG_CPU_PHYADDR * pCpuPAddr)
{
	MMU_HEAP *pMMUHeap;
	IMG_DEV_PHYADDR DevPAddr;

	pMMUHeap = (MMU_HEAP *) BM_GetMMUHeap(hDevMemHeap);

	DevPAddr = MMU_GetPhysPageAddr(pMMUHeap, sDevVAddr);
	pCpuPAddr->uiAddr = DevPAddr.uiAddr;
	pDevPAddr->uiAddr = DevPAddr.uiAddr;

	return (pDevPAddr->uiAddr !=
		0) ? PVRSRV_OK : PVRSRV_ERROR_INVALID_PARAMS;
}

PVRSRV_ERROR SGXGetMMUPDAddrKM(void *hDevCookie,
			       void *hDevMemContext,
			       IMG_DEV_PHYADDR * psPDDevPAddr)
{
	if (!hDevCookie || !hDevMemContext || !psPDDevPAddr) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*psPDDevPAddr =
	    ((BM_CONTEXT *) hDevMemContext)->psMMUContext->sPDDevPAddr;

	return PVRSRV_OK;
}

PVRSRV_ERROR MMU_BIFResetPDAlloc(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;
	void *hOSMemHandle = NULL;
	unsigned char *pui8MemBlock = NULL;
	IMG_SYS_PHYADDR sMemBlockSysPAddr;
	IMG_CPU_PHYADDR sMemBlockCpuPAddr;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	if (psLocalDevMemArena == NULL) {

		eError =
		    OSAllocPages(PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY, 3 * SGX_MMU_PAGE_SIZE,
				 SGX_MMU_PAGE_SIZE, (void **)&pui8MemBlock,
				 &hOSMemHandle);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_BIFResetPDAlloc: ERROR call to OSAllocPages failed"));
			return eError;
		}

		if (pui8MemBlock) {
			sMemBlockCpuPAddr = OSMapLinToCPUPhys(pui8MemBlock);
		} else {

			sMemBlockCpuPAddr =
			    OSMemHandleToCpuPAddr(hOSMemHandle, 0);
		}
	} else {

		if (RA_Alloc(psLocalDevMemArena,
			     3 * SGX_MMU_PAGE_SIZE,
			     NULL,
			     NULL,
			     0,
			     SGX_MMU_PAGE_SIZE,
			     0, &(sMemBlockSysPAddr.uiAddr)) != 1) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_BIFResetPDAlloc: ERROR call to RA_Alloc failed"));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		sMemBlockCpuPAddr = SysSysPAddrToCpuPAddr(sMemBlockSysPAddr);
		pui8MemBlock = OSMapPhysToLin(sMemBlockCpuPAddr,
					      SGX_MMU_PAGE_SIZE * 3,
					      PVRSRV_HAP_WRITECOMBINE |
					      PVRSRV_HAP_KERNEL_ONLY,
					      &hOSMemHandle);
		if (!pui8MemBlock) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_BIFResetPDAlloc: ERROR failed to map page tables"));
			return PVRSRV_ERROR_BAD_MAPPING;
		}
	}

	psDevInfo->hBIFResetPDOSMemHandle = hOSMemHandle;
	psDevInfo->sBIFResetPDDevPAddr =
	    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sMemBlockCpuPAddr);
	psDevInfo->sBIFResetPTDevPAddr.uiAddr =
	    psDevInfo->sBIFResetPDDevPAddr.uiAddr + SGX_MMU_PAGE_SIZE;
	psDevInfo->sBIFResetPageDevPAddr.uiAddr =
	    psDevInfo->sBIFResetPTDevPAddr.uiAddr + SGX_MMU_PAGE_SIZE;

	psDevInfo->pui32BIFResetPD = (u32 *) pui8MemBlock;
	psDevInfo->pui32BIFResetPT = (u32 *) (pui8MemBlock + SGX_MMU_PAGE_SIZE);

	memset(psDevInfo->pui32BIFResetPD, 0, SGX_MMU_PAGE_SIZE);
	memset(psDevInfo->pui32BIFResetPT, 0, SGX_MMU_PAGE_SIZE);

	memset(pui8MemBlock + (2 * SGX_MMU_PAGE_SIZE), 0xDB, SGX_MMU_PAGE_SIZE);

	return PVRSRV_OK;
}

void MMU_BIFResetPDFree(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;
	IMG_SYS_PHYADDR sPDSysPAddr;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	if (psLocalDevMemArena == NULL) {
		OSFreePages(PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_KERNEL_ONLY,
			    3 * SGX_MMU_PAGE_SIZE,
			    psDevInfo->pui32BIFResetPD,
			    psDevInfo->hBIFResetPDOSMemHandle);
	} else {
		OSUnMapPhysToLin(psDevInfo->pui32BIFResetPD,
				 3 * SGX_MMU_PAGE_SIZE,
				 PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY,
				 psDevInfo->hBIFResetPDOSMemHandle);

		sPDSysPAddr =
		    SysDevPAddrToSysPAddr(PVRSRV_DEVICE_TYPE_SGX,
					  psDevInfo->sBIFResetPDDevPAddr);
		RA_Free(psLocalDevMemArena, sPDSysPAddr.uiAddr, 0);
	}
}

#if defined(FIX_HW_BRN_22997) && defined(FIX_HW_BRN_23030) && defined(SGX_FEATURE_HOST_PORT)
PVRSRV_ERROR WorkaroundBRN22997Alloc(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;
	void *hPTPageOSMemHandle = NULL;
	void *hPDPageOSMemHandle = NULL;
	u32 *pui32PD = NULL;
	u32 *pui32PT = NULL;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sPTDevPAddr;
	IMG_DEV_PHYADDR sPDDevPAddr;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	if (psLocalDevMemArena == NULL) {

		eError =
		    OSAllocPages(PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				 SGX_MMU_PAGE_SIZE, (void **)&pui32PT,
				 &hPTPageOSMemHandle);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "WorkaroundBRN22997: ERROR call to OSAllocPages failed"));
			return eError;
		}

		eError =
		    OSAllocPages(PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				 SGX_MMU_PAGE_SIZE, (void **)&pui32PD,
				 &hPDPageOSMemHandle);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "WorkaroundBRN22997: ERROR call to OSAllocPages failed"));
			return eError;
		}

		if (pui32PT) {
			sCpuPAddr = OSMapLinToCPUPhys(pui32PT);
		} else {

			sCpuPAddr =
			    OSMemHandleToCpuPAddr(hPTPageOSMemHandle, 0);
		}
		sPTDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

		if (pui32PD) {
			sCpuPAddr = OSMapLinToCPUPhys(pui32PD);
		} else {

			sCpuPAddr =
			    OSMemHandleToCpuPAddr(hPDPageOSMemHandle, 0);
		}
		sPDDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

	} else {

		if (RA_Alloc(psLocalDevMemArena,
			     SGX_MMU_PAGE_SIZE * 2,
			     NULL,
			     NULL,
			     0,
			     SGX_MMU_PAGE_SIZE,
			     0, &(psDevInfo->sBRN22997SysPAddr.uiAddr)) != 1) {
			PVR_DPF((PVR_DBG_ERROR,
				 "WorkaroundBRN22997: ERROR call to RA_Alloc failed"));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		sCpuPAddr = SysSysPAddrToCpuPAddr(psDevInfo->sBRN22997SysPAddr);
		pui32PT = OSMapPhysToLin(sCpuPAddr,
					 SGX_MMU_PAGE_SIZE * 2,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 &hPTPageOSMemHandle);
		if (!pui32PT) {
			PVR_DPF((PVR_DBG_ERROR,
				 "WorkaroundBRN22997: ERROR failed to map page tables"));
			return PVRSRV_ERROR_BAD_MAPPING;
		}

		sPTDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

		pui32PD = pui32PT + 1024;
		sPDDevPAddr.uiAddr = sPTDevPAddr.uiAddr + 4096;
	}

	memset(pui32PD, 0, SGX_MMU_PAGE_SIZE);
	memset(pui32PT, 0, SGX_MMU_PAGE_SIZE);

	PDUMPMALLOCPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE,
			     PDUMP_PD_UNIQUETAG);
	PDUMPMALLOCPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE,
			     PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PT_UNIQUETAG, PDUMP_PD_UNIQUETAG);

	psDevInfo->hBRN22997PTPageOSMemHandle = hPTPageOSMemHandle;
	psDevInfo->hBRN22997PDPageOSMemHandle = hPDPageOSMemHandle;
	psDevInfo->sBRN22997PTDevPAddr = sPTDevPAddr;
	psDevInfo->sBRN22997PDDevPAddr = sPDDevPAddr;
	psDevInfo->pui32BRN22997PD = pui32PD;
	psDevInfo->pui32BRN22997PT = pui32PT;

	return PVRSRV_OK;
}

void WorkaroundBRN22997ReadHostPort(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	u32 *pui32PD = psDevInfo->pui32BRN22997PD;
	u32 *pui32PT = psDevInfo->pui32BRN22997PT;
	u32 ui32PDIndex;
	u32 ui32PTIndex;
	IMG_DEV_VIRTADDR sDevVAddr;
	volatile u32 *pui32HostPort;
	u32 ui32BIFCtrl;

	pui32HostPort =
	    (volatile u32 *)(((u8 *) psDevInfo->pvHostPortBaseKM) +
			     SYS_SGX_HOSTPORT_BRN23030_OFFSET);

	sDevVAddr.uiAddr =
	    SYS_SGX_HOSTPORT_BASE_DEVVADDR + SYS_SGX_HOSTPORT_BRN23030_OFFSET;

	ui32PDIndex =
	    (sDevVAddr.uiAddr & SGX_MMU_PD_MASK) >> (SGX_MMU_PAGE_SHIFT +
						     SGX_MMU_PT_SHIFT);
	ui32PTIndex =
	    (sDevVAddr.uiAddr & SGX_MMU_PT_MASK) >> SGX_MMU_PAGE_SHIFT;

	pui32PD[ui32PDIndex] =
	    (psDevInfo->sBRN22997PTDevPAddr.
	     uiAddr >> SGX_MMU_PDE_ADDR_ALIGNSHIFT)
	    | SGX_MMU_PDE_VALID;

	pui32PT[ui32PTIndex] =
	    (psDevInfo->sBRN22997PTDevPAddr.
	     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
	    | SGX_MMU_PTE_VALID;

	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PT_UNIQUETAG, PDUMP_PD_UNIQUETAG);

	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_DIR_LIST_BASE0,
		     psDevInfo->sBRN22997PDDevPAddr.uiAddr);
	PDUMPPDREG(EUR_CR_BIF_DIR_LIST_BASE0,
		   psDevInfo->sBRN22997PDDevPAddr.uiAddr, PDUMP_PD_UNIQUETAG);

	ui32BIFCtrl = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL,
		     ui32BIFCtrl | EUR_CR_BIF_CTRL_INVALDC_MASK);
	PDUMPREG(EUR_CR_BIF_CTRL, ui32BIFCtrl | EUR_CR_BIF_CTRL_INVALDC_MASK);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL, ui32BIFCtrl);
	PDUMPREG(EUR_CR_BIF_CTRL, ui32BIFCtrl);

	if (pui32HostPort) {

		u32 ui32Tmp;
		ui32Tmp = *pui32HostPort;
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "Host Port not present for BRN22997 workaround"));
	}

	PDUMPCOMMENT("RDW :SGXMEM:v4:%08lX\r\n", sDevVAddr.uiAddr);

	PDUMPCOMMENT("SAB :SGXMEM:v4:%08lX 4 0 hostport.bin", sDevVAddr.uiAddr);

	pui32PD[ui32PDIndex] = 0;
	pui32PT[ui32PTIndex] = 0;

	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PT_UNIQUETAG, PDUMP_PD_UNIQUETAG);

	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL,
		     ui32BIFCtrl | EUR_CR_BIF_CTRL_INVALDC_MASK);
	PDUMPREG(EUR_CR_BIF_CTRL, ui32BIFCtrl | EUR_CR_BIF_CTRL_INVALDC_MASK);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL, ui32BIFCtrl);
	PDUMPREG(EUR_CR_BIF_CTRL, ui32BIFCtrl);
}

void WorkaroundBRN22997Free(PVRSRV_SGXDEV_INFO * psDevInfo)
{
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, psDevInfo->pui32BRN22997PD,
			   SGX_MMU_PAGE_SIZE, PDUMP_PD_UNIQUETAG);
	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, psDevInfo->pui32BRN22997PT,
			   SGX_MMU_PAGE_SIZE, PDUMP_PT_UNIQUETAG);

	if (psLocalDevMemArena == NULL) {
		if (psDevInfo->pui32BRN22997PD != NULL) {
			OSFreePages(PVRSRV_HAP_WRITECOMBINE |
				    PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				    psDevInfo->pui32BRN22997PD,
				    psDevInfo->hBRN22997PDPageOSMemHandle);
		}

		if (psDevInfo->pui32BRN22997PT != NULL) {
			OSFreePages(PVRSRV_HAP_WRITECOMBINE |
				    PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				    psDevInfo->pui32BRN22997PT,
				    psDevInfo->hBRN22997PTPageOSMemHandle);
		}
	} else {
		if (psDevInfo->pui32BRN22997PT != NULL) {
			OSUnMapPhysToLin(psDevInfo->pui32BRN22997PT,
					 SGX_MMU_PAGE_SIZE * 2,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 psDevInfo->hBRN22997PTPageOSMemHandle);

			RA_Free(psLocalDevMemArena,
				psDevInfo->sBRN22997SysPAddr.uiAddr, 0);
		}
	}
}
#endif

#if defined(SUPPORT_EXTERNAL_SYSTEM_CACHE)
PVRSRV_ERROR MMU_MapExtSystemCacheRegs(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;
	void *hPTPageOSMemHandle = NULL;
	u32 *pui32PD;
	u32 *pui32PT = NULL;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sPTDevPAddr;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	u32 ui32PDIndex;
	u32 ui32PTIndex;

	psDevInfo = (PVRSRV_SGXDEV_INFO *) psDeviceNode->pvDevice;
	pui32PD =
	    (u32 *) psDeviceNode->sDevMemoryInfo.pBMKernelContext->
	    psMMUContext->pvPDCpuVAddr;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	if (psLocalDevMemArena == NULL) {

		eError =
		    OSAllocPages(PVRSRV_HAP_WRITECOMBINE |
				 PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				 SGX_MMU_PAGE_SIZE, (void **)&pui32PT,
				 &hPTPageOSMemHandle);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_MapExtSystemCacheRegs: ERROR call to OSAllocPages failed"));
			return eError;
		}

		if (pui32PT) {
			sCpuPAddr = OSMapLinToCPUPhys(pui32PT);
		} else {

			sCpuPAddr =
			    OSMemHandleToCpuPAddr(hPTPageOSMemHandle, 0);
		}
		sPTDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);
	} else {
		IMG_SYS_PHYADDR sSysPAddr;

		if (RA_Alloc(psLocalDevMemArena,
			     SGX_MMU_PAGE_SIZE,
			     NULL,
			     NULL,
			     0,
			     SGX_MMU_PAGE_SIZE, 0, &(sSysPAddr.uiAddr)) != 1) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_MapExtSystemCacheRegs: ERROR call to RA_Alloc failed"));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		sCpuPAddr = SysSysPAddrToCpuPAddr(sSysPAddr);
		pui32PT = OSMapPhysToLin(sCpuPAddr,
					 SGX_MMU_PAGE_SIZE,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 &hPTPageOSMemHandle);
		if (!pui32PT) {
			PVR_DPF((PVR_DBG_ERROR,
				 "MMU_MapExtSystemCacheRegs: ERROR failed to map page tables"));
			return PVRSRV_ERROR_BAD_MAPPING;
		}

		sPTDevPAddr =
		    SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, sCpuPAddr);

		psDevInfo->sExtSystemCacheRegsPTSysPAddr = sSysPAddr;
	}

	memset(pui32PT, 0, SGX_MMU_PAGE_SIZE);

	ui32PDIndex =
	    (SGX_EXT_SYSTEM_CACHE_REGS_DEVVADDR_BASE & SGX_MMU_PD_MASK) >>
	    (SGX_MMU_PAGE_SHIFT + SGX_MMU_PT_SHIFT);
	ui32PTIndex =
	    (SGX_EXT_SYSTEM_CACHE_REGS_DEVVADDR_BASE & SGX_MMU_PT_MASK) >>
	    SGX_MMU_PAGE_SHIFT;

	pui32PD[ui32PDIndex] =
	    (sPTDevPAddr.uiAddr >> SGX_MMU_PDE_ADDR_ALIGNSHIFT)
	    | SGX_MMU_PDE_VALID;

	pui32PT[ui32PTIndex] =
	    (psDevInfo->sExtSysCacheRegsDevPBase.
	     uiAddr >> SGX_MMU_PTE_ADDR_ALIGNSHIFT)
	    | SGX_MMU_PTE_VALID;

	PDUMPMALLOCPAGETABLE(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE,
			     PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PT, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PT_UNIQUETAG, PDUMP_PD_UNIQUETAG);

	psDevInfo->pui32ExtSystemCacheRegsPT = pui32PT;
	psDevInfo->hExtSystemCacheRegsPTPageOSMemHandle = hPTPageOSMemHandle;

	return PVRSRV_OK;
}

PVRSRV_ERROR MMU_UnmapExtSystemCacheRegs(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	SYS_DATA *psSysData;
	RA_ARENA *psLocalDevMemArena;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	u32 ui32PDIndex;
	u32 *pui32PD;

	psDevInfo = (PVRSRV_SGXDEV_INFO *) psDeviceNode->pvDevice;
	pui32PD =
	    (u32 *) psDeviceNode->sDevMemoryInfo.pBMKernelContext->
	    psMMUContext->pvPDCpuVAddr;

	SysAcquireData(&psSysData);

	psLocalDevMemArena = psSysData->apsLocalDevMemArena[0];

	ui32PDIndex =
	    (SGX_EXT_SYSTEM_CACHE_REGS_DEVVADDR_BASE & SGX_MMU_PD_MASK) >>
	    (SGX_MMU_PAGE_SHIFT + SGX_MMU_PT_SHIFT);
	pui32PD[ui32PDIndex] = 0;

	PDUMPMEM2(PVRSRV_DEVICE_TYPE_SGX, pui32PD, SGX_MMU_PAGE_SIZE, 0, 1,
		  PDUMP_PD_UNIQUETAG, PDUMP_PT_UNIQUETAG);
	PDUMPFREEPAGETABLE(PVRSRV_DEVICE_TYPE_SGX,
			   psDevInfo->pui32ExtSystemCacheRegsPT,
			   SGX_MMU_PAGE_SIZE, PDUMP_PT_UNIQUETAG);

	if (psLocalDevMemArena == NULL) {
		if (psDevInfo->pui32ExtSystemCacheRegsPT != NULL) {
			OSFreePages(PVRSRV_HAP_WRITECOMBINE |
				    PVRSRV_HAP_KERNEL_ONLY, SGX_MMU_PAGE_SIZE,
				    psDevInfo->pui32ExtSystemCacheRegsPT,
				    psDevInfo->
				    hExtSystemCacheRegsPTPageOSMemHandle);
		}
	} else {
		if (psDevInfo->pui32ExtSystemCacheRegsPT != NULL) {
			OSUnMapPhysToLin(psDevInfo->pui32ExtSystemCacheRegsPT,
					 SGX_MMU_PAGE_SIZE,
					 PVRSRV_HAP_WRITECOMBINE |
					 PVRSRV_HAP_KERNEL_ONLY,
					 psDevInfo->
					 hExtSystemCacheRegsPTPageOSMemHandle);

			RA_Free(psLocalDevMemArena,
				psDevInfo->sExtSystemCacheRegsPTSysPAddr.uiAddr,
				0);
		}
	}

	return PVRSRV_OK;
}
#endif

#if PAGE_TEST
static void PageTest(void *pMem, IMG_DEV_PHYADDR sDevPAddr)
{
	volatile u32 ui32WriteData;
	volatile u32 ui32ReadData;
	volatile u32 *pMem32 = (volatile u32 *)pMem;
	int n;
	int bOK = 1;

	ui32WriteData = 0xffffffff;

	for (n = 0; n < 1024; n++) {
		pMem32[n] = ui32WriteData;
		ui32ReadData = pMem32[n];

		if (ui32WriteData != ui32ReadData) {

			PVR_DPF((PVR_DBG_ERROR,
				 "Error - memory page test failed at device phys address 0x%08X",
				 sDevPAddr.uiAddr + (n << 2)));
			PVR_DBG_BREAK;
			bOK = 0;
		}
	}

	ui32WriteData = 0;

	for (n = 0; n < 1024; n++) {
		pMem32[n] = ui32WriteData;
		ui32ReadData = pMem32[n];

		if (ui32WriteData != ui32ReadData) {

			PVR_DPF((PVR_DBG_ERROR,
				 "Error - memory page test failed at device phys address 0x%08X",
				 sDevPAddr.uiAddr + (n << 2)));
			PVR_DBG_BREAK;
			bOK = 0;
		}
	}

	if (bOK) {
		PVR_DPF((PVR_DBG_VERBOSE, "MMU Page 0x%08X is OK",
			 sDevPAddr.uiAddr));
	} else {
		PVR_DPF((PVR_DBG_VERBOSE, "MMU Page 0x%08X *** FAILED ***",
			 sDevPAddr.uiAddr));
	}
}
#endif

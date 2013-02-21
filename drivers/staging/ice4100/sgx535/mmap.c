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

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/shmparam.h>
#include <asm/pgtable.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <drm/drmP.h>


#include "services.h"
#include "servicesint.h"
#include "pvrmmap.h"
#include "mutils.h"
#include "mmap.h"
#include "mm.h"
#include "pvr_debug.h"
#include "osfunc.h"
#include "proc.h"
#include "handle.h"
#include "perproc.h"
#include "env_perproc.h"
#include "bridged_support.h"
#if defined(SUPPORT_DRI_DRM)
#include "pvr_drm.h"
#endif

#if !defined(PVR_SECURE_HANDLES)
#error "The mmap code requires PVR_SECURE_HANDLES"
#endif

static struct mutex g_sMMapMutex;

static LinuxKMemCache *g_psMemmapCache = NULL;
static LIST_HEAD(g_sMMapAreaList);
static LIST_HEAD(g_sMMapOffsetStructList);
#if defined(DEBUG_LINUX_MMAP_AREAS)
static u32 g_ui32RegisteredAreas = 0;
static u32 g_ui32TotalByteSize = 0;
#endif

#if defined(PVR_PROC_USE_SEQ_FILE) && defined(DEBUG_LINUX_MMAP_AREAS)
static struct proc_dir_entry *g_ProcMMap;
#endif

#define	FIRST_PHYSICAL_PFN	0
#define	LAST_PHYSICAL_PFN	0x7fffffffUL
#define	FIRST_SPECIAL_PFN	(LAST_PHYSICAL_PFN + 1)
#define	LAST_SPECIAL_PFN	0xffffffffUL

#define	MAX_MMAP_HANDLE		0x7fffffffUL

static inline int PFNIsPhysical(u32 pfn)
{

	return ((pfn >= FIRST_PHYSICAL_PFN)
		&& (pfn <= LAST_PHYSICAL_PFN)) ? 1 : 0;
}

static inline int PFNIsSpecial(u32 pfn)
{

	return ((pfn >= FIRST_SPECIAL_PFN)
		&& (pfn <= LAST_SPECIAL_PFN)) ? 1 : 0;
}

static inline void *MMapOffsetToHandle(u32 pfn)
{
	if (PFNIsPhysical(pfn)) {
		PVR_ASSERT(PFNIsPhysical(pfn));
		return NULL;
	}

	return (void *)(pfn - FIRST_SPECIAL_PFN);
}

static inline u32 HandleToMMapOffset(void *hHandle)
{
	u32 ulHandle = (u32) hHandle;

	if (PFNIsSpecial(ulHandle)) {
		PVR_ASSERT(PFNIsSpecial(ulHandle));
		return 0;
	}

	return ulHandle + FIRST_SPECIAL_PFN;
}

static inline int LinuxMemAreaUsesPhysicalMap(LinuxMemArea * psLinuxMemArea)
{
	return LinuxMemAreaPhysIsContig(psLinuxMemArea);
}

static inline u32 GetCurrentThreadID(void)
{

	return (u32) current->pid;
}

static PKV_OFFSET_STRUCT
CreateOffsetStruct(LinuxMemArea * psLinuxMemArea, u32 ui32Offset,
		   u32 ui32RealByteSize)
{
	PKV_OFFSET_STRUCT psOffsetStruct;
#if defined(DEBUG) || defined(DEBUG_LINUX_MMAP_AREAS)
	const char *pszName =
	    LinuxMemAreaTypeToString(LinuxMemAreaRootType(psLinuxMemArea));
#endif

#if defined(DEBUG) || defined(DEBUG_LINUX_MMAP_AREAS)
	PVR_DPF((PVR_DBG_MESSAGE,
		 "%s(%s, psLinuxMemArea: 0x%p, ui32AllocFlags: 0x%8lx)",
		 __FUNCTION__, pszName, psLinuxMemArea,
		 psLinuxMemArea->ui32AreaFlags));
#endif

	PVR_ASSERT(psLinuxMemArea->eAreaType != LINUX_MEM_AREA_SUB_ALLOC
		   || LinuxMemAreaRoot(psLinuxMemArea)->eAreaType !=
		   LINUX_MEM_AREA_SUB_ALLOC);

	PVR_ASSERT(psLinuxMemArea->bMMapRegistered);

	psOffsetStruct = KMemCacheAllocWrapper(g_psMemmapCache, GFP_KERNEL);
	if (psOffsetStruct == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRMMapRegisterArea: Couldn't alloc another mapping record from cache"));
		return NULL;
	}

	psOffsetStruct->ui32MMapOffset = ui32Offset;

	psOffsetStruct->psLinuxMemArea = psLinuxMemArea;

	psOffsetStruct->ui32Mapped = 0;

	psOffsetStruct->ui32RealByteSize = ui32RealByteSize;

	psOffsetStruct->ui32TID = GetCurrentThreadID();

	psOffsetStruct->ui32PID = OSGetCurrentProcessIDKM();

	psOffsetStruct->bOnMMapList = 0;

	psOffsetStruct->ui32RefCount = 0;

	psOffsetStruct->ui32UserVAddr = 0;

#if defined(DEBUG_LINUX_MMAP_AREAS)

	psOffsetStruct->pszName = pszName;
#endif

	list_add_tail(&psOffsetStruct->sAreaItem,
		      &psLinuxMemArea->sMMapOffsetStructList);

	return psOffsetStruct;
}

static void DestroyOffsetStruct(PKV_OFFSET_STRUCT psOffsetStruct)
{
	list_del(&psOffsetStruct->sAreaItem);

	if (psOffsetStruct->bOnMMapList) {
		list_del(&psOffsetStruct->sMMapItem);
	}

	PVR_DPF((PVR_DBG_MESSAGE, "%s: Table entry: "
		 "psLinuxMemArea=0x%08lX, CpuPAddr=0x%08lX", __FUNCTION__,
		 psOffsetStruct->psLinuxMemArea,
		 LinuxMemAreaToCpuPAddr(psOffsetStruct->psLinuxMemArea, 0)));

	KMemCacheFreeWrapper(g_psMemmapCache, psOffsetStruct);
}

static inline void
DetermineUsersSizeAndByteOffset(LinuxMemArea * psLinuxMemArea,
				u32 * pui32RealByteSize, u32 * pui32ByteOffset)
{
	u32 ui32PageAlignmentOffset;
	IMG_CPU_PHYADDR CpuPAddr;

	CpuPAddr = LinuxMemAreaToCpuPAddr(psLinuxMemArea, 0);
	ui32PageAlignmentOffset = ADDR_TO_PAGE_OFFSET(CpuPAddr.uiAddr);

	*pui32ByteOffset = ui32PageAlignmentOffset;

	*pui32RealByteSize =
	    PAGE_ALIGN(psLinuxMemArea->ui32ByteSize + ui32PageAlignmentOffset);
}

PVRSRV_ERROR
PVRMMapOSMemHandleToMMapData(PVRSRV_PER_PROCESS_DATA * psPerProc,
			     void *hMHandle,
			     u32 * pui32MMapOffset,
			     u32 * pui32ByteOffset,
			     u32 * pui32RealByteSize, u32 * pui32UserVAddr)
{
	LinuxMemArea *psLinuxMemArea;
	PKV_OFFSET_STRUCT psOffsetStruct;
	void *hOSMemHandle;
	PVRSRV_ERROR eError;

	mutex_lock(&g_sMMapMutex);

	PVR_ASSERT(PVRSRVGetMaxHandle(psPerProc->psHandleBase) <=
		   MAX_MMAP_HANDLE);

	eError =
	    PVRSRVLookupOSMemHandle(psPerProc->psHandleBase, &hOSMemHandle,
				    hMHandle);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Lookup of handle 0x%lx failed",
			 __FUNCTION__, hMHandle));

		goto exit_unlock;
	}

	psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;

	DetermineUsersSizeAndByteOffset(psLinuxMemArea,
					pui32RealByteSize, pui32ByteOffset);

	list_for_each_entry(psOffsetStruct,
			    &psLinuxMemArea->sMMapOffsetStructList, sAreaItem) {
		if (psPerProc->ui32PID == psOffsetStruct->ui32PID) {

			PVR_ASSERT(*pui32RealByteSize ==
				   psOffsetStruct->ui32RealByteSize);

			*pui32MMapOffset = psOffsetStruct->ui32MMapOffset;
			*pui32UserVAddr = psOffsetStruct->ui32UserVAddr;
			psOffsetStruct->ui32RefCount++;

			eError = PVRSRV_OK;
			goto exit_unlock;
		}
	}

	*pui32UserVAddr = 0;

	if (LinuxMemAreaUsesPhysicalMap(psLinuxMemArea)) {
		*pui32MMapOffset = LinuxMemAreaToCpuPFN(psLinuxMemArea, 0);
		PVR_ASSERT(PFNIsPhysical(*pui32MMapOffset));
	} else {
		*pui32MMapOffset = HandleToMMapOffset(hMHandle);
		PVR_ASSERT(PFNIsSpecial(*pui32MMapOffset));
	}

	psOffsetStruct =
	    CreateOffsetStruct(psLinuxMemArea, *pui32MMapOffset,
			       *pui32RealByteSize);
	if (psOffsetStruct == NULL) {
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto exit_unlock;
	}

	list_add_tail(&psOffsetStruct->sMMapItem, &g_sMMapOffsetStructList);

	psOffsetStruct->bOnMMapList = 1;

	psOffsetStruct->ui32RefCount++;

	eError = PVRSRV_OK;

exit_unlock:
	mutex_unlock(&g_sMMapMutex);

	return eError;
}

PVRSRV_ERROR
PVRMMapReleaseMMapData(PVRSRV_PER_PROCESS_DATA * psPerProc,
		       void *hMHandle,
		       int *pbMUnmap,
		       u32 * pui32RealByteSize, u32 * pui32UserVAddr)
{
	LinuxMemArea *psLinuxMemArea;
	PKV_OFFSET_STRUCT psOffsetStruct;
	void *hOSMemHandle;
	PVRSRV_ERROR eError;
	u32 ui32PID = OSGetCurrentProcessIDKM();

	mutex_lock(&g_sMMapMutex);

	PVR_ASSERT(PVRSRVGetMaxHandle(psPerProc->psHandleBase) <=
		   MAX_MMAP_HANDLE);

	eError =
	    PVRSRVLookupOSMemHandle(psPerProc->psHandleBase, &hOSMemHandle,
				    hMHandle);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Lookup of handle 0x%lx failed",
			 __FUNCTION__, hMHandle));

		goto exit_unlock;
	}

	psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;

	list_for_each_entry(psOffsetStruct,
			    &psLinuxMemArea->sMMapOffsetStructList, sAreaItem) {
		if (psOffsetStruct->ui32PID == ui32PID) {
			if (psOffsetStruct->ui32RefCount == 0) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s: Attempt to release mmap data with zero reference count for offset struct 0x%p, memory area 0x%p",
					 __FUNCTION__, psOffsetStruct,
					 psLinuxMemArea));
				eError = PVRSRV_ERROR_GENERIC;
				goto exit_unlock;
			}

			psOffsetStruct->ui32RefCount--;

			*pbMUnmap = (int)((psOffsetStruct->ui32RefCount == 0)
					  && (psOffsetStruct->ui32UserVAddr !=
					      0));

			*pui32UserVAddr =
			    (*pbMUnmap) ? psOffsetStruct->ui32UserVAddr : 0;
			*pui32RealByteSize =
			    (*pbMUnmap) ? psOffsetStruct->ui32RealByteSize : 0;

			eError = PVRSRV_OK;
			goto exit_unlock;
		}
	}

	PVR_DPF((PVR_DBG_ERROR,
		 "%s: Mapping data not found for handle 0x%lx (memory area 0x%p)",
		 __FUNCTION__, hMHandle, psLinuxMemArea));

	eError = PVRSRV_ERROR_GENERIC;

exit_unlock:
	mutex_unlock(&g_sMMapMutex);

	return eError;
}

static inline PKV_OFFSET_STRUCT
FindOffsetStructByOffset(u32 ui32Offset, u32 ui32RealByteSize)
{
	PKV_OFFSET_STRUCT psOffsetStruct;
	u32 ui32TID = GetCurrentThreadID();
	u32 ui32PID = OSGetCurrentProcessIDKM();

	list_for_each_entry(psOffsetStruct, &g_sMMapOffsetStructList, sMMapItem) {
		if (ui32Offset == psOffsetStruct->ui32MMapOffset
		    && ui32RealByteSize == psOffsetStruct->ui32RealByteSize
		    && psOffsetStruct->ui32PID == ui32PID) {

			if (!PFNIsPhysical(ui32Offset)
			    || psOffsetStruct->ui32TID == ui32TID) {
				return psOffsetStruct;
			}
		}
	}

	return NULL;
}

static int
DoMapToUser(LinuxMemArea * psLinuxMemArea,
	    struct vm_area_struct *ps_vma, u32 ui32ByteOffset)
{
	u32 ui32ByteSize;

	if (psLinuxMemArea->eAreaType == LINUX_MEM_AREA_SUB_ALLOC) {
		return DoMapToUser(LinuxMemAreaRoot(psLinuxMemArea),
				   ps_vma,
				   psLinuxMemArea->uData.sSubAlloc.
				   ui32ByteOffset + ui32ByteOffset);
	}

	ui32ByteSize = ps_vma->vm_end - ps_vma->vm_start;
	PVR_ASSERT(ADDR_TO_PAGE_OFFSET(ui32ByteSize) == 0);

#if defined (__sparc__)

#error "SPARC not supported"
#endif

	if (PFNIsPhysical(ps_vma->vm_pgoff)) {
		int result;

		PVR_ASSERT(LinuxMemAreaPhysIsContig(psLinuxMemArea));
		PVR_ASSERT(LinuxMemAreaToCpuPFN(psLinuxMemArea, ui32ByteOffset)
			   == ps_vma->vm_pgoff);

		result =
		    IO_REMAP_PFN_RANGE(ps_vma, ps_vma->vm_start,
				       ps_vma->vm_pgoff, ui32ByteSize,
				       ps_vma->vm_page_prot);

		if (result == 0) {
			return 1;
		}

		PVR_DPF((PVR_DBG_MESSAGE,
			 "%s: Failed to map contiguous physical address range (%d), trying non-contiguous path",
			 __FUNCTION__, result));
	}

	{

		u32 ulVMAPos;
		u32 ui32ByteEnd = ui32ByteOffset + ui32ByteSize;
		u32 ui32PA;

		for (ui32PA = ui32ByteOffset; ui32PA < ui32ByteEnd;
		     ui32PA += PAGE_SIZE) {
			u32 pfn = LinuxMemAreaToCpuPFN(psLinuxMemArea, ui32PA);

			if (!pfn_valid(pfn)) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s: Error - PFN invalid: 0x%lx",
					 __FUNCTION__, pfn));
				return 0;
			}
		}

		ulVMAPos = ps_vma->vm_start;
		for (ui32PA = ui32ByteOffset; ui32PA < ui32ByteEnd;
		     ui32PA += PAGE_SIZE) {
			u32 pfn;
			struct page *psPage;
			int result;

			pfn = LinuxMemAreaToCpuPFN(psLinuxMemArea, ui32PA);
			PVR_ASSERT(pfn_valid(pfn));

			psPage = pfn_to_page(pfn);

			result = VM_INSERT_PAGE(ps_vma, ulVMAPos, psPage);
			if (result != 0) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s: Error - VM_INSERT_PAGE failed (%d)",
					 __FUNCTION__, result));
				return 0;
			}
			ulVMAPos += PAGE_SIZE;
		}
	}

	return 1;
}

static void MMapVOpenNoLock(struct vm_area_struct *ps_vma)
{
	PKV_OFFSET_STRUCT psOffsetStruct =
	    (PKV_OFFSET_STRUCT) ps_vma->vm_private_data;
	PVR_ASSERT(psOffsetStruct != NULL)
	    psOffsetStruct->ui32Mapped++;
	PVR_ASSERT(!psOffsetStruct->bOnMMapList);

	if (psOffsetStruct->ui32Mapped > 1) {
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: Offset structure 0x%p is being shared across processes (psOffsetStruct->ui32Mapped: %lu)",
			 __FUNCTION__, psOffsetStruct,
			 psOffsetStruct->ui32Mapped));
		PVR_ASSERT((ps_vma->vm_flags & VM_DONTCOPY) == 0);
	}
#if defined(DEBUG_LINUX_MMAP_AREAS)

	PVR_DPF((PVR_DBG_MESSAGE,
		 "%s: psLinuxMemArea 0x%p, KVAddress 0x%p MMapOffset %ld, ui32Mapped %d",
		 __FUNCTION__,
		 psOffsetStruct->psLinuxMemArea,
		 LinuxMemAreaToCpuVAddr(psOffsetStruct->psLinuxMemArea),
		 psOffsetStruct->ui32MMapOffset, psOffsetStruct->ui32Mapped));
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
	MOD_INC_USE_COUNT;
#endif
}

static void MMapVOpen(struct vm_area_struct *ps_vma)
{
	mutex_lock(&g_sMMapMutex);

	MMapVOpenNoLock(ps_vma);

	mutex_unlock(&g_sMMapMutex);
}

static void MMapVCloseNoLock(struct vm_area_struct *ps_vma)
{
	PKV_OFFSET_STRUCT psOffsetStruct =
	    (PKV_OFFSET_STRUCT) ps_vma->vm_private_data;
	PVR_ASSERT(psOffsetStruct != NULL)
#if defined(DEBUG_LINUX_MMAP_AREAS)
	    PVR_DPF((PVR_DBG_MESSAGE,
		     "%s: psLinuxMemArea 0x%p, CpuVAddr 0x%p ui32MMapOffset %ld, ui32Mapped %d",
		     __FUNCTION__,
		     psOffsetStruct->psLinuxMemArea,
		     LinuxMemAreaToCpuVAddr(psOffsetStruct->psLinuxMemArea),
		     psOffsetStruct->ui32MMapOffset,
		     psOffsetStruct->ui32Mapped));
#endif

	PVR_ASSERT(!psOffsetStruct->bOnMMapList);
	psOffsetStruct->ui32Mapped--;
	if (psOffsetStruct->ui32Mapped == 0) {
		if (psOffsetStruct->ui32RefCount != 0) {
			PVR_DPF((PVR_DBG_MESSAGE,
				 "%s: psOffsetStruct 0x%p has non-zero reference count (ui32RefCount = %lu). User mode address of start of mapping: 0x%lx",
				 __FUNCTION__, psOffsetStruct,
				 psOffsetStruct->ui32RefCount,
				 psOffsetStruct->ui32UserVAddr));
		}

		DestroyOffsetStruct(psOffsetStruct);
	}

	ps_vma->vm_private_data = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
	MOD_DEC_USE_COUNT;
#endif
}

static void MMapVClose(struct vm_area_struct *ps_vma)
{
	mutex_lock(&g_sMMapMutex);

	MMapVCloseNoLock(ps_vma);

	mutex_unlock(&g_sMMapMutex);
}

static struct vm_operations_struct MMapIOOps = {
	.open = MMapVOpen,
	.close = MMapVClose
};

int PVRMMap(struct file *pFile, struct vm_area_struct *ps_vma)
{
	u32 ui32ByteSize;
	PKV_OFFSET_STRUCT psOffsetStruct;
	int iRetVal = 0;

	mutex_lock(&g_sMMapMutex);

	ui32ByteSize = ps_vma->vm_end - ps_vma->vm_start;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "%s: Received mmap(2) request with ui32MMapOffset 0x%08lx,"
		 " and ui32ByteSize %ld(0x%08lx)", __FUNCTION__,
		 ps_vma->vm_pgoff, ui32ByteSize, ui32ByteSize));

	psOffsetStruct =
	    FindOffsetStructByOffset(ps_vma->vm_pgoff, ui32ByteSize);
	if (psOffsetStruct == NULL) {
#if defined(SUPPORT_DRI_DRM)
		mutex_lock(&g_sMMapMutex);

		return drm_mmap(pFile, ps_vma);
#else
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Attempted to mmap unregistered area at vm_pgoff %ld",
			 __FUNCTION__, ps_vma->vm_pgoff));
		iRetVal = -EINVAL;
#endif
		goto unlock_and_return;
	}
	list_del(&psOffsetStruct->sMMapItem);
	psOffsetStruct->bOnMMapList = 0;

	if (((ps_vma->vm_flags & VM_WRITE) != 0) &&
	    ((ps_vma->vm_flags & VM_SHARED) == 0)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Cannot mmap non-shareable writable areas",
			 __FUNCTION__));
		iRetVal = -EINVAL;
		goto unlock_and_return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "%s: Mapped psLinuxMemArea 0x%p\n",
		 __FUNCTION__, psOffsetStruct->psLinuxMemArea));

	ps_vma->vm_flags |= VM_RESERVED;
	ps_vma->vm_flags |= VM_IO;

	ps_vma->vm_flags |= VM_DONTEXPAND;

	ps_vma->vm_flags |= VM_DONTCOPY;

	ps_vma->vm_private_data = (void *)psOffsetStruct;

	switch (psOffsetStruct->psLinuxMemArea->
		ui32AreaFlags & PVRSRV_HAP_CACHETYPE_MASK) {
	case PVRSRV_HAP_CACHED:

		break;
	case PVRSRV_HAP_WRITECOMBINE:
		ps_vma->vm_page_prot = PGPROT_WC(ps_vma->vm_page_prot);
		break;
	case PVRSRV_HAP_UNCACHED:
		ps_vma->vm_page_prot = PGPROT_UC(ps_vma->vm_page_prot);
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR, "%s: unknown cache type",
			 __FUNCTION__));
		iRetVal = -EINVAL;
		goto unlock_and_return;
	}

	ps_vma->vm_ops = &MMapIOOps;

	if (!DoMapToUser(psOffsetStruct->psLinuxMemArea, ps_vma, 0)) {
		iRetVal = -EAGAIN;
		goto unlock_and_return;
	}

	PVR_ASSERT(psOffsetStruct->ui32UserVAddr == 0)

	    psOffsetStruct->ui32UserVAddr = ps_vma->vm_start;

	MMapVOpenNoLock(ps_vma);

	PVR_DPF((PVR_DBG_MESSAGE, "%s: Mapped area at offset 0x%08lx\n",
		 __FUNCTION__, ps_vma->vm_pgoff));

unlock_and_return:
	if (iRetVal != 0 && psOffsetStruct != NULL) {
		DestroyOffsetStruct(psOffsetStruct);
	}

	mutex_unlock(&g_sMMapMutex);

	return iRetVal;
}

#if defined(DEBUG_LINUX_MMAP_AREAS)

#ifdef PVR_PROC_USE_SEQ_FILE

static void ProcSeqStartstopMMapRegistations(struct seq_file *sfile, int start)
{
	if (start) {
		mutex_lock(&g_sMMapMutex);
	} else {
		mutex_unlock(&g_sMMapMutex);
	}
}

static void *ProcSeqOff2ElementMMapRegistrations(struct seq_file *sfile,
						 loff_t off)
{
	LinuxMemArea *psLinuxMemArea;
	if (!off) {
		return PVR_PROC_SEQ_START_TOKEN;
	}

	list_for_each_entry(psLinuxMemArea, &g_sMMapAreaList, sMMapItem) {
		PKV_OFFSET_STRUCT psOffsetStruct;

		list_for_each_entry(psOffsetStruct,
				    &psLinuxMemArea->sMMapOffsetStructList,
				    sAreaItem) {
			off--;
			if (off == 0) {
				PVR_ASSERT(psOffsetStruct->psLinuxMemArea ==
					   psLinuxMemArea);
				return (void *)psOffsetStruct;
			}
		}
	}
	return (void *)0;
}

static void *ProcSeqNextMMapRegistrations(struct seq_file *sfile, void *el,
					  loff_t off)
{
	return ProcSeqOff2ElementMMapRegistrations(sfile, off);
}

static void ProcSeqShowMMapRegistrations(struct seq_file *sfile, void *el)
{
	KV_OFFSET_STRUCT *psOffsetStruct = (KV_OFFSET_STRUCT *) el;
	LinuxMemArea *psLinuxMemArea;
	u32 ui32RealByteSize;
	u32 ui32ByteOffset;

	if (el == PVR_PROC_SEQ_START_TOKEN) {
		seq_printf(sfile,
#if !defined(DEBUG_LINUX_XML_PROC_FILES)
			   "Allocations registered for mmap: %lu\n"
			   "In total these areas correspond to %lu bytes\n"
			   "psLinuxMemArea "
			   "UserVAddr "
			   "KernelVAddr "
			   "CpuPAddr "
			   "MMapOffset "
			   "ByteLength "
			   "LinuxMemType             " "Pid   Name     Flags\n",
#else
			   "<mmap_header>\n"
			   "\t<count>%lu</count>\n"
			   "\t<bytes>%lu</bytes>\n" "</mmap_header>\n",
#endif
			   g_ui32RegisteredAreas, g_ui32TotalByteSize);
		return;
	}

	psLinuxMemArea = psOffsetStruct->psLinuxMemArea;

	DetermineUsersSizeAndByteOffset(psLinuxMemArea,
					&ui32RealByteSize, &ui32ByteOffset);

	seq_printf(sfile,
#if !defined(DEBUG_LINUX_XML_PROC_FILES)
		   "%-8p       %08lx %-8p %08lx %08lx   %-8ld   %-24s %-5lu %-8s %08lx(%s)\n",
#else
		   "<mmap_record>\n"
		   "\t<pointer>%-8p</pointer>\n"
		   "\t<user_virtual>%-8lx</user_virtual>\n"
		   "\t<kernel_virtual>%-8p</kernel_virtual>\n"
		   "\t<cpu_physical>%08lx</cpu_physical>\n"
		   "\t<mmap_offset>%08lx</mmap_offset>\n"
		   "\t<bytes>%-8ld</bytes>\n"
		   "\t<linux_mem_area_type>%-24s</linux_mem_area_type>\n"
		   "\t<pid>%-5lu</pid>\n"
		   "\t<name>%-8s</name>\n"
		   "\t<flags>%08lx</flags>\n"
		   "\t<flags_string>%s</flags_string>\n" "</mmap_record>\n",
#endif
		   psLinuxMemArea,
		   psOffsetStruct->ui32UserVAddr + ui32ByteOffset,
		   LinuxMemAreaToCpuVAddr(psLinuxMemArea),
		   LinuxMemAreaToCpuPAddr(psLinuxMemArea, 0).uiAddr,
		   psOffsetStruct->ui32MMapOffset,
		   psLinuxMemArea->ui32ByteSize,
		   LinuxMemAreaTypeToString(psLinuxMemArea->eAreaType),
		   psOffsetStruct->ui32PID,
		   psOffsetStruct->pszName,
		   psLinuxMemArea->ui32AreaFlags,
		   HAPFlagsToString(psLinuxMemArea->ui32AreaFlags));
}

#else

static off_t PrintMMapRegistrations(char *buffer, size_t size, off_t off)
{
	LinuxMemArea *psLinuxMemArea;
	off_t Ret;

	mutex_lock(&g_sMMapMutex);

	if (!off) {
		Ret = printAppend(buffer, size, 0,
#if !defined(DEBUG_LINUX_XML_PROC_FILES)
				  "Allocations registered for mmap: %lu\n"
				  "In total these areas correspond to %lu bytes\n"
				  "psLinuxMemArea "
				  "UserVAddr "
				  "KernelVAddr "
				  "CpuPAddr "
				  "MMapOffset "
				  "ByteLength "
				  "LinuxMemType             "
				  "Pid   Name     Flags\n",
#else
				  "<mmap_header>\n"
				  "\t<count>%lu</count>\n"
				  "\t<bytes>%lu</bytes>\n" "</mmap_header>\n",
#endif
				  g_ui32RegisteredAreas, g_ui32TotalByteSize);

		goto unlock_and_return;
	}

	if (size < 135) {
		Ret = 0;
		goto unlock_and_return;
	}

	PVR_ASSERT(off != 0);
	list_for_each_entry(psLinuxMemArea, &g_sMMapAreaList, sMMapItem) {
		PKV_OFFSET_STRUCT psOffsetStruct;

		list_for_each_entry(psOffsetStruct,
				    &psLinuxMemArea->sMMapOffsetStructList,
				    sAreaItem) {
			off--;
			if (off == 0) {
				u32 ui32RealByteSize;
				u32 ui32ByteOffset;

				PVR_ASSERT(psOffsetStruct->psLinuxMemArea ==
					   psLinuxMemArea);

				DetermineUsersSizeAndByteOffset(psLinuxMemArea,
								&ui32RealByteSize,
								&ui32ByteOffset);

				Ret = printAppend(buffer, size, 0,
#if !defined(DEBUG_LINUX_XML_PROC_FILES)
						  "%-8p       %08lx %-8p %08lx %08lx   %-8ld   %-24s %-5lu %-8s %08lx(%s)\n",
#else
						  "<mmap_record>\n"
						  "\t<pointer>%-8p</pointer>\n"
						  "\t<user_virtual>%-8lx</user_virtual>\n"
						  "\t<kernel_virtual>%-8p</kernel_virtual>\n"
						  "\t<cpu_physical>%08lx</cpu_physical>\n"
						  "\t<mmap_offset>%08lx</mmap_offset>\n"
						  "\t<bytes>%-8ld</bytes>\n"
						  "\t<linux_mem_area_type>%-24s</linux_mem_area_type>\n"
						  "\t<pid>%-5lu</pid>\n"
						  "\t<name>%-8s</name>\n"
						  "\t<flags>%08lx</flags>\n"
						  "\t<flags_string>%s</flags_string>\n"
						  "</mmap_record>\n",
#endif
						  psLinuxMemArea,
						  psOffsetStruct->
						  ui32UserVAddr +
						  ui32ByteOffset,
						  LinuxMemAreaToCpuVAddr
						  (psLinuxMemArea),
						  LinuxMemAreaToCpuPAddr
						  (psLinuxMemArea, 0).uiAddr,
						  psOffsetStruct->
						  ui32MMapOffset,
						  psLinuxMemArea->ui32ByteSize,
						  LinuxMemAreaTypeToString
						  (psLinuxMemArea->eAreaType),
						  psOffsetStruct->ui32PID,
						  psOffsetStruct->pszName,
						  psLinuxMemArea->ui32AreaFlags,
						  HAPFlagsToString
						  (psLinuxMemArea->
						   ui32AreaFlags));
				goto unlock_and_return;
			}
		}
	}
	Ret = END_OF_FILE;

unlock_and_return:
	mutex_unlock(&g_sMMapMutex);
	return Ret;
}
#endif
#endif

PVRSRV_ERROR PVRMMapRegisterArea(LinuxMemArea * psLinuxMemArea)
{
	PVRSRV_ERROR eError;
#if defined(DEBUG) || defined(DEBUG_LINUX_MMAP_AREAS)
	const char *pszName =
	    LinuxMemAreaTypeToString(LinuxMemAreaRootType(psLinuxMemArea));
#endif

	mutex_lock(&g_sMMapMutex);

#if defined(DEBUG) || defined(DEBUG_LINUX_MMAP_AREAS)
	PVR_DPF((PVR_DBG_MESSAGE,
		 "%s(%s, psLinuxMemArea 0x%p, ui32AllocFlags 0x%8lx)",
		 __FUNCTION__, pszName, psLinuxMemArea,
		 psLinuxMemArea->ui32AreaFlags));
#endif

	PVR_ASSERT(psLinuxMemArea->eAreaType != LINUX_MEM_AREA_SUB_ALLOC
		   || LinuxMemAreaRoot(psLinuxMemArea)->eAreaType !=
		   LINUX_MEM_AREA_SUB_ALLOC);

	if (psLinuxMemArea->bMMapRegistered) {
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: psLinuxMemArea 0x%p is already registered",
			 __FUNCTION__, psLinuxMemArea));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto exit_unlock;
	}

	list_add_tail(&psLinuxMemArea->sMMapItem, &g_sMMapAreaList);

	psLinuxMemArea->bMMapRegistered = 1;

#if defined(DEBUG_LINUX_MMAP_AREAS)
	g_ui32RegisteredAreas++;

	if (psLinuxMemArea->eAreaType != LINUX_MEM_AREA_SUB_ALLOC) {
		g_ui32TotalByteSize += psLinuxMemArea->ui32ByteSize;
	}
#endif

	eError = PVRSRV_OK;

exit_unlock:
	mutex_unlock(&g_sMMapMutex);

	return eError;
}

PVRSRV_ERROR PVRMMapRemoveRegisteredArea(LinuxMemArea * psLinuxMemArea)
{
	PVRSRV_ERROR eError;
	PKV_OFFSET_STRUCT psOffsetStruct, psTmpOffsetStruct;

	mutex_lock(&g_sMMapMutex);

	PVR_ASSERT(psLinuxMemArea->bMMapRegistered);

	list_for_each_entry_safe(psOffsetStruct, psTmpOffsetStruct,
				 &psLinuxMemArea->sMMapOffsetStructList,
				 sAreaItem) {
		if (psOffsetStruct->ui32Mapped != 0) {
			PVR_DPF((PVR_DBG_ERROR,
				 "%s: psOffsetStruct 0x%p for memory area 0x0x%p is still mapped; psOffsetStruct->ui32Mapped %lu",
				 __FUNCTION__, psOffsetStruct, psLinuxMemArea,
				 psOffsetStruct->ui32Mapped));
			eError = PVRSRV_ERROR_GENERIC;
			goto exit_unlock;
		} else {

			PVR_DPF((PVR_DBG_WARNING,
				 "%s: psOffsetStruct 0x%p was never mapped",
				 __FUNCTION__, psOffsetStruct));
		}

		PVR_ASSERT((psOffsetStruct->ui32Mapped == 0)
			   && psOffsetStruct->bOnMMapList);

		DestroyOffsetStruct(psOffsetStruct);
	}

	list_del(&psLinuxMemArea->sMMapItem);

	psLinuxMemArea->bMMapRegistered = 0;

#if defined(DEBUG_LINUX_MMAP_AREAS)
	g_ui32RegisteredAreas--;
	if (psLinuxMemArea->eAreaType != LINUX_MEM_AREA_SUB_ALLOC) {
		g_ui32TotalByteSize -= psLinuxMemArea->ui32ByteSize;
	}
#endif

	eError = PVRSRV_OK;

exit_unlock:
	mutex_unlock(&g_sMMapMutex);
	return eError;
}

PVRSRV_ERROR
LinuxMMapPerProcessConnect(PVRSRV_ENV_PER_PROCESS_DATA * psEnvPerProc)
{
	return PVRSRV_OK;
}

void LinuxMMapPerProcessDisconnect(PVRSRV_ENV_PER_PROCESS_DATA * psEnvPerProc)
{
	PKV_OFFSET_STRUCT psOffsetStruct, psTmpOffsetStruct;
	int bWarn = 0;
	u32 ui32PID = OSGetCurrentProcessIDKM();

	mutex_lock(&g_sMMapMutex);

	list_for_each_entry_safe(psOffsetStruct, psTmpOffsetStruct,
				 &g_sMMapOffsetStructList, sMMapItem) {
		if (psOffsetStruct->ui32PID == ui32PID) {
			if (!bWarn) {
				PVR_DPF((PVR_DBG_WARNING,
					 "%s: process has unmapped offset structures. Removing them",
					 __FUNCTION__));
				bWarn = 1;
			}
			PVR_ASSERT(psOffsetStruct->ui32Mapped == 0);
			PVR_ASSERT(psOffsetStruct->bOnMMapList);

			DestroyOffsetStruct(psOffsetStruct);
		}
	}

	mutex_unlock(&g_sMMapMutex);
}

PVRSRV_ERROR LinuxMMapPerProcessHandleOptions(PVRSRV_HANDLE_BASE * psHandleBase)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVSetMaxHandle(psHandleBase, MAX_MMAP_HANDLE);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: failed to set handle limit (%d)",
			 __FUNCTION__, eError));
		return eError;
	}

	return eError;
}

void PVRMMapInit(void)
{
	mutex_init(&g_sMMapMutex);

	g_psMemmapCache =
	    KMemCacheCreateWrapper("img-mmap", sizeof(KV_OFFSET_STRUCT), 0, 0);
	if (!g_psMemmapCache) {
		PVR_DPF((PVR_DBG_ERROR, "%s: failed to allocate kmem_cache",
			 __FUNCTION__));
		goto error;
	}
#if defined(DEBUG_LINUX_MMAP_AREAS)
#ifdef PVR_PROC_USE_SEQ_FILE
	g_ProcMMap = CreateProcReadEntrySeq("mmap", NULL,
					    ProcSeqNextMMapRegistrations,
					    ProcSeqShowMMapRegistrations,
					    ProcSeqOff2ElementMMapRegistrations,
					    ProcSeqStartstopMMapRegistations);
#else
	CreateProcReadEntry("mmap", PrintMMapRegistrations);
#endif
#endif
	return;

error:
	PVRMMapCleanup();
	return;
}

void PVRMMapCleanup(void)
{
	PVRSRV_ERROR eError;

	if (!list_empty(&g_sMMapAreaList)) {
		LinuxMemArea *psLinuxMemArea, *psTmpMemArea;

		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Memory areas are still registered with MMap",
			 __FUNCTION__));

		PVR_TRACE(("%s: Unregistering memory areas", __FUNCTION__));
		list_for_each_entry_safe(psLinuxMemArea, psTmpMemArea,
					 &g_sMMapAreaList, sMMapItem) {
			eError = PVRMMapRemoveRegisteredArea(psLinuxMemArea);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s: PVRMMapRemoveRegisteredArea failed (%d)",
					 __FUNCTION__, eError));
			}
			PVR_ASSERT(eError == PVRSRV_OK);

			LinuxMemAreaDeepFree(psLinuxMemArea);
		}
	}
	PVR_ASSERT(list_empty((&g_sMMapAreaList)));

#if defined(DEBUG_LINUX_MMAP_AREAS)
#ifdef PVR_PROC_USE_SEQ_FILE
	RemoveProcEntrySeq(g_ProcMMap);
#else
	RemoveProcEntry("mmap");
#endif
#endif

	if (g_psMemmapCache) {
		KMemCacheDestroyWrapper(g_psMemmapCache);
		g_psMemmapCache = NULL;
	}
}

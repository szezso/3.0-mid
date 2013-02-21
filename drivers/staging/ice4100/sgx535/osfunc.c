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

#include <linux/version.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <linux/string.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/hardirq.h>
#include <linux/timer.h>
#include <linux/capability.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#if defined(PVR_LINUX_MISR_USING_WORKQUEUE) || \
	defined(PVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE) || \
	defined(PVR_LINUX_TIMERS_USING_WORKQUEUES) || \
	defined(PVR_LINUX_USING_WORKQUEUES)
#include <linux/workqueue.h>
#endif

#include "img_types.h"
#include "services_headers.h"
#include "mm.h"
#include "pvrmmap.h"
#include "mmap.h"
#include "env_data.h"
#include "proc.h"
#include "event.h"
#include "linkage.h"

#define EVENT_OBJECT_TIMEOUT_MS		(100)

#if defined(SUPPORT_CPU_CACHED_BUFFERS) || \
	defined(SUPPORT_CACHEFLUSH_ON_ALLOC)

#if defined(__i386__)
static void per_cpu_cache_flush(void *arg)
{
	PVR_UNREFERENCED_PARAMETER(arg);
	wbinvd();
}
#endif

#if !defined(SUPPORT_CPU_CACHED_BUFFERS)
static
#endif
void OSFlushCPUCacheKM(void)
{
#if defined(__arm__)
	flush_cache_all();
#elif defined(__i386__)

	on_each_cpu(per_cpu_cache_flush, NULL, 1);
#else
#error "Implement full CPU cache flush for this CPU!"
#endif
}

#endif
#if defined(SUPPORT_CPU_CACHED_BUFFERS)

void OSFlushCPUCacheRangeKM(void *pvRangeAddrStart, void *pvRangeAddrEnd)
{
	PVR_UNREFERENCED_PARAMETER(pvRangeAddrStart);
	PVR_UNREFERENCED_PARAMETER(pvRangeAddrEnd);

	OSFlushCPUCacheKM();
}

#endif

#define HOST_ALLOC_MEM_USING_KMALLOC ((void *)0)
#define HOST_ALLOC_MEM_USING_VMALLOC ((void *)1)

#if !defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
PVRSRV_ERROR OSAllocMem_Impl(u32 ui32Flags, u32 ui32Size, void **ppvCpuVAddr,
			     void **phBlockAlloc)
#else
PVRSRV_ERROR OSAllocMem_Impl(u32 ui32Flags, u32 ui32Size, void **ppvCpuVAddr,
			     void **phBlockAlloc, char *pszFilename,
			     u32 ui32Line)
#endif
{
#if defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
	*ppvCpuVAddr = _KMallocWrapper(ui32Size, pszFilename, ui32Line);
#else
	*ppvCpuVAddr = KMallocWrapper(ui32Size);
#endif
	if (*ppvCpuVAddr) {
		if (phBlockAlloc) {

			*phBlockAlloc = HOST_ALLOC_MEM_USING_KMALLOC;
		}
	} else {
		if (!phBlockAlloc) {
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

#if defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
		*ppvCpuVAddr =
		    _VMallocWrapper(ui32Size, PVRSRV_HAP_CACHED, pszFilename,
				    ui32Line);
#else
		*ppvCpuVAddr = VMallocWrapper(ui32Size, PVRSRV_HAP_CACHED);
#endif
		if (!*ppvCpuVAddr) {
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		*phBlockAlloc = HOST_ALLOC_MEM_USING_VMALLOC;
	}

	return PVRSRV_OK;
}

#if !defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
PVRSRV_ERROR OSFreeMem_Impl(u32 ui32Flags, u32 ui32Size, void *pvCpuVAddr,
			    void *hBlockAlloc)
#else
PVRSRV_ERROR OSFreeMem_Impl(u32 ui32Flags, u32 ui32Size, void *pvCpuVAddr,
			    void *hBlockAlloc, char *pszFilename, u32 ui32Line)
#endif
{
	if (hBlockAlloc == HOST_ALLOC_MEM_USING_VMALLOC) {
#if defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
		_VFreeWrapper(pvCpuVAddr, pszFilename, ui32Line);
#else
		VFreeWrapper(pvCpuVAddr);
#endif
	} else {
#if defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
		_KFreeWrapper(pvCpuVAddr, pszFilename, ui32Line);
#else
		KFreeWrapper(pvCpuVAddr);
#endif
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR
OSAllocPages_Impl(u32 ui32AllocFlags,
		  u32 ui32Size,
		  u32 ui32PageSize, void **ppvCpuVAddr, void **phOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea;

#if 0

	if (ui32AllocFlags & PVRSRV_HAP_SINGLE_PROCESS) {
		ui32AllocFlags &= ~PVRSRV_HAP_SINGLE_PROCESS;
		ui32AllocFlags |= PVRSRV_HAP_MULTI_PROCESS;
	}
#endif

	switch (ui32AllocFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		{
			psLinuxMemArea =
			    NewVMallocLinuxMemArea(ui32Size, ui32AllocFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}
			break;
		}
	case PVRSRV_HAP_SINGLE_PROCESS:
		{

			psLinuxMemArea =
			    NewAllocPagesLinuxMemArea(ui32Size, ui32AllocFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}

	case PVRSRV_HAP_MULTI_PROCESS:
		{

#if defined(VIVT_CACHE) || defined(__sh__)

			ui32AllocFlags &= ~PVRSRV_HAP_CACHED;
#endif
			psLinuxMemArea =
			    NewVMallocLinuxMemArea(ui32Size, ui32AllocFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}
	default:
		PVR_DPF((PVR_DBG_ERROR, "OSAllocPages: invalid flags 0x%x\n",
			 ui32AllocFlags));
		*ppvCpuVAddr = NULL;
		*phOSMemHandle = (void *)0;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

#if defined(SUPPORT_CACHEFLUSH_ON_ALLOC)

	if (ui32AllocFlags & (PVRSRV_HAP_WRITECOMBINE | PVRSRV_HAP_UNCACHED)) {
		OSFlushCPUCacheKM();
	}
#endif

	*ppvCpuVAddr = LinuxMemAreaToCpuVAddr(psLinuxMemArea);
	*phOSMemHandle = psLinuxMemArea;

	LinuxMemAreaRegister(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR
OSFreePages(u32 ui32AllocFlags, u32 ui32Bytes, void *pvCpuVAddr,
	    void *hOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea;

	psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;

	switch (ui32AllocFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		break;
	case PVRSRV_HAP_SINGLE_PROCESS:
	case PVRSRV_HAP_MULTI_PROCESS:
		if (PVRMMapRemoveRegisteredArea(psLinuxMemArea) != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "OSFreePages(ui32AllocFlags=0x%08X, ui32Bytes=%ld, "
				 "pvCpuVAddr=%p, hOSMemHandle=%p) FAILED!",
				 ui32AllocFlags, ui32Bytes, pvCpuVAddr,
				 hOSMemHandle));
			return PVRSRV_ERROR_GENERIC;
		}
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR, "%s: invalid flags 0x%x\n",
			 __FUNCTION__, ui32AllocFlags));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	LinuxMemAreaDeepFree(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR
OSGetSubMemHandle(void *hOSMemHandle,
		  u32 ui32ByteOffset,
		  u32 ui32Bytes, u32 ui32Flags, void **phOSMemHandleRet)
{
	LinuxMemArea *psParentLinuxMemArea, *psLinuxMemArea;
	PVRSRV_ERROR eError;

	psParentLinuxMemArea = (LinuxMemArea *) hOSMemHandle;

	psLinuxMemArea =
	    NewSubLinuxMemArea(psParentLinuxMemArea, ui32ByteOffset, ui32Bytes);
	if (!psLinuxMemArea) {
		*phOSMemHandleRet = NULL;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	*phOSMemHandleRet = psLinuxMemArea;

	if (ui32Flags & PVRSRV_HAP_KERNEL_ONLY) {
		return PVRSRV_OK;
	}

	eError = PVRMMapRegisterArea(psLinuxMemArea);
	if (eError != PVRSRV_OK) {
		goto failed_register_area;
	}

	return PVRSRV_OK;

failed_register_area:
	*phOSMemHandleRet = NULL;
	LinuxMemAreaDeepFree(psLinuxMemArea);
	return eError;
}

PVRSRV_ERROR OSReleaseSubMemHandle(void *hOSMemHandle, u32 ui32Flags)
{
	LinuxMemArea *psLinuxMemArea;
	PVRSRV_ERROR eError;

	psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;
	PVR_ASSERT(psLinuxMemArea->eAreaType == LINUX_MEM_AREA_SUB_ALLOC);

	if ((ui32Flags & PVRSRV_HAP_KERNEL_ONLY) == 0) {
		eError = PVRMMapRemoveRegisteredArea(psLinuxMemArea);
		if (eError != PVRSRV_OK) {
			return eError;
		}
	}
	LinuxMemAreaDeepFree(psLinuxMemArea);

	return PVRSRV_OK;
}

IMG_CPU_PHYADDR OSMemHandleToCpuPAddr(void *hOSMemHandle, u32 ui32ByteOffset)
{
	PVR_ASSERT(hOSMemHandle);

	return LinuxMemAreaToCpuPAddr(hOSMemHandle, ui32ByteOffset);
}

void OSBreakResourceLock(PVRSRV_RESOURCE * psResource, u32 ui32ID)
{
	volatile u32 *pui32Access = (volatile u32 *)&psResource->ui32Lock;

	if (*pui32Access) {
		if (psResource->ui32ID == ui32ID) {
			psResource->ui32ID = 0;
			*pui32Access = 0;
		} else {
			PVR_DPF((PVR_DBG_MESSAGE,
				 "OSBreakResourceLock: Resource is not locked for this process."));
		}
	} else {
		PVR_DPF((PVR_DBG_MESSAGE,
			 "OSBreakResourceLock: Resource is not locked"));
	}
}

PVRSRV_ERROR OSCreateResource(PVRSRV_RESOURCE * psResource)
{
	psResource->ui32ID = 0;
	psResource->ui32Lock = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSDestroyResource(PVRSRV_RESOURCE * psResource)
{
	OSBreakResourceLock(psResource, psResource->ui32ID);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSInitEnvData(void **ppvEnvSpecificData)
{
	ENV_DATA *psEnvData;

	if (OSAllocMem
	    (PVRSRV_OS_PAGEABLE_HEAP, sizeof(ENV_DATA), (void **)&psEnvData,
	     NULL, "Environment Data") != PVRSRV_OK) {
		return PVRSRV_ERROR_GENERIC;
	}

	if (OSAllocMem
	    (PVRSRV_OS_PAGEABLE_HEAP,
	     PVRSRV_MAX_BRIDGE_IN_SIZE + PVRSRV_MAX_BRIDGE_OUT_SIZE,
	     &psEnvData->pvBridgeData, NULL, "Bridge Data") != PVRSRV_OK) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(ENV_DATA), psEnvData,
			  NULL);

		return PVRSRV_ERROR_GENERIC;
	}

	psEnvData->bMISRInstalled = 0;
	psEnvData->bLISRInstalled = 0;

	*ppvEnvSpecificData = psEnvData;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSDeInitEnvData(void *pvEnvSpecificData)
{
	ENV_DATA *psEnvData = (ENV_DATA *) pvEnvSpecificData;

	PVR_ASSERT(!psEnvData->bMISRInstalled);
	PVR_ASSERT(!psEnvData->bLISRInstalled);

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
		  PVRSRV_MAX_BRIDGE_IN_SIZE + PVRSRV_MAX_BRIDGE_OUT_SIZE,
		  psEnvData->pvBridgeData, NULL);
	psEnvData->pvBridgeData = NULL;

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(ENV_DATA), pvEnvSpecificData,
		  NULL);

	return PVRSRV_OK;
}

#ifdef INTEL_D3_CHANGES
u32 OSPCIReadDword(u32 ui32Bus, u32 ui32Dev, u32 ui32Func, u32 ui32Reg)
{
	struct pci_dev *dev;
	u32 ui32Value;

	dev = pci_get_bus_and_slot(ui32Bus, PCI_DEVFN(ui32Dev, ui32Func));

	if (dev) {
		pci_read_config_dword(dev, (int)ui32Reg, (u32 *) & ui32Value);
		return (ui32Value);
	} else {
		return (0);
	}
}

void OSPCIWriteDword(u32 ui32Bus, u32 ui32Dev, u32 ui32Func, u32 ui32Reg,
		     u32 ui32Value)
{
	struct pci_dev *dev;

	dev = pci_get_bus_and_slot(ui32Bus, PCI_DEVFN(ui32Dev, ui32Func));

	if (dev) {
		pci_write_config_dword(dev, (int)ui32Reg, (u32) ui32Value);
	}
}
#endif

void OSReleaseThreadQuanta(void)
{
	schedule();
}

u32 OSClockus(void)
{
	u32 time, j = jiffies;

	time = j * (1000000 / HZ);

	return time;
}

void OSWaitus(u32 ui32Timeus)
{
	udelay(ui32Timeus);
}

u32 OSGetCurrentProcessIDKM(void)
{
	if (in_interrupt()) {
		return KERNEL_ID;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
	return (u32) current->pgrp;
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	return (u32) task_tgid_nr(current);
#else
	return (u32) current->tgid;
#endif
#endif
}

u32 OSGetPageSize(void)
{
#if defined(__sh__)
	u32 ui32ReturnValue = PAGE_SIZE;

	return (ui32ReturnValue);
#else
	return PAGE_SIZE;
#endif
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
static irqreturn_t DeviceISRWrapper(int irq, void *dev_id
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
				    , struct pt_regs *regs
#endif
    )
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	int bStatus = 0;

	psDeviceNode = (PVRSRV_DEVICE_NODE *) dev_id;
	if (!psDeviceNode) {
		PVR_DPF((PVR_DBG_ERROR, "DeviceISRWrapper: invalid params\n"));
		goto out;
	}

	bStatus = PVRSRVDeviceLISR(psDeviceNode);

	if (bStatus) {
		OSScheduleMISR((void *)psDeviceNode->psSysData);
	}

out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	return bStatus ? IRQ_HANDLED : IRQ_NONE;
#endif
}

static irqreturn_t SystemISRWrapper(int irq, void *dev_id
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
				    , struct pt_regs *regs
#endif
    )
{
	SYS_DATA *psSysData;
	int bStatus = 0;

	psSysData = (SYS_DATA *) dev_id;
	if (!psSysData) {
		PVR_DPF((PVR_DBG_ERROR, "SystemISRWrapper: invalid params\n"));
		goto out;
	}

	bStatus = PVRSRVSystemLISR(psSysData);

	if (bStatus) {
		OSScheduleMISR((void *)psSysData);
	}

out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	return bStatus ? IRQ_HANDLED : IRQ_NONE;
#endif
}

PVRSRV_ERROR OSInstallDeviceLISR(void *pvSysData,
				 u32 ui32Irq,
				 char *pszISRName, void *pvDeviceNode)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bLISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallDeviceLISR: An ISR has already been installed: IRQ %d cookie %x",
			 psEnvData->ui32IRQ, psEnvData->pvISRCookie));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Installing device LISR %s on IRQ %d with cookie %x",
		   pszISRName, ui32Irq, pvDeviceNode));

	if (request_irq(ui32Irq, DeviceISRWrapper,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
			SA_SHIRQ
#else
			IRQF_SHARED
#endif
			, pszISRName, pvDeviceNode)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallDeviceLISR: Couldn't install device LISR on IRQ %d",
			 ui32Irq));

		return PVRSRV_ERROR_GENERIC;
	}

	psEnvData->ui32IRQ = ui32Irq;
	psEnvData->pvISRCookie = pvDeviceNode;
	psEnvData->bLISRInstalled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallDeviceLISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (!psEnvData->bLISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUninstallDeviceLISR: No LISR has been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Uninstalling device LISR on IRQ %d with cookie %x",
		   psEnvData->ui32IRQ, psEnvData->pvISRCookie));

	free_irq(psEnvData->ui32IRQ, psEnvData->pvISRCookie);

	psEnvData->bLISRInstalled = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSInstallSystemLISR(void *pvSysData, u32 ui32Irq)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bLISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallSystemLISR: An LISR has already been installed: IRQ %d cookie %x",
			 psEnvData->ui32IRQ, psEnvData->pvISRCookie));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Installing system LISR on IRQ %d with cookie %x", ui32Irq,
		   pvSysData));

	if (request_irq(ui32Irq, SystemISRWrapper,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
			SA_SHIRQ
#else
			IRQF_SHARED
#endif
			, "PowerVR", pvSysData)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallSystemLISR: Couldn't install system LISR on IRQ %d",
			 ui32Irq));

		return PVRSRV_ERROR_GENERIC;
	}

	psEnvData->ui32IRQ = ui32Irq;
	psEnvData->pvISRCookie = pvSysData;
	psEnvData->bLISRInstalled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallSystemLISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (!psEnvData->bLISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUninstallSystemLISR: No LISR has been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Uninstalling system LISR on IRQ %d with cookie %x",
		   psEnvData->ui32IRQ, psEnvData->pvISRCookie));

	free_irq(psEnvData->ui32IRQ, psEnvData->pvISRCookie);

	psEnvData->bLISRInstalled = 0;

	return PVRSRV_OK;
}

#if defined(PVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE)
static void MISRWrapper(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
			       void *data
#else
			       struct work_struct *data
#endif
    )
{
	ENV_DATA *psEnvData = container_of(data, ENV_DATA, sMISRWork);
	SYS_DATA *psSysData = (SYS_DATA *) psEnvData->pvMISRData;

	PVRSRVMISR(psSysData);
}

PVRSRV_ERROR OSInstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallMISR: An MISR has already been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Installing MISR with cookie %p", pvSysData));

	psEnvData->psWorkQueue = create_singlethread_workqueue("pvr_workqueue");

	if (psEnvData->psWorkQueue == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallMISR: create_singlethreaded_workqueue failed"));
		return PVRSRV_ERROR_GENERIC;
	}

	INIT_WORK(&psEnvData->sMISRWork, MISRWrapper
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
		  , (void *)&psEnvData->sMISRWork
#endif
	    );

	psEnvData->pvMISRData = pvSysData;
	psEnvData->bMISRInstalled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (!psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUninstallMISR: No MISR has been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Uninstalling MISR"));

	destroy_workqueue(psEnvData->psWorkQueue);

	psEnvData->bMISRInstalled = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSScheduleMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		queue_work(psEnvData->psWorkQueue, &psEnvData->sMISRWork);
	}

	return PVRSRV_OK;
}
#else
#if defined(PVR_LINUX_MISR_USING_WORKQUEUE)
static void MISRWrapper(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
			       void *data
#else
			       struct work_struct *data
#endif
    )
{
	ENV_DATA *psEnvData = container_of(data, ENV_DATA, sMISRWork);
	SYS_DATA *psSysData = (SYS_DATA *) psEnvData->pvMISRData;

	PVRSRVMISR(psSysData);
}

PVRSRV_ERROR OSInstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallMISR: An MISR has already been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Installing MISR with cookie %x", pvSysData));

	INIT_WORK(&psEnvData->sMISRWork, MISRWrapper
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
		  , (void *)&psEnvData->sMISRWork
#endif
	    );

	psEnvData->pvMISRData = pvSysData;
	psEnvData->bMISRInstalled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (!psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUninstallMISR: No MISR has been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Uninstalling MISR"));

	flush_scheduled_work();

	psEnvData->bMISRInstalled = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSScheduleMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		schedule_work(&psEnvData->sMISRWork);
	}

	return PVRSRV_OK;
}

#else

static void MISRWrapper(unsigned long data)
{
	SYS_DATA *psSysData;

	psSysData = (SYS_DATA *) data;

	PVRSRVMISR(psSysData);
}

PVRSRV_ERROR OSInstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallMISR: An MISR has already been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Installing MISR with cookie %x", pvSysData));

	tasklet_init(&psEnvData->sMISRTasklet, MISRWrapper,
		     (unsigned long)pvSysData);

	psEnvData->bMISRInstalled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (!psEnvData->bMISRInstalled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUninstallMISR: No MISR has been installed"));
		return PVRSRV_ERROR_GENERIC;
	}

	PVR_TRACE(("Uninstalling MISR"));

	tasklet_kill(&psEnvData->sMISRTasklet);

	psEnvData->bMISRInstalled = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSScheduleMISR(void *pvSysData)
{
	SYS_DATA *psSysData = (SYS_DATA *) pvSysData;
	ENV_DATA *psEnvData = (ENV_DATA *) psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled) {
		tasklet_schedule(&psEnvData->sMISRTasklet);
	}

	return PVRSRV_OK;
}

#endif
#endif

#endif

void OSPanic(void)
{
	BUG();
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
#define	OS_TAS(p)	xchg((p), 1)
#else
#define	OS_TAS(p)	tas(p)
#endif
PVRSRV_ERROR OSLockResource(PVRSRV_RESOURCE * psResource, u32 ui32ID)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!OS_TAS(&psResource->ui32Lock))
		psResource->ui32ID = ui32ID;
	else
		eError = PVRSRV_ERROR_GENERIC;

	return eError;
}

PVRSRV_ERROR OSUnlockResource(PVRSRV_RESOURCE * psResource, u32 ui32ID)
{
	volatile u32 *pui32Access = (volatile u32 *)&psResource->ui32Lock;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (*pui32Access) {
		if (psResource->ui32ID == ui32ID) {
			psResource->ui32ID = 0;
			*pui32Access = 0;
		} else {
			PVR_DPF((PVR_DBG_ERROR,
				 "OSUnlockResource: Resource %p is not locked with expected value.",
				 psResource));
			PVR_DPF((PVR_DBG_MESSAGE, "Should be %x is actually %x",
				 ui32ID, psResource->ui32ID));
			eError = PVRSRV_ERROR_GENERIC;
		}
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUnlockResource: Resource %p is not locked",
			 psResource));
		eError = PVRSRV_ERROR_GENERIC;
	}

	return eError;
}

int OSIsResourceLocked(PVRSRV_RESOURCE * psResource, u32 ui32ID)
{
	volatile u32 *pui32Access = (volatile u32 *)&psResource->ui32Lock;

	return (*(volatile u32 *)pui32Access == 1)
	    && (psResource->ui32ID == ui32ID)
	    ? 1 : 0;
}

IMG_CPU_PHYADDR OSMapLinToCPUPhys(void *pvLinAddr)
{
	IMG_CPU_PHYADDR CpuPAddr;

	CpuPAddr.uiAddr = (u32) VMallocToPhys(pvLinAddr);

	return CpuPAddr;
}

void *OSMapPhysToLin(IMG_CPU_PHYADDR BasePAddr,
		     u32 ui32Bytes, u32 ui32MappingFlags, void **phOSMemHandle)
{
	if (phOSMemHandle) {
		*phOSMemHandle = (void *)0;
	}

	if (ui32MappingFlags & PVRSRV_HAP_KERNEL_ONLY) {
		void *pvIORemapCookie;
		pvIORemapCookie =
		    IORemapWrapper(BasePAddr, ui32Bytes, ui32MappingFlags);
		if (pvIORemapCookie == NULL) {
			return NULL;
		}
		return pvIORemapCookie;
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSMapPhysToLin should only be used with PVRSRV_HAP_KERNEL_ONLY "
			 " (Use OSReservePhys otherwise)"));
		return NULL;
	}
}

int
OSUnMapPhysToLin(void *pvLinAddr, u32 ui32Bytes, u32 ui32MappingFlags,
		 void *hPageAlloc)
{
	PVR_TRACE(("%s: unmapping %d bytes from 0x%08x", __FUNCTION__,
		   ui32Bytes, pvLinAddr));

	if (ui32MappingFlags & PVRSRV_HAP_KERNEL_ONLY) {
		IOUnmapWrapper(pvLinAddr);
		return 1;
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUnMapPhysToLin should only be used with PVRSRV_HAP_KERNEL_ONLY "
			 " (Use OSUnReservePhys otherwise)"));
		return 0;
	}
}

static PVRSRV_ERROR
RegisterExternalMem(IMG_SYS_PHYADDR * pBasePAddr,
		    void *pvCPUVAddr,
		    u32 ui32Bytes,
		    int bPhysContig, u32 ui32MappingFlags, void **phOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea;

	switch (ui32MappingFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		{
			psLinuxMemArea =
			    NewExternalKVLinuxMemArea(pBasePAddr, pvCPUVAddr,
						      ui32Bytes, bPhysContig,
						      ui32MappingFlags);

			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			break;
		}
	case PVRSRV_HAP_SINGLE_PROCESS:
		{
			psLinuxMemArea =
			    NewExternalKVLinuxMemArea(pBasePAddr, pvCPUVAddr,
						      ui32Bytes, bPhysContig,
						      ui32MappingFlags);

			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}
	case PVRSRV_HAP_MULTI_PROCESS:
		{

#if defined(VIVT_CACHE) || defined(__sh__)

			ui32MappingFlags &= ~PVRSRV_HAP_CACHED;
#endif
			psLinuxMemArea =
			    NewExternalKVLinuxMemArea(pBasePAddr, pvCPUVAddr,
						      ui32Bytes, bPhysContig,
						      ui32MappingFlags);

			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}
	default:
		PVR_DPF((PVR_DBG_ERROR, "OSRegisterMem : invalid flags 0x%x\n",
			 ui32MappingFlags));
		*phOSMemHandle = (void *)0;
		return PVRSRV_ERROR_GENERIC;
	}

	*phOSMemHandle = (void *)psLinuxMemArea;

	LinuxMemAreaRegister(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR
OSRegisterMem(IMG_CPU_PHYADDR BasePAddr,
	      void *pvCPUVAddr,
	      u32 ui32Bytes, u32 ui32MappingFlags, void **phOSMemHandle)
{
	IMG_SYS_PHYADDR SysPAddr = SysCpuPAddrToSysPAddr(BasePAddr);

	return RegisterExternalMem(&SysPAddr, pvCPUVAddr, ui32Bytes, 1,
				   ui32MappingFlags, phOSMemHandle);
}

PVRSRV_ERROR OSRegisterDiscontigMem(IMG_SYS_PHYADDR * pBasePAddr,
				    void *pvCPUVAddr, u32 ui32Bytes,
				    u32 ui32MappingFlags, void **phOSMemHandle)
{
	return RegisterExternalMem(pBasePAddr, pvCPUVAddr, ui32Bytes, 0,
				   ui32MappingFlags, phOSMemHandle);
}

PVRSRV_ERROR
OSUnRegisterMem(void *pvCpuVAddr,
		u32 ui32Bytes, u32 ui32MappingFlags, void *hOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;
#ifdef INTEL_D3_P_CHANGES
	if (!hOSMemHandle) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSUnRegisterMem : memory handle is null\n"));
		return PVRSRV_ERROR_GENERIC;
	}
#endif

	switch (ui32MappingFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		break;
	case PVRSRV_HAP_SINGLE_PROCESS:
	case PVRSRV_HAP_MULTI_PROCESS:
		{
			if (PVRMMapRemoveRegisteredArea(psLinuxMemArea) !=
			    PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s(%p, %d, 0x%08X, %p) FAILED!",
					 __FUNCTION__, pvCpuVAddr, ui32Bytes,
					 ui32MappingFlags, hOSMemHandle));
				return PVRSRV_ERROR_GENERIC;
			}
			break;
		}
	default:
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "OSUnRegisterMem : invalid flags 0x%x",
				 ui32MappingFlags));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	LinuxMemAreaDeepFree(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSUnRegisterDiscontigMem(void *pvCpuVAddr, u32 ui32Bytes,
				      u32 ui32Flags, void *hOSMemHandle)
{
	return OSUnRegisterMem(pvCpuVAddr, ui32Bytes, ui32Flags, hOSMemHandle);
}

PVRSRV_ERROR
OSReservePhys(IMG_CPU_PHYADDR BasePAddr,
	      u32 ui32Bytes,
	      u32 ui32MappingFlags, void **ppvCpuVAddr, void **phOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea;

#if 0

	if (ui32MappingFlags & PVRSRV_HAP_SINGLE_PROCESS) {
		ui32MappingFlags &= ~PVRSRV_HAP_SINGLE_PROCESS;
		ui32MappingFlags |= PVRSRV_HAP_MULTI_PROCESS;
	}
#endif

	switch (ui32MappingFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		{

			psLinuxMemArea =
			    NewIORemapLinuxMemArea(BasePAddr, ui32Bytes,
						   ui32MappingFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			break;
		}
	case PVRSRV_HAP_SINGLE_PROCESS:
		{

			psLinuxMemArea =
			    NewIOLinuxMemArea(BasePAddr, ui32Bytes,
					      ui32MappingFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}
	case PVRSRV_HAP_MULTI_PROCESS:
		{

#if defined(VIVT_CACHE) || defined(__sh__)

			ui32MappingFlags &= ~PVRSRV_HAP_CACHED;
#endif
			psLinuxMemArea =
			    NewIORemapLinuxMemArea(BasePAddr, ui32Bytes,
						   ui32MappingFlags);
			if (!psLinuxMemArea) {
				return PVRSRV_ERROR_GENERIC;
			}
			PVRMMapRegisterArea(psLinuxMemArea);
			break;
		}
	default:
		PVR_DPF((PVR_DBG_ERROR, "OSMapPhysToLin : invalid flags 0x%x\n",
			 ui32MappingFlags));
		*ppvCpuVAddr = NULL;
		*phOSMemHandle = (void *)0;
		return PVRSRV_ERROR_GENERIC;
	}

	*phOSMemHandle = (void *)psLinuxMemArea;
	*ppvCpuVAddr = LinuxMemAreaToCpuVAddr(psLinuxMemArea);

	LinuxMemAreaRegister(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR
OSUnReservePhys(void *pvCpuVAddr,
		u32 ui32Bytes, u32 ui32MappingFlags, void *hOSMemHandle)
{
	LinuxMemArea *psLinuxMemArea;

	psLinuxMemArea = (LinuxMemArea *) hOSMemHandle;

	switch (ui32MappingFlags & PVRSRV_HAP_MAPTYPE_MASK) {
	case PVRSRV_HAP_KERNEL_ONLY:
		break;
	case PVRSRV_HAP_SINGLE_PROCESS:
	case PVRSRV_HAP_MULTI_PROCESS:
		{
			if (PVRMMapRemoveRegisteredArea(psLinuxMemArea) !=
			    PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "%s(%p, %d, 0x%08X, %p) FAILED!",
					 __FUNCTION__, pvCpuVAddr, ui32Bytes,
					 ui32MappingFlags, hOSMemHandle));
				return PVRSRV_ERROR_GENERIC;
			}
			break;
		}
	default:
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "OSUnMapPhysToLin : invalid flags 0x%x",
				 ui32MappingFlags));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	LinuxMemAreaDeepFree(psLinuxMemArea);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSBaseAllocContigMemory(u32 ui32Size, IMG_CPU_VIRTADDR * pvLinAddr,
				     IMG_CPU_PHYADDR * psPhysAddr)
{
#if !defined(NO_HARDWARE)
	PVR_DPF((PVR_DBG_ERROR, "%s: Not available", __FUNCTION__));

	return PVRSRV_ERROR_OUT_OF_MEMORY;
#else
	void *pvKernLinAddr;

#if defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
	pvKernLinAddr = _KMallocWrapper(ui32Size, __FILE__, __LINE__);
#else
	pvKernLinAddr = KMallocWrapper(ui32Size);
#endif
	if (!pvKernLinAddr) {
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	*pvLinAddr = pvKernLinAddr;

	psPhysAddr->uiAddr = virt_to_phys(pvKernLinAddr);

	return PVRSRV_OK;
#endif
}

PVRSRV_ERROR OSBaseFreeContigMemory(u32 ui32Size, IMG_CPU_VIRTADDR pvLinAddr,
				    IMG_CPU_PHYADDR psPhysAddr)
{
#if !defined(NO_HARDWARE)
	PVR_DPF((PVR_DBG_WARNING, "%s: Not available", __FUNCTION__));
#else
	KFreeWrapper(pvLinAddr);
#endif
	return PVRSRV_OK;
}

u32 OSReadHWReg(void *pvLinRegBaseAddr, u32 ui32Offset)
{
#if !defined(NO_HARDWARE)
	return (u32) readl((unsigned char *) pvLinRegBaseAddr + ui32Offset);
#else
	return *(u32 *) ((unsigned char *) pvLinRegBaseAddr + ui32Offset);
#endif
}

void OSWriteHWReg(void *pvLinRegBaseAddr, u32 ui32Offset, u32 ui32Value)
{
#if !defined(NO_HARDWARE)
	writel(ui32Value, (unsigned char *) pvLinRegBaseAddr + ui32Offset);
#else
	*(u32 *) ((unsigned char *) pvLinRegBaseAddr + ui32Offset) = ui32Value;
#endif
}

#if defined(CONFIG_PCI) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14))

PVRSRV_PCI_DEV_HANDLE OSPCISetDev(void *pvPCICookie, HOST_PCI_INIT_FLAGS eFlags)
{
	int err;
	u32 i;
	PVR_PCI_DEV *psPVRPCI;

	PVR_TRACE(("OSPCISetDev"));

	if (OSAllocMem
	    (PVRSRV_OS_PAGEABLE_HEAP, sizeof(*psPVRPCI), (void **)&psPVRPCI,
	     NULL, "PCI Device") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCISetDev: Couldn't allocate PVR PCI structure"));
		return NULL;
	}

	psPVRPCI->psPCIDev = (struct pci_dev *)pvPCICookie;
	psPVRPCI->ePCIFlags = eFlags;

	err = pci_enable_device(psPVRPCI->psPCIDev);
	if (err != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCISetDev: Couldn't enable device (%d)", err));
		return NULL;
	}

	if (psPVRPCI->ePCIFlags & HOST_PCI_INIT_FLAG_BUS_MASTER) {
		pci_set_master(psPVRPCI->psPCIDev);
	}

	if (psPVRPCI->ePCIFlags & HOST_PCI_INIT_FLAG_MSI) {
#if defined(CONFIG_PCI_MSI)
		err = pci_enable_msi(psPVRPCI->psPCIDev);
		if (err != 0) {
			PVR_DPF((PVR_DBG_WARNING,
				 "OSPCISetDev: Couldn't enable MSI (%d)", err));
			psPVRPCI->ePCIFlags &= ~HOST_PCI_INIT_FLAG_MSI;
		}
#else
		PVR_DPF((PVR_DBG_WARNING,
			 "OSPCISetDev: MSI support not enabled in the kernel"));
#endif
	}

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		psPVRPCI->abPCIResourceInUse[i] = 0;
	}

	return (PVRSRV_PCI_DEV_HANDLE) psPVRPCI;
}

PVRSRV_PCI_DEV_HANDLE OSPCIAcquireDev(u16 ui16VendorID, u16 ui16DeviceID,
				      HOST_PCI_INIT_FLAGS eFlags)
{
	struct pci_dev *psPCIDev;

	psPCIDev = pci_get_device(ui16VendorID, ui16DeviceID, NULL);
	if (psPCIDev == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIAcquireDev: Couldn't acquire device"));
		return NULL;
	}

	return OSPCISetDev((void *)psPCIDev, eFlags);
}

PVRSRV_ERROR OSPCIIRQ(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 * pui32IRQ)
{
	PVR_PCI_DEV *psPVRPCI = (PVR_PCI_DEV *) hPVRPCI;

	*pui32IRQ = psPVRPCI->psPCIDev->irq;

	return PVRSRV_OK;
}

enum HOST_PCI_ADDR_RANGE_FUNC {
	HOST_PCI_ADDR_RANGE_FUNC_LEN,
	HOST_PCI_ADDR_RANGE_FUNC_START,
	HOST_PCI_ADDR_RANGE_FUNC_END,
	HOST_PCI_ADDR_RANGE_FUNC_REQUEST,
	HOST_PCI_ADDR_RANGE_FUNC_RELEASE
};

static u32 OSPCIAddrRangeFunc(enum HOST_PCI_ADDR_RANGE_FUNC eFunc,
			      PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	PVR_PCI_DEV *psPVRPCI = (PVR_PCI_DEV *) hPVRPCI;

	if (ui32Index >= DEVICE_COUNT_RESOURCE) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIAddrRangeFunc: Index out of range"));
		return 0;

	}

	switch (eFunc) {
	case HOST_PCI_ADDR_RANGE_FUNC_LEN:
		return pci_resource_len(psPVRPCI->psPCIDev, ui32Index);
	case HOST_PCI_ADDR_RANGE_FUNC_START:
		return pci_resource_start(psPVRPCI->psPCIDev, ui32Index);
	case HOST_PCI_ADDR_RANGE_FUNC_END:
		return pci_resource_end(psPVRPCI->psPCIDev, ui32Index);
	case HOST_PCI_ADDR_RANGE_FUNC_REQUEST:
		{
			int err;

			err =
			    pci_request_region(psPVRPCI->psPCIDev,
					       (int)ui32Index, "PowerVR");
			if (err != 0) {
				PVR_DPF((PVR_DBG_ERROR,
					 "OSPCIAddrRangeFunc: pci_request_region_failed (%d)",
					 err));
				return 0;
			}
			psPVRPCI->abPCIResourceInUse[ui32Index] = 1;
			return 1;
		}
	case HOST_PCI_ADDR_RANGE_FUNC_RELEASE:
		if (psPVRPCI->abPCIResourceInUse[ui32Index]) {
			pci_release_region(psPVRPCI->psPCIDev, (int)ui32Index);
			psPVRPCI->abPCIResourceInUse[ui32Index] = 0;
		}
		return 1;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIAddrRangeFunc: Unknown function"));
		break;
	}

	return 0;
}

u32 OSPCIAddrRangeLen(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	return OSPCIAddrRangeFunc(HOST_PCI_ADDR_RANGE_FUNC_LEN, hPVRPCI,
				  ui32Index);
}

u32 OSPCIAddrRangeStart(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	return OSPCIAddrRangeFunc(HOST_PCI_ADDR_RANGE_FUNC_START, hPVRPCI,
				  ui32Index);
}

u32 OSPCIAddrRangeEnd(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	return OSPCIAddrRangeFunc(HOST_PCI_ADDR_RANGE_FUNC_END, hPVRPCI,
				  ui32Index);
}

PVRSRV_ERROR OSPCIRequestAddrRange(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	return OSPCIAddrRangeFunc(HOST_PCI_ADDR_RANGE_FUNC_REQUEST, hPVRPCI,
				  ui32Index) ==
	    0 ? PVRSRV_ERROR_GENERIC : PVRSRV_OK;
}

PVRSRV_ERROR OSPCIReleaseAddrRange(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index)
{
	return OSPCIAddrRangeFunc(HOST_PCI_ADDR_RANGE_FUNC_RELEASE, hPVRPCI,
				  ui32Index) ==
	    0 ? PVRSRV_ERROR_GENERIC : PVRSRV_OK;
}

PVRSRV_ERROR OSPCIReleaseDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI)
{
	PVR_PCI_DEV *psPVRPCI = (PVR_PCI_DEV *) hPVRPCI;
	int i;

	PVR_TRACE(("OSPCIReleaseDev"));

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if (psPVRPCI->abPCIResourceInUse[i]) {
			PVR_TRACE(("OSPCIReleaseDev: Releasing Address range %d", i));
			pci_release_region(psPVRPCI->psPCIDev, i);
			psPVRPCI->abPCIResourceInUse[i] = 0;
		}
	}

#if defined(CONFIG_PCI_MSI)
	if (psPVRPCI->ePCIFlags & HOST_PCI_INIT_FLAG_MSI) {
		pci_disable_msi(psPVRPCI->psPCIDev);
	}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
	if (psPVRPCI->ePCIFlags & HOST_PCI_INIT_FLAG_BUS_MASTER) {
		pci_clear_master(psPVRPCI->psPCIDev);
	}
#endif
	pci_disable_device(psPVRPCI->psPCIDev);

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(*psPVRPCI), (void *)psPVRPCI,
		  NULL);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSPCISuspendDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI)
{
	PVR_PCI_DEV *psPVRPCI = (PVR_PCI_DEV *) hPVRPCI;
	int i;
	int err;

	PVR_TRACE(("OSPCISuspendDev"));

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if (psPVRPCI->abPCIResourceInUse[i]) {
			pci_release_region(psPVRPCI->psPCIDev, i);
		}
	}

	err = pci_save_state(psPVRPCI->psPCIDev);
	if (err != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCISuspendDev: pci_save_state_failed (%d)", err));
		return PVRSRV_ERROR_GENERIC;
	}

	pci_disable_device(psPVRPCI->psPCIDev);

	err =
	    pci_set_power_state(psPVRPCI->psPCIDev,
				pci_choose_state(psPVRPCI->psPCIDev,
						 PMSG_SUSPEND));
	switch (err) {
	case 0:
		break;
	case -EIO:
		PVR_DPF((PVR_DBG_WARNING,
			 "OSPCISuspendDev: device doesn't support PCI PM"));
		break;
	case -EINVAL:
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCISuspendDev: can't enter requested power state"));
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCISuspendDev: pci_set_power_state failed (%d)",
			 err));
		break;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR OSPCIResumeDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI)
{
	PVR_PCI_DEV *psPVRPCI = (PVR_PCI_DEV *) hPVRPCI;
	int err;
	int i;

	PVR_TRACE(("OSPCIResumeDev"));

	err =
	    pci_set_power_state(psPVRPCI->psPCIDev,
				pci_choose_state(psPVRPCI->psPCIDev, PMSG_ON));
	switch (err) {
	case 0:
		break;
	case -EIO:
		PVR_DPF((PVR_DBG_WARNING,
			 "OSPCIResumeDev: device doesn't support PCI PM"));
		break;
	case -EINVAL:
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIResumeDev: can't enter requested power state"));
		return PVRSRV_ERROR_GENERIC;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIResumeDev: pci_set_power_state failed (%d)",
			 err));
		return PVRSRV_ERROR_GENERIC;
	}

	pci_restore_state(psPVRPCI->psPCIDev);

	err = pci_enable_device(psPVRPCI->psPCIDev);
	if (err != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSPCIResumeDev: Couldn't enable device (%d)", err));
		return PVRSRV_ERROR_GENERIC;
	}

	if (psPVRPCI->ePCIFlags & HOST_PCI_INIT_FLAG_BUS_MASTER)
		pci_set_master(psPVRPCI->psPCIDev);

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if (psPVRPCI->abPCIResourceInUse[i]) {
			err =
			    pci_request_region(psPVRPCI->psPCIDev, i,
					       "PowerVR");
			if (err != 0) {
				PVR_DPF((PVR_DBG_ERROR,
					 "OSPCIResumeDev: pci_request_region_failed (region %d, error %d)",
					 i, err));
			}
		}

	}

	return PVRSRV_OK;
}

#endif

#define	OS_MAX_TIMERS	8

typedef struct TIMER_CALLBACK_DATA_TAG {
	int bInUse;
	PFN_TIMER_FUNC pfnTimerFunc;
	void *pvData;
	struct timer_list sTimer;
	u32 ui32Delay;
	int bActive;
#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	struct work_struct sWork;
#endif
} TIMER_CALLBACK_DATA;

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
static struct workqueue_struct *psTimerWorkQueue;
#endif

static TIMER_CALLBACK_DATA sTimers[OS_MAX_TIMERS];

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
DEFINE_MUTEX(sTimerStructLock);
#else
static DEFINE_SPINLOCK(sTimerStructLock);

#endif

static void OSTimerCallbackBody(TIMER_CALLBACK_DATA * psTimerCBData)
{
	if (!psTimerCBData->bActive)
		return;

	psTimerCBData->pfnTimerFunc(psTimerCBData->pvData);

	mod_timer(&psTimerCBData->sTimer, psTimerCBData->ui32Delay + jiffies);
}

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
static void OSTimerWorkQueueCallBack(struct work_struct *psWork)
{
	TIMER_CALLBACK_DATA *psTimerCBData =
	    container_of(psWork, TIMER_CALLBACK_DATA, sWork);

	OSTimerCallbackBody(psTimerCBData);
}
#endif

static void OSTimerCallbackWrapper(u32 ui32Data)
{
	TIMER_CALLBACK_DATA *psTimerCBData = (TIMER_CALLBACK_DATA *) ui32Data;

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	int res;

	res = queue_work(psTimerWorkQueue, &psTimerCBData->sWork);
	if (res == 0) {
		PVR_DPF((PVR_DBG_WARNING,
			 "OSTimerCallbackWrapper: work already queued"));
	}
#else
	OSTimerCallbackBody(psTimerCBData);
#endif
}

void *OSAddTimer(PFN_TIMER_FUNC pfnTimerFunc, void *pvData, u32 ui32MsTimeout)
{
	TIMER_CALLBACK_DATA *psTimerCBData;
	u32 ui32i;
#if !defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	unsigned long ulLockFlags;
#endif

	if (!pfnTimerFunc) {
		PVR_DPF((PVR_DBG_ERROR, "OSAddTimer: passed invalid callback"));
		return NULL;
	}

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	mutex_lock(&sTimerStructLock);
#else
	spin_lock_irqsave(&sTimerStructLock, ulLockFlags);
#endif
	for (ui32i = 0; ui32i < OS_MAX_TIMERS; ui32i++) {
		psTimerCBData = &sTimers[ui32i];
		if (!psTimerCBData->bInUse) {
			psTimerCBData->bInUse = 1;
			break;
		}
	}
#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	mutex_unlock(&sTimerStructLock);
#else
	spin_unlock_irqrestore(&sTimerStructLock, ulLockFlags);
#endif
	if (ui32i >= OS_MAX_TIMERS) {
		PVR_DPF((PVR_DBG_ERROR, "OSAddTimer: all timers are in use"));
		return NULL;
	}

	psTimerCBData->pfnTimerFunc = pfnTimerFunc;
	psTimerCBData->pvData = pvData;
	psTimerCBData->bActive = 0;

	psTimerCBData->ui32Delay = ((HZ * ui32MsTimeout) < 1000)
	    ? 1 : ((HZ * ui32MsTimeout) / 1000);

	init_timer(&psTimerCBData->sTimer);

	psTimerCBData->sTimer.function = (void *)OSTimerCallbackWrapper;
	psTimerCBData->sTimer.data = (u32) psTimerCBData;
	psTimerCBData->sTimer.expires = psTimerCBData->ui32Delay + jiffies;

	return (void *)(ui32i + 1);
}

static inline TIMER_CALLBACK_DATA *GetTimerStructure(void *hTimer)
{
	u32 ui32i = ((u32) hTimer) - 1;

	PVR_ASSERT(ui32i < OS_MAX_TIMERS);

	return &sTimers[ui32i];
}

PVRSRV_ERROR OSRemoveTimer(void *hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = GetTimerStructure(hTimer);

	PVR_ASSERT(psTimerCBData->bInUse);
	PVR_ASSERT(!psTimerCBData->bActive);

	psTimerCBData->bInUse = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR OSEnableTimer(void *hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = GetTimerStructure(hTimer);

	PVR_ASSERT(psTimerCBData->bInUse);
	PVR_ASSERT(!psTimerCBData->bActive);

	psTimerCBData->bActive = 1;

	psTimerCBData->sTimer.expires = psTimerCBData->ui32Delay + jiffies;

	add_timer(&psTimerCBData->sTimer);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSDisableTimer(void *hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = GetTimerStructure(hTimer);

	PVR_ASSERT(psTimerCBData->bInUse);
	PVR_ASSERT(psTimerCBData->bActive);

	psTimerCBData->bActive = 0;
	smp_mb();

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	flush_workqueue(psTimerWorkQueue);
#endif

	del_timer_sync(&psTimerCBData->sTimer);

#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)

	flush_workqueue(psTimerWorkQueue);
#endif

	return PVRSRV_OK;
}

PVRSRV_ERROR OSEventObjectCreate(const char *pszName,
				 PVRSRV_EVENTOBJECT * psEventObject)
{

	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psEventObject) {
		if (pszName) {

			strncpy(psEventObject->szName, pszName,
				EVENTOBJNAME_MAXLENGTH);
		} else {

			static u16 ui16NameIndex = 0;
			snprintf(psEventObject->szName, EVENTOBJNAME_MAXLENGTH,
				 "PVRSRV_EVENTOBJECT_%d", ui16NameIndex++);
		}

		if (LinuxEventObjectListCreate(&psEventObject->hOSEventKM) !=
		    PVRSRV_OK) {
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		}

	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectCreate: psEventObject is not a valid pointer"));
		eError = PVRSRV_ERROR_GENERIC;
	}

	return eError;

}

PVRSRV_ERROR OSEventObjectDestroy(PVRSRV_EVENTOBJECT * psEventObject)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psEventObject) {
		if (psEventObject->hOSEventKM) {
			LinuxEventObjectListDestroy(psEventObject->hOSEventKM);
		} else {
			PVR_DPF((PVR_DBG_ERROR,
				 "OSEventObjectDestroy: hOSEventKM is not a valid pointer"));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
		}
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectDestroy: psEventObject is not a valid pointer"));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	return eError;
}

PVRSRV_ERROR OSEventObjectWait(void *hOSEventKM)
{
	PVRSRV_ERROR eError;

	if (hOSEventKM) {
		eError =
		    LinuxEventObjectWait(hOSEventKM, EVENT_OBJECT_TIMEOUT_MS);
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectWait: hOSEventKM is not a valid handle"));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	return eError;
}

PVRSRV_ERROR OSEventObjectOpen(PVRSRV_EVENTOBJECT * psEventObject,
			       void **phOSEvent)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psEventObject) {
		if (LinuxEventObjectAdd(psEventObject->hOSEventKM, phOSEvent) !=
		    PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR, "LinuxEventObjectAdd: failed"));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
		}

	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectCreate: psEventObject is not a valid pointer"));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	return eError;
}

PVRSRV_ERROR OSEventObjectClose(PVRSRV_EVENTOBJECT * psEventObject,
				void *hOSEventKM)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psEventObject) {
		if (LinuxEventObjectDelete
		    (psEventObject->hOSEventKM, hOSEventKM) != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "LinuxEventObjectDelete: failed"));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
		}

	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectDestroy: psEventObject is not a valid pointer"));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	return eError;

}

PVRSRV_ERROR OSEventObjectSignal(void *hOSEventKM)
{
	PVRSRV_ERROR eError;

	if (hOSEventKM) {
		eError = LinuxEventObjectSignal(hOSEventKM);
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSEventObjectSignal: hOSEventKM is not a valid handle"));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
	}

	return eError;
}

int OSProcHasPrivSrvInit(void)
{
	return (capable(CAP_SYS_MODULE) != 0) ? 1 : 0;
}

PVRSRV_ERROR OSCopyToUser(void *pvProcess,
			  void *pvDest, void *pvSrc, u32 ui32Bytes)
{
	if (copy_to_user(pvDest, pvSrc, ui32Bytes) == 0)
		return PVRSRV_OK;
	else
		return PVRSRV_ERROR_GENERIC;
}

PVRSRV_ERROR OSCopyFromUser(void *pvProcess,
			    void *pvDest, void *pvSrc, u32 ui32Bytes)
{
	if (copy_from_user(pvDest, pvSrc, ui32Bytes) == 0)
		return PVRSRV_OK;
	else
		return PVRSRV_ERROR_GENERIC;
}

int OSAccessOK(IMG_VERIFY_TEST eVerification, void *pvUserPtr, u32 ui32Bytes)
{
	int linuxType;

	if (eVerification == PVR_VERIFY_READ) {
		linuxType = VERIFY_READ;
	} else {
		PVR_ASSERT(eVerification == PVR_VERIFY_WRITE);
		linuxType = VERIFY_WRITE;
	}

	return access_ok(linuxType, pvUserPtr, ui32Bytes);
}

typedef enum _eWrapMemType_ {
	WRAP_TYPE_CLEANUP,
	WRAP_TYPE_GET_USER_PAGES,
	WRAP_TYPE_FIND_VMA_PAGES,
	WRAP_TYPE_FIND_VMA_PFN
} eWrapMemType;

typedef struct _sWrapMemInfo_ {
	eWrapMemType eType;
	int iNumPages;
	struct page **ppsPages;
	IMG_SYS_PHYADDR *psPhysAddr;
	int iPageOffset;
	int iContiguous;
#if defined(DEBUG)
	u32 ulStartAddr;
	u32 ulBeyondEndAddr;
	struct vm_area_struct *psVMArea;
#endif
	int bWrapWorkaround;
} sWrapMemInfo;

static void CheckPagesContiguous(sWrapMemInfo * psInfo)
{
	int i;
	u32 ui32AddrChk;

	BUG_ON(psInfo == NULL);

	psInfo->iContiguous = 1;

	for (i = 0, ui32AddrChk = psInfo->psPhysAddr[0].uiAddr;
	     i < psInfo->iNumPages; i++, ui32AddrChk += PAGE_SIZE) {
		if (psInfo->psPhysAddr[i].uiAddr != ui32AddrChk) {
			psInfo->iContiguous = 0;
			break;
		}
	}
}

static struct page *CPUVAddrToPage(struct vm_area_struct *psVMArea,
				   u32 ulCPUVAddr)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
	pgd_t *psPGD;
	pud_t *psPUD;
	pmd_t *psPMD;
	pte_t *psPTE;
	struct mm_struct *psMM = psVMArea->vm_mm;
	u32 ulPFN;
	spinlock_t *psPTLock;
	struct page *psPage;

	psPGD = pgd_offset(psMM, ulCPUVAddr);
	if (pgd_none(*psPGD) || pgd_bad(*psPGD))
		return NULL;

	psPUD = pud_offset(psPGD, ulCPUVAddr);
	if (pud_none(*psPUD) || pud_bad(*psPUD))
		return NULL;

	psPMD = pmd_offset(psPUD, ulCPUVAddr);
	if (pmd_none(*psPMD) || pmd_bad(*psPMD))
		return NULL;

	psPage = NULL;

	psPTE =
	    (pte_t *) pte_offset_map_lock(psMM, psPMD, ulCPUVAddr, &psPTLock);
	if ((pte_none(*psPTE) != 0) || (pte_present(*psPTE) == 0)
	    || (pte_write(*psPTE) == 0))
		goto exit_unlock;

	ulPFN = pte_pfn(*psPTE);
	if (!pfn_valid(ulPFN))
		goto exit_unlock;

	psPage = pfn_to_page(ulPFN);

	get_page(psPage);

exit_unlock:
	pte_unmap_unlock(psPTE, psPTLock);

	return psPage;
#else
	return NULL;
#endif
}

PVRSRV_ERROR OSReleasePhysPageAddr(void *hOSWrapMem)
{
	sWrapMemInfo *psInfo = (sWrapMemInfo *) hOSWrapMem;
	int i;

	BUG_ON(psInfo == NULL);

	switch (psInfo->eType) {
	case WRAP_TYPE_CLEANUP:
		break;
	case WRAP_TYPE_FIND_VMA_PFN:
		break;
	case WRAP_TYPE_GET_USER_PAGES:
		{
			for (i = 0; i < psInfo->iNumPages; i++) {
				struct page *psPage = psInfo->ppsPages[i];

				if (!PageReserved(psPage)) ;
				{
					SetPageDirty(psPage);
				}
				page_cache_release(psPage);
			}
			break;
		}
	case WRAP_TYPE_FIND_VMA_PAGES:
		{
			for (i = 0; i < psInfo->iNumPages; i++) {
				if (psInfo->bWrapWorkaround)
					put_page(psInfo->ppsPages[i]);
				else
					put_page_testzero(psInfo->ppsPages[i]);
			}
			break;
		}
	default:
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "OSReleasePhysPageAddr: Unknown wrap type (%d)",
				 psInfo->eType));
			return PVRSRV_ERROR_GENERIC;
		}
	}

	if (psInfo->ppsPages != NULL) {
		kfree(psInfo->ppsPages);
	}

	if (psInfo->psPhysAddr != NULL) {
		kfree(psInfo->psPhysAddr);
	}

	kfree(psInfo);

	return PVRSRV_OK;
}

PVRSRV_ERROR OSAcquirePhysPageAddr(void *pvCPUVAddr,
				   u32 ui32Bytes,
				   IMG_SYS_PHYADDR * psSysPAddr,
				   void **phOSWrapMem, int bWrapWorkaround)
{
	u32 ulStartAddrOrig = (u32) pvCPUVAddr;
	u32 ulAddrRangeOrig = (u32) ui32Bytes;
	u32 ulBeyondEndAddrOrig = ulStartAddrOrig + ulAddrRangeOrig;
	u32 ulStartAddr;
	u32 ulAddrRange;
	u32 ulBeyondEndAddr;
	u32 ulAddr;
	int iNumPagesMapped;
	int i;
	struct vm_area_struct *psVMArea;
	sWrapMemInfo *psInfo;

	ulStartAddr = ulStartAddrOrig & PAGE_MASK;
	ulBeyondEndAddr = PAGE_ALIGN(ulBeyondEndAddrOrig);
	ulAddrRange = ulBeyondEndAddr - ulStartAddr;

	psInfo = kmalloc(sizeof(*psInfo), GFP_KERNEL);
	if (psInfo == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Couldn't allocate information structure"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	memset(psInfo, 0, sizeof(*psInfo));
	psInfo->bWrapWorkaround = bWrapWorkaround;

#if defined(DEBUG)
	psInfo->ulStartAddr = ulStartAddrOrig;
	psInfo->ulBeyondEndAddr = ulBeyondEndAddrOrig;
#endif

	psInfo->iNumPages = (int)(ulAddrRange >> PAGE_SHIFT);
	psInfo->iPageOffset = (int)(ulStartAddrOrig & ~PAGE_MASK);

	psInfo->psPhysAddr =
	    kmalloc((size_t) psInfo->iNumPages * sizeof(*psInfo->psPhysAddr),
		    GFP_KERNEL);
	if (psInfo->psPhysAddr == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Couldn't allocate page array"));
		goto error_free;
	}

	psInfo->ppsPages =
	    kmalloc((size_t) psInfo->iNumPages * sizeof(*psInfo->ppsPages),
		    GFP_KERNEL);
	if (psInfo->ppsPages == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Couldn't allocate page array"));
		goto error_free;
	}

	down_read(&current->mm->mmap_sem);
	iNumPagesMapped =
	    get_user_pages(current, current->mm, ulStartAddr, psInfo->iNumPages,
			   1, 0, psInfo->ppsPages, NULL);
	up_read(&current->mm->mmap_sem);

	if (iNumPagesMapped >= 0) {

		if (iNumPagesMapped != psInfo->iNumPages) {
			PVR_TRACE(("OSAcquirePhysPageAddr: Couldn't map all the pages needed (wanted: %d, got %d)", psInfo->iNumPages, iNumPagesMapped));

			for (i = 0; i < iNumPagesMapped; i++) {
				page_cache_release(psInfo->ppsPages[i]);

			}
			goto error_free;
		}

		for (i = 0; i < psInfo->iNumPages; i++) {
			IMG_CPU_PHYADDR CPUPhysAddr;

			CPUPhysAddr.uiAddr =
			    page_to_pfn(psInfo->ppsPages[i]) << PAGE_SHIFT;
			psInfo->psPhysAddr[i] =
			    SysCpuPAddrToSysPAddr(CPUPhysAddr);
			psSysPAddr[i] = psInfo->psPhysAddr[i];

		}

		psInfo->eType = WRAP_TYPE_GET_USER_PAGES;

		goto exit_check;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "OSAcquirePhysPageAddr: get_user_pages failed (%d), trying something else",
		 iNumPagesMapped));

	down_read(&current->mm->mmap_sem);

	psVMArea = find_vma(current->mm, ulStartAddrOrig);
	if (psVMArea == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Couldn't find memory region containing start address %lx",
			 ulStartAddrOrig));

		goto error_release_mmap_sem;
	}
#if defined(DEBUG)
	psInfo->psVMArea = psVMArea;
#endif

	if (ulStartAddrOrig < psVMArea->vm_start) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Start address %lx is outside of the region returned by find_vma",
			 ulStartAddrOrig));
		goto error_release_mmap_sem;
	}

	if (ulBeyondEndAddrOrig > psVMArea->vm_end) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: End address %lx is outside of the region returned by find_vma",
			 ulBeyondEndAddrOrig));
		goto error_release_mmap_sem;
	}

	if ((psVMArea->vm_flags & (VM_IO | VM_RESERVED)) !=
	    (VM_IO | VM_RESERVED)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: Memory region does not represent memory mapped I/O (VMA flags: 0x%lx)",
			 psVMArea->vm_flags));
		goto error_release_mmap_sem;
	}

	if ((psVMArea->vm_flags & (VM_READ | VM_WRITE)) != (VM_READ | VM_WRITE)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSAcquirePhysPageAddr: No read/write access to memory region (VMA flags: 0x%lx)",
			 psVMArea->vm_flags));
		goto error_release_mmap_sem;
	}

	for (ulAddr = ulStartAddrOrig, i = 0; ulAddr < ulBeyondEndAddrOrig;
	     ulAddr += PAGE_SIZE, i++) {
		struct page *psPage;

		BUG_ON(i >= psInfo->iNumPages);

		psPage = CPUVAddrToPage(psVMArea, ulAddr);
		if (psPage == NULL) {
			int j;

			PVR_TRACE(("OSAcquirePhysPageAddr: Couldn't lookup page structure for address 0x%lx, trying something else", ulAddr));

			for (j = 0; j < i; j++) {
				if (psInfo->bWrapWorkaround)
					put_page(psInfo->ppsPages[j]);
				else
					put_page_testzero(psInfo->ppsPages[j]);
			}
			break;
		}

		psInfo->ppsPages[i] = psPage;
	}

	BUG_ON(i > psInfo->iNumPages);
	if (i == psInfo->iNumPages) {

		for (i = 0; i < psInfo->iNumPages; i++) {
			struct page *psPage = psInfo->ppsPages[i];
			IMG_CPU_PHYADDR CPUPhysAddr;

			CPUPhysAddr.uiAddr = page_to_pfn(psPage) << PAGE_SHIFT;

			psInfo->psPhysAddr[i] =
			    SysCpuPAddrToSysPAddr(CPUPhysAddr);
			psSysPAddr[i] = psInfo->psPhysAddr[i];
		}

		psInfo->eType = WRAP_TYPE_FIND_VMA_PAGES;
	} else {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) && defined(PVR_SECURE_HANDLES)

		if ((psVMArea->vm_flags & VM_PFNMAP) == 0) {
			PVR_DPF((PVR_DBG_WARNING,
				 "OSAcquirePhysPageAddr: Region isn't a raw PFN mapping.  Giving up."));
			goto error_release_mmap_sem;
		}

		for (ulAddr = ulStartAddrOrig, i = 0;
		     ulAddr < ulBeyondEndAddrOrig; ulAddr += PAGE_SIZE, i++) {
			IMG_CPU_PHYADDR CPUPhysAddr;

			CPUPhysAddr.uiAddr =
			    ((ulAddr - psVMArea->vm_start) +
			     (psVMArea->vm_pgoff << PAGE_SHIFT)) & PAGE_MASK;

			psInfo->psPhysAddr[i] =
			    SysCpuPAddrToSysPAddr(CPUPhysAddr);
			psSysPAddr[i] = psInfo->psPhysAddr[i];
		}
		BUG_ON(i != psInfo->iNumPages);

		psInfo->eType = WRAP_TYPE_FIND_VMA_PFN;

		PVR_DPF((PVR_DBG_WARNING,
			 "OSAcquirePhysPageAddr: Region can't be locked down"));
#else
		PVR_DPF((PVR_DBG_WARNING,
			 "OSAcquirePhysPageAddr: Raw PFN mappings not supported.  Giving up."));
		goto error_release_mmap_sem;
#endif
	}

	up_read(&current->mm->mmap_sem);

exit_check:
	CheckPagesContiguous(psInfo);

	*phOSWrapMem = (void *)psInfo;

	return PVRSRV_OK;

error_release_mmap_sem:
	up_read(&current->mm->mmap_sem);
error_free:
	psInfo->eType = WRAP_TYPE_CLEANUP;
	OSReleasePhysPageAddr((void *)psInfo);
	return PVRSRV_ERROR_GENERIC;
}

PVRSRV_ERROR PVROSFuncInit(void)
{
#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	{
		u32 ui32i;

		psTimerWorkQueue = create_workqueue("pvr_timer");
		if (psTimerWorkQueue == NULL) {
			PVR_DPF((PVR_DBG_ERROR,
				 "%s: couldn't create timer workqueue",
				 __FUNCTION__));
			return PVRSRV_ERROR_GENERIC;

		}

		for (ui32i = 0; ui32i < OS_MAX_TIMERS; ui32i++) {
			TIMER_CALLBACK_DATA *psTimerCBData = &sTimers[ui32i];

			INIT_WORK(&psTimerCBData->sWork,
				  OSTimerWorkQueueCallBack);
		}
	}
#endif
	return PVRSRV_OK;
}

void PVROSFuncDeInit(void)
{
#if defined(PVR_LINUX_TIMERS_USING_WORKQUEUES)
	if (psTimerWorkQueue != NULL) {
		destroy_workqueue(psTimerWorkQueue);
	}
#endif
}

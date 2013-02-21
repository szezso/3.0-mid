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

#ifdef DEBUG_RELEASE_BUILD
#pragma optimize( "", off )
#define DEBUG		1
#endif

#ifndef __OSFUNC_H__
#define __OSFUNC_H__


#include <linux/hardirq.h>
#include <linux/string.h>



#define PVRSRV_PAGEABLE_SELECT		PVRSRV_OS_PAGEABLE_HEAP

#define KERNEL_ID			0xffffffffL
#define POWER_MANAGER_ID	0xfffffffeL
#define ISR_ID				0xfffffffdL
#define TIMER_ID			0xfffffffcL


#define HOST_PAGESIZE			OSGetPageSize
#define HOST_PAGEMASK			(~(HOST_PAGESIZE()-1))
#define HOST_PAGEALIGN(addr)	(((addr)+HOST_PAGESIZE()-1)&HOST_PAGEMASK)

#define PVRSRV_OS_HEAP_MASK			0xf
#define PVRSRV_OS_PAGEABLE_HEAP		0x1
#define PVRSRV_OS_NON_PAGEABLE_HEAP	0x2


u32 OSClockus(void);
u32 OSGetPageSize(void);
PVRSRV_ERROR OSInstallDeviceLISR(void *pvSysData,
								 u32 ui32Irq,
								 char *pszISRName,
								 void *pvDeviceNode);
PVRSRV_ERROR OSUninstallDeviceLISR(void *pvSysData);
PVRSRV_ERROR OSInstallSystemLISR(void *pvSysData, u32 ui32Irq);
PVRSRV_ERROR OSUninstallSystemLISR(void *pvSysData);
PVRSRV_ERROR OSInstallMISR(void *pvSysData);
PVRSRV_ERROR OSUninstallMISR(void *pvSysData);
IMG_CPU_PHYADDR OSMapLinToCPUPhys(void* pvLinAddr);
void *OSMapPhysToLin(IMG_CPU_PHYADDR BasePAddr, u32 ui32Bytes, u32 ui32Flags, void * *phOSMemHandle);
int OSUnMapPhysToLin(void *pvLinAddr, u32 ui32Bytes, u32 ui32Flags, void * hOSMemHandle);

PVRSRV_ERROR OSReservePhys(IMG_CPU_PHYADDR BasePAddr, u32 ui32Bytes, u32 ui32Flags, void **ppvCpuVAddr, void * *phOSMemHandle);
PVRSRV_ERROR OSUnReservePhys(void *pvCpuVAddr, u32 ui32Bytes, u32 ui32Flags, void * hOSMemHandle);

#if defined(SUPPORT_CPU_CACHED_BUFFERS)
void OSFlushCPUCacheKM(void);
void OSFlushCPUCacheRangeKM(void *pvRangeAddrStart,
						 	void *pvRangeAddrEnd);
#endif

PVRSRV_ERROR OSRegisterDiscontigMem(IMG_SYS_PHYADDR *pBasePAddr,
									void *pvCpuVAddr,
									u32 ui32Bytes,
									u32 ui32Flags,
									void * *phOSMemHandle);
PVRSRV_ERROR OSUnRegisterDiscontigMem(void *pvCpuVAddr,
									u32 ui32Bytes,
									u32 ui32Flags,
									void * hOSMemHandle);

static inline PVRSRV_ERROR OSReserveDiscontigPhys(IMG_SYS_PHYADDR *pBasePAddr, u32 ui32Bytes, u32 ui32Flags, void **ppvCpuVAddr, void * *phOSMemHandle)
{
	*ppvCpuVAddr = NULL;
	return OSRegisterDiscontigMem(pBasePAddr, *ppvCpuVAddr, ui32Bytes, ui32Flags, phOSMemHandle);
}

static inline PVRSRV_ERROR OSUnReserveDiscontigPhys(void *pvCpuVAddr, u32 ui32Bytes, u32 ui32Flags, void * hOSMemHandle)
{
	OSUnRegisterDiscontigMem(pvCpuVAddr, ui32Bytes, ui32Flags, hOSMemHandle);
	return PVRSRV_OK;
}

PVRSRV_ERROR OSRegisterMem(IMG_CPU_PHYADDR BasePAddr,
							void *pvCpuVAddr,
							u32 ui32Bytes,
							u32 ui32Flags,
							void * *phOSMemHandle);
PVRSRV_ERROR OSUnRegisterMem(void *pvCpuVAddr,
							u32 ui32Bytes,
							u32 ui32Flags,
							void * hOSMemHandle);



PVRSRV_ERROR OSGetSubMemHandle(void * hOSMemHandle,
							   u32 ui32ByteOffset,
							   u32 ui32Bytes,
							   u32 ui32Flags,
							   void * *phOSMemHandleRet);
PVRSRV_ERROR OSReleaseSubMemHandle(void * hOSMemHandle, u32 ui32Flags);

u32 OSGetCurrentProcessIDKM(void);
u32 OSGetCurrentThreadID( void );

PVRSRV_ERROR OSAllocPages_Impl(u32 ui32Flags, u32 ui32Size, u32 ui32PageSize, void * *ppvLinAddr, void * *phPageAlloc);
PVRSRV_ERROR OSFreePages(u32 ui32Flags, u32 ui32Size, void * pvLinAddr, void * hPageAlloc);


#ifdef PVRSRV_LOG_MEMORY_ALLOCS
	#define OSAllocMem(flags, size, linAddr, blockAlloc, logStr) \
		(PVR_TRACE(("OSAllocMem(" #flags ", " #size ", " #linAddr ", " #blockAlloc "): " logStr " (size = 0x%lx)", size)), \
			OSAllocMem_Debug_Wrapper(flags, size, linAddr, blockAlloc, __FILE__, __LINE__))

	#define OSAllocPages(flags, size, pageSize, linAddr, pageAlloc) \
		(PVR_TRACE(("OSAllocPages(" #flags ", " #size ", " #pageSize ", " #linAddr ", " #pageAlloc "): (size = 0x%lx)", size)), \
			OSAllocPages_Impl(flags, size, pageSize, linAddr, pageAlloc))

	#define OSFreeMem(flags, size, linAddr, blockAlloc) \
		(PVR_TRACE(("OSFreeMem(" #flags ", " #size ", " #linAddr ", " #blockAlloc "): (pointer = 0x%X)", linAddr)), \
			OSFreeMem_Debug_Wrapper(flags, size, linAddr, blockAlloc, __FILE__, __LINE__))
#else
	#define OSAllocMem(flags, size, linAddr, blockAlloc, logString) \
		OSAllocMem_Debug_Wrapper(flags, size, linAddr, blockAlloc, __FILE__, __LINE__)

	#define OSAllocPages OSAllocPages_Impl

	#define OSFreeMem(flags, size, linAddr, blockAlloc) \
			OSFreeMem_Debug_Wrapper(flags, size, linAddr, blockAlloc, __FILE__, __LINE__)
#endif

#ifdef PVRSRV_DEBUG_OS_MEMORY

	PVRSRV_ERROR OSAllocMem_Debug_Wrapper(u32 ui32Flags,
										u32 ui32Size,
										void * *ppvCpuVAddr,
										void * *phBlockAlloc,
										char *pszFilename,
										u32 ui32Line);

	PVRSRV_ERROR OSFreeMem_Debug_Wrapper(u32 ui32Flags,
									 u32 ui32Size,
									 void * pvCpuVAddr,
									 void * hBlockAlloc,
									 char *pszFilename,
									 u32 ui32Line);


	typedef struct
	{
		u8 sGuardRegionBefore[8];
		char sFileName[128];
		u32 uLineNo;
		u32 uSize;
		u32 uSizeParityCheck;
		enum valid_tag
		{	isFree = 0x277260FF,
			isAllocated = 0x260511AA
		} eValid;
	} OSMEM_DEBUG_INFO;

	#define TEST_BUFFER_PADDING_STATUS (sizeof(OSMEM_DEBUG_INFO))
	#define TEST_BUFFER_PADDING_AFTER  (8)
	#define TEST_BUFFER_PADDING (TEST_BUFFER_PADDING_STATUS + TEST_BUFFER_PADDING_AFTER)
#else
	#define OSAllocMem_Debug_Wrapper OSAllocMem_Debug_Linux_Memory_Allocations
	#define OSFreeMem_Debug_Wrapper OSFreeMem_Debug_Linux_Memory_Allocations
#endif

#if defined(__linux__) && defined(DEBUG_LINUX_MEMORY_ALLOCATIONS)
    PVRSRV_ERROR OSAllocMem_Impl(u32 ui32Flags, u32 ui32Size, void * *ppvLinAddr, void * *phBlockAlloc, char *pszFilename, u32     ui32Line);
    PVRSRV_ERROR OSFreeMem_Impl(u32 ui32Flags, u32 ui32Size, void * pvLinAddr, void * hBlockAlloc, char *pszFilename, u32 ui32L    ine);

    #define OSAllocMem_Debug_Linux_Memory_Allocations OSAllocMem_Impl
    #define OSFreeMem_Debug_Linux_Memory_Allocations OSFreeMem_Impl
#else
    PVRSRV_ERROR OSAllocMem_Impl(u32 ui32Flags, u32 ui32Size, void * *ppvLinAddr, void * *phBlockAlloc);
    PVRSRV_ERROR OSFreeMem_Impl(u32 ui32Flags, u32 ui32Size, void * pvLinAddr, void * hBlockAlloc);

    #define OSAllocMem_Debug_Linux_Memory_Allocations(flags, size, addr, blockAlloc, file, line) \
        OSAllocMem_Impl(flags, size, addr, blockAlloc)
    #define OSFreeMem_Debug_Linux_Memory_Allocations(flags, size, addr, blockAlloc, file, line) \
        OSFreeMem_Impl(flags, size, addr, blockAlloc)
#endif


IMG_CPU_PHYADDR OSMemHandleToCpuPAddr(void *hOSMemHandle, u32 ui32ByteOffset);

PVRSRV_ERROR OSInitEnvData(void * *ppvEnvSpecificData);
PVRSRV_ERROR OSDeInitEnvData(void * pvEnvSpecificData);

PVRSRV_ERROR OSEventObjectCreate(const char *pszName,
								 PVRSRV_EVENTOBJECT *psEventObject);
PVRSRV_ERROR OSEventObjectDestroy(PVRSRV_EVENTOBJECT *psEventObject);
PVRSRV_ERROR OSEventObjectSignal(void * hOSEventKM);
PVRSRV_ERROR OSEventObjectWait(void * hOSEventKM);
PVRSRV_ERROR OSEventObjectOpen(PVRSRV_EVENTOBJECT *psEventObject,
											void * *phOSEvent);
PVRSRV_ERROR OSEventObjectClose(PVRSRV_EVENTOBJECT *psEventObject,
											void * hOSEventKM);


PVRSRV_ERROR OSBaseAllocContigMemory(u32 ui32Size, IMG_CPU_VIRTADDR *pLinAddr, IMG_CPU_PHYADDR *pPhysAddr);
PVRSRV_ERROR OSBaseFreeContigMemory(u32 ui32Size, IMG_CPU_VIRTADDR LinAddr, IMG_CPU_PHYADDR PhysAddr);

void * MapUserFromKernel(void * pvLinAddrKM,u32 ui32Size,void * *phMemBlock);
void * OSMapHWRegsIntoUserSpace(void * hDevCookie, IMG_SYS_PHYADDR sRegAddr, u32 ulSize, void * *ppvProcess);
void  OSUnmapHWRegsFromUserSpace(void * hDevCookie, void * pvUserAddr, void * pvProcess);

void  UnmapUserFromKernel(void * pvLinAddrUM, u32 ui32Size, void * hMemBlock);

PVRSRV_ERROR OSMapPhysToUserSpace(void * hDevCookie,
								  IMG_SYS_PHYADDR sCPUPhysAddr,
								  u32 uiSizeInBytes,
								  u32 ui32CacheFlags,
								  void * *ppvUserAddr,
								  u32 *puiActualSize,
								  void * hMappingHandle);

PVRSRV_ERROR OSUnmapPhysToUserSpace(void * hDevCookie,
									void * pvUserAddr,
									void * pvProcess);

PVRSRV_ERROR OSLockResource(PVRSRV_RESOURCE *psResource, u32 ui32ID);
PVRSRV_ERROR OSUnlockResource(PVRSRV_RESOURCE *psResource, u32 ui32ID);
int OSIsResourceLocked(PVRSRV_RESOURCE *psResource, u32 ui32ID);
PVRSRV_ERROR OSCreateResource(PVRSRV_RESOURCE *psResource);
PVRSRV_ERROR OSDestroyResource(PVRSRV_RESOURCE *psResource);
void OSBreakResourceLock(PVRSRV_RESOURCE *psResource, u32 ui32ID);
void OSWaitus(u32 ui32Timeus);
void OSReleaseThreadQuanta(void);
u32 OSPCIReadDword(u32 ui32Bus, u32 ui32Dev, u32 ui32Func, u32 ui32Reg);
void OSPCIWriteDword(u32 ui32Bus, u32 ui32Dev, u32 ui32Func, u32 ui32Reg, u32 ui32Value);

#ifndef OSReadHWReg
u32 OSReadHWReg(void * pvLinRegBaseAddr, u32 ui32Offset);
#endif
#ifndef OSWriteHWReg
void OSWriteHWReg(void * pvLinRegBaseAddr, u32 ui32Offset, u32 ui32Value);
#endif

typedef void (*PFN_TIMER_FUNC)(void*);
void * OSAddTimer(PFN_TIMER_FUNC pfnTimerFunc, void *pvData, u32 ui32MsTimeout);
PVRSRV_ERROR OSRemoveTimer (void * hTimer);
PVRSRV_ERROR OSEnableTimer (void * hTimer);
PVRSRV_ERROR OSDisableTimer (void * hTimer);

PVRSRV_ERROR OSGetSysMemSize(u32 *pui32Bytes);

typedef enum _HOST_PCI_INIT_FLAGS_
{
	HOST_PCI_INIT_FLAG_BUS_MASTER	= 0x00000001,
	HOST_PCI_INIT_FLAG_MSI		= 0x00000002,
	HOST_PCI_INIT_FLAG_FORCE_I32 	= 0x7fffffff
} HOST_PCI_INIT_FLAGS;

struct _PVRSRV_PCI_DEV_OPAQUE_STRUCT_;
typedef struct _PVRSRV_PCI_DEV_OPAQUE_STRUCT_ *PVRSRV_PCI_DEV_HANDLE;

PVRSRV_PCI_DEV_HANDLE OSPCIAcquireDev(u16 ui16VendorID, u16 ui16DeviceID, HOST_PCI_INIT_FLAGS eFlags);
PVRSRV_PCI_DEV_HANDLE OSPCISetDev(void *pvPCICookie, HOST_PCI_INIT_FLAGS eFlags);
PVRSRV_ERROR OSPCIReleaseDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI);
PVRSRV_ERROR OSPCIIRQ(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 *pui32IRQ);
u32 OSPCIAddrRangeLen(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index);
u32 OSPCIAddrRangeStart(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index);
u32 OSPCIAddrRangeEnd(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index);
PVRSRV_ERROR OSPCIRequestAddrRange(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index);
PVRSRV_ERROR OSPCIReleaseAddrRange(PVRSRV_PCI_DEV_HANDLE hPVRPCI, u32 ui32Index);
PVRSRV_ERROR OSPCISuspendDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI);
PVRSRV_ERROR OSPCIResumeDev(PVRSRV_PCI_DEV_HANDLE hPVRPCI);

PVRSRV_ERROR OSScheduleMISR(void *pvSysData);

void OSPanic(void);

int OSProcHasPrivSrvInit(void);

typedef enum _img_verify_test
{
	PVR_VERIFY_WRITE = 0,
	PVR_VERIFY_READ
} IMG_VERIFY_TEST;

int OSAccessOK(IMG_VERIFY_TEST eVerification, void *pvUserPtr, u32 ui32Bytes);

PVRSRV_ERROR OSCopyToUser(void * pvProcess, void *pvDest, void *pvSrc, u32 ui32Bytes);
PVRSRV_ERROR OSCopyFromUser(void * pvProcess, void *pvDest, void *pvSrc, u32 ui32Bytes);

PVRSRV_ERROR OSAcquirePhysPageAddr(void* pvCPUVAddr,
									u32 ui32Bytes,
									IMG_SYS_PHYADDR *psSysPAddr,
									void * *phOSWrapMem,
									int bWrapWorkaround);
PVRSRV_ERROR OSReleasePhysPageAddr(void * hOSWrapMem);


#define	OS_SUPPORTS_IN_LISR


#endif


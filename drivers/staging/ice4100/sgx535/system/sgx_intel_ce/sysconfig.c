/**********************************************************************
 *
 * Copyright (c) 2008-2010 Intel Corporation.
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
 *     Intel Corporation
 *     2200 Mission College Blvd.
 *     Santa Clara, CA  97052
 *
 ******************************************************************************/

/* This file was based on the Imagination Technologies sample implementation. */

#include "sgxdefs.h"
#include "services_headers.h"
#include "kerneldisplay.h"
#include "oemfuncs.h"
#include "sgxinfo.h"
#include "pdump_km.h"
#include "sgxinfokm.h"
#include "syslocal.h"

#ifdef __linux__
#include "mm.h"
#endif

#ifdef	LDM_PCI
#include "linux/pci.h"
#endif

#ifdef INTEL_D3_PM
#include "graphics_pm.h"
#endif

#include "linux/pci.h"

#define SYS_SGX_CLOCK_SPEED					(400000000)
#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ		(100)
#define SYS_SGX_PDS_TIMER_FREQ				(1000)
#define SYS_SGX_ACTIVE_POWER_LATENCY_MS		(500)

SYS_DATA *gpsSysData = (SYS_DATA *) NULL;
SYS_DATA gsSysData;

static SYS_SPECIFIC_DATA gsSysSpecificData;

static u32 gui32SGXDeviceID;
static SGX_DEVICE_MAP gsSGXDeviceMap;

#ifdef	LDM_PCI
extern struct pci_dev *gpsPVRLDMDev;
#endif

u32 PVRSRV_BridgeDispatchKM(u32 Ioctl,
			unsigned char * pInBuf,
			u32 InBufLen,
			unsigned char * pOutBuf,
			u32 OutBufLen, u32 * pdwBytesTransferred);

static PVRSRV_ERROR InitSGXPCIDev(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	struct pci_dev *psPCIDev = NULL;
	psPCIDev = pci_get_device(SYS_SGX_DEV_VENDOR_ID, SYS_SGX_DEV_DEVICE_ID, NULL);
	if (psPCIDev != NULL)
	{
		if (pci_enable_device(psPCIDev))
			goto init_failed;

		psSysSpecData->psSGXPCIDev = psPCIDev;

		return PVRSRV_OK;
	}

init_failed:
	psSysSpecData->psSGXPCIDev = NULL;
	PVR_DPF((PVR_DBG_ERROR,"InitSGXPCIDev: failed to initialize the sgx PCI device!\n"));
	return PVRSRV_ERROR_GENERIC;
}

static void DeinitSGXPCIDev(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	if (psSysSpecData->psSGXPCIDev != NULL)
	{
		pci_disable_device(psSysSpecData->psSGXPCIDev);
	}
}

static PVRSRV_ERROR GetSGXPCIDevIRQ(SYS_DATA *psSysData, u32 *pui32IRQ)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	if (psSysSpecData->psSGXPCIDev != NULL)
	{
		*pui32IRQ = psSysSpecData->psSGXPCIDev->irq;
		return PVRSRV_OK;
	}

	return PVRSRV_ERROR_GENERIC;
}


static PVRSRV_ERROR SysLocateDevices(SYS_DATA * psSysData)
{
	u32 ui32SGXRegBaseAddr, ui32SGXMemBaseAddr;
	u32 ui32IRQ;

#ifdef INTEL_D3_NO_PCI_ENUM
	ui32SGXRegBaseAddr = 0xDC000000;
	ui32IRQ = 4;
#else

#define SGX_PCI_BUS 1
#define SGX_PCI_DEV 2
#define SGX_PCI_FN  0

#define BAR0 0x10

	ui32SGXRegBaseAddr = OSPCIReadDword(SGX_PCI_BUS,
					    SGX_PCI_DEV, SGX_PCI_FN, BAR0);
	if(GetSGXPCIDevIRQ(psSysData, &ui32IRQ) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "sysLocateDevices: failed to get the pci IRQ!"));
		return PVRSRV_ERROR_INVALID_DEVICE;
	}

#endif

	// This address was only ever used in the addr translation functions. We took
	// that out though, so it isn't used anywhere
#ifdef INTEL_D3_LOCALMEM
	ui32SGXMemBaseAddr = 0x18000000;
#else
	ui32SGXMemBaseAddr = 0;
#endif

	PVR_TRACE(("IRQ: %d", ui32IRQ));

	gsSGXDeviceMap.ui32Flags = 0x0;

	gsSGXDeviceMap.sRegsSysPBase.uiAddr =
	    ui32SGXRegBaseAddr + SYS_SGX_REG_OFFSET;
	//gsSGXDeviceMap.sRegsDevPBase = SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX, gsSGXDeviceMap.sRegsSysPBase);
	gsSGXDeviceMap.sRegsCpuPBase =
	    SysSysPAddrToCpuPAddr(gsSGXDeviceMap.sRegsSysPBase);
	gsSGXDeviceMap.ui32RegsSize = SYS_SGX_REG_SIZE;

	gsSGXDeviceMap.sLocalMemSysPBase.uiAddr = ui32SGXMemBaseAddr;
	gsSGXDeviceMap.sLocalMemDevPBase =
	    SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE_SGX,
				  gsSGXDeviceMap.sLocalMemSysPBase);
	gsSGXDeviceMap.sLocalMemCpuPBase =
	    SysSysPAddrToCpuPAddr(gsSGXDeviceMap.sLocalMemSysPBase);
	gsSGXDeviceMap.ui32LocalMemSize = SYS_LOCALMEM_FOR_SGX_RESERVE_SIZE;

	gsSGXDeviceMap.ui32IRQ = ui32IRQ;

	return PVRSRV_OK;
}

PVRSRV_ERROR SysInitialise(void)
{
	u32 i;
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SGX_TIMING_INFORMATION *psTimingInfo;

	gpsSysData = &gsSysData;
	memset(gpsSysData, 0, sizeof(SYS_DATA));

	gpsSysData->pvSysSpecificData = &gsSysSpecificData;
	gsSysSpecificData.ui32SysSpecificData = 0;
#ifdef	LDM_PCI

	PVR_ASSERT(gpsPVRLDMDev != NULL);
	gsSysSpecificData.psPCIDev = gpsPVRLDMDev;
#endif

	eError = OSInitEnvData(&gpsSysData->pvEnvSpecificData);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitialise: Failed to setup env structure"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_ENVDATA);

	psTimingInfo = &gsSGXDeviceMap.sTimingInfo;
	psTimingInfo->ui32CoreClockSpeed = SYS_SGX_CLOCK_SPEED;
	psTimingInfo->ui32HWRecoveryFreq = SYS_SGX_HWRECOVERY_TIMEOUT_FREQ;
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = 1;
#else
	psTimingInfo->bEnableActivePM = 0;
#endif
	psTimingInfo->ui32ActivePowManLatencyms =
	    SYS_SGX_ACTIVE_POWER_LATENCY_MS;
	psTimingInfo->ui32uKernelFreq = SYS_SGX_PDS_TIMER_FREQ;

	eError = InitSGXPCIDev(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}


	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_PCINIT);

	gpsSysData->ui32NumDevices = SYS_DEVICE_COUNT;

	for (i = 0; i < SYS_DEVICE_COUNT; i++) {
		gpsSysData->sDeviceID[i].uiID = i;
		gpsSysData->sDeviceID[i].bInUse = 0;
	}

	gpsSysData->psDeviceNodeList = NULL;
	gpsSysData->psQueueList = NULL;

	eError = SysInitialiseCommon(gpsSysData);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitialise: Failed in SysInitialiseCommon"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}

	eError = SysLocateDevices(gpsSysData);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitialise: Failed to locate devices"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_LOCATEDEV);

	eError = SysInitRegisters();
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitRegisters: Failed to initialise registers"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_INITREG);

#if defined(READ_TCF_HOST_MEM_SIGNATURE)
	SysTestTCFHostMemSig(gsSGXDeviceMap.sRegsCpuPBase,
			     gsSGXDeviceMap.sLocalMemCpuPBase);
#endif

	eError =
	    PVRSRVRegisterDevice(gpsSysData, SGXRegisterDevice, 0x00000001,
				 &gui32SGXDeviceID);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitialise: Failed to register device!"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_REGDEV);

#ifdef INTEL_D3_LOCALMEM
	gpsSysData->apsLocalDevMemArena[0] = RA_Create("SGXLocalDeviceMemory",
						       gsSGXDeviceMap.
						       sLocalMemSysPBase.uiAddr,
						       gsSGXDeviceMap.
						       ui32LocalMemSize, NULL,
						       HOST_PAGESIZE(), NULL,
						       NULL, NULL, NULL);
#else
	gpsSysData->apsLocalDevMemArena[0] = NULL;
#endif

	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_RA_ARENA);

	psDeviceNode = gpsSysData->psDeviceNodeList;
	while (psDeviceNode) {
		switch (psDeviceNode->sDevId.eDeviceType) {
		case PVRSRV_DEVICE_TYPE_SGX:
			{
				DEVICE_MEMORY_INFO *psDevMemoryInfo;
				DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;

				psDeviceNode->psLocalDevMemArena =
				    gpsSysData->apsLocalDevMemArena[0];

				psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
				psDeviceMemoryHeap =
				    psDevMemoryInfo->psDeviceMemoryHeap;

				for (i = 0; i < psDevMemoryInfo->ui32HeapCount;
				     i++) {
#ifdef INTEL_D3_LOCALMEM
					psDeviceMemoryHeap[i].ui32Attribs |=
					    PVRSRV_BACKINGSTORE_LOCALMEM_CONTIG;
#else
					psDeviceMemoryHeap[i].ui32Attribs |=
					    PVRSRV_BACKINGSTORE_SYSMEM_NONCONTIG;
					//psDeviceMemoryHeap[i].ui32Attribs |= PVRSRV_BACKINGSTORE_SYSMEM_CONTIG;
#endif
					psDeviceMemoryHeap[i].
					    psLocalDevMemArena =
					    gpsSysData->apsLocalDevMemArena[0];

				}
				break;
			}
		default:
			break;
		}

		psDeviceNode = psDeviceNode->psNext;
	}

	PDUMPINIT();
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_PDUMPINIT);

	eError = PVRSRVInitialiseDevice(gui32SGXDeviceID);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitialise: Failed to initialise device!"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_INITDEV);

#ifdef INTEL_D3_PM
	{
		int pm_rc = graphics_pm_init();
		if (GRAPHICS_PM_OK != pm_rc) {
			eError = PVRSRV_ERROR_INIT_FAILURE;
			PVR_DPF((PVR_DBG_ERROR,
				 "SysInitialise: Failed to initialise power management!"));
			SysDeinitialise(gpsSysData);
			gpsSysData = NULL;
			return eError;
		}
		SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
				      SYS_SPECIFIC_DATA_ENABLE_GRAPHICS_PM);
	}
#endif

	PVR_DPF((PVR_DBG_WARNING, "SysInitialise: OK 0x%x",
		 gsSysSpecificData.ui32SysSpecificData));
	return PVRSRV_OK;
}

PVRSRV_ERROR SysFinalise(void)
{
#if defined(SYS_USING_INTERRUPTS)
	PVRSRV_ERROR eError;

	eError = OSInstallMISR(gpsSysData);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallMISR: Failed to install MISR"));
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_MISR);

	eError = OSInstallSystemLISR(gpsSysData, gsSGXDeviceMap.ui32IRQ);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "OSInstallSystemLISR: Failed to install ISR"));
		OSUninstallMISR(gpsSysData);
		SysDeinitialise(gpsSysData);
		gpsSysData = NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
			      SYS_SPECIFIC_DATA_ENABLE_LISR);
	SysEnableInterrupts(gpsSysData);
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_IRQ);
#endif

	gpsSysData->pszVersionString =
	    SysCreateVersionString(gsSGXDeviceMap.sRegsCpuPBase);
	if (!gpsSysData->pszVersionString) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysFinalise: Failed to create a system version string"));
	} else {
		PVR_DPF((PVR_DBG_WARNING, "SysFinalise: Version string: %s",
			 gpsSysData->pszVersionString));
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDeinitialise(SYS_DATA * psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData;
	PVRSRV_ERROR eError;

	if (psSysData == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysDeinitialise: Called with NULL SYS_DATA pointer.  Probably called before."));
		return PVRSRV_OK;
	}

	psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

#ifdef INTEL_D3_PM
	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_GRAPHICS_PM)) {
		int pm_rc = graphics_pm_deinit();
		if (GRAPHICS_PM_OK != pm_rc) {
			PVR_DPF((PVR_DBG_ERROR,
				 "SysDeinitialise: Failed to deinitialise power management!"));
			/* Continue with deinit even if failed to deinit power management */
		}
	}
#endif

#if defined(SYS_USING_INTERRUPTS)
	if (SYS_SPECIFIC_DATA_TEST(psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_IRQ)) {
		SysDisableInterrupts(psSysData);
	}
	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_LISR)) {
		eError = OSUninstallSystemLISR(psSysData);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "SysDeinitialise: OSUninstallSystemLISR failed"));
			return eError;
		}
	}
	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_MISR)) {
		eError = OSUninstallMISR(psSysData);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "SysDeinitialise: OSUninstallMISR failed"));
			return eError;
		}
	}
#endif
	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_INITDEV)) {
		eError = PVRSRVDeinitialiseDevice(gui32SGXDeviceID);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "SysDeinitialise: failed to de-init the device"));
			return eError;
		}
	}

	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_RA_ARENA)) {
#ifdef INTEL_D3_LOCALMEM
		RA_Delete(gpsSysData->apsLocalDevMemArena[0]);
#endif
		gpsSysData->apsLocalDevMemArena[0] = NULL;
	}

	SysDeinitialiseCommon(gpsSysData);

	DeinitSGXPCIDev(gpsSysData);

	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_ENVDATA)) {
		eError = OSDeInitEnvData(gpsSysData->pvEnvSpecificData);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "SysDeinitialise: failed to de-init env structure"));
			return eError;
		}
	}

	if (SYS_SPECIFIC_DATA_TEST
	    (psSysSpecData, SYS_SPECIFIC_DATA_ENABLE_PDUMPINIT)) {
		PDUMPDEINIT();
	}

	psSysSpecData->ui32SysSpecificData = 0;

	gpsSysData = NULL;

	return PVRSRV_OK;
}

PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE eDeviceType,
				   void **ppvDeviceMap)
{

	switch (eDeviceType) {
	case PVRSRV_DEVICE_TYPE_SGX:
		{
			*ppvDeviceMap = (void *)&gsSGXDeviceMap;
			break;
		}
	default:
		{
			PVR_DPF((PVR_DBG_ERROR,
				 "SysGetDeviceMemoryMap: unsupported device type"));
		}
	}
	return PVRSRV_OK;
}

IMG_DEV_PHYADDR SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE eDeviceType,
				      IMG_CPU_PHYADDR CpuPAddr)
{
	IMG_DEV_PHYADDR DevPAddr;

	DevPAddr.uiAddr = CpuPAddr.uiAddr;

	return DevPAddr;
}

IMG_CPU_PHYADDR SysSysPAddrToCpuPAddr(IMG_SYS_PHYADDR sys_paddr)
{
	IMG_CPU_PHYADDR cpu_paddr;

	cpu_paddr.uiAddr = sys_paddr.uiAddr;

	return cpu_paddr;
}

IMG_SYS_PHYADDR SysCpuPAddrToSysPAddr(IMG_CPU_PHYADDR cpu_paddr)
{
	IMG_SYS_PHYADDR sys_paddr;

	sys_paddr.uiAddr = cpu_paddr.uiAddr;

	return sys_paddr;
}

IMG_DEV_PHYADDR SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE eDeviceType,
				      IMG_SYS_PHYADDR SysPAddr)
{
	IMG_DEV_PHYADDR DevPAddr;

	DevPAddr.uiAddr = SysPAddr.uiAddr;

	return DevPAddr;
}

IMG_SYS_PHYADDR SysDevPAddrToSysPAddr(PVRSRV_DEVICE_TYPE eDeviceType,
				      IMG_DEV_PHYADDR DevPAddr)
{
	IMG_SYS_PHYADDR SysPAddr;

	SysPAddr.uiAddr = DevPAddr.uiAddr;

	return SysPAddr;
}

void SysRegisterExternalDevice(PVRSRV_DEVICE_NODE * psDeviceNode)
{
}

void SysRemoveExternalDevice(PVRSRV_DEVICE_NODE * psDeviceNode)
{
}

PVRSRV_ERROR SysResetDevice(u32 ui32DeviceIndex)
{
	if (ui32DeviceIndex == gui32SGXDeviceID) {
		SysResetSGX(gpsSysData->pvSOCRegsBase);
	} else {
		return PVRSRV_ERROR_GENERIC;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysOEMFunction(u32 ui32ID,
			    void *pvIn,
			    u32 ulInSize, void *pvOut, u32 ulOutSize)
{
	if (ulInSize || pvIn) ;

	if ((ui32ID == OEM_GET_EXT_FUNCS) &&
	    (ulOutSize == sizeof(PVRSRV_DC_OEM_JTABLE))) {
		PVRSRV_DC_OEM_JTABLE *psOEMJTable =
		    (PVRSRV_DC_OEM_JTABLE *) pvOut;
		psOEMJTable->pfnOEMBridgeDispatch = &PVRSRV_BridgeDispatchKM;
#if !defined(SERVICES4)
		psOEMJTable->pfnOEMReadRegistryString =
		    &PVRSRVReadRegistryString;
		psOEMJTable->pfnOEMWriteRegistryString =
		    &PVRSRVWriteRegistryString;
#endif
		return PVRSRV_OK;
	}

	return PVRSRV_ERROR_INVALID_PARAMS;
}

void SysSaveRestoreArenaLiveSegments(int bSave)
{
	u32 uiBufferSize;
	static void *pvBackupBuffer = NULL;
	static void *hBlockAlloc;
	static u32 uiWriteBufferSize = 0;
	PVRSRV_ERROR eError;

	uiBufferSize = 0;

	if (gpsSysData->apsLocalDevMemArena[0] != NULL) {

		if (PVRSRVSaveRestoreLiveSegments
		    ((void *)gpsSysData->apsLocalDevMemArena[0], NULL,
		     &uiBufferSize, bSave) == PVRSRV_OK) {
			if (uiBufferSize) {
				if (bSave && pvBackupBuffer == NULL) {
					uiWriteBufferSize = uiBufferSize;

					eError =
					    OSAllocPages(PVRSRV_HAP_KERNEL_ONLY
							 | PVRSRV_HAP_CACHED,
							 uiBufferSize,
							 HOST_PAGESIZE(),
							 &pvBackupBuffer,
							 &hBlockAlloc);
					if (eError != PVRSRV_OK) {
						PVR_DPF((PVR_DBG_ERROR,
							 "SysSaveRestoreArenaLiveSegments: OSAllocPages(0x%x) failed:%lu",
							 uiBufferSize, eError));
						return;
					}
				} else {
					PVR_ASSERT(uiWriteBufferSize ==
						   uiBufferSize);
				}

				PVRSRVSaveRestoreLiveSegments((void *)
							      gpsSysData->
							      apsLocalDevMemArena
							      [0],
							      pvBackupBuffer,
							      &uiBufferSize,
							      bSave);

				if (!bSave && pvBackupBuffer) {

					eError =
					    OSFreePages(PVRSRV_HAP_KERNEL_ONLY |
							PVRSRV_HAP_CACHED,
							uiWriteBufferSize,
							pvBackupBuffer,
							hBlockAlloc);
					if (eError != PVRSRV_OK) {
						PVR_DPF((PVR_DBG_ERROR,
							 "SysSaveRestoreArenaLiveSegments: OSFreePages(0x%x) failed:%lu",
							 uiBufferSize, eError));
					}

					pvBackupBuffer = NULL;
				}
			}
		}
	}

}

PVRSRV_ERROR SysMapInRegisters(void)
{
	PVRSRV_DEVICE_NODE *psDeviceNodeList;

	psDeviceNodeList = gpsSysData->psDeviceNodeList;

	while (psDeviceNodeList) {
		PVRSRV_SGXDEV_INFO *psDevInfo =
		    (PVRSRV_SGXDEV_INFO *) psDeviceNodeList->pvDevice;
		if (psDeviceNodeList->sDevId.eDeviceType ==
		    PVRSRV_DEVICE_TYPE_SGX) {

			psDevInfo->pvRegsBaseKM =
			    OSMapPhysToLin(gsSGXDeviceMap.sRegsCpuPBase,
					   gsSGXDeviceMap.ui32RegsSize,
					   PVRSRV_HAP_KERNEL_ONLY |
					   PVRSRV_HAP_UNCACHED, NULL);

			if (!psDevInfo->pvRegsBaseKM) {
				PVR_DPF((PVR_DBG_ERROR,
					 "SysMapInRegisters : Failed to map in regs\n"));
				return PVRSRV_ERROR_BAD_MAPPING;
			}

			psDevInfo->ui32RegSize = gsSGXDeviceMap.ui32RegsSize;
			psDevInfo->sRegsPhysBase = gsSGXDeviceMap.sRegsSysPBase;

#ifdef SGX_FEATURE_2D_HARDWARE

			psDevInfo->s2DSlavePortKM.pvData =
			    OSMapPhysToLin(gsSGXDeviceMap.sSPCpuPBase,
					   gsSGXDeviceMap.ui32SPSize,
					   PVRSRV_HAP_KERNEL_ONLY |
					   PVRSRV_HAP_UNCACHED, NULL);

			if (!psDevInfo->s2DSlavePortKM.pvData) {
				PVR_DPF((PVR_DBG_ERROR,
					 "SysMapInRegisters : Failed to map 2D Slave port region\n"));
				return PVRSRV_ERROR_BAD_MAPPING;
			}

			psDevInfo->s2DSlavePortKM.ui32DataRange =
			    gsSGXDeviceMap.ui32SPSize;
			psDevInfo->s2DSlavePortKM.sPhysBase =
			    gsSGXDeviceMap.sSPSysPBase;
#endif

		}

		psDeviceNodeList = psDeviceNodeList->psNext;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysUnmapRegisters(void)
{
	PVRSRV_DEVICE_NODE *psDeviceNodeList;

	psDeviceNodeList = gpsSysData->psDeviceNodeList;

	while (psDeviceNodeList) {
		PVRSRV_SGXDEV_INFO *psDevInfo =
		    (PVRSRV_SGXDEV_INFO *) psDeviceNodeList->pvDevice;
		if (psDeviceNodeList->sDevId.eDeviceType ==
		    PVRSRV_DEVICE_TYPE_SGX) {

			if (psDevInfo->pvRegsBaseKM) {
				OSUnMapPhysToLin(psDevInfo->pvRegsBaseKM,
						 gsSGXDeviceMap.ui32RegsSize,
						 PVRSRV_HAP_KERNEL_ONLY |
						 PVRSRV_HAP_UNCACHED, NULL);

				psDevInfo->pvRegsBaseKM = NULL;
			}

			psDevInfo->pvRegsBaseKM = NULL;
			psDevInfo->ui32RegSize = 0;
			psDevInfo->sRegsPhysBase.uiAddr = 0;

#ifdef SGX_FEATURE_2D_HARDWARE

			if (psDevInfo->s2DSlavePortKM.pvData) {
				OSUnMapPhysToLin(psDevInfo->s2DSlavePortKM.
						 pvData,
						 gsSGXDeviceMap.ui32SPSize,
						 PVRSRV_HAP_KERNEL_ONLY |
						 PVRSRV_HAP_UNCACHED, NULL);

				psDevInfo->s2DSlavePortKM.pvData = NULL;
			}

			psDevInfo->s2DSlavePortKM.pvData = NULL;
			psDevInfo->s2DSlavePortKM.ui32DataRange = 0;
			psDevInfo->s2DSlavePortKM.sPhysBase.uiAddr = 0;
#endif

		}

		psDeviceNodeList = psDeviceNodeList->psNext;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (eNewPowerState != gpsSysData->eCurrentPowerState) {
		if ((eNewPowerState == PVRSRV_SYS_POWER_STATE_D3) &&
		    (gpsSysData->eCurrentPowerState <
		     PVRSRV_SYS_POWER_STATE_D3)) {

#if defined(SYS_USING_INTERRUPTS)
			if (SYS_SPECIFIC_DATA_TEST
			    (&gsSysSpecificData,
			     SYS_SPECIFIC_DATA_ENABLE_IRQ)) {
				SysDisableInterrupts(gpsSysData);

				SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
						      SYS_SPECIFIC_DATA_PM_IRQ_DISABLE);
				SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData,
							SYS_SPECIFIC_DATA_ENABLE_IRQ);

			}

			if (SYS_SPECIFIC_DATA_TEST
			    (&gsSysSpecificData,
			     SYS_SPECIFIC_DATA_ENABLE_LISR)) {
				eError = OSUninstallSystemLISR(gpsSysData);
				if (eError != PVRSRV_OK) {
					PVR_DPF((PVR_DBG_ERROR,
						 "SysSystemPrePowerState: OSUninstallSystemLISR failed (%d)",
						 eError));
				}
				SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
						      SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR);
				SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData,
							SYS_SPECIFIC_DATA_ENABLE_LISR);
			}
#endif

			SysUnmapRegisters();
		}
	}

	return eError;
}

PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (eNewPowerState != gpsSysData->eCurrentPowerState) {
		if ((gpsSysData->eCurrentPowerState ==
		     PVRSRV_SYS_POWER_STATE_D3)
		    && (eNewPowerState < PVRSRV_SYS_POWER_STATE_D3)) {

			eError = SysLocateDevices(gpsSysData);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "SysSystemPostPowerState: Failed to locate devices"));
				return eError;
			}

			eError = SysMapInRegisters();
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "SysSystemPostPowerState: Failed to map in registers"));
				return eError;
			}

			eError = SysInitRegisters();
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "SysSystemPostPowerState: Failed to Initialise registers"));
				return eError;
			}
#if defined(SYS_USING_INTERRUPTS)
			if (SYS_SPECIFIC_DATA_TEST
			    (&gsSysSpecificData,
			     SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR)) {
				eError =
				    OSInstallSystemLISR(gpsSysData,
							gsSGXDeviceMap.ui32IRQ);
				if (eError != PVRSRV_OK) {
					PVR_DPF((PVR_DBG_ERROR,
						 "SysSystemPostPowerState: OSInstallSystemLISR failed to install ISR (%d)",
						 eError));
				}
				SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
						      SYS_SPECIFIC_DATA_ENABLE_LISR);
				SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData,
							SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR);
			}

			if (SYS_SPECIFIC_DATA_TEST
			    (&gsSysSpecificData,
			     SYS_SPECIFIC_DATA_PM_IRQ_DISABLE)) {
				SysEnableInterrupts(gpsSysData);

				SYS_SPECIFIC_DATA_SET(&gsSysSpecificData,
						      SYS_SPECIFIC_DATA_ENABLE_IRQ);
				SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData,
							SYS_SPECIFIC_DATA_PM_IRQ_DISABLE);
			}
#endif
		}
	}

	return eError;
}

PVRSRV_ERROR SysDevicePrePowerState(u32 ui32DeviceIndex,
				    PVRSRV_DEV_POWER_STATE eNewPowerState,
				    PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	if (ui32DeviceIndex == gui32SGXDeviceID) {
		if ((eNewPowerState != eCurrentPowerState) &&
		    (eNewPowerState == PVRSRV_DEV_POWER_STATE_IDLE)) {
#ifdef INTEL_D3_PM
			graphics_pm_set_idle();
#endif
			PVR_DPF((PVR_DBG_MESSAGE,
				 "SysDevicePrePowerState: Remove SGX power"));
		}
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDevicePostPowerState(u32 ui32DeviceIndex,
				     PVRSRV_DEV_POWER_STATE eNewPowerState,
				     PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	if (ui32DeviceIndex == gui32SGXDeviceID) {
		if ((eNewPowerState != eCurrentPowerState) &&
		    (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_IDLE)) {
#ifdef INTEL_D3_PM
			graphics_pm_set_busy();
#endif
			PVR_DPF((PVR_DBG_MESSAGE,
				 "SysDevicePostPowerState: Restore SGX power"));
		}
	}

	return PVRSRV_OK;
}

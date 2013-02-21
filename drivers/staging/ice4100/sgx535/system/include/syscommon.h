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

#ifndef _SYSCOMMON_H
#define _SYSCOMMON_H

#include "sysconfig.h"
#include "sysinfo.h"
#include "servicesint.h"
#include "queue.h"
#include "power.h"
#include "resman.h"
#include "ra.h"
#include "device.h"
#include "buffer_manager.h"

#if defined(NO_HARDWARE) && defined(__linux__) && defined(__KERNEL__)
#include <asm/io.h>
#endif

typedef struct _SYS_DEVICE_ID_TAG
{
	u32	uiID;
	int	bInUse;

} SYS_DEVICE_ID;


#define SYS_MAX_LOCAL_DEVMEM_ARENAS	4

typedef struct _SYS_DATA_TAG_
{
    u32                  ui32NumDevices;
	SYS_DEVICE_ID				sDeviceID[SYS_DEVICE_COUNT];
    PVRSRV_DEVICE_NODE			*psDeviceNodeList;
    PVRSRV_POWER_DEV			*psPowerDeviceList;
	PVRSRV_RESOURCE				sPowerStateChangeResource;
   	PVRSRV_SYS_POWER_STATE		eCurrentPowerState;
   	PVRSRV_SYS_POWER_STATE		eFailedPowerState;
   	u32		 			ui32CurrentOSPowerState;
    PVRSRV_QUEUE_INFO           *psQueueList;
   	PVRSRV_KERNEL_SYNC_INFO 	*psSharedSyncInfoList;
    void *                   pvEnvSpecificData;
    void *                   pvSysSpecificData;
	PVRSRV_RESOURCE				sQProcessResource;
	void					*pvSOCRegsBase;
    void *                  hSOCTimerRegisterOSMemHandle;
	u32					*pvSOCTimerRegisterKM;
	void					*pvSOCClockGateRegsBase;
	u32					ui32SOCClockGateRegsSize;
	PFN_CMD_PROC				*ppfnCmdProcList[SYS_DEVICE_COUNT];



	PCOMMAND_COMPLETE_DATA		*ppsCmdCompleteData[SYS_DEVICE_COUNT];


	int                    bReProcessQueues;

	RA_ARENA					*apsLocalDevMemArena[SYS_MAX_LOCAL_DEVMEM_ARENAS];

    char                    *pszVersionString;
	PVRSRV_EVENTOBJECT			*psGlobalEventObject;

	int					bFlushAll;

} SYS_DATA;



PVRSRV_ERROR SysInitialise(void);
PVRSRV_ERROR SysFinalise(void);

PVRSRV_ERROR SysDeinitialise(SYS_DATA *psSysData);
PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE eDeviceType,
									void **ppvDeviceMap);

void SysRegisterExternalDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
void SysRemoveExternalDevice(PVRSRV_DEVICE_NODE *psDeviceNode);

u32 SysGetInterruptSource(SYS_DATA			*psSysData,
								 PVRSRV_DEVICE_NODE *psDeviceNode);

void SysClearInterrupts(SYS_DATA* psSysData, u32 ui32ClearBits);

PVRSRV_ERROR SysResetDevice(u32 ui32DeviceIndex);

PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);
PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);
PVRSRV_ERROR SysDevicePrePowerState(u32 ui32DeviceIndex,
									PVRSRV_DEV_POWER_STATE eNewPowerState,
									PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SysDevicePostPowerState(u32 ui32DeviceIndex,
									 PVRSRV_DEV_POWER_STATE eNewPowerState,
									 PVRSRV_DEV_POWER_STATE eCurrentPowerState);

#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
PVRSRV_ERROR SysPowerLockWrap(SYS_DATA *psSysData);
void SysPowerLockUnwrap(SYS_DATA *psSysData);
#endif

PVRSRV_ERROR SysOEMFunction (	u32	ui32ID,
								void	*pvIn,
								u32  ulInSize,
								void	*pvOut,
								u32	ulOutSize);


IMG_DEV_PHYADDR SysCpuPAddrToDevPAddr (PVRSRV_DEVICE_TYPE eDeviceType, IMG_CPU_PHYADDR cpu_paddr);
IMG_DEV_PHYADDR SysSysPAddrToDevPAddr (PVRSRV_DEVICE_TYPE eDeviceType, IMG_SYS_PHYADDR SysPAddr);
IMG_SYS_PHYADDR SysDevPAddrToSysPAddr (PVRSRV_DEVICE_TYPE eDeviceType, IMG_DEV_PHYADDR SysPAddr);
IMG_CPU_PHYADDR SysSysPAddrToCpuPAddr (IMG_SYS_PHYADDR SysPAddr);
IMG_SYS_PHYADDR SysCpuPAddrToSysPAddr (IMG_CPU_PHYADDR cpu_paddr);
#if defined(PVR_LMA)
int SysVerifyCpuPAddrToDevPAddr (PVRSRV_DEVICE_TYPE eDeviceType, IMG_CPU_PHYADDR CpuPAddr);
int SysVerifySysPAddrToDevPAddr (PVRSRV_DEVICE_TYPE eDeviceType, IMG_SYS_PHYADDR SysPAddr);
#endif

extern SYS_DATA* gpsSysData;

#if !defined(USE_CODE)

static inline PVRSRV_ERROR SysAcquireData(SYS_DATA **ppsSysData)
{

	*ppsSysData = gpsSysData;





	if (!gpsSysData)
	{
		return PVRSRV_ERROR_GENERIC;
   	}

	return PVRSRV_OK;
}


static inline PVRSRV_ERROR SysInitialiseCommon(SYS_DATA *psSysData)
{
	PVRSRV_ERROR	eError;


	eError = PVRSRVInit(psSysData);

	return eError;
}

static inline void SysDeinitialiseCommon(SYS_DATA *psSysData)
{

	PVRSRVDeInit(psSysData);

	OSDestroyResource(&psSysData->sPowerStateChangeResource);
}
#endif


#if !(defined(NO_HARDWARE) && defined(__linux__) && defined(__KERNEL__))
#define	SysReadHWReg(p, o) OSReadHWReg(p, o)
#define SysWriteHWReg(p, o, v) OSWriteHWReg(p, o, v)
#else
static inline u32 SysReadHWReg(void * pvLinRegBaseAddr, u32 ui32Offset)
{
	return (u32) readl(pvLinRegBaseAddr + ui32Offset);
}

static inline void SysWriteHWReg(void * pvLinRegBaseAddr, u32 ui32Offset, u32 ui32Value)
{
	writel(ui32Value, pvLinRegBaseAddr + ui32Offset);
}
#endif


#endif


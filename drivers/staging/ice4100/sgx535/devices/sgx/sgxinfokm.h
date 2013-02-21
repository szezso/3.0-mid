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

#ifndef __SGXINFOKM_H__
#define __SGXINFOKM_H__

#include "sgxdefs.h"
#include "device.h"
#include "power.h"
#include "sysconfig.h"
#include "sgxscript.h"
#include "sgxinfo.h"


#define		SGX_HOSTPORT_PRESENT			0x00000001UL


typedef struct _PVRSRV_STUB_PBDESC_ PVRSRV_STUB_PBDESC;


typedef struct _PVRSRV_SGX_CCB_INFO_ *PPVRSRV_SGX_CCB_INFO;

typedef struct _PVRSRV_SGXDEV_INFO_
{
	PVRSRV_DEVICE_TYPE		eDeviceType;
	PVRSRV_DEVICE_CLASS		eDeviceClass;

	u8				ui8VersionMajor;
	u8				ui8VersionMinor;
	u32				ui32CoreConfig;
	u32				ui32CoreFlags;


	void *				pvRegsBaseKM;

#if defined(SGX_FEATURE_HOST_PORT)

	void *				pvHostPortBaseKM;

	u32				ui32HPSize;

	IMG_SYS_PHYADDR			sHPSysPAddr;
#endif


	void *				hRegMapping;


	IMG_SYS_PHYADDR			sRegsPhysBase;

	u32				ui32RegSize;

#if defined(SUPPORT_EXTERNAL_SYSTEM_CACHE)

	u32				ui32ExtSysCacheRegsSize;

	IMG_DEV_PHYADDR			sExtSysCacheRegsDevPBase;

	u32				*pui32ExtSystemCacheRegsPT;

	void *				hExtSystemCacheRegsPTPageOSMemHandle;

	IMG_SYS_PHYADDR			sExtSystemCacheRegsPTSysPAddr;
#endif


	u32				ui32CoreClockSpeed;
	u32				ui32uKernelTimerClock;

	PVRSRV_STUB_PBDESC		*psStubPBDescListKM;



	IMG_DEV_PHYADDR			sKernelPDDevPAddr;

	void				*pvDeviceMemoryHeap;
	PPVRSRV_KERNEL_MEM_INFO	psKernelCCBMemInfo;
	PVRSRV_SGX_KERNEL_CCB	*psKernelCCB;
	PPVRSRV_SGX_CCB_INFO	psKernelCCBInfo;
	PPVRSRV_KERNEL_MEM_INFO	psKernelCCBCtlMemInfo;
	PVRSRV_SGX_CCB_CTL		*psKernelCCBCtl;
	PPVRSRV_KERNEL_MEM_INFO psKernelCCBEventKickerMemInfo;
	u32				*pui32KernelCCBEventKicker;
#if defined(PDUMP)
	u32				ui32KernelCCBEventKickerDumpVal;
#endif
 	PVRSRV_KERNEL_MEM_INFO	*psKernelSGXMiscMemInfo;
	u32				aui32HostKickAddr[SGXMKIF_CMD_MAX];
#if defined(SGX_SUPPORT_HWPROFILING)
	PPVRSRV_KERNEL_MEM_INFO psKernelHWProfilingMemInfo;
#endif
	u32				ui32KickTACounter;
	u32				ui32KickTARenderCounter;
#if defined(SUPPORT_SGX_HWPERF)
	PPVRSRV_KERNEL_MEM_INFO		psKernelHWPerfCBMemInfo;
	u32					ui32HWGroupRequested;
	u32					ui32HWReset;
#endif
#ifdef PVRSRV_USSE_EDM_STATUS_DEBUG
	PPVRSRV_KERNEL_MEM_INFO	psKernelEDMStatusBufferMemInfo;
#endif
#if defined(SGX_FEATURE_OVERLAPPED_SPM)
	PPVRSRV_KERNEL_MEM_INFO	psKernelTmpRgnHeaderMemInfo;
#endif
#if defined(SGX_FEATURE_SPM_MODE_0)
	PPVRSRV_KERNEL_MEM_INFO	psKernelTmpDPMStateMemInfo;
#endif


	u32				ui32ClientRefCount;


	u32				ui32CacheControl;


	u32				ui32ClientBuildOptions;


	SGX_MISCINFO_STRUCT_SIZES	sSGXStructSizes;




	void				*pvMMUContextList;


	int				bForcePTOff;

	u32				ui32EDMTaskReg0;
	u32				ui32EDMTaskReg1;

	u32				ui32ClkGateStatusReg;
	u32				ui32ClkGateStatusMask;
#if defined(SGX_FEATURE_MP)
	u32				ui32MasterClkGateStatusReg;
	u32				ui32MasterClkGateStatusMask;
#endif
	SGX_INIT_SCRIPTS		sScripts;


	void * 				hBIFResetPDOSMemHandle;
	IMG_DEV_PHYADDR 		sBIFResetPDDevPAddr;
	IMG_DEV_PHYADDR 		sBIFResetPTDevPAddr;
	IMG_DEV_PHYADDR 		sBIFResetPageDevPAddr;
	u32				*pui32BIFResetPD;
	u32				*pui32BIFResetPT;

#if defined(FIX_HW_BRN_22997) && defined(FIX_HW_BRN_23030) && defined(SGX_FEATURE_HOST_PORT)

	void *				hBRN22997PTPageOSMemHandle;
	void *				hBRN22997PDPageOSMemHandle;
	IMG_DEV_PHYADDR 		sBRN22997PTDevPAddr;
	IMG_DEV_PHYADDR 		sBRN22997PDDevPAddr;
	u32				*pui32BRN22997PT;
	u32				*pui32BRN22997PD;
	IMG_SYS_PHYADDR 		sBRN22997SysPAddr;
#endif

#if defined(SUPPORT_HW_RECOVERY)

	void *				hTimer;

	u32				ui32TimeStamp;
#endif


	u32				ui32NumResets;


	PVRSRV_KERNEL_MEM_INFO			*psKernelSGXHostCtlMemInfo;
	SGXMKIF_HOST_CTL				*psSGXHostCtl;


	PVRSRV_KERNEL_MEM_INFO			*psKernelSGXTA3DCtlMemInfo;

	u32				ui32Flags;

	#if defined(PDUMP)
	PVRSRV_SGX_PDUMP_CONTEXT	sPDContext;
	#endif

#if defined(SUPPORT_SGX_MMU_DUMMY_PAGE)

	void				*pvDummyPTPageCpuVAddr;
	IMG_DEV_PHYADDR			sDummyPTDevPAddr;
	void *				hDummyPTPageOSMemHandle;
	void				*pvDummyDataPageCpuVAddr;
	IMG_DEV_PHYADDR 		sDummyDataDevPAddr;
	void *				hDummyDataPageOSMemHandle;
#endif

	u32				asSGXDevData[SGX_MAX_DEV_DATA];

} PVRSRV_SGXDEV_INFO;


typedef struct _SGX_TIMING_INFORMATION_
{
	u32			ui32CoreClockSpeed;
	u32			ui32HWRecoveryFreq;
	int			bEnableActivePM;
	u32			ui32ActivePowManLatencyms;
	u32			ui32uKernelFreq;
} SGX_TIMING_INFORMATION;

typedef struct _SGX_DEVICE_MAP_
{
	u32				ui32Flags;


	IMG_SYS_PHYADDR			sRegsSysPBase;
	IMG_CPU_PHYADDR			sRegsCpuPBase;
	IMG_CPU_VIRTADDR		pvRegsCpuVBase;
	u32				ui32RegsSize;

#if defined(SGX_FEATURE_HOST_PORT)
	IMG_SYS_PHYADDR			sHPSysPBase;
	IMG_CPU_PHYADDR			sHPCpuPBase;
	u32				ui32HPSize;
#endif


	IMG_SYS_PHYADDR			sLocalMemSysPBase;
	IMG_DEV_PHYADDR			sLocalMemDevPBase;
	IMG_CPU_PHYADDR			sLocalMemCpuPBase;
	u32				ui32LocalMemSize;

#if defined(SUPPORT_EXTERNAL_SYSTEM_CACHE)
	u32				ui32ExtSysCacheRegsSize;
	IMG_DEV_PHYADDR			sExtSysCacheRegsDevPBase;
#endif


	u32				ui32IRQ;

#if !defined(SGX_DYNAMIC_TIMING_INFO)

	SGX_TIMING_INFORMATION	sTimingInfo;
#endif
} SGX_DEVICE_MAP;


struct _PVRSRV_STUB_PBDESC_
{
	u32		ui32RefCount;
	u32		ui32TotalPBSize;
	PVRSRV_KERNEL_MEM_INFO  *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO  *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO	**ppsSubKernelMemInfos;
	u32		ui32SubKernelMemInfosCount;
	void *		hDevCookie;
	PVRSRV_KERNEL_MEM_INFO  *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO  *psHWBlockKernelMemInfo;
	PVRSRV_STUB_PBDESC	*psNext;
	PVRSRV_STUB_PBDESC	**ppsThis;
};

typedef struct _PVRSRV_SGX_CCB_INFO_
{
	PVRSRV_KERNEL_MEM_INFO	*psCCBMemInfo;
	PVRSRV_KERNEL_MEM_INFO	*psCCBCtlMemInfo;
	SGXMKIF_COMMAND		*psCommands;
	u32				*pui32WriteOffset;
	volatile u32		*pui32ReadOffset;
#if defined(PDUMP)
	u32				ui32CCBDumpWOff;
#endif
} PVRSRV_SGX_CCB_INFO;

PVRSRV_ERROR SGXRegisterDevice (PVRSRV_DEVICE_NODE *psDeviceNode);

void SGXOSTimer(void *pvData);

void SGXReset(PVRSRV_SGXDEV_INFO	*psDevInfo,
				  u32			 ui32PDUMPFlags);

PVRSRV_ERROR SGXInitialise(PVRSRV_SGXDEV_INFO	*psDevInfo);
PVRSRV_ERROR SGXDeinitialise(void * hDevCookie);

PVRSRV_ERROR SGXPrePowerState(void *				hDevHandle,
							  PVRSRV_DEV_POWER_STATE	eNewPowerState,
							  PVRSRV_DEV_POWER_STATE	eCurrentPowerState);

PVRSRV_ERROR SGXPostPowerState(void *				hDevHandle,
							   PVRSRV_DEV_POWER_STATE	eNewPowerState,
							   PVRSRV_DEV_POWER_STATE	eCurrentPowerState);

PVRSRV_ERROR SGXPreClockSpeedChange(void *				hDevHandle,
									int				bIdleDevice,
									PVRSRV_DEV_POWER_STATE	eCurrentPowerState);

PVRSRV_ERROR SGXPostClockSpeedChange(void *				hDevHandle,
									 int				bIdleDevice,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState);

void SGXPanic(PVRSRV_DEVICE_NODE	*psDeviceNode);

PVRSRV_ERROR SGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode);

#if defined(SGX_DYNAMIC_TIMING_INFO)
void SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psSGXTimingInfo);
#endif

#if defined(NO_HARDWARE)
static void NoHardwareGenerateEvent(PVRSRV_SGXDEV_INFO		*psDevInfo,
												u32 ui32StatusRegister,
												u32 ui32StatusValue,
												u32 ui32StatusMask)
{
	u32 ui32RegVal;

	ui32RegVal = OSReadHWReg(psDevInfo->pvRegsBaseKM, ui32StatusRegister);

	ui32RegVal &= ~ui32StatusMask;
	ui32RegVal |= (ui32StatusValue & ui32StatusMask);

	OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32StatusRegister, ui32RegVal);
}
#endif


#endif


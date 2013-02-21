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

#if !defined (__SGXINFO_H__)
#define __SGXINFO_H__

#include "sgxscript.h"
#include "servicesint.h"
#include "services.h"
#include "sgxapi_km.h"
#include "sgx_mkif_km.h"


#define SGX_MAX_DEV_DATA			24
#define	SGX_MAX_INIT_MEM_HANDLES	16


typedef struct _SGX_BRIDGE_INFO_FOR_SRVINIT
{
	IMG_DEV_PHYADDR sPDDevPAddr;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
} SGX_BRIDGE_INFO_FOR_SRVINIT;


typedef enum _SGXMKIF_CMD_TYPE_
{
	SGXMKIF_CMD_TA				= 0,
	SGXMKIF_CMD_TRANSFER		= 1,
	SGXMKIF_CMD_2D				= 2,
	SGXMKIF_CMD_POWER			= 3,
	SGXMKIF_CMD_CLEANUP			= 4,
	SGXMKIF_CMD_GETMISCINFO		= 5,
	SGXMKIF_CMD_PROCESS_QUEUES	= 6,
	SGXMKIF_CMD_MAX				= 7,

	SGXMKIF_CMD_FORCE_I32   	= -1,

} SGXMKIF_CMD_TYPE;


typedef struct _SGX_BRIDGE_INIT_INFO_
{
	void *	hKernelCCBMemInfo;
	void *	hKernelCCBCtlMemInfo;
	void *	hKernelCCBEventKickerMemInfo;
	void *	hKernelSGXHostCtlMemInfo;
	void *	hKernelSGXTA3DCtlMemInfo;
	void *	hKernelSGXMiscMemInfo;

	u32	aui32HostKickAddr[SGXMKIF_CMD_MAX];

	SGX_INIT_SCRIPTS sScripts;

	u32	ui32ClientBuildOptions;
	SGX_MISCINFO_STRUCT_SIZES	sSGXStructSizes;

#if defined(SGX_SUPPORT_HWPROFILING)
	void *	hKernelHWProfilingMemInfo;
#endif
#if defined(SUPPORT_SGX_HWPERF)
	void *	hKernelHWPerfCBMemInfo;
#endif
#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	void *	hKernelEDMStatusBufferMemInfo;
#endif
#if defined(SGX_FEATURE_OVERLAPPED_SPM)
	void * hKernelTmpRgnHeaderMemInfo;
#endif
#if defined(SGX_FEATURE_SPM_MODE_0)
	void * hKernelTmpDPMStateMemInfo;
#endif

	u32 ui32EDMTaskReg0;
	u32 ui32EDMTaskReg1;

	u32 ui32ClkGateStatusReg;
	u32 ui32ClkGateStatusMask;
#if defined(SGX_FEATURE_MP)
	u32 ui32MasterClkGateStatusReg;
	u32 ui32MasterClkGateStatusMask;
#endif

	u32 ui32CacheControl;

	u32	asInitDevData[SGX_MAX_DEV_DATA];
	void *	asInitMemHandles[SGX_MAX_INIT_MEM_HANDLES];

} SGX_BRIDGE_INIT_INFO;


typedef struct _SGX_DEVICE_SYNC_LIST_
{
	PSGXMKIF_HWDEVICE_SYNC_LIST	psHWDeviceSyncList;

	void *				hKernelHWSyncListMemInfo;
	PVRSRV_CLIENT_MEM_INFO	*psHWDeviceSyncListClientMemInfo;
	PVRSRV_CLIENT_MEM_INFO	*psAccessResourceClientMemInfo;

	volatile u32		*pui32Lock;

	struct _SGX_DEVICE_SYNC_LIST_	*psNext;


	u32			ui32NumSyncObjects;
	void *			ahSyncHandles[1];
} SGX_DEVICE_SYNC_LIST, *PSGX_DEVICE_SYNC_LIST;


typedef struct _SGX_INTERNEL_STATUS_UPDATE_
{
	CTL_STATUS				sCtlStatus;
	void *				hKernelMemInfo;

	u32				ui32LastStatusUpdateDumpVal;
} SGX_INTERNEL_STATUS_UPDATE;


typedef struct _SGX_CCB_KICK_
{
	SGXMKIF_COMMAND		sCommand;
	void *			hCCBKernelMemInfo;

	u32	ui32NumDstSyncObjects;
	void *	hKernelHWSyncListMemInfo;


	void *	*pahDstSyncHandles;

	u32	ui32NumTAStatusVals;
	u32	ui32Num3DStatusVals;

#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
	SGX_INTERNEL_STATUS_UPDATE	asTAStatusUpdate[SGX_MAX_TA_STATUS_VALS];
	SGX_INTERNEL_STATUS_UPDATE	as3DStatusUpdate[SGX_MAX_3D_STATUS_VALS];
#else
	void *	ahTAStatusSyncInfo[SGX_MAX_TA_STATUS_VALS];
	void *	ah3DStatusSyncInfo[SGX_MAX_3D_STATUS_VALS];
#endif

	int	bFirstKickOrResume;
#if (defined(NO_HARDWARE) || defined(PDUMP))
	int	bTerminateOrAbort;
#endif
#if defined(SUPPORT_SGX_HWPERF)
	int			bKickRender;
#endif


	u32	ui32CCBOffset;

#if defined(SUPPORT_SGX_GENERALISED_SYNCOBJECTS)

	u32	ui32NumTASrcSyncs;
	void *	ahTASrcKernelSyncInfo[SGX_MAX_TA_SRC_SYNCS];
	u32	ui32NumTADstSyncs;
	void *	ahTADstKernelSyncInfo[SGX_MAX_TA_DST_SYNCS];
	u32	ui32Num3DSrcSyncs;
	void *	ah3DSrcKernelSyncInfo[SGX_MAX_3D_SRC_SYNCS];
#else

	u32	ui32NumSrcSyncs;
	void *	ahSrcKernelSyncInfo[SGX_MAX_SRC_SYNCS];
#endif


	int	bTADependency;
	void *	hTA3DSyncInfo;

	void *	hTASyncInfo;
	void *	h3DSyncInfo;
#if defined(PDUMP)
	u32	ui32CCBDumpWOff;
#endif
#if defined(NO_HARDWARE)
	u32	ui32WriteOpsPendingVal;
#endif
} SGX_CCB_KICK;


#define SGX_KERNEL_USE_CODE_BASE_INDEX		15


typedef struct _SGX_CLIENT_INFO_
{
	u32					ui32ProcessID;
	void					*pvProcess;
	PVRSRV_MISC_INFO			sMiscInfo;

	u32					asDevData[SGX_MAX_DEV_DATA];

} SGX_CLIENT_INFO;

typedef struct _SGX_INTERNAL_DEVINFO_
{
	u32			ui32Flags;
	void *			hHostCtlKernelMemInfoHandle;
	int			bForcePTOff;
} SGX_INTERNAL_DEVINFO;


#if defined(TRANSFER_QUEUE)
typedef struct _PVRSRV_TRANSFER_SGX_KICK_
{
	void *		hCCBMemInfo;
	u32		ui32SharedCmdCCBOffset;

	IMG_DEV_VIRTADDR 	sHWTransferContextDevVAddr;

	void *		hTASyncInfo;
	void *		h3DSyncInfo;

	u32		ui32NumSrcSync;
	void *		ahSrcSyncInfo[SGX_MAX_TRANSFER_SYNC_OPS];

	u32		ui32NumDstSync;
	void *		ahDstSyncInfo[SGX_MAX_TRANSFER_SYNC_OPS];

	u32		ui32Flags;

	u32		ui32PDumpFlags;
#if defined(PDUMP)
	u32		ui32CCBDumpWOff;
#endif
} PVRSRV_TRANSFER_SGX_KICK, *PPVRSRV_TRANSFER_SGX_KICK;

#if defined(SGX_FEATURE_2D_HARDWARE)
typedef struct _PVRSRV_2D_SGX_KICK_
{
	void *		hCCBMemInfo;
	u32		ui32SharedCmdCCBOffset;

	IMG_DEV_VIRTADDR 	sHW2DContextDevVAddr;

	u32		ui32NumSrcSync;
	void *		ahSrcSyncInfo[SGX_MAX_2D_SRC_SYNC_OPS];


	void * 		hDstSyncInfo;


	void *		hTASyncInfo;


	void *		h3DSyncInfo;

	u32		ui32PDumpFlags;
#if defined(PDUMP)
	u32		ui32CCBDumpWOff;
#endif
} PVRSRV_2D_SGX_KICK, *PPVRSRV_2D_SGX_KICK;
#endif
#endif

#define PVRSRV_SGX_DIFF_NUM_COUNTERS	9

typedef struct _PVRSRV_SGXDEV_DIFF_INFO_
{
	u32	aui32Counters[PVRSRV_SGX_DIFF_NUM_COUNTERS];
	u32	ui32Time[3];
	u32	ui32Marker[2];
} PVRSRV_SGXDEV_DIFF_INFO, *PPVRSRV_SGXDEV_DIFF_INFO;



#endif

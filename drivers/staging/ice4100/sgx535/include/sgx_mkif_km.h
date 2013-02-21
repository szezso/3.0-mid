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

#if !defined (__SGX_MKIF_KM_H__)
#define __SGX_MKIF_KM_H__

#include "img_types.h"
#include "servicesint.h"
#include "sgxapi_km.h"


#if defined(SGX_FEATURE_MP)
	#define SGX_REG_BANK_SHIFT 			(12)
	#define SGX_REG_BANK_SIZE 			(0x4000)
	#if defined(SGX541)
		#define SGX_REG_BANK_BASE_INDEX		(1)
		#define	SGX_REG_BANK_MASTER_INDEX	(SGX_REG_BANK_BASE_INDEX + SGX_FEATURE_MP_CORE_COUNT)
	#else
		#define SGX_REG_BANK_BASE_INDEX		(2)
		#define	SGX_REG_BANK_MASTER_INDEX	(1)
	#endif
	#define SGX_MP_CORE_SELECT(x,i) 	(x + ((i + SGX_REG_BANK_BASE_INDEX) * SGX_REG_BANK_SIZE))
	#define SGX_MP_MASTER_SELECT(x) 	(x + (SGX_REG_BANK_MASTER_INDEX * SGX_REG_BANK_SIZE))
#else
	#define SGX_MP_CORE_SELECT(x,i) 	(x)
#endif


typedef struct _SGXMKIF_COMMAND_
{
	u32				ui32ServiceAddress;
	u32				ui32CacheControl;
	u32				ui32Data[2];
} SGXMKIF_COMMAND;


typedef struct _PVRSRV_SGX_KERNEL_CCB_
{
	SGXMKIF_COMMAND		asCommands[256];
} PVRSRV_SGX_KERNEL_CCB;


typedef struct _PVRSRV_SGX_CCB_CTL_
{
	u32				ui32WriteOffset;
#ifdef INTEL_D3_PAD
	u32				_reserved[15];
#endif
	u32				ui32ReadOffset;
} PVRSRV_SGX_CCB_CTL;


typedef struct _SGXMKIF_HOST_CTL_
{
#ifndef INTEL_D3_PAD
#if defined(PVRSRV_USSE_EDM_BREAKPOINTS)
	u32				ui32BreakpointDisable;
	u32				ui32Continue;
#endif

	volatile u32		ui32InitStatus;
	volatile u32		ui32PowerStatus;
	volatile u32		ui32CleanupStatus;
#if defined(SUPPORT_HW_RECOVERY)
	u32				ui32uKernelDetectedLockups;
	u32				ui32HostDetectedLockups;
	u32				ui32HWRecoverySampleRate;
#endif
	u32				ui32uKernelTimerClock;
	u32				ui32ActivePowManSampleRate;
	u32				ui32InterruptFlags;
	u32				ui32InterruptClearFlags;


	u32				ui32NumActivePowerEvents;

#if defined(SUPPORT_SGX_HWPERF)
	u32			ui32HWPerfFlags;
#endif


	u32			ui32TimeWraps;
#else
	// INTEL_D3_PAD defined

	// SGX only write
#if defined(PVRSRV_USSE_EDM_BREAKPOINTS)
	u32				ui32BreakpointDisable;
	u32				ui32Continue;
#else
    u32              _reserved1[2];
#endif
#if defined(SUPPORT_HW_RECOVERY)
	u32				ui32uKernelDetectedLockups;
#else
    u32              _reserved2;
#endif
	u32              ui32InterruptFlags;
	u32              ui32TimeWraps;

    u32              _reserved3[11];

	// CPU only write
#if defined(SUPPORT_HW_RECOVERY)
	u32				ui32HostDetectedLockups;
	u32				ui32HWRecoverySampleRate;
#else
    u32              _reserved4[2];
#endif
	u32				ui32uKernelTimerClock;
	u32				ui32ActivePowManSampleRate;
	u32				ui32NumActivePowerEvents;
#if defined(SUPPORT_SGX_HWPERF)
	u32              ui32HWPerfFlags;
#else
	u32              _reserved5;
#endif

    u32              _reserved6[10];

	// Both write
	volatile u32		ui32InitStatus;
    u32              _reserved7[15];
	volatile u32		ui32PowerStatus;
    u32              _reserved8[15];
	volatile u32		ui32CleanupStatus;
    u32              _reserved9[15];
	volatile u32		ui32InterruptClearFlags;
    u32              _reserved10[15];

#endif
} SGXMKIF_HOST_CTL;

#define	SGXMKIF_CMDTA_CTRLFLAGS_READY			0x00000001
typedef struct _SGXMKIF_CMDTA_SHARED_
{
	u32			ui32CtrlFlags;

	u32			ui32NumTAStatusVals;
	u32			ui32Num3DStatusVals;


	u32			ui32TATQSyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR	sTATQSyncWriteOpsCompleteDevVAddr;
	u32			ui32TATQSyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR	sTATQSyncReadOpsCompleteDevVAddr;


	u32			ui323DTQSyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR	s3DTQSyncWriteOpsCompleteDevVAddr;
	u32			ui323DTQSyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR	s3DTQSyncReadOpsCompleteDevVAddr;


#if defined(SUPPORT_SGX_GENERALISED_SYNCOBJECTS)

	u32					ui32NumTASrcSyncs;
	PVRSRV_DEVICE_SYNC_OBJECT	asTASrcSyncs[SGX_MAX_TA_SRC_SYNCS];
	u32					ui32NumTADstSyncs;
	PVRSRV_DEVICE_SYNC_OBJECT	asTADstSyncs[SGX_MAX_TA_DST_SYNCS];
	u32					ui32Num3DSrcSyncs;
	PVRSRV_DEVICE_SYNC_OBJECT	as3DSrcSyncs[SGX_MAX_3D_SRC_SYNCS];
#else

	u32			ui32NumSrcSyncs;
	PVRSRV_DEVICE_SYNC_OBJECT	asSrcSyncs[SGX_MAX_SRC_SYNCS];
#endif


	PVRSRV_DEVICE_SYNC_OBJECT	sTA3DDependency;

	CTL_STATUS			sCtlTAStatusInfo[SGX_MAX_TA_STATUS_VALS];
	CTL_STATUS			sCtl3DStatusInfo[SGX_MAX_3D_STATUS_VALS];

} SGXMKIF_CMDTA_SHARED;

#define SGXTQ_MAX_STATUS						SGX_MAX_TRANSFER_STATUS_VALS + 2

#define SGXMKIF_TQFLAGS_NOSYNCUPDATE			0x00000001
#define SGXMKIF_TQFLAGS_KEEPPENDING				0x00000002
#define SGXMKIF_TQFLAGS_TATQ_SYNC				0x00000004
#define SGXMKIF_TQFLAGS_3DTQ_SYNC				0x00000008
#if defined(SGX_FEATURE_FAST_RENDER_CONTEXT_SWITCH)
#define SGXMKIF_TQFLAGS_CTXSWITCH				0x00000010
#endif
#define SGXMKIF_TQFLAGS_DUMMYTRANSFER			0x00000020

typedef struct _SGXMKIF_TRANSFERCMD_SHARED_
{


	u32		ui32SrcReadOpPendingVal;
	IMG_DEV_VIRTADDR	sSrcReadOpsCompleteDevAddr;

	u32		ui32SrcWriteOpPendingVal;
	IMG_DEV_VIRTADDR	sSrcWriteOpsCompleteDevAddr;



	u32		ui32DstReadOpPendingVal;
	IMG_DEV_VIRTADDR	sDstReadOpsCompleteDevAddr;

	u32		ui32DstWriteOpPendingVal;
	IMG_DEV_VIRTADDR	sDstWriteOpsCompleteDevAddr;


	u32		ui32TASyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR	sTASyncWriteOpsCompleteDevVAddr;
	u32		ui32TASyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR	sTASyncReadOpsCompleteDevVAddr;


	u32		ui323DSyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR	s3DSyncWriteOpsCompleteDevVAddr;
	u32		ui323DSyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR	s3DSyncReadOpsCompleteDevVAddr;

	u32 		ui32NumStatusVals;
	CTL_STATUS  	sCtlStatusInfo[SGXTQ_MAX_STATUS];
} SGXMKIF_TRANSFERCMD_SHARED, *PSGXMKIF_TRANSFERCMD_SHARED;


#if defined(SGX_FEATURE_2D_HARDWARE)
typedef struct _SGXMKIF_2DCMD_SHARED_ {

	u32			ui32NumSrcSync;
	PVRSRV_DEVICE_SYNC_OBJECT	sSrcSyncData[SGX_MAX_2D_SRC_SYNC_OPS];


	PVRSRV_DEVICE_SYNC_OBJECT	sDstSyncData;


	PVRSRV_DEVICE_SYNC_OBJECT	sTASyncData;


	PVRSRV_DEVICE_SYNC_OBJECT	s3DSyncData;
} SGXMKIF_2DCMD_SHARED, *PSGXMKIF_2DCMD_SHARED;
#endif


typedef struct _SGXMKIF_HWDEVICE_SYNC_LIST_
{
	IMG_DEV_VIRTADDR	sAccessDevAddr;
	u32			ui32NumSyncObjects;

	PVRSRV_DEVICE_SYNC_OBJECT	asSyncData[1];
} SGXMKIF_HWDEVICE_SYNC_LIST, *PSGXMKIF_HWDEVICE_SYNC_LIST;


#define PVRSRV_USSE_EDM_INIT_COMPLETE			(1UL << 0)

#define PVRSRV_USSE_EDM_POWMAN_IDLE_COMPLETE				(1UL << 2)
#define PVRSRV_USSE_EDM_POWMAN_POWEROFF_COMPLETE			(1UL << 3)
#define PVRSRV_USSE_EDM_POWMAN_POWEROFF_RESTART_IMMEDIATE	(1UL << 4)
#define PVRSRV_USSE_EDM_POWMAN_NO_WORK						(1UL << 5)

#define PVRSRV_USSE_EDM_INTERRUPT_HWR			(1UL << 0)
#define PVRSRV_USSE_EDM_INTERRUPT_ACTIVE_POWER	(1UL << 1)

#define PVRSRV_USSE_EDM_CLEANUPCMD_COMPLETE 	(1UL << 0)

#define PVRSRV_USSE_MISCINFO_READY		0x1UL
#define PVRSRV_USSE_MISCINFO_GET_STRUCT_SIZES	0x2UL
#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
#define PVRSRV_USSE_MISCINFO_MEMREAD			0x4UL

#if !defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
#define PVRSRV_USSE_MISCINFO_MEMREAD_FAIL		0x1UL << 31
#endif
#endif


#define	PVRSRV_CLEANUPCMD_RT		0x1
#define	PVRSRV_CLEANUPCMD_RC		0x2
#define	PVRSRV_CLEANUPCMD_TC		0x3
#define	PVRSRV_CLEANUPCMD_2DC		0x4
#define	PVRSRV_CLEANUPCMD_PB		0x5

#define PVRSRV_POWERCMD_POWEROFF	0x1
#define PVRSRV_POWERCMD_IDLE		0x2
#define PVRSRV_POWERCMD_RESUME		0x3


#if defined(SGX_FEATURE_BIF_NUM_DIRLISTS)
#define SGX_BIF_DIR_LIST_INDEX_EDM	(SGX_FEATURE_BIF_NUM_DIRLISTS - 1)
#else
#define SGX_BIF_DIR_LIST_INDEX_EDM	(0)
#endif

#define	SGX_BIF_INVALIDATE_PTCACHE	0x1
#define	SGX_BIF_INVALIDATE_PDCACHE	0x2
#define SGX_BIF_INVALIDATE_SLCACHE	0x4


typedef struct _SGX_MISCINFO_STRUCT_SIZES_
{
#if defined (SGX_FEATURE_2D_HARDWARE)
	u32	ui32Sizeof_2DCMD;
	u32	ui32Sizeof_2DCMD_SHARED;
#endif
	u32	ui32Sizeof_CMDTA;
	u32	ui32Sizeof_CMDTA_SHARED;
	u32	ui32Sizeof_TRANSFERCMD;
	u32	ui32Sizeof_TRANSFERCMD_SHARED;
	u32	ui32Sizeof_3DREGISTERS;
	u32	ui32Sizeof_HWPBDESC;
	u32	ui32Sizeof_HWRENDERCONTEXT;
	u32	ui32Sizeof_HWRENDERDETAILS;
	u32	ui32Sizeof_HWRTDATA;
	u32	ui32Sizeof_HWRTDATASET;
	u32	ui32Sizeof_HWTRANSFERCONTEXT;
	u32	ui32Sizeof_HOST_CTL;
	u32	ui32Sizeof_COMMAND;
} SGX_MISCINFO_STRUCT_SIZES;


#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
typedef struct _PVRSRV_SGX_MISCINFO_MEMREAD
{
	IMG_DEV_VIRTADDR	sDevVAddr;
	IMG_DEV_PHYADDR		sPDDevPAddr;
} PVRSRV_SGX_MISCINFO_MEMREAD;
#endif

typedef struct _PVRSRV_SGX_MISCINFO_INFO
{
	u32						ui32MiscInfoFlags;
	PVRSRV_SGX_MISCINFO_FEATURES	sSGXFeatures;
	SGX_MISCINFO_STRUCT_SIZES		sSGXStructSizes;
#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
	PVRSRV_SGX_MISCINFO_MEMREAD		sSGXMemReadData;
#endif
} PVRSRV_SGX_MISCINFO_INFO;

#ifdef PVRSRV_USSE_EDM_STATUS_DEBUG
#define SGXMK_TRACE_BUFFER_SIZE 512
#endif

#define SGXMKIF_HWPERF_CB_SIZE					0x100

#if defined(SUPPORT_SGX_HWPERF)
typedef struct _SGXMKIF_HWPERF_CB_ENTRY_
{
	u32	ui32FrameNo;
	u32	ui32Type;
	u32	ui32Ordinal;
	u32	ui32TimeWraps;
	u32	ui32Time;
	u32	ui32Counters[PVRSRV_SGX_HWPERF_NUM_COUNTERS];
} SGXMKIF_HWPERF_CB_ENTRY;

typedef struct _SGXMKIF_HWPERF_CB_
{
	u32				ui32Woff;
	u32				ui32Roff;
	u32				ui32OrdinalGRAPHICS;
	u32				ui32OrdinalMK_EXECUTION;
	SGXMKIF_HWPERF_CB_ENTRY psHWPerfCBData[SGXMKIF_HWPERF_CB_SIZE];
} SGXMKIF_HWPERF_CB;
#endif


#endif


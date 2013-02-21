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

#if !defined (__SERVICESINT_H__)
#define __SERVICESINT_H__


#include "services.h"
#include "sysinfo.h"

#define HWREC_DEFAULT_TIMEOUT	(500)

#define DRIVERNAME_MAXLENGTH	(100)

typedef enum _PVRSRV_MEMTYPE_
{
	PVRSRV_MEMTYPE_UNKNOWN		= 0,
	PVRSRV_MEMTYPE_DEVICE		= 1,
	PVRSRV_MEMTYPE_DEVICECLASS	= 2,
	PVRSRV_MEMTYPE_WRAPPED		= 3,
	PVRSRV_MEMTYPE_MAPPED		= 4,
}
PVRSRV_MEMTYPE;

typedef struct _PVRSRV_KERNEL_MEM_INFO_
{

	void *				pvLinAddrKM;


	IMG_DEV_VIRTADDR		sDevVAddr;


	u32				ui32Flags;


	u32				ui32AllocSize;


	PVRSRV_MEMBLK			sMemBlk;


	void *				pvSysBackupBuffer;


	u32				ui32RefCount;


	int				bPendingFree;


	#if defined(SUPPORT_MEMINFO_IDS)
	#if !defined(USE_CODE)

	u64				ui64Stamp;
	#else
	u32				dummy1;
	u32				dummy2;
	#endif
	#endif


	struct _PVRSRV_KERNEL_SYNC_INFO_	*psKernelSyncInfo;

	PVRSRV_MEMTYPE			memType;

} PVRSRV_KERNEL_MEM_INFO;


typedef struct _PVRSRV_KERNEL_SYNC_INFO_
{

	PVRSRV_SYNC_DATA		*psSyncData;


	IMG_DEV_VIRTADDR		sWriteOpsCompleteDevVAddr;


	IMG_DEV_VIRTADDR		sReadOpsCompleteDevVAddr;


	PVRSRV_KERNEL_MEM_INFO	*psSyncDataMemInfoKM;


	void *				hResItem;



        u32              ui32RefCount;

} PVRSRV_KERNEL_SYNC_INFO;

typedef struct _PVRSRV_DEVICE_SYNC_OBJECT_
{

	u32			ui32ReadOpsPendingVal;
	IMG_DEV_VIRTADDR	sReadOpsCompleteDevVAddr;
	u32			ui32WriteOpsPendingVal;
	IMG_DEV_VIRTADDR	sWriteOpsCompleteDevVAddr;
} PVRSRV_DEVICE_SYNC_OBJECT;

typedef struct _PVRSRV_SYNC_OBJECT
{
	PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfoKM;
	u32				ui32WriteOpsPending;
	u32				ui32ReadOpsPending;

}PVRSRV_SYNC_OBJECT, *PPVRSRV_SYNC_OBJECT;

typedef struct _PVRSRV_COMMAND
{
	u32			ui32CmdSize;
	u32			ui32DevIndex;
	u32			CommandType;
	u32			ui32DstSyncCount;
	u32			ui32SrcSyncCount;
	PVRSRV_SYNC_OBJECT	*psDstSync;
	PVRSRV_SYNC_OBJECT	*psSrcSync;
	u32			ui32DataSize;
	u32			ui32ProcessID;
	void			*pvData;
}PVRSRV_COMMAND, *PPVRSRV_COMMAND;


typedef struct _PVRSRV_QUEUE_INFO_
{
	void			*pvLinQueueKM;
	void			*pvLinQueueUM;
	volatile u32	ui32ReadOffset;
	volatile u32	ui32WriteOffset;
	u32			*pui32KickerAddrKM;
	u32			*pui32KickerAddrUM;
	u32			ui32QueueSize;

	u32			ui32ProcessID;

	void *			hMemBlock[2];

	struct _PVRSRV_QUEUE_INFO_ *psNextKM;
}PVRSRV_QUEUE_INFO;

typedef PVRSRV_ERROR (*PFN_INSERT_CMD) (PVRSRV_QUEUE_INFO*,
										PVRSRV_COMMAND**,
										u32,
										u16,
										u32,
										PVRSRV_KERNEL_SYNC_INFO*[],
										u32,
										PVRSRV_KERNEL_SYNC_INFO*[],
										u32);
typedef PVRSRV_ERROR (*PFN_SUBMIT_CMD) (PVRSRV_QUEUE_INFO*, PVRSRV_COMMAND*, int);


typedef struct PVRSRV_DEVICECLASS_BUFFER_TAG
{
	PFN_GET_BUFFER_ADDR		pfnGetBufferAddr;
	void *				hDevMemContext;
	void *				hExtDevice;
	void *				hExtBuffer;
	PVRSRV_KERNEL_SYNC_INFO	*psKernelSyncInfo;

} PVRSRV_DEVICECLASS_BUFFER;


typedef struct PVRSRV_CLIENT_DEVICECLASS_INFO_TAG
{
	void * hDeviceKM;
	void *	hServices;
} PVRSRV_CLIENT_DEVICECLASS_INFO;


static inline
u32 PVRSRVGetWriteOpsPending(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo, int bIsReadOp)
{
	u32 ui32WriteOpsPending;

	if(bIsReadOp)
	{
		ui32WriteOpsPending = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
	else
	{



		ui32WriteOpsPending = psSyncInfo->psSyncData->ui32WriteOpsPending++;
	}

	return ui32WriteOpsPending;
}

static inline
u32 PVRSRVGetReadOpsPending(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo, int bIsReadOp)
{
	u32 ui32ReadOpsPending;

	if(bIsReadOp)
	{
		ui32ReadOpsPending = psSyncInfo->psSyncData->ui32ReadOpsPending++;
	}
	else
	{
		ui32ReadOpsPending = psSyncInfo->psSyncData->ui32ReadOpsPending;
	}

	return ui32ReadOpsPending;
}


PVRSRV_ERROR PVRSRVQueueCommand(void * hQueueInfo,
								PVRSRV_COMMAND *psCommand);



 PVRSRV_ERROR
PVRSRVGetMMUContextPDDevPAddr(const PVRSRV_CONNECTION *psConnection,
                              void * hDevMemContext,
                              IMG_DEV_PHYADDR *sPDDevPAddr);

 PVRSRV_ERROR
PVRSRVAllocSharedSysMem(const PVRSRV_CONNECTION *psConnection,
						u32 ui32Flags,
						u32 ui32Size,
						PVRSRV_CLIENT_MEM_INFO **ppsClientMemInfo);

 PVRSRV_ERROR
PVRSRVFreeSharedSysMem(const PVRSRV_CONNECTION *psConnection,
					   PVRSRV_CLIENT_MEM_INFO *psClientMemInfo);

 PVRSRV_ERROR
PVRSRVUnrefSharedSysMem(const PVRSRV_CONNECTION *psConnection,
                        PVRSRV_CLIENT_MEM_INFO *psClientMemInfo);

 PVRSRV_ERROR
PVRSRVMapMemInfoMem(const PVRSRV_CONNECTION *psConnection,
                    void * hKernelMemInfo,
                    PVRSRV_CLIENT_MEM_INFO **ppsClientMemInfo);


#endif


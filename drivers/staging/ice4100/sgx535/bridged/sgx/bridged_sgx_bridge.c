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



#include <stddef.h>



#if defined(SUPPORT_SGX)

#include "services.h"
#include "pvr_debug.h"
#include "pvr_bridge.h"
#include "sgx_bridge.h"
#include "perproc.h"
#include "power.h"
#include "pvr_bridge_km.h"
#include "sgx_bridge_km.h"

#if defined(SUPPORT_MSVDX)
	#include "msvdx_bridge.h"
#endif

#include "bridged_pvr_bridge.h"
#include "bridged_sgx_bridge.h"
#include "sgxutils.h"
#include "pdump_km.h"

static int
SGXGetClientInfoBW(u32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_GETCLIENTINFO *psGetClientInfoIN,
				   PVRSRV_BRIDGE_OUT_GETCLIENTINFO *psGetClientInfoOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_GETCLIENTINFO);

	psGetClientInfoOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psGetClientInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psGetClientInfoOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psGetClientInfoOUT->eError =
		SGXGetClientInfoKM(hDevCookieInt,
						   &psGetClientInfoOUT->sClientInfo);
	return 0;
}

static int
SGXReleaseClientInfoBW(u32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_RELEASECLIENTINFO *psReleaseClientInfoIN,
					   PVRSRV_BRIDGE_RETURN *psRetOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;
	void * hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_RELEASECLIENTINFO);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psReleaseClientInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = (PVRSRV_SGXDEV_INFO *)((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;

	PVR_ASSERT(psDevInfo->ui32ClientRefCount > 0);

	psDevInfo->ui32ClientRefCount--;

	psRetOUT->eError = PVRSRV_OK;

	return 0;
}


static int
SGXGetInternalDevInfoBW(u32 ui32BridgeID,
						PVRSRV_BRIDGE_IN_GETINTERNALDEVINFO *psSGXGetInternalDevInfoIN,
						PVRSRV_BRIDGE_OUT_GETINTERNALDEVINFO *psSGXGetInternalDevInfoOUT,
						PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_GETINTERNALDEVINFO);

	psSGXGetInternalDevInfoOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psSGXGetInternalDevInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXGetInternalDevInfoOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXGetInternalDevInfoOUT->eError =
		SGXGetInternalDevInfoKM(hDevCookieInt,
								&psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo);


	psSGXGetInternalDevInfoOUT->eError =
		PVRSRVAllocHandle(psPerProc->psHandleBase,
						  &psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo.hHostCtlKernelMemInfoHandle,
						  psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo.hHostCtlKernelMemInfoHandle,
						  PVRSRV_HANDLE_TYPE_MEM_INFO,
						  PVRSRV_HANDLE_ALLOC_FLAG_SHARED);

	return 0;
}


static int
SGXDoKickBW(u32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_DOKICK *psDoKickIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	u32 i;
	int ret = 0;
	u32 ui32NumDstSyncs;
	void * *phKernelSyncInfoHandles = NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_DOKICK);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psDoKickIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psDoKickIN->sCCBKick.hCCBKernelMemInfo,
						   psDoKickIN->sCCBKick.hCCBKernelMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	if(psDoKickIN->sCCBKick.hTA3DSyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.hTA3DSyncInfo,
							   psDoKickIN->sCCBKick.hTA3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if(psDoKickIN->sCCBKick.hTASyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.hTASyncInfo,
							   psDoKickIN->sCCBKick.hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if(psDoKickIN->sCCBKick.h3DSyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.h3DSyncInfo,
							   psDoKickIN->sCCBKick.h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}


#if defined(SUPPORT_SGX_GENERALISED_SYNCOBJECTS)

	if (psDoKickIN->sCCBKick.ui32NumTASrcSyncs > SGX_MAX_TA_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

	for(i=0; i<psDoKickIN->sCCBKick.ui32NumTASrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ahTASrcKernelSyncInfo[i],
							   psDoKickIN->sCCBKick.ahTASrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32NumTADstSyncs > SGX_MAX_TA_DST_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

	for(i=0; i<psDoKickIN->sCCBKick.ui32NumTADstSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ahTADstKernelSyncInfo[i],
							   psDoKickIN->sCCBKick.ahTADstKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32Num3DSrcSyncs > SGX_MAX_3D_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

	for(i=0; i<psDoKickIN->sCCBKick.ui32Num3DSrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ah3DSrcKernelSyncInfo[i],
							   psDoKickIN->sCCBKick.ah3DSrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#else

	if (psDoKickIN->sCCBKick.ui32NumSrcSyncs > SGX_MAX_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for(i=0; i<psDoKickIN->sCCBKick.ui32NumSrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ahSrcKernelSyncInfo[i],
							   psDoKickIN->sCCBKick.ahSrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#endif

	if (psDoKickIN->sCCBKick.ui32NumTAStatusVals > SGX_MAX_TA_STATUS_VALS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psDoKickIN->sCCBKick.ui32NumTAStatusVals; i++)
	{
		psRetOUT->eError =
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.asTAStatusUpdate[i].hKernelMemInfo,
							   psDoKickIN->sCCBKick.asTAStatusUpdate[i].hKernelMemInfo,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
#else
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ahTAStatusSyncInfo[i],
							   psDoKickIN->sCCBKick.ahTAStatusSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
#endif
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32Num3DStatusVals > SGX_MAX_3D_STATUS_VALS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for(i = 0; i < psDoKickIN->sCCBKick.ui32Num3DStatusVals; i++)
	{
		psRetOUT->eError =
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.as3DStatusUpdate[i].hKernelMemInfo,
							   psDoKickIN->sCCBKick.as3DStatusUpdate[i].hKernelMemInfo,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
#else
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psDoKickIN->sCCBKick.ah3DStatusSyncInfo[i],
							   psDoKickIN->sCCBKick.ah3DStatusSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
#endif

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	ui32NumDstSyncs = psDoKickIN->sCCBKick.ui32NumDstSyncObjects;

	if(ui32NumDstSyncs > 0)
	{
		if(!OSAccessOK(PVR_VERIFY_READ,
						psDoKickIN->sCCBKick.pahDstSyncHandles,
						ui32NumDstSyncs * sizeof(void *)))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: SGXDoKickBW:"
					" Invalid pasDstSyncHandles pointer", __FUNCTION__));
			return -EFAULT;
		}

		psRetOUT->eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
										ui32NumDstSyncs * sizeof(void *),
										(void **)&phKernelSyncInfoHandles,
										0,
										"Array of Synchronization Info Handles");
		if (psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}

		if(CopyFromUserWrapper(psPerProc,
							ui32BridgeID,
							phKernelSyncInfoHandles,
							psDoKickIN->sCCBKick.pahDstSyncHandles,
							ui32NumDstSyncs * sizeof(void *)) != PVRSRV_OK)
		{
			ret = -EFAULT;
			goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
		}


		psDoKickIN->sCCBKick.pahDstSyncHandles = phKernelSyncInfoHandles;

		for( i = 0; i < ui32NumDstSyncs; i++)
		{
			psRetOUT->eError =
				PVRSRVLookupHandle(psPerProc->psHandleBase,
									&psDoKickIN->sCCBKick.pahDstSyncHandles[i],
									psDoKickIN->sCCBKick.pahDstSyncHandles[i],
									PVRSRV_HANDLE_TYPE_SYNC_INFO);

			if(psRetOUT->eError != PVRSRV_OK)
			{
				goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
			}

		}

		psRetOUT->eError =
					PVRSRVLookupHandle(psPerProc->psHandleBase,
									   &psDoKickIN->sCCBKick.hKernelHWSyncListMemInfo,
									   psDoKickIN->sCCBKick.hKernelHWSyncListMemInfo,
									   PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
		}
	}

	psRetOUT->eError =
		SGXDoKickKM(hDevCookieInt,
					&psDoKickIN->sCCBKick);

PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT:

	if(phKernelSyncInfoHandles)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32NumDstSyncs * sizeof(void *),
				  (void *)phKernelSyncInfoHandles,
				  0);

	}
	return ret;
}


static int
SGXScheduleProcessQueuesBW(u32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SGX_SCHEDULE_PROCESS_QUEUES *psScheduleProcQIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SCHEDULE_PROCESS_QUEUES);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psScheduleProcQIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXScheduleProcessQueuesKM(hDevCookieInt);

	return 0;
}


#if defined(TRANSFER_QUEUE)
static int
SGXSubmitTransferBW(u32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SUBMITTRANSFER *psSubmitTransferIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_TRANSFER_SGX_KICK *psKick;
	u32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SUBMITTRANSFER);

	psKick = &psSubmitTransferIN->sKick;

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSubmitTransferIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psKick->hCCBMemInfo,
						   psKick->hCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	if (psKick->hTASyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->hTASyncInfo,
							   psKick->hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->h3DSyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->h3DSyncInfo,
							   psKick->h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->ui32NumSrcSync > SGX_MAX_TRANSFER_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->ahSrcSyncInfo[i],
							   psKick->ahSrcSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->ui32NumDstSync > SGX_MAX_TRANSFER_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psKick->ui32NumDstSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->ahDstSyncInfo[i],
							   psKick->ahDstSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	psRetOUT->eError = SGXSubmitTransferKM(hDevCookieInt, psKick);

	return 0;
}


#if defined(SGX_FEATURE_2D_HARDWARE)
static int
SGXSubmit2DBW(u32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SUBMIT2D *psSubmit2DIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_2D_SGX_KICK *psKick;
	u32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SUBMIT2D);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSubmit2DIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psKick = &psSubmit2DIN->sKick;

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psKick->hCCBMemInfo,
						   psKick->hCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	if (psKick->hTASyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->hTASyncInfo,
							   psKick->hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->h3DSyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->h3DSyncInfo,
							   psKick->h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->ui32NumSrcSync > SGX_MAX_2D_SRC_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->ahSrcSyncInfo[i],
							   psKick->ahSrcSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->hDstSyncInfo != NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->hDstSyncInfo,
							   psKick->hDstSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	psRetOUT->eError =
		SGXSubmit2DKM(hDevCookieInt, psKick);

	return 0;
}
#endif
#endif


static int
SGXGetMiscInfoBW(u32 ui32BridgeID,
				 PVRSRV_BRIDGE_IN_SGXGETMISCINFO *psSGXGetMiscInfoIN,
				 PVRSRV_BRIDGE_RETURN *psRetOUT,
				 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	void * hDevMemContextInt = 0;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	SGX_MISC_INFO        sMiscInfo;
 	PVRSRV_DEVICE_NODE *psDeviceNode;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID,
							PVRSRV_BRIDGE_SGX_GETMISCINFO);

	psRetOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							&hDevCookieInt,
							psSGXGetMiscInfoIN->hDevCookie,
							PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)

	if (psSGXGetMiscInfoIN->psMiscInfo->eRequest == SGX_MISC_INFO_REQUEST_MEMREAD)
	{
		psRetOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDevMemContextInt,
								psSGXGetMiscInfoIN->psMiscInfo->hDevMemContext,
								PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#endif

	psDeviceNode = hDevCookieInt;
	PVR_ASSERT(psDeviceNode != NULL);
	if (psDeviceNode == NULL)
	{
		return -EFAULT;
	}

	psDevInfo = psDeviceNode->pvDevice;


	psRetOUT->eError = CopyFromUserWrapper(psPerProc,
			                               ui32BridgeID,
			                               &sMiscInfo,
			                               psSGXGetMiscInfoIN->psMiscInfo,
			                               sizeof(SGX_MISC_INFO));
	if (psRetOUT->eError != PVRSRV_OK)
	{
		return -EFAULT;
	}

#ifdef SUPPORT_SGX_HWPERF
	if (sMiscInfo.eRequest == SGX_MISC_INFO_REQUEST_HWPERF_RETRIEVE_CB)
	{

		void           * pAllocated;
		void *           hAllocatedHandle;
		void           * psTmpUserData;
		u32           allocatedSize;

		allocatedSize = (u32)(sMiscInfo.uData.sRetrieveCB.ui32ArraySize * sizeof(PVRSRV_SGX_HWPERF_CBDATA));

		ASSIGN_AND_EXIT_ON_ERROR(psRetOUT->eError,
		                    OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		                    allocatedSize,
		                    &pAllocated,
		                    &hAllocatedHandle,
							"Array of Hardware Performance Circular Buffer Data"));


		psTmpUserData = sMiscInfo.uData.sRetrieveCB.psHWPerfData;
		sMiscInfo.uData.sRetrieveCB.psHWPerfData = pAllocated;

		psRetOUT->eError = SGXGetMiscInfoKM(psDevInfo, &sMiscInfo, psDeviceNode, 0);
		if (psRetOUT->eError != PVRSRV_OK)
		{
			OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
					  allocatedSize,
					  pAllocated,
					  hAllocatedHandle);

			return 0;
		}


		psRetOUT->eError = CopyToUserWrapper(psPerProc,
					                         ui32BridgeID,
					                         psTmpUserData,
					                         sMiscInfo.uData.sRetrieveCB.psHWPerfData,
					                         allocatedSize);

		sMiscInfo.uData.sRetrieveCB.psHWPerfData = psTmpUserData;

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  allocatedSize,
				  pAllocated,
			      hAllocatedHandle);

		if (psRetOUT->eError != PVRSRV_OK)
		{
			return -EFAULT;
		}
	}
	else
#endif
	{
		psRetOUT->eError = SGXGetMiscInfoKM(psDevInfo, &sMiscInfo, psDeviceNode, hDevMemContextInt);

		if (psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}


	psRetOUT->eError = CopyToUserWrapper(psPerProc,
		                             ui32BridgeID,
		                             psSGXGetMiscInfoIN->psMiscInfo,
		                             &sMiscInfo,
		                             sizeof(SGX_MISC_INFO));
	if (psRetOUT->eError != PVRSRV_OK)
	{
		return -EFAULT;
	}
	return 0;
}


#if defined(SUPPORT_SGX_HWPERF)
static int
SGXReadDiffCountersBW(u32									ui32BridgeID,
						PVRSRV_BRIDGE_IN_SGX_READ_DIFF_COUNTERS		*psSGXReadDiffCountersIN,
						PVRSRV_BRIDGE_OUT_SGX_READ_DIFF_COUNTERS	*psSGXReadDiffCountersOUT,
						PVRSRV_PER_PROCESS_DATA						*psPerProc)
{
	void *			hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_READ_DIFF_COUNTERS);

	psSGXReadDiffCountersOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							&hDevCookieInt,
							psSGXReadDiffCountersIN->hDevCookie,
							PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psSGXReadDiffCountersOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXReadDiffCountersOUT->eError = SGXReadDiffCountersKM(hDevCookieInt,
							psSGXReadDiffCountersIN->ui32Reg,
							&psSGXReadDiffCountersOUT->ui32Old,
							psSGXReadDiffCountersIN->bNew,
							psSGXReadDiffCountersIN->ui32New,
							psSGXReadDiffCountersIN->ui32NewReset,
							psSGXReadDiffCountersIN->ui32CountersReg,
							psSGXReadDiffCountersIN->ui32Reg2,
							&psSGXReadDiffCountersOUT->bActive,
							&psSGXReadDiffCountersOUT->sDiffs);

	return 0;
}


static int
SGXReadHWPerfCBBW(u32							ui32BridgeID,
				  PVRSRV_BRIDGE_IN_SGX_READ_HWPERF_CB	*psSGXReadHWPerfCBIN,
				  PVRSRV_BRIDGE_OUT_SGX_READ_HWPERF_CB	*psSGXReadHWPerfCBOUT,
				  PVRSRV_PER_PROCESS_DATA				*psPerProc)
{
	void *					hDevCookieInt;
	PVRSRV_SGX_HWPERF_CB_ENTRY	*psAllocated;
	void *					hAllocatedHandle;
	u32					ui32AllocatedSize;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_READ_HWPERF_CB);

	psSGXReadHWPerfCBOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							&hDevCookieInt,
							psSGXReadHWPerfCBIN->hDevCookie,
							PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psSGXReadHWPerfCBOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	ui32AllocatedSize = psSGXReadHWPerfCBIN->ui32ArraySize *
							sizeof(psSGXReadHWPerfCBIN->psHWPerfCBData[0]);
	ASSIGN_AND_EXIT_ON_ERROR(psSGXReadHWPerfCBOUT->eError,
	                    OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
	                    ui32AllocatedSize,
	                    (void **)&psAllocated,
	                    &hAllocatedHandle,
						"Array of Hardware Performance Circular Buffer Data"));

	psSGXReadHWPerfCBOUT->eError = SGXReadHWPerfCBKM(hDevCookieInt,
													 psSGXReadHWPerfCBIN->ui32ArraySize,
													 psAllocated,
													 &psSGXReadHWPerfCBOUT->ui32DataCount,
													 &psSGXReadHWPerfCBOUT->ui32ClockSpeed,
													 &psSGXReadHWPerfCBOUT->ui32HostTimeStamp);
	if (psSGXReadHWPerfCBOUT->eError == PVRSRV_OK)
	{
		psSGXReadHWPerfCBOUT->eError = CopyToUserWrapper(psPerProc,
		                                                 ui32BridgeID,
		                                                 psSGXReadHWPerfCBIN->psHWPerfCBData,
		                                                 psAllocated,
		                                                 ui32AllocatedSize);
	}

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  ui32AllocatedSize,
			  psAllocated,
			  hAllocatedHandle);


	return 0;
}
#endif


static int
SGXDevInitPart2BW(u32 ui32BridgeID,
				  PVRSRV_BRIDGE_IN_SGXDEVINITPART2 *psSGXDevInitPart2IN,
				  PVRSRV_BRIDGE_RETURN *psRetOUT,
				  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_ERROR eError;
	int bDissociateFailed = 0;
	int bLookupFailed = 0;
	int bReleaseFailed = 0;
	void * hDummy;
	u32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_DEVINITPART2);

	if(!psPerProc->bInitProcess)
	{
		psRetOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXDevInitPart2IN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}




	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);


	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);

#if defined(SGX_SUPPORT_HWPROFILING)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SUPPORT_SGX_HWPERF)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bLookupFailed |= (int)(eError != PVRSRV_OK);
#endif

	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
		void * hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

		if (hHandle == NULL)
		{
			continue;
		}

		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &hDummy,
							   hHandle,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
		bLookupFailed |= (int)(eError != PVRSRV_OK);
	}

	if (bLookupFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A handle lookup failed"));
		psRetOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}


	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);


	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);


	#if defined(SGX_SUPPORT_HWPROFILING)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SUPPORT_SGX_HWPERF)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	bReleaseFailed |= (int)(eError != PVRSRV_OK);
#endif


	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
		void * *phHandle = &psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

		if (*phHandle == NULL)
			continue;

		eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
							   phHandle,
							   *phHandle,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
		bReleaseFailed |= (int)(eError != PVRSRV_OK);
	}

	if (bReleaseFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A handle release failed"));
		psRetOUT->eError = PVRSRV_ERROR_GENERIC;

		PVR_DBG_BREAK;
		return 0;
	}


	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);

	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);


	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);


#if defined(SGX_SUPPORT_HWPROFILING)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SUPPORT_SGX_HWPERF)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo);
	bDissociateFailed |= (int)(eError != PVRSRV_OK);
#endif

	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
		void * hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

		if (hHandle == NULL)
			continue;

		eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, hHandle);
		bDissociateFailed |= (int)(eError != PVRSRV_OK);
	}




	if(bDissociateFailed)
	{
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo);

		for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
		{
			void * hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

			if (hHandle == NULL)
				continue;

			PVRSRVFreeDeviceMemKM(hDevCookieInt, (PVRSRV_KERNEL_MEM_INFO *)hHandle);

		}

		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A dissociate failed"));

		psRetOUT->eError = PVRSRV_ERROR_GENERIC;


		PVR_DBG_BREAK;
		return 0;
	}

	psRetOUT->eError =
		DevInitSGXPart2KM(psPerProc,
						  hDevCookieInt,
						  &psSGXDevInitPart2IN->sInitInfo);

	return 0;
}


static int
SGXRegisterHWRenderContextBW(u32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_RENDER_CONTEXT *psSGXRegHWRenderContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_RENDER_CONTEXT *psSGXRegHWRenderContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	void * hHWRenderContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_RENDER_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHWRenderContextOUT->eError, psPerProc, 1);

	psSGXRegHWRenderContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHWRenderContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHWRenderContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHWRenderContextInt =
		SGXRegisterHWRenderContextKM(hDevCookieInt,
									 &psSGXRegHWRenderContextIN->sHWRenderContextDevVAddr,
									 psPerProc);

	if (hHWRenderContextInt == NULL)
	{
		psSGXRegHWRenderContextOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHWRenderContextOUT->hHWRenderContext,
					  hHWRenderContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHWRenderContextOUT->eError, psPerProc);

	return 0;
}


static int
SGXUnregisterHWRenderContextBW(u32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_RENDER_CONTEXT *psSGXUnregHWRenderContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hHWRenderContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_RENDER_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHWRenderContextInt,
						   psSGXUnregHWRenderContextIN->hHWRenderContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHWRenderContextKM(hHWRenderContextInt);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHWRenderContextIN->hHWRenderContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT);

	return 0;
}


static int
SGXRegisterHWTransferContextBW(u32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_TRANSFER_CONTEXT *psSGXRegHWTransferContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_TRANSFER_CONTEXT *psSGXRegHWTransferContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	void * hHWTransferContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_TRANSFER_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHWTransferContextOUT->eError, psPerProc, 1);

	psSGXRegHWTransferContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHWTransferContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHWTransferContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHWTransferContextInt =
		SGXRegisterHWTransferContextKM(hDevCookieInt,
									   &psSGXRegHWTransferContextIN->sHWTransferContextDevVAddr,
									   psPerProc);

	if (hHWTransferContextInt == NULL)
	{
		psSGXRegHWTransferContextOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHWTransferContextOUT->hHWTransferContext,
					  hHWTransferContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHWTransferContextOUT->eError, psPerProc);

	return 0;
}


static int
SGXUnregisterHWTransferContextBW(u32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_TRANSFER_CONTEXT *psSGXUnregHWTransferContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hHWTransferContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_TRANSFER_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHWTransferContextInt,
						   psSGXUnregHWTransferContextIN->hHWTransferContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHWTransferContextKM(hHWTransferContextInt);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHWTransferContextIN->hHWTransferContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT);

	return 0;
}


#if defined(SGX_FEATURE_2D_HARDWARE)
static int
SGXRegisterHW2DContextBW(u32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_2D_CONTEXT *psSGXRegHW2DContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_2D_CONTEXT *psSGXRegHW2DContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	void * hHW2DContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_2D_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHW2DContextOUT->eError, psPerProc, 1);

	psSGXRegHW2DContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHW2DContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHW2DContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHW2DContextInt =
		SGXRegisterHW2DContextKM(hDevCookieInt,
								 &psSGXRegHW2DContextIN->sHW2DContextDevVAddr,
								 psPerProc);

	if (hHW2DContextInt == NULL)
	{
		psSGXRegHW2DContextOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHW2DContextOUT->hHW2DContext,
					  hHW2DContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHW2DContextOUT->eError, psPerProc);

	return 0;
}


static int
SGXUnregisterHW2DContextBW(u32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_2D_CONTEXT *psSGXUnregHW2DContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hHW2DContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_2D_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHW2DContextInt,
						   psSGXUnregHW2DContextIN->hHW2DContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHW2DContextKM(hHW2DContextInt);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHW2DContextIN->hHW2DContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT);

	return 0;
}
#endif

static int
SGXFlushHWRenderTargetBW(u32 ui32BridgeID,
						  PVRSRV_BRIDGE_IN_SGX_FLUSH_HW_RENDER_TARGET *psSGXFlushHWRenderTargetIN,
						  PVRSRV_BRIDGE_RETURN *psRetOUT,
						  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_FLUSH_HW_RENDER_TARGET);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXFlushHWRenderTargetIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	SGXFlushHWRenderTargetKM(hDevCookieInt, psSGXFlushHWRenderTargetIN->sHWRTDataSetDevVAddr);

	return 0;
}


static int
SGX2DQueryBlitsCompleteBW(u32 ui32BridgeID,
						  PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE *ps2DQueryBltsCompleteIN,
						  PVRSRV_BRIDGE_RETURN *psRetOUT,
						  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	void *pvSyncInfo;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_2DQUERYBLTSCOMPLETE);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   ps2DQueryBltsCompleteIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &pvSyncInfo,
						   ps2DQueryBltsCompleteIN->hKernSyncInfo,
						   PVRSRV_HANDLE_TYPE_SYNC_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = (PVRSRV_SGXDEV_INFO *)((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;

	psRetOUT->eError =
		SGX2DQueryBlitsCompleteKM(psDevInfo,
								  (PVRSRV_KERNEL_SYNC_INFO *)pvSyncInfo,
								  ps2DQueryBltsCompleteIN->bWaitForComplete);

	return 0;
}


static int
SGXFindSharedPBDescBW(u32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescIN,
					  PVRSRV_BRIDGE_OUT_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescOUT,
					  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescSubKernelMemInfos = NULL;
	u32 ui32SharedPBDescSubKernelMemInfosCount = 0;
	u32 i;
	void * hSharedPBDesc = NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXFindSharedPBDescOUT->eError, psPerProc, PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS + 4);

	psSGXFindSharedPBDescOUT->hSharedPBDesc = NULL;

	psSGXFindSharedPBDescOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXFindSharedPBDescIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;

	psSGXFindSharedPBDescOUT->eError =
		SGXFindSharedPBDescKM(psPerProc, hDevCookieInt,
							  psSGXFindSharedPBDescIN->bLockOnFailure,
							  psSGXFindSharedPBDescIN->ui32TotalPBSize,
							  &hSharedPBDesc,
							  &psSharedPBDescKernelMemInfo,
							  &psHWPBDescKernelMemInfo,
							  &psBlockKernelMemInfo,
							  &psHWBlockKernelMemInfo,
							  &ppsSharedPBDescSubKernelMemInfos,
							  &ui32SharedPBDescSubKernelMemInfosCount);
	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;

	PVR_ASSERT(ui32SharedPBDescSubKernelMemInfosCount
			   <= PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS);

	psSGXFindSharedPBDescOUT->ui32SharedPBDescSubKernelMemInfoHandlesCount =
		ui32SharedPBDescSubKernelMemInfosCount;

	if(hSharedPBDesc == NULL)
	{
		psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle = 0;

		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hSharedPBDesc,
					  hSharedPBDesc,
					  PVRSRV_HANDLE_TYPE_SHARED_PB_DESC,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);


	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle,
					  psSharedPBDescKernelMemInfo,
					  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
					  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
					  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hHWPBDescKernelMemInfoHandle,
					  psHWPBDescKernelMemInfo,
					  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
					  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
					  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
				  &psSGXFindSharedPBDescOUT->hBlockKernelMemInfoHandle,
				  psBlockKernelMemInfo,
				  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
				  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
				  &psSGXFindSharedPBDescOUT->hHWBlockKernelMemInfoHandle,
				  psHWBlockKernelMemInfo,
				  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
				  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				  psSGXFindSharedPBDescOUT->hSharedPBDesc);


	for(i=0; i<ui32SharedPBDescSubKernelMemInfosCount; i++)
	{
		PVRSRV_BRIDGE_OUT_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescOut =
			psSGXFindSharedPBDescOUT;

			PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
							  &psSGXFindSharedPBDescOut->ahSharedPBDescSubKernelMemInfoHandles[i],
							  ppsSharedPBDescSubKernelMemInfos[i],
							  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
							  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
							  psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle);
	}

PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT:
	if (ppsSharedPBDescSubKernelMemInfos != NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  sizeof(PVRSRV_KERNEL_MEM_INFO *) * ui32SharedPBDescSubKernelMemInfosCount,
				  ppsSharedPBDescSubKernelMemInfos,
				  NULL);
	}

	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
	{
		if(hSharedPBDesc != NULL)
		{
			SGXUnrefSharedPBDescKM(hSharedPBDesc);
		}
	}
	else
	{
		COMMIT_HANDLE_BATCH_OR_ERROR(psSGXFindSharedPBDescOUT->eError, psPerProc);
	}

	return 0;
}


static int
SGXUnrefSharedPBDescBW(u32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_SGXUNREFSHAREDPBDESC *psSGXUnrefSharedPBDescIN,
					   PVRSRV_BRIDGE_OUT_SGXUNREFSHAREDPBDESC *psSGXUnrefSharedPBDescOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hSharedPBDesc;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREFSHAREDPBDESC);

	psSGXUnrefSharedPBDescOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hSharedPBDesc,
						   psSGXUnrefSharedPBDescIN->hSharedPBDesc,
						   PVRSRV_HANDLE_TYPE_SHARED_PB_DESC);
	if(psSGXUnrefSharedPBDescOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXUnrefSharedPBDescOUT->eError =
		SGXUnrefSharedPBDescKM(hSharedPBDesc);

	if(psSGXUnrefSharedPBDescOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXUnrefSharedPBDescOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
						   psSGXUnrefSharedPBDescIN->hSharedPBDesc,
						   PVRSRV_HANDLE_TYPE_SHARED_PB_DESC);

	return 0;
}


static int
SGXAddSharedPBDescBW(u32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_SGXADDSHAREDPBDESC *psSGXAddSharedPBDescIN,
					 PVRSRV_BRIDGE_OUT_SGXADDSHAREDPBDESC *psSGXAddSharedPBDescOUT,
					 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo;
	u32 ui32KernelMemInfoHandlesCount =
		psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount;
	int ret = 0;
	void * *phKernelMemInfoHandles = NULL;
	PVRSRV_KERNEL_MEM_INFO **ppsKernelMemInfos = NULL;
	u32 i;
	PVRSRV_ERROR eError;
	void * hSharedPBDesc = NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXAddSharedPBDescOUT->eError, psPerProc, 1);

	psSGXAddSharedPBDescOUT->hSharedPBDesc = NULL;

	PVR_ASSERT(ui32KernelMemInfoHandlesCount
			   <= PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDevCookieInt,
								psSGXAddSharedPBDescIN->hDevCookie,
								PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(void **)&psSharedPBDescKernelMemInfo,
								psSGXAddSharedPBDescIN->hSharedPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(void **)&psHWPBDescKernelMemInfo,
								psSGXAddSharedPBDescIN->hHWPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(void **)&psBlockKernelMemInfo,
								psSGXAddSharedPBDescIN->hBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(void **)&psHWBlockKernelMemInfo,
								psSGXAddSharedPBDescIN->hHWBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}


	if(!OSAccessOK(PVR_VERIFY_READ,
				   psSGXAddSharedPBDescIN->phKernelMemInfoHandles,
				   ui32KernelMemInfoHandlesCount * sizeof(void *)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC:"
				 " Invalid phKernelMemInfos pointer", __FUNCTION__));
		ret = -EFAULT;
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32KernelMemInfoHandlesCount * sizeof(void *),
				  (void **)&phKernelMemInfoHandles,
				  0,
				  "Array of Handles");
	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	if(CopyFromUserWrapper(psPerProc,
			               ui32BridgeID,
			               phKernelMemInfoHandles,
						   psSGXAddSharedPBDescIN->phKernelMemInfoHandles,
						   ui32KernelMemInfoHandlesCount * sizeof(void *))
	   != PVRSRV_OK)
	{
		ret = -EFAULT;
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32KernelMemInfoHandlesCount * sizeof(PVRSRV_KERNEL_MEM_INFO *),
				  (void **)&ppsKernelMemInfos,
				  0,
				  "Array of pointers to Kernel Memory Info");
	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	for(i=0; i<ui32KernelMemInfoHandlesCount; i++)
	{
		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									(void **)&ppsKernelMemInfos[i],
									phKernelMemInfoHandles[i],
									PVRSRV_HANDLE_TYPE_MEM_INFO);
		if(eError != PVRSRV_OK)
		{
			goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
		}
	}



	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hSharedPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);


	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hHWPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);


	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);


	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hHWBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);

	for(i=0; i<ui32KernelMemInfoHandlesCount; i++)
	{

		eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
									phKernelMemInfoHandles[i],
									PVRSRV_HANDLE_TYPE_MEM_INFO);
		PVR_ASSERT(eError == PVRSRV_OK);
	}

	eError = SGXAddSharedPBDescKM(psPerProc, hDevCookieInt,
								  psSharedPBDescKernelMemInfo,
								  psHWPBDescKernelMemInfo,
								  psBlockKernelMemInfo,
								  psHWBlockKernelMemInfo,
								  psSGXAddSharedPBDescIN->ui32TotalPBSize,
								  &hSharedPBDesc,
								  ppsKernelMemInfos,
								  ui32KernelMemInfoHandlesCount);


	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
				  &psSGXAddSharedPBDescOUT->hSharedPBDesc,
				  hSharedPBDesc,
				  PVRSRV_HANDLE_TYPE_SHARED_PB_DESC,
				  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT:

	if(phKernelMemInfoHandles)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount * sizeof(void *),
				  (void *)phKernelMemInfoHandles,
				  0);
	}
	if(ppsKernelMemInfos)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount * sizeof(PVRSRV_KERNEL_MEM_INFO *),
				  (void *)ppsKernelMemInfos,
				  0);
	}

	if(ret == 0 && eError == PVRSRV_OK)
	{
		COMMIT_HANDLE_BATCH_OR_ERROR(psSGXAddSharedPBDescOUT->eError, psPerProc);
	}

	psSGXAddSharedPBDescOUT->eError = eError;

	return ret;
}

static int
SGXGetInfoForSrvinitBW(u32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_SGXINFO_FOR_SRVINIT *psSGXInfoForSrvinitIN,
					   PVRSRV_BRIDGE_OUT_SGXINFO_FOR_SRVINIT *psSGXInfoForSrvinitOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	void * hDevCookieInt;
	u32 i;
	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGXINFO_FOR_SRVINIT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXInfoForSrvinitOUT->eError, psPerProc, PVRSRV_MAX_CLIENT_HEAPS);

	if(!psPerProc->bInitProcess)
	{
		psSGXInfoForSrvinitOUT->eError = PVRSRV_ERROR_GENERIC;
		return 0;
	}

	psSGXInfoForSrvinitOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psSGXInfoForSrvinitIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psSGXInfoForSrvinitOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXInfoForSrvinitOUT->eError =
		SGXGetInfoForSrvinitKM(hDevCookieInt,
							   &psSGXInfoForSrvinitOUT->sInitInfo);

	if(psSGXInfoForSrvinitOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	for(i = 0; i < PVRSRV_MAX_CLIENT_HEAPS; i++)
	{
		PVRSRV_HEAP_INFO *psHeapInfo;

		psHeapInfo = &psSGXInfoForSrvinitOUT->sInitInfo.asHeapInfo[i];

		if (psHeapInfo->ui32HeapID != (u32)SGX_UNDEFINED_HEAP_ID)
		{
			void * hDevMemHeapExt;

			if (psHeapInfo->hDevMemHeap != NULL)
			{

				PVRSRVAllocHandleNR(psPerProc->psHandleBase,
								  &hDevMemHeapExt,
								  psHeapInfo->hDevMemHeap,
								  PVRSRV_HANDLE_TYPE_DEV_MEM_HEAP,
								  PVRSRV_HANDLE_ALLOC_FLAG_SHARED);
				psHeapInfo->hDevMemHeap = hDevMemHeapExt;
			}
		}
	}

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXInfoForSrvinitOUT->eError, psPerProc);

	return 0;
}

#if defined(PDUMP)
static void
DumpBufferArray(PVRSRV_PER_PROCESS_DATA *psPerProc,
				PSGX_KICKTA_DUMP_BUFFER	psBufferArray,
				u32						ui32BufferArrayLength,
				int						bDumpPolls)
{
	u32	i;

	for (i=0; i<ui32BufferArrayLength; i++)
	{
		PSGX_KICKTA_DUMP_BUFFER	psBuffer;
		PVRSRV_KERNEL_MEM_INFO 	*psCtrlMemInfoKM;
		char * pszName;
		void * hUniqueTag;
		u32	ui32Offset;

		psBuffer = &psBufferArray[i];
		pszName = psBuffer->pszName;
		if (!pszName)
		{
			pszName = "Nameless buffer";
		}

		hUniqueTag = MAKEUNIQUETAG((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hKernelMemInfo);

	#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
		psCtrlMemInfoKM	= ((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hCtrlKernelMemInfo);
		ui32Offset =  psBuffer->sCtrlDevVAddr.uiAddr - psCtrlMemInfoKM->sDevVAddr.uiAddr;
	#else
		psCtrlMemInfoKM = ((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hKernelMemInfo)->psKernelSyncInfo->psSyncDataMemInfoKM;
		ui32Offset = offsetof(PVRSRV_SYNC_DATA, ui32ReadOpsComplete);
	#endif

		if (psBuffer->ui32Start <= psBuffer->ui32End)
		{
			if (bDumpPolls)
			{
				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 psBuffer->ui32Start,
						 psBuffer->ui32SpaceUsed,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}

			PDUMPCOMMENTWITHFLAGS(0, "%s\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 psBuffer->ui32Start,
					 psBuffer->ui32End - psBuffer->ui32Start,
					 0,
					 hUniqueTag);
		}
		else
		{


			if (bDumpPolls)
			{
				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 psBuffer->ui32Start,
						 psBuffer->ui32BackEndLength,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}
			PDUMPCOMMENTWITHFLAGS(0, "%s (part 1)\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 psBuffer->ui32Start,
					 psBuffer->ui32BackEndLength,
					 0,
					 hUniqueTag);

			if (bDumpPolls)
			{
				PDUMPMEMPOL(psCtrlMemInfoKM,
							ui32Offset,
							0,
							0xFFFFFFFF,
							PDUMP_POLL_OPERATOR_NOTEQUAL,
							0,
							MAKEUNIQUETAG(psCtrlMemInfoKM));

				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 0,
						 psBuffer->ui32End,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}
			PDUMPCOMMENTWITHFLAGS(0, "%s (part 2)\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 0,
					 psBuffer->ui32End,
					 0,
					 hUniqueTag);
		}
	}
}
static int
SGXPDumpBufferArrayBW(u32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_BUFFER_ARRAY *psPDumpBufferArrayIN,
				   void *psBridgeOut,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	u32 i;
	SGX_KICKTA_DUMP_BUFFER *psKickTADumpBuffer;
	u32 ui32BufferArrayLength =
		psPDumpBufferArrayIN->ui32BufferArrayLength;
	u32 ui32BufferArraySize =
		ui32BufferArrayLength * sizeof(SGX_KICKTA_DUMP_BUFFER);
	PVRSRV_ERROR eError = PVRSRV_ERROR_GENERIC;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY);

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32BufferArraySize,
				  (void * *)&psKickTADumpBuffer, 0,
				  "Array of Kick Tile Accelerator Dump Buffer") != PVRSRV_OK)
	{
		return -ENOMEM;
	}

	if(CopyFromUserWrapper(psPerProc,
			               ui32BridgeID,
						   psKickTADumpBuffer,
						   psPDumpBufferArrayIN->psBufferArray,
						   ui32BufferArraySize) != PVRSRV_OK)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32BufferArraySize, psKickTADumpBuffer, 0);

		return -EFAULT;
	}

	for(i = 0; i < ui32BufferArrayLength; i++)
	{
		void *pvMemInfo;

		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									&pvMemInfo,
									psKickTADumpBuffer[i].hKernelMemInfo,
									PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY: "
					 "PVRSRVLookupHandle failed (%d)", eError));
			break;
		}
		psKickTADumpBuffer[i].hKernelMemInfo = pvMemInfo;

#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									&pvMemInfo,
									psKickTADumpBuffer[i].hCtrlKernelMemInfo,
									PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY: "
					 "PVRSRVLookupHandle failed (%d)", eError));
			break;
		}
		psKickTADumpBuffer[i].hCtrlKernelMemInfo = pvMemInfo;
#endif
	}

	if(eError == PVRSRV_OK)
	{
		DumpBufferArray(psPerProc,
						psKickTADumpBuffer,
						ui32BufferArrayLength,
						psPDumpBufferArrayIN->bDumpPolls);
	}

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32BufferArraySize, psKickTADumpBuffer, 0);


	return 0;
}

static int
SGXPDump3DSignatureRegistersBW(u32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_3D_SIGNATURE_REGISTERS *psPDump3DSignatureRegistersIN,
				   PVRSRV_BRIDGE_RETURN *psRetOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	u32 ui32RegisterArraySize =  psPDump3DSignatureRegistersIN->ui32NumRegisters * sizeof(u32);
	u32 *pui32Registers = NULL;
#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	PVRSRV_SGXDEV_INFO	*psDevInfo = NULL;
	void * 	hDevCookieInt;
	u32	ui32RegVal = 0;
#endif
	int ret = -EFAULT;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_3D_SIGNATURE_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psPDump3DSignatureRegistersIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: hDevCookie lookup failed"));
		goto Exit;
	}

	psDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;


	ui32RegVal = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT);
#if defined(PDUMP)
	PDUMPREGWITHFLAGS(EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT,
						psPDump3DSignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
#endif

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (void * *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDump3DSignatureRegistersBW: OSAllocMem failed"));
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDump3DSignatureRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDump3DSignatureRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDump3DSignatureRegisters(psPDump3DSignatureRegistersIN->ui32DumpFrameNum,
					psPDump3DSignatureRegistersIN->bLastFrame,
					pui32Registers,
					psPDump3DSignatureRegistersIN->ui32NumRegisters);

ExitNoError:
	psRetOUT->eError = PVRSRV_OK;
	ret = 0;
Exit:
	if (pui32Registers != NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	if (psDevInfo != NULL)
	{
		OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, ui32RegVal);
#if defined(PDUMP)
		PDUMPREGWITHFLAGS(EUR_CR_MASTER_CORE, ui32RegVal,
							psPDump3DSignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
	}
#endif

	return ret;
}

static int
SGXPDumpCounterRegistersBW(u32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_COUNTER_REGISTERS *psPDumpCounterRegistersIN,
				   void *psBridgeOut,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	u32 ui32RegisterArraySize =  psPDumpCounterRegistersIN->ui32NumRegisters * sizeof(u32);
	u32 *pui32Registers = NULL;
	int ret = -EFAULT;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_COUNTER_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (void * *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpCounterRegistersBW: OSAllocMem failed"));
		ret = -ENOMEM;
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDumpCounterRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpCounterRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDumpCounterRegisters(psPDumpCounterRegistersIN->ui32DumpFrameNum,
					psPDumpCounterRegistersIN->bLastFrame,
					pui32Registers,
					psPDumpCounterRegistersIN->ui32NumRegisters);

ExitNoError:
	ret = 0;
Exit:
	if (pui32Registers != NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

	return ret;
}

static int
SGXPDumpTASignatureRegistersBW(u32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_TA_SIGNATURE_REGISTERS *psPDumpTASignatureRegistersIN,
				   PVRSRV_BRIDGE_RETURN *psRetOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	u32 ui32RegisterArraySize =  psPDumpTASignatureRegistersIN->ui32NumRegisters * sizeof(u32);
	u32 *pui32Registers = NULL;
#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	PVRSRV_SGXDEV_INFO	*psDevInfo = NULL;
	void * hDevCookieInt;
	u32	ui32RegVal = 0;
#endif
	int ret = -EFAULT;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_TA_SIGNATURE_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psPDumpTASignatureRegistersIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: hDevCookie lookup failed"));
		goto Exit;
	}

	psDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;


	ui32RegVal = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT);
#if defined(PDUMP)
	PDUMPREGWITHFLAGS(EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT,
						psPDumpTASignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
#endif

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (void * *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: OSAllocMem failed"));
		ret = -ENOMEM;
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDumpTASignatureRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDumpTASignatureRegisters(psPDumpTASignatureRegistersIN->ui32DumpFrameNum,
					psPDumpTASignatureRegistersIN->ui32TAKickCount,
					psPDumpTASignatureRegistersIN->bLastFrame,
					pui32Registers,
					psPDumpTASignatureRegistersIN->ui32NumRegisters);

ExitNoError:
	psRetOUT->eError = PVRSRV_OK;
	ret = 0;
Exit:
	if (pui32Registers != NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	if (psDevInfo != NULL)
	{
		OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, ui32RegVal);
#if defined(PDUMP)
		PDUMPREGWITHFLAGS(EUR_CR_MASTER_CORE, ui32RegVal,
							psPDumpTASignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
	}
#endif

	return ret;
}
static int
SGXPDumpHWPerfCBBW(u32						ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_HWPERFCB	*psPDumpHWPerfCBIN,
				   PVRSRV_BRIDGE_RETURN 			*psRetOUT,
				   PVRSRV_PER_PROCESS_DATA 			*psPerProc)
{
#if defined(SUPPORT_SGX_HWPERF)
#if defined(__linux__)
	PVRSRV_SGXDEV_INFO	*psDevInfo;
	void *			hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_HWPERFCB);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psPDumpHWPerfCBIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;

	PDumpHWPerfCBKM(&psPDumpHWPerfCBIN->szFileName[0],
					psPDumpHWPerfCBIN->ui32FileOffset,
					psDevInfo->psKernelHWPerfCBMemInfo->sDevVAddr,
					psDevInfo->psKernelHWPerfCBMemInfo->ui32AllocSize,
					psPDumpHWPerfCBIN->ui32PDumpFlags);

	return 0;
#else
	return 0;
#endif
#else
	return -EFAULT;
#endif
}

#endif


void SetSGXDispatchTableEntry(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETCLIENTINFO, SGXGetClientInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_RELEASECLIENTINFO, SGXReleaseClientInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETINTERNALDEVINFO, SGXGetInternalDevInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_DOKICK, SGXDoKickBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETPHYSPAGEADDR, DummyBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_READREGISTRYDWORD, DummyBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_2DQUERYBLTSCOMPLETE, SGX2DQueryBlitsCompleteBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETMMUPDADDR, DummyBW);

#if defined(TRANSFER_QUEUE)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SUBMITTRANSFER, SGXSubmitTransferBW);
#endif
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETMISCINFO, SGXGetMiscInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGXINFO_FOR_SRVINIT	, SGXGetInfoForSrvinitBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_DEVINITPART2, SGXDevInitPart2BW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC, SGXFindSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREFSHAREDPBDESC, SGXUnrefSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC, SGXAddSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_RENDER_CONTEXT, SGXRegisterHWRenderContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_FLUSH_HW_RENDER_TARGET, SGXFlushHWRenderTargetBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_RENDER_CONTEXT, SGXUnregisterHWRenderContextBW);
#if defined(SGX_FEATURE_2D_HARDWARE)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SUBMIT2D, SGXSubmit2DBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_2D_CONTEXT, SGXRegisterHW2DContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_2D_CONTEXT, SGXUnregisterHW2DContextBW);
#endif
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_TRANSFER_CONTEXT, SGXRegisterHWTransferContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_TRANSFER_CONTEXT, SGXUnregisterHWTransferContextBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SCHEDULE_PROCESS_QUEUES, SGXScheduleProcessQueuesBW);

#if defined(SUPPORT_SGX_HWPERF)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_READ_DIFF_COUNTERS, SGXReadDiffCountersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_READ_HWPERF_CB, SGXReadHWPerfCBBW);
#endif

#if defined(PDUMP)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY, SGXPDumpBufferArrayBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_3D_SIGNATURE_REGISTERS, SGXPDump3DSignatureRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_COUNTER_REGISTERS, SGXPDumpCounterRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_TA_SIGNATURE_REGISTERS, SGXPDumpTASignatureRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_HWPERFCB, SGXPDumpHWPerfCBBW);
#endif
}

#endif

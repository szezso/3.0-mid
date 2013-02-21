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

#include "services_headers.h"
#include "buffer_manager.h"
#include "handle.h"
#include "perproc.h"
#include "pdump_km.h"
#include "ra.h"

#include "pvrversion.h"

#include "lists.h"

#ifdef INTEL_D3_CHANGES

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/jiffies.h>

#define GFX_MS_TO_JIFFIES(time_ms) msecs_to_jiffies((time_ms))
#define WAIT_FOR_WRITE_OP_SYNC_TIMEOUT 10000

DECLARE_WAIT_QUEUE_HEAD(render_wait_queue);

static void WakeWriteOpSyncs(void)
{
	wake_up(&render_wait_queue);
}

PVRSRV_ERROR PVRSRVWaitForWriteOpSyncKM(PVRSRV_KERNEL_SYNC_INFO *
					psKernelSyncInfo)
{
	int rc = 0;

	if (NULL == psKernelSyncInfo) {
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	rc = wait_event_interruptible_timeout(render_wait_queue,
					      (psKernelSyncInfo->psSyncData->
					       ui32WriteOpsComplete >=
					       psKernelSyncInfo->psSyncData->
					       ui32WriteOpsPending),
					      GFX_MS_TO_JIFFIES
					      (WAIT_FOR_WRITE_OP_SYNC_TIMEOUT));

	if (rc == 0) {
		return PVRSRV_ERROR_TIMEOUT;
	} else if (rc == -ERESTARTSYS) {
		return PVRSRV_ERROR_RETRY;
	}

	return PVRSRV_OK;
}

#endif

DECLARE_LIST_ANY_VA_2(BM_CONTEXT, PVRSRV_ERROR, PVRSRV_OK);

DECLARE_LIST_FOR_EACH_VA(BM_HEAP);

DECLARE_LIST_ANY_2(PVRSRV_DEVICE_NODE, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_ANY_VA(PVRSRV_DEVICE_NODE);
DECLARE_LIST_ANY_VA_2(PVRSRV_DEVICE_NODE, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_FOR_EACH_VA(PVRSRV_DEVICE_NODE);
DECLARE_LIST_FOR_EACH(PVRSRV_DEVICE_NODE);
DECLARE_LIST_INSERT(PVRSRV_DEVICE_NODE);
DECLARE_LIST_REMOVE(PVRSRV_DEVICE_NODE);

void *MatchDeviceKM_AnyVaCb(PVRSRV_DEVICE_NODE * psDeviceNode, va_list va);

PVRSRV_ERROR AllocateDeviceID(SYS_DATA * psSysData, u32 * pui32DevID)
{
	SYS_DEVICE_ID *psDeviceWalker;
	SYS_DEVICE_ID *psDeviceEnd;

	psDeviceWalker = &psSysData->sDeviceID[0];
	psDeviceEnd = psDeviceWalker + psSysData->ui32NumDevices;

	while (psDeviceWalker < psDeviceEnd) {
		if (!psDeviceWalker->bInUse) {
			psDeviceWalker->bInUse = 1;
			*pui32DevID = psDeviceWalker->uiID;
			return PVRSRV_OK;
		}
		psDeviceWalker++;
	}

	PVR_DPF((PVR_DBG_ERROR,
		 "AllocateDeviceID: No free and valid device IDs available!"));

	PVR_ASSERT(psDeviceWalker < psDeviceEnd);

	return PVRSRV_ERROR_GENERIC;
}

PVRSRV_ERROR FreeDeviceID(SYS_DATA * psSysData, u32 ui32DevID)
{
	SYS_DEVICE_ID *psDeviceWalker;
	SYS_DEVICE_ID *psDeviceEnd;

	psDeviceWalker = &psSysData->sDeviceID[0];
	psDeviceEnd = psDeviceWalker + psSysData->ui32NumDevices;

	while (psDeviceWalker < psDeviceEnd) {

		if ((psDeviceWalker->uiID == ui32DevID) &&
		    (psDeviceWalker->bInUse)
		    ) {
			psDeviceWalker->bInUse = 0;
			return PVRSRV_OK;
		}
		psDeviceWalker++;
	}

	PVR_DPF((PVR_DBG_ERROR,
		 "FreeDeviceID: no matching dev ID that is in use!"));

	PVR_ASSERT(psDeviceWalker < psDeviceEnd);

	return PVRSRV_ERROR_GENERIC;
}

#ifndef ReadHWReg

u32 ReadHWReg(void *pvLinRegBaseAddr, u32 ui32Offset)
{
	return *(volatile u32 *)((u32) pvLinRegBaseAddr + ui32Offset);
}
#endif

#ifndef WriteHWReg

void WriteHWReg(void *pvLinRegBaseAddr, u32 ui32Offset, u32 ui32Value)
{
	PVR_DPF((PVR_DBG_MESSAGE, "WriteHWReg Base:%x, Offset: %x, Value %x",
		 pvLinRegBaseAddr, ui32Offset, ui32Value));

	*(u32 *) ((u32) pvLinRegBaseAddr + ui32Offset) = ui32Value;
}
#endif

#ifndef WriteHWRegs

void WriteHWRegs(void *pvLinRegBaseAddr, u32 ui32Count, PVRSRV_HWREG * psHWRegs)
{
	while (ui32Count) {
		WriteHWReg(pvLinRegBaseAddr, psHWRegs->ui32RegAddr,
			   psHWRegs->ui32RegVal);
		psHWRegs++;
		ui32Count--;
	}
}
#endif

void PVRSRVEnumerateDevicesKM_ForEachVaCb(PVRSRV_DEVICE_NODE * psDeviceNode,
					  va_list va)
{
	u32 *pui32DevCount;
	PVRSRV_DEVICE_IDENTIFIER **ppsDevIdList;

	pui32DevCount = va_arg(va, u32 *);
	ppsDevIdList = va_arg(va, PVRSRV_DEVICE_IDENTIFIER **);

	if (psDeviceNode->sDevId.eDeviceType != PVRSRV_DEVICE_TYPE_EXT) {
		*(*ppsDevIdList) = psDeviceNode->sDevId;
		(*ppsDevIdList)++;
		(*pui32DevCount)++;
	}
}

PVRSRV_ERROR PVRSRVEnumerateDevicesKM(u32 * pui32NumDevices,
				      PVRSRV_DEVICE_IDENTIFIER * psDevIdList)
{
	SYS_DATA *psSysData;
	u32 i;

	if (!pui32NumDevices || !psDevIdList) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVEnumerateDevicesKM: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	SysAcquireData(&psSysData);

	for (i = 0; i < PVRSRV_MAX_DEVICES; i++) {
		psDevIdList[i].eDeviceType = PVRSRV_DEVICE_TYPE_UNKNOWN;
	}

	*pui32NumDevices = 0;

	List_PVRSRV_DEVICE_NODE_ForEach_va(psSysData->psDeviceNodeList,
					   PVRSRVEnumerateDevicesKM_ForEachVaCb,
					   pui32NumDevices, &psDevIdList);

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVInit(PSYS_DATA psSysData)
{
	PVRSRV_ERROR eError;

	eError = ResManInit();
	if (eError != PVRSRV_OK) {
		goto Error;
	}

	eError = PVRSRVPerProcessDataInit();
	if (eError != PVRSRV_OK) {
		goto Error;
	}

	eError = PVRSRVHandleInit();
	if (eError != PVRSRV_OK) {
		goto Error;
	}

	eError = OSCreateResource(&psSysData->sPowerStateChangeResource);
	if (eError != PVRSRV_OK) {
		goto Error;
	}

	psSysData->eCurrentPowerState = PVRSRV_SYS_POWER_STATE_D0;
	psSysData->eFailedPowerState = PVRSRV_SYS_POWER_STATE_Unspecified;

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(PVRSRV_EVENTOBJECT),
		       (void **)&psSysData->psGlobalEventObject, 0,
		       "Event Object") != PVRSRV_OK) {

		goto Error;
	}

	if (OSEventObjectCreate
	    ("PVRSRV_GLOBAL_EVENTOBJECT",
	     psSysData->psGlobalEventObject) != PVRSRV_OK) {
		goto Error;
	}

	return eError;

Error:
	PVRSRVDeInit(psSysData);
	return eError;
}

void PVRSRVDeInit(PSYS_DATA psSysData)
{
	PVRSRV_ERROR eError;

	if (psSysData == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeInit: PVRSRVHandleDeInit failed - invalid param"));
		return;
	}

	if (psSysData->psGlobalEventObject) {
		OSEventObjectDestroy(psSysData->psGlobalEventObject);
		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(PVRSRV_EVENTOBJECT),
			  psSysData->psGlobalEventObject, 0);
		psSysData->psGlobalEventObject = NULL;
	}

	eError = PVRSRVHandleDeInit();
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeInit: PVRSRVHandleDeInit failed"));
	}

	eError = PVRSRVPerProcessDataDeInit();
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeInit: PVRSRVPerProcessDataDeInit failed"));
	}

	ResManDeInit();
}

PVRSRV_ERROR PVRSRVRegisterDevice(PSYS_DATA psSysData,
				  PVRSRV_ERROR(*pfnRegisterDevice)
				  (PVRSRV_DEVICE_NODE *),
				  u32 ui32SOCInterruptBit,
				  u32 * pui32DeviceIndex)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode;

	if (OSAllocMem(PVRSRV_OS_NON_PAGEABLE_HEAP,
		       sizeof(PVRSRV_DEVICE_NODE),
		       (void **)&psDeviceNode, NULL,
		       "Device Node") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVRegisterDevice : Failed to alloc memory for psDeviceNode"));
		return (PVRSRV_ERROR_OUT_OF_MEMORY);
	}
	memset(psDeviceNode, 0, sizeof(PVRSRV_DEVICE_NODE));

	eError = pfnRegisterDevice(psDeviceNode);
	if (eError != PVRSRV_OK) {
		OSFreeMem(PVRSRV_OS_NON_PAGEABLE_HEAP,
			  sizeof(PVRSRV_DEVICE_NODE), psDeviceNode, NULL);

		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVRegisterDevice : Failed to register device"));
		return (PVRSRV_ERROR_DEVICE_REGISTER_FAILED);
	}

	psDeviceNode->ui32RefCount = 1;
	psDeviceNode->psSysData = psSysData;
	psDeviceNode->ui32SOCInterruptBit = ui32SOCInterruptBit;

	AllocateDeviceID(psSysData, &psDeviceNode->sDevId.ui32DeviceIndex);

	List_PVRSRV_DEVICE_NODE_Insert(&psSysData->psDeviceNodeList,
				       psDeviceNode);

	*pui32DeviceIndex = psDeviceNode->sDevId.ui32DeviceIndex;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVInitialiseDevice(u32 ui32DevIndex)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVInitialiseDevice"));

	SysAcquireData(&psSysData);

	psDeviceNode = (PVRSRV_DEVICE_NODE *)
	    List_PVRSRV_DEVICE_NODE_Any_va(psSysData->psDeviceNodeList,
					   MatchDeviceKM_AnyVaCb,
					   ui32DevIndex, 1);
	if (!psDeviceNode) {

		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVInitialiseDevice: requested device is not present"));
		return PVRSRV_ERROR_INIT_FAILURE;
	}
	PVR_ASSERT(psDeviceNode->ui32RefCount > 0);

	eError = PVRSRVResManConnect(NULL, &psDeviceNode->hResManContext);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVInitialiseDevice: Failed PVRSRVResManConnect call"));
		return eError;
	}

	if (psDeviceNode->pfnInitDevice != NULL) {
		eError = psDeviceNode->pfnInitDevice(psDeviceNode);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVInitialiseDevice: Failed InitDevice call"));
			return eError;
		}
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVFinaliseSystem_SetPowerState_AnyCb(PVRSRV_DEVICE_NODE *
						      psDeviceNode)
{
	PVRSRV_ERROR eError;
	eError =
	    PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRV_DEV_POWER_STATE_DEFAULT,
					KERNEL_ID, 0);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVFinaliseSystem: Failed PVRSRVSetDevicePowerStateKM call (device index: %d)",
			 psDeviceNode->sDevId.ui32DeviceIndex));
	}
	return eError;
}

PVRSRV_ERROR PVRSRVFinaliseSystem_CompatCheck_AnyCb(PVRSRV_DEVICE_NODE *
						    psDeviceNode)
{
	PVRSRV_ERROR eError;
	eError = PVRSRVDevInitCompatCheck(psDeviceNode);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVFinaliseSystem: Failed PVRSRVDevInitCompatCheck call (device index: %d)",
			 psDeviceNode->sDevId.ui32DeviceIndex));
	}
	return eError;
}

PVRSRV_ERROR PVRSRVFinaliseSystem(int bInitSuccessful)
{
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVFinaliseSystem"));

	SysAcquireData(&psSysData);

	if (bInitSuccessful) {
		eError = SysFinalise();
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVFinaliseSystem: SysFinalise failed (%d)",
				 eError));
			return eError;
		}

		eError =
		    List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(psSysData->
							     psDeviceNodeList,
							     PVRSRVFinaliseSystem_SetPowerState_AnyCb);
		if (eError != PVRSRV_OK) {
			return eError;
		}

		eError =
		    List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(psSysData->
							     psDeviceNodeList,
							     PVRSRVFinaliseSystem_CompatCheck_AnyCb);
		if (eError != PVRSRV_OK) {
			return eError;
		}
	}

#if !defined(SUPPORT_PDUMP_DELAYED_INITPHASE_TERMINATION)
	PDUMPENDINITPHASE();
#endif

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVDevInitCompatCheck(PVRSRV_DEVICE_NODE * psDeviceNode)
{

	if (psDeviceNode->pfnInitDeviceCompatCheck)
		return psDeviceNode->pfnInitDeviceCompatCheck(psDeviceNode);
	else
		return PVRSRV_OK;
}

void *PVRSRVAcquireDeviceDataKM_Match_AnyVaCb(PVRSRV_DEVICE_NODE * psDeviceNode,
					      va_list va)
{
	PVRSRV_DEVICE_TYPE eDeviceType;
	u32 ui32DevIndex;

	eDeviceType = va_arg(va, PVRSRV_DEVICE_TYPE);
	ui32DevIndex = va_arg(va, u32);

	if ((eDeviceType != PVRSRV_DEVICE_TYPE_UNKNOWN &&
	     psDeviceNode->sDevId.eDeviceType == eDeviceType) ||
	    (eDeviceType == PVRSRV_DEVICE_TYPE_UNKNOWN &&
	     psDeviceNode->sDevId.ui32DeviceIndex == ui32DevIndex)) {
		return psDeviceNode;
	} else {
		return NULL;
	}
}

PVRSRV_ERROR PVRSRVAcquireDeviceDataKM(u32 ui32DevIndex,
				       PVRSRV_DEVICE_TYPE eDeviceType,
				       void **phDevCookie)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;

	PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVAcquireDeviceDataKM"));

	SysAcquireData(&psSysData);

	psDeviceNode =
	    List_PVRSRV_DEVICE_NODE_Any_va(psSysData->psDeviceNodeList,
					   PVRSRVAcquireDeviceDataKM_Match_AnyVaCb,
					   eDeviceType, ui32DevIndex);

	if (!psDeviceNode) {

		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVAcquireDeviceDataKM: requested device is not present"));
		return PVRSRV_ERROR_INIT_FAILURE;
	}

	PVR_ASSERT(psDeviceNode->ui32RefCount > 0);

	if (phDevCookie) {
		*phDevCookie = (void *)psDeviceNode;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVDeinitialiseDevice(u32 ui32DevIndex)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	SysAcquireData(&psSysData);

	psDeviceNode = (PVRSRV_DEVICE_NODE *)
	    List_PVRSRV_DEVICE_NODE_Any_va(psSysData->psDeviceNodeList,
					   MatchDeviceKM_AnyVaCb,
					   ui32DevIndex, 1);

	if (!psDeviceNode) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeinitialiseDevice: requested device %d is not present",
			 ui32DevIndex));
		return PVRSRV_ERROR_GENERIC;
	}

	eError = PVRSRVSetDevicePowerStateKM(ui32DevIndex,
					     PVRSRV_DEV_POWER_STATE_OFF,
					     KERNEL_ID, 0);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeinitialiseDevice: Failed PVRSRVSetDevicePowerStateKM call"));
		return eError;
	}

	eError = ResManFreeResByCriteria(psDeviceNode->hResManContext,
					 RESMAN_CRITERIA_RESTYPE,
					 RESMAN_TYPE_DEVICEMEM_ALLOCATION,
					 NULL, 0);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVDeinitialiseDevice: Failed ResManFreeResByCriteria call"));
		return eError;
	}

	if (psDeviceNode->pfnDeInitDevice != NULL) {
		eError = psDeviceNode->pfnDeInitDevice(psDeviceNode);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVDeinitialiseDevice: Failed DeInitDevice call"));
			return eError;
		}
	}

	PVRSRVResManDisconnect(psDeviceNode->hResManContext, 1);
	psDeviceNode->hResManContext = NULL;

	List_PVRSRV_DEVICE_NODE_Remove(psDeviceNode);

	(void)FreeDeviceID(psSysData, ui32DevIndex);
	OSFreeMem(PVRSRV_OS_NON_PAGEABLE_HEAP,
		  sizeof(PVRSRV_DEVICE_NODE), psDeviceNode, NULL);

	return (PVRSRV_OK);
}

PVRSRV_ERROR PollForValueKM(volatile u32 * pui32LinMemAddr,
			    u32 ui32Value,
			    u32 ui32Mask, u32 ui32Waitus, u32 ui32Tries)
{
	{
		u32 uiMaxTime = ui32Tries * ui32Waitus;

		LOOP_UNTIL_TIMEOUT(uiMaxTime) {
			if ((*pui32LinMemAddr & ui32Mask) == ui32Value) {
				return PVRSRV_OK;
			}
			OSWaitus(ui32Waitus);
		}
		END_LOOP_UNTIL_TIMEOUT();
	}

	return PVRSRV_ERROR_GENERIC;
}

#if defined (USING_ISR_INTERRUPTS)

extern u32 gui32EventStatusServicesByISR;

PVRSRV_ERROR PollForInterruptKM(u32 ui32Value,
				u32 ui32Mask, u32 ui32Waitus, u32 ui32Tries)
{
	u32 uiMaxTime;

	uiMaxTime = ui32Tries * ui32Waitus;

	LOOP_UNTIL_TIMEOUT(uiMaxTime) {
		if ((gui32EventStatusServicesByISR & ui32Mask) == ui32Value) {
			gui32EventStatusServicesByISR = 0;
			return PVRSRV_OK;
		}
		OSWaitus(ui32Waitus);
	}
	END_LOOP_UNTIL_TIMEOUT();

	return PVRSRV_ERROR_GENERIC;
}
#endif

void PVRSRVGetMiscInfoKM_RA_GetStats_ForEachVaCb(BM_HEAP * psBMHeap, va_list va)
{
	char **ppszStr;
	u32 *pui32StrLen;

	ppszStr = va_arg(va, char **);
	pui32StrLen = va_arg(va, u32 *);

	if (psBMHeap->pImportArena) {
		RA_GetStats(psBMHeap->pImportArena, ppszStr, pui32StrLen);
	}

	if (psBMHeap->pVMArena) {
		RA_GetStats(psBMHeap->pVMArena, ppszStr, pui32StrLen);
	}
}

PVRSRV_ERROR PVRSRVGetMiscInfoKM_BMContext_AnyVaCb(BM_CONTEXT * psBMContext,
						   va_list va)
{

	u32 *pui32StrLen;
	s32 *pi32Count;
	char **ppszStr;

	pui32StrLen = va_arg(va, u32 *);
	pi32Count = va_arg(va, s32 *);
	ppszStr = va_arg(va, char **);

	CHECK_SPACE(*pui32StrLen);
	*pi32Count =
	    snprintf(*ppszStr, 100,
		     "\nApplication Context (hDevMemContext) 0x%p:\n",
		     (void *)psBMContext);
	UPDATE_SPACE(*ppszStr, *pi32Count, *pui32StrLen);

	List_BM_HEAP_ForEach_va(psBMContext->psBMHeap,
				PVRSRVGetMiscInfoKM_RA_GetStats_ForEachVaCb,
				ppszStr, pui32StrLen);
	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVGetMiscInfoKM_Device_AnyVaCb(PVRSRV_DEVICE_NODE *
						psDeviceNode, va_list va)
{
	u32 *pui32StrLen;
	s32 *pi32Count;
	char **ppszStr;

	pui32StrLen = va_arg(va, u32 *);
	pi32Count = va_arg(va, s32 *);
	ppszStr = va_arg(va, char **);

	CHECK_SPACE(*pui32StrLen);
	*pi32Count =
	    snprintf(*ppszStr, 100, "\n\nDevice Type %d:\n",
		     psDeviceNode->sDevId.eDeviceType);
	UPDATE_SPACE(*ppszStr, *pi32Count, *pui32StrLen);

	if (psDeviceNode->sDevMemoryInfo.pBMKernelContext) {
		CHECK_SPACE(*pui32StrLen);
		*pi32Count = snprintf(*ppszStr, 100, "\nKernel Context:\n");
		UPDATE_SPACE(*ppszStr, *pi32Count, *pui32StrLen);

		List_BM_HEAP_ForEach_va(psDeviceNode->sDevMemoryInfo.
					pBMKernelContext->psBMHeap,
					PVRSRVGetMiscInfoKM_RA_GetStats_ForEachVaCb,
					ppszStr, pui32StrLen);
	}

	return List_BM_CONTEXT_PVRSRV_ERROR_Any_va(psDeviceNode->sDevMemoryInfo.
						   pBMContext,
						   PVRSRVGetMiscInfoKM_BMContext_AnyVaCb,
						   pui32StrLen, pi32Count,
						   ppszStr);
}

PVRSRV_ERROR PVRSRVGetMiscInfoKM(PVRSRV_MISC_INFO * psMiscInfo)
{
	SYS_DATA *psSysData;

	if (!psMiscInfo) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetMiscInfoKM: invalid parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psMiscInfo->ui32StatePresent = 0;

	if (psMiscInfo->ui32StateRequest & ~(PVRSRV_MISC_INFO_TIMER_PRESENT
					     |
					     PVRSRV_MISC_INFO_CLOCKGATE_PRESENT
					     | PVRSRV_MISC_INFO_MEMSTATS_PRESENT
					     |
					     PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT
					     |
					     PVRSRV_MISC_INFO_DDKVERSION_PRESENT
					     |
					     PVRSRV_MISC_INFO_CPUCACHEFLUSH_PRESENT
					     | PVRSRV_MISC_INFO_RESET_PRESENT))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetMiscInfoKM: invalid state request flags"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	SysAcquireData(&psSysData);

	if (((psMiscInfo->ui32StateRequest & PVRSRV_MISC_INFO_TIMER_PRESENT) !=
	     0UL) && (psSysData->pvSOCTimerRegisterKM != NULL)) {
		psMiscInfo->ui32StatePresent |= PVRSRV_MISC_INFO_TIMER_PRESENT;
		psMiscInfo->pvSOCTimerRegisterKM =
		    psSysData->pvSOCTimerRegisterKM;
		psMiscInfo->hSOCTimerRegisterOSMemHandle =
		    psSysData->hSOCTimerRegisterOSMemHandle;
	} else {
		psMiscInfo->pvSOCTimerRegisterKM = NULL;
		psMiscInfo->hSOCTimerRegisterOSMemHandle = NULL;
	}

	if (((psMiscInfo->
	      ui32StateRequest & PVRSRV_MISC_INFO_CLOCKGATE_PRESENT) != 0UL)
	    && (psSysData->pvSOCClockGateRegsBase != NULL)) {
		psMiscInfo->ui32StatePresent |=
		    PVRSRV_MISC_INFO_CLOCKGATE_PRESENT;
		psMiscInfo->pvSOCClockGateRegs =
		    psSysData->pvSOCClockGateRegsBase;
		psMiscInfo->ui32SOCClockGateRegsSize =
		    psSysData->ui32SOCClockGateRegsSize;
	}

	if (((psMiscInfo->
	      ui32StateRequest & PVRSRV_MISC_INFO_MEMSTATS_PRESENT) != 0UL)
	    && (psMiscInfo->pszMemoryStr != NULL)) {
		RA_ARENA **ppArena;
		char *pszStr;
		u32 ui32StrLen;
		s32 i32Count;

		pszStr = psMiscInfo->pszMemoryStr;
		ui32StrLen = psMiscInfo->ui32MemoryStrLen;

		psMiscInfo->ui32StatePresent |=
		    PVRSRV_MISC_INFO_MEMSTATS_PRESENT;

		ppArena = &psSysData->apsLocalDevMemArena[0];
		while (*ppArena) {
			CHECK_SPACE(ui32StrLen);
			i32Count =
			    snprintf(pszStr, 100, "\nLocal Backing Store:\n");
			UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

			RA_GetStats(*ppArena, &pszStr, &ui32StrLen);

			ppArena++;
		}

		List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any_va(psSysData->
							    psDeviceNodeList,
							    PVRSRVGetMiscInfoKM_Device_AnyVaCb,
							    &ui32StrLen,
							    &i32Count, &pszStr);

		i32Count = snprintf(pszStr, 100, "\n");
		UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
	}

	if (((psMiscInfo->
	      ui32StateRequest & PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT) !=
	     0UL) && (psSysData->psGlobalEventObject != NULL)) {
		psMiscInfo->ui32StatePresent |=
		    PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT;
		psMiscInfo->sGlobalEventObject =
		    *psSysData->psGlobalEventObject;
	}

	if (((psMiscInfo->
	      ui32StateRequest & PVRSRV_MISC_INFO_DDKVERSION_PRESENT) != 0UL)
	    &&
	    ((psMiscInfo->
	      ui32StateRequest & PVRSRV_MISC_INFO_MEMSTATS_PRESENT) == 0UL)
	    && (psMiscInfo->pszMemoryStr != NULL)) {
		char *pszStr;
		u32 ui32StrLen;
		u32 ui32LenStrPerNum = 12;
		s32 i32Count;
		int i;
		psMiscInfo->ui32StatePresent |=
		    PVRSRV_MISC_INFO_DDKVERSION_PRESENT;

		psMiscInfo->aui32DDKVersion[0] = PVRVERSION_MAJ;
		psMiscInfo->aui32DDKVersion[1] = PVRVERSION_MIN;
		psMiscInfo->aui32DDKVersion[2] = PVRVERSION_BRANCH;
		psMiscInfo->aui32DDKVersion[3] = PVRVERSION_BUILD;

		pszStr = psMiscInfo->pszMemoryStr;
		ui32StrLen = psMiscInfo->ui32MemoryStrLen;

		for (i = 0; i < 4; i++) {
			if (ui32StrLen < ui32LenStrPerNum) {
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			i32Count =
			    snprintf(pszStr, ui32LenStrPerNum, "%d",
				     (s32) psMiscInfo->aui32DDKVersion[i]);
			UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
			if (i != 3) {
				i32Count = snprintf(pszStr, 2, ".");
				UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
			}
		}
	}
#if defined(SUPPORT_CPU_CACHED_BUFFERS)
	if ((psMiscInfo->
	     ui32StateRequest & PVRSRV_MISC_INFO_CPUCACHEFLUSH_PRESENT) !=
	    0UL) {
		if (psMiscInfo->bDeferCPUCacheFlush) {

			if (!psMiscInfo->bCPUCacheFlushAll) {

				PVR_DPF((PVR_DBG_MESSAGE,
					 "PVRSRVGetMiscInfoKM: don't support deferred range flushes"));
				PVR_DPF((PVR_DBG_MESSAGE,
					 "                     using deferred flush all instead"));
			}

			psSysData->bFlushAll = 1;
		} else {

			if (psMiscInfo->bCPUCacheFlushAll) {

				OSFlushCPUCacheKM();

				psSysData->bFlushAll = 0;
			} else {

				OSFlushCPUCacheRangeKM(psMiscInfo->
						       pvRangeAddrStart,
						       psMiscInfo->
						       pvRangeAddrEnd);
			}
		}
	}
#endif

#if defined(PVRSRV_RESET_ON_HWTIMEOUT)
	if ((psMiscInfo->ui32StateRequest & PVRSRV_MISC_INFO_RESET_PRESENT) !=
	    0UL) {
		PVR_LOG(("User requested OS reset"));
		OSPanic();
	}
#endif

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVGetFBStatsKM(u32 * pui32Total, u32 * pui32Available)
{
	u32 ui32Total = 0, i = 0;
	u32 ui32Available = 0;

	*pui32Total = 0;
	*pui32Available = 0;

	while (BM_ContiguousStatistics(i, &ui32Total, &ui32Available) == 1) {
		*pui32Total += ui32Total;
		*pui32Available += ui32Available;

		i++;
	}

	return PVRSRV_OK;
}

int PVRSRVDeviceLISR(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	SYS_DATA *psSysData;
	int bStatus = 0;
	u32 ui32InterruptSource;

	if (!psDeviceNode) {
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVDeviceLISR: Invalid params\n"));
		goto out;
	}
	psSysData = psDeviceNode->psSysData;

	ui32InterruptSource = SysGetInterruptSource(psSysData, psDeviceNode);
	if (ui32InterruptSource & psDeviceNode->ui32SOCInterruptBit) {
		if (psDeviceNode->pfnDeviceISR != NULL) {
			bStatus =
			    (*psDeviceNode->pfnDeviceISR) (psDeviceNode->
							   pvISRData);
		}

		SysClearInterrupts(psSysData,
				   psDeviceNode->ui32SOCInterruptBit);
	}

out:
	return bStatus;
}

void PVRSRVSystemLISR_ForEachVaCb(PVRSRV_DEVICE_NODE * psDeviceNode, va_list va)
{

	int *pbStatus;
	u32 *pui32InterruptSource;
	u32 *pui32ClearInterrupts;

	pbStatus = va_arg(va, int *);
	pui32InterruptSource = va_arg(va, u32 *);
	pui32ClearInterrupts = va_arg(va, u32 *);

	if (psDeviceNode->pfnDeviceISR != NULL) {
		if (*pui32InterruptSource & psDeviceNode->ui32SOCInterruptBit) {
			if ((*psDeviceNode->pfnDeviceISR) (psDeviceNode->
							   pvISRData)) {

				*pbStatus = 1;
			}

			*pui32ClearInterrupts |=
			    psDeviceNode->ui32SOCInterruptBit;
		}
	}
}

int PVRSRVSystemLISR(void *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	int bStatus = 0;
	u32 ui32InterruptSource;
	u32 ui32ClearInterrupts = 0;
	if (!psSysData) {
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVSystemLISR: Invalid params\n"));
	} else {

		ui32InterruptSource = SysGetInterruptSource(psSysData, NULL);

		if (ui32InterruptSource) {

			List_PVRSRV_DEVICE_NODE_ForEach_va(psSysData->
							   psDeviceNodeList,
							   PVRSRVSystemLISR_ForEachVaCb,
							   &bStatus,
							   &ui32InterruptSource,
							   &ui32ClearInterrupts);

			SysClearInterrupts(psSysData, ui32ClearInterrupts);
		}
	}
	return bStatus;
}

void PVRSRVMISR_ForEachCb(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	if (psDeviceNode->pfnDeviceMISR != NULL) {
		(*psDeviceNode->pfnDeviceMISR) (psDeviceNode->pvISRData);
	}
}

void PVRSRVMISR(void *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	if (!psSysData) {
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVMISR: Invalid params\n"));
		return;
	}

	List_PVRSRV_DEVICE_NODE_ForEach(psSysData->psDeviceNodeList,
					PVRSRVMISR_ForEachCb);

	if (PVRSRVProcessQueues(ISR_ID, 0) == PVRSRV_ERROR_PROCESSING_BLOCKED) {
		PVRSRVProcessQueues(ISR_ID, 0);
	}

	if (psSysData->psGlobalEventObject) {
		void *hOSEventKM = psSysData->psGlobalEventObject->hOSEventKM;
		if (hOSEventKM) {
			OSEventObjectSignal(hOSEventKM);
		}
	}
#ifdef INTEL_D3_CHANGES
	WakeWriteOpSyncs();
#endif
}

PVRSRV_ERROR PVRSRVProcessConnect(u32 ui32PID)
{
	return PVRSRVPerProcessDataConnect(ui32PID);
}

void PVRSRVProcessDisconnect(u32 ui32PID)
{
	PVRSRVPerProcessDataDisconnect(ui32PID);
}

PVRSRV_ERROR PVRSRVSaveRestoreLiveSegments(void *hArena, unsigned char *pbyBuffer,
					   u32 * puiBufSize, int bSave)
{
	u32 uiBytesSaved = 0;
	void *pvLocalMemCPUVAddr;
	RA_SEGMENT_DETAILS sSegDetails;

	if (hArena == NULL) {
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	sSegDetails.uiSize = 0;
	sSegDetails.sCpuPhyAddr.uiAddr = 0;
	sSegDetails.hSegment = 0;

	while (RA_GetNextLiveSegment(hArena, &sSegDetails)) {
		if (pbyBuffer == NULL) {

			uiBytesSaved +=
			    sizeof(sSegDetails.uiSize) + sSegDetails.uiSize;
		} else {
			if ((uiBytesSaved + sizeof(sSegDetails.uiSize) +
			     sSegDetails.uiSize) > *puiBufSize) {
				return (PVRSRV_ERROR_OUT_OF_MEMORY);
			}

			PVR_DPF((PVR_DBG_MESSAGE,
				 "PVRSRVSaveRestoreLiveSegments: Base %08x size %08x",
				 sSegDetails.sCpuPhyAddr.uiAddr,
				 sSegDetails.uiSize));

			pvLocalMemCPUVAddr =
			    OSMapPhysToLin(sSegDetails.sCpuPhyAddr,
					   sSegDetails.uiSize,
					   PVRSRV_HAP_KERNEL_ONLY |
					   PVRSRV_HAP_UNCACHED, NULL);
			if (pvLocalMemCPUVAddr == NULL) {
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVSaveRestoreLiveSegments: Failed to map local memory to host"));
				return (PVRSRV_ERROR_OUT_OF_MEMORY);
			}

			if (bSave) {

				memcpy(pbyBuffer, &sSegDetails.uiSize,
				       sizeof(sSegDetails.uiSize));
				pbyBuffer += sizeof(sSegDetails.uiSize);

				memcpy(pbyBuffer, pvLocalMemCPUVAddr,
				       sSegDetails.uiSize);
				pbyBuffer += sSegDetails.uiSize;
			} else {
				u32 uiSize;

				memcpy(&uiSize, pbyBuffer,
				       sizeof(sSegDetails.uiSize));

				if (uiSize != sSegDetails.uiSize) {
					PVR_DPF((PVR_DBG_ERROR,
						 "PVRSRVSaveRestoreLiveSegments: Segment size error"));
				} else {
					pbyBuffer += sizeof(sSegDetails.uiSize);

					memcpy(pvLocalMemCPUVAddr, pbyBuffer,
					       sSegDetails.uiSize);
					pbyBuffer += sSegDetails.uiSize;
				}
			}

			uiBytesSaved +=
			    sizeof(sSegDetails.uiSize) + sSegDetails.uiSize;

			OSUnMapPhysToLin(pvLocalMemCPUVAddr,
					 sSegDetails.uiSize,
					 PVRSRV_HAP_KERNEL_ONLY |
					 PVRSRV_HAP_UNCACHED, NULL);
		}
	}

	if (pbyBuffer == NULL) {
		*puiBufSize = uiBytesSaved;
	}

	return (PVRSRV_OK);
}

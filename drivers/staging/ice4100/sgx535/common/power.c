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

#include "services_headers.h"
#include "pdump_km.h"

#include "lists.h"

DECLARE_LIST_ANY_VA(PVRSRV_POWER_DEV);
DECLARE_LIST_ANY_VA_2(PVRSRV_POWER_DEV, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_INSERT(PVRSRV_POWER_DEV);
DECLARE_LIST_REMOVE(PVRSRV_POWER_DEV);

void *MatchPowerDeviceIndex_AnyVaCb(PVRSRV_POWER_DEV * psPowerDev, va_list va);

static int gbInitServerRunning = 0;
static int gbInitServerRan = 0;
static int gbInitSuccessful = 0;

PVRSRV_ERROR PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState,
				      int bState)
{

	switch (eInitServerState) {
	case PVRSRV_INIT_SERVER_RUNNING:
		gbInitServerRunning = bState;
		break;
	case PVRSRV_INIT_SERVER_RAN:
		gbInitServerRan = bState;
		break;
	case PVRSRV_INIT_SERVER_SUCCESSFUL:
		gbInitSuccessful = bState;
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVSetInitServerState : Unknown state %lx",
			 eInitServerState));
		return PVRSRV_ERROR_GENERIC;
	}

	return PVRSRV_OK;
}

int PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState)
{
	int bReturnVal;

	switch (eInitServerState) {
	case PVRSRV_INIT_SERVER_RUNNING:
		bReturnVal = gbInitServerRunning;
		break;
	case PVRSRV_INIT_SERVER_RAN:
		bReturnVal = gbInitServerRan;
		break;
	case PVRSRV_INIT_SERVER_SUCCESSFUL:
		bReturnVal = gbInitSuccessful;
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetInitServerState : Unknown state %lx",
			 eInitServerState));
		bReturnVal = 0;
	}

	return bReturnVal;
}

static int _IsSystemStatePowered(PVRSRV_SYS_POWER_STATE eSystemPowerState)
{
	return (int)(eSystemPowerState < PVRSRV_SYS_POWER_STATE_D2);
}

PVRSRV_ERROR PVRSRVPowerLock(u32 ui32CallerID, int bSystemPowerEvent)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	u32 ui32Timeout = 1000000;

#if defined(SUPPORT_LMA)

	ui32Timeout *= 60;
#endif

	SysAcquireData(&psSysData);

#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
	eError = SysPowerLockWrap(psSysData);
	if (eError != PVRSRV_OK) {
		return eError;
	}
#endif
	do {
		eError = OSLockResource(&psSysData->sPowerStateChangeResource,
					ui32CallerID);
		if (eError == PVRSRV_OK) {
			break;
		} else if (ui32CallerID == ISR_ID) {

			eError = PVRSRV_ERROR_RETRY;
			break;
		}

		OSWaitus(1);
		ui32Timeout--;
	} while (ui32Timeout > 0);

#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
	if (eError != PVRSRV_OK) {
		SysPowerLockUnwrap(psSysData);
	}
#endif
	if ((eError == PVRSRV_OK) &&
	    !bSystemPowerEvent &&
	    !_IsSystemStatePowered(psSysData->eCurrentPowerState)) {

		PVRSRVPowerUnlock(ui32CallerID);
		eError = PVRSRV_ERROR_RETRY;
	}

	return eError;
}

void PVRSRVPowerUnlock(u32 ui32CallerID)
{
	OSUnlockResource(&gpsSysData->sPowerStateChangeResource, ui32CallerID);
#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
	SysPowerLockUnwrap(gpsSysData);
#endif
}

PVRSRV_ERROR PVRSRVDevicePrePowerStateKM_AnyVaCb(PVRSRV_POWER_DEV *
						 psPowerDevice, va_list va)
{
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;
	PVRSRV_ERROR eError;

	int bAllDevices;
	u32 ui32DeviceIndex;
	PVRSRV_DEV_POWER_STATE eNewPowerState;

	bAllDevices = va_arg(va, int);
	ui32DeviceIndex = va_arg(va, u32);
	eNewPowerState = va_arg(va, PVRSRV_DEV_POWER_STATE);

	if (bAllDevices || (ui32DeviceIndex == psPowerDevice->ui32DeviceIndex)) {
		eNewDevicePowerState =
		    (eNewPowerState ==
		     PVRSRV_DEV_POWER_STATE_DEFAULT) ? psPowerDevice->
		    eDefaultPowerState : eNewPowerState;

		if (psPowerDevice->eCurrentPowerState != eNewDevicePowerState) {
			if (psPowerDevice->pfnPrePower != NULL) {

				eError =
				    psPowerDevice->pfnPrePower(psPowerDevice->
							       hDevCookie,
							       eNewDevicePowerState,
							       psPowerDevice->
							       eCurrentPowerState);
				if (eError != PVRSRV_OK) {
					return eError;
				}
			}

			eError =
			    SysDevicePrePowerState(psPowerDevice->
						   ui32DeviceIndex,
						   eNewDevicePowerState,
						   psPowerDevice->
						   eCurrentPowerState);
			if (eError != PVRSRV_OK) {
				return eError;
			}
		}
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVDevicePrePowerStateKM(int bAllDevices,
					 u32 ui32DeviceIndex,
					 PVRSRV_DEV_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;

	SysAcquireData(&psSysData);

	eError =
	    List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(psSysData->
						      psPowerDeviceList,
						      PVRSRVDevicePrePowerStateKM_AnyVaCb,
						      bAllDevices,
						      ui32DeviceIndex,
						      eNewPowerState);

	return eError;
}

PVRSRV_ERROR PVRSRVDevicePostPowerStateKM_AnyVaCb(PVRSRV_POWER_DEV *
						  psPowerDevice, va_list va)
{
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;
	PVRSRV_ERROR eError;

	int bAllDevices;
	u32 ui32DeviceIndex;
	PVRSRV_DEV_POWER_STATE eNewPowerState;

	bAllDevices = va_arg(va, int);
	ui32DeviceIndex = va_arg(va, u32);
	eNewPowerState = va_arg(va, PVRSRV_DEV_POWER_STATE);

	if (bAllDevices || (ui32DeviceIndex == psPowerDevice->ui32DeviceIndex)) {
		eNewDevicePowerState =
		    (eNewPowerState ==
		     PVRSRV_DEV_POWER_STATE_DEFAULT) ? psPowerDevice->
		    eDefaultPowerState : eNewPowerState;

		if (psPowerDevice->eCurrentPowerState != eNewDevicePowerState) {

			eError =
			    SysDevicePostPowerState(psPowerDevice->
						    ui32DeviceIndex,
						    eNewDevicePowerState,
						    psPowerDevice->
						    eCurrentPowerState);
			if (eError != PVRSRV_OK) {
				return eError;
			}

			if (psPowerDevice->pfnPostPower != NULL) {

				eError =
				    psPowerDevice->pfnPostPower(psPowerDevice->
								hDevCookie,
								eNewDevicePowerState,
								psPowerDevice->
								eCurrentPowerState);
				if (eError != PVRSRV_OK) {
					return eError;
				}
			}

			psPowerDevice->eCurrentPowerState =
			    eNewDevicePowerState;
		}
	}
	return PVRSRV_OK;
}

static
PVRSRV_ERROR PVRSRVDevicePostPowerStateKM(int bAllDevices,
					  u32 ui32DeviceIndex,
					  PVRSRV_DEV_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;

	SysAcquireData(&psSysData);

	eError =
	    List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(psSysData->
						      psPowerDeviceList,
						      PVRSRVDevicePostPowerStateKM_AnyVaCb,
						      bAllDevices,
						      ui32DeviceIndex,
						      eNewPowerState);

	return eError;
}

PVRSRV_ERROR PVRSRVSetDevicePowerStateCoreKM(u32 ui32DeviceIndex,
					     PVRSRV_DEV_POWER_STATE
					     eNewPowerState)
{
	PVRSRV_ERROR eError;
	eError =
	    PVRSRVDevicePrePowerStateKM(0, ui32DeviceIndex, eNewPowerState);
	if (eError != PVRSRV_OK) {
		return eError;
	}

	eError =
	    PVRSRVDevicePostPowerStateKM(0, ui32DeviceIndex, eNewPowerState);
	return eError;
}

PVRSRV_ERROR PVRSRVSetDevicePowerStateKM(u32 ui32DeviceIndex,
					 PVRSRV_DEV_POWER_STATE eNewPowerState,
					 u32 ui32CallerID, int bRetainMutex)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;

	SysAcquireData(&psSysData);

	eError = PVRSRVPowerLock(ui32CallerID, 0);
	if (eError != PVRSRV_OK) {
		return eError;
	}
#if defined(PDUMP)
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_DEFAULT) {

		eError =
		    PVRSRVDevicePrePowerStateKM(0, ui32DeviceIndex,
						PVRSRV_DEV_POWER_STATE_ON);
		if (eError != PVRSRV_OK) {
			goto Exit;
		}

		eError =
		    PVRSRVDevicePostPowerStateKM(0, ui32DeviceIndex,
						 PVRSRV_DEV_POWER_STATE_ON);

		if (eError != PVRSRV_OK) {
			goto Exit;
		}

		PDUMPSUSPEND();
	}
#endif

	eError =
	    PVRSRVDevicePrePowerStateKM(0, ui32DeviceIndex, eNewPowerState);
	if (eError != PVRSRV_OK) {
		if (eNewPowerState == PVRSRV_DEV_POWER_STATE_DEFAULT) {
			PDUMPRESUME();
		}
		goto Exit;
	}

	eError =
	    PVRSRVDevicePostPowerStateKM(0, ui32DeviceIndex, eNewPowerState);

	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_DEFAULT) {
		PDUMPRESUME();
	}

Exit:

	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVSetDevicePowerStateKM : Transition to %d FAILED 0x%x",
			 eNewPowerState, eError));
	}

	if (!bRetainMutex || (eError != PVRSRV_OK)) {
		PVRSRVPowerUnlock(ui32CallerID);
	}

	return eError;
}

PVRSRV_ERROR PVRSRVSystemPrePowerStateKM(PVRSRV_SYS_POWER_STATE
					 eNewSysPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;

	SysAcquireData(&psSysData);

	eError = PVRSRVPowerLock(KERNEL_ID, 1);
	if (eError != PVRSRV_OK) {
		return eError;
	}

	if (_IsSystemStatePowered(eNewSysPowerState) !=
	    _IsSystemStatePowered(psSysData->eCurrentPowerState)) {
		if (_IsSystemStatePowered(eNewSysPowerState)) {

			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;
		} else {
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_OFF;
		}

		eError =
		    PVRSRVDevicePrePowerStateKM(1, 0, eNewDevicePowerState);
		if (eError != PVRSRV_OK) {
			goto ErrorExit;
		}
	}

	if (eNewSysPowerState != psSysData->eCurrentPowerState) {

		eError = SysSystemPrePowerState(eNewSysPowerState);
		if (eError != PVRSRV_OK) {
			goto ErrorExit;
		}
	}

	return eError;

ErrorExit:

	PVR_DPF((PVR_DBG_ERROR,
		 "PVRSRVSystemPrePowerStateKM: Transition from %d to %d FAILED 0x%x",
		 psSysData->eCurrentPowerState, eNewSysPowerState, eError));

	psSysData->eFailedPowerState = eNewSysPowerState;

	PVRSRVPowerUnlock(KERNEL_ID);

	return eError;
}

PVRSRV_ERROR PVRSRVSystemPostPowerStateKM(PVRSRV_SYS_POWER_STATE
					  eNewSysPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	SYS_DATA *psSysData;
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;

	SysAcquireData(&psSysData);

	if (eNewSysPowerState != psSysData->eCurrentPowerState) {

		eError = SysSystemPostPowerState(eNewSysPowerState);
		if (eError != PVRSRV_OK) {
			goto Exit;
		}
	}

	if (_IsSystemStatePowered(eNewSysPowerState) !=
	    _IsSystemStatePowered(psSysData->eCurrentPowerState)) {
		if (_IsSystemStatePowered(eNewSysPowerState)) {

			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;
		} else {
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_OFF;
		}

		eError =
		    PVRSRVDevicePostPowerStateKM(1, 0, eNewDevicePowerState);
		if (eError != PVRSRV_OK) {
			goto Exit;
		}
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "PVRSRVSystemPostPowerStateKM: System Power Transition from %d to %d OK",
		 psSysData->eCurrentPowerState, eNewSysPowerState));

	psSysData->eCurrentPowerState = eNewSysPowerState;

Exit:

	PVRSRVPowerUnlock(KERNEL_ID);

	if (_IsSystemStatePowered(eNewSysPowerState) &&
	    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL)) {

		PVRSRVCommandCompleteCallbacks();
	}

	return eError;
}

PVRSRV_ERROR PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;

	SysAcquireData(&psSysData);

	eError = PVRSRVSystemPrePowerStateKM(eNewSysPowerState);
	if (eError != PVRSRV_OK) {
		goto ErrorExit;
	}

	eError = PVRSRVSystemPostPowerStateKM(eNewSysPowerState);
	if (eError != PVRSRV_OK) {
		goto ErrorExit;
	}

	psSysData->eFailedPowerState = PVRSRV_SYS_POWER_STATE_Unspecified;

	return PVRSRV_OK;

ErrorExit:

	PVR_DPF((PVR_DBG_ERROR,
		 "PVRSRVSetPowerStateKM: Transition from %d to %d FAILED 0x%x",
		 psSysData->eCurrentPowerState, eNewSysPowerState, eError));

	psSysData->eFailedPowerState = eNewSysPowerState;

	return eError;
}

PVRSRV_ERROR PVRSRVRegisterPowerDevice(u32 ui32DeviceIndex,
				       PFN_PRE_POWER pfnPrePower,
				       PFN_POST_POWER pfnPostPower,
				       PFN_PRE_CLOCKSPEED_CHANGE
				       pfnPreClockSpeedChange,
				       PFN_POST_CLOCKSPEED_CHANGE
				       pfnPostClockSpeedChange,
				       void *hDevCookie,
				       PVRSRV_DEV_POWER_STATE
				       eCurrentPowerState,
				       PVRSRV_DEV_POWER_STATE
				       eDefaultPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	if (pfnPrePower == NULL && pfnPostPower == NULL) {
		return PVRSRVRemovePowerDevice(ui32DeviceIndex);
	}

	SysAcquireData(&psSysData);

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			    sizeof(PVRSRV_POWER_DEV),
			    (void **)&psPowerDevice, NULL, "Power Device");
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVRegisterPowerDevice: Failed to alloc PVRSRV_POWER_DEV"));
		return eError;
	}

	psPowerDevice->pfnPrePower = pfnPrePower;
	psPowerDevice->pfnPostPower = pfnPostPower;
	psPowerDevice->pfnPreClockSpeedChange = pfnPreClockSpeedChange;
	psPowerDevice->pfnPostClockSpeedChange = pfnPostClockSpeedChange;
	psPowerDevice->hDevCookie = hDevCookie;
	psPowerDevice->ui32DeviceIndex = ui32DeviceIndex;
	psPowerDevice->eCurrentPowerState = eCurrentPowerState;
	psPowerDevice->eDefaultPowerState = eDefaultPowerState;

	List_PVRSRV_POWER_DEV_Insert(&(psSysData->psPowerDeviceList),
				     psPowerDevice);

	return (PVRSRV_OK);
}

PVRSRV_ERROR PVRSRVRemovePowerDevice(u32 ui32DeviceIndex)
{
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDev;

	SysAcquireData(&psSysData);

	psPowerDev = (PVRSRV_POWER_DEV *)
	    List_PVRSRV_POWER_DEV_Any_va(psSysData->psPowerDeviceList,
					 MatchPowerDeviceIndex_AnyVaCb,
					 ui32DeviceIndex);

	if (psPowerDev) {
		List_PVRSRV_POWER_DEV_Remove(psPowerDev);
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(PVRSRV_POWER_DEV),
			  psPowerDev, NULL);

	}

	return (PVRSRV_OK);
}

int PVRSRVIsDevicePowered(u32 ui32DeviceIndex)
{
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	SysAcquireData(&psSysData);

	if (OSIsResourceLocked(&psSysData->sPowerStateChangeResource, KERNEL_ID)
	    || OSIsResourceLocked(&psSysData->sPowerStateChangeResource,
				  ISR_ID)) {
		return 0;
	}

	psPowerDevice = (PVRSRV_POWER_DEV *)
	    List_PVRSRV_POWER_DEV_Any_va(psSysData->psPowerDeviceList,
					 MatchPowerDeviceIndex_AnyVaCb,
					 ui32DeviceIndex);
	return (psPowerDevice
		&& (psPowerDevice->eCurrentPowerState ==
		    PVRSRV_DEV_POWER_STATE_ON))
	    ? 1 : 0;
}

PVRSRV_ERROR PVRSRVDevicePreClockSpeedChange(u32 ui32DeviceIndex,
					     int bIdleDevice, void *pvInfo)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	SysAcquireData(&psSysData);

	if (bIdleDevice) {

		eError = PVRSRVPowerLock(KERNEL_ID, 0);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVDevicePreClockSpeedChange : failed to acquire lock, error:0x%lx",
				 eError));
			return eError;
		}
	}

	psPowerDevice = (PVRSRV_POWER_DEV *)
	    List_PVRSRV_POWER_DEV_Any_va(psSysData->psPowerDeviceList,
					 MatchPowerDeviceIndex_AnyVaCb,
					 ui32DeviceIndex);

	if (psPowerDevice && psPowerDevice->pfnPostClockSpeedChange) {
		eError =
		    psPowerDevice->pfnPreClockSpeedChange(psPowerDevice->
							  hDevCookie,
							  bIdleDevice,
							  psPowerDevice->
							  eCurrentPowerState);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVDevicePreClockSpeedChange : Device %lu failed, error:0x%lx",
				 ui32DeviceIndex, eError));
		}
	}

	if (bIdleDevice && eError != PVRSRV_OK) {
		PVRSRVPowerUnlock(KERNEL_ID);
	}

	return eError;
}

void PVRSRVDevicePostClockSpeedChange(u32 ui32DeviceIndex,
				      int bIdleDevice, void *pvInfo)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	SysAcquireData(&psSysData);

	psPowerDevice = (PVRSRV_POWER_DEV *)
	    List_PVRSRV_POWER_DEV_Any_va(psSysData->psPowerDeviceList,
					 MatchPowerDeviceIndex_AnyVaCb,
					 ui32DeviceIndex);

	if (psPowerDevice && psPowerDevice->pfnPostClockSpeedChange) {
		eError =
		    psPowerDevice->pfnPostClockSpeedChange(psPowerDevice->
							   hDevCookie,
							   bIdleDevice,
							   psPowerDevice->
							   eCurrentPowerState);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVDevicePostClockSpeedChange : Device %lu failed, error:0x%lx",
				 ui32DeviceIndex, eError));
		}
	}

	if (bIdleDevice) {

		PVRSRVPowerUnlock(KERNEL_ID);
	}
}

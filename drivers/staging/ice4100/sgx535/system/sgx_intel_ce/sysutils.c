/**********************************************************************
 *
 * Copyright (c) 2008-2009 Intel Corporation.
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
#include "sysinfo.h"
#include "sgxinfo.h"
#include "syslocal.h"

PVRSRV_ERROR SysInitRegisters(void)
{
	SYS_DATA *psSysData;

	if (SysAcquireData(&psSysData) != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "SysInitRegisters: Failed to get SysData"));
		return PVRSRV_ERROR_GENERIC;
	}

	return PVRSRV_OK;
}

char *SysCreateVersionString(IMG_CPU_PHYADDR sRegRegion)
{
	static char aszVersionString[100];
	void *pvRegsLinAddr;
	SYS_DATA *psSysData;
	u32 ui32SGXRevision;
	s32 i32Count;

	pvRegsLinAddr = OSMapPhysToLin(sRegRegion,
				       SYS_SGX_REG_SIZE,
				       PVRSRV_HAP_UNCACHED |
				       PVRSRV_HAP_KERNEL_ONLY, NULL);
	if (!pvRegsLinAddr) {
		return NULL;
	}

	ui32SGXRevision =
	    OSReadHWReg((void *)((unsigned char *) pvRegsLinAddr +
				 SYS_SGX_REG_OFFSET), EUR_CR_CORE_REVISION);

	if (SysAcquireData(&psSysData) != PVRSRV_OK) {
		return NULL;
	}

	i32Count = snprintf(aszVersionString, 100,
			    "SGX revision = %u.%u.%u",
			    (u32) ((ui32SGXRevision &
				    EUR_CR_CORE_REVISION_MAJOR_MASK)
				   >> EUR_CR_CORE_REVISION_MAJOR_SHIFT),
			    (u32) ((ui32SGXRevision &
				    EUR_CR_CORE_REVISION_MINOR_MASK)
				   >> EUR_CR_CORE_REVISION_MINOR_SHIFT),
			    (u32) ((ui32SGXRevision &
				    EUR_CR_CORE_REVISION_MAINTENANCE_MASK)
				   >> EUR_CR_CORE_REVISION_MAINTENANCE_SHIFT)
	    );

	OSUnMapPhysToLin(pvRegsLinAddr,
			 SYS_SGX_REG_SIZE,
			 PVRSRV_HAP_UNCACHED | PVRSRV_HAP_KERNEL_ONLY, NULL);

	if (i32Count == -1) {
		return NULL;
	}

	return aszVersionString;
}

void SysResetSGX(void *pvRegsBaseKM)
{
}

void SysEnableInterrupts(SYS_DATA * psSysData)
{
}

void SysDisableInterrupts(SYS_DATA * psSysData)
{
}

u32 SysGetInterruptSource(SYS_DATA * psSysData,
			  PVRSRV_DEVICE_NODE * psDeviceNode)
{
	return 0x00000001;
}

void SysClearInterrupts(SYS_DATA * psSysData, u32 ui32InterruptBits)
{
}

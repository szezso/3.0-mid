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

#ifndef MEM_DEBUG_C
#define MEM_DEBUG_C

#if defined(PVRSRV_DEBUG_OS_MEMORY)

#include "img_types.h"
#include "services_headers.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define STOP_ON_ERROR 0

	int MemCheck(const void *pvAddr, const u8 ui8Pattern, u32 uSize) {
		u8 *pui8Addr;
		for (pui8Addr = (u8 *) pvAddr; uSize > 0; uSize--, pui8Addr++) {
			if (*pui8Addr != ui8Pattern) {
				return 0;
			}
		} return 1;
	}

	void OSCheckMemDebug(void *pvCpuVAddr, u32 uSize,
			     const char *pszFileName, const u32 uLine) {
		OSMEM_DEBUG_INFO const *psInfo =
		    (OSMEM_DEBUG_INFO *) ((u32) pvCpuVAddr -
					  TEST_BUFFER_PADDING_STATUS);

		if (pvCpuVAddr == NULL) {
			PVR_DPF((PVR_DBG_ERROR, "Pointer 0x%X : null pointer"
				 " - referenced %s:%d - allocated %s:%d",
				 pvCpuVAddr,
				 pszFileName, uLine,
				 psInfo->sFileName, psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		}

		if (((u32) pvCpuVAddr & 3) != 0) {
			PVR_DPF((PVR_DBG_ERROR,
				 "Pointer 0x%X : invalid alignment"
				 " - referenced %s:%d - allocated %s:%d",
				 pvCpuVAddr, pszFileName, uLine,
				 psInfo->sFileName, psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		}

		if (!MemCheck
		    ((void *)psInfo->sGuardRegionBefore, 0xB1,
		     sizeof(psInfo->sGuardRegionBefore))) {
			PVR_DPF((PVR_DBG_ERROR,
				 "Pointer 0x%X : guard region before overwritten"
				 " - referenced %s:%d - allocated %s:%d",
				 pvCpuVAddr, pszFileName, uLine,
				 psInfo->sFileName, psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		}

		if (uSize != psInfo->uSize) {
			PVR_DPF((PVR_DBG_WARNING,
				 "Pointer 0x%X : supplied size was different to stored size (0x%X != 0x%X)"
				 " - referenced %s:%d - allocated %s:%d",
				 pvCpuVAddr, uSize, psInfo->uSize, pszFileName,
				 uLine, psInfo->sFileName, psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		}

		if ((0x01234567 ^ psInfo->uSizeParityCheck) != psInfo->uSize) {
			PVR_DPF((PVR_DBG_WARNING,
				 "Pointer 0x%X : stored size parity error (0x%X != 0x%X)"
				 " - referenced %s:%d - allocated %s:%d",
				 pvCpuVAddr, psInfo->uSize,
				 0x01234567 ^ psInfo->uSizeParityCheck,
				 pszFileName, uLine, psInfo->sFileName,
				 psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		} else {

			uSize = psInfo->uSize;
		}

		if (uSize) {
			if (!MemCheck
			    ((void *)((u32) pvCpuVAddr + uSize), 0xB2,
			     TEST_BUFFER_PADDING_AFTER)) {
				PVR_DPF((PVR_DBG_ERROR,
					 "Pointer 0x%X : guard region after overwritten"
					 " - referenced from %s:%d - allocated from %s:%d",
					 pvCpuVAddr, pszFileName, uLine,
					 psInfo->sFileName, psInfo->uLineNo));
			}
		}

		if (psInfo->eValid != isAllocated) {
			PVR_DPF((PVR_DBG_ERROR,
				 "Pointer 0x%X : not allocated (freed? %d)"
				 " - referenced %s:%d - freed %s:%d",
				 pvCpuVAddr, psInfo->eValid == isFree,
				 pszFileName, uLine, psInfo->sFileName,
				 psInfo->uLineNo));
			while (STOP_ON_ERROR) ;
		}
	}

	void debug_strcpy(char *pDest, const char *pSrc) {
		u32 i = 0;

		for (; i < 128; i++) {
			*pDest = *pSrc;
			if (*pSrc == '\0')
				break;
			pDest++;
			pSrc++;
		}
	}

	PVRSRV_ERROR OSAllocMem_Debug_Wrapper(u32 ui32Flags,
					      u32 ui32Size,
					      void **ppvCpuVAddr,
					      void **phBlockAlloc,
					      char *pszFilename, u32 ui32Line) {
		OSMEM_DEBUG_INFO *psInfo;

		PVRSRV_ERROR eError;

		eError = OSAllocMem_Debug_Linux_Memory_Allocations(ui32Flags,
								   ui32Size +
								   TEST_BUFFER_PADDING,
								   ppvCpuVAddr,
								   phBlockAlloc,
								   pszFilename,
								   ui32Line);

		if (eError != PVRSRV_OK) {
			return eError;
		}

		memset((char *)(*ppvCpuVAddr) + TEST_BUFFER_PADDING_STATUS,
		       0xBB, ui32Size);
		memset((char *)(*ppvCpuVAddr) + ui32Size +
		       TEST_BUFFER_PADDING_STATUS, 0xB2,
		       TEST_BUFFER_PADDING_AFTER);

		psInfo = (OSMEM_DEBUG_INFO *) (*ppvCpuVAddr);

		memset(psInfo->sGuardRegionBefore, 0xB1,
		       sizeof(psInfo->sGuardRegionBefore));
		debug_strcpy(psInfo->sFileName, pszFilename);
		psInfo->uLineNo = ui32Line;
		psInfo->eValid = isAllocated;
		psInfo->uSize = ui32Size;
		psInfo->uSizeParityCheck = 0x01234567 ^ ui32Size;

		*ppvCpuVAddr =
		    (void *)((u32) * ppvCpuVAddr) + TEST_BUFFER_PADDING_STATUS;

#ifdef PVRSRV_LOG_MEMORY_ALLOCS

		PVR_TRACE(("Allocated pointer (after debug info): 0x%X from %s:%d", *ppvCpuVAddr, pszFilename, ui32Line));
#endif

		return PVRSRV_OK;
	}

	PVRSRV_ERROR OSFreeMem_Debug_Wrapper(u32 ui32Flags,
					     u32 ui32Size,
					     void *pvCpuVAddr,
					     void *hBlockAlloc,
					     char *pszFilename, u32 ui32Line) {
		OSMEM_DEBUG_INFO *psInfo;

		OSCheckMemDebug(pvCpuVAddr, ui32Size, pszFilename, ui32Line);

		memset(pvCpuVAddr, 0xBF, ui32Size + TEST_BUFFER_PADDING_AFTER);

		psInfo =
		    (OSMEM_DEBUG_INFO *) ((u32) pvCpuVAddr -
					  TEST_BUFFER_PADDING_STATUS);

		psInfo->uSize = 0;
		psInfo->uSizeParityCheck = 0;
		psInfo->eValid = isFree;
		psInfo->uLineNo = ui32Line;
		debug_strcpy(psInfo->sFileName, pszFilename);

		return OSFreeMem_Debug_Linux_Memory_Allocations(ui32Flags,
								ui32Size +
								TEST_BUFFER_PADDING,
								psInfo,
								hBlockAlloc,
								pszFilename,
								ui32Line);
	}

#if defined (__cplusplus)

}
#endif

#endif

#endif

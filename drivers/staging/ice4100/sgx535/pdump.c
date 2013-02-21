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

#if defined (SUPPORT_SGX)
#if defined (PDUMP)

#include <asm/atomic.h>
#include <stdarg.h>
#include "sgxdefs.h"
#include "services_headers.h"

#include "pvrversion.h"
#include "pvr_debug.h"

#include "dbgdrvif.h"
#include "sgxmmu.h"
#include "mm.h"
#include "pdump_km.h"

#include <linux/tty.h>

static int PDumpWriteString2(char *pszString, u32 ui32Flags);
static int PDumpWriteILock(PDBG_STREAM psStream, u8 * pui8Data, u32 ui32Count,
			   u32 ui32Flags);
static void DbgSetFrame(PDBG_STREAM psStream, u32 ui32Frame);
static u32 DbgGetFrame(PDBG_STREAM psStream);
static void DbgSetMarker(PDBG_STREAM psStream, u32 ui32Marker);
static u32 DbgWrite(PDBG_STREAM psStream, u8 * pui8Data, u32 ui32BCount,
		    u32 ui32Flags);

#define PDUMP_DATAMASTER_PIXEL		(1)
#define PDUMP_DATAMASTER_EDM		(3)

#define MIN(a,b)       (a > b ? b : a)

#define MAX_FILE_SIZE	0x40000000

static atomic_t gsPDumpSuspended = ATOMIC_INIT(0);

static PDBGKM_SERVICE_TABLE gpfnDbgDrv = NULL;

char *pszStreamName[PDUMP_NUM_STREAMS] = { "ParamStream2",
	"ScriptStream2",
	"DriverInfoStream"
};

typedef struct PDBG_PDUMP_STATE_TAG {
	PDBG_STREAM psStream[PDUMP_NUM_STREAMS];
	u32 ui32ParamFileNum;

	char *pszMsg;
	char *pszScript;
	char *pszFile;

} PDBG_PDUMP_STATE;

static PDBG_PDUMP_STATE gsDBGPdumpState = { {NULL}, 0, NULL, NULL, NULL };

#define SZ_MSG_SIZE_MAX			PVRSRV_PDUMP_MAX_COMMENT_SIZE-1
#define SZ_SCRIPT_SIZE_MAX		PVRSRV_PDUMP_MAX_COMMENT_SIZE-1
#define SZ_FILENAME_SIZE_MAX	PVRSRV_PDUMP_MAX_COMMENT_SIZE-1

void DBGDrvGetServiceTable(void **fn_table);

static inline int PDumpSuspended(void)
{
	return atomic_read(&gsPDumpSuspended) != 0;
}

PVRSRV_ERROR PDumpOSGetScriptString(void **phScript, u32 * pui32MaxLen)
{
	*phScript = (void *)gsDBGPdumpState.pszScript;
	*pui32MaxLen = SZ_SCRIPT_SIZE_MAX;
	if ((!*phScript) || PDumpSuspended()) {
		return PVRSRV_ERROR_GENERIC;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpOSGetMessageString(void **phMsg, u32 * pui32MaxLen)
{
	*phMsg = (void *)gsDBGPdumpState.pszMsg;
	*pui32MaxLen = SZ_MSG_SIZE_MAX;
	if ((!*phMsg) || PDumpSuspended()) {
		return PVRSRV_ERROR_GENERIC;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpOSGetFilenameString(char **ppszFile, u32 * pui32MaxLen)
{
	*ppszFile = gsDBGPdumpState.pszFile;
	*pui32MaxLen = SZ_FILENAME_SIZE_MAX;
	if ((!*ppszFile) || PDumpSuspended()) {
		return PVRSRV_ERROR_GENERIC;
	}
	return PVRSRV_OK;
}

int PDumpOSWriteString2(void *hScript, u32 ui32Flags)
{
	return PDumpWriteString2(hScript, ui32Flags);
}

PVRSRV_ERROR PDumpOSBufprintf(void *hBuf, u32 ui32ScriptSizeMax,
			      char *pszFormat, ...)
{
	char *pszBuf = hBuf;
	u32 n;
	va_list vaArgs;

	va_start(vaArgs, pszFormat);

	n = vsnprintf(pszBuf, ui32ScriptSizeMax, pszFormat, vaArgs);

	va_end(vaArgs);

	if (n >= ui32ScriptSizeMax || n == -1) {
		PVR_DPF((PVR_DBG_ERROR,
			 "Buffer overflow detected, pdump output may be incomplete."));

		return PVRSRV_ERROR_PDUMP_BUF_OVERFLOW;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpOSVSprintf(char *pszComment, u32 ui32ScriptSizeMax,
			     char *pszFormat, PDUMP_va_list vaArgs)
{
	u32 n;

	n = vsnprintf(pszComment, ui32ScriptSizeMax, pszFormat, vaArgs);

	if (n >= ui32ScriptSizeMax || n == -1) {
		PVR_DPF((PVR_DBG_ERROR,
			 "Buffer overflow detected, pdump output may be incomplete."));

		return PVRSRV_ERROR_PDUMP_BUF_OVERFLOW;
	}

	return PVRSRV_OK;
}

void PDumpOSDebugPrintf(char *pszFormat, ...)
{

}

PVRSRV_ERROR PDumpOSSprintf(char *pszComment, u32 ui32ScriptSizeMax,
			    char *pszFormat, ...)
{
	u32 n;
	va_list vaArgs;

	va_start(vaArgs, pszFormat);

	n = vsnprintf(pszComment, ui32ScriptSizeMax, pszFormat, vaArgs);

	va_end(vaArgs);

	if (n >= ui32ScriptSizeMax || n == -1) {
		PVR_DPF((PVR_DBG_ERROR,
			 "Buffer overflow detected, pdump output may be incomplete."));

		return PVRSRV_ERROR_PDUMP_BUF_OVERFLOW;
	}

	return PVRSRV_OK;
}

u32 PDumpOSBuflen(void *hBuffer, u32 ui32BufferSizeMax)
{
	char *pszBuf = hBuffer;
	u32 ui32Count = 0;

	while ((pszBuf[ui32Count] != 0) && (ui32Count < ui32BufferSizeMax)) {
		ui32Count++;
	}
	return (ui32Count);
}

void PDumpOSVerifyLineEnding(void *hBuffer, u32 ui32BufferSizeMax)
{
	u32 ui32Count = 0;
	char *pszBuf = hBuffer;

	ui32Count = PDumpOSBuflen(hBuffer, ui32BufferSizeMax);

	if ((ui32Count >= 1) && (pszBuf[ui32Count - 1] != '\n')
	    && (ui32Count < ui32BufferSizeMax)) {
		pszBuf[ui32Count] = '\n';
		ui32Count++;
		pszBuf[ui32Count] = '\0';
	}
	if ((ui32Count >= 2) && (pszBuf[ui32Count - 2] != '\r')
	    && (ui32Count < ui32BufferSizeMax)) {
		pszBuf[ui32Count - 1] = '\r';
		pszBuf[ui32Count] = '\n';
		ui32Count++;
		pszBuf[ui32Count] = '\0';
	}
}

void *PDumpOSGetStream(u32 ePDumpStream)
{
	return (void *)gsDBGPdumpState.psStream[ePDumpStream];
}

u32 PDumpOSGetStreamOffset(u32 ePDumpStream)
{
	PDBG_STREAM psStream = gsDBGPdumpState.psStream[ePDumpStream];
	return gpfnDbgDrv->pfnGetStreamOffset(psStream);
}

u32 PDumpOSGetParamFileNum(void)
{
	return gsDBGPdumpState.ui32ParamFileNum;
}

int PDumpOSWriteString(void *hStream,
		       u8 * psui8Data, u32 ui32Size, u32 ui32Flags)
{
	PDBG_STREAM psStream = (PDBG_STREAM) hStream;
	return PDumpWriteILock(psStream, psui8Data, ui32Size, ui32Flags);
}

void PDumpOSCheckForSplitting(void *hStream, u32 ui32Size, u32 ui32Flags)
{

}

int PDumpOSJTInitialised(void)
{
	if (gpfnDbgDrv) {
		return 1;
	}
	return 0;
}

inline int PDumpOSIsSuspended(void)
{
	return atomic_read(&gsPDumpSuspended) != 0;
}

void PDumpOSCPUVAddrToDevPAddr(PVRSRV_DEVICE_TYPE eDeviceType,
			       void *hOSMemHandle,
			       u32 ui32Offset,
			       u8 * pui8LinAddr,
			       u32 ui32PageSize, IMG_DEV_PHYADDR * psDevPAddr)
{
	if (hOSMemHandle) {

		IMG_CPU_PHYADDR sCpuPAddr;

		sCpuPAddr = OSMemHandleToCpuPAddr(hOSMemHandle, ui32Offset);
		PVR_ASSERT((sCpuPAddr.uiAddr & (ui32PageSize - 1)) == 0);

		*psDevPAddr = SysCpuPAddrToDevPAddr(eDeviceType, sCpuPAddr);
	} else {
		IMG_CPU_PHYADDR sCpuPAddr;

		sCpuPAddr = OSMapLinToCPUPhys(pui8LinAddr);
		*psDevPAddr = SysCpuPAddrToDevPAddr(eDeviceType, sCpuPAddr);
	}
}

void PDumpOSCPUVAddrToPhysPages(void *hOSMemHandle,
				u32 ui32Offset,
				u8 * pui8LinAddr, u32 * pui32PageOffset)
{
	if (hOSMemHandle) {

		IMG_CPU_PHYADDR sCpuPAddr;

		sCpuPAddr = OSMemHandleToCpuPAddr(hOSMemHandle, ui32Offset);
		*pui32PageOffset = sCpuPAddr.uiAddr & (HOST_PAGESIZE() - 1);
	} else {
		*pui32PageOffset = (u32) pui8LinAddr & (HOST_PAGESIZE() - 1);
	}
}

void PDumpInit(void)
{
	u32 i;

	if (!gpfnDbgDrv) {
		DBGDrvGetServiceTable((void **)&gpfnDbgDrv);

		if (gpfnDbgDrv == NULL) {
			return;
		}

		if (!gsDBGPdumpState.pszFile) {
			if (OSAllocMem
			    (PVRSRV_OS_PAGEABLE_HEAP, SZ_FILENAME_SIZE_MAX,
			     (void **)&gsDBGPdumpState.pszFile, 0,
			     "Filename string") != PVRSRV_OK) {
				goto init_failed;
			}
		}

		if (!gsDBGPdumpState.pszMsg) {
			if (OSAllocMem
			    (PVRSRV_OS_PAGEABLE_HEAP, SZ_MSG_SIZE_MAX,
			     (void **)&gsDBGPdumpState.pszMsg, 0,
			     "Message string") != PVRSRV_OK) {
				goto init_failed;
			}
		}

		if (!gsDBGPdumpState.pszScript) {
			if (OSAllocMem
			    (PVRSRV_OS_PAGEABLE_HEAP, SZ_SCRIPT_SIZE_MAX,
			     (void **)&gsDBGPdumpState.pszScript, 0,
			     "Script string") != PVRSRV_OK) {
				goto init_failed;
			}
		}

		for (i = 0; i < PDUMP_NUM_STREAMS; i++) {
			gsDBGPdumpState.psStream[i] =
			    gpfnDbgDrv->pfnCreateStream(pszStreamName[i],
							DEBUG_CAPMODE_FRAMED,
							DEBUG_OUTMODE_STREAMENABLE,
							0, 10);

			gpfnDbgDrv->pfnSetCaptureMode(gsDBGPdumpState.
						      psStream[i],
						      DEBUG_CAPMODE_FRAMED,
						      0xFFFFFFFF, 0xFFFFFFFF,
						      1);
			gpfnDbgDrv->pfnSetFrame(gsDBGPdumpState.psStream[i], 0);
		}

		PDUMPCOMMENT("Driver Product Name: %s", VS_PRODUCT_NAME);
		PDUMPCOMMENT("Driver Product Version: %s (%s)",
			     PVRVERSION_STRING, PVRVERSION_FILE);
		PDUMPCOMMENT("Start of Init Phase");
	}

	return;

init_failed:

	if (gsDBGPdumpState.pszFile) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_FILENAME_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszFile, 0);
		gsDBGPdumpState.pszFile = NULL;
	}

	if (gsDBGPdumpState.pszScript) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_SCRIPT_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszScript, 0);
		gsDBGPdumpState.pszScript = NULL;
	}

	if (gsDBGPdumpState.pszMsg) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_MSG_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszMsg, 0);
		gsDBGPdumpState.pszMsg = NULL;
	}

	gpfnDbgDrv = NULL;
}

void PDumpDeInit(void)
{
	u32 i;

	for (i = 0; i < PDUMP_NUM_STREAMS; i++) {
		gpfnDbgDrv->pfnDestroyStream(gsDBGPdumpState.psStream[i]);
	}

	if (gsDBGPdumpState.pszFile) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_FILENAME_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszFile, 0);
		gsDBGPdumpState.pszFile = NULL;
	}

	if (gsDBGPdumpState.pszScript) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_SCRIPT_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszScript, 0);
		gsDBGPdumpState.pszScript = NULL;
	}

	if (gsDBGPdumpState.pszMsg) {
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, SZ_MSG_SIZE_MAX,
			  (void *)gsDBGPdumpState.pszMsg, 0);
		gsDBGPdumpState.pszMsg = NULL;
	}

	gpfnDbgDrv = NULL;
}

PVRSRV_ERROR PDumpStartInitPhaseKM(void)
{
	u32 i;

	if (gpfnDbgDrv) {
		PDUMPCOMMENT("Start Init Phase");
		for (i = 0; i < PDUMP_NUM_STREAMS; i++) {
			gpfnDbgDrv->pfnStartInitPhase(gsDBGPdumpState.
						      psStream[i]);
		}
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpStopInitPhaseKM(void)
{
	u32 i;

	if (gpfnDbgDrv) {
		PDUMPCOMMENT("Stop Init Phase");

		for (i = 0; i < PDUMP_NUM_STREAMS; i++) {
			gpfnDbgDrv->pfnStopInitPhase(gsDBGPdumpState.
						     psStream[i]);
		}
	}
	return PVRSRV_OK;
}

int PDumpIsLastCaptureFrameKM(void)
{
	return gpfnDbgDrv->pfnIsLastCaptureFrame(gsDBGPdumpState.
						 psStream
						 [PDUMP_STREAM_SCRIPT2]);
}

int PDumpIsCaptureFrameKM(void)
{
	if (PDumpSuspended()) {
		return 0;
	}
	return gpfnDbgDrv->pfnIsCaptureFrame(gsDBGPdumpState.
					     psStream[PDUMP_STREAM_SCRIPT2], 0);
}

PVRSRV_ERROR PDumpSetFrameKM(u32 ui32Frame)
{
	u32 ui32Stream;

	for (ui32Stream = 0; ui32Stream < PDUMP_NUM_STREAMS; ui32Stream++) {
		if (gsDBGPdumpState.psStream[ui32Stream]) {
			DbgSetFrame(gsDBGPdumpState.psStream[ui32Stream],
				    ui32Frame);
		}
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpGetFrameKM(u32 * pui32Frame)
{
	*pui32Frame =
	    DbgGetFrame(gsDBGPdumpState.psStream[PDUMP_STREAM_SCRIPT2]);

	return PVRSRV_OK;
}

static int PDumpWriteString2(char *pszString, u32 ui32Flags)
{
	return PDumpWriteILock(gsDBGPdumpState.psStream[PDUMP_STREAM_SCRIPT2],
			       (u8 *) pszString, strlen(pszString), ui32Flags);
}

static int PDumpWriteILock(PDBG_STREAM psStream, u8 * pui8Data, u32 ui32Count,
			   u32 ui32Flags)
{
	u32 ui32Written = 0;
	u32 ui32Off = 0;

	if ((psStream == NULL) || PDumpSuspended()
	    || ((ui32Flags & PDUMP_FLAGS_NEVER) != 0)) {
		return 1;
	}

	if (psStream == gsDBGPdumpState.psStream[PDUMP_STREAM_PARAM2]) {
		u32 ui32ParamOutPos =
		    gpfnDbgDrv->pfnGetStreamOffset(gsDBGPdumpState.
						   psStream
						   [PDUMP_STREAM_PARAM2]);

		if (ui32ParamOutPos + ui32Count > MAX_FILE_SIZE) {
			if ((gsDBGPdumpState.psStream[PDUMP_STREAM_SCRIPT2]
			     &&
			     PDumpWriteString2
			     ("\r\n-- Splitting pdump output file\r\n\r\n",
			      ui32Flags))) {
				DbgSetMarker(gsDBGPdumpState.
					     psStream[PDUMP_STREAM_PARAM2],
					     ui32ParamOutPos);
				gsDBGPdumpState.ui32ParamFileNum++;
			}
		}
	}

	while (((u32) ui32Count > 0) && (ui32Written != 0xFFFFFFFF)) {
		ui32Written =
		    DbgWrite(psStream, &pui8Data[ui32Off], ui32Count,
			     ui32Flags);

		if (ui32Written == 0) {
			OSReleaseThreadQuanta();
		}

		if (ui32Written != 0xFFFFFFFF) {
			ui32Off += ui32Written;
			ui32Count -= ui32Written;
		}
	}

	if (ui32Written == 0xFFFFFFFF) {
		return 0;
	}

	return 1;
}

static void DbgSetFrame(PDBG_STREAM psStream, u32 ui32Frame)
{
	gpfnDbgDrv->pfnSetFrame(psStream, ui32Frame);
}

static u32 DbgGetFrame(PDBG_STREAM psStream)
{
	return gpfnDbgDrv->pfnGetFrame(psStream);
}

static void DbgSetMarker(PDBG_STREAM psStream, u32 ui32Marker)
{
	gpfnDbgDrv->pfnSetMarker(psStream, ui32Marker);
}

static u32 DbgWrite(PDBG_STREAM psStream, u8 * pui8Data, u32 ui32BCount,
		    u32 ui32Flags)
{
	u32 ui32BytesWritten;

	if ((ui32Flags & PDUMP_FLAGS_CONTINUOUS) != 0) {

		if (((psStream->ui32CapMode & DEBUG_CAPMODE_FRAMED) != 0) &&
		    (psStream->ui32Start == 0xFFFFFFFFUL) &&
		    (psStream->ui32End == 0xFFFFFFFFUL) &&
		    psStream->bInitPhaseComplete) {
			ui32BytesWritten = ui32BCount;
		} else {
			ui32BytesWritten =
			    gpfnDbgDrv->pfnDBGDrivWrite2(psStream, pui8Data,
							 ui32BCount, 1);
		}
	} else {
		if (ui32Flags & PDUMP_FLAGS_LASTFRAME) {
			u32 ui32DbgFlags;

			ui32DbgFlags = 0;
			if (ui32Flags & PDUMP_FLAGS_RESETLFBUFFER) {
				ui32DbgFlags |= WRITELF_FLAGS_RESETBUF;
			}

			ui32BytesWritten =
			    gpfnDbgDrv->pfnWriteLF(psStream, pui8Data,
						   ui32BCount, 1, ui32DbgFlags);
		} else {
			ui32BytesWritten =
			    gpfnDbgDrv->pfnWriteBINCM(psStream, pui8Data,
						      ui32BCount, 1);
		}
	}

	return ui32BytesWritten;
}

void PDumpSuspendKM(void)
{
	atomic_inc(&gsPDumpSuspended);
}

void PDumpResumeKM(void)
{
	atomic_dec(&gsPDumpSuspended);
}

#endif
#endif

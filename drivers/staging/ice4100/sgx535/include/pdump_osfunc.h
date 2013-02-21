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

#ifndef __PDUMP_OSFUNC_H__
#define __PDUMP_OSFUNC_H__

#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif


#define MAX_PDUMP_STRING_LENGTH (256)
#define PDUMP_GET_SCRIPT_STRING()				\
	void * hScript;							\
	u32	ui32MaxLen;						\
	PVRSRV_ERROR eError;						\
	eError = PDumpOSGetScriptString(&hScript, &ui32MaxLen);\
	if(eError != PVRSRV_OK) return eError;

#define PDUMP_GET_MSG_STRING()					\
	void * hMsg;							\
	u32	ui32MaxLen;						\
	PVRSRV_ERROR eError;						\
	eError = PDumpOSGetMessageString(&hMsg, &ui32MaxLen);\
	if(eError != PVRSRV_OK) return eError;

#define PDUMP_GET_FILE_STRING()				\
	char *pszFileName;					\
	u32	ui32MaxLen;					\
	PVRSRV_ERROR eError;					\
	eError = PDumpOSGetFilenameString(&pszFileName, &ui32MaxLen);\
	if(eError != PVRSRV_OK) return eError;

#define PDUMP_GET_SCRIPT_AND_FILE_STRING()		\
	void * hScript;							\
	char *pszFileName;						\
	u32	ui32MaxLenScript;				\
	u32	ui32MaxLenFileName;				\
	PVRSRV_ERROR eError;						\
	eError = PDumpOSGetScriptString(&hScript, &ui32MaxLenScript);\
	if(eError != PVRSRV_OK) return eError;		\
	eError = PDumpOSGetFilenameString(&pszFileName, &ui32MaxLenFileName);\
	if(eError != PVRSRV_OK) return eError;



	PVRSRV_ERROR PDumpOSGetScriptString(void * *phScript, u32 *pui32MaxLen);


	PVRSRV_ERROR PDumpOSGetMessageString(void * *phMsg, u32 *pui32MaxLen);


	PVRSRV_ERROR PDumpOSGetFilenameString(char **ppszFile, u32 *pui32MaxLen);




#define PDUMP_va_list	va_list
#define PDUMP_va_start	va_start
#define PDUMP_va_end	va_end



void * PDumpOSGetStream(u32 ePDumpStream);

u32 PDumpOSGetStreamOffset(u32 ePDumpStream);

u32 PDumpOSGetParamFileNum(void);

void PDumpOSCheckForSplitting(void * hStream, u32 ui32Size, u32 ui32Flags);

int PDumpOSIsSuspended(void);

int PDumpOSJTInitialised(void);

int PDumpOSWriteString(void * hDbgStream,
		u8 *psui8Data,
		u32 ui32Size,
		u32 ui32Flags);

int PDumpOSWriteString2(void *	hScript, u32 ui32Flags);

PVRSRV_ERROR PDumpOSBufprintf(void * hBuf, u32 ui32ScriptSizeMax, char* pszFormat, ...);

void PDumpOSDebugPrintf(char* pszFormat, ...);

PVRSRV_ERROR PDumpOSSprintf(char *pszComment, u32 ui32ScriptSizeMax, char *pszFormat, ...);

PVRSRV_ERROR PDumpOSVSprintf(char *pszMsg, u32 ui32ScriptSizeMax, char* pszFormat, PDUMP_va_list vaArgs);

u32 PDumpOSBuflen(void * hBuffer, u32 ui32BufferSizeMax);

void PDumpOSVerifyLineEnding(void * hBuffer, u32 ui32BufferSizeMax);

void PDumpOSCPUVAddrToDevPAddr(PVRSRV_DEVICE_TYPE eDeviceType,
        void * hOSMemHandle,
		u32 ui32Offset,
		u8 *pui8LinAddr,
		u32 ui32PageSize,
		IMG_DEV_PHYADDR *psDevPAddr);

void PDumpOSCPUVAddrToPhysPages(void * hOSMemHandle,
		u32 ui32Offset,
		u8 * pui8LinAddr,
		u32 *pui32PageOffset);

#if defined (__cplusplus)
}
#endif

#endif


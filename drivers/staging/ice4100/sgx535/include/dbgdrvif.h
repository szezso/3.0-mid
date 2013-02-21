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

#ifndef _DBGDRVIF_
#define _DBGDRVIF_


#include "ioctldef.h"

#define DEBUG_CAPMODE_FRAMED			0x00000001UL
#define DEBUG_CAPMODE_CONTINUOUS		0x00000002UL
#define DEBUG_CAPMODE_HOTKEY			0x00000004UL

#define DEBUG_OUTMODE_STANDARDDBG		0x00000001UL
#define DEBUG_OUTMODE_MONO				0x00000002UL
#define DEBUG_OUTMODE_STREAMENABLE		0x00000004UL
#define DEBUG_OUTMODE_ASYNC				0x00000008UL
#define DEBUG_OUTMODE_SGXVGA            0x00000010UL

#define DEBUG_FLAGS_USE_NONPAGED_MEM	0x00000001UL
#define DEBUG_FLAGS_NO_BUF_EXPANDSION	0x00000002UL
#define DEBUG_FLAGS_ENABLESAMPLE		0x00000004UL

#define DEBUG_FLAGS_TEXTSTREAM			0x80000000UL

#define DEBUG_LEVEL_0					0x00000001UL
#define DEBUG_LEVEL_1					0x00000003UL
#define DEBUG_LEVEL_2					0x00000007UL
#define DEBUG_LEVEL_3					0x0000000FUL
#define DEBUG_LEVEL_4					0x0000001FUL
#define DEBUG_LEVEL_5					0x0000003FUL
#define DEBUG_LEVEL_6					0x0000007FUL
#define DEBUG_LEVEL_7					0x000000FFUL
#define DEBUG_LEVEL_8					0x000001FFUL
#define DEBUG_LEVEL_9					0x000003FFUL
#define DEBUG_LEVEL_10					0x000007FFUL
#define DEBUG_LEVEL_11					0x00000FFFUL

#define DEBUG_LEVEL_SEL0				0x00000001UL
#define DEBUG_LEVEL_SEL1				0x00000002UL
#define DEBUG_LEVEL_SEL2				0x00000004UL
#define DEBUG_LEVEL_SEL3				0x00000008UL
#define DEBUG_LEVEL_SEL4				0x00000010UL
#define DEBUG_LEVEL_SEL5				0x00000020UL
#define DEBUG_LEVEL_SEL6				0x00000040UL
#define DEBUG_LEVEL_SEL7				0x00000080UL
#define DEBUG_LEVEL_SEL8				0x00000100UL
#define DEBUG_LEVEL_SEL9				0x00000200UL
#define DEBUG_LEVEL_SEL10				0x00000400UL
#define DEBUG_LEVEL_SEL11				0x00000800UL

#define DEBUG_SERVICE_IOCTL_BASE		0x800UL
#define DEBUG_SERVICE_CREATESTREAM		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_DESTROYSTREAM		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_GETSTREAM			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITESTRING		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_READSTRING		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x05, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITE				CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x06, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_READ				CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x07, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_SETDEBUGMODE		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_SETDEBUGOUTMODE	CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x09, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_SETDEBUGLEVEL		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_SETFRAME			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_GETFRAME			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_OVERRIDEMODE		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_DEFAULTMODE		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0E, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_GETSERVICETABLE	CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x0F, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITE2			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITESTRINGCM		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITECM			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_SETMARKER			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_GETMARKER			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x14, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_ISCAPTUREFRAME	CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WRITELF			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x16, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_READLF			CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x17, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEBUG_SERVICE_WAITFOREVENT		CTL_CODE(FILE_DEVICE_UNKNOWN, DEBUG_SERVICE_IOCTL_BASE + 0x18, METHOD_BUFFERED, FILE_ANY_ACCESS)


typedef enum _DBG_EVENT_
{
	DBG_EVENT_STREAM_DATA = 1
} DBG_EVENT;

typedef struct _DBG_IN_CREATESTREAM_
{
	u32 ui32Pages;
	u32 ui32CapMode;
	u32 ui32OutMode;
	char *pszName;
}DBG_IN_CREATESTREAM, *PDBG_IN_CREATESTREAM;

typedef struct _DBG_IN_FINDSTREAM_
{
	int bResetStream;
	char *pszName;
}DBG_IN_FINDSTREAM, *PDBG_IN_FINDSTREAM;

typedef struct _DBG_IN_WRITESTRING_
{
	void *pvStream;
	u32 ui32Level;
	char *pszString;
}DBG_IN_WRITESTRING, *PDBG_IN_WRITESTRING;

typedef struct _DBG_IN_READSTRING_
{
	void *pvStream;
	u32 ui32StringLen;
	char *pszString;
} DBG_IN_READSTRING, *PDBG_IN_READSTRING;

typedef struct _DBG_IN_SETDEBUGMODE_
{
	void *pvStream;
	u32 ui32Mode;
	u32 ui32Start;
	u32 ui32End;
	u32 ui32SampleRate;
} DBG_IN_SETDEBUGMODE, *PDBG_IN_SETDEBUGMODE;

typedef struct _DBG_IN_SETDEBUGOUTMODE_
{
	void *pvStream;
	u32 ui32Mode;
} DBG_IN_SETDEBUGOUTMODE, *PDBG_IN_SETDEBUGOUTMODE;

typedef struct _DBG_IN_SETDEBUGLEVEL_
{
	void *pvStream;
	u32 ui32Level;
} DBG_IN_SETDEBUGLEVEL, *PDBG_IN_SETDEBUGLEVEL;

typedef struct _DBG_IN_SETFRAME_
{
	void *pvStream;
	u32 ui32Frame;
} DBG_IN_SETFRAME, *PDBG_IN_SETFRAME;

typedef struct _DBG_IN_WRITE_
{
	void *pvStream;
	u32 ui32Level;
	u32 ui32TransferSize;
	u8 *pui8InBuffer;
} DBG_IN_WRITE, *PDBG_IN_WRITE;

typedef struct _DBG_IN_READ_
{
	void *pvStream;
	int bReadInitBuffer;
	u32 ui32OutBufferSize;
	u8 *pui8OutBuffer;
} DBG_IN_READ, *PDBG_IN_READ;

typedef struct _DBG_IN_OVERRIDEMODE_
{
	void *pvStream;
	u32 ui32Mode;
} DBG_IN_OVERRIDEMODE, *PDBG_IN_OVERRIDEMODE;

typedef struct _DBG_IN_ISCAPTUREFRAME_
{
	void *pvStream;
	int bCheckPreviousFrame;
} DBG_IN_ISCAPTUREFRAME, *PDBG_IN_ISCAPTUREFRAME;

typedef struct _DBG_IN_SETMARKER_
{
	void *pvStream;
	u32 ui32Marker;
} DBG_IN_SETMARKER, *PDBG_IN_SETMARKER;

typedef struct _DBG_IN_WRITE_LF_
{
	u32 ui32Flags;
	void *pvStream;
	u32 ui32Level;
	u32 ui32BufferSize;
	u8 *pui8InBuffer;
} DBG_IN_WRITE_LF, *PDBG_IN_WRITE_LF;

#define WRITELF_FLAGS_RESETBUF		0x00000001UL

typedef struct _DBG_STREAM_
{
	struct _DBG_STREAM_ *psNext;
	struct _DBG_STREAM_ *psInitStream;
	int   bInitPhaseComplete;
	u32 ui32Flags;
	u32 ui32Base;
	u32 ui32Size;
	u32 ui32RPtr;
	u32 ui32WPtr;
	u32 ui32DataWritten;
	u32 ui32CapMode;
	u32 ui32OutMode;
	u32 ui32DebugLevel;
	u32 ui32DefaultMode;
	u32 ui32Start;
	u32 ui32End;
	u32 ui32Current;
	u32 ui32Access;
	u32 ui32SampleRate;
	u32 ui32Reserved;
	u32 ui32Timeout;
	u32 ui32Marker;
	char szName[30];
} DBG_STREAM,*PDBG_STREAM;

typedef struct _DBGKM_SERVICE_TABLE_
{
	u32 ui32Size;
	void * 	( *pfnCreateStream)			(char * pszName,u32 ui32CapMode,u32 ui32OutMode,u32 ui32Flags,u32 ui32Pages);
	void 	( *pfnDestroyStream)		(PDBG_STREAM psStream);
	void * 	( *pfnFindStream) 			(char * pszName, int bResetInitBuffer);
	u32 	( *pfnWriteString) 			(PDBG_STREAM psStream,char * pszString,u32 ui32Level);
	u32 	( *pfnReadString)			(PDBG_STREAM psStream,char * pszString,u32 ui32Limit);
	u32 	( *pfnWriteBIN)				(PDBG_STREAM psStream,u8 *pui8InBuf,u32 ui32InBuffSize,u32 ui32Level);
	u32 	( *pfnReadBIN)				(PDBG_STREAM psStream,int bReadInitBuffer, u32 ui32OutBufferSize,u8 *pui8OutBuf);
	void 	( *pfnSetCaptureMode)		(PDBG_STREAM psStream,u32 ui32CapMode,u32 ui32Start,u32 ui32Stop,u32 ui32SampleRate);
	void 	( *pfnSetOutputMode)		(PDBG_STREAM psStream,u32 ui32OutMode);
	void 	( *pfnSetDebugLevel)		(PDBG_STREAM psStream,u32 ui32DebugLevel);
	void 	( *pfnSetFrame)				(PDBG_STREAM psStream,u32 ui32Frame);
	u32 	( *pfnGetFrame)				(PDBG_STREAM psStream);
	void 	( *pfnOverrideMode)			(PDBG_STREAM psStream,u32 ui32Mode);
	void 	( *pfnDefaultMode)			(PDBG_STREAM psStream);
	u32	( *pfnDBGDrivWrite2)		(PDBG_STREAM psStream,u8 *pui8InBuf,u32 ui32InBuffSize,u32 ui32Level);
	u32 	( *pfnWriteStringCM)		(PDBG_STREAM psStream,char * pszString,u32 ui32Level);
	u32	( *pfnWriteBINCM)			(PDBG_STREAM psStream,u8 *pui8InBuf,u32 ui32InBuffSize,u32 ui32Level);
	void 	( *pfnSetMarker)			(PDBG_STREAM psStream,u32 ui32Marker);
	u32 	( *pfnGetMarker)			(PDBG_STREAM psStream);
	void 	( *pfnStartInitPhase)		(PDBG_STREAM psStream);
	void 	( *pfnStopInitPhase)		(PDBG_STREAM psStream);
	int 	( *pfnIsCaptureFrame)		(PDBG_STREAM psStream, int bCheckPreviousFrame);
	u32 	( *pfnWriteLF)				(PDBG_STREAM psStream, u8 *pui8InBuf, u32 ui32InBuffSize, u32 ui32Level, u32 ui32Flags);
	u32 	( *pfnReadLF)				(PDBG_STREAM psStream, u32 ui32OutBuffSize, u8 *pui8OutBuf);
	u32 	( *pfnGetStreamOffset)		(PDBG_STREAM psStream);
	void	( *pfnSetStreamOffset)		(PDBG_STREAM psStream, u32 ui32StreamOffset);
	int 	( *pfnIsLastCaptureFrame)	(PDBG_STREAM psStream);
	void 	( *pfnWaitForEvent)	(DBG_EVENT eEvent);
} DBGKM_SERVICE_TABLE, *PDBGKM_SERVICE_TABLE;


#endif

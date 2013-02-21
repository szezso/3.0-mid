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

#ifndef __BRIDGED_PVR_BRIDGE_H__
#define __BRIDGED_PVR_BRIDGE_H__

#include "pvr_bridge.h"

#if defined(__linux__)
#define PVRSRV_GET_BRIDGE_ID(X) _IOC_NR(X)
#else
#define PVRSRV_GET_BRIDGE_ID(X) (X - PVRSRV_IOWR(PVRSRV_BRIDGE_CORE_CMD_FIRST))
#endif

#ifndef ENOMEM
#define ENOMEM	12
#endif
#ifndef EFAULT
#define EFAULT	14
#endif
#ifndef ENOTTY
#define ENOTTY	25
#endif

#if defined(DEBUG_BRIDGE_KM)
PVRSRV_ERROR
CopyFromUserWrapper(PVRSRV_PER_PROCESS_DATA *pProcData,
					u32 ui32BridgeID,
					void *pvDest,
					void *pvSrc,
					u32 ui32Size);
PVRSRV_ERROR
CopyToUserWrapper(PVRSRV_PER_PROCESS_DATA *pProcData,
				  u32 ui32BridgeID,
				  void *pvDest,
				  void *pvSrc,
				  u32 ui32Size);
#else
#define CopyFromUserWrapper(pProcData, ui32BridgeID, pvDest, pvSrc, ui32Size) \
	OSCopyFromUser(pProcData, pvDest, pvSrc, ui32Size)
#define CopyToUserWrapper(pProcData, ui32BridgeID, pvDest, pvSrc, ui32Size) \
	OSCopyToUser(pProcData, pvDest, pvSrc, ui32Size)
#endif


#define ASSIGN_AND_RETURN_ON_ERROR(error, src, res)		\
	do							\
	{							\
		(error) = (src);				\
		if ((error) != PVRSRV_OK) 			\
		{						\
			return (res);				\
		}						\
	} while (error != PVRSRV_OK)

#define ASSIGN_AND_EXIT_ON_ERROR(error, src)		\
	ASSIGN_AND_RETURN_ON_ERROR(error, src, 0)

#if defined (PVR_SECURE_HANDLES)
static inline PVRSRV_ERROR
NewHandleBatch(PVRSRV_PER_PROCESS_DATA *psPerProc,
					u32 ui32BatchSize)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(!psPerProc->bHandlesBatched);

	eError = PVRSRVNewHandleBatch(psPerProc->psHandleBase, ui32BatchSize);

	if (eError == PVRSRV_OK)
	{
		psPerProc->bHandlesBatched = 1;
	}

	return eError;
}

#define NEW_HANDLE_BATCH_OR_ERROR(error, psPerProc, ui32BatchSize)	\
	ASSIGN_AND_EXIT_ON_ERROR(error, NewHandleBatch(psPerProc, ui32BatchSize))

static inline PVRSRV_ERROR
CommitHandleBatch(PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVR_ASSERT(psPerProc->bHandlesBatched);

	psPerProc->bHandlesBatched = 0;

	return PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
}


#define COMMIT_HANDLE_BATCH_OR_ERROR(error, psPerProc) 			\
	ASSIGN_AND_EXIT_ON_ERROR(error, CommitHandleBatch(psPerProc))

static inline void
ReleaseHandleBatch(PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	if (psPerProc->bHandlesBatched)
	{
		psPerProc->bHandlesBatched = 0;

		PVRSRVReleaseHandleBatch(psPerProc->psHandleBase);
	}
}
#else
#define NEW_HANDLE_BATCH_OR_ERROR(error, psPerProc, ui32BatchSize)
#define COMMIT_HANDLE_BATCH_OR_ERROR(error, psPerProc)
#define ReleaseHandleBatch(psPerProc)
#endif

int
DummyBW(u32 ui32BridgeID,
		void *psBridgeIn,
		void *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc);

typedef int (*BridgeWrapperFunction)(u32 ui32BridgeID,
									 void *psBridgeIn,
									 void *psBridgeOut,
									 PVRSRV_PER_PROCESS_DATA *psPerProc);

typedef struct _PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY
{
	BridgeWrapperFunction pfFunction;
#if defined(DEBUG_BRIDGE_KM)
	const char *pszIOCName;
	const char *pszFunctionName;
	u32 ui32CallCount;
	u32 ui32CopyFromUserTotalBytes;
	u32 ui32CopyToUserTotalBytes;
#endif
}PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY;

#if defined(SUPPORT_VGX) || defined(SUPPORT_MSVDX)
	#if defined(SUPPORT_VGX)
		#define BRIDGE_DISPATCH_TABLE_ENTRY_COUNT (PVRSRV_BRIDGE_LAST_VGX_CMD+1)
		#define PVRSRV_BRIDGE_LAST_DEVICE_CMD	   PVRSRV_BRIDGE_LAST_VGX_CMD
	#else
		#define BRIDGE_DISPATCH_TABLE_ENTRY_COUNT (PVRSRV_BRIDGE_LAST_MSVDX_CMD+1)
		#define PVRSRV_BRIDGE_LAST_DEVICE_CMD	   PVRSRV_BRIDGE_LAST_MSVDX_CMD
	#endif
#else
	#if defined(SUPPORT_SGX)
		#define BRIDGE_DISPATCH_TABLE_ENTRY_COUNT (PVRSRV_BRIDGE_LAST_SGX_CMD+1)
		#define PVRSRV_BRIDGE_LAST_DEVICE_CMD	   PVRSRV_BRIDGE_LAST_SGX_CMD
	#else
		#define BRIDGE_DISPATCH_TABLE_ENTRY_COUNT (PVRSRV_BRIDGE_LAST_NON_DEVICE_CMD+1)
		#define PVRSRV_BRIDGE_LAST_DEVICE_CMD	   PVRSRV_BRIDGE_LAST_NON_DEVICE_CMD
	#endif
#endif

extern PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY g_BridgeDispatchTable[BRIDGE_DISPATCH_TABLE_ENTRY_COUNT];

void
_SetDispatchTableEntry(u32 ui32Index,
					   const char *pszIOCName,
					   BridgeWrapperFunction pfFunction,
					   const char *pszFunctionName);


#define SetDispatchTableEntry(ui32Index, pfFunction) \
	_SetDispatchTableEntry(PVRSRV_GET_BRIDGE_ID(ui32Index), #ui32Index, (BridgeWrapperFunction)pfFunction, #pfFunction)

#define DISPATCH_TABLE_GAP_THRESHOLD 5

#if defined(DEBUG)
#define PVRSRV_BRIDGE_ASSERT_CMD(X, Y) PVR_ASSERT(X == PVRSRV_GET_BRIDGE_ID(Y))
#else
#define PVRSRV_BRIDGE_ASSERT_CMD(X, Y) do {} while(0)
#endif


#if defined(DEBUG_BRIDGE_KM)
typedef struct _PVRSRV_BRIDGE_GLOBAL_STATS
{
	u32 ui32IOCTLCount;
	u32 ui32TotalCopyFromUserBytes;
	u32 ui32TotalCopyToUserBytes;
}PVRSRV_BRIDGE_GLOBAL_STATS;

extern PVRSRV_BRIDGE_GLOBAL_STATS g_BridgeGlobalStats;
#endif


PVRSRV_ERROR CommonBridgeInit(void);

int BridgedDispatchKM(PVRSRV_PER_PROCESS_DATA * psPerProc,
					  PVRSRV_BRIDGE_PACKAGE   * psBridgePackageKM);

#if defined (__cplusplus)
}
#endif

#endif


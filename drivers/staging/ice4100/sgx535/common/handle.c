/**********************************************************************
 *
 * Copyright (c) 2010 Intel Corporation.
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

#ifdef	PVR_SECURE_HANDLES
#include <stddef.h>

#include "services_headers.h"
#include "handle.h"

#ifdef	DEBUG
#define	HANDLE_BLOCK_SIZE	1
#else
#define	HANDLE_BLOCK_SIZE	256
#endif

#define	HANDLE_HASH_TAB_INIT_SIZE	32

#define	DEFAULT_MAX_INDEX_PLUS_ONE	0xfffffffful
#define	DEFAULT_MAX_HANDLE		DEFAULT_MAX_INDEX_PLUS_ONE

#define	INDEX_IS_VALID(psBase, i) ((i) < (psBase)->ui32TotalHandCount)

#define	INDEX_TO_HANDLE(psBase, idx) ((void *)((idx) + 1))
#define	HANDLE_TO_INDEX(psBase, hand) ((u32)(hand) - 1)

#define INDEX_TO_HANDLE_PTR(psBase, i) (((psBase)->psHandleArray) + (i))
#define	HANDLE_TO_HANDLE_PTR(psBase, h) (INDEX_TO_HANDLE_PTR(psBase, HANDLE_TO_INDEX(psBase, h)))

#define	HANDLE_PTR_TO_INDEX(psBase, psHandle) (u32)((psHandle) - ((psBase)->psHandleArray))
#define	HANDLE_PTR_TO_HANDLE(psBase, psHandle) \
	INDEX_TO_HANDLE(psBase, HANDLE_PTR_TO_INDEX(psBase, psHandle))

#define	ROUND_UP_TO_MULTIPLE(a, b) ((((a) + (b) - 1) / (b)) * (b))

#define	HANDLES_BATCHED(psBase) ((psBase)->ui32HandBatchSize != 0)

#define	SET_FLAG(v, f) ((void)((v) |= (f)))
#define	CLEAR_FLAG(v, f) ((void)((v) &= ~(f)))
#define	TEST_FLAG(v, f) ((int)(((v) & (f)) != 0))

#define	TEST_ALLOC_FLAG(psHandle, f) TEST_FLAG((psHandle)->eFlag, f)

#define	SET_INTERNAL_FLAG(psHandle, f) SET_FLAG((psHandle)->eInternalFlag, f)
#define	CLEAR_INTERNAL_FLAG(psHandle, f) CLEAR_FLAG((psHandle)->eInternalFlag, f)
#define	TEST_INTERNAL_FLAG(psHandle, f) TEST_FLAG((psHandle)->eInternalFlag, f)

#define	BATCHED_HANDLE(psHandle) TEST_INTERNAL_FLAG(psHandle, INTERNAL_HANDLE_FLAG_BATCHED)

#define	SET_BATCHED_HANDLE(psHandle) SET_INTERNAL_FLAG(psHandle, INTERNAL_HANDLE_FLAG_BATCHED)

#define	SET_UNBATCHED_HANDLE(psHandle) CLEAR_INTERNAL_FLAG(psHandle, INTERNAL_HANDLE_FLAG_BATCHED)

#define	BATCHED_HANDLE_PARTIALLY_FREE(psHandle) TEST_INTERNAL_FLAG(psHandle, INTERNAL_HANDLE_FLAG_BATCHED_PARTIALLY_FREE)

#define SET_BATCHED_HANDLE_PARTIALLY_FREE(psHandle) SET_INTERNAL_FLAG(psHandle, INTERNAL_HANDLE_FLAG_BATCHED_PARTIALLY_FREE)

#define	HANDLE_STRUCT_IS_FREE(psHandle) ((psHandle)->eType == PVRSRV_HANDLE_TYPE_NONE && (psHandle)->eInternalFlag == INTERNAL_HANDLE_FLAG_NONE)

#ifdef	MIN
#undef MIN
#endif

#define	MIN(x, y) (((x) < (y)) ? (x) : (y))

struct sHandleList {
	u32 ui32Prev;
	u32 ui32Next;
	void *hParent;
};

enum ePVRSRVInternalHandleFlag {
	INTERNAL_HANDLE_FLAG_NONE = 0x00,
	INTERNAL_HANDLE_FLAG_BATCHED = 0x01,
	INTERNAL_HANDLE_FLAG_BATCHED_PARTIALLY_FREE = 0x02,
};

struct sHandle {

	PVRSRV_HANDLE_TYPE eType;

	void *pvData;

	u32 ui32NextIndexPlusOne;

	enum ePVRSRVInternalHandleFlag eInternalFlag;

	PVRSRV_HANDLE_ALLOC_FLAG eFlag;

	u32 ui32Index;

	struct sHandleList sChildren;

	struct sHandleList sSiblings;
};

struct _PVRSRV_HANDLE_BASE_ {

	void *hBaseBlockAlloc;

	void *hHandBlockAlloc;

	struct sHandle *psHandleArray;

	HASH_TABLE *psHashTab;

	u32 ui32FreeHandCount;

	u32 ui32FirstFreeIndex;

	u32 ui32MaxIndexPlusOne;

	u32 ui32TotalHandCount;

	u32 ui32LastFreeIndexPlusOne;

	u32 ui32HandBatchSize;

	u32 ui32TotalHandCountPreBatch;

	u32 ui32FirstBatchIndexPlusOne;

	u32 ui32BatchHandAllocFailures;

	int bPurgingEnabled;
};

enum eHandKey {
	HAND_KEY_DATA = 0,
	HAND_KEY_TYPE,
	HAND_KEY_PARENT,
	HAND_KEY_LEN
};

PVRSRV_HANDLE_BASE *gpsKernelHandleBase = NULL;

typedef u32 HAND_KEY[HAND_KEY_LEN];

static
void HandleListInit(u32 ui32Index, struct sHandleList *psList, void *hParent)
{
	psList->ui32Next = ui32Index;
	psList->ui32Prev = ui32Index;
	psList->hParent = hParent;
}

static
void InitParentList(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psHandle)
{
	u32 ui32Parent = HANDLE_PTR_TO_INDEX(psBase, psHandle);

	HandleListInit(ui32Parent, &psHandle->sChildren,
		       INDEX_TO_HANDLE(psBase, ui32Parent));
}

static
void InitChildEntry(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psHandle)
{
	HandleListInit(HANDLE_PTR_TO_INDEX(psBase, psHandle),
		       &psHandle->sSiblings, NULL);
}

static
int HandleListIsEmpty(u32 ui32Index, struct sHandleList *psList)
{
	int bIsEmpty;

	bIsEmpty = (int)(psList->ui32Next == ui32Index);

#ifdef	DEBUG
	{
		int bIsEmpty2;

		bIsEmpty2 = (int)(psList->ui32Prev == ui32Index);
		PVR_ASSERT(bIsEmpty == bIsEmpty2);
	}
#endif

	return bIsEmpty;
}

#ifdef  DEBUG
static
int NoChildren(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psHandle)
{
	PVR_ASSERT(psHandle->sChildren.hParent ==
		   HANDLE_PTR_TO_HANDLE(psBase, psHandle));

	return HandleListIsEmpty(HANDLE_PTR_TO_INDEX(psBase, psHandle),
				 &psHandle->sChildren);
}

static
int NoParent(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psHandle)
{
	if (HandleListIsEmpty
	    (HANDLE_PTR_TO_INDEX(psBase, psHandle), &psHandle->sSiblings)) {
		PVR_ASSERT(psHandle->sSiblings.hParent == NULL);

		return 1;
	} else {
		PVR_ASSERT(psHandle->sSiblings.hParent != NULL);
	}
	return 0;
}
#endif

static
void *ParentHandle(struct sHandle *psHandle)
{
	return psHandle->sSiblings.hParent;
}

#define	LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, i, p, po, eo) \
		((struct sHandleList *)((char *)(INDEX_TO_HANDLE_PTR(psBase, i)) + (((i) == (p)) ? (po) : (eo))))

static
void HandleListInsertBefore(PVRSRV_HANDLE_BASE * psBase, u32 ui32InsIndex,
			    struct sHandleList *psIns, u32 uiParentOffset,
			    u32 ui32EntryIndex, struct sHandleList *psEntry,
			    u32 uiEntryOffset, u32 ui32ParentIndex)
{

	struct sHandleList *psPrevIns =
	    LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, psIns->ui32Prev,
					   ui32ParentIndex, uiParentOffset,
					   uiEntryOffset);

	PVR_ASSERT(psEntry->hParent == NULL);
	PVR_ASSERT(ui32InsIndex == psPrevIns->ui32Next);
	PVR_ASSERT(LIST_PTR_FROM_INDEX_AND_OFFSET
		   (psBase, ui32ParentIndex, ui32ParentIndex, uiParentOffset,
		    uiParentOffset)->hParent == INDEX_TO_HANDLE(psBase,
								ui32ParentIndex));

	psEntry->ui32Prev = psIns->ui32Prev;
	psIns->ui32Prev = ui32EntryIndex;
	psEntry->ui32Next = ui32InsIndex;
	psPrevIns->ui32Next = ui32EntryIndex;

	psEntry->hParent = INDEX_TO_HANDLE(psBase, ui32ParentIndex);
}

static
void AdoptChild(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psParent,
		struct sHandle *psChild)
{
	u32 ui32Parent = HANDLE_TO_INDEX(psBase, psParent->sChildren.hParent);

	PVR_ASSERT(ui32Parent == HANDLE_PTR_TO_INDEX(psBase, psParent));

	HandleListInsertBefore(psBase, ui32Parent, &psParent->sChildren,
			       offsetof(struct sHandle, sChildren),
			       HANDLE_PTR_TO_INDEX(psBase, psChild),
			       &psChild->sSiblings, offsetof(struct sHandle,
							     sSiblings),
			       ui32Parent);

}

static
void HandleListRemove(PVRSRV_HANDLE_BASE * psBase, u32 ui32EntryIndex,
		      struct sHandleList *psEntry, u32 uiEntryOffset,
		      u32 uiParentOffset)
{
	if (!HandleListIsEmpty(ui32EntryIndex, psEntry)) {

		struct sHandleList *psPrev =
		    LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, psEntry->ui32Prev,
						   HANDLE_TO_INDEX(psBase,
								   psEntry->
								   hParent),
						   uiParentOffset,
						   uiEntryOffset);
		struct sHandleList *psNext =
		    LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, psEntry->ui32Next,
						   HANDLE_TO_INDEX(psBase,
								   psEntry->
								   hParent),
						   uiParentOffset,
						   uiEntryOffset);

		PVR_ASSERT(psEntry->hParent != NULL);

		psPrev->ui32Next = psEntry->ui32Next;
		psNext->ui32Prev = psEntry->ui32Prev;

		HandleListInit(ui32EntryIndex, psEntry, NULL);
	}
}

static
void UnlinkFromParent(PVRSRV_HANDLE_BASE * psBase, struct sHandle *psHandle)
{
	HandleListRemove(psBase, HANDLE_PTR_TO_INDEX(psBase, psHandle),
			 &psHandle->sSiblings, offsetof(struct sHandle,
							sSiblings),
			 offsetof(struct sHandle, sChildren));
}

static
PVRSRV_ERROR HandleListIterate(PVRSRV_HANDLE_BASE * psBase,
			       struct sHandleList *psHead, u32 uiParentOffset,
			       u32 uiEntryOffset,
			       PVRSRV_ERROR(*pfnIterFunc) (PVRSRV_HANDLE_BASE *,
							   struct sHandle *))
{
	u32 ui32Index;
	u32 ui32Parent = HANDLE_TO_INDEX(psBase, psHead->hParent);

	PVR_ASSERT(psHead->hParent != NULL);

	for (ui32Index = psHead->ui32Next; ui32Index != ui32Parent;) {
		struct sHandle *psHandle =
		    INDEX_TO_HANDLE_PTR(psBase, ui32Index);

		struct sHandleList *psEntry =
		    LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, ui32Index,
						   ui32Parent, uiParentOffset,
						   uiEntryOffset);
		PVRSRV_ERROR eError;

		PVR_ASSERT(psEntry->hParent == psHead->hParent);

		ui32Index = psEntry->ui32Next;

		eError = (*pfnIterFunc) (psBase, psHandle);
		if (eError != PVRSRV_OK) {
			return eError;
		}
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR IterateOverChildren(PVRSRV_HANDLE_BASE * psBase,
				 struct sHandle *psParent,
				 PVRSRV_ERROR(*pfnIterFunc) (PVRSRV_HANDLE_BASE
							     *,
							     struct sHandle *))
{
	return HandleListIterate(psBase, &psParent->sChildren,
				 offsetof(struct sHandle, sChildren),
				 offsetof(struct sHandle, sSiblings),
				 pfnIterFunc);
}

static
PVRSRV_ERROR GetHandleStructure(PVRSRV_HANDLE_BASE * psBase,
				struct sHandle **ppsHandle, void *hHandle,
				PVRSRV_HANDLE_TYPE eType)
{
	u32 ui32Index = HANDLE_TO_INDEX(psBase, hHandle);
	struct sHandle *psHandle;

	if (!INDEX_IS_VALID(psBase, ui32Index)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "GetHandleStructure: Handle index out of range (%u >= %u)",
			 ui32Index, psBase->ui32TotalHandCount));
		return PVRSRV_ERROR_GENERIC;
	}

	psHandle = INDEX_TO_HANDLE_PTR(psBase, ui32Index);
	if (psHandle->eType == PVRSRV_HANDLE_TYPE_NONE) {
		PVR_DPF((PVR_DBG_ERROR,
			 "GetHandleStructure: Handle not allocated (index: %u)",
			 ui32Index));
		return PVRSRV_ERROR_GENERIC;
	}

	if (eType != PVRSRV_HANDLE_TYPE_NONE && eType != psHandle->eType) {
		PVR_DPF((PVR_DBG_ERROR,
			 "GetHandleStructure: Handle type mismatch (%d != %d)",
			 eType, psHandle->eType));
		return PVRSRV_ERROR_GENERIC;
	}

	*ppsHandle = psHandle;

	return PVRSRV_OK;
}

static
void *ParentIfPrivate(struct sHandle *psHandle)
{
	return TEST_ALLOC_FLAG(psHandle, PVRSRV_HANDLE_ALLOC_FLAG_PRIVATE) ?
	    ParentHandle(psHandle) : NULL;
}

static
void InitKey(HAND_KEY aKey, PVRSRV_HANDLE_BASE * psBase, void *pvData,
	     PVRSRV_HANDLE_TYPE eType, void *hParent)
{
	aKey[HAND_KEY_DATA] = (u32) pvData;
	aKey[HAND_KEY_TYPE] = (u32) eType;
	aKey[HAND_KEY_PARENT] = (u32) hParent;
}

static PVRSRV_ERROR FreeHandleArray(PVRSRV_HANDLE_BASE * psBase)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psBase->psHandleArray != NULL) {
		eError = OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				   psBase->ui32TotalHandCount *
				   sizeof(struct sHandle),
				   psBase->psHandleArray,
				   psBase->hHandBlockAlloc);

		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "FreeHandleArray: Error freeing memory (%d)",
				 eError));
		} else {
			psBase->psHandleArray = NULL;
		}
	}

	return eError;
}

static PVRSRV_ERROR FreeHandle(PVRSRV_HANDLE_BASE * psBase,
			       struct sHandle *psHandle)
{
	HAND_KEY aKey;
	u32 ui32Index = HANDLE_PTR_TO_INDEX(psBase, psHandle);
	PVRSRV_ERROR eError;

	InitKey(aKey, psBase, psHandle->pvData, psHandle->eType,
		ParentIfPrivate(psHandle));

	if (!TEST_ALLOC_FLAG(psHandle, PVRSRV_HANDLE_ALLOC_FLAG_MULTI)
	    && !BATCHED_HANDLE_PARTIALLY_FREE(psHandle)) {
		void *hHandle;
		hHandle = (void *)HASH_Remove_Extended(psBase->psHashTab, aKey);

		PVR_ASSERT(hHandle != NULL);
		PVR_ASSERT(hHandle == INDEX_TO_HANDLE(psBase, ui32Index));
	}

	UnlinkFromParent(psBase, psHandle);

	eError = IterateOverChildren(psBase, psHandle, FreeHandle);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeHandle: Error whilst freeing subhandles (%d)",
			 eError));
		return eError;
	}

	psHandle->eType = PVRSRV_HANDLE_TYPE_NONE;

	if (BATCHED_HANDLE(psHandle)
	    && !BATCHED_HANDLE_PARTIALLY_FREE(psHandle)) {

		SET_BATCHED_HANDLE_PARTIALLY_FREE(psHandle);

		return PVRSRV_OK;
	}

	if (!psBase->bPurgingEnabled) {
		if (psBase->ui32FreeHandCount == 0) {
			PVR_ASSERT(psBase->ui32FirstFreeIndex == 0);
			PVR_ASSERT(psBase->ui32LastFreeIndexPlusOne == 0);

			psBase->ui32FirstFreeIndex = ui32Index;
		} else {

			PVR_ASSERT(psBase->ui32LastFreeIndexPlusOne != 0);
			PVR_ASSERT(INDEX_TO_HANDLE_PTR
				   (psBase,
				    psBase->ui32LastFreeIndexPlusOne -
				    1)->ui32NextIndexPlusOne == 0);
			INDEX_TO_HANDLE_PTR(psBase,
					    psBase->ui32LastFreeIndexPlusOne -
					    1)->ui32NextIndexPlusOne =
			    ui32Index + 1;
		}

		PVR_ASSERT(psHandle->ui32NextIndexPlusOne == 0);

		psBase->ui32LastFreeIndexPlusOne = ui32Index + 1;
	}

	psBase->ui32FreeHandCount++;

	return PVRSRV_OK;
}

static PVRSRV_ERROR FreeAllHandles(PVRSRV_HANDLE_BASE * psBase)
{
	u32 i;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psBase->ui32FreeHandCount == psBase->ui32TotalHandCount) {
		return eError;
	}

	for (i = 0; i < psBase->ui32TotalHandCount; i++) {
		struct sHandle *psHandle;

		psHandle = INDEX_TO_HANDLE_PTR(psBase, i);

		if (psHandle->eType != PVRSRV_HANDLE_TYPE_NONE) {
			eError = FreeHandle(psBase, psHandle);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "FreeAllHandles: FreeHandle failed (%d)",
					 eError));
				break;
			}

			if (psBase->ui32FreeHandCount ==
			    psBase->ui32TotalHandCount) {
				break;
			}
		}
	}

	return eError;
}

static PVRSRV_ERROR FreeHandleBase(PVRSRV_HANDLE_BASE * psBase)
{
	PVRSRV_ERROR eError;

	if (HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_WARNING,
			 "FreeHandleBase: Uncommitted/Unreleased handle batch"));
		PVRSRVReleaseHandleBatch(psBase);
	}

	eError = FreeAllHandles(psBase);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeHandleBase: Couldn't free handles (%d)", eError));
		return eError;
	}

	eError = FreeHandleArray(psBase);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeHandleBase: Couldn't free handle array (%d)",
			 eError));
		return eError;
	}

	if (psBase->psHashTab != NULL) {

		HASH_Delete(psBase->psHashTab);
	}

	eError = OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			   sizeof(*psBase), psBase, psBase->hBaseBlockAlloc);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeHandleBase: Couldn't free handle base (%d)",
			 eError));
		return eError;
	}

	return PVRSRV_OK;
}

static
void *FindHandle(PVRSRV_HANDLE_BASE * psBase, void *pvData,
		 PVRSRV_HANDLE_TYPE eType, void *hParent)
{
	HAND_KEY aKey;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	InitKey(aKey, psBase, pvData, eType, hParent);

	return (void *)HASH_Retrieve_Extended(psBase->psHashTab, aKey);
}

static PVRSRV_ERROR ReallocMem(void **ppvMem, void **phBlockAlloc,
			       u32 ui32NewSize, u32 ui32OldSize)
{
	void *pvOldMem = *ppvMem;
	void *hOldBlockAlloc = *phBlockAlloc;
	u32 ui32CopySize = MIN(ui32NewSize, ui32OldSize);
	void *pvNewMem = NULL;
	void *hNewBlockAlloc = NULL;
	PVRSRV_ERROR eError;

	if (ui32NewSize == ui32OldSize) {
		return (PVRSRV_OK);
	}

	if (ui32NewSize != 0) {

		eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				    ui32NewSize,
				    &pvNewMem, &hNewBlockAlloc, "Memory Area");
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "ReallocMem: Couldn't allocate new memory area (%d)",
				 eError));
			return eError;
		}
	}

	if (ui32CopySize != 0) {

		memcpy(pvNewMem, pvOldMem, ui32CopySize);
	}

	if (ui32OldSize != 0) {

		eError = OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				   ui32OldSize, pvOldMem, hOldBlockAlloc);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "ReallocMem: Couldn't free old memory area (%d)",
				 eError));
		}
	}

	*ppvMem = pvNewMem;
	*phBlockAlloc = hNewBlockAlloc;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR ReallocHandleArray(PVRSRV_HANDLE_BASE * psBase, u32 ui32NewCount,
				u32 ui32OldCount)
{
	return ReallocMem((void **)&psBase->psHandleArray,
			  &psBase->hHandBlockAlloc,
			  ui32NewCount * sizeof(struct sHandle),
			  ui32OldCount * sizeof(struct sHandle));
}

static PVRSRV_ERROR IncreaseHandleArraySize(PVRSRV_HANDLE_BASE * psBase,
					    u32 ui32Delta)
{
	PVRSRV_ERROR eError;
	struct sHandle *psHandle;
	u32 ui32DeltaAdjusted =
	    ROUND_UP_TO_MULTIPLE(ui32Delta, HANDLE_BLOCK_SIZE);
	u32 ui32NewTotalHandCount =
	    psBase->ui32TotalHandCount + ui32DeltaAdjusted;
	;

	PVR_ASSERT(ui32Delta != 0);

	if (ui32NewTotalHandCount > psBase->ui32MaxIndexPlusOne
	    || ui32NewTotalHandCount <= psBase->ui32TotalHandCount) {
		ui32NewTotalHandCount = psBase->ui32MaxIndexPlusOne;

		ui32DeltaAdjusted =
		    ui32NewTotalHandCount - psBase->ui32TotalHandCount;

		if (ui32DeltaAdjusted < ui32Delta) {
			PVR_DPF((PVR_DBG_ERROR,
				 "IncreaseHandleArraySize: Maximum handle limit reached (%d)",
				 psBase->ui32MaxIndexPlusOne));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}

	PVR_ASSERT(ui32DeltaAdjusted >= ui32Delta);

	eError =
	    ReallocHandleArray(psBase, ui32NewTotalHandCount,
			       psBase->ui32TotalHandCount);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "IncreaseHandleArraySize: ReallocHandleArray failed (%d)",
			 eError));
		return eError;
	}

	for (psHandle = psBase->psHandleArray + psBase->ui32TotalHandCount;
	     psHandle < psBase->psHandleArray + ui32NewTotalHandCount;
	     psHandle++) {
		psHandle->eType = PVRSRV_HANDLE_TYPE_NONE;
		psHandle->eInternalFlag = INTERNAL_HANDLE_FLAG_NONE;
		psHandle->ui32NextIndexPlusOne = 0;
	}

	psBase->ui32FreeHandCount += ui32DeltaAdjusted;

	if (psBase->ui32FirstFreeIndex == 0) {
		PVR_ASSERT(psBase->ui32LastFreeIndexPlusOne == 0);

		psBase->ui32FirstFreeIndex = psBase->ui32TotalHandCount;
	} else {
		if (!psBase->bPurgingEnabled) {
			PVR_ASSERT(psBase->ui32LastFreeIndexPlusOne != 0)
			    PVR_ASSERT(INDEX_TO_HANDLE_PTR
				       (psBase,
					psBase->ui32LastFreeIndexPlusOne -
					1)->ui32NextIndexPlusOne == 0);

			INDEX_TO_HANDLE_PTR(psBase,
					    psBase->ui32LastFreeIndexPlusOne -
					    1)->ui32NextIndexPlusOne =
			    psBase->ui32TotalHandCount + 1;
		}
	}

	if (!psBase->bPurgingEnabled) {
		psBase->ui32LastFreeIndexPlusOne = ui32NewTotalHandCount;
	}

	psBase->ui32TotalHandCount = ui32NewTotalHandCount;

	return PVRSRV_OK;
}

static PVRSRV_ERROR EnsureFreeHandles(PVRSRV_HANDLE_BASE * psBase, u32 ui32Free)
{
	PVRSRV_ERROR eError;

	if (ui32Free > psBase->ui32FreeHandCount) {
		u32 ui32FreeHandDelta = ui32Free - psBase->ui32FreeHandCount;
		eError = IncreaseHandleArraySize(psBase, ui32FreeHandDelta);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "EnsureFreeHandles: Couldn't allocate %u handles to ensure %u free handles (IncreaseHandleArraySize failed with error %d)",
				 ui32FreeHandDelta, ui32Free, eError));

			return eError;
		}
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR AllocHandle(PVRSRV_HANDLE_BASE * psBase, void **phHandle,
				void *pvData, PVRSRV_HANDLE_TYPE eType,
				PVRSRV_HANDLE_ALLOC_FLAG eFlag, void *hParent)
{
	u32 ui32NewIndex;
	struct sHandle *psNewHandle = NULL;
	void *hHandle;
	HAND_KEY aKey;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	PVR_ASSERT(psBase->psHashTab != NULL);

	if (!TEST_FLAG(eFlag, PVRSRV_HANDLE_ALLOC_FLAG_MULTI)) {

		PVR_ASSERT(FindHandle(psBase, pvData, eType, hParent) == NULL);
	}

	if (psBase->ui32FreeHandCount == 0 && HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_WARNING,
			 "AllocHandle: Handle batch size (%u) was too small, allocating additional space",
			 psBase->ui32HandBatchSize));
	}

	eError = EnsureFreeHandles(psBase, 1);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "AllocHandle: EnsureFreeHandles failed (%d)", eError));
		return eError;
	}
	PVR_ASSERT(psBase->ui32FreeHandCount != 0)

	    if (!psBase->bPurgingEnabled) {

		ui32NewIndex = psBase->ui32FirstFreeIndex;

		psNewHandle = INDEX_TO_HANDLE_PTR(psBase, ui32NewIndex);
	} else {

		for (ui32NewIndex = psBase->ui32FirstFreeIndex;
		     ui32NewIndex < psBase->ui32TotalHandCount;
		     ui32NewIndex++) {
			psNewHandle = INDEX_TO_HANDLE_PTR(psBase, ui32NewIndex);
			if (HANDLE_STRUCT_IS_FREE(psNewHandle)) {
				break;
			}

		}
		psBase->ui32FirstFreeIndex = 0;
		PVR_ASSERT(ui32NewIndex < psBase->ui32TotalHandCount);
	}
	PVR_ASSERT(psNewHandle != NULL);
#ifdef INTEL_D3_P_CHANGES
	if (NULL == psNewHandle) {
		PVR_DPF((PVR_DBG_ERROR,
			 "AllocHandle: New handle pointer is NULL"));
		return PVRSRV_ERROR_GENERIC;
	}
#endif

	hHandle = INDEX_TO_HANDLE(psBase, ui32NewIndex);

	if (!TEST_FLAG(eFlag, PVRSRV_HANDLE_ALLOC_FLAG_MULTI)) {

		InitKey(aKey, psBase, pvData, eType, hParent);

		if (!HASH_Insert_Extended
		    (psBase->psHashTab, aKey, (u32) hHandle)) {
			PVR_DPF((PVR_DBG_ERROR,
				 "AllocHandle: Couldn't add handle to hash table"));

			return PVRSRV_ERROR_GENERIC;
		}
	}

	psBase->ui32FreeHandCount--;

	if (!psBase->bPurgingEnabled) {

		if (psBase->ui32FreeHandCount == 0) {
			PVR_ASSERT(psBase->ui32FirstFreeIndex == ui32NewIndex);
			PVR_ASSERT(psBase->ui32LastFreeIndexPlusOne ==
				   (ui32NewIndex + 1));

			psBase->ui32LastFreeIndexPlusOne = 0;
			psBase->ui32FirstFreeIndex = 0;
		} else {

			psBase->ui32FirstFreeIndex =
			    (psNewHandle->ui32NextIndexPlusOne ==
			     0) ? ui32NewIndex +
			    1 : psNewHandle->ui32NextIndexPlusOne - 1;
		}
	}

	psNewHandle->eType = eType;
	psNewHandle->pvData = pvData;
	psNewHandle->eInternalFlag = INTERNAL_HANDLE_FLAG_NONE;
	psNewHandle->eFlag = eFlag;
	psNewHandle->ui32Index = ui32NewIndex;

	InitParentList(psBase, psNewHandle);
#if defined(DEBUG)
	PVR_ASSERT(NoChildren(psBase, psNewHandle));
#endif

	InitChildEntry(psBase, psNewHandle);
#if defined(DEBUG)
	PVR_ASSERT(NoParent(psBase, psNewHandle));
#endif

	if (HANDLES_BATCHED(psBase)) {

		psNewHandle->ui32NextIndexPlusOne =
		    psBase->ui32FirstBatchIndexPlusOne;

		psBase->ui32FirstBatchIndexPlusOne = ui32NewIndex + 1;

		SET_BATCHED_HANDLE(psNewHandle);
	} else {
		psNewHandle->ui32NextIndexPlusOne = 0;
	}

	*phHandle = hHandle;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVAllocHandle(PVRSRV_HANDLE_BASE * psBase, void **phHandle,
			       void *pvData, PVRSRV_HANDLE_TYPE eType,
			       PVRSRV_HANDLE_ALLOC_FLAG eFlag)
{
	void *hHandle;
	PVRSRV_ERROR eError;

	*phHandle = NULL;

	if (HANDLES_BATCHED(psBase)) {

		psBase->ui32BatchHandAllocFailures++;
	}

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	if (!TEST_FLAG(eFlag, PVRSRV_HANDLE_ALLOC_FLAG_MULTI)) {

		hHandle = FindHandle(psBase, pvData, eType, NULL);
		if (hHandle != NULL) {
			struct sHandle *psHandle;

			eError =
			    GetHandleStructure(psBase, &psHandle, hHandle,
					       eType);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVAllocHandle: Lookup of existing handle failed"));
				return eError;
			}

			if (TEST_FLAG
			    (psHandle->eFlag & eFlag,
			     PVRSRV_HANDLE_ALLOC_FLAG_SHARED)) {
				*phHandle = hHandle;
				eError = PVRSRV_OK;
				goto exit_ok;
			}
			return PVRSRV_ERROR_GENERIC;
		}
	}

	eError = AllocHandle(psBase, phHandle, pvData, eType, eFlag, NULL);

exit_ok:
	if (HANDLES_BATCHED(psBase) && (eError == PVRSRV_OK)) {
		psBase->ui32BatchHandAllocFailures--;
	}

	return eError;
}

PVRSRV_ERROR PVRSRVAllocSubHandle(PVRSRV_HANDLE_BASE * psBase, void **phHandle,
				  void *pvData, PVRSRV_HANDLE_TYPE eType,
				  PVRSRV_HANDLE_ALLOC_FLAG eFlag, void *hParent)
{
	struct sHandle *psPHand;
	struct sHandle *psCHand;
	PVRSRV_ERROR eError;
	void *hParentKey;
	void *hHandle;

	*phHandle = NULL;

	if (HANDLES_BATCHED(psBase)) {

		psBase->ui32BatchHandAllocFailures++;
	}

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	hParentKey = TEST_FLAG(eFlag, PVRSRV_HANDLE_ALLOC_FLAG_PRIVATE) ?
	    hParent : NULL;

	eError =
	    GetHandleStructure(psBase, &psPHand, hParent,
			       PVRSRV_HANDLE_TYPE_NONE);
	if (eError != PVRSRV_OK) {
		return PVRSRV_ERROR_GENERIC;
	}

	if (!TEST_FLAG(eFlag, PVRSRV_HANDLE_ALLOC_FLAG_MULTI)) {

		hHandle = FindHandle(psBase, pvData, eType, hParentKey);
		if (hHandle != NULL) {
			struct sHandle *psCHandle;
			PVRSRV_ERROR eErr;

			eErr =
			    GetHandleStructure(psBase, &psCHandle, hHandle,
					       eType);
			if (eErr != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVAllocSubHandle: Lookup of existing handle failed"));
				return eErr;
			}

			PVR_ASSERT(hParentKey != NULL
				   &&
				   ParentHandle(HANDLE_TO_HANDLE_PTR
						(psBase, hHandle)) == hParent);

			if (TEST_FLAG
			    (psCHandle->eFlag & eFlag,
			     PVRSRV_HANDLE_ALLOC_FLAG_SHARED)
			    &&
			    ParentHandle(HANDLE_TO_HANDLE_PTR(psBase, hHandle))
			    == hParent) {
				*phHandle = hHandle;
				goto exit_ok;
			}
			return PVRSRV_ERROR_GENERIC;
		}
	}

	eError =
	    AllocHandle(psBase, &hHandle, pvData, eType, eFlag, hParentKey);
	if (eError != PVRSRV_OK) {
		return eError;
	}

	psPHand = HANDLE_TO_HANDLE_PTR(psBase, hParent);

	psCHand = HANDLE_TO_HANDLE_PTR(psBase, hHandle);

	AdoptChild(psBase, psPHand, psCHand);

	*phHandle = hHandle;

exit_ok:
	if (HANDLES_BATCHED(psBase)) {
		psBase->ui32BatchHandAllocFailures--;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVFindHandle(PVRSRV_HANDLE_BASE * psBase, void **phHandle,
			      void *pvData, PVRSRV_HANDLE_TYPE eType)
{
	void *hHandle;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	hHandle = (void *)FindHandle(psBase, pvData, eType, NULL);
	if (hHandle == NULL) {
		return PVRSRV_ERROR_GENERIC;
	}

	*phHandle = hHandle;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVLookupHandleAnyType(PVRSRV_HANDLE_BASE * psBase,
				       void **ppvData,
				       PVRSRV_HANDLE_TYPE * peType,
				       void *hHandle)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	eError =
	    GetHandleStructure(psBase, &psHandle, hHandle,
			       PVRSRV_HANDLE_TYPE_NONE);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVLookupHandleAnyType: Error looking up handle (%d)",
			 eError));
		return eError;
	}

	*ppvData = psHandle->pvData;
	*peType = psHandle->eType;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVLookupHandle(PVRSRV_HANDLE_BASE * psBase, void **ppvData,
				void *hHandle, PVRSRV_HANDLE_TYPE eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVLookupHandle: Error looking up handle (%d)",
			 eError));
		return eError;
	}

	*ppvData = psHandle->pvData;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVLookupSubHandle(PVRSRV_HANDLE_BASE * psBase, void **ppvData,
				   void *hHandle, PVRSRV_HANDLE_TYPE eType,
				   void *hAncestor)
{
	struct sHandle *psPHand;
	struct sHandle *psCHand;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	eError = GetHandleStructure(psBase, &psCHand, hHandle, eType);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVLookupSubHandle: Error looking up subhandle (%d)",
			 eError));
		return eError;
	}

	for (psPHand = psCHand; ParentHandle(psPHand) != hAncestor;) {
		eError =
		    GetHandleStructure(psBase, &psPHand, ParentHandle(psPHand),
				       PVRSRV_HANDLE_TYPE_NONE);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVLookupSubHandle: Subhandle doesn't belong to given ancestor"));
			return PVRSRV_ERROR_GENERIC;
		}
	}

	*ppvData = psCHand->pvData;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVGetParentHandle(PVRSRV_HANDLE_BASE * psBase, void **phParent,
				   void *hHandle, PVRSRV_HANDLE_TYPE eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVGetParentHandle: Error looking up subhandle (%d)",
			 eError));
		return eError;
	}

	*phParent = ParentHandle(psHandle);

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVLookupAndReleaseHandle(PVRSRV_HANDLE_BASE * psBase,
					  void **ppvData, void *hHandle,
					  PVRSRV_HANDLE_TYPE eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVLookupAndReleaseHandle: Error looking up handle (%d)",
			 eError));
		return eError;
	}

	*ppvData = psHandle->pvData;

	eError = FreeHandle(psBase, psHandle);

	return eError;
}

PVRSRV_ERROR PVRSRVReleaseHandle(PVRSRV_HANDLE_BASE * psBase, void *hHandle,
				 PVRSRV_HANDLE_TYPE eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	PVR_ASSERT(eType != PVRSRV_HANDLE_TYPE_NONE);

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVReleaseHandle: Error looking up handle (%d)",
			 eError));
		return eError;
	}

	eError = FreeHandle(psBase, psHandle);

	return eError;
}

PVRSRV_ERROR PVRSRVNewHandleBatch(PVRSRV_HANDLE_BASE * psBase,
				  u32 ui32BatchSize)
{
	PVRSRV_ERROR eError;

	if (HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVNewHandleBatch: There is a handle batch already in use (size %u)",
			 psBase->ui32HandBatchSize));
		return PVRSRV_ERROR_GENERIC;
	}

	if (ui32BatchSize == 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVNewHandleBatch: Invalid batch size (%u)",
			 ui32BatchSize));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError = EnsureFreeHandles(psBase, ui32BatchSize);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVNewHandleBatch: EnsureFreeHandles failed (error %d)",
			 eError));
		return eError;
	}

	psBase->ui32HandBatchSize = ui32BatchSize;

	psBase->ui32TotalHandCountPreBatch = psBase->ui32TotalHandCount;

	PVR_ASSERT(psBase->ui32BatchHandAllocFailures == 0);

	PVR_ASSERT(psBase->ui32FirstBatchIndexPlusOne == 0);

	PVR_ASSERT(HANDLES_BATCHED(psBase));

	return PVRSRV_OK;
}

static PVRSRV_ERROR PVRSRVHandleBatchCommitOrRelease(PVRSRV_HANDLE_BASE *
						     psBase, int bCommit)
{

	u32 ui32IndexPlusOne;
	int bCommitBatch = bCommit;

	if (!HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVHandleBatchCommitOrRelease: There is no handle batch"));
		return PVRSRV_ERROR_INVALID_PARAMS;

	}

	if (psBase->ui32BatchHandAllocFailures != 0) {
		if (bCommit) {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVHandleBatchCommitOrRelease: Attempting to commit batch with handle allocation failures."));
		}
		bCommitBatch = 0;
	}

	PVR_ASSERT(psBase->ui32BatchHandAllocFailures == 0 || !bCommit);

	ui32IndexPlusOne = psBase->ui32FirstBatchIndexPlusOne;
	while (ui32IndexPlusOne != 0) {
		struct sHandle *psHandle =
		    INDEX_TO_HANDLE_PTR(psBase, ui32IndexPlusOne - 1);
		u32 ui32NextIndexPlusOne = psHandle->ui32NextIndexPlusOne;
		PVR_ASSERT(BATCHED_HANDLE(psHandle));

		psHandle->ui32NextIndexPlusOne = 0;

		if (!bCommitBatch || BATCHED_HANDLE_PARTIALLY_FREE(psHandle)) {
			PVRSRV_ERROR eError;

			if (!BATCHED_HANDLE_PARTIALLY_FREE(psHandle)) {
				SET_UNBATCHED_HANDLE(psHandle);
			}

			eError = FreeHandle(psBase, psHandle);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "PVRSRVHandleBatchCommitOrRelease: Error freeing handle (%d)",
					 eError));
			}
			PVR_ASSERT(eError == PVRSRV_OK);
		} else {
			SET_UNBATCHED_HANDLE(psHandle);
		}

		ui32IndexPlusOne = ui32NextIndexPlusOne;
	}

#ifdef DEBUG
	if (psBase->ui32TotalHandCountPreBatch != psBase->ui32TotalHandCount) {
		u32 ui32Delta =
		    psBase->ui32TotalHandCount -
		    psBase->ui32TotalHandCountPreBatch;

		PVR_ASSERT(psBase->ui32TotalHandCount >
			   psBase->ui32TotalHandCountPreBatch);

		PVR_DPF((PVR_DBG_WARNING,
			 "PVRSRVHandleBatchCommitOrRelease: The batch size was too small.  Batch size was %u, but needs to be %u",
			 psBase->ui32HandBatchSize,
			 psBase->ui32HandBatchSize + ui32Delta));

	}
#endif

	psBase->ui32HandBatchSize = 0;
	psBase->ui32FirstBatchIndexPlusOne = 0;
	psBase->ui32TotalHandCountPreBatch = 0;
	psBase->ui32BatchHandAllocFailures = 0;

	if (psBase->ui32BatchHandAllocFailures != 0 && bCommit) {
		PVR_ASSERT(!bCommitBatch);

		return PVRSRV_ERROR_GENERIC;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVCommitHandleBatch(PVRSRV_HANDLE_BASE * psBase)
{
	return PVRSRVHandleBatchCommitOrRelease(psBase, 1);
}

void PVRSRVReleaseHandleBatch(PVRSRV_HANDLE_BASE * psBase)
{
	(void)PVRSRVHandleBatchCommitOrRelease(psBase, 0);
}

PVRSRV_ERROR PVRSRVSetMaxHandle(PVRSRV_HANDLE_BASE * psBase, u32 ui32MaxHandle)
{
	if (HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVSetMaxHandle: Limit cannot be set whilst in batch mode"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32MaxHandle == 0 || ui32MaxHandle >= DEFAULT_MAX_HANDLE) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVSetMaxHandle: Limit must be between %u and %u, inclusive",
			 0, DEFAULT_MAX_HANDLE));

		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (psBase->ui32TotalHandCount != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVSetMaxHandle: Limit cannot be set becuase handles have already been allocated"));

		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psBase->ui32MaxIndexPlusOne = ui32MaxHandle;

	return PVRSRV_OK;
}

u32 PVRSRVGetMaxHandle(PVRSRV_HANDLE_BASE * psBase)
{
	return psBase->ui32MaxIndexPlusOne;
}

PVRSRV_ERROR PVRSRVEnableHandlePurging(PVRSRV_HANDLE_BASE * psBase)
{
	if (psBase->bPurgingEnabled) {
		PVR_DPF((PVR_DBG_WARNING,
			 "PVRSRVEnableHandlePurging: Purging already enabled"));
		return PVRSRV_OK;
	}

	if (psBase->ui32TotalHandCount != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVEnableHandlePurging: Handles have already been allocated"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psBase->bPurgingEnabled = 1;

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVPurgeHandles(PVRSRV_HANDLE_BASE * psBase)
{
	u32 ui32Handle;
	u32 ui32NewHandCount;

	if (!psBase->bPurgingEnabled) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVPurgeHandles: Purging not enabled for this handle base"));
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if (HANDLES_BATCHED(psBase)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVPurgeHandles: Purging not allowed whilst in batch mode"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	for (ui32Handle = psBase->ui32TotalHandCount; ui32Handle != 0;
	     ui32Handle--) {
		struct sHandle *psHandle =
		    HANDLE_TO_HANDLE_PTR(psBase, ui32Handle);
		if (!HANDLE_STRUCT_IS_FREE(psHandle)) {
			break;
		}
	}

	ui32NewHandCount = ROUND_UP_TO_MULTIPLE(ui32Handle, HANDLE_BLOCK_SIZE);

	if (ui32NewHandCount >= ui32Handle
	    && ui32NewHandCount <= (psBase->ui32TotalHandCount / 2)) {
		u32 ui32Delta = psBase->ui32TotalHandCount - ui32NewHandCount;
		PVRSRV_ERROR eError;

		eError =
		    ReallocHandleArray(psBase, ui32NewHandCount,
				       psBase->ui32TotalHandCount);
		if (eError != PVRSRV_OK) {
			return eError;
		}

		psBase->ui32TotalHandCount = ui32NewHandCount;
		psBase->ui32FreeHandCount -= ui32Delta;
		psBase->ui32FirstFreeIndex = 0;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVAllocHandleBase(PVRSRV_HANDLE_BASE ** ppsBase)
{
	PVRSRV_HANDLE_BASE *psBase;
	void *hBlockAlloc;
	PVRSRV_ERROR eError;

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			    sizeof(*psBase),
			    (void **)&psBase, &hBlockAlloc, "Handle Base");
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVAllocHandleBase: Couldn't allocate handle base (%d)",
			 eError));
		return eError;
	}
	memset(psBase, 0, sizeof(*psBase));

	psBase->psHashTab =
	    HASH_Create_Extended(HANDLE_HASH_TAB_INIT_SIZE, sizeof(HAND_KEY),
				 HASH_Func_Default, HASH_Key_Comp_Default);
	if (psBase->psHashTab == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVAllocHandleBase: Couldn't create data pointer hash table\n"));
		goto failure;
	}

	psBase->hBaseBlockAlloc = hBlockAlloc;

	psBase->ui32MaxIndexPlusOne = DEFAULT_MAX_INDEX_PLUS_ONE;

	*ppsBase = psBase;

	return PVRSRV_OK;
failure:
	(void)PVRSRVFreeHandleBase(psBase);
	return PVRSRV_ERROR_GENERIC;
}

PVRSRV_ERROR PVRSRVFreeHandleBase(PVRSRV_HANDLE_BASE * psBase)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(psBase != gpsKernelHandleBase);

	eError = FreeHandleBase(psBase);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVFreeHandleBase: FreeHandleBase failed (%d)",
			 eError));
	}

	return eError;
}

PVRSRV_ERROR PVRSRVHandleInit(void)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(gpsKernelHandleBase == NULL);

	eError = PVRSRVAllocHandleBase(&gpsKernelHandleBase);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVHandleInit: PVRSRVAllocHandleBase failed (%d)",
			 eError));
		goto error;
	}

	eError = PVRSRVEnableHandlePurging(gpsKernelHandleBase);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVHandleInit: PVRSRVEnableHandlePurging failed (%d)",
			 eError));
		goto error;
	}

	return PVRSRV_OK;
error:
	(void)PVRSRVHandleDeInit();
	return eError;
}

PVRSRV_ERROR PVRSRVHandleDeInit(void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (gpsKernelHandleBase != NULL) {
		eError = FreeHandleBase(gpsKernelHandleBase);
		if (eError == PVRSRV_OK) {
			gpsKernelHandleBase = NULL;
		} else {
			PVR_DPF((PVR_DBG_ERROR,
				 "PVRSRVHandleDeInit: FreeHandleBase failed (%d)",
				 eError));
		}
	}

	return eError;
}
#else
#endif

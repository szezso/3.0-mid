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
#include "hash.h"
#include "ra.h"
#include "buffer_manager.h"
#include "osfunc.h"

#ifdef __linux__
#include <linux/kernel.h>
#include "proc.h"
#endif

#ifdef USE_BM_FREESPACE_CHECK
#include <stdio.h>
#endif

#define MINIMUM_HASH_SIZE (64)

#if defined(VALIDATE_ARENA_TEST)

typedef enum RESOURCE_DESCRIPTOR_TAG {

	RESOURCE_SPAN_LIVE = 10,
	RESOURCE_SPAN_FREE,
	IMPORTED_RESOURCE_SPAN_START,
	IMPORTED_RESOURCE_SPAN_LIVE,
	IMPORTED_RESOURCE_SPAN_FREE,
	IMPORTED_RESOURCE_SPAN_END,

} RESOURCE_DESCRIPTOR;

typedef enum RESOURCE_TYPE_TAG {

	IMPORTED_RESOURCE_TYPE = 20,
	NON_IMPORTED_RESOURCE_TYPE
} RESOURCE_TYPE;

static u32 ui32BoundaryTagID = 0;

u32 ValidateArena(RA_ARENA * pArena);
#endif

struct _BT_ {
	enum bt_type {
		btt_span,
		btt_free,
		btt_live
	} type;

	u32 base;
	u32 uSize;

	struct _BT_ *pNextSegment;
	struct _BT_ *pPrevSegment;

	struct _BT_ *pNextFree;
	struct _BT_ *pPrevFree;

	BM_MAPPING *psMapping;

#if defined(VALIDATE_ARENA_TEST)
	RESOURCE_DESCRIPTOR eResourceSpan;
	RESOURCE_TYPE eResourceType;

	u32 ui32BoundaryTagID;
#endif

};
typedef struct _BT_ BT;

struct _RA_ARENA_ {

	char *name;

	u32 uQuantum;

	int (*pImportAlloc) (void *,
			     u32 uSize,
			     u32 * pActualSize,
			     BM_MAPPING ** ppsMapping, u32 uFlags, u32 * pBase);
	void (*pImportFree) (void *, u32, BM_MAPPING * psMapping);
	void (*pBackingStoreFree) (void *, u32, u32, void *);

	void *pImportHandle;

#define FREE_TABLE_LIMIT 32

	BT *aHeadFree[FREE_TABLE_LIMIT];

	BT *pHeadSegment;
	BT *pTailSegment;

	HASH_TABLE *pSegmentHash;

#ifdef RA_STATS
	RA_STATISTICS sStatistics;
#endif

#if defined(CONFIG_PROC_FS) && defined(DEBUG)
#define PROC_NAME_SIZE		32

#ifdef PVR_PROC_USE_SEQ_FILE
	struct proc_dir_entry *pProcInfo;
	struct proc_dir_entry *pProcSegs;
#else
	char szProcInfoName[PROC_NAME_SIZE];
	char szProcSegsName[PROC_NAME_SIZE];
#endif

	int bInitProcEntry;
#endif
};
#if defined(ENABLE_RA_DUMP)
void RA_Dump(RA_ARENA * pArena);
#endif

#if defined(CONFIG_PROC_FS) && defined(DEBUG)

#ifdef PVR_PROC_USE_SEQ_FILE

static void RA_ProcSeqShowInfo(struct seq_file *sfile, void *el);
static void *RA_ProcSeqOff2ElementInfo(struct seq_file *sfile, loff_t off);

static void RA_ProcSeqShowRegs(struct seq_file *sfile, void *el);
static void *RA_ProcSeqOff2ElementRegs(struct seq_file *sfile, loff_t off);

#else
static int
RA_DumpSegs(char *page, char **start, off_t off, int count, int *eof,
	    void *data);
static int RA_DumpInfo(char *page, char **start, off_t off, int count, int *eof,
		       void *data);
#endif

#endif

#ifdef USE_BM_FREESPACE_CHECK
void CheckBMFreespace(void);
#endif

#if defined(CONFIG_PROC_FS) && defined(DEBUG)
static char *ReplaceSpaces(char *const pS)
{
	char *pT;

	for (pT = pS; *pT != 0; pT++) {
		if (*pT == ' ' || *pT == '\t') {
			*pT = '_';
		}
	}

	return pS;
}
#endif

static int
_RequestAllocFail(void *_h,
		  u32 _uSize,
		  u32 * _pActualSize,
		  BM_MAPPING ** _ppsMapping, u32 _uFlags, u32 * _pBase)
{
	return 0;
}

static u32 pvr_log2(u32 n)
{
	u32 l = 0;
	n >>= 1;
	while (n > 0) {
		n >>= 1;
		l++;
	}
	return l;
}

static PVRSRV_ERROR
_SegmentListInsertAfter(RA_ARENA * pArena, BT * pInsertionPoint, BT * pBT)
{
	PVR_ASSERT(pArena != NULL);
	PVR_ASSERT(pInsertionPoint != NULL);

	if ((pInsertionPoint == NULL) || (pArena == NULL)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "_SegmentListInsertAfter: invalid parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	pBT->pNextSegment = pInsertionPoint->pNextSegment;
	pBT->pPrevSegment = pInsertionPoint;
	if (pInsertionPoint->pNextSegment == NULL)
		pArena->pTailSegment = pBT;
	else
		pInsertionPoint->pNextSegment->pPrevSegment = pBT;
	pInsertionPoint->pNextSegment = pBT;

	return PVRSRV_OK;
}

static PVRSRV_ERROR _SegmentListInsert(RA_ARENA * pArena, BT * pBT)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (pArena->pHeadSegment == NULL) {
		pArena->pHeadSegment = pArena->pTailSegment = pBT;
		pBT->pNextSegment = pBT->pPrevSegment = NULL;
	} else {
		BT *pBTScan;

		if (pBT->base < pArena->pHeadSegment->base) {

			pBT->pNextSegment = pArena->pHeadSegment;
			pArena->pHeadSegment->pPrevSegment = pBT;
			pArena->pHeadSegment = pBT;
			pBT->pPrevSegment = NULL;
		} else {

			pBTScan = pArena->pHeadSegment;

			while ((pBTScan->pNextSegment != NULL)
			       && (pBT->base >= pBTScan->pNextSegment->base)) {
				pBTScan = pBTScan->pNextSegment;
			}

			eError = _SegmentListInsertAfter(pArena, pBTScan, pBT);
			if (eError != PVRSRV_OK) {
				return eError;
			}
		}
	}
	return eError;
}

static void _SegmentListRemove(RA_ARENA * pArena, BT * pBT)
{
	if (pBT->pPrevSegment == NULL)
		pArena->pHeadSegment = pBT->pNextSegment;
	else
		pBT->pPrevSegment->pNextSegment = pBT->pNextSegment;

	if (pBT->pNextSegment == NULL)
		pArena->pTailSegment = pBT->pPrevSegment;
	else
		pBT->pNextSegment->pPrevSegment = pBT->pPrevSegment;
}

static BT *_SegmentSplit(RA_ARENA * pArena, BT * pBT, u32 uSize)
{
	BT *pNeighbour;

	PVR_ASSERT(pArena != NULL);

	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "_SegmentSplit: invalid parameter - pArena"));
		return NULL;
	}

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(BT),
		       (void **)&pNeighbour, NULL,
		       "Boundary Tag") != PVRSRV_OK) {
		return NULL;
	}

	memset(pNeighbour, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pNeighbour->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pNeighbour->pPrevSegment = pBT;
	pNeighbour->pNextSegment = pBT->pNextSegment;
	if (pBT->pNextSegment == NULL)
		pArena->pTailSegment = pNeighbour;
	else
		pBT->pNextSegment->pPrevSegment = pNeighbour;
	pBT->pNextSegment = pNeighbour;

	pNeighbour->type = btt_free;
	pNeighbour->uSize = pBT->uSize - uSize;
	pNeighbour->base = pBT->base + uSize;
	pNeighbour->psMapping = pBT->psMapping;
	pBT->uSize = uSize;

#if defined(VALIDATE_ARENA_TEST)
	if (pNeighbour->pPrevSegment->eResourceType == IMPORTED_RESOURCE_TYPE) {
		pNeighbour->eResourceType = IMPORTED_RESOURCE_TYPE;
		pNeighbour->eResourceSpan = IMPORTED_RESOURCE_SPAN_FREE;
	} else if (pNeighbour->pPrevSegment->eResourceType ==
		   NON_IMPORTED_RESOURCE_TYPE) {
		pNeighbour->eResourceType = NON_IMPORTED_RESOURCE_TYPE;
		pNeighbour->eResourceSpan = RESOURCE_SPAN_FREE;
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "_SegmentSplit: pNeighbour->pPrevSegment->eResourceType unrecognized"));
		PVR_DBG_BREAK;
	}
#endif

	return pNeighbour;
}

static void _FreeListInsert(RA_ARENA * pArena, BT * pBT)
{
	u32 uIndex;
	uIndex = pvr_log2(pBT->uSize);
	pBT->type = btt_free;
	pBT->pNextFree = pArena->aHeadFree[uIndex];
	pBT->pPrevFree = NULL;
	if (pArena->aHeadFree[uIndex] != NULL)
		pArena->aHeadFree[uIndex]->pPrevFree = pBT;
	pArena->aHeadFree[uIndex] = pBT;
}

static void _FreeListRemove(RA_ARENA * pArena, BT * pBT)
{
	u32 uIndex;
	uIndex = pvr_log2(pBT->uSize);
	if (pBT->pNextFree != NULL)
		pBT->pNextFree->pPrevFree = pBT->pPrevFree;
	if (pBT->pPrevFree == NULL)
		pArena->aHeadFree[uIndex] = pBT->pNextFree;
	else
		pBT->pPrevFree->pNextFree = pBT->pNextFree;
}

static BT *_BuildSpanMarker(u32 base, u32 uSize)
{
	BT *pBT;

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(BT),
		       (void **)&pBT, NULL, "Boundary Tag") != PVRSRV_OK) {
		return NULL;
	}

	memset(pBT, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pBT->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pBT->type = btt_span;
	pBT->base = base;
	pBT->uSize = uSize;
	pBT->psMapping = NULL;

	return pBT;
}

static BT *_BuildBT(u32 base, u32 uSize)
{
	BT *pBT;

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(BT),
		       (void **)&pBT, NULL, "Boundary Tag") != PVRSRV_OK) {
		return NULL;
	}

	memset(pBT, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pBT->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pBT->type = btt_free;
	pBT->base = base;
	pBT->uSize = uSize;

	return pBT;
}

static BT *_InsertResource(RA_ARENA * pArena, u32 base, u32 uSize)
{
	BT *pBT;
	PVR_ASSERT(pArena != NULL);
	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "_InsertResource: invalid parameter - pArena"));
		return NULL;
	}

	pBT = _BuildBT(base, uSize);
	if (pBT != NULL) {

#if defined(VALIDATE_ARENA_TEST)
		pBT->eResourceSpan = RESOURCE_SPAN_FREE;
		pBT->eResourceType = NON_IMPORTED_RESOURCE_TYPE;
#endif

		if (_SegmentListInsert(pArena, pBT) != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "_InsertResource: call to _SegmentListInsert failed"));
			return NULL;
		}
		_FreeListInsert(pArena, pBT);
#ifdef RA_STATS
		pArena->sStatistics.uTotalResourceCount += uSize;
		pArena->sStatistics.uFreeResourceCount += uSize;
		pArena->sStatistics.uSpanCount++;
#endif
	}
	return pBT;
}

static BT *_InsertResourceSpan(RA_ARENA * pArena, u32 base, u32 uSize)
{
	PVRSRV_ERROR eError;
	BT *pSpanStart;
	BT *pSpanEnd;
	BT *pBT;

	PVR_ASSERT(pArena != NULL);
	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "_InsertResourceSpan: invalid parameter - pArena"));
		return NULL;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_InsertResourceSpan: arena='%s', base=0x%x, size=0x%x",
		 pArena->name, base, uSize));

	pSpanStart = _BuildSpanMarker(base, uSize);
	if (pSpanStart == NULL) {
		goto fail_start;
	}
#if defined(VALIDATE_ARENA_TEST)
	pSpanStart->eResourceSpan = IMPORTED_RESOURCE_SPAN_START;
	pSpanStart->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	pSpanEnd = _BuildSpanMarker(base + uSize, 0);
	if (pSpanEnd == NULL) {
		goto fail_end;
	}
#if defined(VALIDATE_ARENA_TEST)
	pSpanEnd->eResourceSpan = IMPORTED_RESOURCE_SPAN_END;
	pSpanEnd->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	pBT = _BuildBT(base, uSize);
	if (pBT == NULL) {
		goto fail_bt;
	}
#if defined(VALIDATE_ARENA_TEST)
	pBT->eResourceSpan = IMPORTED_RESOURCE_SPAN_FREE;
	pBT->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	eError = _SegmentListInsert(pArena, pSpanStart);
	if (eError != PVRSRV_OK) {
		goto fail_SegListInsert;
	}

	eError = _SegmentListInsertAfter(pArena, pSpanStart, pBT);
	if (eError != PVRSRV_OK) {
		goto fail_SegListInsert;
	}

	_FreeListInsert(pArena, pBT);

	eError = _SegmentListInsertAfter(pArena, pBT, pSpanEnd);
	if (eError != PVRSRV_OK) {
		goto fail_SegListInsert;
	}
#ifdef RA_STATS
	pArena->sStatistics.uTotalResourceCount += uSize;
#endif
	return pBT;

fail_SegListInsert:
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pBT, NULL);

fail_bt:
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pSpanEnd, NULL);

fail_end:
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pSpanStart, NULL);

fail_start:
	return NULL;
}

static void _FreeBT(RA_ARENA * pArena, BT * pBT, int bFreeBackingStore)
{
	BT *pNeighbour;
	u32 uOrigBase;
	u32 uOrigSize;

	PVR_ASSERT(pArena != NULL);
	PVR_ASSERT(pBT != NULL);

	if ((pArena == NULL) || (pBT == NULL)) {
		PVR_DPF((PVR_DBG_ERROR, "_FreeBT: invalid parameter"));
		return;
	}
#ifdef RA_STATS
	pArena->sStatistics.uLiveSegmentCount--;
	pArena->sStatistics.uFreeSegmentCount++;
	pArena->sStatistics.uFreeResourceCount += pBT->uSize;
#endif

	uOrigBase = pBT->base;
	uOrigSize = pBT->uSize;

	pNeighbour = pBT->pPrevSegment;
	if (pNeighbour != NULL
	    && pNeighbour->type == btt_free
	    && pNeighbour->base + pNeighbour->uSize == pBT->base) {
		_FreeListRemove(pArena, pNeighbour);
		_SegmentListRemove(pArena, pNeighbour);
		pBT->base = pNeighbour->base;
		pBT->uSize += pNeighbour->uSize;
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pNeighbour,
			  NULL);

#ifdef RA_STATS
		pArena->sStatistics.uFreeSegmentCount--;
#endif
	}

	pNeighbour = pBT->pNextSegment;
	if (pNeighbour != NULL
	    && pNeighbour->type == btt_free
	    && pBT->base + pBT->uSize == pNeighbour->base) {
		_FreeListRemove(pArena, pNeighbour);
		_SegmentListRemove(pArena, pNeighbour);
		pBT->uSize += pNeighbour->uSize;
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pNeighbour,
			  NULL);

#ifdef RA_STATS
		pArena->sStatistics.uFreeSegmentCount--;
#endif
	}

	if (pArena->pBackingStoreFree != NULL && bFreeBackingStore) {
		u32 uRoundedStart, uRoundedEnd;

		uRoundedStart =
		    (uOrigBase / pArena->uQuantum) * pArena->uQuantum;

		if (uRoundedStart < pBT->base) {
			uRoundedStart += pArena->uQuantum;
		}

		uRoundedEnd =
		    ((uOrigBase + uOrigSize + pArena->uQuantum -
		      1) / pArena->uQuantum) * pArena->uQuantum;

		if (uRoundedEnd > (pBT->base + pBT->uSize)) {
			uRoundedEnd -= pArena->uQuantum;
		}

		if (uRoundedStart < uRoundedEnd) {
			pArena->pBackingStoreFree(pArena->pImportHandle,
						  uRoundedStart, uRoundedEnd,
						  (void *)0);
		}
	}

	if (pBT->pNextSegment != NULL && pBT->pNextSegment->type == btt_span
	    && pBT->pPrevSegment != NULL && pBT->pPrevSegment->type == btt_span)
	{
		BT *next = pBT->pNextSegment;
		BT *prev = pBT->pPrevSegment;
		_SegmentListRemove(pArena, next);
		_SegmentListRemove(pArena, prev);
		_SegmentListRemove(pArena, pBT);
		pArena->pImportFree(pArena->pImportHandle, pBT->base,
				    pBT->psMapping);
#ifdef RA_STATS
		pArena->sStatistics.uSpanCount--;
		pArena->sStatistics.uExportCount++;
		pArena->sStatistics.uFreeSegmentCount--;
		pArena->sStatistics.uFreeResourceCount -= pBT->uSize;
		pArena->sStatistics.uTotalResourceCount -= pBT->uSize;
#endif
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), next, NULL);

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), prev, NULL);

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pBT, NULL);

	} else
		_FreeListInsert(pArena, pBT);
}

static int
_AttemptAllocAligned(RA_ARENA * pArena,
		     u32 uSize,
		     BM_MAPPING ** ppsMapping,
		     u32 uFlags,
		     u32 uAlignment, u32 uAlignmentOffset, u32 * base)
{
	u32 uIndex;
	PVR_ASSERT(pArena != NULL);
	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "_AttemptAllocAligned: invalid parameter - pArena"));
		return 0;
	}

	if (uAlignment > 1)
		uAlignmentOffset %= uAlignment;

	uIndex = pvr_log2(uSize);

#if 0

	if (1u << uIndex < uSize)
		uIndex++;
#endif

	while (uIndex < FREE_TABLE_LIMIT && pArena->aHeadFree[uIndex] == NULL)
		uIndex++;

	while (uIndex < FREE_TABLE_LIMIT) {
		if (pArena->aHeadFree[uIndex] != NULL) {

			BT *pBT;

			pBT = pArena->aHeadFree[uIndex];
			while (pBT != NULL) {
				u32 aligned_base;

				if (uAlignment > 1)
					aligned_base =
					    (pBT->base + uAlignmentOffset +
					     uAlignment -
					     1) / uAlignment * uAlignment -
					    uAlignmentOffset;
				else
					aligned_base = pBT->base;
				PVR_DPF((PVR_DBG_MESSAGE,
					 "RA_AttemptAllocAligned: pBT-base=0x%x "
					 "pBT-size=0x%x alignedbase=0x%x size=0x%x",
					 pBT->base, pBT->uSize, aligned_base,
					 uSize));

				if (pBT->base + pBT->uSize >=
				    aligned_base + uSize) {
					if (!pBT->psMapping
					    || pBT->psMapping->ui32Flags ==
					    uFlags) {
						_FreeListRemove(pArena, pBT);

						PVR_ASSERT(pBT->type ==
							   btt_free);

#ifdef RA_STATS
						pArena->sStatistics.
						    uLiveSegmentCount++;
						pArena->sStatistics.
						    uFreeSegmentCount--;
						pArena->sStatistics.
						    uFreeResourceCount -=
						    pBT->uSize;
#endif

						if (aligned_base > pBT->base) {
							BT *pNeighbour;

							pNeighbour =
							    _SegmentSplit
							    (pArena, pBT,
							     aligned_base -
							     pBT->base);

							if (pNeighbour == NULL) {
								PVR_DPF((PVR_DBG_ERROR, "_AttemptAllocAligned: Front split failed"));

								_FreeListInsert
								    (pArena,
								     pBT);
								return 0;
							}

							_FreeListInsert(pArena,
									pBT);
#ifdef RA_STATS
							pArena->sStatistics.
							    uFreeSegmentCount++;
							pArena->sStatistics.
							    uFreeResourceCount
							    += pBT->uSize;
#endif
							pBT = pNeighbour;
						}

						if (pBT->uSize > uSize) {
							BT *pNeighbour;
							pNeighbour =
							    _SegmentSplit
							    (pArena, pBT,
							     uSize);

							if (pNeighbour == NULL) {
								PVR_DPF((PVR_DBG_ERROR, "_AttemptAllocAligned: Back split failed"));

								_FreeListInsert
								    (pArena,
								     pBT);
								return 0;
							}

							_FreeListInsert(pArena,
									pNeighbour);
#ifdef RA_STATS
							pArena->sStatistics.
							    uFreeSegmentCount++;
							pArena->sStatistics.
							    uFreeResourceCount
							    +=
							    pNeighbour->uSize;
#endif
						}

						pBT->type = btt_live;

#if defined(VALIDATE_ARENA_TEST)
						if (pBT->eResourceType ==
						    IMPORTED_RESOURCE_TYPE) {
							pBT->eResourceSpan =
							    IMPORTED_RESOURCE_SPAN_LIVE;
						} else if (pBT->eResourceType ==
							   NON_IMPORTED_RESOURCE_TYPE)
						{
							pBT->eResourceSpan =
							    RESOURCE_SPAN_LIVE;
						} else {
							PVR_DPF((PVR_DBG_ERROR,
								 "_AttemptAllocAligned ERROR: pBT->eResourceType unrecognized"));
							PVR_DBG_BREAK;
						}
#endif
						if (!HASH_Insert
						    (pArena->pSegmentHash,
						     pBT->base, (u32) pBT)) {
							_FreeBT(pArena, pBT, 0);
							return 0;
						}

						if (ppsMapping != NULL)
							*ppsMapping =
							    pBT->psMapping;

						*base = pBT->base;

						return 1;
					} else {
						PVR_DPF((PVR_DBG_MESSAGE,
							 "AttemptAllocAligned: mismatch in flags. Import has %x, request was %x",
							 pBT->psMapping->
							 ui32Flags, uFlags));

					}
				}
				pBT = pBT->pNextFree;
			}

		}
		uIndex++;
	}

	return 0;
}

RA_ARENA *RA_Create(char *name,
		    u32 base,
		    u32 uSize,
		    BM_MAPPING * psMapping,
		    u32 uQuantum,
		    int (*imp_alloc) (void *, u32 uSize, u32 * pActualSize,
				      BM_MAPPING ** ppsMapping, u32 _flags,
				      u32 * pBase), void (*imp_free) (void *,
								      u32,
								      BM_MAPPING
								      *),
		    void (*backingstore_free) (void *, u32, u32, void *),
		    void *pImportHandle)
{
	RA_ARENA *pArena;
	BT *pBT;
	int i;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_Create: name='%s', base=0x%x, uSize=0x%x, alloc=0x%x, free=0x%x",
		 name, base, uSize, imp_alloc, imp_free));

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(*pArena),
		       (void **)&pArena, NULL, "Resource Arena") != PVRSRV_OK) {
		goto arena_fail;
	}

	pArena->name = name;
	pArena->pImportAlloc =
	    (imp_alloc != NULL) ? imp_alloc : _RequestAllocFail;
	pArena->pImportFree = imp_free;
	pArena->pBackingStoreFree = backingstore_free;
	pArena->pImportHandle = pImportHandle;
	for (i = 0; i < FREE_TABLE_LIMIT; i++)
		pArena->aHeadFree[i] = NULL;
	pArena->pHeadSegment = NULL;
	pArena->pTailSegment = NULL;
	pArena->uQuantum = uQuantum;

#ifdef RA_STATS
	pArena->sStatistics.uSpanCount = 0;
	pArena->sStatistics.uLiveSegmentCount = 0;
	pArena->sStatistics.uFreeSegmentCount = 0;
	pArena->sStatistics.uFreeResourceCount = 0;
	pArena->sStatistics.uTotalResourceCount = 0;
	pArena->sStatistics.uCumulativeAllocs = 0;
	pArena->sStatistics.uCumulativeFrees = 0;
	pArena->sStatistics.uImportCount = 0;
	pArena->sStatistics.uExportCount = 0;
#endif

#if defined(CONFIG_PROC_FS) && defined(DEBUG)
	if (strcmp(pArena->name, "") != 0) {

#ifndef PVR_PROC_USE_SEQ_FILE
		int ret;
		int (*pfnCreateProcEntry) (const char *, read_proc_t,
					   write_proc_t, void *);

		pArena->bInitProcEntry =
		    !PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL);

		pfnCreateProcEntry =
		    pArena->
		    bInitProcEntry ? CreateProcEntry :
		    CreatePerProcessProcEntry;

		ret =
		    snprintf(pArena->szProcInfoName,
			     sizeof(pArena->szProcInfoName), "ra_info_%s",
			     pArena->name);
		if (ret > 0 && ret < sizeof(pArena->szProcInfoName)) {
			(void)
			    pfnCreateProcEntry(ReplaceSpaces
					       (pArena->szProcInfoName),
					       RA_DumpInfo, 0, pArena);
		} else {
			pArena->szProcInfoName[0] = 0;
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Create: couldn't create ra_info proc entry for arena %s",
				 pArena->name));
		}

		ret =
		    snprintf(pArena->szProcSegsName,
			     sizeof(pArena->szProcSegsName), "ra_segs_%s",
			     pArena->name);
		if (ret > 0 && ret < sizeof(pArena->szProcSegsName)) {
			(void)
			    pfnCreateProcEntry(ReplaceSpaces
					       (pArena->szProcSegsName),
					       RA_DumpSegs, 0, pArena);
		} else {
			pArena->szProcSegsName[0] = 0;
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Create: couldn't create ra_segs proc entry for arena %s",
				 pArena->name));
		}
#else

		int ret;
		char szProcInfoName[PROC_NAME_SIZE];
		char szProcSegsName[PROC_NAME_SIZE];
		struct proc_dir_entry *(*pfnCreateProcEntrySeq) (const char *,
								 void *,
								 pvr_next_proc_seq_t,
								 pvr_show_proc_seq_t,
								 pvr_off2element_proc_seq_t,
								 pvr_startstop_proc_seq_t,
								 write_proc_t);

		pArena->bInitProcEntry =
		    !PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL);

		pfnCreateProcEntrySeq =
		    pArena->
		    bInitProcEntry ? CreateProcEntrySeq :
		    CreatePerProcessProcEntrySeq;

		ret =
		    snprintf(szProcInfoName, sizeof(szProcInfoName),
			     "ra_info_%s", pArena->name);
		if (ret > 0 && ret < sizeof(szProcInfoName)) {
			pArena->pProcInfo =
			    pfnCreateProcEntrySeq(ReplaceSpaces(szProcInfoName),
						  pArena, NULL,
						  RA_ProcSeqShowInfo,
						  RA_ProcSeqOff2ElementInfo,
						  NULL, NULL);
		} else {
			pArena->pProcInfo = 0;
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Create: couldn't create ra_info proc entry for arena %s",
				 pArena->name));
		}

		ret =
		    snprintf(szProcSegsName, sizeof(szProcSegsName),
			     "ra_segs_%s", pArena->name);
		if (ret > 0 && ret < sizeof(szProcInfoName)) {
			pArena->pProcSegs =
			    pfnCreateProcEntrySeq(ReplaceSpaces(szProcSegsName),
						  pArena, NULL,
						  RA_ProcSeqShowRegs,
						  RA_ProcSeqOff2ElementRegs,
						  NULL, NULL);
		} else {
			pArena->pProcSegs = 0;
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Create: couldn't create ra_segs proc entry for arena %s",
				 pArena->name));
		}

#endif

	}
#endif

	pArena->pSegmentHash = HASH_Create(MINIMUM_HASH_SIZE);
	if (pArena->pSegmentHash == NULL) {
		goto hash_fail;
	}
	if (uSize > 0) {
		uSize = (uSize + uQuantum - 1) / uQuantum * uQuantum;
		pBT = _InsertResource(pArena, base, uSize);
		if (pBT == NULL) {
			goto insert_fail;
		}
		pBT->psMapping = psMapping;

	}
	return pArena;

insert_fail:
	HASH_Delete(pArena->pSegmentHash);
hash_fail:
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(RA_ARENA), pArena, NULL);

arena_fail:
	return NULL;
}

void RA_Delete(RA_ARENA * pArena)
{
	u32 uIndex;

	PVR_ASSERT(pArena != NULL);

	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "RA_Delete: invalid parameter - pArena"));
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "RA_Delete: name='%s'", pArena->name));

	for (uIndex = 0; uIndex < FREE_TABLE_LIMIT; uIndex++)
		pArena->aHeadFree[uIndex] = NULL;

	while (pArena->pHeadSegment != NULL) {
		BT *pBT = pArena->pHeadSegment;

		if (pBT->type != btt_free) {
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Delete: allocations still exist in the arena that is being destroyed"));
			PVR_DPF((PVR_DBG_ERROR,
				 "Likely Cause: client drivers not freeing alocations before destroying devmemcontext"));
			PVR_DPF((PVR_DBG_ERROR,
				 "RA_Delete: base = 0x%x size=0x%x", pBT->base,
				 pBT->uSize));
		}

		_SegmentListRemove(pArena, pBT);
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(BT), pBT, NULL);

#ifdef RA_STATS
		pArena->sStatistics.uSpanCount--;
#endif
	}
#if defined(CONFIG_PROC_FS) && defined(DEBUG)
	{

#ifdef PVR_PROC_USE_SEQ_FILE
		void (*pfnRemoveProcEntrySeq) (struct proc_dir_entry *);

		pfnRemoveProcEntrySeq =
		    pArena->
		    bInitProcEntry ? RemoveProcEntrySeq :
		    RemovePerProcessProcEntrySeq;

		if (pArena->pProcInfo != 0) {
			pfnRemoveProcEntrySeq(pArena->pProcInfo);
		}

		if (pArena->pProcSegs != 0) {
			pfnRemoveProcEntrySeq(pArena->pProcSegs);
		}
#else
		void (*pfnRemoveProcEntry) (const char *);

		pfnRemoveProcEntry =
		    pArena->
		    bInitProcEntry ? RemoveProcEntry :
		    RemovePerProcessProcEntry;

		if (pArena->szProcInfoName[0] != 0) {
			pfnRemoveProcEntry(pArena->szProcInfoName);
		}

		if (pArena->szProcSegsName[0] != 0) {
			pfnRemoveProcEntry(pArena->szProcSegsName);
		}
#endif
	}
#endif
	HASH_Delete(pArena->pSegmentHash);
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(RA_ARENA), pArena, NULL);

}

int RA_TestDelete(RA_ARENA * pArena)
{
	PVR_ASSERT(pArena != NULL);

	if (pArena != NULL) {
		while (pArena->pHeadSegment != NULL) {
			BT *pBT = pArena->pHeadSegment;
			if (pBT->type != btt_free) {
				PVR_DPF((PVR_DBG_ERROR,
					 "RA_TestDelete: detected resource leak!"));
				PVR_DPF((PVR_DBG_ERROR,
					 "RA_TestDelete: base = 0x%x size=0x%x",
					 pBT->base, pBT->uSize));
				return 0;
			}
		}
	}

	return 1;
}

int RA_Add(RA_ARENA * pArena, u32 base, u32 uSize)
{
	PVR_ASSERT(pArena != NULL);

	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "RA_Add: invalid parameter - pArena"));
		return 0;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_Add: name='%s', base=0x%x, size=0x%x", pArena->name, base,
		 uSize));

	uSize =
	    (uSize + pArena->uQuantum -
	     1) / pArena->uQuantum * pArena->uQuantum;
	return ((int)(_InsertResource(pArena, base, uSize) != NULL));
}

int
RA_Alloc(RA_ARENA * pArena,
	 u32 uRequestSize,
	 u32 * pActualSize,
	 BM_MAPPING ** ppsMapping,
	 u32 uFlags, u32 uAlignment, u32 uAlignmentOffset, u32 * base)
{
	int bResult;
	u32 uSize = uRequestSize;

	PVR_ASSERT(pArena != NULL);

	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "RA_Alloc: invalid parameter - pArena"));
		return 0;
	}
#if defined(VALIDATE_ARENA_TEST)
	ValidateArena(pArena);
#endif

#ifdef USE_BM_FREESPACE_CHECK
	CheckBMFreespace();
#endif

	if (pActualSize != NULL) {
		*pActualSize = uSize;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_Alloc: arena='%s', size=0x%x(0x%x), alignment=0x%x, offset=0x%x",
		 pArena->name, uSize, uRequestSize, uAlignment,
		 uAlignmentOffset));

	bResult = _AttemptAllocAligned(pArena, uSize, ppsMapping, uFlags,
				       uAlignment, uAlignmentOffset, base);
	if (!bResult) {
		BM_MAPPING *psImportMapping;
		u32 import_base;
		u32 uImportSize = uSize;

		if (uAlignment > pArena->uQuantum) {
			uImportSize += (uAlignment - 1);
		}

		uImportSize =
		    ((uImportSize + pArena->uQuantum -
		      1) / pArena->uQuantum) * pArena->uQuantum;

		bResult =
		    pArena->pImportAlloc(pArena->pImportHandle, uImportSize,
					 &uImportSize, &psImportMapping, uFlags,
					 &import_base);
		if (bResult) {
			BT *pBT;
			pBT =
			    _InsertResourceSpan(pArena, import_base,
						uImportSize);

			if (pBT == NULL) {

				pArena->pImportFree(pArena->pImportHandle,
						    import_base,
						    psImportMapping);
				PVR_DPF((PVR_DBG_MESSAGE,
					 "RA_Alloc: name='%s', size=0x%x failed!",
					 pArena->name, uSize));

				return 0;
			}
			pBT->psMapping = psImportMapping;
#ifdef RA_STATS
			pArena->sStatistics.uFreeSegmentCount++;
			pArena->sStatistics.uFreeResourceCount += uImportSize;
			pArena->sStatistics.uImportCount++;
			pArena->sStatistics.uSpanCount++;
#endif
			bResult =
			    _AttemptAllocAligned(pArena, uSize, ppsMapping,
						 uFlags, uAlignment,
						 uAlignmentOffset, base);
			if (!bResult) {
				PVR_DPF((PVR_DBG_MESSAGE,
					 "RA_Alloc: name='%s' uAlignment failed!",
					 pArena->name));
			}
		}
	}
#ifdef RA_STATS
	if (bResult)
		pArena->sStatistics.uCumulativeAllocs++;
#endif

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_Alloc: name='%s', size=0x%x, *base=0x%x = %d",
		 pArena->name, uSize, *base, bResult));

#if defined(VALIDATE_ARENA_TEST)
	ValidateArena(pArena);
#endif

	return bResult;
}

#if defined(VALIDATE_ARENA_TEST)

u32 ValidateArena(RA_ARENA * pArena)
{
	BT *pSegment;
	RESOURCE_DESCRIPTOR eNextSpan;

	pSegment = pArena->pHeadSegment;

	if (pSegment == NULL) {
		return 0;
	}

	if (pSegment->eResourceType == IMPORTED_RESOURCE_TYPE) {
		PVR_ASSERT(pSegment->eResourceSpan ==
			   IMPORTED_RESOURCE_SPAN_START);

		while (pSegment->pNextSegment) {
			eNextSpan = pSegment->pNextSegment->eResourceSpan;

			switch (pSegment->eResourceSpan) {
			case IMPORTED_RESOURCE_SPAN_LIVE:

				if (!
				    ((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE)
				     || (eNextSpan ==
					 IMPORTED_RESOURCE_SPAN_FREE)
				     || (eNextSpan ==
					 IMPORTED_RESOURCE_SPAN_END))) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			case IMPORTED_RESOURCE_SPAN_FREE:

				if (!
				    ((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE)
				     || (eNextSpan ==
					 IMPORTED_RESOURCE_SPAN_END))) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			case IMPORTED_RESOURCE_SPAN_END:

				if ((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE)
				    || (eNextSpan ==
					IMPORTED_RESOURCE_SPAN_FREE)
				    || (eNextSpan ==
					IMPORTED_RESOURCE_SPAN_END)) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			case IMPORTED_RESOURCE_SPAN_START:

				if (!
				    ((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE)
				     || (eNextSpan ==
					 IMPORTED_RESOURCE_SPAN_FREE))) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			default:
				PVR_DPF((PVR_DBG_ERROR,
					 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
					 pSegment->ui32BoundaryTagID,
					 pSegment->base,
					 pSegment->pNextSegment->
					 ui32BoundaryTagID,
					 pSegment->pNextSegment->base,
					 pArena->name));

				PVR_DBG_BREAK;
				break;
			}
			pSegment = pSegment->pNextSegment;
		}
	} else if (pSegment->eResourceType == NON_IMPORTED_RESOURCE_TYPE) {
		PVR_ASSERT((pSegment->eResourceSpan == RESOURCE_SPAN_FREE)
			   || (pSegment->eResourceSpan == RESOURCE_SPAN_LIVE));

		while (pSegment->pNextSegment) {
			eNextSpan = pSegment->pNextSegment->eResourceSpan;

			switch (pSegment->eResourceSpan) {
			case RESOURCE_SPAN_LIVE:

				if (!((eNextSpan == RESOURCE_SPAN_FREE) ||
				      (eNextSpan == RESOURCE_SPAN_LIVE))) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			case RESOURCE_SPAN_FREE:

				if (!((eNextSpan == RESOURCE_SPAN_FREE) ||
				      (eNextSpan == RESOURCE_SPAN_LIVE))) {

					PVR_DPF((PVR_DBG_ERROR,
						 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
						 pSegment->ui32BoundaryTagID,
						 pSegment->base,
						 pSegment->pNextSegment->
						 ui32BoundaryTagID,
						 pSegment->pNextSegment->base,
						 pArena->name));

					PVR_DBG_BREAK;
				}
				break;

			default:
				PVR_DPF((PVR_DBG_ERROR,
					 "ValidateArena ERROR: adjacent boundary tags %d (base=0x%x) and %d (base=0x%x) are incompatible (arena: %s)",
					 pSegment->ui32BoundaryTagID,
					 pSegment->base,
					 pSegment->pNextSegment->
					 ui32BoundaryTagID,
					 pSegment->pNextSegment->base,
					 pArena->name));

				PVR_DBG_BREAK;
				break;
			}
			pSegment = pSegment->pNextSegment;
		}

	} else {
		PVR_DPF((PVR_DBG_ERROR,
			 "ValidateArena ERROR: pSegment->eResourceType unrecognized"));

		PVR_DBG_BREAK;
	}

	return 0;
}

#endif

void RA_Free(RA_ARENA * pArena, u32 base, int bFreeBackingStore)
{
	BT *pBT;

	PVR_ASSERT(pArena != NULL);

	if (pArena == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "RA_Free: invalid parameter - pArena"));
		return;
	}
#ifdef USE_BM_FREESPACE_CHECK
	CheckBMFreespace();
#endif

	PVR_DPF((PVR_DBG_MESSAGE,
		 "RA_Free: name='%s', base=0x%x", pArena->name, base));

	pBT = (BT *) HASH_Remove(pArena->pSegmentHash, base);
	PVR_ASSERT(pBT != NULL);

	if (pBT) {
		PVR_ASSERT(pBT->base == base);

#ifdef RA_STATS
		pArena->sStatistics.uCumulativeFrees++;
#endif

#ifdef USE_BM_FREESPACE_CHECK
		{
			unsigned char *p;
			unsigned char *endp;

			p = (unsigned char *) pBT->base + SysGetDevicePhysOffset();
			endp = (unsigned char *) ((u32) (p + pBT->uSize));
			while ((u32) p & 3) {
				*p++ = 0xAA;
			}
			while (p < (unsigned char *) ((u32) endp & 0xfffffffc)) {
				*(u32 *) p = 0xAAAAAAAA;
				p += sizeof(u32);
			}
			while (p < endp) {
				*p++ = 0xAA;
			}
			PVR_DPF((PVR_DBG_MESSAGE,
				 "BM_FREESPACE_CHECK: RA_Free Cleared %08X to %08X (size=0x%x)",
				 (unsigned char *) pBT->base +
				 SysGetDevicePhysOffset(), endp - 1,
				 pBT->uSize));
		}
#endif
		_FreeBT(pArena, pBT, bFreeBackingStore);
	}
}

int RA_GetNextLiveSegment(void *hArena, RA_SEGMENT_DETAILS * psSegDetails)
{
	BT *pBT;

	if (psSegDetails->hSegment) {
		pBT = (BT *) psSegDetails->hSegment;
	} else {
		RA_ARENA *pArena = (RA_ARENA *) hArena;

		pBT = pArena->pHeadSegment;
	}

	while (pBT != NULL) {
		if (pBT->type == btt_live) {
			psSegDetails->uiSize = pBT->uSize;
			psSegDetails->sCpuPhyAddr.uiAddr = pBT->base;
			psSegDetails->hSegment = (void *)pBT->pNextSegment;

			return 1;
		}

		pBT = pBT->pNextSegment;
	}

	psSegDetails->uiSize = 0;
	psSegDetails->sCpuPhyAddr.uiAddr = 0;
	psSegDetails->hSegment = (void *)-1;

	return 0;
}

#ifdef USE_BM_FREESPACE_CHECK
RA_ARENA *pJFSavedArena = NULL;

void CheckBMFreespace(void)
{
	BT *pBT;
	unsigned char *p;
	unsigned char *endp;

	if (pJFSavedArena != NULL) {
		for (pBT = pJFSavedArena->pHeadSegment; pBT != NULL;
		     pBT = pBT->pNextSegment) {
			if (pBT->type == btt_free) {
				p = (unsigned char *) pBT->base +
				    SysGetDevicePhysOffset();
				endp =
				    (unsigned char *) ((u32) (p + pBT->uSize) &
						  0xfffffffc);

				while ((u32) p & 3) {
					if (*p++ != 0xAA) {
						fprintf(stderr,
							"BM_FREESPACE_CHECK: Blank space at %08X has changed to 0x%x\n",
							p, *(u32 *) p);
						for (;;) ;
						break;
					}
				}
				while (p < endp) {
					if (*(u32 *) p != 0xAAAAAAAA) {
						fprintf(stderr,
							"BM_FREESPACE_CHECK: Blank space at %08X has changed to 0x%x\n",
							p, *(u32 *) p);
						for (;;) ;
						break;
					}
					p += 4;
				}
			}
		}
	}
}
#endif

#if (defined(CONFIG_PROC_FS) && defined(DEBUG)) || defined (RA_STATS)
static char *_BTType(int eType)
{
	switch (eType) {
	case btt_span:
		return "span";
	case btt_free:
		return "free";
	case btt_live:
		return "live";
	}
	return "junk";
}
#endif

#if defined(ENABLE_RA_DUMP)
void RA_Dump(RA_ARENA * pArena)
{
	BT *pBT;
	PVR_ASSERT(pArena != NULL);
	PVR_DPF((PVR_DBG_MESSAGE, "Arena '%s':", pArena->name));
	PVR_DPF((PVR_DBG_MESSAGE,
		 "  alloc=%08X free=%08X handle=%08X quantum=%d",
		 pArena->pImportAlloc, pArena->pImportFree,
		 pArena->pImportHandle, pArena->uQuantum));
	PVR_DPF((PVR_DBG_MESSAGE, "  segment Chain:"));
	if (pArena->pHeadSegment != NULL &&
	    pArena->pHeadSegment->pPrevSegment != NULL)
		PVR_DPF((PVR_DBG_MESSAGE,
			 "  error: head boundary tag has invalid pPrevSegment"));
	if (pArena->pTailSegment != NULL
	    && pArena->pTailSegment->pNextSegment != NULL)
		PVR_DPF((PVR_DBG_MESSAGE,
			 "  error: tail boundary tag has invalid pNextSegment"));

	for (pBT = pArena->pHeadSegment; pBT != NULL; pBT = pBT->pNextSegment) {
		PVR_DPF((PVR_DBG_MESSAGE,
			 "\tbase=0x%x size=0x%x type=%s ref=%08X",
			 (u32) pBT->base, pBT->uSize, _BTType(pBT->type),
			 pBT->pRef));
	}

#ifdef HASH_TRACE
	HASH_Dump(pArena->pSegmentHash);
#endif
}
#endif

#if defined(CONFIG_PROC_FS) && defined(DEBUG)

#ifdef PVR_PROC_USE_SEQ_FILE

static void RA_ProcSeqShowInfo(struct seq_file *sfile, void *el)
{
	PVR_PROC_SEQ_HANDLERS *handlers =
	    (PVR_PROC_SEQ_HANDLERS *) sfile->private;
	RA_ARENA *pArena = (RA_ARENA *) handlers->data;
	int off = (int)el;

	switch (off) {
	case 1:
		seq_printf(sfile, "quantum\t\t\t%lu\n", pArena->uQuantum);
		break;
	case 2:
		seq_printf(sfile, "import_handle\t\t%08X\n",
			   (u32) pArena->pImportHandle);
		break;
#ifdef RA_STATS
	case 3:
		seq_printf(sfile, "span count\t\t%lu\n",
			   pArena->sStatistics.uSpanCount);
		break;
	case 4:
		seq_printf(sfile, "live segment count\t%lu\n",
			   pArena->sStatistics.uLiveSegmentCount);
		break;
	case 5:
		seq_printf(sfile, "free segment count\t%lu\n",
			   pArena->sStatistics.uFreeSegmentCount);
		break;
	case 6:
		seq_printf(sfile, "free resource count\t%lu (0x%x)\n",
			   pArena->sStatistics.uFreeResourceCount,
			   (u32) pArena->sStatistics.uFreeResourceCount);
		break;
	case 7:
		seq_printf(sfile, "total allocs\t\t%lu\n",
			   pArena->sStatistics.uCumulativeAllocs);
		break;
	case 8:
		seq_printf(sfile, "total frees\t\t%lu\n",
			   pArena->sStatistics.uCumulativeFrees);
		break;
	case 9:
		seq_printf(sfile, "import count\t\t%lu\n",
			   pArena->sStatistics.uImportCount);
		break;
	case 10:
		seq_printf(sfile, "export count\t\t%lu\n",
			   pArena->sStatistics.uExportCount);
		break;
#endif
	}

}

static void *RA_ProcSeqOff2ElementInfo(struct seq_file *sfile, loff_t off)
{
#ifdef RA_STATS
	if (off <= 9)
#else
	if (off <= 1)
#endif
		return (void *)(int)(off + 1);
	return 0;
}

static void RA_ProcSeqShowRegs(struct seq_file *sfile, void *el)
{
	PVR_PROC_SEQ_HANDLERS *handlers =
	    (PVR_PROC_SEQ_HANDLERS *) sfile->private;
	RA_ARENA *pArena = (RA_ARENA *) handlers->data;
	BT *pBT = (BT *) el;

	if (el == PVR_PROC_SEQ_START_TOKEN) {
		seq_printf(sfile, "Arena \"%s\"\nBase         Size Type Ref\n",
			   pArena->name);
		return;
	}

	if (pBT) {
		seq_printf(sfile, "%08x %8x %4s %08x\n",
			   (u32) pBT->base, (u32) pBT->uSize,
			   _BTType(pBT->type), (u32) pBT->psMapping);
	}
}

static void *RA_ProcSeqOff2ElementRegs(struct seq_file *sfile, loff_t off)
{
	PVR_PROC_SEQ_HANDLERS *handlers =
	    (PVR_PROC_SEQ_HANDLERS *) sfile->private;
	RA_ARENA *pArena = (RA_ARENA *) handlers->data;
	BT *pBT = 0;

	if (off == 0)
		return PVR_PROC_SEQ_START_TOKEN;

	for (pBT = pArena->pHeadSegment; --off && pBT;
	     pBT = pBT->pNextSegment) ;

	return (void *)pBT;
}

#else
static int
RA_DumpSegs(char *page, char **start, off_t off, int count, int *eof,
	    void *data)
{
	BT *pBT = 0;
	int len = 0;
	RA_ARENA *pArena = (RA_ARENA *) data;

	if (count < 80) {
		*start = (char *)0;
		return (0);
	}
	*eof = 0;
	*start = (char *)1;
	if (off == 0) {
		return printAppend(page, count, 0,
				   "Arena \"%s\"\nBase         Size Type Ref\n",
				   pArena->name);
	}
	for (pBT = pArena->pHeadSegment; --off && pBT;
	     pBT = pBT->pNextSegment) ;
	if (pBT) {
		len = printAppend(page, count, 0, "%08x %8x %4s %08x\n",
				  (u32) pBT->base, (u32) pBT->uSize,
				  _BTType(pBT->type), (u32) pBT->psMapping);
	} else {
		*eof = 1;
	}
	return (len);
}

static int
RA_DumpInfo(char *page, char **start, off_t off, int count, int *eof,
	    void *data)
{
	int len = 0;
	RA_ARENA *pArena = (RA_ARENA *) data;

	if (count < 80) {
		*start = (char *)0;
		return (0);
	}
	*eof = 0;
	switch (off) {
	case 0:
		len =
		    printAppend(page, count, 0, "quantum\t\t\t%lu\n",
				pArena->uQuantum);
		break;
	case 1:
		len =
		    printAppend(page, count, 0, "import_handle\t\t%08X\n",
				(u32) pArena->pImportHandle);
		break;
#ifdef RA_STATS
	case 2:
		len =
		    printAppend(page, count, 0, "span count\t\t%lu\n",
				pArena->sStatistics.uSpanCount);
		break;
	case 3:
		len =
		    printAppend(page, count, 0, "live segment count\t%lu\n",
				pArena->sStatistics.uLiveSegmentCount);
		break;
	case 4:
		len =
		    printAppend(page, count, 0, "free segment count\t%lu\n",
				pArena->sStatistics.uFreeSegmentCount);
		break;
	case 5:
		len =
		    printAppend(page, count, 0,
				"free resource count\t%lu (0x%x)\n",
				pArena->sStatistics.uFreeResourceCount,
				(u32) pArena->sStatistics.uFreeResourceCount);
		break;
	case 6:
		len =
		    printAppend(page, count, 0, "total allocs\t\t%lu\n",
				pArena->sStatistics.uCumulativeAllocs);
		break;
	case 7:
		len =
		    printAppend(page, count, 0, "total frees\t\t%lu\n",
				pArena->sStatistics.uCumulativeFrees);
		break;
	case 8:
		len =
		    printAppend(page, count, 0, "import count\t\t%lu\n",
				pArena->sStatistics.uImportCount);
		break;
	case 9:
		len =
		    printAppend(page, count, 0, "export count\t\t%lu\n",
				pArena->sStatistics.uExportCount);
		break;
#endif

	default:
		*eof = 1;
	}
	*start = (char *)1;
	return (len);
}
#endif
#endif

#ifdef RA_STATS
PVRSRV_ERROR RA_GetStats(RA_ARENA * pArena, char **ppszStr, u32 * pui32StrLen)
{
	char *pszStr = *ppszStr;
	u32 ui32StrLen = *pui32StrLen;
	s32 i32Count;
	BT *pBT;

	CHECK_SPACE(ui32StrLen);
	i32Count = snprintf(pszStr, 100, "\nArena '%s':\n", pArena->name);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100,
		     "  allocCB=%p freeCB=%p handle=%p quantum=%d\n",
		     pArena->pImportAlloc, pArena->pImportFree,
		     pArena->pImportHandle, pArena->uQuantum);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "span count\t\t%u\n",
		     pArena->sStatistics.uSpanCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "live segment count\t%u\n",
		     pArena->sStatistics.uLiveSegmentCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "free segment count\t%u\n",
		     pArena->sStatistics.uFreeSegmentCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count = snprintf(pszStr, 100, "free resource count\t%u (0x%x)\n",
			    pArena->sStatistics.uFreeResourceCount,
			    (u32) pArena->sStatistics.uFreeResourceCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "total allocs\t\t%u\n",
		     pArena->sStatistics.uCumulativeAllocs);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "total frees\t\t%u\n",
		     pArena->sStatistics.uCumulativeFrees);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "import count\t\t%u\n",
		     pArena->sStatistics.uImportCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count =
	    snprintf(pszStr, 100, "export count\t\t%u\n",
		     pArena->sStatistics.uExportCount);
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	CHECK_SPACE(ui32StrLen);
	i32Count = snprintf(pszStr, 100, "  segment Chain:\n");
	UPDATE_SPACE(pszStr, i32Count, ui32StrLen);

	if (pArena->pHeadSegment != NULL &&
	    pArena->pHeadSegment->pPrevSegment != NULL) {
		CHECK_SPACE(ui32StrLen);
		i32Count =
		    snprintf(pszStr, 100,
			     "  error: head boundary tag has invalid pPrevSegment\n");
		UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
	}

	if (pArena->pTailSegment != NULL &&
	    pArena->pTailSegment->pNextSegment != NULL) {
		CHECK_SPACE(ui32StrLen);
		i32Count =
		    snprintf(pszStr, 100,
			     "  error: tail boundary tag has invalid pNextSegment\n");
		UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
	}

	for (pBT = pArena->pHeadSegment; pBT != NULL; pBT = pBT->pNextSegment) {
		CHECK_SPACE(ui32StrLen);
		i32Count =
		    snprintf(pszStr, 100,
			     "\tbase=0x%x size=0x%x type=%s ref=%p\n",
			     (u32) pBT->base, pBT->uSize, _BTType(pBT->type),
			     pBT->psMapping);
		UPDATE_SPACE(pszStr, i32Count, ui32StrLen);
	}

	*ppszStr = pszStr;
	*pui32StrLen = ui32StrLen;

	return PVRSRV_OK;
}
#endif

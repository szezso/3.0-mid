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

#include "pvr_debug.h"

#include "services.h"
#include "servicesint.h"
#include "hash.h"
#include "osfunc.h"

#define PRIVATE_MAX(a,b) ((a)>(b)?(a):(b))

#define	KEY_TO_INDEX(pHash, key, uSize) \
	((pHash)->pfnHashFunc((pHash)->uKeySize, key, uSize) % uSize)

#define	KEY_COMPARE(pHash, pKey1, pKey2) \
	((pHash)->pfnKeyComp((pHash)->uKeySize, pKey1, pKey2))

struct _BUCKET_ {

	struct _BUCKET_ *pNext;

	u32 v;

	u32 k[];
};
typedef struct _BUCKET_ BUCKET;

struct _HASH_TABLE_ {

	BUCKET **ppBucketTable;

	u32 uSize;

	u32 uCount;

	u32 uMinimumSize;

	u32 uKeySize;

	HASH_FUNC *pfnHashFunc;

	HASH_KEY_COMP *pfnKeyComp;
};

u32 HASH_Func_Default(u32 uKeySize, void *pKey, u32 uHashTabLen)
{
	u32 *p = (u32 *) pKey;
	u32 uKeyLen = uKeySize / sizeof(u32);
	u32 ui;
	u32 uHashKey = 0;

	PVR_ASSERT((uKeySize % sizeof(u32)) == 0);

	for (ui = 0; ui < uKeyLen; ui++) {
		u32 uHashPart = (u32) * p++;

		uHashPart += (uHashPart << 12);
		uHashPart ^= (uHashPart >> 22);
		uHashPart += (uHashPart << 4);
		uHashPart ^= (uHashPart >> 9);
		uHashPart += (uHashPart << 10);
		uHashPart ^= (uHashPart >> 2);
		uHashPart += (uHashPart << 7);
		uHashPart ^= (uHashPart >> 12);

		uHashKey += uHashPart;
	}

	return uHashKey;
}

int HASH_Key_Comp_Default(u32 uKeySize, void *pKey1, void *pKey2)
{
	u32 *p1 = (u32 *) pKey1;
	u32 *p2 = (u32 *) pKey2;
	u32 uKeyLen = uKeySize / sizeof(u32);
	u32 ui;

	PVR_ASSERT((uKeySize % sizeof(u32)) == 0);

	for (ui = 0; ui < uKeyLen; ui++) {
		if (*p1++ != *p2++)
			return 0;
	}

	return 1;
}

static PVRSRV_ERROR
_ChainInsert(HASH_TABLE * pHash, BUCKET * pBucket, BUCKET ** ppBucketTable,
	     u32 uSize)
{
	u32 uIndex;

	PVR_ASSERT(pBucket != NULL);
	PVR_ASSERT(ppBucketTable != NULL);
	PVR_ASSERT(uSize != 0);

	if ((pBucket == NULL) || (ppBucketTable == NULL) || (uSize == 0)) {
		PVR_DPF((PVR_DBG_ERROR, "_ChainInsert: invalid parameter"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	uIndex = KEY_TO_INDEX(pHash, pBucket->k, uSize);
	pBucket->pNext = ppBucketTable[uIndex];
	ppBucketTable[uIndex] = pBucket;

	return PVRSRV_OK;
}

static PVRSRV_ERROR
_Rehash(HASH_TABLE * pHash,
	BUCKET ** ppOldTable, u32 uOldSize, BUCKET ** ppNewTable, u32 uNewSize)
{
	u32 uIndex;
	for (uIndex = 0; uIndex < uOldSize; uIndex++) {
		BUCKET *pBucket;
		pBucket = ppOldTable[uIndex];
		while (pBucket != NULL) {
			BUCKET *pNextBucket = pBucket->pNext;
			if (_ChainInsert(pHash, pBucket, ppNewTable, uNewSize)
			    != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "_Rehash: call to _ChainInsert failed"));
				return PVRSRV_ERROR_GENERIC;
			}
			pBucket = pNextBucket;
		}
	}
	return PVRSRV_OK;
}

static int _Resize(HASH_TABLE * pHash, u32 uNewSize)
{
	if (uNewSize != pHash->uSize) {
		BUCKET **ppNewTable;
		u32 uIndex;

		PVR_DPF((PVR_DBG_MESSAGE,
			 "HASH_Resize: oldsize=0x%x  newsize=0x%x  count=0x%x",
			 pHash->uSize, uNewSize, pHash->uCount));

		OSAllocMem(PVRSRV_PAGEABLE_SELECT,
			   sizeof(BUCKET *) * uNewSize,
			   (void **)&ppNewTable, NULL, "Hash Table Buckets");
		if (ppNewTable == NULL)
			return 0;

		for (uIndex = 0; uIndex < uNewSize; uIndex++)
			ppNewTable[uIndex] = NULL;

		if (_Rehash
		    (pHash, pHash->ppBucketTable, pHash->uSize, ppNewTable,
		     uNewSize) != PVRSRV_OK) {
			return 0;
		}

		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(BUCKET *) * pHash->uSize, pHash->ppBucketTable,
			  NULL);

		pHash->ppBucketTable = ppNewTable;
		pHash->uSize = uNewSize;
	}
	return 1;
}

HASH_TABLE *HASH_Create_Extended(u32 uInitialLen, u32 uKeySize,
				 HASH_FUNC * pfnHashFunc,
				 HASH_KEY_COMP * pfnKeyComp)
{
	HASH_TABLE *pHash;
	u32 uIndex;

	PVR_DPF((PVR_DBG_MESSAGE, "HASH_Create_Extended: InitialSize=0x%x",
		 uInitialLen));

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(HASH_TABLE),
		       (void **)&pHash, NULL, "Hash Table") != PVRSRV_OK) {
		return NULL;
	}

	pHash->uCount = 0;
	pHash->uSize = uInitialLen;
	pHash->uMinimumSize = uInitialLen;
	pHash->uKeySize = uKeySize;
	pHash->pfnHashFunc = pfnHashFunc;
	pHash->pfnKeyComp = pfnKeyComp;

	OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		   sizeof(BUCKET *) * pHash->uSize,
		   (void **)&pHash->ppBucketTable, NULL, "Hash Table Buckets");

	if (pHash->ppBucketTable == NULL) {
		OSFreeMem(PVRSRV_PAGEABLE_SELECT, sizeof(HASH_TABLE), pHash,
			  NULL);

		return NULL;
	}

	for (uIndex = 0; uIndex < pHash->uSize; uIndex++)
		pHash->ppBucketTable[uIndex] = NULL;
	return pHash;
}

HASH_TABLE *HASH_Create(u32 uInitialLen)
{
	return HASH_Create_Extended(uInitialLen, sizeof(u32),
				    &HASH_Func_Default, &HASH_Key_Comp_Default);
}

void HASH_Delete(HASH_TABLE * pHash)
{
	if (pHash != NULL) {
		PVR_DPF((PVR_DBG_MESSAGE, "HASH_Delete"));

		PVR_ASSERT(pHash->uCount == 0);
		if (pHash->uCount != 0) {
			PVR_DPF((PVR_DBG_ERROR,
				 "HASH_Delete: leak detected in hash table!"));
			PVR_DPF((PVR_DBG_ERROR,
				 "Likely Cause: client drivers not freeing alocations before destroying devmemcontext"));
		}
		OSFreeMem(PVRSRV_PAGEABLE_SELECT,
			  sizeof(BUCKET *) * pHash->uSize, pHash->ppBucketTable,
			  NULL);
		pHash->ppBucketTable = NULL;
		OSFreeMem(PVRSRV_PAGEABLE_SELECT, sizeof(HASH_TABLE), pHash,
			  NULL);

	}
}

int HASH_Insert_Extended(HASH_TABLE * pHash, void *pKey, u32 v)
{
	BUCKET *pBucket;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "HASH_Insert_Extended: Hash=%08X, pKey=%08X, v=0x%x", pHash,
		 pKey, v));

	PVR_ASSERT(pHash != NULL);

	if (pHash == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Insert_Extended: invalid parameter"));
		return 0;
	}

	if (OSAllocMem(PVRSRV_PAGEABLE_SELECT,
		       sizeof(BUCKET) + pHash->uKeySize,
		       (void **)&pBucket, NULL,
		       "Hash Table entry") != PVRSRV_OK) {
		return 0;
	}

	pBucket->v = v;

	memcpy(pBucket->k, pKey, pHash->uKeySize);
	if (_ChainInsert(pHash, pBucket, pHash->ppBucketTable, pHash->uSize) !=
	    PVRSRV_OK) {
		return 0;
	}

	pHash->uCount++;

	if (pHash->uCount << 1 > pHash->uSize) {

		_Resize(pHash, pHash->uSize << 1);
	}

	return 1;
}

int HASH_Insert(HASH_TABLE * pHash, u32 k, u32 v)
{
	PVR_DPF((PVR_DBG_MESSAGE,
		 "HASH_Insert: Hash=%08X, k=0x%x, v=0x%x", pHash, k, v));

	return HASH_Insert_Extended(pHash, &k, v);
}

u32 HASH_Remove_Extended(HASH_TABLE * pHash, void *pKey)
{
	BUCKET **ppBucket;
	u32 uIndex;

	PVR_DPF((PVR_DBG_MESSAGE, "HASH_Remove_Extended: Hash=%08X, pKey=%08X",
		 pHash, pKey));

	PVR_ASSERT(pHash != NULL);

	if (pHash == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Remove_Extended: Null hash table"));
		return 0;
	}

	uIndex = KEY_TO_INDEX(pHash, pKey, pHash->uSize);

	for (ppBucket = &(pHash->ppBucketTable[uIndex]); *ppBucket != NULL;
	     ppBucket = &((*ppBucket)->pNext)) {

		if (KEY_COMPARE(pHash, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			u32 v = pBucket->v;
			(*ppBucket) = pBucket->pNext;

			OSFreeMem(PVRSRV_PAGEABLE_SELECT,
				  sizeof(BUCKET) + pHash->uKeySize, pBucket,
				  NULL);

			pHash->uCount--;

			if (pHash->uSize > (pHash->uCount << 2) &&
			    pHash->uSize > pHash->uMinimumSize) {

				_Resize(pHash,
					PRIVATE_MAX(pHash->uSize >> 1,
						    pHash->uMinimumSize));
			}

			PVR_DPF((PVR_DBG_MESSAGE,
				 "HASH_Remove_Extended: Hash=%08X, pKey=%08X = 0x%x",
				 pHash, pKey, v));
			return v;
		}
	}
	PVR_DPF((PVR_DBG_MESSAGE,
		 "HASH_Remove_Extended: Hash=%08X, pKey=%08X = 0x0 !!!!", pHash,
		 pKey));
	return 0;
}

u32 HASH_Remove(HASH_TABLE * pHash, u32 k)
{
	PVR_DPF((PVR_DBG_MESSAGE, "HASH_Remove: Hash=%08X, k=0x%x", pHash, k));

	return HASH_Remove_Extended(pHash, &k);
}

u32 HASH_Retrieve_Extended(HASH_TABLE * pHash, void *pKey)
{
	BUCKET **ppBucket;
	u32 uIndex;

	PVR_DPF((PVR_DBG_MESSAGE,
		 "HASH_Retrieve_Extended: Hash=%08X, pKey=%08X", pHash, pKey));

	PVR_ASSERT(pHash != NULL);

	if (pHash == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Retrieve_Extended: Null hash table"));
		return 0;
	}

	uIndex = KEY_TO_INDEX(pHash, pKey, pHash->uSize);

	for (ppBucket = &(pHash->ppBucketTable[uIndex]); *ppBucket != NULL;
	     ppBucket = &((*ppBucket)->pNext)) {

		if (KEY_COMPARE(pHash, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			u32 v = pBucket->v;

			PVR_DPF((PVR_DBG_MESSAGE,
				 "HASH_Retrieve: Hash=%08X, pKey=%08X = 0x%x",
				 pHash, pKey, v));
			return v;
		}
	}
	PVR_DPF((PVR_DBG_MESSAGE,
		 "HASH_Retrieve: Hash=%08X, pKey=%08X = 0x0 !!!!", pHash,
		 pKey));
	return 0;
}

u32 HASH_Retrieve(HASH_TABLE * pHash, u32 k)
{
	PVR_DPF((PVR_DBG_MESSAGE, "HASH_Retrieve: Hash=%08X, k=0x%x", pHash,
		 k));
	return HASH_Retrieve_Extended(pHash, &k);
}

#ifdef HASH_TRACE
void HASH_Dump(HASH_TABLE * pHash)
{
	u32 uIndex;
	u32 uMaxLength = 0;
	u32 uEmptyCount = 0;

	PVR_ASSERT(pHash != NULL);
	for (uIndex = 0; uIndex < pHash->uSize; uIndex++) {
		BUCKET *pBucket;
		u32 uLength = 0;
		if (pHash->ppBucketTable[uIndex] == NULL)
			uEmptyCount++;
		for (pBucket = pHash->ppBucketTable[uIndex];
		     pBucket != NULL; pBucket = pBucket->pNext)
			uLength++;
		uMaxLength = PRIVATE_MAX(uMaxLength, uLength);
	}

	PVR_TRACE(("hash table: uMinimumSize=%d  size=%d  count=%d",
		   pHash->uMinimumSize, pHash->uSize, pHash->uCount));
	PVR_TRACE(("  empty=%d  max=%d", uEmptyCount, uMaxLength));
}
#endif

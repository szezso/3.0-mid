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
#include "resman.h"

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>

static DEFINE_MUTEX(lock);

#define RESMAN_SIGNATURE 0x12345678

typedef struct _RESMAN_ITEM_ {
#ifdef DEBUG
	u32 ui32Signature;
#endif
	struct _RESMAN_ITEM_ **ppsThis;
	struct _RESMAN_ITEM_ *psNext;

	u32 ui32Flags;
	u32 ui32ResType;

	void *pvParam;
	u32 ui32Param;

	RESMAN_FREE_FN pfnFreeResource;
} RESMAN_ITEM;

typedef struct _RESMAN_CONTEXT_ {
#ifdef DEBUG
	u32 ui32Signature;
#endif
	struct _RESMAN_CONTEXT_ **ppsThis;
	struct _RESMAN_CONTEXT_ *psNext;

	PVRSRV_PER_PROCESS_DATA *psPerProc;

	RESMAN_ITEM *psResItemList;

} RESMAN_CONTEXT;

typedef struct {
	RESMAN_CONTEXT *psContextList;

} RESMAN_LIST, *PRESMAN_LIST;

PRESMAN_LIST gpsResList = NULL;

#include "lists.h"

static IMPLEMENT_LIST_ANY_VA(RESMAN_ITEM)
static IMPLEMENT_LIST_ANY_VA_2(RESMAN_ITEM, int, 0)
static IMPLEMENT_LIST_INSERT(RESMAN_ITEM)
static IMPLEMENT_LIST_REMOVE(RESMAN_ITEM)

static IMPLEMENT_LIST_REMOVE(RESMAN_CONTEXT)
static IMPLEMENT_LIST_INSERT(RESMAN_CONTEXT)

#define PRINT_RESLIST(x, y, z)
 static PVRSRV_ERROR FreeResourceByPtr(RESMAN_ITEM * psItem,
				       int bExecuteCallback);

static PVRSRV_ERROR FreeResourceByCriteria(PRESMAN_CONTEXT psContext,
					   u32 ui32SearchCriteria,
					   u32 ui32ResType,
					   void *pvParam,
					   u32 ui32Param, int bExecuteCallback);

#ifdef DEBUG
static void ValidateResList(PRESMAN_LIST psResList);
#define VALIDATERESLIST() ValidateResList(gpsResList)
#else
#define VALIDATERESLIST()
#endif

PVRSRV_ERROR ResManInit(void)
{
	if (gpsResList == NULL) {

		if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			       sizeof(*gpsResList),
			       (void **)&gpsResList, NULL,
			       "Resource Manager List") != PVRSRV_OK) {
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		gpsResList->psContextList = NULL;

		VALIDATERESLIST();
	}

	return PVRSRV_OK;
}

void ResManDeInit(void)
{
	if (gpsResList != NULL) {

		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(*gpsResList),
			  gpsResList, NULL);
		gpsResList = NULL;
	}
}

PVRSRV_ERROR PVRSRVResManConnect(void *hPerProc,
				 PRESMAN_CONTEXT * phResManContext)
{
	PVRSRV_ERROR eError;
	PRESMAN_CONTEXT psResManContext;


	mutex_lock(&lock);

	VALIDATERESLIST();

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(*psResManContext),
			    (void **)&psResManContext, NULL,
			    "Resource Manager Context");
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVResManConnect: ERROR allocating new RESMAN context struct"));

		VALIDATERESLIST();

		mutex_unlock(&lock);

		return eError;
	}
#ifdef DEBUG
	psResManContext->ui32Signature = RESMAN_SIGNATURE;
#endif
	psResManContext->psResItemList = NULL;
	psResManContext->psPerProc = hPerProc;

	List_RESMAN_CONTEXT_Insert(&gpsResList->psContextList, psResManContext);

	VALIDATERESLIST();

	mutex_unlock(&lock);

	*phResManContext = psResManContext;

	return PVRSRV_OK;
}

void PVRSRVResManDisconnect(PRESMAN_CONTEXT psResManContext, int bKernelContext)
{

        mutex_lock(&lock);
        
	VALIDATERESLIST();

	PRINT_RESLIST(gpsResList, psResManContext, 1);

	if (!bKernelContext) {

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_OS_USERMODE_MAPPING, 0, 0,
				       1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_EVENT_OBJECT, 0, 0, 1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_MODIFY_SYNC_OPS, 0, 0, 1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_HW_RENDER_CONTEXT, 0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_HW_TRANSFER_CONTEXT, 0, 0,
				       1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_HW_2D_CONTEXT, 0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_TRANSFER_CONTEXT, 0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_SHARED_PB_DESC_CREATE_LOCK,
				       0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_SHARED_PB_DESC, 0, 0, 1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DISPLAYCLASS_SWAPCHAIN_REF,
				       0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DISPLAYCLASS_DEVICE, 0, 0,
				       1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_BUFFERCLASS_DEVICE, 0, 0, 1);

		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DEVICECLASSMEM_MAPPING, 0, 0,
				       1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DEVICEMEM_WRAP, 0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DEVICEMEM_MAPPING, 0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_KERNEL_DEVICEMEM_ALLOCATION,
				       0, 0, 1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DEVICEMEM_ALLOCATION, 0, 0,
				       1);
		FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE,
				       RESMAN_TYPE_DEVICEMEM_CONTEXT, 0, 0, 1);
	}

	PVR_ASSERT(psResManContext->psResItemList == NULL);

	List_RESMAN_CONTEXT_Remove(psResManContext);

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, sizeof(RESMAN_CONTEXT),
		  psResManContext, NULL);

	VALIDATERESLIST();

	PRINT_RESLIST(gpsResList, psResManContext, 0);

	mutex_unlock(&lock);
}

PRESMAN_ITEM ResManRegisterRes(PRESMAN_CONTEXT psResManContext,
			       u32 ui32ResType,
			       void *pvParam,
			       u32 ui32Param, RESMAN_FREE_FN pfnFreeResource)
{
	PRESMAN_ITEM psNewResItem;

	PVR_ASSERT(psResManContext != NULL);
	PVR_ASSERT(ui32ResType != 0);

	if (psResManContext == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "ResManRegisterRes: invalid parameter - psResManContext"));
		return (PRESMAN_ITEM) NULL;
	}

        mutex_lock(&lock);

	VALIDATERESLIST();

	PVR_DPF((PVR_DBG_MESSAGE, "ResManRegisterRes: register resource "
		 "Context 0x%x, ResType 0x%x, pvParam 0x%x, ui32Param 0x%x, "
		 "FreeFunc %08X",
		 psResManContext, ui32ResType, (u32) pvParam,
		 ui32Param, pfnFreeResource));

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(RESMAN_ITEM), (void **)&psNewResItem,
		       NULL, "Resource Manager Item") != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "ResManRegisterRes: "
			 "ERROR allocating new resource item"));

        	mutex_unlock(&lock);

		return ((PRESMAN_ITEM) NULL);
	}

#ifdef DEBUG
	psNewResItem->ui32Signature = RESMAN_SIGNATURE;
#endif
	psNewResItem->ui32ResType = ui32ResType;
	psNewResItem->pvParam = pvParam;
	psNewResItem->ui32Param = ui32Param;
	psNewResItem->pfnFreeResource = pfnFreeResource;
	psNewResItem->ui32Flags = 0;

	List_RESMAN_ITEM_Insert(&psResManContext->psResItemList, psNewResItem);

	VALIDATERESLIST();

	mutex_unlock(&lock);

	return (psNewResItem);
}

PVRSRV_ERROR ResManFreeResByPtr(RESMAN_ITEM * psResItem)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(psResItem != NULL);

	if (psResItem == NULL) {
		PVR_DPF((PVR_DBG_MESSAGE,
			 "ResManFreeResByPtr: NULL ptr - nothing to do"));
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
		 "ResManFreeResByPtr: freeing resource at %08X", psResItem));

        mutex_lock(&lock);

	VALIDATERESLIST();

	eError = FreeResourceByPtr(psResItem, 1);

	VALIDATERESLIST();

	mutex_unlock(&lock);

	return (eError);
}

PVRSRV_ERROR ResManFreeResByCriteria(PRESMAN_CONTEXT psResManContext,
				     u32 ui32SearchCriteria,
				     u32 ui32ResType,
				     void *pvParam, u32 ui32Param)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(psResManContext != NULL);

        mutex_lock(&lock);

	VALIDATERESLIST();

	PVR_DPF((PVR_DBG_MESSAGE, "ResManFreeResByCriteria: "
		 "Context 0x%x, Criteria 0x%x, Type 0x%x, Addr 0x%x, Param 0x%x",
		 psResManContext, ui32SearchCriteria, ui32ResType,
		 (u32) pvParam, ui32Param));

	eError = FreeResourceByCriteria(psResManContext, ui32SearchCriteria,
					ui32ResType, pvParam, ui32Param, 1);

	VALIDATERESLIST();

	mutex_unlock(&lock);

	return eError;
}

PVRSRV_ERROR ResManDissociateRes(RESMAN_ITEM * psResItem,
				 PRESMAN_CONTEXT psNewResManContext)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_ASSERT(psResItem != NULL);

	if (psResItem == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "ResManDissociateRes: invalid parameter - psResItem"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#ifdef DEBUG
	PVR_ASSERT(psResItem->ui32Signature == RESMAN_SIGNATURE);
#endif

	if (psNewResManContext != NULL) {

		List_RESMAN_ITEM_Remove(psResItem);

		List_RESMAN_ITEM_Insert(&psNewResManContext->psResItemList,
					psResItem);

	} else {
		eError = FreeResourceByPtr(psResItem, 0);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "ResManDissociateRes: failed to free resource by pointer"));
			return eError;
		}
	}

	return eError;
}

int ResManFindResourceByPtr_AnyVaCb(RESMAN_ITEM * psCurItem, va_list va)
{
	RESMAN_ITEM *psItem;

	psItem = va_arg(va, RESMAN_ITEM *);

	return (int)(psCurItem == psItem);
}

/* FIXME MLD IMG_INTERNAL PVRSRV_ERROR ResManFindResourceByPtr(PRESMAN_CONTEXT*/
PVRSRV_ERROR ResManFindResourceByPtr(PRESMAN_CONTEXT
						  psResManContext,
						  RESMAN_ITEM * psItem)
{
	PVRSRV_ERROR eResult;

	PVR_ASSERT(psResManContext != NULL);
	PVR_ASSERT(psItem != NULL);

	if ((psItem == NULL) || (psResManContext == NULL)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "ResManFindResourceByPtr: invalid parameter"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#ifdef DEBUG
	PVR_ASSERT(psItem->ui32Signature == RESMAN_SIGNATURE);
#endif


	PVR_DPF((PVR_DBG_MESSAGE,
		 "FindResourceByPtr: psItem=%08X, psItem->psNext=%08X",
		 psItem, psItem->psNext));

	PVR_DPF((PVR_DBG_MESSAGE,
		 "FindResourceByPtr: Resource Ctx 0x%x, Type 0x%x, Addr 0x%x, "
		 "Param 0x%x, FnCall %08X, Flags 0x%x",
		 psResManContext,
		 psItem->ui32ResType, (u32) psItem->pvParam, psItem->ui32Param,
		 psItem->pfnFreeResource, psItem->ui32Flags));

	if (List_RESMAN_ITEM_int_Any_va(psResManContext->psResItemList,
					ResManFindResourceByPtr_AnyVaCb,
					psItem)) {
		eResult = PVRSRV_OK;
	} else {
		eResult = PVRSRV_ERROR_NOT_OWNER;
	}

	mutex_unlock(&lock);

	return eResult;
}

static PVRSRV_ERROR FreeResourceByPtr(RESMAN_ITEM * psItem,
				      int bExecuteCallback)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_ASSERT(psItem != NULL);

	if (psItem == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeResourceByPtr: invalid parameter"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#ifdef DEBUG
	PVR_ASSERT(psItem->ui32Signature == RESMAN_SIGNATURE);
#endif

	PVR_DPF((PVR_DBG_MESSAGE,
		 "FreeResourceByPtr: psItem=%08X, psItem->psNext=%08X",
		 psItem, psItem->psNext));

	PVR_DPF((PVR_DBG_MESSAGE,
		 "FreeResourceByPtr: Type 0x%x, Addr 0x%x, "
		 "Param 0x%x, FnCall %08X, Flags 0x%x",
		 psItem->ui32ResType, (u32) psItem->pvParam, psItem->ui32Param,
		 psItem->pfnFreeResource, psItem->ui32Flags));

	List_RESMAN_ITEM_Remove(psItem);

	mutex_unlock(&lock);

	if (bExecuteCallback) {
		eError =
		    psItem->pfnFreeResource(psItem->pvParam, psItem->ui32Param);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR,
				 "FreeResourceByPtr: ERROR calling FreeResource function"));
		}
	}

        mutex_lock(&lock);

	if (OSFreeMem
	    (PVRSRV_OS_PAGEABLE_HEAP, sizeof(RESMAN_ITEM), psItem,
	     NULL) != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR,
			 "FreeResourceByPtr: ERROR freeing resource list item memory"));
		eError = PVRSRV_ERROR_GENERIC;
	}

	return (eError);
}

void *FreeResourceByCriteria_AnyVaCb(RESMAN_ITEM * psCurItem, va_list va)
{
	u32 ui32SearchCriteria;
	u32 ui32ResType;
	void *pvParam;
	u32 ui32Param;

	ui32SearchCriteria = va_arg(va, u32);
	ui32ResType = va_arg(va, u32);
	pvParam = va_arg(va, void *);
	ui32Param = va_arg(va, u32);

	if ((((ui32SearchCriteria & RESMAN_CRITERIA_RESTYPE) == 0UL) ||
	     (psCurItem->ui32ResType == ui32ResType))
	    &&
	    (((ui32SearchCriteria & RESMAN_CRITERIA_PVOID_PARAM) == 0UL) ||
	     (psCurItem->pvParam == pvParam))
	    &&
	    (((ui32SearchCriteria & RESMAN_CRITERIA_UI32_PARAM) == 0UL) ||
	     (psCurItem->ui32Param == ui32Param))
	    ) {
		return psCurItem;
	} else {
		return NULL;
	}
}

static PVRSRV_ERROR FreeResourceByCriteria(PRESMAN_CONTEXT psResManContext,
					   u32 ui32SearchCriteria,
					   u32 ui32ResType,
					   void *pvParam,
					   u32 ui32Param, int bExecuteCallback)
{
	PRESMAN_ITEM psCurItem;
	PVRSRV_ERROR eError = PVRSRV_OK;

	while ((psCurItem = (PRESMAN_ITEM)
		List_RESMAN_ITEM_Any_va(psResManContext->psResItemList,
					FreeResourceByCriteria_AnyVaCb,
					ui32SearchCriteria,
					ui32ResType,
					pvParam,
					ui32Param)) != NULL
	       && eError == PVRSRV_OK) {
		eError = FreeResourceByPtr(psCurItem, bExecuteCallback);
	}

	return eError;
}

#ifdef DEBUG
static void ValidateResList(PRESMAN_LIST psResList)
{
	PRESMAN_ITEM psCurItem, *ppsThisItem;
	PRESMAN_CONTEXT psCurContext, *ppsThisContext;

	if (psResList == NULL) {
		PVR_DPF((PVR_DBG_MESSAGE,
			 "ValidateResList: resman not initialised yet"));
		return;
	}

	psCurContext = psResList->psContextList;
	ppsThisContext = &psResList->psContextList;

	while (psCurContext != NULL) {

		PVR_ASSERT(psCurContext->ui32Signature == RESMAN_SIGNATURE);
		if (psCurContext->ppsThis != ppsThisContext) {
			PVR_DPF((PVR_DBG_WARNING,
				 "psCC=%08X psCC->ppsThis=%08X psCC->psNext=%08X ppsTC=%08X",
				 psCurContext, psCurContext->ppsThis,
				 psCurContext->psNext, ppsThisContext));
			PVR_ASSERT(psCurContext->ppsThis == ppsThisContext);
		}

		psCurItem = psCurContext->psResItemList;
		ppsThisItem = &psCurContext->psResItemList;
		while (psCurItem != NULL) {

			PVR_ASSERT(psCurItem->ui32Signature ==
				   RESMAN_SIGNATURE);
			if (psCurItem->ppsThis != ppsThisItem) {
				PVR_DPF((PVR_DBG_WARNING,
					 "psCurItem=%08X psCurItem->ppsThis=%08X psCurItem->psNext=%08X ppsThisItem=%08X",
					 psCurItem, psCurItem->ppsThis,
					 psCurItem->psNext, ppsThisItem));
				PVR_ASSERT(psCurItem->ppsThis == ppsThisItem);
			}

			ppsThisItem = &psCurItem->psNext;
			psCurItem = psCurItem->psNext;
		}

		ppsThisContext = &psCurContext->psNext;
		psCurContext = psCurContext->psNext;
	}
}
#endif

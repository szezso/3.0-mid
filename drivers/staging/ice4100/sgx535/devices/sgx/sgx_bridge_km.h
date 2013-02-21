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

#if !defined(__SGX_BRIDGE_KM_H__)
#define __SGX_BRIDGE_KM_H__

#include "sgxapi_km.h"
#include "sgxinfo.h"
#include "sgxinfokm.h"
#include "sgx_bridge.h"
#include "pvr_bridge.h"
#include "perproc.h"

#if defined (__cplusplus)
extern "C" {
#endif


PVRSRV_ERROR SGXSubmitTransferKM(void * hDevHandle, PVRSRV_TRANSFER_SGX_KICK *psKick);

#if defined(SGX_FEATURE_2D_HARDWARE)

PVRSRV_ERROR SGXSubmit2DKM(void * hDevHandle, PVRSRV_2D_SGX_KICK *psKick);
#endif


PVRSRV_ERROR SGXDoKickKM(void * hDevHandle,
						 SGX_CCB_KICK *psCCBKick);


PVRSRV_ERROR SGXGetPhysPageAddrKM(void * hDevMemHeap,
								  IMG_DEV_VIRTADDR sDevVAddr,
								  IMG_DEV_PHYADDR *pDevPAddr,
								  IMG_CPU_PHYADDR *pCpuPAddr);


PVRSRV_ERROR  SGXGetMMUPDAddrKM(void *		hDevCookie,
											void *		hDevMemContext,
											IMG_DEV_PHYADDR	*psPDDevPAddr);


PVRSRV_ERROR SGXGetClientInfoKM(void *				hDevCookie,
								SGX_CLIENT_INFO*	psClientInfo);


PVRSRV_ERROR SGXGetMiscInfoKM(PVRSRV_SGXDEV_INFO	*psDevInfo,
							  SGX_MISC_INFO			*psMiscInfo,
							  PVRSRV_DEVICE_NODE 	*psDeviceNode,
							  void * 			 hDevMemContext);

#if defined(SUPPORT_SGX_HWPERF)

PVRSRV_ERROR SGXReadDiffCountersKM(void *				hDevHandle,
								   u32				ui32Reg,
								   u32				*pui32Old,
								   int					bNew,
								   u32				ui32New,
								   u32				ui32NewReset,
								   u32				ui32CountersReg,
								   u32				ui32Reg2,
								   int					*pbActive,
								   PVRSRV_SGXDEV_DIFF_INFO	*psDiffs);

PVRSRV_ERROR SGXReadHWPerfCBKM(void *					hDevHandle,
							   u32					ui32ArraySize,
							   PVRSRV_SGX_HWPERF_CB_ENTRY	*psHWPerfCBData,
							   u32					*pui32DataCount,
							   u32					*pui32ClockSpeed,
							   u32					*pui32HostTimeStamp);
#endif


PVRSRV_ERROR SGX2DQueryBlitsCompleteKM(PVRSRV_SGXDEV_INFO		*psDevInfo,
									   PVRSRV_KERNEL_SYNC_INFO	*psSyncInfo,
									   int bWaitForComplete);


PVRSRV_ERROR SGXGetInfoForSrvinitKM(void * hDevHandle,
									SGX_BRIDGE_INFO_FOR_SRVINIT *psInitInfo);


PVRSRV_ERROR DevInitSGXPart2KM(PVRSRV_PER_PROCESS_DATA *psPerProc,
							   void * hDevHandle,
							   SGX_BRIDGE_INIT_INFO *psInitInfo);

 PVRSRV_ERROR
SGXFindSharedPBDescKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
					  void *				hDevCookie,
					  int				bLockOnFailure,
					  u32				ui32TotalPBSize,
					  void *				*phSharedPBDesc,
					  PVRSRV_KERNEL_MEM_INFO	**ppsSharedPBDescKernelMemInfo,
					  PVRSRV_KERNEL_MEM_INFO	**ppsHWPBDescKernelMemInfo,
					  PVRSRV_KERNEL_MEM_INFO	**ppsBlockKernelMemInfo,
					  PVRSRV_KERNEL_MEM_INFO	**ppsHWBlockKernelMemInfo,
					  PVRSRV_KERNEL_MEM_INFO	***pppsSharedPBDescSubKernelMemInfos,
					  u32				*ui32SharedPBDescSubKernelMemInfosCount);

 PVRSRV_ERROR
SGXUnrefSharedPBDescKM(void * hSharedPBDesc);

 PVRSRV_ERROR
SGXAddSharedPBDescKM(PVRSRV_PER_PROCESS_DATA	*psPerProc,
					 void * 				hDevCookie,
					 PVRSRV_KERNEL_MEM_INFO		*psSharedPBDescKernelMemInfo,
					 PVRSRV_KERNEL_MEM_INFO		*psHWPBDescKernelMemInfo,
					 PVRSRV_KERNEL_MEM_INFO		*psBlockKernelMemInfo,
					 PVRSRV_KERNEL_MEM_INFO		*psHWBlockKernelMemInfo,
					 u32					ui32TotalPBSize,
					 void *					*phSharedPBDesc,
					 PVRSRV_KERNEL_MEM_INFO		**psSharedPBDescSubKernelMemInfos,
					 u32					ui32SharedPBDescSubKernelMemInfosCount);


 PVRSRV_ERROR
SGXGetInternalDevInfoKM(void * hDevCookie,
						SGX_INTERNAL_DEVINFO *psSGXInternalDevInfo);

#if defined (__cplusplus)
}
#endif

#endif


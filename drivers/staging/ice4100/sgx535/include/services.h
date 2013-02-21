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

#ifndef __SERVICES_H__
#define __SERVICES_H__

#include "img_types.h"
#include "servicesext.h"
#include "pdumpdefs.h"


#define PVRSRV_4K_PAGE_SIZE		4096UL

#define PVRSRV_MAX_CMD_SIZE		1024

#define PVRSRV_MAX_DEVICES		16

#define EVENTOBJNAME_MAXLENGTH (50)

#define PVRSRV_MEM_READ						(1UL<<0)
#define PVRSRV_MEM_WRITE					(1UL<<1)
#define PVRSRV_MEM_CACHE_CONSISTENT			(1UL<<2)
#define PVRSRV_MEM_NO_SYNCOBJ				(1UL<<3)
#define PVRSRV_MEM_INTERLEAVED				(1UL<<4)
#define PVRSRV_MEM_DUMMY					(1UL<<5)
#define PVRSRV_MEM_EDM_PROTECT				(1UL<<6)
#define PVRSRV_MEM_ZERO						(1UL<<7)
#define PVRSRV_MEM_USER_SUPPLIED_DEVVADDR	(1UL<<8)
#define PVRSRV_MEM_RAM_BACKED_ALLOCATION	(1UL<<9)
#define PVRSRV_MEM_NO_RESMAN				(1UL<<10)
#define PVRSRV_MEM_EXPORTED					(1UL<<11)


#define PVRSRV_HAP_CACHED					(1UL<<12)
#define PVRSRV_HAP_UNCACHED					(1UL<<13)
#define PVRSRV_HAP_WRITECOMBINE				(1UL<<14)
#define PVRSRV_HAP_CACHETYPE_MASK			(PVRSRV_HAP_CACHED|PVRSRV_HAP_UNCACHED|PVRSRV_HAP_WRITECOMBINE)
#define PVRSRV_HAP_KERNEL_ONLY				(1UL<<15)
#define PVRSRV_HAP_SINGLE_PROCESS			(1UL<<16)
#define PVRSRV_HAP_MULTI_PROCESS			(1UL<<17)
#define PVRSRV_HAP_FROM_EXISTING_PROCESS	(1UL<<18)
#define PVRSRV_HAP_NO_CPU_VIRTUAL			(1UL<<19)
#define PVRSRV_HAP_MAPTYPE_MASK				(PVRSRV_HAP_KERNEL_ONLY \
                                            |PVRSRV_HAP_SINGLE_PROCESS \
                                            |PVRSRV_HAP_MULTI_PROCESS \
                                            |PVRSRV_HAP_FROM_EXISTING_PROCESS \
                                            |PVRSRV_HAP_NO_CPU_VIRTUAL)

#define PVRSRV_MEM_CACHED					PVRSRV_HAP_CACHED
#define PVRSRV_MEM_UNCACHED					PVRSRV_HAP_UNCACHED
#define PVRSRV_MEM_WRITECOMBINE				PVRSRV_HAP_WRITECOMBINE

#define PVRSRV_MEM_BACKINGSTORE_FIELD_SHIFT	(24)

#define PVRSRV_MAP_NOUSERVIRTUAL            (1UL<<27)

#define PVRSRV_NO_CONTEXT_LOSS					0
#define PVRSRV_SEVERE_LOSS_OF_CONTEXT			1
#define PVRSRV_PRE_STATE_CHANGE_MASK			0x80


#define PVRSRV_DEFAULT_DEV_COOKIE			(1)


#define PVRSRV_MISC_INFO_TIMER_PRESENT				(1UL<<0)
#define PVRSRV_MISC_INFO_CLOCKGATE_PRESENT			(1UL<<1)
#define PVRSRV_MISC_INFO_MEMSTATS_PRESENT			(1UL<<2)
#define PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT	(1UL<<3)
#define PVRSRV_MISC_INFO_DDKVERSION_PRESENT			(1UL<<4)
#define PVRSRV_MISC_INFO_CPUCACHEFLUSH_PRESENT		(1UL<<5)

#define PVRSRV_MISC_INFO_RESET_PRESENT				(1UL<<31)

#define PVRSRV_PDUMP_MAX_FILENAME_SIZE			20
#define PVRSRV_PDUMP_MAX_COMMENT_SIZE			200


#define PVRSRV_CHANGEDEVMEM_ATTRIBS_CACHECOHERENT		0x00000001

#define PVRSRV_MAPEXTMEMORY_FLAGS_ALTERNATEVA			0x00000001
#define PVRSRV_MAPEXTMEMORY_FLAGS_PHYSCONTIG			0x00000002

#define PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC			0x00000001
#define PVRSRV_MODIFYSYNCOPS_FLAGS_RO_INC			0x00000002

typedef enum _PVRSRV_DEVICE_TYPE_
{
	PVRSRV_DEVICE_TYPE_UNKNOWN			= 0 ,
	PVRSRV_DEVICE_TYPE_MBX1				= 1 ,
	PVRSRV_DEVICE_TYPE_MBX1_LITE		= 2 ,

	PVRSRV_DEVICE_TYPE_M24VA			= 3,
	PVRSRV_DEVICE_TYPE_MVDA2			= 4,
	PVRSRV_DEVICE_TYPE_MVED1			= 5,
	PVRSRV_DEVICE_TYPE_MSVDX			= 6,

	PVRSRV_DEVICE_TYPE_SGX				= 7,

	PVRSRV_DEVICE_TYPE_VGX				= 8,


	PVRSRV_DEVICE_TYPE_EXT				= 9,

    PVRSRV_DEVICE_TYPE_LAST             = 9,

	PVRSRV_DEVICE_TYPE_FORCE_I32		= 0x7fffffff

} PVRSRV_DEVICE_TYPE;

#define HEAP_ID( _dev_ , _dev_heap_idx_ )	(  ((_dev_)<<24) | ((_dev_heap_idx_)&((1<<24)-1))  )
#define HEAP_IDX( _heap_id_ )				( (_heap_id_)&((1<<24) - 1 ) )
#define HEAP_DEV( _heap_id_ )				( (_heap_id_)>>24 )

#define PVRSRV_UNDEFINED_HEAP_ID			(~0LU)

typedef enum
{
	IMG_EGL				= 0x00000001,
	IMG_OPENGLES1		= 0x00000002,
	IMG_OPENGLES2		= 0x00000003,
	IMG_D3DM			= 0x00000004,
	IMG_SRV_UM			= 0x00000005,
	IMG_OPENVG			= 0x00000006,
	IMG_SRVCLIENT		= 0x00000007,
	IMG_VISTAKMD		= 0x00000008,
	IMG_VISTA3DNODE		= 0x00000009,
	IMG_VISTAMVIDEONODE	= 0x0000000A,
	IMG_VISTAVPBNODE	= 0x0000000B,
	IMG_OPENGL			= 0x0000000C,
	IMG_D3D				= 0x0000000D,
#if defined(SUPPORT_GRAPHICS_HAL)
	IMG_GRAPHICS_HAL	= 0x0000000E
#endif

} IMG_MODULE_ID;


#define APPHINT_MAX_STRING_SIZE	256

typedef enum
{
	IMG_STRING_TYPE		= 1,
	IMG_FLOAT_TYPE		,
	IMG_UINT_TYPE		,
	IMG_INT_TYPE		,
	IMG_FLAG_TYPE
}IMG_DATA_TYPE;


typedef struct _PVRSRV_DEV_DATA_ *PPVRSRV_DEV_DATA;

typedef struct _PVRSRV_DEVICE_IDENTIFIER_
{
	PVRSRV_DEVICE_TYPE		eDeviceType;
	PVRSRV_DEVICE_CLASS		eDeviceClass;
	u32				ui32DeviceIndex;

} PVRSRV_DEVICE_IDENTIFIER;


typedef struct _PVRSRV_CLIENT_DEV_DATA_
{
	u32		ui32NumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDevID[PVRSRV_MAX_DEVICES];
	PVRSRV_ERROR	(*apfnDevConnect[PVRSRV_MAX_DEVICES])(PPVRSRV_DEV_DATA);
	PVRSRV_ERROR	(*apfnDumpTrace[PVRSRV_MAX_DEVICES])(PPVRSRV_DEV_DATA);

} PVRSRV_CLIENT_DEV_DATA;


typedef struct _PVRSRV_CONNECTION_
{
	void * hServices;
	u32 ui32ProcessID;
	PVRSRV_CLIENT_DEV_DATA	sClientDevData;
}PVRSRV_CONNECTION;


typedef struct _PVRSRV_DEV_DATA_
{
	PVRSRV_CONNECTION	sConnection;
	void *			hDevCookie;

} PVRSRV_DEV_DATA;

typedef struct _PVRSRV_MEMUPDATE_
{
	u32			ui32UpdateAddr;
	u32			ui32UpdateVal;
} PVRSRV_MEMUPDATE;

typedef struct _PVRSRV_HWREG_
{
	u32			ui32RegAddr;
	u32			ui32RegVal;
} PVRSRV_HWREG;

typedef struct _PVRSRV_MEMBLK_
{
	IMG_DEV_VIRTADDR	sDevVirtAddr;
	void *			hOSMemHandle;
	void *			hOSWrapMem;
	void *			hBuffer;
	void *			hResItem;
	IMG_SYS_PHYADDR	 	*psIntSysPAddr;

} PVRSRV_MEMBLK;

typedef struct _PVRSRV_KERNEL_MEM_INFO_ *PPVRSRV_KERNEL_MEM_INFO;

typedef struct _PVRSRV_CLIENT_MEM_INFO_
{

	void *				pvLinAddr;


	void *				pvLinAddrKM;


	IMG_DEV_VIRTADDR		sDevVAddr;






	IMG_CPU_PHYADDR			sCpuPAddr;


	u32				ui32Flags;




	u32				ui32ClientFlags;


	u32				ui32AllocSize;



	struct _PVRSRV_CLIENT_SYNC_INFO_	*psClientSyncInfo;


	void *							hMappingInfo;


	void *							hKernelMemInfo;


	void *							hResItem;

#if defined(SUPPORT_MEMINFO_IDS)
	#if !defined(USE_CODE)

	u64							ui64Stamp;
	#else
	u32							dummy1;
	u32							dummy2;
	#endif
#endif




	struct _PVRSRV_CLIENT_MEM_INFO_		*psNext;

} PVRSRV_CLIENT_MEM_INFO, *PPVRSRV_CLIENT_MEM_INFO;


#define PVRSRV_MAX_CLIENT_HEAPS (32)
typedef struct _PVRSRV_HEAP_INFO_
{
	u32			ui32HeapID;
	void * 			hDevMemHeap;
	IMG_DEV_VIRTADDR	sDevVAddrBase;
	u32			ui32HeapByteSize;
	u32			ui32Attribs;
}PVRSRV_HEAP_INFO;




typedef struct _PVRSRV_EVENTOBJECT_
{

	char	szName[EVENTOBJNAME_MAXLENGTH];

	void *	hOSEventKM;

} PVRSRV_EVENTOBJECT;

typedef struct _PVRSRV_MISC_INFO_
{
	u32	ui32StateRequest;
	u32	ui32StatePresent;


	void	*pvSOCTimerRegisterKM;
	void	*pvSOCTimerRegisterUM;
	void *	hSOCTimerRegisterOSMemHandle;
	void *	hSOCTimerRegisterMappingInfo;


	void	*pvSOCClockGateRegs;
	u32	ui32SOCClockGateRegsSize;


	char	*pszMemoryStr;
	u32	ui32MemoryStrLen;


	PVRSRV_EVENTOBJECT	sGlobalEventObject;
	void *			hOSGlobalEvent;


	u32	aui32DDKVersion[4];



	int	bCPUCacheFlushAll;

	int	bDeferCPUCacheFlush;

	void *	pvRangeAddrStart;

	void *	pvRangeAddrEnd;

} PVRSRV_MISC_INFO;


typedef enum _PVRSRV_CLIENT_EVENT_
{
	PVRSRV_CLIENT_EVENT_HWTIMEOUT = 0,
} PVRSRV_CLIENT_EVENT;


PVRSRV_ERROR  PVRSRVClientEvent(const PVRSRV_CLIENT_EVENT eEvent,
											PVRSRV_DEV_DATA *psDevData,
											void * pvData);


PVRSRV_ERROR PVRSRVConnect(PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR PVRSRVDisconnect(PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR PVRSRVEnumerateDevices(const PVRSRV_CONNECTION 			*psConnection,
									u32 					*puiNumDevices,
									PVRSRV_DEVICE_IDENTIFIER 	*puiDevIDs);

PVRSRV_ERROR PVRSRVAcquireDeviceData(const PVRSRV_CONNECTION 	*psConnection,
									u32			uiDevIndex,
									PVRSRV_DEV_DATA		*psDevData,
									PVRSRV_DEVICE_TYPE	eDeviceType);

PVRSRV_ERROR  PVRSRVGetMiscInfo (const PVRSRV_CONNECTION *psConnection, PVRSRV_MISC_INFO *psMiscInfo);


PVRSRV_ERROR PVRSRVReleaseMiscInfo (const PVRSRV_CONNECTION *psConnection, PVRSRV_MISC_INFO *psMiscInfo);

#if 1

u32 ReadHWReg(void * pvLinRegBaseAddr, u32 ui32Offset);


void WriteHWReg(void * pvLinRegBaseAddr, u32 ui32Offset, u32 ui32Value);

 void WriteHWRegs(void * pvLinRegBaseAddr, u32 ui32Count, PVRSRV_HWREG *psHWRegs);
#endif


PVRSRV_ERROR PVRSRVPollForValue ( const PVRSRV_CONNECTION *psConnection,
							void * hOSEvent,
							volatile u32 *pui32LinMemAddr,
							u32 ui32Value,
							u32 ui32Mask,
							u32 ui32Waitus,
							u32 ui32Tries);


PVRSRV_ERROR PVRSRVCreateDeviceMemContext(const PVRSRV_DEV_DATA *psDevData,
											void * *phDevMemContext,
											u32 *pui32SharedHeapCount,
											PVRSRV_HEAP_INFO *psHeapInfo);


PVRSRV_ERROR  PVRSRVDestroyDeviceMemContext(const PVRSRV_DEV_DATA *psDevData,
											void * 			hDevMemContext);


PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfo(const PVRSRV_DEV_DATA *psDevData,
											void * hDevMemContext,
											u32 *pui32SharedHeapCount,
											PVRSRV_HEAP_INFO *psHeapInfo);

#if defined(PVRSRV_LOG_MEMORY_ALLOCS)
	#define PVRSRVAllocDeviceMem_log(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo, logStr) \
		(PVR_TRACE(("PVRSRVAllocDeviceMem(" #psDevData "," #hDevMemHeap "," #ui32Attribs "," #ui32Size "," #ui32Alignment "," #ppsMemInfo ")" \
			": " logStr " (size = 0x%lx)", ui32Size)), \
		PVRSRVAllocDeviceMem(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo))
#else
	#define PVRSRVAllocDeviceMem_log(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo, logStr) \
		PVRSRVAllocDeviceMem(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)
#endif



PVRSRV_ERROR PVRSRVAllocDeviceMem(const PVRSRV_DEV_DATA	*psDevData,
									void *		hDevMemHeap,
									u32		ui32Attribs,
									u32		ui32Size,
									u32		ui32Alignment,
									PVRSRV_CLIENT_MEM_INFO	**ppsMemInfo);


PVRSRV_ERROR PVRSRVFreeDeviceMem(const PVRSRV_DEV_DATA	*psDevData,
								PVRSRV_CLIENT_MEM_INFO		*psMemInfo);


PVRSRV_ERROR PVRSRVExportDeviceMem(const PVRSRV_DEV_DATA	*psDevData,
												PVRSRV_CLIENT_MEM_INFO		*psMemInfo,
												void *					*phMemInfo);


PVRSRV_ERROR PVRSRVReserveDeviceVirtualMem(const PVRSRV_DEV_DATA *psDevData,
											void *			hDevMemHeap,
											IMG_DEV_VIRTADDR	*psDevVAddr,
											u32			ui32Size,
											u32			ui32Alignment,
											PVRSRV_CLIENT_MEM_INFO		**ppsMemInfo);

PVRSRV_ERROR PVRSRVFreeDeviceVirtualMem(const PVRSRV_DEV_DATA *psDevData,
													PVRSRV_CLIENT_MEM_INFO *psMemInfo);


PVRSRV_ERROR PVRSRVMapDeviceMemory (const PVRSRV_DEV_DATA *psDevData,
									void * hKernelMemInfo,
									void * hDstDevMemHeap,
									PVRSRV_CLIENT_MEM_INFO **ppsDstMemInfo);


PVRSRV_ERROR PVRSRVUnmapDeviceMemory (const PVRSRV_DEV_DATA *psDevData,
										PVRSRV_CLIENT_MEM_INFO *psMemInfo);


PVRSRV_ERROR PVRSRVMapExtMemory (const PVRSRV_DEV_DATA	*psDevData,
									PVRSRV_CLIENT_MEM_INFO		*psMemInfo,
									IMG_SYS_PHYADDR				*psSysPAddr,
									u32					ui32Flags);

PVRSRV_ERROR PVRSRVUnmapExtMemory (const PVRSRV_DEV_DATA *psDevData,
									PVRSRV_CLIENT_MEM_INFO		*psMemInfo,
									u32					ui32Flags);


PVRSRV_ERROR  PVRSRVWrapExtMemory2(const PVRSRV_DEV_DATA *psDevData,
												void *				hDevMemContext,
												u32 				ui32ByteSize,
												u32				ui32PageOffset,
												int				bPhysContig,
												IMG_SYS_PHYADDR	 		*psSysPAddr,
												void 				*pvLinAddr,
												u32				ui32Flags,
												PVRSRV_CLIENT_MEM_INFO **ppsMemInfo);

PVRSRV_ERROR  PVRSRVWrapExtMemory(const PVRSRV_DEV_DATA *psDevData,
												void *				hDevMemContext,
												u32 				ui32ByteSize,
												u32				ui32PageOffset,
												int				bPhysContig,
												IMG_SYS_PHYADDR	 		*psSysPAddr,
												void 				*pvLinAddr,
												PVRSRV_CLIENT_MEM_INFO **ppsMemInfo);

PVRSRV_ERROR PVRSRVUnwrapExtMemory (const PVRSRV_DEV_DATA *psDevData,
												PVRSRV_CLIENT_MEM_INFO *psMemInfo);

PVRSRV_ERROR PVRSRVChangeDeviceMemoryAttributes(const PVRSRV_DEV_DATA			*psDevData,
												PVRSRV_CLIENT_MEM_INFO	*psClientMemInfo,
												u32				ui32Attribs);


PVRSRV_ERROR PVRSRVMapDeviceClassMemory (const PVRSRV_DEV_DATA *psDevData,
										void * hDevMemContext,
										void * hDeviceClassBuffer,
										PVRSRV_CLIENT_MEM_INFO **ppsMemInfo);

PVRSRV_ERROR PVRSRVUnmapDeviceClassMemory (const PVRSRV_DEV_DATA *psDevData,
										PVRSRV_CLIENT_MEM_INFO *psMemInfo);


PVRSRV_ERROR PVRSRVMapPhysToUserSpace(const PVRSRV_DEV_DATA *psDevData,
									  IMG_SYS_PHYADDR sSysPhysAddr,
									  u32 uiSizeInBytes,
									  void * *ppvUserAddr,
									  u32 *puiActualSize,
									  void * *ppvProcess);


PVRSRV_ERROR PVRSRVUnmapPhysToUserSpace(const PVRSRV_DEV_DATA *psDevData,
										void * pvUserAddr,
										void * pvProcess);

typedef enum _PVRSRV_SYNCVAL_MODE_
{
	PVRSRV_SYNCVAL_READ				= 1,
	PVRSRV_SYNCVAL_WRITE			= 0,

} PVRSRV_SYNCVAL_MODE, *PPVRSRV_SYNCVAL_MODE;

typedef u32 PVRSRV_SYNCVAL;

 PVRSRV_ERROR PVRSRVWaitForOpsComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode, PVRSRV_SYNCVAL OpRequired);

 PVRSRV_ERROR PVRSRVWaitForAllOpsComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode);

 int PVRSRVTestOpsComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode, PVRSRV_SYNCVAL OpRequired);

 int PVRSRVTestAllOpsComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode);

 int PVRSRVTestOpsNotComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode, PVRSRV_SYNCVAL OpRequired);

 int PVRSRVTestAllOpsNotComplete(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode);

 PVRSRV_SYNCVAL PVRSRVGetPendingOpSyncVal(PPVRSRV_CLIENT_MEM_INFO psMemInfo,
	PVRSRV_SYNCVAL_MODE eMode);



PVRSRV_ERROR  PVRSRVEnumerateDeviceClass(const PVRSRV_CONNECTION *psConnection,
													PVRSRV_DEVICE_CLASS DeviceClass,
													u32 *pui32DevCount,
													u32 *pui32DevID);


void *  PVRSRVOpenDCDevice(const PVRSRV_DEV_DATA *psDevData,
											u32 ui32DeviceID);


PVRSRV_ERROR  PVRSRVCloseDCDevice(const PVRSRV_CONNECTION	*psConnection, void * hDevice);


PVRSRV_ERROR  PVRSRVEnumDCFormats (void * hDevice,
											u32		*pui32Count,
											DISPLAY_FORMAT	*psFormat);


PVRSRV_ERROR  PVRSRVEnumDCDims (void * hDevice,
										u32 		*pui32Count,
										DISPLAY_FORMAT	*psFormat,
										DISPLAY_DIMS	*psDims);


PVRSRV_ERROR  PVRSRVGetDCSystemBuffer(void * hDevice,
										void * *phBuffer);


PVRSRV_ERROR  PVRSRVGetDCInfo(void * hDevice,
										DISPLAY_INFO* psDisplayInfo);


PVRSRV_ERROR  PVRSRVCreateDCSwapChain (void *				hDevice,
													u32				ui32Flags,
													DISPLAY_SURF_ATTRIBUTES	*psDstSurfAttrib,
													DISPLAY_SURF_ATTRIBUTES	*psSrcSurfAttrib,
													u32				ui32BufferCount,
													u32				ui32OEMFlags,
													u32				*pui32SwapChainID,
													void *				*phSwapChain);


PVRSRV_ERROR  PVRSRVDestroyDCSwapChain (void * hDevice,
											void *		hSwapChain);


PVRSRV_ERROR  PVRSRVSetDCDstRect (void * hDevice,
										void *	hSwapChain,
										IMG_RECT	*psDstRect);


PVRSRV_ERROR  PVRSRVSetDCSrcRect (void * hDevice,
										void *	hSwapChain,
										IMG_RECT	*psSrcRect);


PVRSRV_ERROR  PVRSRVSetDCDstColourKey (void * hDevice,
											void *	hSwapChain,
											u32	ui32CKColour);


PVRSRV_ERROR  PVRSRVSetDCSrcColourKey (void * hDevice,
											void *	hSwapChain,
											u32	ui32CKColour);


PVRSRV_ERROR  PVRSRVGetDCBuffers(void * hDevice,
									void * hSwapChain,
									void * *phBuffer);


PVRSRV_ERROR  PVRSRVSwapToDCBuffer (void * hDevice,
										void * hBuffer,
										u32 ui32ClipRectCount,
										IMG_RECT *psClipRect,
										u32 ui32SwapInterval,
										void * hPrivateTag);


PVRSRV_ERROR  PVRSRVSwapToDCSystem (void * hDevice,
										void * hSwapChain);



void *  PVRSRVOpenBCDevice(const PVRSRV_DEV_DATA *psDevData,
											u32 ui32DeviceID);


PVRSRV_ERROR  PVRSRVCloseBCDevice(const PVRSRV_CONNECTION *psConnection,
												void * hDevice);


PVRSRV_ERROR  PVRSRVGetBCBufferInfo(void * hDevice,
												BUFFER_INFO	*psBuffer);


PVRSRV_ERROR  PVRSRVGetBCBuffer(void * hDevice,
												u32 ui32BufferIndex,
												void * *phBuffer);



PVRSRV_ERROR  PVRSRVPDumpInit(const PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR  PVRSRVPDumpStartInitPhase(const PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR  PVRSRVPDumpStopInitPhase(const PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR  PVRSRVPDumpMemPol(const PVRSRV_CONNECTION *psConnection,
										  PVRSRV_CLIENT_MEM_INFO *psMemInfo,
										  u32 ui32Offset,
										  u32 ui32Value,
										  u32 ui32Mask,
										  u32 ui32Flags);


PVRSRV_ERROR  PVRSRVPDumpSyncPol(const PVRSRV_CONNECTION *psConnection,
										  PVRSRV_CLIENT_SYNC_INFO *psClientSyncInfo,
										  int bIsRead,
										  u32 ui32Value,
										  u32 ui32Mask);


PVRSRV_ERROR  PVRSRVPDumpMem(const PVRSRV_CONNECTION *psConnection,
									void * pvAltLinAddr,
									PVRSRV_CLIENT_MEM_INFO *psMemInfo,
									u32 ui32Offset,
									u32 ui32Bytes,
									u32 ui32Flags);


PVRSRV_ERROR  PVRSRVPDumpSync(const PVRSRV_CONNECTION *psConnection,
										void * pvAltLinAddr,
										PVRSRV_CLIENT_SYNC_INFO *psClientSyncInfo,
										u32 ui32Offset,
										u32 ui32Bytes);


PVRSRV_ERROR  PVRSRVPDumpReg(const PVRSRV_CONNECTION *psConnection,
											u32 ui32RegAddr,
											u32 ui32RegValue,
											u32 ui32Flags);


PVRSRV_ERROR  PVRSRVPDumpRegPolWithFlags(const PVRSRV_CONNECTION *psConnection,
													 u32 ui32RegAddr,
													 u32 ui32RegValue,
													 u32 ui32Mask,
													 u32 ui32Flags);

PVRSRV_ERROR  PVRSRVPDumpRegPol(const PVRSRV_CONNECTION *psConnection,
											u32 ui32RegAddr,
											u32 ui32RegValue,
											u32 ui32Mask);


PVRSRV_ERROR  PVRSRVPDumpPDReg(const PVRSRV_CONNECTION *psConnection,
											u32 ui32RegAddr,
											u32 ui32RegValue);

PVRSRV_ERROR  PVRSRVPDumpPDDevPAddr(const PVRSRV_CONNECTION *psConnection,
												PVRSRV_CLIENT_MEM_INFO *psMemInfo,
												u32 ui32Offset,
												IMG_DEV_PHYADDR sPDDevPAddr);


PVRSRV_ERROR  PVRSRVPDumpMemPages(const PVRSRV_CONNECTION *psConnection,
												void *			hKernelMemInfo,
												IMG_DEV_PHYADDR		*pPages,
												u32			ui32NumPages,
												IMG_DEV_VIRTADDR	sDevAddr,
												u32			ui32Start,
												u32			ui32Length,
												int			bContinuous);


PVRSRV_ERROR  PVRSRVPDumpSetFrame(const PVRSRV_CONNECTION *psConnection,
											  u32 ui32Frame);


PVRSRV_ERROR  PVRSRVPDumpComment(const PVRSRV_CONNECTION *psConnection,
											 const char *pszComment,
											 int bContinuous);


PVRSRV_ERROR  PVRSRVPDumpCommentf(const PVRSRV_CONNECTION *psConnection,
											  int bContinuous,
											  const char *pszFormat, ...);


PVRSRV_ERROR  PVRSRVPDumpCommentWithFlagsf(const PVRSRV_CONNECTION *psConnection,
													   u32 ui32Flags,
													   const char *pszFormat, ...);


PVRSRV_ERROR  PVRSRVPDumpDriverInfo(const PVRSRV_CONNECTION *psConnection,
								 				char *pszString,
												int bContinuous);


PVRSRV_ERROR  PVRSRVPDumpIsCapturing(const PVRSRV_CONNECTION *psConnection,
								 				int *pbIsCapturing);


PVRSRV_ERROR  PVRSRVPDumpBitmap(const PVRSRV_CONNECTION *psConnection,
								 			char *pszFileName,
											u32 ui32FileOffset,
											u32 ui32Width,
											u32 ui32Height,
											u32 ui32StrideInBytes,
											IMG_DEV_VIRTADDR sDevBaseAddr,
											u32 ui32Size,
											PDUMP_PIXEL_FORMAT ePixelFormat,
											PDUMP_MEM_FORMAT eMemFormat,
											u32 ui32PDumpFlags);


PVRSRV_ERROR  PVRSRVPDumpRegRead(const PVRSRV_CONNECTION *psConnection,
								 			const char *pszFileName,
											u32 ui32FileOffset,
											u32 ui32Address,
											u32 ui32Size,
											u32 ui32PDumpFlags);



int  PVRSRVPDumpIsCapturingTest(const PVRSRV_CONNECTION *psConnection);


PVRSRV_ERROR  PVRSRVPDumpCycleCountRegRead(const PVRSRV_CONNECTION *psConnection,
														u32 ui32RegOffset,
														int bLastFrame);

 void *	PVRSRVLoadLibrary(const char *pszLibraryName);
 PVRSRV_ERROR	PVRSRVUnloadLibrary(void * hExtDrv);
 PVRSRV_ERROR	PVRSRVGetLibFuncAddr(void * hExtDrv, const char *pszFunctionName, void **ppvFuncAddr);

 u32 PVRSRVClockus (void);
 void PVRSRVWaitus (u32 ui32Timeus);
 void PVRSRVReleaseThreadQuanta (void);
 u32  PVRSRVGetCurrentProcessID(void);
 char *  PVRSRVSetLocale(const char *pszLocale);





 void  PVRSRVCreateAppHintState(IMG_MODULE_ID eModuleID,
														const char *pszAppName,
														void **ppvState);
 void  PVRSRVFreeAppHintState(IMG_MODULE_ID eModuleID,
										 void *pvHintState);

 int  PVRSRVGetAppHint(void			*pvHintState,
												  const char	*pszHintName,
												  IMG_DATA_TYPE		eDataType,
												  const void	*pvDefault,
												  void			*pvReturn);

 void *  PVRSRVAllocUserModeMem (u32 ui32Size);
 void *  PVRSRVCallocUserModeMem (u32 ui32Size);
 void *  PVRSRVReallocUserModeMem (void * pvBase, u32 uNewSize);
 void   PVRSRVFreeUserModeMem (void * pvMem);
 void PVRSRVMemCopy(void *pvDst, const void *pvSrc, u32 ui32Size);
 void PVRSRVMemSet(void *pvDest, u8 ui8Value, u32 ui32Size);

struct _PVRSRV_MUTEX_OPAQUE_STRUCT_;
typedef	struct  _PVRSRV_MUTEX_OPAQUE_STRUCT_ *PVRSRV_MUTEX_HANDLE;

 PVRSRV_ERROR  PVRSRVCreateMutex(PVRSRV_MUTEX_HANDLE *phMutex);
 PVRSRV_ERROR  PVRSRVDestroyMutex(PVRSRV_MUTEX_HANDLE hMutex);
 void  PVRSRVLockMutex(PVRSRV_MUTEX_HANDLE hMutex);
 void  PVRSRVUnlockMutex(PVRSRV_MUTEX_HANDLE hMutex);

#if (defined(DEBUG) && defined(__linux__))
void * PVRSRVAllocUserModeMemTracking(u32 ui32Size, char *pszFileName, u32 ui32LineNumber);
void * PVRSRVCallocUserModeMemTracking(u32 ui32Size, char *pszFileName, u32 ui32LineNumber);
void  PVRSRVFreeUserModeMemTracking(void *pvMem);
void * PVRSRVReallocUserModeMemTracking(void *pvMem, u32 ui32NewSize, char *pszFileName, u32 ui32LineNumber);
#endif

 PVRSRV_ERROR PVRSRVEventObjectWait(const PVRSRV_CONNECTION *psConnection,
									void * hOSEvent);


PVRSRV_ERROR  PVRSRVModifyPendingSyncOps(PVRSRV_CONNECTION *psConnection,
													  void * hKernelSyncInfo,
													  u32 ui32ModifyFlags,
													  u32 *pui32ReadOpsPending,
													  u32 *pui32WriteOpsPending);


PVRSRV_ERROR  PVRSRVModifyCompleteSyncOps(PVRSRV_CONNECTION *psConnection,
													  void * hKernelSyncInfo,
													  u32 ui32ModifyFlags);


#define TIME_NOT_PASSED_UINT32(a,b,c)		((a - b) < c)

#endif


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

#if !defined (__KERNELDISPLAY_H__)
#define __KERNELDISPLAY_H__

typedef PVRSRV_ERROR (*PFN_OPEN_DC_DEVICE)(u32, void **, PVRSRV_SYNC_DATA*);
typedef PVRSRV_ERROR (*PFN_CLOSE_DC_DEVICE)(void *);
typedef PVRSRV_ERROR (*PFN_ENUM_DC_FORMATS)(void *, u32*, DISPLAY_FORMAT*);
typedef PVRSRV_ERROR (*PFN_ENUM_DC_DIMS)(void *,
										 DISPLAY_FORMAT*,
										 u32*,
										 DISPLAY_DIMS*);
typedef PVRSRV_ERROR (*PFN_GET_DC_SYSTEMBUFFER)(void *, void **);
typedef PVRSRV_ERROR (*PFN_GET_DC_INFO)(void *, DISPLAY_INFO*);
typedef PVRSRV_ERROR (*PFN_CREATE_DC_SWAPCHAIN)(void *,
												u32,
												DISPLAY_SURF_ATTRIBUTES*,
												DISPLAY_SURF_ATTRIBUTES*,
												u32,
												PVRSRV_SYNC_DATA**,
												u32,
												void **,
												u32*);
typedef PVRSRV_ERROR (*PFN_DESTROY_DC_SWAPCHAIN)(void *,
												 void *);
typedef PVRSRV_ERROR (*PFN_SET_DC_DSTRECT)(void *, void *, IMG_RECT*);
typedef PVRSRV_ERROR (*PFN_SET_DC_SRCRECT)(void *, void *, IMG_RECT*);
typedef PVRSRV_ERROR (*PFN_SET_DC_DSTCK)(void *, void *, u32);
typedef PVRSRV_ERROR (*PFN_SET_DC_SRCCK)(void *, void *, u32);
typedef PVRSRV_ERROR (*PFN_GET_DC_BUFFERS)(void *,
										   void *,
										   u32*,
										   void **);
typedef PVRSRV_ERROR (*PFN_SWAP_TO_DC_BUFFER)(void *,
											  void *,
											  u32,
											  void *,
											  u32,
											  IMG_RECT*);
typedef PVRSRV_ERROR (*PFN_SWAP_TO_DC_SYSTEM)(void *, void *);
typedef void (*PFN_QUERY_SWAP_COMMAND_ID)(void *, void *, void *, void *, u16*, int*);
typedef void (*PFN_SET_DC_STATE)(void *, u32);

typedef struct PVRSRV_DC_SRV2DISP_KMJTABLE_TAG
{
	u32						ui32TableSize;
	PFN_OPEN_DC_DEVICE				pfnOpenDCDevice;
	PFN_CLOSE_DC_DEVICE				pfnCloseDCDevice;
	PFN_ENUM_DC_FORMATS				pfnEnumDCFormats;
	PFN_ENUM_DC_DIMS				pfnEnumDCDims;
	PFN_GET_DC_SYSTEMBUFFER			pfnGetDCSystemBuffer;
	PFN_GET_DC_INFO					pfnGetDCInfo;
	PFN_GET_BUFFER_ADDR				pfnGetBufferAddr;
	PFN_CREATE_DC_SWAPCHAIN			pfnCreateDCSwapChain;
	PFN_DESTROY_DC_SWAPCHAIN		pfnDestroyDCSwapChain;
	PFN_SET_DC_DSTRECT				pfnSetDCDstRect;
	PFN_SET_DC_SRCRECT				pfnSetDCSrcRect;
	PFN_SET_DC_DSTCK				pfnSetDCDstColourKey;
	PFN_SET_DC_SRCCK				pfnSetDCSrcColourKey;
	PFN_GET_DC_BUFFERS				pfnGetDCBuffers;
	PFN_SWAP_TO_DC_BUFFER			pfnSwapToDCBuffer;
	PFN_SWAP_TO_DC_SYSTEM			pfnSwapToDCSystem;
	PFN_SET_DC_STATE				pfnSetDCState;
	PFN_QUERY_SWAP_COMMAND_ID		pfnQuerySwapCommandID;
} PVRSRV_DC_SRV2DISP_KMJTABLE;

typedef int (*PFN_ISR_HANDLER)(void*);

typedef PVRSRV_ERROR (*PFN_DC_REGISTER_DISPLAY_DEV)(PVRSRV_DC_SRV2DISP_KMJTABLE*, u32*);
typedef PVRSRV_ERROR (*PFN_DC_REMOVE_DISPLAY_DEV)(u32);
typedef PVRSRV_ERROR (*PFN_DC_OEM_FUNCTION)(u32, void*, u32, void*, u32);
typedef PVRSRV_ERROR (*PFN_DC_REGISTER_COMMANDPROCLIST)(u32, PPFN_CMD_PROC,u32[][2], u32);
typedef PVRSRV_ERROR (*PFN_DC_REMOVE_COMMANDPROCLIST)(u32, u32);
typedef void (*PFN_DC_CMD_COMPLETE)(void *, int);
typedef PVRSRV_ERROR (*PFN_DC_REGISTER_SYS_ISR)(PFN_ISR_HANDLER, void*, u32, u32);
typedef PVRSRV_ERROR (*PFN_DC_REGISTER_POWER)(u32, PFN_PRE_POWER, PFN_POST_POWER,
											  PFN_PRE_CLOCKSPEED_CHANGE, PFN_POST_CLOCKSPEED_CHANGE,
											  void *, PVRSRV_DEV_POWER_STATE, PVRSRV_DEV_POWER_STATE);

typedef struct PVRSRV_DC_DISP2SRV_KMJTABLE_TAG
{
	u32						ui32TableSize;
	PFN_DC_REGISTER_DISPLAY_DEV		pfnPVRSRVRegisterDCDevice;
	PFN_DC_REMOVE_DISPLAY_DEV		pfnPVRSRVRemoveDCDevice;
	PFN_DC_OEM_FUNCTION				pfnPVRSRVOEMFunction;
	PFN_DC_REGISTER_COMMANDPROCLIST	pfnPVRSRVRegisterCmdProcList;
	PFN_DC_REMOVE_COMMANDPROCLIST	pfnPVRSRVRemoveCmdProcList;
	PFN_DC_CMD_COMPLETE				pfnPVRSRVCmdComplete;
	PFN_DC_REGISTER_SYS_ISR			pfnPVRSRVRegisterSystemISRHandler;
	PFN_DC_REGISTER_POWER			pfnPVRSRVRegisterPowerDevice;
	PFN_DC_CMD_COMPLETE				pfnPVRSRVFreeCmdCompletePacket;
} PVRSRV_DC_DISP2SRV_KMJTABLE, *PPVRSRV_DC_DISP2SRV_KMJTABLE;


typedef struct DISPLAYCLASS_FLIP_COMMAND_TAG
{

	void * hExtDevice;


	void * hExtSwapChain;


	void * hExtBuffer;


	void * hPrivateTag;


	u32 ui32ClipRectCount;


	IMG_RECT *psClipRect;


	u32	ui32SwapInterval;

} DISPLAYCLASS_FLIP_COMMAND;

#define DC_FLIP_COMMAND		0

#define DC_STATE_NO_FLUSH_COMMANDS		0
#define DC_STATE_FLUSH_COMMANDS			1


typedef int (*PFN_DC_GET_PVRJTABLE)(PPVRSRV_DC_DISP2SRV_KMJTABLE);



#endif


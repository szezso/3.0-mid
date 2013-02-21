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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <asm/intel_scu_ipc.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "mrstlfb.h"

#include "psb_fb.h"
#include "psb_drv.h"
#include "psb_powermgmt.h"

#if !defined(SUPPORT_DRI_DRM)
#error "SUPPORT_DRI_DRM must be set"
#endif

static void *gpvAnchor;

static PFN_DC_GET_PVRJTABLE pfnGetPVRJTable = 0;

static MRSTLFB_DEVINFO * GetAnchorPtr(void)
{
	return (MRSTLFB_DEVINFO *)gpvAnchor;
}

static void SetAnchorPtr(MRSTLFB_DEVINFO *psDevInfo)
{
	gpvAnchor = (void*)psDevInfo;
}

static IMG_VOID SetDCState(IMG_HANDLE hDevice, IMG_UINT32 ui32State)
{
}

static MRST_ERROR UnblankDisplay(MRSTLFB_DEVINFO *psDevInfo)
{
}

static MRST_ERROR DisableLFBEventNotification(MRSTLFB_DEVINFO *psDevInfo)
{
	int res;


	res = fb_unregister_client(&psDevInfo->sLINNotifBlock);
	if (res != 0)
	{
		printk(KERN_WARNING DRIVER_PREFIX
			": fb_unregister_client failed (%d)", res);
		return (MRST_ERROR_GENERIC);
	}

	return (MRST_OK);
}

static PVRSRV_ERROR OpenDCDevice(IMG_UINT32 ui32DeviceID,
                                 IMG_HANDLE *phDevice,
                                 PVRSRV_SYNC_DATA* psSystemBufferSyncData)
{
	MRSTLFB_DEVINFO *psDevInfo;

	UNREFERENCED_PARAMETER(ui32DeviceID);

	psDevInfo = GetAnchorPtr();

	psDevInfo->sSystemBuffer.psSyncData = psSystemBufferSyncData;

	psDevInfo->ulSetFlushStateRefCount = 0;
	psDevInfo->bFlushCommands = MRST_FALSE;

	*phDevice = (IMG_HANDLE)psDevInfo;

	return (PVRSRV_OK);
}

static PVRSRV_ERROR CloseDCDevice(IMG_HANDLE hDevice)
{
	return (PVRSRV_OK);
}

static PVRSRV_ERROR EnumDCFormats(IMG_HANDLE hDevice,
                                  IMG_UINT32 *pui32NumFormats,
                                  DISPLAY_FORMAT *psFormat)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !pui32NumFormats)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*pui32NumFormats = 1;

	if(psFormat)
	{
		psFormat[0] = psDevInfo->sDisplayFormat;
	}

	return (PVRSRV_OK);
}

static PVRSRV_ERROR EnumDCDims(IMG_HANDLE hDevice,
                               DISPLAY_FORMAT *psFormat,
                               IMG_UINT32 *pui32NumDims,
                               DISPLAY_DIMS *psDim)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !psFormat || !pui32NumDims)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*pui32NumDims = 1;

	if(psDim)
	{
		psDim[0] = psDevInfo->sDisplayDim;
	}

	return (PVRSRV_OK);
}


static PVRSRV_ERROR GetDCSystemBuffer(IMG_HANDLE hDevice, IMG_HANDLE *phBuffer)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !phBuffer)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*phBuffer = (IMG_HANDLE)&psDevInfo->sSystemBuffer;

	return (PVRSRV_OK);
}


static PVRSRV_ERROR GetDCInfo(IMG_HANDLE hDevice, DISPLAY_INFO *psDCInfo)
{
	MRSTLFB_DEVINFO	*psDevInfo;

	if(!hDevice || !psDCInfo)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	*psDCInfo = psDevInfo->sDisplayInfo;

	return (PVRSRV_OK);
}

static PVRSRV_ERROR GetDCBufferAddr(IMG_HANDLE        hDevice,
                                    IMG_HANDLE        hBuffer, 
                                    IMG_SYS_PHYADDR   **ppsSysAddr,
                                    IMG_SIZE_T        *pui32ByteSize,
                                    IMG_VOID          **ppvCpuVAddr,
                                    IMG_HANDLE        *phOSMapInfo,
                                    IMG_BOOL          *pbIsContiguous,
	                            IMG_UINT32	      *pui32TilingStride)
{
	MRSTLFB_DEVINFO	*psDevInfo;
	MRSTLFB_BUFFER *psSystemBuffer;

	UNREFERENCED_PARAMETER(pui32TilingStride);

	if(!hDevice)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	psDevInfo = (MRSTLFB_DEVINFO*)hDevice;

	if(!hBuffer)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	psSystemBuffer = (MRSTLFB_BUFFER *)hBuffer;

	if (!ppsSysAddr)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}

	if( psSystemBuffer->bIsContiguous ) 
		*ppsSysAddr = &psSystemBuffer->uSysAddr.sCont;
	else
		*ppsSysAddr = psSystemBuffer->uSysAddr.psNonCont;

	if (!pui32ByteSize)
	{
		return (PVRSRV_ERROR_INVALID_PARAMS);
	}
	*pui32ByteSize = psSystemBuffer->ui32BufferSize;

	if (ppvCpuVAddr)
	{
		*ppvCpuVAddr = psSystemBuffer->sCPUVAddr;
	}

	if (phOSMapInfo)
	{
		*phOSMapInfo = (IMG_HANDLE)0;
	}

	if (pbIsContiguous)
	{
		*pbIsContiguous = psSystemBuffer->bIsContiguous;
	}

	return (PVRSRV_OK);
}

static PVRSRV_ERROR CreateDCSwapChain(IMG_HANDLE hDevice,
                                      IMG_UINT32 ui32Flags,
                                      DISPLAY_SURF_ATTRIBUTES *psDstSurfAttrib,
                                      DISPLAY_SURF_ATTRIBUTES *psSrcSurfAttrib,
                                      IMG_UINT32 ui32BufferCount,
                                      PVRSRV_SYNC_DATA **ppsSyncData,
                                      IMG_UINT32 ui32OEMFlags,
                                      IMG_HANDLE *phSwapChain,
                                      IMG_UINT32 *pui32SwapChainID)
{
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR DestroyDCSwapChain(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain)
{
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SetDCDstRect(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain,
	IMG_RECT *psRect)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(psRect);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SetDCSrcRect(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_RECT *psRect)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(psRect);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SetDCDstColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(ui32CKColour);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SetDCSrcColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	UNREFERENCED_PARAMETER(hDevice);
	UNREFERENCED_PARAMETER(hSwapChain);
	UNREFERENCED_PARAMETER(ui32CKColour);

	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR GetDCBuffers(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_UINT32 *pui32BufferCount,
                                 IMG_HANDLE *phBuffer)
{
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SwapToDCBuffer(IMG_HANDLE hDevice,
                                   IMG_HANDLE hBuffer,
                                   IMG_UINT32 ui32SwapInterval,
                                   IMG_HANDLE hPrivateTag,
                                   IMG_UINT32 ui32ClipRectCount,
                                   IMG_RECT *psClipRect)
{
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

static PVRSRV_ERROR SwapToDCSystem(IMG_HANDLE hDevice,
                                   IMG_HANDLE hSwapChain)
{
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

#include "mm.h"
int MRSTLFBHandleChangeFB(struct drm_device* dev, struct psb_framebuffer *psbfb)
{
	MRSTLFB_DEVINFO *psDevInfo = GetAnchorPtr();
	int i;
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct psb_gtt * pg = dev_priv->pg;
	LinuxMemArea *psLinuxMemArea;

	if( !psDevInfo->sSystemBuffer.bIsContiguous )
		MRSTLFBFreeKernelMem( psDevInfo->sSystemBuffer.uSysAddr.psNonCont );

	psDevInfo->sDisplayFormat.pixelformat = (psbfb->base.depth == 16) ? PVRSRV_PIXEL_FORMAT_RGB565 : PVRSRV_PIXEL_FORMAT_ARGB8888;

	psDevInfo->sDisplayDim.ui32ByteStride = psbfb->base.pitches[0];
	psDevInfo->sDisplayDim.ui32Width = psbfb->base.width;
	psDevInfo->sDisplayDim.ui32Height = psbfb->base.height;

	psDevInfo->sSystemBuffer.ui32BufferSize = psbfb->size;

	if (psbfb->pvrBO != NULL)
	{
		psDevInfo->sSystemBuffer.sCPUVAddr = psbfb->fbdev->screen_base;
		psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = psbfb->offset;
	}
	else
	{
		psDevInfo->sSystemBuffer.sCPUVAddr = pg->vram_addr;
		psDevInfo->sSystemBuffer.sDevVAddr.uiAddr = 0;
	}

	psDevInfo->sSystemBuffer.bIsAllocated = MRST_FALSE;	

	if (psbfb->pvrBO != NULL)
	{
		psLinuxMemArea = (LinuxMemArea *)psbfb->pvrBO->sMemBlk.hOSMemHandle;
	}

	if ((psbfb->pvrBO == NULL) || (psLinuxMemArea->eAreaType != LINUX_MEM_AREA_ALLOC_PAGES))
	{
		psDevInfo->sSystemBuffer.bIsContiguous = MRST_TRUE;
		psDevInfo->sSystemBuffer.uSysAddr.sCont.uiAddr = pg->stolen_base;
	} else {
		struct page **page_list = psLinuxMemArea->uData.sPageList.pvPageList;

		psDevInfo->sSystemBuffer.bIsContiguous = MRST_FALSE;
		psDevInfo->sSystemBuffer.uSysAddr.psNonCont = MRSTLFBAllocKernelMem( sizeof( IMG_SYS_PHYADDR ) * (psbfb->size >> PAGE_SHIFT));	

		for (i = 0; i < psbfb->size >> PAGE_SHIFT; i++) 
		{
			psDevInfo->sSystemBuffer.uSysAddr.psNonCont[i].uiAddr = page_to_pfn( page_list[i] ) << PAGE_SHIFT;
		}
	} 

	return 0;
}

static MRST_ERROR InitDev(MRSTLFB_DEVINFO *psDevInfo)
{
	MRST_ERROR eError = MRST_ERROR_GENERIC;
	struct fb_info *psLINFBInfo;
	struct drm_device * psDrmDevice = psDevInfo->psDrmDevice;
	struct drm_psb_private * psDrmPrivate = (struct drm_psb_private *)psDrmDevice->dev_private;
	struct psb_fbdev * psPsbFBDev = (struct psb_fbdev *)psDrmPrivate->fbdev;
	struct drm_framebuffer * psDrmFB;
	struct psb_framebuffer *psbfb;

	int i;
	unsigned long FBSize;

	psDrmFB = psPsbFBDev->psb_fb_helper.fb;
	if(!psDrmFB) {
		printk(KERN_INFO"%s:Cannot find drm FB", __FUNCTION__);
		return eError;
	}
	psbfb = to_psb_fb(psDrmFB);

	FBSize = psDrmFB->pitches[0] * psDrmFB->height;

	psLINFBInfo = (struct fb_info*)psPsbFBDev->psb_fb_helper.fbdev;

	psDevInfo->sSystemBuffer.bIsContiguous = MRST_TRUE;
	psDevInfo->sSystemBuffer.bIsAllocated = MRST_FALSE;

	MRSTLFBHandleChangeFB(psDrmDevice, psbfb);

	psDevInfo->psLINFBInfo = psLINFBInfo;

	psDevInfo->ui32MainPipe = 0;

	for(i = 0;i < MAX_SWAPCHAINS;++i) 
	{
		psDevInfo->apsSwapChains[i] = NULL;
	}

	psDevInfo->pvRegs = NULL;

	return MRST_OK;
}

static void DeInitDev(MRSTLFB_DEVINFO *psDevInfo)
{

}

MRST_ERROR MRSTLFBInit(struct drm_device * dev)
{
	MRSTLFB_DEVINFO		*psDevInfo;
	//struct drm_psb_private *psDrmPriv = (struct drm_psb_private *)dev->dev_private;

	psDevInfo = GetAnchorPtr();

	if (psDevInfo == NULL)
	{
		psDevInfo = (MRSTLFB_DEVINFO *)MRSTLFBAllocKernelMem(sizeof(MRSTLFB_DEVINFO));

		if(!psDevInfo)
		{
			return (MRST_ERROR_OUT_OF_MEMORY);
		}


		memset(psDevInfo, 0, sizeof(MRSTLFB_DEVINFO));


		SetAnchorPtr((void*)psDevInfo);

		psDevInfo->psDrmDevice = dev;
		psDevInfo->ulRefCount = 0;


		if(InitDev(psDevInfo) != MRST_OK)
		{
			return (MRST_ERROR_INIT_FAILURE);
		}

		if(MRSTLFBGetLibFuncAddr ("PVRGetDisplayClassJTable", &pfnGetPVRJTable) != MRST_OK)
		{
			return (MRST_ERROR_INIT_FAILURE);
		}


		if(!(*pfnGetPVRJTable)(&psDevInfo->sPVRJTable))
		{
			return (MRST_ERROR_INIT_FAILURE);
		}


		psDevInfo->psCurrentSwapChain = NULL;
		psDevInfo->bFlushCommands = MRST_FALSE;

		psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers = 0;
		psDevInfo->sDisplayInfo.ui32MaxSwapChains = 0;
		psDevInfo->sDisplayInfo.ui32MaxSwapInterval = 0;
		psDevInfo->sDisplayInfo.ui32MinSwapInterval = 0;

		strncpy(psDevInfo->sDisplayInfo.szDisplayName, DISPLAY_DEVICE_NAME, MAX_DISPLAY_NAME_SIZE);

		DEBUG_PRINTK((KERN_INFO DRIVER_PREFIX
			": Maximum number of swap chain buffers: %u\n",
			psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers));

		psDevInfo->sDCJTable.ui32TableSize = sizeof(PVRSRV_DC_SRV2DISP_KMJTABLE);
		psDevInfo->sDCJTable.pfnOpenDCDevice = OpenDCDevice;
		psDevInfo->sDCJTable.pfnCloseDCDevice = CloseDCDevice;
		psDevInfo->sDCJTable.pfnEnumDCFormats = EnumDCFormats;
		psDevInfo->sDCJTable.pfnEnumDCDims = EnumDCDims;
		psDevInfo->sDCJTable.pfnGetDCSystemBuffer = GetDCSystemBuffer;
		psDevInfo->sDCJTable.pfnGetDCInfo = GetDCInfo;
		psDevInfo->sDCJTable.pfnGetBufferAddr = GetDCBufferAddr;
		psDevInfo->sDCJTable.pfnCreateDCSwapChain = CreateDCSwapChain;
		psDevInfo->sDCJTable.pfnDestroyDCSwapChain = DestroyDCSwapChain;
		psDevInfo->sDCJTable.pfnSetDCDstRect = SetDCDstRect;
		psDevInfo->sDCJTable.pfnSetDCSrcRect = SetDCSrcRect;
		psDevInfo->sDCJTable.pfnSetDCDstColourKey = SetDCDstColourKey;
		psDevInfo->sDCJTable.pfnSetDCSrcColourKey = SetDCSrcColourKey;
		psDevInfo->sDCJTable.pfnGetDCBuffers = GetDCBuffers;
		psDevInfo->sDCJTable.pfnSwapToDCBuffer = SwapToDCBuffer;
		psDevInfo->sDCJTable.pfnSwapToDCSystem = SwapToDCSystem;
		psDevInfo->sDCJTable.pfnSetDCState = SetDCState;

		if(psDevInfo->sPVRJTable.pfnPVRSRVRegisterDCDevice (
			&psDevInfo->sDCJTable,
			&psDevInfo->uiDeviceID ) != PVRSRV_OK)
		{
			return (MRST_ERROR_DEVICE_REGISTER_FAILED);
		}

		printk("Device ID: %d\n", (int)psDevInfo->uiDeviceID);
	}

	psDevInfo->ulRefCount++;

	return (MRST_OK);	
}

MRST_ERROR MRSTLFBDeinit(void)
{
	MRSTLFB_DEVINFO *psDevInfo, *psDevFirst;

	psDevFirst = GetAnchorPtr();
	psDevInfo = psDevFirst;

	if (psDevInfo == NULL)
	{
		return (MRST_ERROR_GENERIC);
	}

	psDevInfo->ulRefCount--;

	psDevInfo->psDrmDevice = NULL;
	if (psDevInfo->ulRefCount == 0)
	{
		PVRSRV_DC_DISP2SRV_KMJTABLE	*psJTable = &psDevInfo->sPVRJTable;

		if (psJTable->pfnPVRSRVRemoveDCDevice(psDevInfo->uiDeviceID) != PVRSRV_OK)
		{
			return (MRST_ERROR_GENERIC);
		}

		DeInitDev(psDevInfo);

		MRSTLFBFreeKernelMem(psDevInfo);
	}

	SetAnchorPtr(NULL);

	return (MRST_OK);
}

/****************************************************************************
 *
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#if defined(LMA)
#include <linux/pci.h>
#else
#include <linux/dma-mapping.h>
#endif

#include <drm/drmP.h>
#include <drm/drm.h>
#include "psb_drv.h"
#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_placement.h"
#include "ttm/ttm_object.h"

#include "psb_video_bind.h"
#include "pvrmodule.h"
#include "sys_pvr_drm_export.h"
#include "psb_pvr_glue.h"
#include "psb_ttm_userobj_api.h"

#include "perproc.h"

/*#define PSB_VIDEO_BIND_DEBUG 1 */

static int
set_unset_ttm_st_gfx_buffer(
	struct drm_file *file_priv,
	struct page **pPageList,
	int num_pages,
	int handle,
	int release)
{
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;
	struct ttm_buffer_object *bo = NULL;
	struct ttm_tt *ttm = NULL;
	int r = 0;

	if (!handle) {
		printk(KERN_ERR " : handle is NULL.\n");
		return -EINVAL;
	}

	bo = ttm_buffer_object_lookup(tfile, handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR
			" : Could not find buffer object for setstatus.\n");
		return -EINVAL;
	}

	ttm = bo->ttm;

	if (!release && num_pages != ttm->num_pages) {
		printk(KERN_ERR "%s: invalid number of pages\n", __func__);
		r = -EINVAL;
		goto out;
	}

	if (release)
		drm_psb_unset_fixed_pages(ttm->be);
	else
		r = drm_psb_set_fixed_pages(ttm, pPageList, num_pages);

out:
	if (bo)
		ttm_bo_unref(&bo);
	return r;
}


static int
psb_st_drm_tbe_bind(
	struct psb_mmu_pd *pd,
	unsigned long address,
	struct drm_file *file_priv,
	int hTTMHandle,
	IMG_HANDLE hKernelMemInfo)
{
	int ret = 0;
	uint32_t num_pages;
	struct page **pPageList;
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;
	IMG_HANDLE hOSMemHandle;
	IMG_UINT32 uiAllocSize = 0;
	int eError = 0;

	eError = PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
		(IMG_PVOID *)&psKernelMemInfo,
		hKernelMemInfo,
		PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError == PVRSRV_OK) {
		hOSMemHandle = psKernelMemInfo->sMemBlk.hOSMemHandle;
		uiAllocSize  = psKernelMemInfo->ui32AllocSize;
	} else {
		printk(KERN_ERR "LookUpKernelHandle (0x%x)- FAILED\n",
					(unsigned int)hKernelMemInfo);
		return -EINVAL;
	}

	ret = psb_get_pages_by_mem_handle(hOSMemHandle, &pPageList);
	if (ret) {
		printk(KERN_ERR "%s: get pages error\n", __func__);
		return ret;
	}

	num_pages = (uiAllocSize + PAGE_SIZE - 1) / PAGE_SIZE;

#if defined(PSB_VIDEO_BIND_DEBUG)
{
	int i;

	printk(KERN_ALERT "%s: hOSMemHandle:0x%x pageList:0x%x num_pages:%d",
				__func__, hOSMemHandle, pPageList, num_pages);
	for (i = 0; i < 12; i++) {
		printk(KERN_ALERT "[%d] addr: 0x%x vir:0x%x pfn:0x%x kmap:0x%x",
			i, pPageList[i], page_address(pPageList[i]),
			page_to_pfn(pPageList[i]), kmap(pPageList[i]));
			kunmap(pPageList);
	}
}
#endif

	PSB_DEBUG_GENERAL("%s:insert in MMU address: 0x08%lx num_pages:0x%x\n",
						__func__, address, num_pages);
	ret = psb_mmu_insert_pages(pd, pPageList, address, num_pages, 0, 0, 0);
	if (ret)
		printk(KERN_ERR "%s:Insert Pages failed for gralloc buffer\n",
								__func__);
	ret = set_unset_ttm_st_gfx_buffer(file_priv, pPageList, num_pages,
			hTTMHandle, 0);
	if (ret)
		printk(KERN_ERR "ERORR: set_ttm_st_gfx_buffer failed.\n");
	return ret;
}

static int
psb_st_drm_tbe_unbind(
	struct psb_mmu_pd *pd,
	unsigned long address,
	struct drm_file *file_priv,
	int hTTMHandle,
	IMG_HANDLE hKernelMemInfo)
{
	int ret = 0;
	uint32_t num_pages;
	struct page **pPageList;
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;
	IMG_HANDLE hOSMemHandle;
	IMG_UINT32 uiAllocSize = 0;
	int eError = 0;

	eError = PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
			(IMG_PVOID *)&psKernelMemInfo,
			hKernelMemInfo,
			PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError == PVRSRV_OK) {
		hOSMemHandle = psKernelMemInfo->sMemBlk.hOSMemHandle;
		uiAllocSize  = psKernelMemInfo->ui32AllocSize;
	} else {
		printk(KERN_ERR "LookUpKernelHandle (0x%x)- FAILED\n",
					(unsigned int)hKernelMemInfo);
		return -EINVAL;
	}

	ret = psb_get_pages_by_mem_handle(hOSMemHandle, &pPageList);
	if (ret) {
		printk(KERN_ERR "%s: get pages error\n", __func__);
		return ret;
	}

	num_pages = (uiAllocSize + PAGE_SIZE - 1) / PAGE_SIZE;

	if (set_unset_ttm_st_gfx_buffer(file_priv, NULL, 0, hTTMHandle, 1)) {
		printk(KERN_ERR "%s:Failed to release TTM buffer\n", __func__);
		return -EFAULT;
	}

	PSB_DEBUG_GENERAL("%s: remove in MMU address: 0x08%lx num_pages:0x%x\n",
		__func__, address, num_pages);

	psb_mmu_remove_pages(pd, address, uiAllocSize, 0, 0);

	return ret;
}


int
psb_st_gfx_video_bridge(struct drm_device *dev, IMG_VOID * arg,
			struct drm_file *file_priv)
{
	int err = 0;
	struct PSB_Video_ioctl_package *psBridge =
			(struct PSB_Video_ioctl_package *) arg;
	int command = psBridge->ioctl_cmd;
	PVRSRV_BRIDGE_RETURN sRet;
	struct psb_mmu_pd *pd;
	struct BC_Video_bind_input_t bridge;

	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	pd = psb_mmu_get_default_pd(dev_priv->mmu);

	switch (command) {
	case PSB_Video_ioctl_bind_st_gfx_buffer:
		PSB_DEBUG_GENERAL("[%s]:BC_BIND_GFX - Triggered", __func__);
		if (copy_from_user
			(&bridge, (void __user *) (psBridge->inputparam),
			sizeof(bridge))) {
			printk(KERN_ERR ":Failed to copy inputparam.\n");
			err = -EFAULT;
			goto _exitError;
		}
		if (psb_st_drm_tbe_bind(pd,
			(unsigned long)bridge.pVAddr,
			file_priv,
			bridge.hTTMBuffer,
			bridge.view_ids[0])) {
			printk(KERN_ERR " ERROR : psb_st_drm_tbe_bind failed\n");
			err = -EFAULT;
			goto _exitError;
		}
		return 0;
	case PSB_Video_ioctl_unbind_st_gfx_buffer:
		PSB_DEBUG_GENERAL("[%s]:BC_UNBIND_GFX - Triggered", __func__);
		if (copy_from_user
			(&bridge, (void __user *) (psBridge->inputparam),
			sizeof(bridge))) {
			printk(KERN_ERR "Failed to copy inputparam.\n");
			err = -EFAULT;
			goto _exitError;
		}
		if (psb_st_drm_tbe_unbind(pd, (unsigned long)bridge.pVAddr,
				file_priv,
				bridge.hTTMBuffer,
				bridge.view_ids[0])) {
			printk(KERN_ERR "ERROR : psb_st_drm_tbe_unbind failed\n");
			err = -EFAULT;
			goto _exitError;
		}

		return 0;
	default:
		err = -EFAULT;
		goto _exitError;
		break;
	}

_exitError:
	sRet.eError = err;
	return err;
}
EXPORT_SYMBOL_GPL(psb_st_gfx_video_bridge);

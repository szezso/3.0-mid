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

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include <drm/drmP.h>

#include <asm/io.h>

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "pvrmodule.h"
#include "pvr_drm.h"
#include "mrstlfb.h"
#include "kerneldisplay.h"
#include "psb_irq.h"

#include "psb_drv.h"

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dbi_dpu.h"

#if !defined(SUPPORT_DRI_DRM)
#error "SUPPORT_DRI_DRM must be set"
#endif

#define	MAKESTRING(x) # x

#if !defined(DISPLAY_CONTROLLER)
#define DISPLAY_CONTROLLER pvrlfb
#endif

//#define MAKENAME_HELPER(x, y) x ## y
//#define	MAKENAME2(x, y) MAKENAME_HELPER(x, y)
//#define	MAKENAME(x) MAKENAME2(DISPLAY_CONTROLLER, x)

#define unref__ __attribute__ ((unused))

void *MRSTLFBAllocKernelMem(unsigned long ulSize)
{
	return kmalloc(ulSize, GFP_KERNEL);
}

void MRSTLFBFreeKernelMem(void *pvMem)
{
	kfree(pvMem);
}

MRST_ERROR MRSTLFBGetLibFuncAddr (char *szFunctionName, PFN_DC_GET_PVRJTABLE *ppfnFuncTable)
{
	if(strcmp("PVRGetDisplayClassJTable", szFunctionName) != 0)
	{
		return (MRST_ERROR_INVALID_PARAMS);
	}


	*ppfnFuncTable = PVRGetDisplayClassJTable;

	return (MRST_OK);
}

int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(struct drm_device unref__ *dev)
{
	if(MRSTLFBInit(dev) != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX ": MRSTLFB_Init: MRSTLFBInit failed\n");
		return -ENODEV;
	}

	return 0;
}

void PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(struct drm_device unref__ *dev)
{    
	if(MRSTLFBDeinit() != MRST_OK)
	{
		printk(KERN_WARNING DRIVER_PREFIX "%s: can't deinit device\n", __FUNCTION__);
	}
}

int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Suspend)(struct drm_device unref__ *dev)
{
	return 0;
}

int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Resume)(struct drm_device unref__ *dev)
{
	return 0;
}

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

#if defined(SUPPORT_DRI_DRM)

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/ioctl.h>
#include <drm/drmP.h>
#include <drm/drm.h>


#include "services.h"
#include "kerneldisplay.h"
#include "kernelbuffer.h"
#include "syscommon.h"
#include "pvrmmap.h"
#include "mm.h"
#include "mmap.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "perproc.h"
#include "handle.h"
#include "pvr_bridge_km.h"
#include "pvr_bridge.h"
#include "proc.h"
#include "pvrmodule.h"
#include "pvrversion.h"
#include "lock.h"
#include "linkage.h"
#include "pvr_drm.h"

#define PVR_DRM_NAME	PVRSRV_MODNAME
#define PVR_DRM_DESC	"Imagination Technologies PVR DRM"

#define PVR_PCI_IDS \
	{SYS_SGX_DEV_VENDOR_ID, SYS_SGX_DEV_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, \
	{0, 0, 0}

struct pci_dev *gpsPVRLDMDev;
struct drm_device *gpsPVRDRMDev;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24))
#error "Linux kernel version 2.6.25 or later required for PVR DRM support"
#endif

#define PVR_DRM_FILE struct drm_file *

#if !defined(SUPPORT_DRI_DRM_EXT)
static struct pci_device_id asPciIdList[] = {
	PVR_PCI_IDS
};
#endif

static int PVRSRVDrmLoad(struct drm_device *dev, unsigned long flags)
{
	int iRes;

	PVR_TRACE(("PVRSRVDrmLoad"));

	gpsPVRDRMDev = dev;
	gpsPVRLDMDev = dev->pdev;

#if defined(PDUMP)
	iRes = dbgdrv_init();
	if (iRes != 0) {
		return iRes;
	}
#endif

	iRes = PVRCore_Init();
	if (iRes != 0) {
		goto exit_dbgdrv_cleanup;
	}
#if defined(DISPLAY_CONTROLLER)
	iRes = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init) (dev);
	if (iRes != 0) {
		goto exit_pvrcore_cleanup;
	}
#endif
	return 0;

#if defined(DISPLAY_CONTROLLER)
exit_pvrcore_cleanup:
	PVRCore_Cleanup();
#endif
exit_dbgdrv_cleanup:
#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
	return iRes;
}

static int PVRSRVDrmUnload(struct drm_device *dev)
{
	PVR_TRACE(("PVRSRVDrmUnload"));

#if defined(DISPLAY_CONTROLLER)
	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup) (dev);
#endif

	PVRCore_Cleanup();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif

	return 0;
}

static int PVRSRVDrmOpen(struct drm_device *dev, struct drm_file *file)
{
	return PVRSRVOpen(dev, file);
}

static void PVRSRVDrmPostClose(struct drm_device *dev, struct drm_file *file)
{
	PVRSRVRelease(dev, file);
}

static int
PVRDRMIsMaster(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	return 0;
}

#if defined(SUPPORT_DRI_DRM_EXT)
int
PVRDRM_Dummy_ioctl(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	return 0;
}
#endif

static int
PVRDRMPCIBusIDField(struct drm_device *dev, u32 * pui32Field, u32 ui32FieldType)
{
	struct pci_dev *psPCIDev = (struct pci_dev *)dev->pdev;

	switch (ui32FieldType) {
	case PVR_DRM_PCI_DOMAIN:
		*pui32Field = pci_domain_nr(psPCIDev->bus);
		break;

	case PVR_DRM_PCI_BUS:
		*pui32Field = psPCIDev->bus->number;
		break;

	case PVR_DRM_PCI_DEV:
		*pui32Field = PCI_SLOT(psPCIDev->devfn);
		break;

	case PVR_DRM_PCI_FUNC:
		*pui32Field = PCI_FUNC(psPCIDev->devfn);
		break;

	default:
		return -EFAULT;
	}

	return 0;
}

static int
PVRDRMUnprivCmd(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	u32 *pui32Args = (u32 *) arg;
	u32 ui32Cmd = pui32Args[0];
	u32 ui32Arg1 = pui32Args[1];
	u32 *pui32OutArg = (u32 *) arg;
	s32 ret = 0;

	mutex_lock(&gPVRSRVLock);

	switch (ui32Cmd) {
	case PVR_DRM_UNPRIV_INIT_SUCCESFUL:
		*pui32OutArg =
		    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL) ? 1
		    : 0;
		break;

	case PVR_DRM_UNPRIV_BUSID_TYPE:
		*pui32OutArg = PVR_DRM_BUS_TYPE_PCI;
		break;

	case PVR_DRM_UNPRIV_BUSID_FIELD:
		ret = PVRDRMPCIBusIDField(dev, pui32OutArg, ui32Arg1);

	default:
		ret = -EFAULT;
	}

	mutex_unlock(&gPVRSRVLock);

	return ret;
}

#define	PVR_DRM_FOPS_IOCTL	.unlocked_ioctl

struct drm_ioctl_desc sPVRDrmIoctls[] = {
	DRM_IOCTL_DEF_DRV(PVR_SRVKM, PVRSRV_BridgeDispatchKM,
		      DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_IS_MASTER, PVRDRMIsMaster,
		      DRM_MASTER | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_UNPRIV, PVRDRMUnprivCmd, DRM_UNLOCKED),
#if defined(PDUMP)
	DRM_IOCTL_DEF_DRV(PVR_DBGDRV, dbgdrv_ioctl, DRM_UNLOCKED),
#endif
};

static int pvr_max_ioctl = DRM_ARRAY_SIZE(sPVRDrmIoctls);

static struct drm_driver sPVRDrmDriver = {
	.driver_features = 0,
	.dev_priv_size = 0,
	.load = PVRSRVDrmLoad,
	.unload = PVRSRVDrmUnload,
	.open = PVRSRVDrmOpen,
	.postclose = PVRSRVDrmPostClose,
	.suspend = PVRSRVDriverSuspend,
	.resume = PVRSRVDriverResume,
	.ioctls = sPVRDrmIoctls,
	.fops = {
		 .owner = THIS_MODULE,
		 .open = drm_open,
		 .release = drm_release,
		 PVR_DRM_FOPS_IOCTL = drm_ioctl,
		 .mmap = PVRMMap,
		 .poll = drm_poll,
		 .fasync = drm_fasync,
		 },
	.name = PVR_DRM_NAME,
	.desc = PVR_DRM_DESC,
	.date = PVR_BUILD_DATE,
	.major = PVRVERSION_MAJ,
	.minor = PVRVERSION_MIN,
	.patchlevel = PVRVERSION_BUILD,
};

static struct pci_driver pci_driver = {
       .name = PVR_DRM_NAME,
       .id_table = asPciIdList,
};

static int __init PVRSRVDrmInit(void)
{
	int iRes;
	sPVRDrmDriver.num_ioctls = pvr_max_ioctl;

	PVRDPFInit();

	iRes = drm_pci_init(&sPVRDrmDriver, &pci_driver);

	return iRes;
}

static void __exit PVRSRVDrmExit(void)
{
	drm_pci_exit(&sPVRDrmDriver, &pci_driver);
}

module_init(PVRSRVDrmInit);
module_exit(PVRSRVDrmExit);

#endif

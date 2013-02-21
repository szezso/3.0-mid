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

#if !defined(SUPPORT_DRI_DRM)

#if defined(LDM_PLATFORM)
#define	PVR_LDM_PLATFORM_MODULE
#define	PVR_LDM_MODULE
#else
#if defined(LDM_PCI)
#define PVR_LDM_PCI_MODULE
#define	PVR_LDM_MODULE
#endif
#endif
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>

#if defined(SUPPORT_DRI_DRM)
#include <drm/drmP.h>
#if defined(PVR_SECURE_DRM_AUTH_EXPORT)
#include "env_perproc.h"
#endif
#endif

#if defined(PVR_LDM_PLATFORM_MODULE)
#include <linux/platform_device.h>
#endif

#if defined(PVR_LDM_PCI_MODULE)
#include <linux/pci.h>
#endif

#if defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL)
#include <asm/uaccess.h>
#endif


#include "services.h"
#include "kerneldisplay.h"
#include "kernelbuffer.h"
#include "syscommon.h"
#include "pvrmmap.h"
#include "mutils.h"
#include "mm.h"
#include "mmap.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "perproc.h"
#include "handle.h"
#include "pvr_bridge_km.h"
#include "proc.h"
#include "pvrmodule.h"
#include "private_data.h"
#include "lock.h"
#include "linkage.h"

#if defined(SUPPORT_DRI_DRM)
#include "pvr_drm.h"
#endif
#define DRVNAME		"pvrsrvkm"
#define DEVNAME		"pvrsrvkm"

#if defined(SUPPORT_DRI_DRM)
#define PRIVATE_DATA(pFile) ((pFile)->driver_priv)
#else
#define PRIVATE_DATA(pFile) ((pFile)->private_data)
#endif

MODULE_SUPPORTED_DEVICE(DEVNAME);
#ifdef DEBUG
static int debug = DBGPRIV_WARNING;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <linux/moduleparam.h>
module_param(debug, int, 0);
#else
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Sets the level of debug output (default=0x4)");
#endif
#endif

extern int PVRGetDisplayClassJTable(PVRSRV_DC_DISP2SRV_KMJTABLE * psJTable);
extern int PVRGetBufferClassJTable(PVRSRV_BC_BUFFER2SRV_KMJTABLE * psJTable);

/*EXPORT_SYMBOL(PVRGetDisplayClassJTable); */
/*EXPORT_SYMBOL(PVRGetBufferClassJTable); */

#if defined(PVR_LDM_MODULE)
static struct class *psPvrClass;
#endif

#if !defined(SUPPORT_DRI_DRM)
static int AssignedMajorNumber;

static int PVRSRVOpen(struct inode *pInode, struct file *pFile);
static int PVRSRVRelease(struct inode *pInode, struct file *pFile);

static struct file_operations pvrsrv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = PVRSRV_BridgeDispatchKM,
	.open = PVRSRVOpen,
	.release = PVRSRVRelease,
	.mmap = PVRMMap,
};
#endif

struct mutex gPVRSRVLock;

u32 gui32ReleasePID;

#if defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL)
static u32 gPVRPowerLevel;
#endif

#if defined(PVR_LDM_MODULE)

#if defined(PVR_LDM_PLATFORM_MODULE)
#define	LDM_DEV	struct platform_device
#define	LDM_DRV	struct platform_driver
#endif

#if defined(PVR_LDM_PCI_MODULE)
#define	LDM_DEV	struct pci_dev
#define	LDM_DRV	struct pci_driver
#endif

#if defined(PVR_LDM_PLATFORM_MODULE)
static int PVRSRVDriverRemove(LDM_DEV * device);
static int PVRSRVDriverProbe(LDM_DEV * device);
#endif
#if defined(PVR_LDM_PCI_MODULE)
static void PVRSRVDriverRemove(LDM_DEV * device);
static int PVRSRVDriverProbe(LDM_DEV * device, const struct pci_device_id *id);
#endif
static int PVRSRVDriverSuspend(LDM_DEV * device, pm_message_t state);
static void PVRSRVDriverShutdown(LDM_DEV * device);
static int PVRSRVDriverResume(LDM_DEV * device);

#if defined(PVR_LDM_PCI_MODULE)
struct pci_device_id powervr_id_table[] __devinitdata = {
	{PCI_DEVICE(SYS_SGX_DEV_VENDOR_ID, SYS_SGX_DEV_DEVICE_ID)},
	{0}
};

MODULE_DEVICE_TABLE(pci, powervr_id_table);
#endif

static LDM_DRV powervr_driver = {
#if defined(PVR_LDM_PLATFORM_MODULE)
	.driver = {
		   .name = DRVNAME,
		   },
#endif
#if defined(PVR_LDM_PCI_MODULE)
	.name = DRVNAME,
	.id_table = powervr_id_table,
#endif
	.probe = PVRSRVDriverProbe,
#if defined(PVR_LDM_PLATFORM_MODULE)
	.remove = PVRSRVDriverRemove,
#endif
#if defined(PVR_LDM_PCI_MODULE)
	.remove = __devexit_p(PVRSRVDriverRemove),
#endif
	.suspend = PVRSRVDriverSuspend,
	.resume = PVRSRVDriverResume,
	.shutdown = PVRSRVDriverShutdown,
};

LDM_DEV *gpsPVRLDMDev;

#if defined(MODULE) && defined(PVR_LDM_PLATFORM_MODULE)

static void PVRSRVDeviceRelease(struct device *pDevice)
{
}

static struct platform_device powervr_device = {
	.name = DEVNAME,
	.id = -1,
	.dev = {
		.release = PVRSRVDeviceRelease}
};

#endif

#if defined(PVR_LDM_PLATFORM_MODULE)
static int PVRSRVDriverProbe(LDM_DEV * pDevice)
#endif
#if defined(PVR_LDM_PCI_MODULE)
static int __devinit PVRSRVDriverProbe(LDM_DEV * pDevice,
				       const struct pci_device_id *id)
#endif
{
	SYS_DATA *psSysData;

	PVR_TRACE(("PVRSRVDriverProbe(pDevice=%p)", pDevice));

#if 0

	if (PerDeviceSysInitialise((void *)pDevice) != PVRSRV_OK) {
		return -EINVAL;
	}
#endif

	if (SysAcquireData(&psSysData) != PVRSRV_OK) {
		gpsPVRLDMDev = pDevice;

		if (SysInitialise() != PVRSRV_OK) {
			return -ENODEV;
		}
	}

	return 0;
}

#if defined (PVR_LDM_PLATFORM_MODULE)
static int PVRSRVDriverRemove(LDM_DEV * pDevice)
#endif
#if defined(PVR_LDM_PCI_MODULE)
static void __devexit PVRSRVDriverRemove(LDM_DEV * pDevice)
#endif
{
	SYS_DATA *psSysData;

	PVR_TRACE(("PVRSRVDriverRemove(pDevice=%p)", pDevice));

	if (SysAcquireData(&psSysData) == PVRSRV_OK) {
#if defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL)
		if (gPVRPowerLevel != 0) {
			if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D0) ==
			    PVRSRV_OK) {
				gPVRPowerLevel = 0;
			}
		}
#endif
		SysDeinitialise(psSysData);

		gpsPVRLDMDev = NULL;
	}
#if 0
	if (PerDeviceSysDeInitialise((void *)pDevice) != PVRSRV_OK) {
		return -EINVAL;
	}
#endif

#if defined (PVR_LDM_PLATFORM_MODULE)
	return 0;
#endif
#if defined (PVR_LDM_PCI_MODULE)
	return;
#endif
}

static void PVRSRVDriverShutdown(LDM_DEV * pDevice)
{
	PVR_TRACE(("PVRSRVDriverShutdown(pDevice=%p)", pDevice));

	(void)PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D3);
}

#endif

#if defined(PVR_LDM_MODULE) || defined(SUPPORT_DRI_DRM)
#if defined(SUPPORT_DRI_DRM)
int PVRSRVDriverSuspend(struct drm_device *pDevice, pm_message_t state)
#else
static int PVRSRVDriverSuspend(LDM_DEV * pDevice, pm_message_t state)
#endif
{
#if !(defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL) && !defined(SUPPORT_DRI_DRM))
	PVR_TRACE(("PVRSRVDriverSuspend(pDevice=%p)", pDevice));

	if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D3) != PVRSRV_OK) {
		return -EINVAL;
	}
#endif
	return 0;
}

#if defined(SUPPORT_DRI_DRM)
int PVRSRVDriverResume(struct drm_device *pDevice)
#else
static int PVRSRVDriverResume(LDM_DEV * pDevice)
#endif
{
#if !(defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL) && !defined(SUPPORT_DRI_DRM))
	PVR_TRACE(("PVRSRVDriverResume(pDevice=%p)", pDevice));

	if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D0) != PVRSRV_OK) {
		return -EINVAL;
	}
#endif
	return 0;
}
#endif

#if defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL) && !defined(SUPPORT_DRI_DRM)
int PVRProcSetPowerLevel(struct file *file, const char *buffer, u32 count,
			 void *data)
{
	char data_buffer[2];
	u32 PVRPowerLevel;

	if (count != sizeof(data_buffer)) {
		return -EINVAL;
	} else {
		if (copy_from_user(data_buffer, buffer, count))
			return -EINVAL;
		if (data_buffer[count - 1] != '\n')
			return -EINVAL;
		PVRPowerLevel = data_buffer[0] - '0';
		if (PVRPowerLevel != gPVRPowerLevel) {
			if (PVRPowerLevel != 0) {
				if (PVRSRVSetPowerStateKM
				    (PVRSRV_SYS_POWER_STATE_D3) != PVRSRV_OK) {
					return -EINVAL;
				}
			} else {
				if (PVRSRVSetPowerStateKM
				    (PVRSRV_SYS_POWER_STATE_D0) != PVRSRV_OK) {
					return -EINVAL;
				}
			}

			gPVRPowerLevel = PVRPowerLevel;
		}
	}
	return (count);
}

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowPowerLevel(struct seq_file *sfile, void *el)
{
	seq_printf(sfile, "%lu\n", gPVRPowerLevel);
}

#else
int PVRProcGetPowerLevel(char *page, char **start, off_t off, int count,
			 int *eof, void *data)
{
	if (off == 0) {
		*start = (char *)1;
		return printAppend(page, count, 0, "%lu\n", gPVRPowerLevel);
	}
	*eof = 1;
	return 0;
}
#endif

#endif

#if defined(SUPPORT_DRI_DRM)
int PVRSRVOpen(struct drm_device  * dev, struct drm_file *pFile)
#else
static int PVRSRVOpen(struct inode unref__ * pInode, struct file *pFile)
#endif
{
	PVRSRV_FILE_PRIVATE_DATA *psPrivateData;
	void *hBlockAlloc;
	int iRet = -ENOMEM;
	PVRSRV_ERROR eError;
	u32 ui32PID;
#if defined(SUPPORT_DRI_DRM) && defined(PVR_SECURE_DRM_AUTH_EXPORT)
	PVRSRV_ENV_PER_PROCESS_DATA *psEnvPerProc;
#endif

#if defined(SUPPORT_DRI_DRM)

#else

#endif

	mutex_lock(&gPVRSRVLock);

	ui32PID = OSGetCurrentProcessIDKM();

	if (PVRSRVProcessConnect(ui32PID) != PVRSRV_OK)
		goto err_unlock;

#if defined(SUPPORT_DRI_DRM) && defined(PVR_SECURE_DRM_AUTH_EXPORT)
	psEnvPerProc = PVRSRVPerProcessPrivateData(ui32PID);
	if (psEnvPerProc == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "%s: No per-process private data",
			 __FUNCTION__));
		goto err_unlock;
	}
#endif

	eError = OSAllocMem(PVRSRV_OS_NON_PAGEABLE_HEAP,
			    sizeof(PVRSRV_FILE_PRIVATE_DATA),
			    (void **)&psPrivateData,
			    &hBlockAlloc, "File Private Data");

	if (eError != PVRSRV_OK)
		goto err_unlock;

#if defined(PVR_SECURE_FD_EXPORT)
	psPrivateData->hKernelMemInfo = NULL;
#endif
#if defined(SUPPORT_DRI_DRM) && defined(PVR_SECURE_DRM_AUTH_EXPORT)
	psPrivateData->psDRMFile = pFile;

	list_add_tail(&psPrivateData->sDRMAuthListItem,
		      &psEnvPerProc->sDRMAuthListHead);
#endif
	psPrivateData->ui32OpenPID = ui32PID;
	psPrivateData->hBlockAlloc = hBlockAlloc;
	PRIVATE_DATA(pFile) = psPrivateData;
	iRet = 0;
err_unlock:
	mutex_unlock(&gPVRSRVLock);
	return iRet;
}

#if defined(SUPPORT_DRI_DRM)
int PVRSRVRelease(struct drm_device * dev, struct drm_file *pFile)
#else
static int PVRSRVRelease(struct inode unref__ * pInode, struct file *pFile)
#endif
{
	PVRSRV_FILE_PRIVATE_DATA *psPrivateData;

#if defined(SUPPORT_DRI_DRM)
#else
#endif

	mutex_lock(&gPVRSRVLock);

	psPrivateData = PRIVATE_DATA(pFile);

#if defined(SUPPORT_DRI_DRM) && defined(PVR_SECURE_DRM_AUTH_EXPORT)
	list_del(&psPrivateData->sDRMAuthListItem);
#endif

	gui32ReleasePID = psPrivateData->ui32OpenPID;
	PVRSRVProcessDisconnect(psPrivateData->ui32OpenPID);
	gui32ReleasePID = 0;

	OSFreeMem(PVRSRV_OS_NON_PAGEABLE_HEAP,
		  sizeof(PVRSRV_FILE_PRIVATE_DATA),
		  psPrivateData, psPrivateData->hBlockAlloc);
	PRIVATE_DATA(pFile) = NULL;

	mutex_unlock(&gPVRSRVLock);
	return 0;
}

#if defined(SUPPORT_DRI_DRM)
int PVRCore_Init(void)
#else
static int __init PVRCore_Init(void)
#endif
{
	int error;
#if !defined(PVR_LDM_MODULE)
	PVRSRV_ERROR eError;
#else
	struct device *psDev;
#endif

#if !defined(SUPPORT_DRI_DRM)

	PVRDPFInit();
#endif
	PVR_TRACE(("PVRCore_Init"));

	mutex_init(&gPVRSRVLock);

#ifdef DEBUG
	PVRDebugSetLevel(debug);
#endif

	if (CreateProcEntries()) {
		error = -ENOMEM;
		return error;
	}

	if (PVROSFuncInit() != PVRSRV_OK) {
		error = -ENOMEM;
		goto init_failed;
	}

	PVRLinuxMUtilsInit();

	if (LinuxMMInit() != PVRSRV_OK) {
		error = -ENOMEM;
		goto init_failed;
	}

	LinuxBridgeInit();

	PVRMMapInit();

#if defined(PVR_LDM_MODULE)

#if defined(PVR_LDM_PLATFORM_MODULE)
	if ((error = platform_driver_register(&powervr_driver)) != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to register platform driver (%d)",
			 error));

		goto init_failed;
	}
#if defined(MODULE)
	if ((error = platform_device_register(&powervr_device)) != 0) {
		platform_driver_unregister(&powervr_driver);

		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to register platform device (%d)",
			 error));

		goto init_failed;
	}
#endif
#endif

#if defined(PVR_LDM_PCI_MODULE)
	if ((error = pci_register_driver(&powervr_driver)) != 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to register PCI driver (%d)",
			 error));

		goto init_failed;
	}
#endif

#else

	if ((eError = SysInitialise()) != PVRSRV_OK) {
		error = -ENODEV;
#if defined(TCF_REV) && (TCF_REV == 110)
		if (eError == PVRSRV_ERROR_NOT_SUPPORTED) {
			printk("\nAtlas wrapper (FPGA image) version mismatch");
			error = -ENODEV;
		}
#endif
		goto init_failed;
	}
#endif

#if !defined(SUPPORT_DRI_DRM)
	AssignedMajorNumber = register_chrdev(0, DEVNAME, &pvrsrv_fops);

	if (AssignedMajorNumber <= 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to get major number"));

		error = -EBUSY;
		goto sys_deinit;
	}

	PVR_TRACE(("PVRCore_Init: major device %d", AssignedMajorNumber));
#endif

#if defined(PVR_LDM_MODULE)

	psPvrClass = class_create(THIS_MODULE, "pvr");

	if (IS_ERR(psPvrClass)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to create class (%ld)",
			 PTR_ERR(psPvrClass)));
		error = -EBUSY;
		goto unregister_device;
	}

	psDev = device_create(psPvrClass, NULL, MKDEV(AssignedMajorNumber, 0),
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26))
			      NULL,
#endif
			      DEVNAME);
	if (IS_ERR(psDev)) {
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRCore_Init: unable to create device (%ld)",
			 PTR_ERR(psDev)));
		error = -EBUSY;
		goto destroy_class;
	}
#endif

	return 0;

#if defined(PVR_LDM_MODULE)
destroy_class:
	class_destroy(psPvrClass);
unregister_device:
	unregister_chrdev((u32) AssignedMajorNumber, DRVNAME);
#endif
#if !defined(SUPPORT_DRI_DRM)
sys_deinit:
#endif
#if defined(PVR_LDM_MODULE)
#if defined(PVR_LDM_PCI_MODULE)
	pci_unregister_driver(&powervr_driver);
#endif

#if defined (PVR_LDM_PLATFORM_MODULE)
#if defined (MODULE)
	platform_device_unregister(&powervr_device);
#endif
	platform_driver_unregister(&powervr_driver);
#endif

#else

	{
		SYS_DATA *psSysData;

		SysAcquireData(&psSysData);
		if (psSysData != NULL) {
			SysDeinitialise(psSysData);
		}
	}
#endif
init_failed:
	PVRMMapCleanup();
	LinuxMMCleanup();
	LinuxBridgeDeInit();
	PVROSFuncDeInit();
	RemoveProcEntries();

	return error;

}

#if defined(SUPPORT_DRI_DRM)
void PVRCore_Cleanup(void)
#else
static void __exit PVRCore_Cleanup(void)
#endif
{
	SYS_DATA *psSysData;

	PVR_TRACE(("PVRCore_Cleanup"));

	SysAcquireData(&psSysData);

#if defined(PVR_LDM_MODULE)
	device_destroy(psPvrClass, MKDEV(AssignedMajorNumber, 0));
	class_destroy(psPvrClass);
#endif

#if !defined(SUPPORT_DRI_DRM)
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	if (
#endif
		   unregister_chrdev((u32) AssignedMajorNumber, DRVNAME)
#if !(LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
		   ;
#else
	    ) {
		PVR_DPF((PVR_DBG_ERROR, " can't unregister device major %d",
			 AssignedMajorNumber));
	}
#endif
#endif

#if defined(PVR_LDM_MODULE)

#if defined(PVR_LDM_PCI_MODULE)
	pci_unregister_driver(&powervr_driver);
#endif

#if defined (PVR_LDM_PLATFORM_MODULE)
#if defined (MODULE)
	platform_device_unregister(&powervr_device);
#endif
	platform_driver_unregister(&powervr_driver);
#endif

#else
#if defined(DEBUG) && defined(PVR_MANUAL_POWER_CONTROL)
	if (gPVRPowerLevel != 0) {
		if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D0) ==
		    PVRSRV_OK) {
			gPVRPowerLevel = 0;
		}
	}
#endif

	SysDeinitialise(psSysData);
#endif

	PVRMMapCleanup();

	LinuxMMCleanup();

	LinuxBridgeDeInit();

	PVROSFuncDeInit();

	RemoveProcEntries();

	PVR_TRACE(("PVRCore_Cleanup: unloading"));
}

#if !defined(SUPPORT_DRI_DRM)
module_init(PVRCore_Init);
module_exit(PVRCore_Cleanup);
#endif

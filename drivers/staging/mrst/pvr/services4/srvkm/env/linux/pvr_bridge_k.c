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

#include "img_defs.h"
#include "services.h"
#include "pvr_bridge.h"
#include "perproc.h"
#include "mutex.h"
#include "syscommon.h"
#include "pvr_debug.h"
#include "proc.h"
#include "private_data.h"
#include "linkage.h"
#include "pvr_bridge_km.h"
#include "sgx_bridge_km.h"
#include "sgx_options.h"

#if defined(SUPPORT_DRI_DRM)
#include <drm/drmP.h>
#include "pvr_drm.h"
#if defined(PVR_SECURE_DRM_AUTH_EXPORT)
#include "env_perproc.h"
#endif
#endif

#if defined(SUPPORT_VGX)
#include "vgx_bridge.h"
#endif

#if defined(SUPPORT_SGX)
#include "sgx_bridge.h"
#endif

#include "bridged_pvr_bridge.h"

#ifdef MODULE_TEST
#include "pvr_test_bridge.h"
#include "kern_test.h"
#endif


#if defined(SUPPORT_DRI_DRM)
#define	PRIVATE_DATA(pFile) ((pFile)->driver_priv)
#else
#define	PRIVATE_DATA(pFile) ((pFile)->private_data)
#endif

#if defined(DEBUG_BRIDGE_KM)

static struct proc_dir_entry *g_ProcBridgeStats =0;
static void* ProcSeqNextBridgeStats(struct seq_file *sfile,void* el,loff_t off);
static void ProcSeqShowBridgeStats(struct seq_file *sfile,void* el);
static void* ProcSeqOff2ElementBridgeStats(struct seq_file * sfile, loff_t off);
static void ProcSeqStartstopBridgeStats(struct seq_file *sfile,IMG_BOOL start);

#endif

extern PVRSRV_LINUX_MUTEX gPVRSRVLock;

#if defined(SUPPORT_MEMINFO_IDS)
static IMG_UINT64 ui64Stamp;
#endif 

PVRSRV_ERROR
LinuxBridgeInit(IMG_VOID)
{
#if defined(DEBUG_BRIDGE_KM)
	{
		g_ProcBridgeStats = CreateProcReadEntrySeq(
												  "bridge_stats", 
												  NULL,
												  ProcSeqNextBridgeStats,
												  ProcSeqShowBridgeStats,
												  ProcSeqOff2ElementBridgeStats,
												  ProcSeqStartstopBridgeStats
						  						 );
		if(!g_ProcBridgeStats)
		{
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}
#endif
	return CommonBridgeInit();
}

IMG_VOID
LinuxBridgeDeInit(IMG_VOID)
{
#if defined(DEBUG_BRIDGE_KM)
    RemoveProcEntrySeq(g_ProcBridgeStats);
#endif
}

#if defined(DEBUG_BRIDGE_KM)

static void ProcSeqStartstopBridgeStats(struct seq_file *sfile,IMG_BOOL start) 
{
	if(start) 
	{
		LinuxLockMutex(&gPVRSRVLock);
	}
	else
	{
		LinuxUnLockMutex(&gPVRSRVLock);
	}
}


static void* ProcSeqOff2ElementBridgeStats(struct seq_file *sfile, loff_t off)
{
	if(!off) 
	{
		return PVR_PROC_SEQ_START_TOKEN;
	}

	if(off > BRIDGE_DISPATCH_TABLE_ENTRY_COUNT)
	{
		return (void*)0;
	}


	return (void*)&g_BridgeDispatchTable[off-1];
}

static void* ProcSeqNextBridgeStats(struct seq_file *sfile,void* el,loff_t off)
{
	return ProcSeqOff2ElementBridgeStats(sfile,off);
}


static void ProcSeqShowBridgeStats(struct seq_file *sfile,void* el)
{
	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psEntry = (	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY*)el;

	if(el == PVR_PROC_SEQ_START_TOKEN) 
	{
		seq_printf(sfile,
						  "Total ioctl call count = %u\n"
						  "Total number of bytes copied via copy_from_user = %u\n"
						  "Total number of bytes copied via copy_to_user = %u\n"
						  "Total number of bytes copied via copy_*_user = %u\n\n"
						  "%-45s | %-40s | %10s | %20s | %10s\n",
						  g_BridgeGlobalStats.ui32IOCTLCount,
						  g_BridgeGlobalStats.ui32TotalCopyFromUserBytes,
						  g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
						  g_BridgeGlobalStats.ui32TotalCopyFromUserBytes+g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
						  "Bridge Name",
						  "Wrapper Function",
						  "Call Count",
						  "copy_from_user Bytes",
						  "copy_to_user Bytes"
						 );
		return;
	}

	seq_printf(sfile,
				   "%-45s   %-40s   %-10u   %-20u   %-10u\n",
				   psEntry->pszIOCName,
				   psEntry->pszFunctionName,
				   psEntry->ui32CallCount,
				   psEntry->ui32CopyFromUserTotalBytes,
				   psEntry->ui32CopyToUserTotalBytes);
}

#endif 

static inline int support_secure_fd_export(void)
{
	unsigned long mask;

	PVRSRV_SGXDEV_INFO *dev_info = pvr_get_sgx_dev_info();

	if (!dev_info)
		return 0;

	mask = 1 << PVR_DISABLE_SECURE_FD_EXPORT_OFFSET;

	return !(dev_info->ui32ClientBuildOptions & mask);
}

static int secure_fd_export_prepare(int cmd, void *param_in, void *sec_meminfo)
{
	if (!support_secure_fd_export())
		return 0;

	switch (cmd) {
	case PVRSRV_BRIDGE_EXPORT_DEVICEMEM_2:
		if (sec_meminfo) {
			pr_err("pvr: %s: can only export one MemInfo "
					 "per file descriptor", __func__);
			return -EINVAL;
		}
		break;

	case PVRSRV_BRIDGE_MAP_DEV_MEMORY_2:
	{
		PVRSRV_BRIDGE_IN_MAP_DEV_MEMORY *map_devmem_in = param_in;

		if (!sec_meminfo) {
			pr_err("pvr: %s: file descriptor has no "
					 "associated MemInfo handle", __func__);
			return -EINVAL;
		}
		map_devmem_in->hKernelMemInfo = sec_meminfo;
		break;
	}

	default:
		if (sec_meminfo) {
			pr_err("pvr: %s: import/export handle tried "
					 "to use privileged service", __func__);
			return -EINVAL;
		}
		break;
	}

	return 0;
}

static void secure_fd_export_finish(int cmd, void *param_out,
				void **sec_mem_info, u64 *time_stamp)
{
	PVRSRV_BRIDGE_OUT_EXPORTDEVICEMEM *map_devmem_out = param_out;

	if (!support_secure_fd_export())
		return;

	if (cmd != PVRSRV_BRIDGE_EXPORT_DEVICEMEM_2)
		return;

	*sec_mem_info = map_devmem_out->hMemInfo;

	ui64Stamp++;
	*time_stamp = ui64Stamp;
	map_devmem_out->ui64Stamp = ui64Stamp;
}


#if defined(SUPPORT_DRI_DRM)
IMG_INT
PVRSRV_BridgeDispatchKM(struct drm_device unref__ *dev, IMG_VOID *arg, struct drm_file *pFile)
#else
IMG_INT32
PVRSRV_BridgeDispatchKM(struct file *pFile, IMG_UINT unref__ ioctlCmd, IMG_UINT32 arg)
#endif
{
	IMG_UINT32 cmd;
#if !defined(SUPPORT_DRI_DRM)
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageUM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVRSRV_BRIDGE_PACKAGE sBridgePackageKM;
#endif
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM;
	IMG_UINT32 ui32PID = OSGetCurrentProcessIDKM();
	PVRSRV_PER_PROCESS_DATA *psPerProc;
	PVRSRV_FILE_PRIVATE_DATA *priv_data = PRIVATE_DATA(pFile);
	IMG_INT err = -EFAULT;

	LinuxLockMutex(&gPVRSRVLock);

#if defined(SUPPORT_DRI_DRM)
	psBridgePackageKM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVR_ASSERT(psBridgePackageKM != IMG_NULL);
#else
	psBridgePackageKM = &sBridgePackageKM;

	if(!OSAccessOK(PVR_VERIFY_WRITE,
				   psBridgePackageUM,
				   sizeof(PVRSRV_BRIDGE_PACKAGE)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Received invalid pointer to function arguments",
				 __FUNCTION__));

		goto unlock_and_return;
	}
	
	
	if(OSCopyFromUser(IMG_NULL,
					  psBridgePackageKM,
					  psBridgePackageUM,
					  sizeof(PVRSRV_BRIDGE_PACKAGE))
	  != PVRSRV_OK)
	{
		goto unlock_and_return;
	}
#endif

	cmd = psBridgePackageKM->ui32BridgeID;

#if defined(MODULE_TEST)
	switch (cmd)
	{
		case PVRSRV_BRIDGE_SERVICES_TEST_MEM1:
			{
				PVRSRV_ERROR eError = MemTest1();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;
		case PVRSRV_BRIDGE_SERVICES_TEST_MEM2:
			{
				PVRSRV_ERROR eError = MemTest2();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_RESOURCE:
			{
				PVRSRV_ERROR eError = ResourceTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_EVENTOBJECT:
			{
				PVRSRV_ERROR eError = EventObjectTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_MEMMAPPING:
			{
				PVRSRV_ERROR eError = MemMappingTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_PROCESSID:
			{
				PVRSRV_ERROR eError = ProcessIDTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_CLOCKUSWAITUS:
			{
				PVRSRV_ERROR eError = ClockusWaitusTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_TIMER:
			{
				PVRSRV_ERROR eError = TimerTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

		case PVRSRV_BRIDGE_SERVICES_TEST_PRIVSRV:
			{
				PVRSRV_ERROR eError = PrivSrvTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;
		case PVRSRV_BRIDGE_SERVICES_TEST_COPYDATA:
		{
			IMG_UINT32               ui32PID;
			PVRSRV_PER_PROCESS_DATA *psPerProc;
			PVRSRV_ERROR eError;
			
			ui32PID = OSGetCurrentProcessIDKM();
		
			PVRSRVTrace("PVRSRV_BRIDGE_SERVICES_TEST_COPYDATA %d", ui32PID);
			
			psPerProc = PVRSRVPerProcessData(ui32PID);
						
			eError = CopyDataTest(psBridgePackageKM->pvParamIn, psBridgePackageKM->pvParamOut, psPerProc);
			
			*(PVRSRV_ERROR*)psBridgePackageKM->pvParamOut = eError;
			err = 0;
			goto unlock_and_return;
		}


		case PVRSRV_BRIDGE_SERVICES_TEST_POWERMGMT:
    			{
				PVRSRV_ERROR eError = PowerMgmtTest();
				if (psBridgePackageKM->ui32OutBufferSize == sizeof(PVRSRV_BRIDGE_RETURN))
				{
					PVRSRV_BRIDGE_RETURN* pReturn = (PVRSRV_BRIDGE_RETURN*)psBridgePackageKM->pvParamOut ;
					pReturn->eError = eError;
				}
			}
			err = 0;
			goto unlock_and_return;

	}
#endif
	
	if(cmd != PVRSRV_BRIDGE_CONNECT_SERVICES)
	{
		PVRSRV_ERROR eError;

		eError = PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
									(IMG_PVOID *)&psPerProc,
									psBridgePackageKM->hKernelServices,
									PVRSRV_HANDLE_TYPE_PERPROC_DATA);
		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Invalid kernel services handle (%d)",
					 __FUNCTION__, eError));
			goto unlock_and_return;
		}

		if(psPerProc->ui32PID != ui32PID)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Process %d tried to access data "
					 "belonging to process %d", __FUNCTION__, ui32PID,
					 psPerProc->ui32PID));
			goto unlock_and_return;
		}
	}
	else
	{
		
		psPerProc = PVRSRVPerProcessData(ui32PID);
		if(psPerProc == IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRV_BridgeDispatchKM: "
					 "Couldn't create per-process data area"));
			goto unlock_and_return;
		}
	}

	psBridgePackageKM->ui32BridgeID = PVRSRV_GET_BRIDGE_ID(psBridgePackageKM->ui32BridgeID);

	/*
	 * FIXME: remove the following workaround, once all user space libraries
	 * have a proper fix for this security issue.
	 */
	err = secure_fd_export_prepare(cmd, psBridgePackageKM->pvParamIn,
				     priv_data->sec_fd_exp_meminfo);
	if (err < 0)
		goto unlock_and_return;

#if defined(SUPPORT_DRI_DRM) && defined(PVR_SECURE_DRM_AUTH_EXPORT)
	switch(cmd)
	{
		case PVRSRV_BRIDGE_MAP_DEV_MEMORY:
		case PVRSRV_BRIDGE_MAP_DEVICECLASS_MEMORY:
		{
			PVRSRV_FILE_PRIVATE_DATA *psPrivateData;
			IMG_INT authenticated = pFile->authenticated;
			PVRSRV_ENV_PER_PROCESS_DATA *psEnvPerProc;

			if (authenticated)
			{
				break;
			}

			
			psEnvPerProc = (PVRSRV_ENV_PER_PROCESS_DATA *)PVRSRVProcessPrivateData(psPerProc);
			if (psEnvPerProc == IMG_NULL)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Process private data not allocated", __FUNCTION__));
				err = -EFAULT;
				goto unlock_and_return;
			}

			list_for_each_entry(psPrivateData, &psEnvPerProc->sDRMAuthListHead, sDRMAuthListItem)
			{
				struct drm_file *psDRMFile = psPrivateData->psDRMFile;

				if (pFile->master == psDRMFile->master)
				{
					authenticated |= psDRMFile->authenticated;
					if (authenticated)
					{
						break;
					}
				}
			}

			if (!authenticated)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Not authenticated for mapping device or device class memory", __FUNCTION__));
				err = -EPERM;
				goto unlock_and_return;
			}
			break;
		}
		default:
			break;
	}
#endif 

	err = BridgedDispatchKM(psPerProc, psBridgePackageKM);
	if(err != PVRSRV_OK)
		goto unlock_and_return;

	/*
	 * FIXME: remove the following workaround, once all user space libraries
	 * have a proper fix for this security issue.
	 */
	secure_fd_export_finish(cmd, psBridgePackageKM->pvParamOut,
				&priv_data->sec_fd_exp_meminfo,
				&priv_data->ui64Stamp);
	switch(cmd)
	{
#if defined(SUPPORT_MEMINFO_IDS)
		case PVRSRV_BRIDGE_MAP_DEV_MEMORY:
		{
			PVRSRV_BRIDGE_OUT_MAP_DEV_MEMORY *psMapDeviceMemoryOUT =
				(PVRSRV_BRIDGE_OUT_MAP_DEV_MEMORY *)psBridgePackageKM->pvParamOut;
			PVRSRV_FILE_PRIVATE_DATA *psPrivateData = PRIVATE_DATA(pFile);
			psMapDeviceMemoryOUT->sDstClientMemInfo.ui64Stamp =	psPrivateData->ui64Stamp;
			break;
		}

		case PVRSRV_BRIDGE_MAP_DEVICECLASS_MEMORY:
		{
			PVRSRV_BRIDGE_OUT_MAP_DEVICECLASS_MEMORY *psDeviceClassMemoryOUT =
				(PVRSRV_BRIDGE_OUT_MAP_DEVICECLASS_MEMORY *)psBridgePackageKM->pvParamOut;
			psDeviceClassMemoryOUT->sClientMemInfo.ui64Stamp = ++ui64Stamp;
			break;
		}
#endif 

		default:
			break;
	}

unlock_and_return:
	LinuxUnLockMutex(&gPVRSRVLock);
	return err;
}

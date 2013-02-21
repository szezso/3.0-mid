/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#include <drm/drmP.h>
#include <drm/drm.h>
#include "psb_drm.h"
#include "psb_drv.h"
#include "psb_fb.h"
#include "psb_reg.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include <drm/drm_pciids.h>
#include "pvr_drm_shared.h"
#include "psb_powermgmt.h"
#include "img_types.h"
#include <linux/cpu.h>
#include <linux/dmi.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>

#include "mdfld_dsi_dbi.h"
#ifdef CONFIG_MDFLD_DSI_DPU
#include "mdfld_dsi_dbi_dpu.h"
#endif

#ifdef CONFIG_MDFD_GL3
#include "mdfld_gl3.h"
#endif

#ifdef CONFIG_MDFD_HDMI
#include "mdfld_msic.h"
#endif
#include "psb_intel_hdmi.h"

/*IMG headers*/
#include "pvr_drm_shared.h"
#include "img_types.h"
#include "pvr_bridge.h"
#include "linkage.h"

#include "bufferclass_video_linux.h"

#include "android_hdmi.h"

int drm_psb_debug;
/*EXPORT_SYMBOL(drm_psb_debug); */
static int drm_psb_trap_pagefaults;

int drm_psb_no_fb;
int drm_tc35876x_debug;
int drm_psb_force_pipeb;
int drm_msvdx_pmpolicy = PSB_PMPOLICY_POWERDOWN;
int drm_psb_cpurelax = 0;
int drm_psb_udelaydivider = 1;
int drm_psb_udelaymultiplier = 1;
int drm_topaz_pmpolicy = PSB_PMPOLICY_NOPM;
int drm_topaz_sbuswa;
int drm_psb_topaz_clockgating = 0;
static int PanelID = TC35876X;
char HDMI_EDID[HDMI_MONITOR_NAME_LENGTH];

static int psb_probe(struct pci_dev *pdev, const struct pci_device_id *ent);

MODULE_PARM_DESC(debug, "Enable debug output");
MODULE_PARM_DESC(tc35876x_debug, "Enable TC35876X register debug output");
MODULE_PARM_DESC(no_fb, "Disable FBdev");
MODULE_PARM_DESC(trap_pagefaults, "Error and reset on MMU pagefaults");
MODULE_PARM_DESC(disable_vsync, "Disable vsync interrupts");
MODULE_PARM_DESC(force_pipeb, "Forces PIPEB to become primary fb");
MODULE_PARM_DESC(ta_mem_size, "TA memory size in kiB");
MODULE_PARM_DESC(msvdx_pmpolicy, "msvdx power management policy btw frames");
MODULE_PARM_DESC(topaz_pmpolicy, "topaz power managerment policy btw frames");
MODULE_PARM_DESC(topaz_sbuswa, "WA for topaz sysbus write");
MODULE_PARM_DESC(PanelID, "Panel info for querying");
MODULE_PARM_DESC(hdmi_edid, "EDID info for HDMI monitor");
MODULE_PARM_DESC(cpu_relax, "replace udelay with cpurelax for video");
MODULE_PARM_DESC(udelay_multiplier, "the multiplier of the usec of video udelay");
MODULE_PARM_DESC(udelay_divider, "the divider of the usec of video udelay");

module_param_named(debug, drm_psb_debug, int, 0600);
module_param_named(tc35876x_debug, drm_tc35876x_debug, int, 0600);
module_param_named(no_fb, drm_psb_no_fb, int, 0600);
module_param_named(trap_pagefaults, drm_psb_trap_pagefaults, int, 0600);
module_param_named(force_pipeb, drm_psb_force_pipeb, int, 0600);
module_param_named(msvdx_pmpolicy, drm_msvdx_pmpolicy, int, 0600);
module_param_named(cpu_relax, drm_psb_cpurelax, int, 0600);
module_param_named(udelay_multiplier, drm_psb_udelaymultiplier, int, 0600);
module_param_named(udelay_divider, drm_psb_udelaydivider, int, 0600);
module_param_named(topaz_pmpolicy, drm_topaz_pmpolicy, int, 0600);
module_param_named(topaz_sbuswa, drm_topaz_sbuswa, int, 0600);
module_param_named(topaz_clockgating, drm_psb_topaz_clockgating, int, 0600);
module_param_named(PanelID, PanelID, int, 0600);
module_param_string(hdmi_edid, HDMI_EDID, 20, 0600);

static struct pci_device_id pciidlist[] = {
#ifdef SGX535
	{0x8086, 0x4100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4101, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4102, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4103, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4104, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4105, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4106, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
	{0x8086, 0x4107, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRST_4100},
#endif
#ifdef SGX540
	{0x8086, 0x0130, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0131, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0132, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0133, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0134, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0135, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0136, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
	{0x8086, 0x0137, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MDFLD_0130},
#endif
	{0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, pciidlist);
/*
 * Standard IOCTLs.
 */

#define DRM_IOCTL_PSB_KMS_OFF	\
		DRM_IO(DRM_PSB_KMS_OFF + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_KMS_ON	\
		DRM_IO(DRM_PSB_KMS_ON + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_VT_LEAVE	\
		DRM_IO(DRM_PSB_VT_LEAVE + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_VT_ENTER	\
		DRM_IO(DRM_PSB_VT_ENTER + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_EXTENSION	\
		DRM_IOWR(DRM_PSB_EXTENSION + DRM_COMMAND_BASE, \
			 union drm_psb_extension_arg)
#define DRM_IOCTL_PSB_SIZES	\
		DRM_IOR(DRM_PSB_SIZES + DRM_COMMAND_BASE, \
			struct drm_psb_sizes_arg)
#define DRM_IOCTL_PSB_FUSE_REG	\
		DRM_IOWR(DRM_PSB_FUSE_REG + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_VBT	\
		DRM_IOWR(DRM_PSB_VBT + DRM_COMMAND_BASE, \
			struct gct_ioctl_arg)
#define DRM_IOCTL_PSB_DC_STATE	\
		DRM_IOW(DRM_PSB_DC_STATE + DRM_COMMAND_BASE, \
			struct drm_psb_dc_state_arg)
#define DRM_IOCTL_PSB_ADB	\
		DRM_IOWR(DRM_PSB_ADB + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_MODE_OPERATION	\
		DRM_IOWR(DRM_PSB_MODE_OPERATION + DRM_COMMAND_BASE, \
			 struct drm_psb_mode_operation_arg)
#define DRM_IOCTL_PSB_STOLEN_MEMORY	\
		DRM_IOWR(DRM_PSB_STOLEN_MEMORY + DRM_COMMAND_BASE, \
			 struct drm_psb_stolen_memory_arg)
#define DRM_IOCTL_PSB_REGISTER_RW	\
		DRM_IOWR(DRM_PSB_REGISTER_RW + DRM_COMMAND_BASE, \
			 struct drm_psb_register_rw_arg)
#define DRM_IOCTL_PSB_GTT_MAP	\
		DRM_IOWR(DRM_PSB_GTT_MAP + DRM_COMMAND_BASE, \
			 struct psb_gtt_mapping_arg)
#define DRM_IOCTL_PSB_GTT_UNMAP	\
		DRM_IOW(DRM_PSB_GTT_UNMAP + DRM_COMMAND_BASE, \
			struct psb_gtt_mapping_arg)
#define DRM_IOCTL_PSB_GETPAGEADDRS	\
		DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_GETPAGEADDRS,\
			 struct drm_psb_getpageaddrs_arg)
#define DRM_IOCTL_PSB_HIST_ENABLE	\
		DRM_IOWR(DRM_PSB_HIST_ENABLE + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_HIST_STATUS	\
		DRM_IOWR(DRM_PSB_HIST_STATUS + DRM_COMMAND_BASE, \
			 struct drm_psb_hist_status_arg)
#define DRM_IOCTL_PSB_UPDATE_GUARD	\
		DRM_IOWR(DRM_PSB_UPDATE_GUARD + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_INIT_COMM	\
		DRM_IOWR(DRM_PSB_INIT_COMM + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_DPST	\
		DRM_IOWR(DRM_PSB_DPST + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_GAMMA	\
		DRM_IOWR(DRM_PSB_GAMMA + DRM_COMMAND_BASE, \
			 struct drm_psb_dpst_lut_arg)
#define DRM_IOCTL_PSB_DPST_BL	\
		DRM_IOWR(DRM_PSB_DPST_BL + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_GET_PIPE_FROM_CRTC_ID	\
		DRM_IOWR(DRM_PSB_GET_PIPE_FROM_CRTC_ID + DRM_COMMAND_BASE, \
			 struct drm_psb_get_pipe_from_crtc_id_arg)

/*pvr ioctls*/
#define PVR_DRM_SRVKM_IOCTL \
	DRM_IOW(DRM_COMMAND_BASE + PVR_DRM_SRVKM_CMD, \
		PVRSRV_BRIDGE_PACKAGE)
#define PVR_DRM_DISP_IOCTL \
	DRM_IO(DRM_COMMAND_BASE + PVR_DRM_DISP_CMD)
#define PVR_DRM_BC_IOCTL \
	DRM_IO(DRM_COMMAND_BASE + PVR_DRM_BC_CMD)
#define PVR_DRM_IS_MASTER_IOCTL \
	DRM_IO(DRM_COMMAND_BASE + PVR_DRM_IS_MASTER_CMD)
#define PVR_DRM_UNPRIV_IOCTL \
	DRM_IOWR(DRM_COMMAND_BASE + PVR_DRM_UNPRIV_CMD, \
		IMG_UINT32)
#if defined(PDUMP)
#define PVR_DRM_DBGDRV_IOCTL \
	DRM_IOW(DRM_COMMAND_BASE + PVR_DRM_DBGDRV_CMD, IOCTL_PACKAGE)
#endif

/*DPU/DSR stuff*/
#define DRM_IOCRL_PSB_DPU_QUERY DRM_IOR(DRM_PSB_DPU_QUERY + DRM_COMMAND_BASE, IMG_UINT32)
#define DRM_IOCRL_PSB_DPU_DSR_ON DRM_IOW(DRM_PSB_DPU_DSR_ON + DRM_COMMAND_BASE, IMG_UINT32)
#define DRM_IOCRL_PSB_DPU_DSR_OFF DRM_IOW(DRM_PSB_DPU_DSR_OFF + DRM_COMMAND_BASE, struct drm_psb_drv_dsr_off_arg)

/*
 * TTM execbuf extension.
 */
#if defined(PDUMP)
#define DRM_PSB_CMDBUF		  (PVR_DRM_DBGDRV_CMD + 1)
#else
#define DRM_PSB_CMDBUF		  (DRM_PSB_DPU_DSR_OFF + 1)
/* #define DRM_PSB_CMDBUF		  (DRM_PSB_DPST_BL + 1) */
#endif

#define DRM_PSB_SCENE_UNREF	  (DRM_PSB_CMDBUF + 1)
#define DRM_IOCTL_PSB_CMDBUF	\
		DRM_IOW(DRM_PSB_CMDBUF + DRM_COMMAND_BASE,	\
			struct drm_psb_cmdbuf_arg)
/*
 * TTM placement user extension.
 */

#define DRM_PSB_PLACEMENT_OFFSET   (DRM_PSB_SCENE_UNREF + 1)

#define DRM_PSB_TTM_PL_CREATE	 (TTM_PL_CREATE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_REFERENCE (TTM_PL_REFERENCE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_UNREF	 (TTM_PL_UNREF + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_SYNCCPU	 (TTM_PL_SYNCCPU + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_WAITIDLE  (TTM_PL_WAITIDLE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_SETSTATUS (TTM_PL_SETSTATUS + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_CREATE_UB (TTM_PL_CREATE_UB + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_WRAP_PVR_BUF   \
				(TTM_PL_WRAP_PVR_BUF + DRM_PSB_PLACEMENT_OFFSET)

/*
 * TTM fence extension.
 */

#define DRM_PSB_FENCE_OFFSET	   (DRM_PSB_TTM_PL_CREATE_UB + 1)
#define DRM_PSB_TTM_FENCE_SIGNALED (TTM_FENCE_SIGNALED + DRM_PSB_FENCE_OFFSET)
#define DRM_PSB_TTM_FENCE_FINISH   (TTM_FENCE_FINISH + DRM_PSB_FENCE_OFFSET)
#define DRM_PSB_TTM_FENCE_UNREF    (TTM_FENCE_UNREF + DRM_PSB_FENCE_OFFSET)

#define DRM_PSB_FLIP	   (DRM_PSB_TTM_FENCE_UNREF + 1)	/*20*/
/* PSB video extension */
#define DRM_LNC_VIDEO_GETPARAM		(DRM_PSB_FLIP + 1)

/*BC_VIDEO ioctl*/
#define DRM_BUFFER_CLASS_VIDEO      (DRM_LNC_VIDEO_GETPARAM + 1)    /*0x32*/

/*BC_ST_GFX_VIDEO ioctl*/
#define DRM_ST_GFX_BUFFER_CLASS_VIDEO	(DRM_BUFFER_CLASS_VIDEO + 1)  /*0x33*/

#define DRM_IOCTL_PSB_TTM_PL_CREATE    \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_CREATE,\
		 union ttm_pl_create_arg)
#define DRM_IOCTL_PSB_TTM_PL_REFERENCE \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_REFERENCE,\
		 union ttm_pl_reference_arg)
#define DRM_IOCTL_PSB_TTM_PL_UNREF    \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_UNREF,\
		struct ttm_pl_reference_req)
#define DRM_IOCTL_PSB_TTM_PL_SYNCCPU	\
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_SYNCCPU,\
		struct ttm_pl_synccpu_arg)
#define DRM_IOCTL_PSB_TTM_PL_WAITIDLE	 \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_WAITIDLE,\
		struct ttm_pl_waitidle_arg)
#define DRM_IOCTL_PSB_TTM_PL_SETSTATUS \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_SETSTATUS,\
		 union ttm_pl_setstatus_arg)
#define DRM_IOCTL_PSB_TTM_PL_CREATE_UB    \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_CREATE_UB,\
		 union ttm_pl_create_ub_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_SIGNALED \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_SIGNALED,	\
		  union ttm_fence_signaled_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_FINISH \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_FINISH,	\
		 union ttm_fence_finish_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_UNREF \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_UNREF,	\
		 struct ttm_fence_unref_arg)
#define DRM_IOCTL_PSB_FLIP \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_FLIP, \
		 struct drm_psb_pageflip_arg)
#define DRM_IOCTL_LNC_VIDEO_GETPARAM \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_LNC_VIDEO_GETPARAM, \
		 struct drm_lnc_video_getparam_arg)

/*bc_video ioctl*/
#define DRM_IOCTL_BUFFER_CLASS_VIDEO \
        DRM_IOWR(DRM_COMMAND_BASE + DRM_BUFFER_CLASS_VIDEO, \
             BC_Video_ioctl_package)

#define DRM_IOCTL_PSB_TTM_PL_WRAP_PVR_BUF \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_WRAP_PVR_BUF, \
		 union ttm_pl_create_arg)
/*bc_st_gfx_video ioctl*/
#define DRM_IOCTL_ST_GFX_BUFFER_CLASS_VIDEO \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_ST_GFX_BUFFER_CLASS_VIDEO, \
		BC_Video_ioctl_package)

static int psb_vt_leave_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_vt_enter_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_sizes_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
static int psb_fuse_reg_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_vbt_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
static int psb_dc_state_ioctl(struct drm_device *dev, void * data,
			      struct drm_file *file_priv);
static int psb_adb_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
static int psb_mode_operation_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
static int psb_stolen_memory_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
static int psb_register_rw_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_hist_enable_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_hist_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_update_guard_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
static int psb_init_comm_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
static int psb_dpst_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
static int psb_gamma_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
static int psb_dpst_bl_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
static int psb_dpu_query_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
static int psb_dpu_dsr_on_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);

static int psb_dpu_dsr_off_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);

#define PSB_IOCTL_DEF(ioctl, func, flags) \
	[DRM_IOCTL_NR(ioctl) - DRM_COMMAND_BASE] = {ioctl, flags, func}

static struct drm_ioctl_desc psb_ioctls[] = {
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_KMS_OFF, psbfb_kms_off_ioctl,
	DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_KMS_ON,
	psbfb_kms_on_ioctl,
	DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VT_LEAVE, psb_vt_leave_ioctl,
	DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VT_ENTER,
	psb_vt_enter_ioctl,
	DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_EXTENSION, psb_extension_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_SIZES, psb_sizes_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_FUSE_REG, psb_fuse_reg_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VBT, psb_vbt_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DC_STATE, psb_dc_state_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_ADB, psb_adb_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_MODE_OPERATION, psb_mode_operation_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_STOLEN_MEMORY, psb_stolen_memory_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_REGISTER_RW, psb_register_rw_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GTT_MAP,
	psb_gtt_map_meminfo_ioctl,
	DRM_AUTH|DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GTT_UNMAP,
	psb_gtt_unmap_meminfo_ioctl,
	DRM_AUTH|DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GETPAGEADDRS,
	psb_getpageaddrs_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(PVR_DRM_SRVKM_IOCTL, PVRSRV_BridgeDispatchKM, 0),
	PSB_IOCTL_DEF(PVR_DRM_DISP_IOCTL, PVRDRM_Dummy_ioctl, 0),
	PSB_IOCTL_DEF(PVR_DRM_BC_IOCTL, PVRDRM_Dummy_ioctl, 0),
	PSB_IOCTL_DEF(PVR_DRM_IS_MASTER_IOCTL, PVRDRMIsMaster, DRM_MASTER),
	PSB_IOCTL_DEF(PVR_DRM_UNPRIV_IOCTL, PVRDRMUnprivCmd, 0),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HIST_ENABLE,
	psb_hist_enable_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HIST_STATUS,
	psb_hist_status_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_UPDATE_GUARD, psb_update_guard_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_INIT_COMM, psb_init_comm_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DPST, psb_dpst_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GAMMA, psb_gamma_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DPST_BL, psb_dpst_bl_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GET_PIPE_FROM_CRTC_ID, psb_intel_get_pipe_from_crtc_id, 0),
#if defined(PDUMP)
	PSB_IOCTL_DEF(PVR_DRM_DBGDRV_IOCTL, SYSPVRDBGDrivIoctl, 0),
#endif
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_CMDBUF, psb_cmdbuf_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_CREATE, psb_pl_create_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_REFERENCE, psb_pl_reference_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_UNREF, psb_pl_unref_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_SYNCCPU, psb_pl_synccpu_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_WAITIDLE, psb_pl_waitidle_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_SETSTATUS, psb_pl_setstatus_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_CREATE_UB, psb_pl_ub_create_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_WRAP_PVR_BUF,
		      psb_pl_wrap_pvr_buf_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_SIGNALED,
	psb_fence_signaled_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_FINISH, psb_fence_finish_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_UNREF, psb_fence_unref_ioctl,
	DRM_AUTH),
	/*to be removed later */
	/*PSB_IOCTL_DEF(DRM_IOCTL_PSB_FLIP, psb_page_flip, DRM_AUTH),*/
	PSB_IOCTL_DEF(DRM_IOCTL_LNC_VIDEO_GETPARAM,
	lnc_video_getparam, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_BUFFER_CLASS_VIDEO,
	BC_Video_Bridge, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_QUERY, psb_dpu_query_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_DSR_ON, psb_dpu_dsr_on_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_DSR_OFF, psb_dpu_dsr_off_ioctl,
	DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_ST_GFX_BUFFER_CLASS_VIDEO,
	psb_st_gfx_video_bridge, DRM_AUTH)
};

static void psb_lastclose(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct psb_fbdev * fbdev = dev_priv->fbdev;

	drm_fb_helper_restore_fbdev_mode(&fbdev->psb_fb_helper);

	return;

	if (!dev->dev_private)
		return;

	mutex_lock(&dev_priv->cmdbuf_mutex);
	if (dev_priv->context.buffers) {
		vfree(dev_priv->context.buffers);
		dev_priv->context.buffers = NULL;
	}
	mutex_unlock(&dev_priv->cmdbuf_mutex);
}

static void psb_do_takedown(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct ttm_bo_device *bdev = &dev_priv->bdev;


	if (dev_priv->have_mem_mmu) {
		ttm_bo_clean_mm(bdev, DRM_PSB_MEM_MMU);
		dev_priv->have_mem_mmu = 0;
	}

	if (dev_priv->have_tt) {
		ttm_bo_clean_mm(bdev, TTM_PL_TT);
		dev_priv->have_tt = 0;
	}

	if (dev_priv->have_camera) {
		ttm_bo_clean_mm(bdev, TTM_PL_CI);
		dev_priv->have_camera = 0;
	}
	if (dev_priv->have_rar) {
		ttm_bo_clean_mm(bdev, TTM_PL_RAR);
		dev_priv->have_rar = 0;
	}

	psb_msvdx_uninit(dev);
	pnw_topaz_uninit(dev);
}

#define FB_REG06 0xD0810600
#define FB_TOPAZ_DISABLE BIT(0)
#define FB_MIPI_DISABLE  BIT(11)
#define FB_REG09 0xD0810900
#define FB_SKU_MASK  (BIT(12)|BIT(13)|BIT(14))
#define FB_SKU_SHIFT 12
#define FB_SKU_100 0
#define FB_SKU_100L 1
#define FB_SKU_83 2
#if 1 /* FIXME remove it after PO */
#define FB_GFX_CLK_DIVIDE_MASK	(BIT(20)|BIT(21)|BIT(22))
#define FB_GFX_CLK_DIVIDE_SHIFT 20
#define FB_VED_CLK_DIVIDE_MASK	(BIT(23)|BIT(24))
#define FB_VED_CLK_DIVIDE_SHIFT 23
#define FB_VEC_CLK_DIVIDE_MASK	(BIT(25)|BIT(26))
#define FB_VEC_CLK_DIVIDE_SHIFT 25
#endif	/* FIXME remove it after PO */


void mrst_get_fuse_settings(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pci_dev *pci_root = pci_get_bus_and_slot(0, 0);
	uint32_t fuse_value = 0;
	uint32_t fuse_value_tmp = 0;

	pci_write_config_dword(pci_root, 0xD0, FB_REG06);
	pci_read_config_dword(pci_root, 0xD4, &fuse_value);

	dev_priv->iLVDS_enable = 0;

	DRM_INFO("internal display is %s\n",
		 dev_priv->iLVDS_enable ? "LVDS display" : "MIPI display");

	/*prevent Runtime suspend at start*/
	if (dev_priv->iLVDS_enable) {
		dev_priv->is_lvds_on = true;
		dev_priv->is_mipi_on = false;
	} else {
		dev_priv->is_mipi_on = true;
		dev_priv->is_lvds_on = false;
	}

	if (dev_priv->dev->pci_device == PCI_ID_TOPAZ_DISABLED)
		dev_priv->topaz_disabled = 1;
	else
		dev_priv->topaz_disabled = 0;

	dev_priv->video_device_fuse = fuse_value;

	DRM_INFO("topaz is %s\n",
		 dev_priv->topaz_disabled ? "disabled" : "enabled");

	pci_write_config_dword(pci_root, 0xD0, FB_REG09);
	pci_read_config_dword(pci_root, 0xD4, &fuse_value);

	DRM_INFO("SKU values is 0x%x. \n", fuse_value);
	fuse_value_tmp = (fuse_value & FB_SKU_MASK) >> FB_SKU_SHIFT;

	dev_priv->fuse_reg_value = fuse_value;

	switch (fuse_value_tmp) {
	case FB_SKU_100:
		dev_priv->core_freq = 200;
		break;
	case FB_SKU_100L:
		dev_priv->core_freq = 100;
		break;
	case FB_SKU_83:
		dev_priv->core_freq = 166;
		break;
	default:
		DRM_ERROR("Invalid SKU values, SKU value = 0x%08x\n", fuse_value_tmp);
		dev_priv->core_freq = 0;
	}
	DRM_INFO("LNC core clk is %dMHz.\n", dev_priv->core_freq);

#if 1 /* FIXME remove it after PO */
	fuse_value_tmp =
		(fuse_value & FB_GFX_CLK_DIVIDE_MASK) >> FB_GFX_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Gfx clk : core clk = 1:1. \n");
		break;
	case 1:
		DRM_INFO("Gfx clk : core clk = 4:3. \n");
		break;
	case 2:
		DRM_INFO("Gfx clk : core clk = 8:5. \n");
		break;
	case 3:
		DRM_INFO("Gfx clk : core clk = 2:1. \n");
		break;
	case 4:
		DRM_INFO("Gfx clk : core clk = 16:7. \n");
		break;
	case 5:
		DRM_INFO("Gfx clk : core clk = 8:3. \n");
		break;
	case 6:
		DRM_INFO("Gfx clk : core clk = 16:5. \n");
		break;
	case 7:
		DRM_INFO("Gfx clk : core clk = 4:1. \n");
		break;
	default:
		DRM_ERROR("Invalid GFX CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}

	fuse_value_tmp =
		(fuse_value & FB_VED_CLK_DIVIDE_MASK) >> FB_VED_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Ved clk : core clk = 1:1. \n");
		break;
	case 1:
		DRM_INFO("Ved clk : core clk = 4:3. \n");
		break;
	case 2:
		DRM_INFO("Ved clk : core clk = 8:5. \n");
		break;
	case 3:
		DRM_INFO("Ved clk : core clk = 2:1. \n");
		break;
	default:
		DRM_ERROR("Invalid VED CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}

	fuse_value_tmp =
		(fuse_value & FB_VEC_CLK_DIVIDE_MASK) >> FB_VEC_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Vec clk : core clk = 1:1. \n");
		break;
	case 1:
		DRM_INFO("Vec clk : core clk = 4:3. \n");
		break;
	case 2:
		DRM_INFO("Vec clk : core clk = 8:5. \n");
		break;
	case 3:
		DRM_INFO("Vec clk : core clk = 2:1. \n");
		break;
	default:
		DRM_ERROR("Invalid VEC CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}
#endif /* FIXME remove it after PO */

#if KSEL_BYPASS_83_100_ENABLE
	dev_priv->ksel = KSEL_BYPASS_83_100;
#endif /* KSEL_BYPASS_83_100_ENABLE */

#if  KSEL_CRYSTAL_19_ENABLED
	dev_priv->ksel = KSEL_CRYSTAL_19;
#endif /*  KSEL_CRYSTAL_19_ENABLED */

	return;
}

bool mid_get_pci_revID(struct drm_psb_private *dev_priv)
{
	uint32_t platform_rev_id = 0;
	struct pci_dev *pci_gfx_root = pci_get_bus_and_slot(0, PCI_DEVFN(2, 0));

	/*get the revison ID, B0:D2:F0;0x08 */
	pci_read_config_dword(pci_gfx_root, 0x08, &platform_rev_id);
	dev_priv->platform_rev_id = (uint8_t) platform_rev_id;
	pci_dev_put(pci_gfx_root);
	PSB_DEBUG_ENTRY("platform_rev_id is %x\n",	dev_priv->platform_rev_id);

	return true;
}

bool mrst_get_vbt_data(struct drm_psb_private *dev_priv)
{
	struct mrst_vbt *pVBT = &dev_priv->vbt_data;
	u32 platform_config_address;
	u16 new_size;
	u8 *pVBT_virtual;
	u8 bpi;
	u8 number_desc = 0;
	struct mrst_timing_info *dp_ti = &dev_priv->gct_data.DTD;
	struct gct_r10_timing_info ti;
	void *pGCT;
	struct pci_dev *pci_gfx_root = pci_get_bus_and_slot(0, PCI_DEVFN(2, 0));

	/*get the address of the platform config vbt, B0:D2:F0;0xFC */
	pci_read_config_dword(pci_gfx_root, 0xFC, &platform_config_address);
	pci_dev_put(pci_gfx_root);
	DRM_INFO("drm platform config address is %x\n",
		 platform_config_address);

	/* check for platform config address == 0. */
	/* this means fw doesn't support vbt */

	if (platform_config_address == 0) {
		pVBT->Size = 0;
		return false;
	}

	/* get the virtual address of the vbt */
	pVBT_virtual = ioremap(platform_config_address, sizeof(*pVBT));

	memcpy(pVBT, pVBT_virtual, sizeof(*pVBT));
	iounmap(pVBT_virtual); /* Free virtual address space */

	printk(KERN_ALERT "GCT Revision is %x\n", pVBT->Revision);

	switch (pVBT->Revision) {
	case 0:
		pVBT->mrst_gct = NULL;
		pVBT->mrst_gct = \
				 ioremap(platform_config_address + sizeof(*pVBT) - 4,
					 pVBT->Size - sizeof(*pVBT) + 4);
		pGCT = pVBT->mrst_gct;
		bpi = ((struct mrst_gct_v1 *)pGCT)->PD.BootPanelIndex;
		dev_priv->gct_data.bpi = bpi;
		dev_priv->gct_data.pt =
			((struct mrst_gct_v1 *)pGCT)->PD.PanelType;
		memcpy(&dev_priv->gct_data.DTD,
		       &((struct mrst_gct_v1 *)pGCT)->panel[bpi].DTD,
		       sizeof(struct mrst_timing_info));
		dev_priv->gct_data.Panel_Port_Control =
			((struct mrst_gct_v1 *)pGCT)->panel[bpi].Panel_Port_Control;
		dev_priv->gct_data.Panel_MIPI_Display_Descriptor =
			((struct mrst_gct_v1 *)pGCT)->panel[bpi].Panel_MIPI_Display_Descriptor;
		break;
	case 1:
		pVBT->mrst_gct = NULL;
		pVBT->mrst_gct = \
				 ioremap(platform_config_address + sizeof(*pVBT) - 4,
					 pVBT->Size - sizeof(*pVBT) + 4);
		pGCT = pVBT->mrst_gct;
		bpi = ((struct mrst_gct_v2 *)pGCT)->PD.BootPanelIndex;
		dev_priv->gct_data.bpi = bpi;
		dev_priv->gct_data.pt =
			((struct mrst_gct_v2 *)pGCT)->PD.PanelType;
		memcpy(&dev_priv->gct_data.DTD,
		       &((struct mrst_gct_v2 *)pGCT)->panel[bpi].DTD,
		       sizeof(struct mrst_timing_info));
		dev_priv->gct_data.Panel_Port_Control =
			((struct mrst_gct_v2 *)pGCT)->panel[bpi].Panel_Port_Control;
		dev_priv->gct_data.Panel_MIPI_Display_Descriptor =
			((struct mrst_gct_v2 *)pGCT)->panel[bpi].Panel_MIPI_Display_Descriptor;
		break;
	case 0x10:
		/*header definition changed from rev 01 (v2) to rev 10h. */
		/*so, some values have changed location*/
		new_size = pVBT->Checksum; /*checksum contains lo size byte*/
		/*LSB of mrst_gct contains hi size byte*/
		new_size |= ((0xff & (unsigned int)pVBT->mrst_gct)) << 8;

		pVBT->Checksum = pVBT->Size; /*size contains the checksum*/
		if (new_size > 0xff)
			pVBT->Size = 0xff; /*restrict size to 255*/
		else
			pVBT->Size = new_size;

		/* number of descriptors defined in the GCT */
		number_desc = ((0xff00 & (unsigned int)pVBT->mrst_gct)) >> 8;
		bpi = ((0xff0000 & (unsigned int)pVBT->mrst_gct)) >> 16;
		pVBT->mrst_gct = NULL;
		pVBT->mrst_gct = \
				 ioremap(platform_config_address + GCT_R10_HEADER_SIZE,
					 GCT_R10_DISPLAY_DESC_SIZE * number_desc);
		pGCT = pVBT->mrst_gct;
		pGCT = (u8 *)pGCT + (bpi * GCT_R10_DISPLAY_DESC_SIZE);
		dev_priv->gct_data.bpi = bpi; /*save boot panel id*/

		/*copy the GCT display timings into a temp structure*/
		memcpy(&ti, pGCT, sizeof(struct gct_r10_timing_info));

		/*now copy the temp struct into the dev_priv->gct_data*/
		dp_ti->pixel_clock = ti.pixel_clock;
		dp_ti->hactive_hi = ti.hactive_hi;
		dp_ti->hactive_lo = ti.hactive_lo;
		dp_ti->hblank_hi = ti.hblank_hi;
		dp_ti->hblank_lo = ti.hblank_lo;
		dp_ti->hsync_offset_hi = ti.hsync_offset_hi;
		dp_ti->hsync_offset_lo = ti.hsync_offset_lo;
		dp_ti->hsync_pulse_width_hi = ti.hsync_pulse_width_hi;
		dp_ti->hsync_pulse_width_lo = ti.hsync_pulse_width_lo;
		dp_ti->vactive_hi = ti.vactive_hi;
		dp_ti->vactive_lo = ti.vactive_lo;
		dp_ti->vblank_hi = ti.vblank_hi;
		dp_ti->vblank_lo = ti.vblank_lo;
		dp_ti->vsync_offset_hi = ti.vsync_offset_hi;
		dp_ti->vsync_offset_lo = ti.vsync_offset_lo;
		dp_ti->vsync_pulse_width_hi = ti.vsync_pulse_width_hi;
		dp_ti->vsync_pulse_width_lo = ti.vsync_pulse_width_lo;

		/*mov the MIPI_Display_Descriptor data from GCT to dev priv*/
		dev_priv->gct_data.Panel_MIPI_Display_Descriptor =
			*((u8 *)pGCT + 0x0d);
		dev_priv->gct_data.Panel_MIPI_Display_Descriptor |=
			(*((u8 *)pGCT + 0x0e)) << 8;
		break;
	default:
		printk(KERN_ALERT "Unknown revision of GCT!\n");
		pVBT->Size = 0;
		return false;
	}

	if (PanelID == GCT_DETECT) {
		if (dev_priv->gct_data.bpi == 0) {
			PSB_DEBUG_ENTRY("[GFX] TMD Panel Detected.\n");
			dev_priv->panel_id = TMD_VID;
			PanelID = TMD_VID;
		} else {
			PSB_DEBUG_ENTRY("[GFX] Default Panel (TPO)\n");
			dev_priv->panel_id = TPO_CMD;
			PanelID = TPO_CMD;
		}
	} else {
		PSB_DEBUG_ENTRY("[GFX] Panel Parameter Passed in through cmd line\n");
		dev_priv->panel_id = PanelID;
	}

	return true;
}

static int psb_do_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct ttm_bo_device *bdev = &dev_priv->bdev;
	struct psb_gtt *pg = dev_priv->pg;

	uint32_t stolen_gtt;
	uint32_t tt_start;
	uint32_t tt_pages;

	int ret = -ENOMEM;


	/*
	 * Initialize sequence numbers for the different command
	 * submission mechanisms.
	 */

	dev_priv->sequence[PSB_ENGINE_2D] = 0;
	dev_priv->sequence[PSB_ENGINE_VIDEO] = 1;
	dev_priv->sequence[LNC_ENGINE_ENCODE] = 0;

	if (pg->mmu_gatt_start & 0x0FFFFFFF) {
		DRM_ERROR("Gatt must be 256M aligned. This is a bug.\n");
		ret = -EINVAL;
		goto out_err;
	}

	stolen_gtt = (pg->stolen_size >> PAGE_SHIFT) * 4;
	stolen_gtt = (stolen_gtt + PAGE_SIZE - 1) >> PAGE_SHIFT;
	stolen_gtt =
		(stolen_gtt < pg->gtt_pages) ? stolen_gtt : pg->gtt_pages;

	dev_priv->gatt_free_offset = pg->mmu_gatt_start +
				     (stolen_gtt << PAGE_SHIFT) * 1024;

	if (1 || drm_debug) {
		uint32_t core_id = PSB_RSGX32(PSB_CR_CORE_ID);
		uint32_t core_rev = PSB_RSGX32(PSB_CR_CORE_REVISION);
		DRM_INFO("SGX core id = 0x%08x\n", core_id);
		DRM_INFO("SGX core rev major = 0x%02x, minor = 0x%02x\n",
			 (core_rev & _PSB_CC_REVISION_MAJOR_MASK) >>
			 _PSB_CC_REVISION_MAJOR_SHIFT,
			 (core_rev & _PSB_CC_REVISION_MINOR_MASK) >>
			 _PSB_CC_REVISION_MINOR_SHIFT);
		DRM_INFO
		("SGX core rev maintenance = 0x%02x, designer = 0x%02x\n",
		 (core_rev & _PSB_CC_REVISION_MAINTENANCE_MASK) >>
		 _PSB_CC_REVISION_MAINTENANCE_SHIFT,
		 (core_rev & _PSB_CC_REVISION_DESIGNER_MASK) >>
		 _PSB_CC_REVISION_DESIGNER_SHIFT);
	}

	spin_lock_init(&dev_priv->irqmask_lock);

	tt_pages = (pg->gatt_pages < PSB_TT_PRIV0_PLIMIT) ?
		   pg->gatt_pages : PSB_TT_PRIV0_PLIMIT;
	tt_start = dev_priv->gatt_free_offset - pg->mmu_gatt_start;
	tt_pages -= tt_start >> PAGE_SHIFT;
	dev_priv->sizes.ta_mem_size = 0;

	/* TT region managed by TTM. */
	if (!ttm_bo_init_mm(bdev, TTM_PL_TT,
			    pg->gatt_pages -
			    (pg->ci_start >> PAGE_SHIFT) -
			    ((dev_priv->ci_region_size + dev_priv->rar_region_size)
			     >> PAGE_SHIFT))) {

		dev_priv->have_tt = 1;
		dev_priv->sizes.tt_size =
			(tt_pages << PAGE_SHIFT) / (1024 * 1024) / 2;
	}

	if (!ttm_bo_init_mm(bdev,
			    DRM_PSB_MEM_MMU,
			    PSB_MEM_TT_START >> PAGE_SHIFT)) {
		dev_priv->have_mem_mmu = 1;
		dev_priv->sizes.mmu_size =
			PSB_MEM_TT_START / (1024 * 1024);
	}


	PSB_DEBUG_INIT("Init MSVDX\n");
	psb_msvdx_init(dev);

	PSB_DEBUG_INIT("Init Topaz\n");
	/* for sku100L and sku100M, VEC is disabled in fuses */
	pnw_topaz_init(dev);

	return 0;
out_err:
	psb_do_takedown(dev);
	return ret;
}

static int psb_driver_unload(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	/*Fristly, unload pvr driver*/
	PVRSRVDrmUnload(dev);

	/*TODO: destroy DSR/DPU infos here*/
	psb_backlight_exit(); /*writes minimum value to backlight HW reg */

	if (drm_psb_no_fb == 0)
		psb_modeset_cleanup(dev);

	if (dev_priv) {
		psb_do_takedown(dev);

		if (dev_priv->pf_pd) {
			psb_mmu_free_pagedir(dev_priv->pf_pd);
			dev_priv->pf_pd = NULL;
		}
		if (dev_priv->mmu) {
			struct psb_gtt *pg = dev_priv->pg;

			psb_mmu_remove_pfn_sequence(
				psb_mmu_get_default_pd
				(dev_priv->mmu),
				pg->mmu_gatt_start,
				pg->vram_stolen_size >> PAGE_SHIFT);
			if (pg->ci_stolen_size != 0)
				psb_mmu_remove_pfn_sequence(
					psb_mmu_get_default_pd
					(dev_priv->mmu),
					pg->ci_start,
					pg->ci_stolen_size >> PAGE_SHIFT);
			if (pg->rar_stolen_size != 0)
				psb_mmu_remove_pfn_sequence(
					psb_mmu_get_default_pd
					(dev_priv->mmu),
					pg->rar_start,
					pg->rar_stolen_size >> PAGE_SHIFT);
			psb_mmu_driver_takedown(dev_priv->mmu);
			dev_priv->mmu = NULL;
		}
		psb_gtt_takedown(dev_priv->pg, 1);
		if (dev_priv->scratch_page) {
			__free_page(dev_priv->scratch_page);
			dev_priv->scratch_page = NULL;
		}
		if (dev_priv->has_bo_device) {
			ttm_bo_device_release(&dev_priv->bdev);
			dev_priv->has_bo_device = 0;
		}
		if (dev_priv->has_fence_device) {
			ttm_fence_device_release(&dev_priv->fdev);
			dev_priv->has_fence_device = 0;
		}
		if (dev_priv->vdc_reg) {
			iounmap(dev_priv->vdc_reg);
			dev_priv->vdc_reg = NULL;
		}
		if (dev_priv->sgx_reg) {
			iounmap(dev_priv->sgx_reg);
			dev_priv->sgx_reg = NULL;
		}
#ifdef CONFIG_MDFD_GL3
		iounmap(dev_priv->gl3_reg);
		dev_priv->gl3_reg = NULL;
#endif

		if (dev_priv->msvdx_reg) {
			iounmap(dev_priv->msvdx_reg);
			dev_priv->msvdx_reg = NULL;
		}

		if (dev_priv->topaz_reg) {
			iounmap(dev_priv->topaz_reg);
			dev_priv->topaz_reg = NULL;
		}

		if (dev_priv->tdev)
			ttm_object_device_release(&dev_priv->tdev);

		if (dev_priv->has_global)
			psb_ttm_global_release(dev_priv);

		kfree(dev_priv);
		dev->dev_private = NULL;
	}

	ospm_power_uninit(dev);

	return 0;
}

static int psb_driver_load(struct drm_device *dev, unsigned long chipset)
{
	struct drm_psb_private *dev_priv;
	struct ttm_bo_device *bdev;
	unsigned long resource_start;
	struct psb_gtt *pg;
	unsigned long irqflags;
	int ret = -ENOMEM;
	uint32_t tt_pages;
	struct pci_dev *pdev;
	struct device *ddev;
	struct kobject *kobj;

	DRM_INFO("psb - %s\n", PSB_PACKAGE_VERSION);

	dev_priv = kzalloc(sizeof(*dev_priv), GFP_KERNEL);
	if (dev_priv == NULL)
		return -ENOMEM;

	DRM_INFO("Run drivers on Medfield platform!\n");

	INIT_LIST_HEAD(&dev_priv->video_ctx);

	dev_priv->num_pipe = 3;

	dev_priv->psb_hotplug_state = NULL;
	dev_priv->hdmi_done_reading_edid = false;
	dev_priv->xserver_start = false;

	dev_priv->dev = dev;
	bdev = &dev_priv->bdev;

	ret = psb_ttm_global_init(dev_priv);
	if (unlikely(ret != 0))
		goto out_err;
	dev_priv->has_global = 1;

	dev_priv->tdev = ttm_object_device_init
			 (dev_priv->mem_global_ref.object, PSB_OBJECT_HASH_ORDER);
	if (unlikely(dev_priv->tdev == NULL))
		goto out_err;

	mutex_init(&dev_priv->temp_mem);
	mutex_init(&dev_priv->cmdbuf_mutex);
	mutex_init(&dev_priv->reset_mutex);
	INIT_LIST_HEAD(&dev_priv->context.validate_list);
	INIT_LIST_HEAD(&dev_priv->context.kern_validate_list);

	mutex_init(&dev_priv->dsr_mutex);

	spin_lock_init(&dev_priv->reloc_lock);

	DRM_INIT_WAITQUEUE(&dev_priv->rel_mapped_queue);

	dev->dev_private = (void *) dev_priv;
	dev_priv->chipset = chipset;

	PSB_DEBUG_GENERAL("Init scheduler\n");
	psb_scheduler_init(dev, &dev_priv->scheduler);


	PSB_DEBUG_INIT("Mapping MMIO\n");
	resource_start = pci_resource_start(dev->pdev, PSB_MMIO_RESOURCE);

	dev_priv->msvdx_reg = ioremap(resource_start + MRST_MSVDX_OFFSET,
				PSB_MSVDX_SIZE);

	if (!dev_priv->msvdx_reg)
		goto out_err;

	dev_priv->topaz_reg = ioremap(resource_start + PNW_TOPAZ_OFFSET,
				PNW_TOPAZ_SIZE);
	if (!dev_priv->topaz_reg)
		goto out_err;

	dev_priv->vdc_reg =
		ioremap(resource_start + PSB_VDC_OFFSET, PSB_VDC_SIZE);
	if (!dev_priv->vdc_reg)
		goto out_err;

	dev_priv->sgx_reg = ioremap(resource_start + MRST_SGX_OFFSET,
				PSB_SGX_SIZE);

	if (!dev_priv->sgx_reg)
		goto out_err;

	/* setup hdmi driver */
	android_hdmi_driver_setup(dev);

	mrst_get_fuse_settings(dev);
	mrst_get_vbt_data(dev_priv);
	mid_get_pci_revID(dev_priv);

#ifdef CONFIG_MDFD_GL3
	// GL3
	dev_priv->gl3_reg = ioremap(resource_start + MDFLD_GL3_OFFSET,
				MDFLD_GL3_SIZE);
#endif

	PSB_DEBUG_INIT("Init TTM fence and BO driver\n");

	/* Init OSPM support */
	ospm_power_init(dev);

	ret = psb_ttm_fence_device_init(&dev_priv->fdev);
	if (unlikely(ret != 0))
		goto out_err;

	/* For VXD385 DE2.x firmware support 16bit fence value */
	dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].wrap_diff = (1 << 14);
	dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].flush_diff = (1 << 13);
	dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].sequence_mask = 0x0000ffff;

	dev_priv->has_fence_device = 1;
	ret = ttm_bo_device_init(bdev,
				 dev_priv->bo_global_ref.ref.object,
				 &psb_ttm_bo_driver,
				 DRM_PSB_FILE_PAGE_OFFSET, false);
	if (unlikely(ret != 0))
		goto out_err;
	dev_priv->has_bo_device = 1;
	ttm_lock_init(&dev_priv->ttm_lock);

	ret = -ENOMEM;

	dev_priv->scratch_page = alloc_page(GFP_DMA32 | __GFP_ZERO);
	if (!dev_priv->scratch_page)
		goto out_err;

	set_pages_uc(dev_priv->scratch_page, 1);

	dev_priv->pg = psb_gtt_alloc(dev);
	if (!dev_priv->pg)
		goto out_err;

	ret = psb_gtt_init(dev_priv->pg, 0);
	if (ret)
		goto out_err;

	ret = psb_gtt_mm_init(dev_priv->pg);
	if (ret)
		goto out_err;

	dev_priv->mmu = psb_mmu_driver_init((void *)0,
					    drm_psb_trap_pagefaults, 0,
					    dev_priv);
	if (!dev_priv->mmu)
		goto out_err;

	pg = dev_priv->pg;

	tt_pages = (pg->gatt_pages < PSB_TT_PRIV0_PLIMIT) ?
		   (pg->gatt_pages) : PSB_TT_PRIV0_PLIMIT;

	/* CI/RAR use the lower half of TT. */
	pg->ci_start = (tt_pages / 2) << PAGE_SHIFT;
	pg->rar_start = pg->ci_start + pg->ci_stolen_size;


	/*
	 * Make MSVDX/TOPAZ MMU aware of the CI stolen memory area.
	 */
	if (dev_priv->pg->ci_stolen_size != 0) {
		ret = psb_mmu_insert_pfn_sequence(psb_mmu_get_default_pd
						  (dev_priv->mmu),
						  dev_priv->ci_region_start >> PAGE_SHIFT,
						  pg->mmu_gatt_start + pg->ci_start,
						  pg->ci_stolen_size >> PAGE_SHIFT, 0);
		if (ret)
			goto out_err;
	}

	/*
	 * Make MSVDX/TOPAZ MMU aware of the rar stolen memory area.
	 */
	if (dev_priv->pg->rar_stolen_size != 0) {
		ret = psb_mmu_insert_pfn_sequence(
			      psb_mmu_get_default_pd(dev_priv->mmu),
			      dev_priv->rar_region_start >> PAGE_SHIFT,
			      pg->mmu_gatt_start + pg->rar_start,
			      pg->rar_stolen_size >> PAGE_SHIFT, 0);
		if (ret)
			goto out_err;
	}

	dev_priv->pf_pd = psb_mmu_alloc_pd(dev_priv->mmu, 1, 0);
	if (!dev_priv->pf_pd)
		goto out_err;

	psb_mmu_set_pd_context(psb_mmu_get_default_pd(dev_priv->mmu), 0);
	psb_mmu_set_pd_context(dev_priv->pf_pd, 1);

	spin_lock_init(&dev_priv->sequence_lock);

	PSB_DEBUG_INIT("Begin to init MSVDX/Topaz\n");

	ret = psb_do_init(dev);
	if (ret)
		return ret;

	ret = drm_vblank_init(dev, dev_priv->num_pipe);
	if (ret)
		goto out_err;

	/*
	 * Install interrupt handlers prior to powering off SGX or else we will
	 * crash.
	 */
	dev_priv->vdc_irq_mask = 0;
	dev_priv->pipestat[0] = 0;
	dev_priv->pipestat[1] = 0;
	dev_priv->pipestat[2] = 0;
	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	PSB_WVDC32(0x00000000, PSB_INT_ENABLE_R);
	PSB_WVDC32(0xFFFFFFFF, PSB_INT_MASK_R);
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_irq_install(dev);

	dev->vblank_disable_allowed = 1;

	dev->max_vblank_count = 0xffffff; /* only 24 bits of frame count */

	dev->driver->get_vblank_counter = psb_get_vblank_counter;

#ifdef CONFIG_MDFLD_DSI_DPU
	/*init dpu info*/
	mdfld_dbi_dpu_init(dev);
#else
	mdfld_dbi_dsr_init(dev);
#endif /*CONFIG_MDFLD_DSI_DPU*/
	INIT_WORK(&dev_priv->te_work, mdfld_te_handler_work);

	/*must be after mrst_get_fuse_settings()*/
	ret = psb_backlight_init(dev);
	if (ret)
		return ret;

	if (drm_psb_no_fb == 0) {
		psb_modeset_init(dev);
		psb_fbdev_init(dev);
		drm_kms_helper_poll_init(dev);
		/* register HDMI hotplug interrupt
		 * handle after psb_fbdev_init
		 */
		android_hdmi_enable_hotplug(dev);
	}

	/*find handle to drm kboject*/
	pdev = dev->pdev;
	ddev = &pdev->dev;
	kobj = &ddev->kobj;

	dev_priv->psb_hotplug_state = psb_hotplug_init(kobj);

	// GL3
#ifdef CONFIG_MDFD_GL3
	gl3_enable();
#endif

	/*Intel drm driver load is done, continue doing pvr load*/
	DRM_DEBUG("Pvr driver load\n");

	return PVRSRVDrmLoad(dev, chipset);
out_err:
	psb_driver_unload(dev);
	return ret;
}

int psb_driver_device_is_agp(struct drm_device *dev)
{
	return 0;
}

int psb_extension_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	union drm_psb_extension_arg *arg = data;
	struct drm_psb_extension_rep *rep = &arg->rep;

	if (strcmp(arg->extension, "psb_ttm_placement_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_PLACEMENT_OFFSET;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}
	if (strcmp(arg->extension, "psb_ttm_fence_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_FENCE_OFFSET;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}
	if (strcmp(arg->extension, "psb_ttm_execbuf_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_CMDBUF;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	/*return the page flipping ioctl offset*/
	if (strcmp(arg->extension, "psb_page_flipping_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_FLIP;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	/* return the video rar offset */
	if (strcmp(arg->extension, "lnc_video_getparam") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_LNC_VIDEO_GETPARAM;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	/* return the video bind offset */
	if (strcmp(arg->extension, "psb_video_bind") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_ST_GFX_BUFFER_CLASS_VIDEO;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	rep->exists = 0;
	return 0;
}

static int psb_vt_leave_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_bo_device *bdev = &dev_priv->bdev;
#if 0	/* see REVISIT below */
	struct ttm_mem_type_manager *man;
	int clean;
#endif
	int ret;

	ret = ttm_vt_lock(&dev_priv->ttm_lock, 1,
			  psb_fpriv(file_priv)->tfile);
	if (unlikely(ret != 0))
		return ret;

	ret = ttm_bo_evict_mm(bdev, TTM_PL_TT);
	if (unlikely(ret != 0))
		goto out_unlock;


#if 0
	/*
	 * REVISIT: struct ttm_mem_type_manager no longer has manager field, but
	 * this has not been resolved, because the code only prints an
	 * informational message.
	 */
	man = &bdev->man[TTM_PL_TT];
	/*spin_lock(&bdev->lru_lock);*///lru_lock is removed from upstream TTM
	clean = drm_mm_clean(&man->manager);
	/*spin_unlock(&bdev->lru_lock);*/
	if (unlikely(!clean))
		DRM_INFO("Warning: GATT was not clean after VT switch.\n");
#endif

	ttm_bo_swapout_all(&dev_priv->bdev);

	return 0;
out_unlock:
	(void) ttm_vt_unlock(&dev_priv->ttm_lock);
	return ret;
}

static int psb_vt_enter_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	return ttm_vt_unlock(&dev_priv->ttm_lock);
}

static int psb_sizes_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_sizes_arg *arg =
		(struct drm_psb_sizes_arg *) data;

	*arg = dev_priv->sizes;
	return 0;
}

static int psb_fuse_reg_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;

	*arg = dev_priv->fuse_reg_value;
	return 0;
}
static int psb_vbt_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct gct_ioctl_arg *pGCT = data;

	memcpy(pGCT, &dev_priv->gct_data, sizeof(*pGCT));

	return 0;
}

static int psb_dc_state_ioctl(struct drm_device *dev, void * data,
			      struct drm_file *file_priv)
{
	return 0;
}

static int psb_dpst_bl_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;
	dev_priv->blc_adj2 = *arg;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct backlight_device bd;

	bd.props.brightness = psb_get_brightness(&bd);
	psb_set_brightness(&bd);
#endif
	return 0;
}

static int psb_adb_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;
	dev_priv->blc_adj1 = *arg;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct backlight_device bd;

	bd.props.brightness = psb_get_brightness(&bd);
	psb_set_brightness(&bd);
#endif
	return 0;
}

static int psb_hist_enable_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	u32 irqCtrl = 0;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct dpst_guardband guardband_reg;
	struct dpst_ie_histogram_control ie_hist_cont_reg;
	uint32_t *enable = data;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, true))
		return 0;

	if (*enable == 1) {
		ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ie_hist_cont_reg.ie_pipe_assignment = 0;
		ie_hist_cont_reg.histogram_mode_select = DPST_YUV_LUMA_MODE;
		ie_hist_cont_reg.ie_histogram_enable = 1;
		PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);

		guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		guardband_reg.interrupt_enable = 1;
		guardband_reg.interrupt_status = 1;
		PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

		irqCtrl = PSB_RVDC32(PSB_PIPESTAT(PSB_PIPE_A));
		PSB_WVDC32(irqCtrl | PIPE_DPST_EVENT_ENABLE,
			   PSB_PIPESTAT(PSB_PIPE_A));
		/* Wait for two vblanks */
	} else {
		guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		guardband_reg.interrupt_enable = 0;
		guardband_reg.interrupt_status = 1;
		PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

		ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ie_hist_cont_reg.ie_histogram_enable = 0;
		PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);

		irqCtrl = PSB_RVDC32(PSB_PIPESTAT(PSB_PIPE_A));
		irqCtrl &= ~PIPE_DPST_EVENT_ENABLE;
		PSB_WVDC32(irqCtrl, PSB_PIPESTAT(PSB_PIPE_A));
	}

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return 0;
}

static int psb_hist_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv		= psb_priv(dev);
	struct drm_psb_hist_status_arg *hist_status	= data;
	uint32_t *arg					= hist_status->buf;
	u32 iedbr_reg_data				= 0;
	struct dpst_ie_histogram_control ie_hist_cont_reg;
	u32 i;
	int dpst3_bin_threshold_count	= 0;
	uint32_t blm_hist_ctl		= HISTOGRAM_LOGIC_CONTROL;
	uint32_t iebdr_reg		= HISTOGRAM_BIN_DATA;
	uint32_t segvalue_max_22_bit	= 0x3fffff;
	uint32_t iedbr_busy_bit		= 0x80000000;
	int dpst3_bin_count		= 32;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false))
		return 0;

	ie_hist_cont_reg.data			= PSB_RVDC32(blm_hist_ctl);
	ie_hist_cont_reg.bin_reg_func_select	= dpst3_bin_threshold_count;
	ie_hist_cont_reg.bin_reg_index		= 0;

	PSB_WVDC32(ie_hist_cont_reg.data, blm_hist_ctl);

	for (i = 0; i < dpst3_bin_count; i++) {
		iedbr_reg_data = PSB_RVDC32(iebdr_reg);

		if (!(iedbr_reg_data & iedbr_busy_bit)) {
			arg[i] = iedbr_reg_data & segvalue_max_22_bit;
		} else {
			i = 0;
			ie_hist_cont_reg.data = PSB_RVDC32(blm_hist_ctl);
			ie_hist_cont_reg.bin_reg_index = 0;
			PSB_WVDC32(ie_hist_cont_reg.data, blm_hist_ctl);
		}
	}

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return 0;
}

static int psb_init_comm_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	if (*(int *)data == 1)
		psb_irq_enable_dpst(dev);
	else
		psb_irq_disable_dpst(dev);
	return 0;
}

/* return the current mode to the dpst module */
static int psb_dpst_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;
	uint32_t x;
	uint32_t y;
	uint32_t reg;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false))
		return 0;

	reg = PSB_RVDC32(PSB_PIPESRC(PSB_PIPE_A));

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	/* horizontal is the left 16 bits */
	x = reg >> 16;
	/* vertical is the right 16 bits */
	y = reg & 0x0000ffff;

	/* the values are the image size minus one */
	x += 1;
	y += 1;

	*arg = (x << 16) | y;

	return 0;
}
static int psb_gamma_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_dpst_lut_arg *lut_arg = data;
	struct drm_mode_object *obj;
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct psb_intel_crtc *psb_intel_crtc;
	int i = 0;
	int32_t obj_id;

	obj_id = lut_arg->output_id;
	obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_CONNECTOR);
	if (!obj) {
		DRM_DEBUG("Invalid Connector object.\n");
		return -EINVAL;
	}

	connector = obj_to_connector(obj);
	crtc = connector->encoder->crtc;
	psb_intel_crtc = to_psb_intel_crtc(crtc);

	for (i = 0; i < 256; i++)
		psb_intel_crtc->lut_adj[i] = lut_arg->lut[i];

	psb_intel_crtc_load_lut(crtc);

	return 0;
}

static int psb_update_guard_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct dpst_guardband* input = (struct dpst_guardband*) data;
	struct dpst_guardband reg_data;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false))
		return 0;

	reg_data.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
	reg_data.guardband = input->guardband;
	reg_data.guardband_interrupt_delay = input->guardband_interrupt_delay;
	/* printk(KERN_ALERT "guardband = %u\ninterrupt delay = %u\n",
		reg_data.guardband, reg_data.guardband_interrupt_delay); */
	PSB_WVDC32(reg_data.data, HISTOGRAM_INT_CONTROL);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return 0;
}

static int psb_mode_operation_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv)
{
	uint32_t obj_id;
	uint16_t op;
	struct drm_mode_modeinfo *umode;
	struct drm_display_mode *mode = NULL;
	struct drm_psb_mode_operation_arg *arg;
	struct drm_mode_object *obj;
	struct drm_connector *connector;
	struct drm_framebuffer * drm_fb;
	struct psb_framebuffer * psb_fb;
	struct drm_connector_helper_funcs *connector_funcs;
	int ret = 0;
	int resp = MODE_OK;
	struct drm_psb_private *dev_priv = psb_priv(dev);

	arg = (struct drm_psb_mode_operation_arg *)data;
	obj_id = arg->obj_id;
	op = arg->operation;

	switch (op) {
	case PSB_MODE_OPERATION_SET_DC_BASE:
		obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_FB);
		if (!obj) {
			DRM_ERROR("Invalid FB id %d\n", obj_id);
			return -EINVAL;
		}

		drm_fb = obj_to_fb(obj);
		psb_fb = to_psb_fb(drm_fb);

		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, false)) {
			REG_WRITE(PSB_DSPSURF(PSB_PIPE_A), psb_fb->offset);
			REG_READ(PSB_DSPSURF(PSB_PIPE_A));
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			dev_priv->pipe_regs[0].dsp_surf = psb_fb->offset;
		}

		return 0;
	case PSB_MODE_OPERATION_MODE_VALID:
		umode = &arg->mode;

		mutex_lock(&dev->mode_config.mutex);

		obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_CONNECTOR);
		if (!obj) {
			ret = -EINVAL;
			goto mode_op_out;
		}

		connector = obj_to_connector(obj);

		mode = drm_mode_create(dev);
		if (!mode) {
			ret = -ENOMEM;
			goto mode_op_out;
		}

		/* drm_crtc_convert_umode(mode, umode); */
		{
			mode->clock = umode->clock;
			mode->hdisplay = umode->hdisplay;
			mode->hsync_start = umode->hsync_start;
			mode->hsync_end = umode->hsync_end;
			mode->htotal = umode->htotal;
			mode->hskew = umode->hskew;
			mode->vdisplay = umode->vdisplay;
			mode->vsync_start = umode->vsync_start;
			mode->vsync_end = umode->vsync_end;
			mode->vtotal = umode->vtotal;
			mode->vscan = umode->vscan;
			mode->vrefresh = umode->vrefresh;
			mode->flags = umode->flags;
			mode->type = umode->type;
			strncpy(mode->name, umode->name, DRM_DISPLAY_MODE_LEN);
			mode->name[DRM_DISPLAY_MODE_LEN-1] = 0;
		}

		connector_funcs = (struct drm_connector_helper_funcs *)
				  connector->helper_private;

		if (connector_funcs->mode_valid) {
			resp = connector_funcs->mode_valid(connector, mode);
			arg->data = (void *)resp;
		}

		/*do some clean up work*/
		if (mode) {
			drm_mode_destroy(dev, mode);
		}
mode_op_out:
		mutex_unlock(&dev->mode_config.mutex);
		return ret;

	default:
		DRM_DEBUG("Unsupported psb mode operation");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int psb_stolen_memory_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_stolen_memory_arg *arg = data;

	arg->base = dev_priv->pg->stolen_base;
	arg->size = dev_priv->pg->vram_stolen_size;

	return 0;
}

static int psb_dpu_query_ioctl(struct drm_device *dev, void *arg,
			       struct drm_file *file_priv)
{
	IMG_INT *data = (IMG_INT*)arg;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;

	int panel_type;

	DRM_INFO("dsr query. \n");

	dev_priv->xserver_start = true;
	panel_type = is_panel_vid_or_cmd(dev);

	if (panel_type == MDFLD_DSI_ENCODER_DPI) {
		DRM_INFO("DSI panel is working in video mode\n");
		dev_priv->b_dsr_enable = false;
		*data = 0;
		return 0;
	}

#if defined(CONFIG_MDFLD_DSI_DSR)
	dev_priv->b_dsr_enable = true;
	*data = MDFLD_DSR_RR | MDFLD_DSR_FULLSCREEN;
#elif defined(CONFIG_MDFLD_DSI_DPU)
	dev_priv->b_dsr_enable = true;
	*data = MDFLD_DSR_RR | MDFLD_DPU_ENABLE;
#else /*DBI panel but DSR was not defined*/
	DRM_INFO("DSR is disabled by kernel configuration.\n");

	dev_priv->b_dsr_enable = false;
	*data = 0;
#endif /*CONFIG_MDFLD_DSI_DSR*/
	return 0;
}

static int psb_dpu_dsr_on_ioctl(struct drm_device *dev, void *arg,
				struct drm_file *file_priv)
{
	u32 * param = (u32 *)arg;
	struct drm_psb_private * dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	int panel_type;

	panel_type = is_panel_vid_or_cmd(dev);

	if (panel_type == MDFLD_DSI_ENCODER_DPI) {
		DRM_INFO("DSI panel is working in video mode\n");
		dev_priv->b_dsr_enable = false;
		return 0;
	}

	if (!param) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	PSB_DEBUG_ENTRY("dsr kick in. param 0x%08x\n", *param);

	if (*param == DRM_PSB_DSR_DISABLE) {
		PSB_DEBUG_ENTRY("DSR is turned off\n");
		dev_priv->b_dsr_enable = false;
#if defined(CONFIG_MDFLD_DSI_DPU)
		mdfld_dbi_dpu_report_fullscreen_damage(dev);
#elif defined(CONFIG_MDFLD_DSI_DSR)
		mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_2D_3D);
#endif
		return 0;
	} else if (*param == DRM_PSB_DSR_ENABLE) {
		PSB_DEBUG_ENTRY("DSR is turned on\n");
#if defined(CONFIG_MDFLD_DSI_DPU) || defined(CONFIG_MDFLD_DSI_DSR)
		dev_priv->b_dsr_enable = true;
#endif
		return 0;
	}

	return -EINVAL;
}

static int psb_dpu_dsr_off_ioctl(struct drm_device *dev, void *arg,
				 struct drm_file *file_priv)
{
	static int pipe = 0;
#if defined(CONFIG_MDFLD_DSI_DPU)
	struct drm_psb_drv_dsr_off_arg *dsr_off_arg = (struct drm_psb_drv_dsr_off_arg *) arg;
	struct psb_drm_dpu_rect rect = dsr_off_arg->damage_rect;

	return mdfld_dsi_dbi_dsr_off(dev, &rect);
#elif defined(CONFIG_MDFLD_DSI_DSR)
	struct drm_psb_private * dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	pipe++;

	if ((dev_priv->dsr_fb_update & MDFLD_DSR_2D_3D) != MDFLD_DSR_2D_3D) {
		mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_2D_3D);
	}

	if (pipe > 0) {
		pipe = 0;
		if (gdbi_output) {
			dev_priv->b_dsr_enable = true;
			mdfld_dsi_dbi_enter_dsr(gdbi_output, 1);
			mdfld_dsi_dbi_enter_dsr(gdbi_output, 2);
		}
	}

#endif
	return 0;
}

static int psb_register_rw_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_register_rw_arg *arg = data;
	unsigned int iep_ble_status;
	unsigned long iep_timeout;
	bool force_on = arg->b_force_hw_on;
	struct psb_pipe_regs *pipe_regs = dev_priv->pipe_regs;

	if (arg->display_write_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			if (arg->display_write_mask & REGRWBITS_PFIT_CONTROLS)
				PSB_WVDC32(arg->display.pfit_controls,
					   PFIT_CONTROL);
			if (arg->display_write_mask &
			    REGRWBITS_PFIT_AUTOSCALE_RATIOS)
				PSB_WVDC32(arg->display.pfit_autoscale_ratios,
					   PFIT_AUTO_RATIOS);
			if (arg->display_write_mask &
			    REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS)
				PSB_WVDC32(
					arg->display.pfit_programmed_scale_ratios,
					PFIT_PGM_RATIOS);
			if (arg->display_write_mask & REGRWBITS_PIPEASRC)
				PSB_WVDC32(arg->display.pipeasrc,
					   PSB_PIPESRC(PSB_PIPE_A));
			if (arg->display_write_mask & REGRWBITS_PIPEBSRC)
				PSB_WVDC32(arg->display.pipebsrc,
					   PSB_PIPESRC(PSB_PIPE_B));
			if (arg->display_write_mask & REGRWBITS_VTOTAL_A)
				PSB_WVDC32(arg->display.vtotal_a,
					   PSB_VTOTAL(PSB_PIPE_A));
			if (arg->display_write_mask & REGRWBITS_VTOTAL_B)
				PSB_WVDC32(arg->display.vtotal_b,
					   PSB_VTOTAL(PSB_PIPE_B));
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			if (arg->display_write_mask & REGRWBITS_PFIT_CONTROLS)
				dev_priv->savePFIT_CONTROL =
					arg->display.pfit_controls;
			if (arg->display_write_mask &
			    REGRWBITS_PFIT_AUTOSCALE_RATIOS)
				dev_priv->savePFIT_AUTO_RATIOS =
					arg->display.pfit_autoscale_ratios;
			if (arg->display_write_mask &
			    REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS)
				dev_priv->savePFIT_PGM_RATIOS =
					arg->display.pfit_programmed_scale_ratios;
			if (arg->display_write_mask & REGRWBITS_PIPEASRC)
				pipe_regs[0].src = arg->display.pipeasrc;
			if (arg->display_write_mask & REGRWBITS_PIPEBSRC)
				pipe_regs[1].src = arg->display.pipebsrc;
			if (arg->display_write_mask & REGRWBITS_VTOTAL_A)
				pipe_regs[0].vtotal = arg->display.vtotal_a;
			if (arg->display_write_mask & REGRWBITS_VTOTAL_B)
				pipe_regs[1].vtotal = arg->display.vtotal_b;
		}
	}

	if (arg->display_read_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_CONTROLS)
				arg->display.pfit_controls =
					PSB_RVDC32(PFIT_CONTROL);
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_AUTOSCALE_RATIOS)
				arg->display.pfit_autoscale_ratios =
					PSB_RVDC32(PFIT_AUTO_RATIOS);
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS)
				arg->display.pfit_programmed_scale_ratios =
					PSB_RVDC32(PFIT_PGM_RATIOS);
			if (arg->display_read_mask & REGRWBITS_PIPEASRC)
				arg->display.pipeasrc = \
					PSB_RVDC32(PSB_PIPESRC(PSB_PIPE_A));
			if (arg->display_read_mask & REGRWBITS_PIPEBSRC)
				arg->display.pipebsrc = \
					PSB_RVDC32(PSB_PIPESRC(PSB_PIPE_B));
			if (arg->display_read_mask & REGRWBITS_VTOTAL_A)
				arg->display.vtotal_a = \
					PSB_RVDC32(PSB_VTOTAL(PSB_PIPE_A));
			if (arg->display_read_mask & REGRWBITS_VTOTAL_B)
				arg->display.vtotal_b = \
					PSB_RVDC32(PSB_VTOTAL(PSB_PIPE_B));
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_CONTROLS)
				arg->display.pfit_controls =
					dev_priv->savePFIT_CONTROL;
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_AUTOSCALE_RATIOS)
				arg->display.pfit_autoscale_ratios =
					dev_priv->savePFIT_AUTO_RATIOS;
			if (arg->display_read_mask &
			    REGRWBITS_PFIT_PROGRAMMED_SCALE_RATIOS)
				arg->display.pfit_programmed_scale_ratios =
					dev_priv->savePFIT_PGM_RATIOS;
			if (arg->display_read_mask & REGRWBITS_PIPEASRC)
				arg->display.pipeasrc = pipe_regs[0].src;
			if (arg->display_read_mask & REGRWBITS_PIPEBSRC)
				arg->display.pipebsrc = pipe_regs[1].src;
			if (arg->display_read_mask & REGRWBITS_VTOTAL_A)
				arg->display.vtotal_a = pipe_regs[0].vtotal;
			if (arg->display_read_mask & REGRWBITS_VTOTAL_B)
				arg->display.vtotal_b = pipe_regs[1].vtotal;
		}
	}

	if (arg->overlay_write_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			if (arg->overlay_write_mask & OV_REGRWBITS_OGAM_ALL) {
				PSB_WVDC32(arg->overlay.OGAMC5, OV_OGAMC5);
				PSB_WVDC32(arg->overlay.OGAMC4, OV_OGAMC4);
				PSB_WVDC32(arg->overlay.OGAMC3, OV_OGAMC3);
				PSB_WVDC32(arg->overlay.OGAMC2, OV_OGAMC2);
				PSB_WVDC32(arg->overlay.OGAMC1, OV_OGAMC1);
				PSB_WVDC32(arg->overlay.OGAMC0, OV_OGAMC0);
			}
			if (arg->overlay_write_mask & OVC_REGRWBITS_OGAM_ALL) {
				PSB_WVDC32(arg->overlay.OGAMC5, OVC_OGAMC5);
				PSB_WVDC32(arg->overlay.OGAMC4, OVC_OGAMC4);
				PSB_WVDC32(arg->overlay.OGAMC3, OVC_OGAMC3);
				PSB_WVDC32(arg->overlay.OGAMC2, OVC_OGAMC2);
				PSB_WVDC32(arg->overlay.OGAMC1, OVC_OGAMC1);
				PSB_WVDC32(arg->overlay.OGAMC0, OVC_OGAMC0);
			}

			if (arg->overlay_write_mask & OV_REGRWBITS_OVADD) {
				PSB_WVDC32(arg->overlay.OVADD, OV_OVADD);

				if (arg->overlay.b_wait_vblank) {
					/*Wait for 20ms.*/
					unsigned long vblank_timeout = jiffies + HZ / 50;
					uint32_t temp;
					while (time_before_eq(jiffies, vblank_timeout)) {
						temp = PSB_RVDC32(OV_DOVASTA);
						if ((temp & (0x1 << 31)) != 0) {
							break;
						}
						cpu_relax();
					}
				}

				if ((((arg->overlay.OVADD & OV_PIPE_SELECT) >> OV_PIPE_SELECT_POS) == OV_PIPE_A)) {
#ifndef CONFIG_MDFLD_DSI_DPU
					mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_OVERLAY_0);
#else
					/*TODO: report overlay damage*/
#endif
				}

				if ((((arg->overlay.OVADD & OV_PIPE_SELECT) >> OV_PIPE_SELECT_POS) == OV_PIPE_C)) {
#ifndef CONFIG_MDFLD_DSI_DPU
					mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_OVERLAY_2);
#else
					/*TODO: report overlay damage*/
#endif
				}

				if (arg->overlay.IEP_ENABLED) {
					/* VBLANK period */
					iep_timeout = jiffies + HZ / 10;
					do {
						iep_ble_status = PSB_RVDC32(0x31800);
						if (time_after_eq(jiffies, iep_timeout)) {
							DRM_ERROR("IEP Lite timeout\n");
							break;
						}
						cpu_relax();
					} while ((iep_ble_status >> 1) != 1);

					arg->overlay.IEP_BLE_MINMAX    = PSB_RVDC32(0x31804);
					arg->overlay.IEP_BSSCC_CONTROL = PSB_RVDC32(0x32000);
				}
			}
			if (arg->overlay_write_mask & OVC_REGRWBITS_OVADD) {
				PSB_WVDC32(arg->overlay.OVADD, OVC_OVADD);
				if (arg->overlay.b_wait_vblank) {
					/*Wait for 20ms.*/
					unsigned long vblank_timeout = jiffies + HZ / 50;
					uint32_t temp;
					while (time_before_eq(jiffies, vblank_timeout)) {
						temp = PSB_RVDC32(OVC_DOVCSTA);
						if ((temp & (0x1 << 31)) != 0) {
							break;
						}
						cpu_relax();
					}
				}
			}
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			if (arg->overlay_write_mask & OV_REGRWBITS_OGAM_ALL) {
				dev_priv->saveOV_OGAMC5 = arg->overlay.OGAMC5;
				dev_priv->saveOV_OGAMC4 = arg->overlay.OGAMC4;
				dev_priv->saveOV_OGAMC3 = arg->overlay.OGAMC3;
				dev_priv->saveOV_OGAMC2 = arg->overlay.OGAMC2;
				dev_priv->saveOV_OGAMC1 = arg->overlay.OGAMC1;
				dev_priv->saveOV_OGAMC0 = arg->overlay.OGAMC0;
			}
			if (arg->overlay_write_mask & OVC_REGRWBITS_OGAM_ALL) {
				dev_priv->saveOVC_OGAMC5 = arg->overlay.OGAMC5;
				dev_priv->saveOVC_OGAMC4 = arg->overlay.OGAMC4;
				dev_priv->saveOVC_OGAMC3 = arg->overlay.OGAMC3;
				dev_priv->saveOVC_OGAMC2 = arg->overlay.OGAMC2;
				dev_priv->saveOVC_OGAMC1 = arg->overlay.OGAMC1;
				dev_priv->saveOVC_OGAMC0 = arg->overlay.OGAMC0;
			}
			if (arg->overlay_write_mask & OV_REGRWBITS_OVADD)
				dev_priv->saveOV_OVADD = arg->overlay.OVADD;
			if (arg->overlay_write_mask & OVC_REGRWBITS_OVADD)
				dev_priv->saveOVC_OVADD = arg->overlay.OVADD;
		}
	}

	if (arg->overlay_read_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			if (arg->overlay_read_mask & OV_REGRWBITS_OGAM_ALL) {
				arg->overlay.OGAMC5 = PSB_RVDC32(OV_OGAMC5);
				arg->overlay.OGAMC4 = PSB_RVDC32(OV_OGAMC4);
				arg->overlay.OGAMC3 = PSB_RVDC32(OV_OGAMC3);
				arg->overlay.OGAMC2 = PSB_RVDC32(OV_OGAMC2);
				arg->overlay.OGAMC1 = PSB_RVDC32(OV_OGAMC1);
				arg->overlay.OGAMC0 = PSB_RVDC32(OV_OGAMC0);
			}
			if (arg->overlay_read_mask & OVC_REGRWBITS_OGAM_ALL) {
				arg->overlay.OGAMC5 = PSB_RVDC32(OVC_OGAMC5);
				arg->overlay.OGAMC4 = PSB_RVDC32(OVC_OGAMC4);
				arg->overlay.OGAMC3 = PSB_RVDC32(OVC_OGAMC3);
				arg->overlay.OGAMC2 = PSB_RVDC32(OVC_OGAMC2);
				arg->overlay.OGAMC1 = PSB_RVDC32(OVC_OGAMC1);
				arg->overlay.OGAMC0 = PSB_RVDC32(OVC_OGAMC0);
			}
			if (arg->overlay_read_mask & OV_REGRWBITS_OVADD)
				arg->overlay.OVADD = PSB_RVDC32(OV_OVADD);
			if (arg->overlay_read_mask & OVC_REGRWBITS_OVADD)
				arg->overlay.OVADD = PSB_RVDC32(OVC_OVADD);
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		} else {
			if (arg->overlay_read_mask & OV_REGRWBITS_OGAM_ALL) {
				arg->overlay.OGAMC5 = dev_priv->saveOV_OGAMC5;
				arg->overlay.OGAMC4 = dev_priv->saveOV_OGAMC4;
				arg->overlay.OGAMC3 = dev_priv->saveOV_OGAMC3;
				arg->overlay.OGAMC2 = dev_priv->saveOV_OGAMC2;
				arg->overlay.OGAMC1 = dev_priv->saveOV_OGAMC1;
				arg->overlay.OGAMC0 = dev_priv->saveOV_OGAMC0;
			}
			if (arg->overlay_read_mask & OVC_REGRWBITS_OGAM_ALL) {
				arg->overlay.OGAMC5 = dev_priv->saveOVC_OGAMC5;
				arg->overlay.OGAMC4 = dev_priv->saveOVC_OGAMC4;
				arg->overlay.OGAMC3 = dev_priv->saveOVC_OGAMC3;
				arg->overlay.OGAMC2 = dev_priv->saveOVC_OGAMC2;
				arg->overlay.OGAMC1 = dev_priv->saveOVC_OGAMC1;
				arg->overlay.OGAMC0 = dev_priv->saveOVC_OGAMC0;
			}
			if (arg->overlay_read_mask & OV_REGRWBITS_OVADD)
				arg->overlay.OVADD = dev_priv->saveOV_OVADD;
			if (arg->overlay_read_mask & OVC_REGRWBITS_OVADD)
				arg->overlay.OVADD = dev_priv->saveOVC_OVADD;
		}
	}

	if (arg->sprite_enable_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			PSB_WVDC32(0x1F3E, DSPARB);
			PSB_WVDC32(arg->sprite.dspa_control |
					PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_A)),
				   PSB_DSPCNTR(PSB_PIPE_A));
			PSB_WVDC32(arg->sprite.dspa_key_value, DSPAKEYVAL);
			PSB_WVDC32(arg->sprite.dspa_key_mask, DSPAKEYMASK);
			PSB_WVDC32(PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A)),
				   PSB_DSPSURF(PSB_PIPE_A));
			PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A));
			PSB_WVDC32(arg->sprite.dspc_control,
				   PSB_DSPCNTR(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_stride,
				   PSB_DSPSTRIDE(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_position,
				   PSB_DSPPOS(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_linear_offset,
				   PSB_DSPLINOFF(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_size,
				   PSB_DSPSIZE(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_surface,
				   PSB_DSPSURF(PSB_PIPE_C));
			PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}

	if (arg->sprite_disable_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			PSB_WVDC32(0x3F3E, DSPARB);
			PSB_WVDC32(0x0, PSB_DSPCNTR(PSB_PIPE_C));
			PSB_WVDC32(arg->sprite.dspc_surface,
				   PSB_DSPSURF(PSB_PIPE_C));
			PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}

	if (arg->subpicture_enable_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			uint32_t temp;
			if (arg->subpicture_enable_mask & REGRWBITS_DSPACNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_A));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp &= ~DISPPLANE_BOTTOM;
				temp |= DISPPLANE_32BPP;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_A));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_A));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_A));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_A));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_A));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A));
			}
			if (arg->subpicture_enable_mask & REGRWBITS_DSPBCNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_B));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp &= ~DISPPLANE_BOTTOM;
				temp |= DISPPLANE_32BPP;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_B));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_B));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_B));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_B));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_B));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_B));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_B));
			}
			if (arg->subpicture_enable_mask & REGRWBITS_DSPCCNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_C));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp &= ~DISPPLANE_BOTTOM;
				temp |= DISPPLANE_32BPP;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_C));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_C));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_C));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_C));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_C));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
			}
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}

	if (arg->subpicture_disable_mask != 0) {
		if (ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, force_on)) {
			uint32_t temp;
			if (arg->subpicture_disable_mask & REGRWBITS_DSPACNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_A));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp |= DISPPLANE_32BPP_NO_ALPHA;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_A));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_A));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_A));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_A));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_A));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_A));
			}
			if (arg->subpicture_disable_mask & REGRWBITS_DSPBCNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_B));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp |= DISPPLANE_32BPP_NO_ALPHA;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_B));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_B));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_B));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_B));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_B));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_B));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_B));
			}
			if (arg->subpicture_disable_mask & REGRWBITS_DSPCCNTR) {
				temp =  PSB_RVDC32(PSB_DSPCNTR(PSB_PIPE_C));
				temp &= ~DISPPLANE_PIXFORMAT_MASK;
				temp |= DISPPLANE_32BPP_NO_ALPHA;
				PSB_WVDC32(temp, PSB_DSPCNTR(PSB_PIPE_C));

				temp =  PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_C));
				PSB_WVDC32(temp, PSB_DSPBASE(PSB_PIPE_C));
				PSB_RVDC32(PSB_DSPBASE(PSB_PIPE_C));
				temp =  PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
				PSB_WVDC32(temp, PSB_DSPSURF(PSB_PIPE_C));
				PSB_RVDC32(PSB_DSPSURF(PSB_PIPE_C));
			}
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		}
	}

	return 0;
}

static int psb_driver_open(struct drm_device *dev, struct drm_file *priv)
{
	DRM_DEBUG("\n");
	return PVRSRVOpen(dev, priv);
}

static long psb_unlocked_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	unsigned int nr = DRM_IOCTL_NR(cmd);
	long ret;

	DRM_DEBUG("cmd = %x, nr = %x\n", cmd, nr);

	/*
	 * The driver private ioctls and TTM ioctls should be
	 * thread-safe.
	 */

	if ((nr >= DRM_COMMAND_BASE) && (nr < DRM_COMMAND_END)
	    && (nr < DRM_COMMAND_BASE + dev->driver->num_ioctls)) {
		struct drm_ioctl_desc *ioctl =
					&psb_ioctls[nr - DRM_COMMAND_BASE];

		if (unlikely(ioctl->cmd != cmd)) {
			DRM_ERROR(
				"Invalid drm cmnd %d ioctl->cmd %x, cmd %x\n",
				nr - DRM_COMMAND_BASE, ioctl->cmd, cmd);
			return -EINVAL;
		}
	}
	/*
	 * Not all old drm ioctls are thread-safe.
	 */

	/* FIXME: lock_kernel(); */
	ret = drm_ioctl(filp, cmd, arg);
	/* FIXME: unlock_kernel(); */
	return ret;
}

/* When a client dies:
 *    - Check for and clean up flipped page state
 */
void psb_driver_preclose(struct drm_device *dev, struct drm_file *priv)
{
}

static void psb_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	drm_put_dev(dev);
}

static const struct dev_pm_ops psb_pm_ops = {
	.runtime_suspend = psb_runtime_suspend,
	.runtime_resume = psb_runtime_resume,
	.runtime_idle = psb_runtime_idle,
	.resume = ospm_power_resume,
	.suspend = ospm_power_suspend,
};

static struct drm_driver driver = {
	.driver_features = DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED | \
	DRIVER_IRQ_VBL | DRIVER_MODESET,
	.load = psb_driver_load,
	.unload = psb_driver_unload,

	.ioctls = psb_ioctls,
	.num_ioctls = DRM_ARRAY_SIZE(psb_ioctls),
	.device_is_agp = psb_driver_device_is_agp,
	.irq_preinstall = psb_irq_preinstall,
	.irq_postinstall = psb_irq_postinstall,
	.irq_uninstall = psb_irq_uninstall,
	.irq_handler = psb_irq_handler,
	.enable_vblank = psb_enable_vblank,
	.disable_vblank = psb_disable_vblank,
	.get_vblank_counter = psb_get_vblank_counter,
	.firstopen = NULL,
	.lastclose = psb_lastclose,
	.open = psb_driver_open,
	.postclose = PVRSRVDrmPostClose,
	.suspend = PVRSRVDriverSuspend,
	.resume = PVRSRVDriverResume,
	.preclose = psb_driver_preclose,
	.fops = {
		.owner = THIS_MODULE,
		.open = psb_open,
		.release = psb_release,
		.unlocked_ioctl = psb_unlocked_ioctl,
		.mmap = psb_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
		.read = drm_read,
	},
	.name = DRIVER_NAME,
	 .desc = DRIVER_DESC,
	  .date = PSB_DRM_DRIVER_DATE,
	   .major = PSB_DRM_DRIVER_MAJOR,
	    .minor = PSB_DRM_DRIVER_MINOR,
	     .patchlevel = PSB_DRM_DRIVER_PATCHLEVEL
		   };

static struct pci_driver psb_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = psb_probe,
	.remove = psb_remove,
#ifdef CONFIG_PM
	.driver.pm = &psb_pm_ops,
#endif
};

static int psb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	if (pci_enable_msi(pdev))
		DRM_ERROR("Enable MSI failed!\n");
	return drm_get_pci_dev(pdev, ent, &driver);
}

#ifndef MODULE
static __init int parse_panelid(char *arg)
{
	/* panel ID can be passed in as a cmdline parameter */
	/* to enable this feature add panelid=TMD to cmdline for TMD panel*/
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "TMD_CMD"))
		PanelID = TMD_CMD;
	else if (!strcasecmp(arg, "TPO_CMD"))
		PanelID = TPO_CMD;
	else if (!strcasecmp(arg, "TMD_VID"))
		PanelID = TMD_VID;
	else if (!strcasecmp(arg, "TPO_VID"))
		PanelID = TPO_VID;
	else if (!strcasecmp(arg, "TC35876X"))
		PanelID = TC35876X;
	else
		PanelID = GCT_DETECT;

	return 0;
}
early_param("panelid", parse_panelid);
#endif

#ifndef MODULE
static __init int parse_hdmi_edid(char *arg)
{
	/* HDMI EDID info can be passed in as a cmdline parameter,
	 * and need to remove it after we can get EDID info via MSIC.*/
	if ((!arg) || (strlen(arg) >= 20))
		return -EINVAL;

	strcpy(HDMI_EDID, arg);

	return 0;
}
early_param("hdmi_edid", parse_hdmi_edid);
#endif

static int __init psb_init(void)
{
	int ret;

#if defined(MODULE) && defined(CONFIG_NET)
	psb_kobject_uevent_init();
#endif

	ret = SYSPVRInit();
	if (ret != 0) {
		return ret;
	}

	ret = drm_pci_init(&driver, &psb_pci_driver);
	if (ret != 0) {
		return ret;
	}

	/*init for bc_video*/
	ret = BC_Video_ModInit();
	if (ret != 0) {
		return ret;
	}

	return ret;
}

static void __exit psb_exit(void)
{
	int ret;
	/*cleanup for bc_video*/
	ret = BC_Video_ModCleanup();
	if (ret != 0) {
		return;
	}
	drm_pci_exit(&driver, &psb_pci_driver);
}

late_initcall(psb_init);
module_exit(psb_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

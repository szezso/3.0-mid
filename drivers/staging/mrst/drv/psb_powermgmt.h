/**************************************************************************
 * Copyright (c) 2009, Intel Corporation.
 * All Rights Reserved.

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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Benjamin Defnet <benjamin.r.defnet@intel.com>
 *    Rajesh Poornachandran <rajesh.poornachandran@intel.com>
 *
 */
#ifndef _PSB_POWERMGMT_H_
#define _PSB_POWERMGMT_H_

#include <linux/pci.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <linux/intel_mid_pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif

#define OSPM_GRAPHICS_ISLAND	0x1
#define OSPM_VIDEO_ENC_ISLAND	0x2
#define OSPM_VIDEO_DEC_ISLAND	0x4
#define OSPM_DISPLAY_ISLAND	0x8
#define OSPM_GL3_CACHE_ISLAND	0x10
#define OSPM_ALL_ISLANDS	(OSPM_GRAPHICS_ISLAND | OSPM_VIDEO_ENC_ISLAND \
				| OSPM_VIDEO_DEC_ISLAND | OSPM_DISPLAY_ISLAND \
				| OSPM_GL3_CACHE_ISLAND)

/* IPC message and command defines used to enable/disable mipi panel voltages */
#define IPC_MSG_PANEL_ON_OFF    0xE9
#define IPC_CMD_PANEL_ON        1
#define IPC_CMD_PANEL_OFF       0

//extern int psb_check_msvdx_idle(struct drm_device *dev);
//extern int lnc_check_topaz_idle(struct drm_device *dev);
/* Use these functions to power down video HW for D0i3 purpose  */
void ospm_apm_power_down_msvdx(struct drm_device *dev);
void ospm_apm_power_down_topaz(struct drm_device *dev);

void ospm_power_init(struct drm_device *dev);
void ospm_power_uninit(struct drm_device *dev);


/*
 * OSPM will call these functions
 */
int ospm_power_suspend(struct device *dev);
int ospm_power_resume(struct device *dev);

/*
 * These are the functions the driver should use to wrap all hw access
 * (i.e. register reads and writes)
 */
bool ospm_power_using_hw_begin(int hw_island, bool force_on);
bool ospm_power_using_hw_begin_atomic(int hw_island); /* force_on == false */
void ospm_power_using_hw_end(int hw_island);

/*
 * Use this function to do an instantaneous check for if the hw is on.
 * Only use this in cases where you know the g_state_change_mutex
 * is already held such as in irq install/uninstall and you need to
 * prevent a deadlock situation.  Otherwise use ospm_power_using_hw_begin().
 */
bool ospm_power_is_hw_on(int hw_islands);

/*
 * Power up/down different hw component rails/islands
 */
void ospm_power_island_down(int hw_islands);
void ospm_power_island_up(int hw_islands);
/*
 * GFX-Runtime PM callbacks
 */
int psb_runtime_suspend(struct device *dev);
int psb_runtime_resume(struct device *dev);
int psb_runtime_idle(struct device *dev);

extern struct drm_device *gpDrmDevice;

#endif /*_PSB_POWERMGMT_H_*/

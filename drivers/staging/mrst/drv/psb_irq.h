/**************************************************************************
 * Copyright (c) 2009, Intel Corporation.
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
 * Authors:
 *    Benjamin Defnet <benjamin.r.defnet@intel.com>
 *    Rajesh Poornachandran <rajesh.poornachandran@intel.com>
 *
 **************************************************************************/

#ifndef _PSB_IRQ_H_
#define _PSB_IRQ_H_

#include <drm/drmP.h>
#include "psb_drv.h"

void psb_irq_preinstall(struct drm_device *dev);
int  psb_irq_postinstall(struct drm_device *dev);
void psb_irq_uninstall(struct drm_device *dev);
irqreturn_t psb_irq_handler(DRM_IRQ_ARGS);

void psb_irq_preinstall_islands(struct drm_device *dev, int hw_islands);
int  psb_irq_postinstall_islands(struct drm_device *dev, int hw_islands);
void psb_irq_uninstall_islands(struct drm_device *dev, int hw_islands);

int psb_irq_enable_dpst(struct drm_device *dev);
int psb_irq_disable_dpst(struct drm_device *dev);
void psb_irq_turn_on_dpst(struct drm_device *dev);
void psb_irq_turn_off_dpst(struct drm_device *dev);
int  psb_enable_vblank(struct drm_device *dev, int pipe);
void psb_disable_vblank(struct drm_device *dev, int pipe);
u32  psb_get_vblank_counter(struct drm_device *dev, int pipe);

void psb_enable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);
void psb_disable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);

int mdfld_enable_te(struct drm_device *dev, int pipe);
void mdfld_disable_te(struct drm_device *dev, int pipe);
int mdfld_irq_enable_hdmi_audio(struct drm_device *dev);
int mdfld_irq_disable_hdmi_audio(struct drm_device *dev);
void psb_te_timer_func(unsigned long data);
void mdfld_te_handler_work(struct work_struct *te_work);

#endif /* _PSB_IRQ_H_ */

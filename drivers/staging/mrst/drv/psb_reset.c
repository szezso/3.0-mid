/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
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
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 **************************************************************************/

#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_reg.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include <linux/spinlock.h>

void psb_msvdx_flush_cmd_queue(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_msvdx_cmd_queue *msvdx_cmd;
	struct list_head *list, *next;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	/*Flush the msvdx cmd queue and signal all fences in the queue */
	list_for_each_safe(list, next, &msvdx_priv->msvdx_queue) {
		msvdx_cmd =
			list_entry(list, struct psb_msvdx_cmd_queue, head);
		PSB_DEBUG_GENERAL("MSVDXQUE: flushing sequence:%d\n",
				  msvdx_cmd->sequence);
		msvdx_priv->msvdx_current_sequence = msvdx_cmd->sequence;
		psb_fence_error(dev, PSB_ENGINE_VIDEO,
				msvdx_priv->msvdx_current_sequence,
				_PSB_FENCE_TYPE_EXE, DRM_CMD_HANG);
		list_del(list);
		kfree(msvdx_cmd->cmd);
		kfree(msvdx_cmd
		     );
	}
}

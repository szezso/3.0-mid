/*
 * Copyright (c) 2011, Intel Corporation.
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
 * this program; if not, write to the Free Software Foundation, Inc.,·
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 * Ander Conselvan de Oliveira <ander.conselvan.de.oliveira@intel.com>
 *
 */

#ifndef _OSSYNC_H_
#define _OSSYNC_H_

#include "servicesint.h"

#define PVRSRV_SYNC_READ	(1 << 0)
#define PVRSRV_SYNC_WRITE	(1 << 1)

#define pvr_ops_after(a, b) ((s32)(b) - (s32)(a) < 0)

struct pvr_pending_sync;

typedef void (*pvr_sync_callback)(struct pvr_pending_sync *pending_sync, bool from_misr);

struct pvr_pending_sync {
	PVRSRV_KERNEL_SYNC_INFO *sync_info;
	u32 pending_read_ops;
	u32 pending_write_ops;
	unsigned int flags;
	pvr_sync_callback callback;
	struct list_head list;
};

void PVRSRVCallbackOnSync(PVRSRV_KERNEL_SYNC_INFO *sync_info, unsigned int flags,
			  pvr_sync_callback callback,
			  struct pvr_pending_sync *pending_sync);
void PVRSRVCallbackOnSync2(struct pvr_pending_sync *pending_sync);
void PVRSRVCheckPendingSyncs(void);

#endif /* _OSSYNC_H_ */

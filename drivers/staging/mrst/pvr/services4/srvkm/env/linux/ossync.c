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
 * Pauli Nieminen <pauli.nieminen@intel.com>
 *
 */

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include "ossync.h"
#include "servicesint.h"

static DEFINE_SPINLOCK(sync_lock);
static LIST_HEAD(sync_list);

static bool pending_ops_completed(PVRSRV_KERNEL_SYNC_INFO *sync_info,
				  unsigned int flags,
				  u32 pending_read_ops,
				  u32 pending_write_ops)
{
	if (flags & PVRSRV_SYNC_READ &&
	    pvr_ops_after(pending_read_ops, sync_info->psSyncData->ui32ReadOpsComplete))
		return false;

	if (flags & PVRSRV_SYNC_WRITE &&
	    pvr_ops_after(pending_write_ops, sync_info->psSyncData->ui32WriteOpsComplete))
		return false;

	return true;
}

void
PVRSRVCallbackOnSync2(struct pvr_pending_sync *pending_sync)
{
	bool complete = false;
	unsigned long flags;

	spin_lock_irqsave(&sync_lock, flags);

	/* If the object is already in sync, don't add it to the list */
	if (!pending_ops_completed(pending_sync->sync_info,
				  pending_sync->flags,
				  pending_sync->pending_read_ops,
				  pending_sync->pending_write_ops))
		list_add_tail(&pending_sync->list, &sync_list);
	else
		complete = true;

	spin_unlock_irqrestore(&sync_lock, flags);

	if (complete)
		pending_sync->callback(pending_sync, false);
}

/* Returns 0 if the callback was successfully registered.
 * Returns a negative value on error.
 */
void
PVRSRVCallbackOnSync(PVRSRV_KERNEL_SYNC_INFO *sync_info,
		     unsigned int flags,
		     pvr_sync_callback callback,
		     struct pvr_pending_sync *pending_sync)
{
	u32 pending_read_ops = sync_info->psSyncData->ui32ReadOpsPending;
	u32 pending_write_ops = sync_info->psSyncData->ui32WriteOpsPending;

	pending_sync->sync_info = sync_info;
	pending_sync->pending_read_ops = pending_read_ops;
	pending_sync->pending_write_ops = pending_write_ops;
	pending_sync->flags = flags;
	pending_sync->callback = callback;

	PVRSRVCallbackOnSync2(pending_sync);

	return;
}

void
PVRSRVCheckPendingSyncs(void)
{
	struct pvr_pending_sync *ps, *tmp;
	unsigned long flags;
	LIST_HEAD(completed_list);

	/* Pull the completed objects from the list. */
	spin_lock_irqsave(&sync_lock, flags);

	list_for_each_entry_safe(ps, tmp, &sync_list, list) {
		if (pending_ops_completed(ps->sync_info, ps->flags,
					  ps->pending_read_ops,
					  ps->pending_write_ops)) {
			list_move_tail(&ps->list, &completed_list);
		}
	}

	spin_unlock_irqrestore(&sync_lock, flags);

	/* Execute the callbacks */
	list_for_each_entry_safe(ps, tmp, &completed_list, list)
		ps->callback(ps, true);
}


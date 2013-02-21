/*
 * Copyright (C) 2011 Nokia Corporation
 * Copyright (C) 2011 Intel Corporation
 * Author: Imre Deak <imre.deak@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __PVR_TRACE_CMD_H__
#define __PVR_TRACE_CMD_H__

#include <linux/mutex.h>

#include "servicesint.h"
#include "sgxapi_km.h"

enum pvr_trcmd_type {
	PVR_TRCMD_PAD,
	PVR_TRCMD_SGX_FIRSTKICK,
	PVR_TRCMD_SGX_KICK,
	PVR_TRCMD_SGX_TFER_KICK,
	PVR_TRCMD_SGX_COMP,
	PVR_TRCMD_SYN_REMOVE,
	PVR_TRCMD_SGX_QBLT_SYNCHK,
	PVR_TRCMD_FLPREQ,
	PVR_TRCMD_FLPCOMP,
	PVR_TRCMD_SUSPEND,
	PVR_TRCMD_RESUME,
};

struct pvr_trcmd_buf;

struct pvr_trcmd_syn {
	unsigned long	rd_pend;
	unsigned long	rd_comp;
	unsigned long	wr_pend;
	unsigned long	wr_comp;
	unsigned long	addr;
};

struct pvr_trcmd_sgxkick {
	struct pvr_trcmd_syn	tatq_syn;
	struct pvr_trcmd_syn	_3dtq_syn;
	struct pvr_trcmd_syn	src_syn[SGX_MAX_SRC_SYNCS];
	struct pvr_trcmd_syn	dst_syn;
	struct pvr_trcmd_syn	ta3d_syn;
	unsigned long		ctx;
};

struct pvr_trcmd_sgxtransfer {
	struct pvr_trcmd_syn	ta_syn;
	struct pvr_trcmd_syn	_3d_syn;
	struct pvr_trcmd_syn	src_syn[SGX_MAX_TRANSFER_SYNC_OPS];
	struct pvr_trcmd_syn	dst_syn[SGX_MAX_TRANSFER_SYNC_OPS];
	unsigned long		ctx;
};

struct pvr_trcmd_flpreq {
	struct pvr_trcmd_syn	old_syn;
	struct pvr_trcmd_syn	new_syn;
};

enum pvr_trcmd_device {
	PVR_TRCMD_DEVICE_PCI,
	PVR_TRCMD_DEVICE_SGX,
	PVR_TRCMD_DEVICE_DISPC,
	PVR_TRCMD_DEVICE_PIPE_A_VSYNC,
	PVR_TRCMD_DEVICE_PIPE_B_VSYNC,
	PVR_TRCMD_DEVICE_PIPE_C_VSYNC,
};

struct pvr_trcmd_power {
	enum pvr_trcmd_device dev;
};

#ifdef CONFIG_PVR_TRACE_CMD

void *pvr_trcmd_reserve(unsigned type, int pid, const char *pname,
			   size_t size);

void pvr_trcmd_commit(void *alloc_ptr);

int pvr_trcmd_create_snapshot(u8 **snapshot_ret, size_t *snapshot_size);
void pvr_trcmd_destroy_snapshot(void *snapshot);

size_t pvr_trcmd_print(char *dst, size_t dst_size, const u8 *snapshot,
		       size_t snapshot_size, loff_t *snapshot_ofs);

void pvr_trcmd_set_syn(struct pvr_trcmd_syn *ts,
		       PVRSRV_KERNEL_SYNC_INFO *si);

void pvr_trcmd_remove_syn(int pid, const char *pname,
			  PVRSRV_KERNEL_SYNC_INFO *si);

static inline void pvr_trcmd_set_data(unsigned long *a, unsigned long val)
{
	*a = val;
}

static inline void pvr_trcmd_clear_syn(struct pvr_trcmd_syn *ts)
{
	ts->addr = 0;
}

void pvr_trcmd_check_syn_completions(int type);

#else

static inline void *
pvr_trcmd_reserve(unsigned type, int pid, const char *pname,
			   size_t size)
{
	return NULL;
}

static inline void pvr_trcmd_commit(void *alloc_ptr)
{
}

static inline int
pvr_trcmd_create_snapshot(u8 **snapshot_ret, size_t *snapshot_size)
{
	return 0;
}

static inline void pvr_trcmd_destroy_snapshot(void *snapshot)
{
}

static inline size_t
pvr_trcmd_print(char *dst, size_t dst_size, const u8 *snapshot,
		size_t snapshot_size, loff_t *snapshot_ofs)
{
	return 0;
}

static inline void
pvr_trcmd_set_syn(struct pvr_trcmd_syn *ts,
		  PVRSRV_KERNEL_SYNC_INFO *si)
{
}

static inline void pvr_trcmd_remove_syn(int pid, const char *pname,
					PVRSRV_KERNEL_SYNC_INFO *si)
{
}

static inline void pvr_trcmd_set_data(unsigned long *a, unsigned long val)
{
}

static inline void pvr_trcmd_clear_syn(struct pvr_trcmd_syn *ts)
{
}

static inline void pvr_trcmd_check_syn_completions(int type)
{
}

#endif		/* CONFIG_PVR_SYNC_CNT */

#endif

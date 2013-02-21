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

#include <linux/vmalloc.h>

#include "img_types.h"
#include "resman.h"
#include "handle.h"
#include "pvr_trace_cmd.h"
#include "perproc.h"
#include "pvr_bridge_km.h"

/* Need to be log2 size. */
#define PVR_TBUF_SIZE	(1 << (CONFIG_PVR_TRACE_CMD_BUF_SHIFT + PAGE_SHIFT))

#define PVR_TRCMD_INDENT	3

static struct pvr_trcmd_buf {
	int		read_idx;
	int		write_idx;
	int		pending_idx;	/*
					 * the pending_idx ... write_idx area
					 * is already allocated but still being
					 * written to
					 */
	int		write_pending;	/*
					 * number of writers in the above
					 * pending area
					 */
	char		data[PVR_TBUF_SIZE];
} tbuf;

static LIST_HEAD(syn_track_list);

static char tbuf_overflow[1024];

static DEFINE_SPINLOCK(tbuf_lock);	/* protects tbuf and syn_track_list */

struct tbuf_frame {
	unsigned short size;
	unsigned short type;
	unsigned long pid;
	unsigned long long time;
	char pname[16];
};

struct trcmd_desc {
	const char *name;
	size_t (*print)(char *dst, size_t dst_size, const void *tbuf);
};

static size_t prn_syn(const char *name, char *dst, size_t dst_size,
		      const struct pvr_trcmd_syn *ts)
{
	size_t len;

	if (!ts->addr)
		return 0;

	len =  scnprintf(dst, dst_size, "%*s%s", PVR_TRCMD_INDENT, "", name);
	len += scnprintf(&dst[len], dst_size - len, " addr:%08lx", ts->addr);
	len += scnprintf(&dst[len], dst_size - len,
			 " rop/c:%8lu/%8lu wop/c:%8lu/%8lu\n",
			 ts->rd_pend, ts->rd_comp, ts->wr_pend, ts->wr_comp);

	return len;
}

static size_t trcmd_prn_syn(char *dst, size_t dst_size, const void *tbuf)
{
	const struct pvr_trcmd_syn *ts = tbuf;

	return prn_syn("syn     ", dst, dst_size, ts);
}

static size_t trcmd_prn_sgxkick(char *dst, size_t dst_size, const void *tbuf)
{
	const struct pvr_trcmd_sgxkick *d = tbuf;
	size_t len;
	int i;

	len  = prn_syn("tatq_syn", dst, dst_size, &d->tatq_syn);
	len += prn_syn("3dtq_syn", &dst[len], dst_size - len, &d->_3dtq_syn);
	for (i = 0; i < SGX_MAX_SRC_SYNCS; i++) {
		char sname[10];

		snprintf(sname, sizeof(sname), "src_syn%d", i);
		len += prn_syn(sname, &dst[len], dst_size - len,
			       &d->src_syn[i]);
	}
	len += prn_syn("dst_syn ", &dst[len], dst_size - len, &d->dst_syn);
	len += prn_syn("ta3d_syn", &dst[len], dst_size - len, &d->ta3d_syn);
	len += scnprintf(&dst[len], dst_size - len, "%*sctx  %08lx\n",
			 PVR_TRCMD_INDENT, "", d->ctx);

	return len;
}

static size_t trcmd_prn_sgxtfer(char *dst, size_t dst_size, const void *tbuf)
{
	const struct pvr_trcmd_sgxtransfer *d = tbuf;
	size_t len;
	int i;

	len  = prn_syn("ta_syn  ", dst, dst_size, &d->ta_syn);
	len += prn_syn("3d_syn  ", &dst[len], dst_size - len, &d->_3d_syn);
	for (i = 0; i < ARRAY_SIZE(d->src_syn); i++) {
		char sname[10];

		snprintf(sname, sizeof(sname), "src_syn%d", i);
		len += prn_syn(sname, &dst[len], dst_size - len,
			       &d->src_syn[i]);
	}
	for (i = 0; i < ARRAY_SIZE(d->dst_syn); i++) {
		char sname[10];

		snprintf(sname, sizeof(sname), "dst_syn%d", i);
		len += prn_syn(sname, &dst[len], dst_size - len,
			       &d->dst_syn[i]);
	}
	len += scnprintf(&dst[len], dst_size - len, "%*sctx  %08lx\n",
			 PVR_TRCMD_INDENT, "", d->ctx);

	return len;
}

static size_t trcmd_prn_flpreq(char *dst, size_t dst_size, const void *tbuf)
{
	const struct pvr_trcmd_flpreq *d = tbuf;
	size_t len = 0;

	len += prn_syn("old_syn ", &dst[len], dst_size - len, &d->old_syn);
	len += prn_syn("new_syn ", &dst[len], dst_size - len, &d->new_syn);

	return len;
}

static const char * const trcmd_devices[] = {
	[PVR_TRCMD_DEVICE_PCI]          = "PCI device",
	[PVR_TRCMD_DEVICE_SGX]          = "SGX",
	[PVR_TRCMD_DEVICE_DISPC]        = "Display controller",
	[PVR_TRCMD_DEVICE_PIPE_A_VSYNC] = "Pipe A VSync",
	[PVR_TRCMD_DEVICE_PIPE_B_VSYNC] = "Pipe B VSync",
	[PVR_TRCMD_DEVICE_PIPE_C_VSYNC] = "Pipe C VSync",
};

static size_t trcmd_prn_power(char *dst, size_t dst_size, const void *tbuf)
{
	const struct pvr_trcmd_power *s = tbuf;

	return scnprintf(dst, dst_size, "%*sdev %s\n", PVR_TRCMD_INDENT,
			"", trcmd_devices[s->dev]);
}

static struct trcmd_desc trcmd_desc_table[] = {
	[PVR_TRCMD_SGX_FIRSTKICK]    = { "sgx_first_kick", trcmd_prn_sgxkick },
	[PVR_TRCMD_SGX_KICK]	     = { "sgx_kick", trcmd_prn_sgxkick },
	[PVR_TRCMD_SGX_TFER_KICK]    = { "sgx_tfer_kick", trcmd_prn_sgxtfer },
	[PVR_TRCMD_SGX_QBLT_SYNCHK]  = { "sgx_qblt_synchk", trcmd_prn_syn },
	[PVR_TRCMD_SGX_COMP]         = { "sgx_comp", trcmd_prn_syn },
	[PVR_TRCMD_FLPREQ]           = { "sgx_flip_req", trcmd_prn_flpreq },
	[PVR_TRCMD_FLPCOMP]          = { "sgx_flip_comp", trcmd_prn_syn },
	[PVR_TRCMD_SYN_REMOVE]       = { "sgx_syn_remove", trcmd_prn_syn },
	[PVR_TRCMD_SUSPEND]          = { "pvr_suspend", trcmd_prn_power },
	[PVR_TRCMD_RESUME]           = { "pvr_resume", trcmd_prn_power },
};

/* Modular add */
static inline int tbuf_idx_add(int val, int delta)
{
	val += delta;
	val &= PVR_TBUF_SIZE - 1;

	return val;
}

static size_t prn_frame(const struct tbuf_frame *f, char *dst, size_t dst_size)
{
	const struct trcmd_desc *desc;
	unsigned long long sec;
	unsigned long usec_frac;
	size_t len;

	desc = &trcmd_desc_table[f->type];

	sec = f->time;
	usec_frac = do_div(sec, 1000000000) / 1000;

	len = scnprintf(dst, dst_size, "[%5llu.%06lu] %s[%ld]: %s\n",
			sec, usec_frac, f->pname, f->pid, desc->name);
	if (desc->print)
		len += desc->print(&dst[len], dst_size - len, (void *)(f + 1));

	return len;
}

int pvr_trcmd_create_snapshot(uint8_t **snapshot_ret, size_t *snapshot_size)
{
	uint8_t *snapshot = NULL;
	int read_idx;
	size_t size = -1;
	size_t tail_size;
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);

	while (1) {
		int check_size;

		read_idx = tbuf.read_idx;
		check_size = tbuf_idx_add(tbuf.pending_idx, -read_idx);
		if (snapshot || !size) {
			if (size == check_size)
				break;
			/* alloc size changed, try again */
		}
		size = check_size;

		spin_unlock_irqrestore(&tbuf_lock, flags);

		/*
		 * vmalloc might reschedule, so release the lock and recheck
		 * later if the required allocation size changed. If so, try
		 * again.
		 */
		if (snapshot)
			vfree(snapshot);
		if (size) {
			snapshot = vmalloc(size);

			if (!snapshot)
				return -ENOMEM;
		}

		spin_lock_irqsave(&tbuf_lock, flags);
	}

	tail_size = min_t(size_t, size, PVR_TBUF_SIZE - read_idx);
	memcpy(snapshot, &tbuf.data[read_idx], tail_size);
	memcpy(&snapshot[tail_size], tbuf.data, size - tail_size);

	spin_unlock_irqrestore(&tbuf_lock, flags);

	*snapshot_ret = snapshot;
	*snapshot_size = size;

	return 0;
}

void pvr_trcmd_destroy_snapshot(void *snapshot)
{
	vfree(snapshot);
}

size_t pvr_trcmd_print(char *dst, size_t dst_size, const u8 *snapshot,
		       size_t snapshot_size, loff_t *snapshot_ofs)
{
	size_t dst_len;

	if (*snapshot_ofs >= snapshot_size)
		return 0;
	dst_len = 0;

	snapshot_size -= *snapshot_ofs;

	while (snapshot_size) {
		const struct tbuf_frame *f;
		size_t this_len;

		if (WARN_ON_ONCE(snapshot_size < 4))
			break;

		f = (struct tbuf_frame *)&snapshot[*snapshot_ofs];
		if (WARN_ON_ONCE(!f->size || f->size > snapshot_size ||
				 f->type >= ARRAY_SIZE(trcmd_desc_table)))
			break;

		if (f->type != PVR_TRCMD_PAD)
			this_len = prn_frame(f, &dst[dst_len],
					     dst_size - dst_len);
		else
			this_len = 0;

		if (dst_len + this_len + 1 == dst_size) {
			/* drop the last printed frame */
			dst[dst_len] = '\0';

			break;
		}

		*snapshot_ofs += f->size;
		dst_len += this_len;
		snapshot_size -= f->size;
	}

	return dst_len;
}

static void *tbuf_get_space(size_t size)
{
	void *ret;
	int buf_idx;

	while (1) {
		if (tbuf_idx_add(tbuf.pending_idx - 1, -tbuf.write_idx) <
		    size) {
			/*
			 * Can't allocate into the incomplete area, since it
			 * might be still being written to. New data will be
			 * lost.
			 */
			WARN_ONCE(1, "pvr: command trace buffer overflow\n");

			return NULL;
		}

		if (tbuf_idx_add(tbuf.read_idx - 1, -tbuf.write_idx) < size) {
			/*
			 * Trace buffer overflow, discard the frame that will
			 * be overwritten by the next write. Old data will be
			 * lost.
			 */
			struct tbuf_frame *f;

			f = (struct tbuf_frame *)&tbuf.data[tbuf.read_idx];
			buf_idx = tbuf.read_idx;
			tbuf.read_idx = tbuf_idx_add(tbuf.read_idx, f->size);
		} else if (PVR_TBUF_SIZE - tbuf.write_idx < size) {
			struct tbuf_frame *f =
				(void *)&tbuf.data[tbuf.write_idx];
			/*
			 * Not enough space until the end of trace buffer,
			 * rewind to the beginning. Frames are sizeof(long)
			 * aligned, thus we are guaranteed to have space for
			 * the following two fields.
			 */
			f->size = PVR_TBUF_SIZE - tbuf.write_idx;
			f->type = PVR_TRCMD_PAD;
			if (tbuf.pending_idx == tbuf.write_idx)
				tbuf.pending_idx = 0;
			tbuf.write_idx = 0;
		} else {
			break;
		}
	}
	ret = &tbuf.data[tbuf.write_idx];
	tbuf.write_idx = tbuf_idx_add(tbuf.write_idx, size);

	return ret;
}

static void *frame_reserve(unsigned type, int pid, const char *pname,
			   size_t size)
{
	struct tbuf_frame *f;
	size_t total_size;

	size = ALIGN(size, __alignof__(*f));
	total_size = sizeof(*f) + size;
	f = tbuf_get_space(total_size);
	if (!f) {
		/*
		 * Return something the caller can write into, so the caller
		 * doesn't need to check for NULL.
		 */
		WARN_ON_ONCE(total_size > sizeof(tbuf_overflow));

		return tbuf_overflow;
	}
	tbuf.write_pending++;

	f->size = total_size;
	f->type = type;
	f->pid = pid;
	f->time = cpu_clock(smp_processor_id());
	strlcpy(f->pname, pname, sizeof(f->pname));

	return f + 1;
}

static void frame_commit(void *alloc_ptr)
{
	struct tbuf_frame *f;

	if (alloc_ptr == tbuf_overflow)
		return;

	if (WARN_ON_ONCE(tbuf.write_pending <= 0))
		return;

	f = (struct tbuf_frame *)alloc_ptr - 1;

	tbuf.write_pending--;
	WARN_ON_ONCE(f->size > tbuf_idx_add(tbuf.write_idx, -tbuf.pending_idx));
	if (!tbuf.write_pending) {
		tbuf.pending_idx = tbuf.write_idx;
	} else {
		int f_idx = (char *)f - tbuf.data;

		if (f_idx == tbuf.pending_idx)
			tbuf.pending_idx = tbuf_idx_add(tbuf.pending_idx,
							f->size);
	}
}

void *pvr_trcmd_reserve(unsigned type, int pid, const char *pname, size_t size)
{
	struct tbuf_frame *f;
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);
	f = frame_reserve(type, pid, pname, size);
	spin_unlock_irqrestore(&tbuf_lock, flags);

	return f;
}

void pvr_trcmd_commit(void *alloc_ptr)
{
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);
	frame_commit(alloc_ptr);
	spin_unlock_irqrestore(&tbuf_lock, flags);
}

static void add_syn_to_track_list(PVRSRV_KERNEL_SYNC_INFO *si)
{
	/* only add if not already on the list */
	if (list_empty(&si->link))
		list_add(&si->link, &syn_track_list);
}

static void remove_syn_from_track_list(PVRSRV_KERNEL_SYNC_INFO *si)
{
	list_del_init(&si->link);
}

static void __set_syn(struct pvr_trcmd_syn *ts,
		      PVRSRV_KERNEL_SYNC_INFO *si)
{
	PVRSRV_SYNC_DATA *sd = si->psSyncData;

	ts->rd_pend = sd->ui32ReadOpsPending;
	ts->rd_comp = sd->ui32ReadOpsComplete;
	ts->wr_pend = sd->ui32WriteOpsPending;
	ts->wr_comp = sd->ui32WriteOpsComplete;
	ts->addr    = si->sWriteOpsCompleteDevVAddr.uiAddr - 4;
}

void pvr_trcmd_remove_syn(int pid, const char *pname,
			  PVRSRV_KERNEL_SYNC_INFO *si)
{
	struct pvr_trcmd_syn *ts;
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);

	ts = frame_reserve(PVR_TRCMD_SYN_REMOVE, pid, pname,
				 sizeof(*ts));
	__set_syn(ts, si);
	frame_commit(ts);
	remove_syn_from_track_list(si);

	spin_unlock_irqrestore(&tbuf_lock, flags);
}

void pvr_trcmd_set_syn(struct pvr_trcmd_syn *ts,
		       PVRSRV_KERNEL_SYNC_INFO *si)
{
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);
	add_syn_to_track_list(si);
	spin_unlock_irqrestore(&tbuf_lock, flags);

	__set_syn(ts, si);
}

static unsigned long calc_syn_digest(PVRSRV_SYNC_DATA *sd)
{
	return sd->ui32ReadOpsPending +
	       sd->ui32ReadOpsComplete +
	       sd->ui32WriteOpsPending +
	       sd->ui32WriteOpsComplete;
}

static int syn_has_changed(PVRSRV_KERNEL_SYNC_INFO *si)
{
	unsigned long digest;
	int changed;

	digest = calc_syn_digest(si->psSyncData);
	changed = digest != si->counter_digest;
	si->counter_digest = digest;

	return changed;
}

static int syn_is_complete(PVRSRV_KERNEL_SYNC_INFO *si)
{
	PVRSRV_SYNC_DATA *sd = si->psSyncData;

	return sd->ui32ReadOpsPending == sd->ui32ReadOpsComplete &&
	       sd->ui32WriteOpsPending == sd->ui32WriteOpsComplete;
}

void pvr_trcmd_check_syn_completions(int type)
{
	PVRSRV_KERNEL_SYNC_INFO *si;
	PVRSRV_KERNEL_SYNC_INFO *tmp;
	unsigned long flags;

	spin_lock_irqsave(&tbuf_lock, flags);

	list_for_each_entry_safe(si, tmp, &syn_track_list, link) {
		if (syn_has_changed(si)) {
			struct pvr_trcmd_syn *ts;

			ts = frame_reserve(type, 0, "irq", sizeof(*ts));
			__set_syn(ts, si);
			frame_commit(ts);
		}
		if (syn_is_complete(si))
			remove_syn_from_track_list(si);
	}

	spin_unlock_irqrestore(&tbuf_lock, flags);
}

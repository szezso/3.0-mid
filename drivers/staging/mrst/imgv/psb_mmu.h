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
 **************************************************************************/
#ifndef __PSB_MMU_H__
#define __PSB_MMU_H__

#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_reg.h"

/*
 * Code for the SGX MMU:
 */

/*
 * clflush on one processor only:
 * clflush should apparently flush the cache line on all processors in an
 * SMP system.
 */

/*
 * kmap atomic:
 * The usage of the slots must be completely encapsulated within a spinlock, and
 * no other functions that may be using the locks for other purposed may be
 * called from within the locked region.
 * Since the slots are per processor, this will guarantee that we are the only
 * user.
 */

/*
 * TODO: Inserting ptes from an interrupt handler:
 * This may be desirable for some SGX functionality where the GPU can fault in
 * needed pages. For that, we need to make an atomic insert_pages function, that
 * may fail.
 * If it fails, the caller need to insert the page using a workqueue function,
 * but on average it should be fast.
 */

struct psb_mmu_driver {
	/* protects driver- and pd structures. Always take in read mode
	 * before taking the page table spinlock.
	 */
	struct rw_semaphore sem;

	/* protects page tables, directory tables and pt tables.
	 * and pt structures.
	 */
	spinlock_t lock;

	atomic_t needs_tlbflush;

	uint8_t __iomem *register_map;
	struct psb_mmu_pd *default_pd;
	/*uint32_t bif_ctrl;*/
	int has_clflush;
	int clflush_add;
	unsigned long clflush_mask;

	struct drm_psb_private *dev_priv;
};

struct psb_mmu_pd;

struct psb_mmu_pt {
	struct psb_mmu_pd *pd;
	uint32_t index;
	uint32_t count;
	struct page *p;
	uint32_t *v;
};

struct psb_mmu_pd {
	struct psb_mmu_driver *driver;
	int hw_context;
	struct psb_mmu_pt **tables;
	struct page *p;
	struct page *dummy_pt;
	struct page *dummy_page;
	uint32_t pd_mask;
	uint32_t invalid_pde;
	uint32_t invalid_pte;
};

#endif

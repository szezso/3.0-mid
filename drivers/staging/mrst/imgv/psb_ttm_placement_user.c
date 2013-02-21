/**************************************************************************
 *
 * Copyright (c) 2006-2008 Tungsten Graphics, Inc., Cedar Park, TX., USA
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
/*
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include "psb_ttm_placement_user.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_object.h"
#include "psb_ttm_userobj_api.h"
#include "ttm/ttm_lock.h"
#include "psb_drm.h"
#include "pvr_bridge_km.h"
#include <linux/slab.h>
#include <linux/sched.h>

struct ttm_bo_user_object {
	struct ttm_base_object base;
	struct ttm_buffer_object bo;
};

static size_t pl_bo_size;

static uint32_t psb_busy_prios[] = {
	TTM_PL_TT,
	TTM_PL_PRIV0, /* CI */
	TTM_PL_PRIV2, /* RAR */
	TTM_PL_PRIV1, /* DRM_PSB_MEM_MMU */
	TTM_PL_SYSTEM
};

static const struct ttm_placement default_placement = {0, 0, 0, NULL, 5, psb_busy_prios};

static uint32_t psb_pvr_busy_prios[] = {
	TTM_PL_TT,
	TTM_PL_PRIV1, /* DRM_PSB_MEM_MMU */
	TTM_PL_SYSTEM
};

static const struct ttm_placement pvr_default_placement = {
	.busy_placement		= psb_pvr_busy_prios,
	.num_busy_placement	= ARRAY_SIZE(psb_pvr_busy_prios),
};

static size_t ttm_pl_size(struct ttm_bo_device *bdev, unsigned long num_pages)
{
	size_t page_array_size =
		(num_pages * sizeof(void *) + PAGE_SIZE - 1) & PAGE_MASK;

	if (unlikely(pl_bo_size == 0)) {
		pl_bo_size = bdev->glob->ttm_bo_extra_size +
			     ttm_round_pot(sizeof(struct ttm_bo_user_object));
	}

	return bdev->glob->ttm_bo_size + 2 * page_array_size;
}

static struct ttm_bo_user_object *ttm_bo_user_lookup(struct ttm_object_file
		*tfile, uint32_t handle) {
	struct ttm_base_object *base;

	base = ttm_base_object_lookup(tfile, handle);
	if (unlikely(base == NULL)) {
		printk(KERN_ERR "Invalid buffer object handle 0x%08lx.\n",
		       (unsigned long)handle);
		return NULL;
	}

	if (unlikely(base->object_type != ttm_buffer_type)) {
		ttm_base_object_unref(&base);
		printk(KERN_ERR "Invalid buffer object handle 0x%08lx.\n",
		       (unsigned long)handle);
		return NULL;
	}

	return container_of(base, struct ttm_bo_user_object, base);
}

struct ttm_buffer_object *ttm_buffer_object_lookup(struct ttm_object_file
		*tfile, uint32_t handle) {
	struct ttm_bo_user_object *user_bo;
	struct ttm_base_object *base;

	user_bo = ttm_bo_user_lookup(tfile, handle);
	if (unlikely(user_bo == NULL))
		return NULL;

	(void)ttm_bo_reference(&user_bo->bo);
	base = &user_bo->base;
	ttm_base_object_unref(&base);
	return &user_bo->bo;
}

static void ttm_bo_user_destroy(struct ttm_buffer_object *bo)
{
	struct ttm_bo_user_object *user_bo =
		container_of(bo, struct ttm_bo_user_object, bo);

	ttm_mem_global_free(bo->glob->mem_glob, bo->acc_size);
	kfree(user_bo);
}

static void ttm_bo_user_release(struct ttm_base_object **p_base)
{
	struct ttm_bo_user_object *user_bo;
	struct ttm_base_object *base = *p_base;
	struct ttm_buffer_object *bo;

	*p_base = NULL;

	if (unlikely(base == NULL))
		return;

	user_bo = container_of(base, struct ttm_bo_user_object, base);
	bo = &user_bo->bo;
	ttm_bo_unref(&bo);
}

static void ttm_bo_user_ref_release(struct ttm_base_object *base,
				    enum ttm_ref_type ref_type)
{
	struct ttm_bo_user_object *user_bo =
		container_of(base, struct ttm_bo_user_object, base);
	struct ttm_buffer_object *bo = &user_bo->bo;

	switch (ref_type) {
	case TTM_REF_SYNCCPU_WRITE:
		ttm_bo_synccpu_write_release(bo);
		break;
	default:
		BUG();
	}
}

static void ttm_pl_fill_rep(struct ttm_buffer_object *bo,
			    struct ttm_pl_rep *rep)
{
	struct ttm_bo_user_object *user_bo =
		container_of(bo, struct ttm_bo_user_object, bo);

	rep->gpu_offset = bo->offset;
	rep->bo_size = bo->num_pages << PAGE_SHIFT;
	rep->map_handle = bo->addr_space_offset;
	rep->placement = bo->mem.placement;
	rep->handle = user_bo->base.hash.key;
	rep->sync_object_arg = (uint32_t)(unsigned long)bo->sync_obj_arg;
}

/* FIXME Copy from upstream TTM */
static inline size_t ttm_bo_size(struct ttm_bo_global *glob,
				 unsigned long num_pages)
{
	size_t page_array_size = (num_pages * sizeof(void *) + PAGE_SIZE - 1) &
				 PAGE_MASK;

	return glob->ttm_bo_size + 2 * page_array_size;
}

/* FIXME Copy from upstream TTM "ttm_bo_create", upstream TTM does not export this, so copy it here */
static int ttm_bo_create_private(struct ttm_bo_device *bdev,
				 unsigned long size,
				 enum ttm_bo_type type,
				 struct ttm_placement *placement,
				 uint32_t page_alignment,
				 unsigned long buffer_start,
				 bool interruptible,
				 struct file *persistant_swap_storage,
				 struct ttm_buffer_object **p_bo)
{
	struct ttm_buffer_object *bo;
	struct ttm_mem_global *mem_glob = bdev->glob->mem_glob;
	int ret;

	size_t acc_size =
		ttm_bo_size(bdev->glob, (size + PAGE_SIZE - 1) >> PAGE_SHIFT);
	ret = ttm_mem_global_alloc(mem_glob, acc_size, false, false);
	if (unlikely(ret != 0))
		return ret;

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);

	if (unlikely(bo == NULL)) {
		ttm_mem_global_free(mem_glob, acc_size);
		return -ENOMEM;
	}

	ret = ttm_bo_init(bdev, bo, size, type, placement, page_alignment,
			  buffer_start, interruptible,
			  persistant_swap_storage, acc_size, NULL);
	if (likely(ret == 0))
		*p_bo = bo;

	return ret;
}

static bool fixed_placement_valid(uint32_t pl_flags)
{
	if (!(pl_flags & TTM_PL_FLAG_NO_SWAP))
		return false;

	pl_flags &= TTM_PL_MASK_MEM;
	pl_flags &= ~(DRM_PSB_FLAG_MEM_MMU | TTM_PL_FLAG_TT |
		      TTM_PL_FLAG_SYSTEM);

	return !pl_flags;
}

static uint32_t normalize_placement_flags(uint32_t flags)
{
	if ((flags & TTM_PL_MASK_CACHING) == 0)
		flags |= TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;
	if (flags & TTM_PL_FLAG_NO_SWAP)
		flags |= TTM_PL_FLAG_NO_EVICT;

	return flags;
}

static int set_clear_placement_flags(struct ttm_buffer_object *bo, uint32_t set,
				     uint32_t clear, uint32_t *new_ret)
{
	uint32_t cur = bo->mem.placement;
	uint32_t new;
	bool is_root;
	bool has_fixed_pages;
	char *flag_name = "no swap";

	new = (cur | set) & ~clear;
	new = normalize_placement_flags(new);

	has_fixed_pages = drm_psb_has_fixed_pages(bo->ttm->be);
	if (has_fixed_pages && !fixed_placement_valid(new))
		goto err;

	flag_name = "no evict";
	is_root = capable(CAP_SYS_ADMIN);
	if (!is_root && (cur ^ new) & TTM_PL_FLAG_NO_EVICT)
		goto err;

	*new_ret = new;

	return 0;
err:
	pr_debug("imgv: Permission denied to set '%s' flag: "
		 "is_root %d cur %08x set %08x clear %08x\n",
		 flag_name, is_root, cur, set, clear);

	return -EACCES;
}

int ttm_buffer_object_create(struct ttm_bo_device *bdev,
			     unsigned long size,
			     enum ttm_bo_type type,
			     uint32_t flags,
			     uint32_t page_alignment,
			     unsigned long buffer_start,
			     bool interruptible,
			     struct file *persistant_swap_storage,
			     struct ttm_buffer_object **p_bo)
{
	struct ttm_placement placement = default_placement;
	int ret;

	if ((flags & TTM_PL_MASK_CACHING) == 0)
		flags |= TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED;

	placement.num_placement = 1;
	placement.placement = &flags;

	ret = ttm_bo_create_private(bdev,
				    size,
				    type,
				    &placement,
				    page_alignment,
				    buffer_start,
				    interruptible,
				    persistant_swap_storage,
				    p_bo);

	return ret;
}

struct create_params {
	struct ttm_bo_device *bdev;
	enum ttm_bo_type bo_type;
	size_t size;
	off_t align;
	unsigned long user_address;
	struct page **pages;
	struct ttm_placement *placement;
	struct ttm_object_file *tfile;
	bool shareable;
};

static int pl_create_buf(struct create_params *p, struct ttm_lock *lock,
			 struct ttm_pl_rep *rep)
{
	struct ttm_buffer_object *bo;
	struct ttm_buffer_object *tmp;
	struct ttm_bo_user_object *user_bo;
	struct ttm_mem_global *mem_glob = p->bdev->glob->mem_glob;
	size_t acc_size;
	int pg_cnt;
	int ret;

	pg_cnt = PAGE_ALIGN(p->size) >> PAGE_SHIFT;
	acc_size = ttm_pl_size(p->bdev, pg_cnt);
	ret = ttm_mem_global_alloc(mem_glob, acc_size, false, false);
	if (ret < 0)
		return ret;

	user_bo = kzalloc(sizeof(*user_bo), GFP_KERNEL);
	if (!user_bo) {
		ret = -ENOMEM;
		goto err1;
	}

	bo = &user_bo->bo;

	ret = ttm_read_lock(lock, true);
	if (ret < 0)
		goto err2;

	ret = ttm_bo_init(p->bdev, bo, pg_cnt << PAGE_SHIFT, p->bo_type,
			  p->placement, p->align, p->user_address, true, NULL,
			  acc_size, &ttm_bo_user_destroy);

	/*
	 * Note that the ttm_buffer_object_init function
	 * would've called the destroy function on failure!!
	 */
	if (ret < 0) {
		ttm_read_unlock(lock);
		goto err2;
	}

	if (p->pages)
		drm_psb_set_fixed_pages(bo->ttm, p->pages, pg_cnt);

	ttm_read_unlock(lock);

	/* ttm_bo_unref->destroy will unset the fixed pages on failure */
	if (ret < 0)
		goto err3;

	tmp = ttm_bo_reference(bo);

	ret = ttm_base_object_init(p->tfile, &user_bo->base, p->shareable,
				   ttm_buffer_type, &ttm_bo_user_release,
				   &ttm_bo_user_ref_release);
	if (ret < 0)
		goto err3;

	spin_lock(&bo->bdev->fence_lock);
	ttm_pl_fill_rep(bo, rep);
	spin_unlock(&bo->bdev->fence_lock);

	ttm_bo_unref(&tmp);

	return 0;
err3:
	ttm_bo_unref(&tmp);
err2:
	kfree(user_bo);
err1:
	ttm_mem_global_free(mem_glob, acc_size);

	return ret;
}

int ttm_pl_create_ioctl(struct ttm_object_file *tfile,
			struct ttm_bo_device *bdev,
			struct ttm_lock *lock, void *data)
{
	union ttm_pl_create_arg *arg = data;
	struct ttm_pl_create_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_placement pl = default_placement;
	uint32_t pl_flags = normalize_placement_flags(req->placement);
	struct create_params cp = {
		.bdev		= bdev,
		.bo_type	= ttm_bo_type_device,
		.size		= req->size,
		.align		= req->page_alignment,
		.placement	= &pl,
		.tfile		= tfile,
		.shareable	= req->placement & TTM_PL_FLAG_SHARED,
	};

	pl.num_placement = 1;
	pl.placement = &pl_flags;

	return pl_create_buf(&cp, lock, rep);
}

int ttm_pl_ub_create_ioctl(struct ttm_object_file *tfile,
			   struct ttm_bo_device *bdev,
			   struct ttm_lock *lock, void *data)
{
	union ttm_pl_create_ub_arg *arg = data;
	struct ttm_pl_create_ub_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_placement pl = default_placement;
	uint32_t pl_flags = normalize_placement_flags(req->placement);
	struct create_params cp = {
		.bdev		= bdev,
		.bo_type	= ttm_bo_type_user,
		.size		= req->size,
		.user_address	= req->user_address,
		.align		= req->page_alignment,
		.placement	= &pl,
		.tfile		= tfile,
		.shareable	= req->placement & TTM_PL_FLAG_SHARED,
	};

	pl.num_placement = 1;
	pl.placement = &pl_flags;

	return pl_create_buf(&cp, lock, rep);
}

int ttm_pl_wrap_pvr_buf_ioctl(struct ttm_object_file *tfile,
			struct ttm_bo_device *bdev,
			struct ttm_lock *lock, void *data)
{
	union ttm_pl_wrap_pvr_buf_arg *arg = data;
	struct ttm_pl_wrap_pvr_buf_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_placement pl = pvr_default_placement;
	struct pvr_buf_info buf_info;
	uint32_t pl_flags = normalize_placement_flags(req->placement);
	int ret;
	struct create_params cp = {
		.bdev		= bdev,
		.bo_type	= ttm_bo_type_device,
		.align		= req->page_alignment,
		.placement	= &pl,
		.tfile		= tfile,
		.shareable	= req->placement & TTM_PL_FLAG_SHARED,
	};

	if (!fixed_placement_valid(pl_flags))
		return -EINVAL;

	pl.num_placement = 1;
	pl.placement = &pl_flags;

	ret = pvr_lookup_dev_buf(req->handle, &buf_info);
	if (ret < 0)
		return ret;
	/*
	 * We set here the ttm pages to point to the fixed pages from the
	 * pvr buffer. Due to TTM_PL_FLAG_NO_SWAP set above and the
	 * busy_placement settings, these will never be swapped or moved to
	 * an IO region and thus never be freed before the buffer itself is
	 * destroyed.
	 */
	cp.pages = buf_info.pages;
	cp.size = buf_info.page_cnt;

	ret = pl_create_buf(&cp, lock, rep);

	pvr_put_dev_buf(&buf_info);

	return ret;
}

int ttm_pl_reference_ioctl(struct ttm_object_file *tfile, void *data)
{
	union ttm_pl_reference_arg *arg = data;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_bo_user_object *user_bo;
	struct ttm_buffer_object *bo;
	struct ttm_base_object *base;
	int ret;

	user_bo = ttm_bo_user_lookup(tfile, arg->req.handle);
	if (unlikely(user_bo == NULL)) {
		printk(KERN_ERR "Could not reference buffer object.\n");
		return -EINVAL;
	}

	bo = &user_bo->bo;
	ret = ttm_ref_object_add(tfile, &user_bo->base, TTM_REF_USAGE, NULL);
	if (unlikely(ret != 0)) {
		printk(KERN_ERR
		       "Could not add a reference to buffer object.\n");
		goto out;
	}

	spin_lock(&bo->bdev->fence_lock);
	ttm_pl_fill_rep(bo, rep);
	spin_unlock(&bo->bdev->fence_lock);

out:
	base = &user_bo->base;
	ttm_base_object_unref(&base);
	return ret;
}

int ttm_pl_unref_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_reference_req *arg = data;

	return ttm_ref_object_base_unref(tfile, arg->handle, TTM_REF_USAGE);
}

int ttm_pl_synccpu_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_synccpu_arg *arg = data;
	struct ttm_bo_user_object *user_bo;
	struct ttm_buffer_object *bo;
	struct ttm_base_object *base;
	bool existed;
	int ret;

	switch (arg->op) {
	case TTM_PL_SYNCCPU_OP_GRAB:
		user_bo = ttm_bo_user_lookup(tfile, arg->handle);
		if (unlikely(user_bo == NULL)) {
			printk(KERN_ERR
			       "Could not find buffer object for synccpu.\n");
			return -EINVAL;
		}
		bo = &user_bo->bo;
		base = &user_bo->base;
		ret = ttm_bo_synccpu_write_grab(bo,
						arg->access_mode &
						TTM_PL_SYNCCPU_MODE_NO_BLOCK);
		if (unlikely(ret != 0)) {
			ttm_base_object_unref(&base);
			goto out;
		}
		ret = ttm_ref_object_add(tfile, &user_bo->base,
					 TTM_REF_SYNCCPU_WRITE, &existed);
		if (existed || ret != 0)
			ttm_bo_synccpu_write_release(bo);
		ttm_base_object_unref(&base);
		break;
	case TTM_PL_SYNCCPU_OP_RELEASE:
		ret = ttm_ref_object_base_unref(tfile, arg->handle,
						TTM_REF_SYNCCPU_WRITE);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	return ret;
}

int ttm_pl_setstatus_ioctl(struct ttm_object_file *tfile,
			   struct ttm_lock *lock, void *data)
{
	union ttm_pl_setstatus_arg *arg = data;
	struct ttm_pl_setstatus_req *req = &arg->req;
	struct ttm_pl_rep *rep = &arg->rep;
	struct ttm_buffer_object *bo;
	struct ttm_bo_device *bdev;
	struct ttm_placement placement = default_placement;
	uint32_t pl_flags;
	int ret;

	bo = ttm_buffer_object_lookup(tfile, req->handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR
		       "Could not find buffer object for setstatus.\n");
		return -EINVAL;
	}

	bdev = bo->bdev;

	ret = ttm_read_lock(lock, true);
	if (unlikely(ret != 0))
		goto out_err0;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (unlikely(ret != 0))
		goto out_err1;

	ret = ttm_bo_wait_cpu(bo, false);
	if (unlikely(ret != 0))
		goto out_err2;

	spin_lock(&bo->bdev->fence_lock);

	placement.num_placement = 1;
	ret = set_clear_placement_flags(bo, req->set_placement,
					req->clr_placement, &pl_flags);
	if (ret < 0)
		goto out_err2;
	placement.placement = &pl_flags;

	ret = ttm_bo_validate(bo, &placement, true, false, false);
	if (unlikely(ret != 0))
		goto out_err2;

	ttm_pl_fill_rep(bo, rep);
out_err2:
	spin_unlock(&bo->bdev->fence_lock);
	ttm_bo_unreserve(bo);
out_err1:
	ttm_read_unlock(lock);
out_err0:
	ttm_bo_unref(&bo);
	return ret;
}

static int psb_ttm_bo_block_reservation(struct ttm_buffer_object *bo, bool interruptible,
					bool no_wait)
{
	int ret;

	while (unlikely(atomic_cmpxchg(&bo->reserved, 0, 1) != 0)) {
		if (no_wait)
			return -EBUSY;
		else if (interruptible) {
			ret = wait_event_interruptible
			      (bo->event_queue, atomic_read(&bo->reserved) == 0);
			if (unlikely(ret != 0))
				return -ERESTART;
		} else {
			wait_event(bo->event_queue,
				   atomic_read(&bo->reserved) == 0);
		}
	}
	return 0;
}

static void psb_ttm_bo_unblock_reservation(struct ttm_buffer_object *bo)
{
	atomic_set(&bo->reserved, 0);
	wake_up_all(&bo->event_queue);
}

int ttm_pl_waitidle_ioctl(struct ttm_object_file *tfile, void *data)
{
	struct ttm_pl_waitidle_arg *arg = data;
	struct ttm_buffer_object *bo;
	int ret;

	bo = ttm_buffer_object_lookup(tfile, arg->handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR "Could not find buffer object for waitidle.\n");
		return -EINVAL;
	}

	ret =
		psb_ttm_bo_block_reservation(bo, true,
					     arg->mode & TTM_PL_WAITIDLE_MODE_NO_BLOCK);
	if (unlikely(ret != 0))
		goto out;
	spin_lock(&bo->bdev->fence_lock);
	ret = ttm_bo_wait(bo,
			  arg->mode & TTM_PL_WAITIDLE_MODE_LAZY,
			  true, arg->mode & TTM_PL_WAITIDLE_MODE_NO_BLOCK);
	spin_unlock(&bo->bdev->fence_lock);
	psb_ttm_bo_unblock_reservation(bo);
out:
	ttm_bo_unref(&bo);
	return ret;
}

int ttm_pl_verify_access(struct ttm_buffer_object *bo,
			 struct ttm_object_file *tfile)
{
	struct ttm_bo_user_object *ubo;

	/*
	 * Check bo subclass.
	 */

	if (unlikely(bo->destroy != &ttm_bo_user_destroy))
		return -EPERM;

	ubo = container_of(bo, struct ttm_bo_user_object, bo);
	if (likely(ubo->base.shareable || ubo->base.tfile == tfile))
		return 0;

	return -EPERM;
}

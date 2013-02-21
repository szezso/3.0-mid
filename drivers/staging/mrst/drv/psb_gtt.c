/*
 * Copyright (c) 2007, Intel Corporation.
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
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics.com>
 */

#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_pvr_glue.h"

extern struct mutex gPVRSRVLock;

static inline uint32_t psb_gtt_mask_pte(uint32_t pfn, int type)
{
	uint32_t mask = PSB_PTE_VALID;

	if (type & PSB_MMU_CACHED_MEMORY)
		mask |= PSB_PTE_CACHED;
	if (type & PSB_MMU_RO_MEMORY)
		mask |= PSB_PTE_RO;
	if (type & PSB_MMU_WO_MEMORY)
		mask |= PSB_PTE_WO;

	return (pfn << PAGE_SHIFT) | mask;
}

struct psb_gtt *psb_gtt_alloc(struct drm_device *dev) {
	struct psb_gtt *tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);

	if (!tmp)
		return NULL;

	tmp->dev = dev;

	return tmp;
}

void psb_gtt_takedown(struct psb_gtt *pg, int free)
{
	struct drm_psb_private *dev_priv;

	if (!pg)
		return;

	dev_priv = pg->dev->dev_private;

	if (pg->gtt_map) {
		iounmap(pg->gtt_map);
		pg->gtt_map = NULL;
	}
	if (pg->initialized) {
		pci_write_config_word(pg->dev->pdev, PSB_GMCH_CTRL,
				      pg->gmch_ctrl);
		PSB_WVDC32(pg->pge_ctl, PSB_PGETBL_CTL);
		(void) PSB_RVDC32(PSB_PGETBL_CTL);
	}
	if (pg->vram_addr) {
		iounmap(pg->vram_addr);
		pg->vram_addr = NULL;
	}
	if (free)
		kfree(pg);
}

int psb_gtt_init(struct psb_gtt *pg, int resume)
{
	struct drm_device *dev = pg->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned gtt_pages;
	unsigned long stolen_size, vram_stolen_size, ci_stolen_size;
	unsigned long rar_stolen_size;
	unsigned i, num_pages;
	unsigned pfn_base;
	uint32_t ci_pages, vram_pages;
	uint32_t tt_pages;
	uint32_t __iomem *ttm_gtt_map;
	uint32_t dvmt_mode = 0;

	int ret = 0;
	uint32_t pte;

	pci_read_config_word(dev->pdev, PSB_GMCH_CTRL, &pg->gmch_ctrl);
	pci_write_config_word(dev->pdev, PSB_GMCH_CTRL,
			      pg->gmch_ctrl | _PSB_GMCH_ENABLED);

	pg->pge_ctl = PSB_RVDC32(PSB_PGETBL_CTL);
	PSB_WVDC32(pg->pge_ctl | _PSB_PGETBL_ENABLED, PSB_PGETBL_CTL);
	(void) PSB_RVDC32(PSB_PGETBL_CTL);

	pg->initialized = 1;

	pg->gtt_phys_start = pg->pge_ctl & PAGE_MASK;

	pg->gatt_start = pci_resource_start(dev->pdev, PSB_GATT_RESOURCE);
	/* fix me: video mmu has hw bug to access 0x0D0000000,
	 * then make gatt start at 0x0e000,0000 */
	pg->mmu_gatt_start = PSB_MEM_TT_START;
	pg->gtt_start = pci_resource_start(dev->pdev, PSB_GTT_RESOURCE);
	gtt_pages =
		pci_resource_len(dev->pdev, PSB_GTT_RESOURCE) >> PAGE_SHIFT;
	pg->gatt_pages = pci_resource_len(dev->pdev, PSB_GATT_RESOURCE)
			 >> PAGE_SHIFT;

	pci_read_config_dword(dev->pdev, PSB_BSM, &pg->stolen_base);
	vram_stolen_size = pg->gtt_phys_start - pg->stolen_base - PAGE_SIZE;

	/* CI is not included in the stolen size since the TOPAZ MMU bug */
	ci_stolen_size = dev_priv->ci_region_size;
	/* Don't add CI & RAR share buffer space
	 * managed by TTM to stolen_size */
	stolen_size = vram_stolen_size;

	rar_stolen_size = dev_priv->rar_region_size;

	printk(KERN_INFO"GMMADR(region 0) start: 0x%08x (%dM).\n",
	       pg->gatt_start, pg->gatt_pages / 256);
	printk(KERN_INFO"GTTADR(region 3) start: 0x%08x (can map %dM RAM), and actual RAM base 0x%08x.\n",
	       pg->gtt_start, gtt_pages * 4, pg->gtt_phys_start);
	printk(KERN_INFO"Stole memory information \n");
	printk(KERN_INFO"      base in RAM: 0x%x \n", pg->stolen_base);
	printk(KERN_INFO"      size: %luK, calculated by (GTT RAM base) - (Stolen base), seems wrong\n",
	       vram_stolen_size / 1024);
	dvmt_mode = (pg->gmch_ctrl >> 4) & 0x7;
	printk(KERN_INFO"      the correct size should be: %dM(dvmt mode=%d) \n",
	       (dvmt_mode == 1) ? 1 : (2 << (dvmt_mode - 1)), dvmt_mode);

	if (ci_stolen_size > 0)
		printk(KERN_INFO"CI Stole memory: RAM base = 0x%08x, size = %lu M \n",
		       dev_priv->ci_region_start,
		       ci_stolen_size / 1024 / 1024);
	if (rar_stolen_size > 0)
		printk(KERN_INFO"RAR Stole memory: RAM base = 0x%08x, size = %lu M \n",
		       dev_priv->rar_region_start,
		       rar_stolen_size / 1024 / 1024);

	if (resume && (gtt_pages != pg->gtt_pages) &&
	    (stolen_size != pg->stolen_size)) {
		DRM_ERROR("GTT resume error.\n");
		ret = -EINVAL;
		goto out_err;
	}

	pg->gtt_pages = gtt_pages;
	pg->stolen_size = stolen_size;
	pg->vram_stolen_size = vram_stolen_size;
	pg->ci_stolen_size = ci_stolen_size;
	pg->rar_stolen_size = rar_stolen_size;
	pg->gtt_map =
		ioremap_nocache(pg->gtt_phys_start, gtt_pages << PAGE_SHIFT);
	if (!pg->gtt_map) {
		DRM_ERROR("Failure to map gtt.\n");
		ret = -ENOMEM;
		goto out_err;
	}

	pg->vram_addr = ioremap_wc(pg->stolen_base, stolen_size);
	if (!pg->vram_addr) {
		DRM_ERROR("Failure to map stolen base.\n");
		ret = -ENOMEM;
		goto out_err;
	}

	DRM_DEBUG("vram kernel virtual address %p\n", pg->vram_addr);

	tt_pages = (pg->gatt_pages < PSB_TT_PRIV0_PLIMIT) ?
		   (pg->gatt_pages) : PSB_TT_PRIV0_PLIMIT;

	ttm_gtt_map = pg->gtt_map + tt_pages / 2;

	/*
	 * insert vram stolen pages.
	 */

	pfn_base = pg->stolen_base >> PAGE_SHIFT;
	vram_pages = num_pages = vram_stolen_size >> PAGE_SHIFT;
	printk(KERN_INFO"Set up %d stolen pages starting at 0x%08x, GTT offset %dK\n",
	       num_pages, pfn_base, 0);
	for (i = 0; i < num_pages; ++i) {
		pte = psb_gtt_mask_pte(pfn_base + i, 0);
		iowrite32(pte, pg->gtt_map + i);
	}

	/*
	 * Init rest of gtt managed by IMG.
	 */
	pfn_base = page_to_pfn(dev_priv->scratch_page);
	pte = psb_gtt_mask_pte(pfn_base, 0);
	for (; i < tt_pages / 2 - 1; ++i)
		iowrite32(pte, pg->gtt_map + i);

	/*
	 * insert CI stolen pages
	 */

	pfn_base = dev_priv->ci_region_start >> PAGE_SHIFT;
	ci_pages = num_pages = ci_stolen_size >> PAGE_SHIFT;
	printk(KERN_INFO"Set up %d CI stolen pages starting at 0x%08x, GTT offset %dK\n",
	       num_pages, pfn_base, (ttm_gtt_map - pg->gtt_map) * 4);
	for (i = 0; i < num_pages; ++i) {
		pte = psb_gtt_mask_pte(pfn_base + i, 0);
		iowrite32(pte, ttm_gtt_map + i);
	}

	/*
	 * insert RAR stolen pages
	 */
	if (rar_stolen_size != 0) {
		pfn_base = dev_priv->rar_region_start >> PAGE_SHIFT;
		num_pages = rar_stolen_size >> PAGE_SHIFT;
		printk(KERN_INFO"Set up %d RAR stolen pages starting at 0x%08x, GTT offset %dK\n",
		       num_pages, pfn_base,
		       (ttm_gtt_map - pg->gtt_map + i) * 4);
		for (; i < num_pages + ci_pages; ++i) {
			pte = psb_gtt_mask_pte(pfn_base + i - ci_pages, 0);
			iowrite32(pte, ttm_gtt_map + i);
		}
	}
	/*
	 * Init rest of gtt managed by TTM.
	 */

	pfn_base = page_to_pfn(dev_priv->scratch_page);
	pte = psb_gtt_mask_pte(pfn_base, 0);
	PSB_DEBUG_INIT("Initializing the rest of a total "
		       "of %d gtt pages.\n", pg->gatt_pages);

	for (; i < pg->gatt_pages - tt_pages / 2; ++i)
		iowrite32(pte, ttm_gtt_map + i);
	(void) ioread32(pg->gtt_map + i - 1);

	return 0;

out_err:
	psb_gtt_takedown(pg, 0);
	return ret;
}

int psb_gtt_insert_pages(struct psb_gtt *pg, struct page **pages,
			 uint32_t offset_pages, uint32_t num_pages,
			 unsigned desired_tile_stride,
			 unsigned hw_tile_stride, int type)
{
	unsigned rows = 1;
	unsigned add;
	unsigned row_add;
	unsigned i;
	unsigned j;
	uint32_t __iomem *cur_page = NULL;
	uint32_t pte;

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride;
	row_add = hw_tile_stride;

	for (i = 0; i < rows; ++i) {
		cur_page = pg->gtt_map + offset_pages;
		for (j = 0; j < desired_tile_stride; ++j) {
			pte =
				psb_gtt_mask_pte(page_to_pfn(*pages++), type);
			iowrite32(pte, cur_page++);
		}
		offset_pages += add;
	}
	(void) ioread32(cur_page - 1);

	return 0;
}

static int psb_gtt_insert_phys_addresses(struct psb_gtt *pg, IMG_CPU_PHYADDR *pPhysFrames,
				  uint32_t offset_pages, uint32_t num_pages,
				  int type)
{
	unsigned j;
	uint32_t __iomem *cur_page = NULL;
	uint32_t pte;

	//printk("Allocatng IMG GTT mem at %x (pages %d)\n",offset_pages,num_pages);

	cur_page = pg->gtt_map + offset_pages;
	for (j = 0; j < num_pages; ++j) {
		pte =  psb_gtt_mask_pte((pPhysFrames++)->uiAddr >> PAGE_SHIFT, type);
		iowrite32(pte, cur_page++);
		//printk("PTE %d: %x/%x\n",j,(pPhysFrames-1)->uiAddr,pte);
	}
	(void) ioread32(cur_page - 1);

	return 0;
}

int psb_gtt_remove_pages(struct psb_gtt *pg, uint32_t offset_pages,
			 uint32_t num_pages, unsigned desired_tile_stride,
			 unsigned hw_tile_stride)
{
	struct drm_psb_private *dev_priv = pg->dev->dev_private;
	unsigned rows = 1;
	unsigned add;
	unsigned row_add;
	unsigned i;
	unsigned j;
	uint32_t __iomem *cur_page = NULL;
	unsigned pfn_base = page_to_pfn(dev_priv->scratch_page);
	uint32_t pte = psb_gtt_mask_pte(pfn_base, 0);

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride;
	row_add = hw_tile_stride;

	for (i = 0; i < rows; ++i) {
		cur_page = pg->gtt_map + offset_pages;
		for (j = 0; j < desired_tile_stride; ++j)
			iowrite32(pte, cur_page++);

		offset_pages += add;
	}
	(void) ioread32(cur_page - 1);

	return 0;
}

int psb_gtt_mm_init(struct psb_gtt *pg)
{
	struct psb_gtt_mm *gtt_mm;
	struct drm_psb_private *dev_priv;
	struct drm_open_hash *ht;
	struct drm_mm *mm;
	int ret;
	uint32_t tt_start;
	uint32_t tt_size;

	if (!pg || !pg->initialized) {
		DRM_DEBUG("Invalid gtt struct\n");
		return -EINVAL;
	}

	dev_priv = pg->dev->dev_private;

	gtt_mm =  kzalloc(sizeof(struct psb_gtt_mm), GFP_KERNEL);
	if (!gtt_mm)
		return -ENOMEM;

	spin_lock_init(&gtt_mm->lock);

	ht = &gtt_mm->hash;
	ret = drm_ht_create(ht, 20);
	if (ret) {
		DRM_DEBUG("Create hash table failed(%d)\n", ret);
		goto err_free;
	}

	tt_start = (pg->stolen_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	tt_start = (tt_start < pg->gatt_pages) ? tt_start : pg->gatt_pages;
	tt_size = (pg->gatt_pages < PSB_TT_PRIV0_PLIMIT) ?
		  (pg->gatt_pages) : PSB_TT_PRIV0_PLIMIT;

	mm = &gtt_mm->base;

	/* will use tt_start ~ 128M for IMG TT buffers */
	ret = drm_mm_init(mm, tt_start, ((tt_size / 2) - tt_start));
	if (ret) {
		DRM_DEBUG("drm_mm_int error(%d)\n", ret);
		goto err_mm_init;
	}

	gtt_mm->count = 0;

	dev_priv->gtt_mm = gtt_mm;

	DRM_INFO("PSB GTT mem manager ready, tt_start %ld, tt_size %ld pages\n",
		 (unsigned long)tt_start,
		 (unsigned long)((tt_size / 2) - tt_start));
	return 0;
err_mm_init:
	drm_ht_remove(ht);

err_free:
	kfree(gtt_mm);
	return ret;
}

/**
 * Delete all hash entries;
 */
void psb_gtt_mm_takedown(void)
{
	return;
}

static struct psb_gtt_hash_entry *
psb_gtt_mm_get_ht_by_pid_locked(struct psb_gtt_mm *mm, u32 tgid)
{
	struct drm_hash_item *entry;
	struct psb_gtt_hash_entry *psb_entry;
	int ret;

	ret = drm_ht_find_item(&mm->hash, tgid, &entry);
	if (ret) {
		DRM_DEBUG("Cannot find entry pid=%u\n", tgid);
		return ERR_PTR(ret);
	}

	psb_entry = container_of(entry, struct psb_gtt_hash_entry, item);
	if (!psb_entry) {
		DRM_DEBUG("Invalid entry");
		return ERR_PTR(-EINVAL);
	}

	return psb_entry;
}


static int psb_gtt_mm_insert_ht_locked(struct psb_gtt_mm *mm,
				       u32 tgid,
				       struct psb_gtt_hash_entry *hentry)
{
	struct drm_hash_item *item;
	int ret;

	if (!hentry) {
		DRM_DEBUG("Invalid parameters\n");
		return -EINVAL;
	}

	item = &hentry->item;
	item->key = tgid;

	/**
	 * NOTE: drm_ht_insert_item will perform such a check
	ret = psb_gtt_mm_get_ht_by_pid(mm, tgid, &tmp);
	if (!ret) {
		DRM_DEBUG("Entry already exists for pid %ld\n", tgid);
		return -EAGAIN;
	}
	*/

	/* Insert the given entry */
	ret = drm_ht_insert_item(&mm->hash, item);
	if (ret) {
		DRM_DEBUG("Insert failure\n");
		return ret;
	}

	mm->count++;

	return 0;
}

static struct psb_gtt_hash_entry *
psb_gtt_mm_alloc_insert_ht(struct psb_gtt_mm *mm, u32 tgid)
{
	struct psb_gtt_hash_entry *hentry;
	int ret;

	/* if the hentry for this tgid exists, just get it and return */
	spin_lock(&mm->lock);
	hentry = psb_gtt_mm_get_ht_by_pid_locked(mm, tgid);
	if (!IS_ERR(hentry)) {
		DRM_DEBUG("Entry for tgid %u exist, hentry %p\n",
			  tgid, hentry);
		spin_unlock(&mm->lock);
		return hentry;
	}
	spin_unlock(&mm->lock);

	DRM_DEBUG("Entry for tgid %u doesn't exist, will create it\n", tgid);

	hentry = kzalloc(sizeof(struct psb_gtt_hash_entry), GFP_KERNEL);
	if (!hentry) {
		DRM_DEBUG("Kmalloc failled\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = drm_ht_create(&hentry->ht, 20);
	if (ret) {
		DRM_DEBUG("Create hash table failed\n");
		return ERR_PTR(ret);
	}

	spin_lock(&mm->lock);
	ret = psb_gtt_mm_insert_ht_locked(mm, tgid, hentry);
	spin_unlock(&mm->lock);

	if (!ret)
		return hentry;

	return ERR_PTR(ret);
}

static struct psb_gtt_hash_entry *
psb_gtt_mm_remove_ht_locked(struct psb_gtt_mm *mm, u32 tgid) {
	struct psb_gtt_hash_entry *tmp;

	tmp = psb_gtt_mm_get_ht_by_pid_locked(mm, tgid);
	if (IS_ERR(tmp)) {
		DRM_DEBUG("Cannot find entry pid %u\n", tgid);
		return NULL;
	}

	/* remove it from ht */
	drm_ht_remove_item(&mm->hash, &tmp->item);

	mm->count--;

	return tmp;
}

static int psb_gtt_mm_remove_free_ht_locked(struct psb_gtt_mm *mm, u32 tgid)
{
	struct psb_gtt_hash_entry *entry;

	entry = psb_gtt_mm_remove_ht_locked(mm, tgid);

	if (!entry) {
		DRM_DEBUG("Invalid entry");
		return -EINVAL;
	}

	/* delete ht */
	drm_ht_remove(&entry->ht);

	/* free this entry */
	kfree(entry);
	return 0;
}

static struct psb_gtt_mem_mapping *
psb_gtt_mm_get_mem_mapping_locked(struct drm_open_hash *ht, u32 key)
{
	struct drm_hash_item *entry;
	struct psb_gtt_mem_mapping *mapping;
	int ret;

	ret = drm_ht_find_item(ht, key, &entry);
	if (ret) {
		DRM_DEBUG("Cannot find key %u\n", key);
		return ERR_PTR(ret);
	}

	mapping =  container_of(entry, struct psb_gtt_mem_mapping, item);
	if (!mapping) {
		DRM_DEBUG("Invalid entry\n");
		return ERR_PTR(-EINVAL);
	}

	return mapping;
}

static int
psb_gtt_mm_insert_mem_mapping_locked(struct drm_open_hash *ht,
				     u32 key,
				     struct psb_gtt_mem_mapping *hentry)
{
	struct drm_hash_item *item;
	struct psb_gtt_hash_entry *entry;
	int ret;

	if (!hentry) {
		DRM_DEBUG("hentry is NULL\n");
		return -EINVAL;
	}

	item = &hentry->item;
	item->key = key;

	ret = drm_ht_insert_item(ht, item);
	if (ret) {
		DRM_DEBUG("insert_item failed\n");
		return ret;
	}

	entry = container_of(ht, struct psb_gtt_hash_entry, ht);
	if (entry)
		entry->count++;

	return 0;
}

static struct psb_gtt_mem_mapping *
psb_gtt_mm_alloc_insert_mem_mapping(struct psb_gtt_mm *mm,
				    struct drm_open_hash *ht,
				    u32 key,
				    struct drm_mm_node *node)
{
	struct psb_gtt_mem_mapping *mapping;
	int ret;

	if (!node || !ht) {
		DRM_DEBUG("parameter error\n");
		return ERR_PTR(-EINVAL);
	}

	/* try to get this mem_map */
	spin_lock(&mm->lock);
	mapping = psb_gtt_mm_get_mem_mapping_locked(ht, key);
	if (!IS_ERR(mapping)) {
		DRM_DEBUG("mapping entry for key %u exists, entry %p\n",
			  key, mapping);
		spin_unlock(&mm->lock);
		return mapping;
	}
	spin_unlock(&mm->lock);

	DRM_DEBUG("Mapping entry for key %u doesn't exist, will create it\n",
		  key);

	mapping = kzalloc(sizeof(struct psb_gtt_mem_mapping), GFP_KERNEL);
	if (!mapping) {
		DRM_DEBUG("kmalloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	mapping->node = node;

	spin_lock(&mm->lock);
	ret = psb_gtt_mm_insert_mem_mapping_locked(ht, key, mapping);
	spin_unlock(&mm->lock);

	if (!ret)
		return mapping;

	return ERR_PTR(ret);
}

static struct psb_gtt_mem_mapping *
psb_gtt_mm_remove_mem_mapping_locked(struct drm_open_hash *ht, u32 key) {
	struct psb_gtt_mem_mapping *tmp;
	struct psb_gtt_hash_entry *entry;

	tmp = psb_gtt_mm_get_mem_mapping_locked(ht, key);
	if (IS_ERR(tmp)) {
		DRM_DEBUG("Cannot find key %u\n", key);
		return NULL;
	}

	drm_ht_remove_item(ht, &tmp->item);

	entry = container_of(ht, struct psb_gtt_hash_entry, ht);
	if (entry)
		entry->count--;

	return tmp;
}

static struct drm_mm_node *
psb_gtt_mm_remove_free_mem_mapping_locked(struct drm_open_hash *ht, u32 key)
{
	struct psb_gtt_mem_mapping *entry;
	struct drm_mm_node *node;

	entry = psb_gtt_mm_remove_mem_mapping_locked(ht, key);
	if (!entry) {
		DRM_DEBUG("entry is NULL\n");
		return ERR_PTR(-EINVAL);
	}

	node = entry->node;

	kfree(entry);
	return node;
}

static struct psb_gtt_mem_mapping *
psb_gtt_add_node(struct psb_gtt_mm *mm, u32 tgid, u32 key,
		 struct drm_mm_node *node)
{
	struct psb_gtt_hash_entry *hentry;
	struct psb_gtt_mem_mapping *mapping;

	hentry = psb_gtt_mm_alloc_insert_ht(mm, tgid);
	if (IS_ERR(hentry)) {
		DRM_DEBUG("alloc_insert failed\n");
		return ERR_CAST(hentry);
	}

	mapping = psb_gtt_mm_alloc_insert_mem_mapping(mm, &hentry->ht, key,
						      node);
	if (IS_ERR(mapping)) {
		DRM_DEBUG("mapping alloc_insert failed\n");
		return ERR_CAST(mapping);
	}

	return mapping;
}

static struct drm_mm_node *
psb_gtt_remove_node(struct psb_gtt_mm *mm, u32 tgid, u32 key)
{
	struct psb_gtt_hash_entry *hentry;
	struct drm_mm_node *tmp;

	spin_lock(&mm->lock);
	hentry = psb_gtt_mm_get_ht_by_pid_locked(mm, tgid);
	if (IS_ERR(hentry)) {
		DRM_DEBUG("Cannot find entry for pid %u\n", tgid);
		spin_unlock(&mm->lock);
		return ERR_CAST(hentry);
	}
	spin_unlock(&mm->lock);

	/* remove mapping entry */
	spin_lock(&mm->lock);
	tmp = psb_gtt_mm_remove_free_mem_mapping_locked(&hentry->ht, key);
	if (IS_ERR(tmp)) {
		DRM_DEBUG("remove_free failed\n");
		spin_unlock(&mm->lock);
		return ERR_CAST(tmp);
	}

	/* check the count of mapping entry */
	if (!hentry->count) {
		DRM_DEBUG("count of mapping entry is zero, tgid=%u\n", tgid);
		psb_gtt_mm_remove_free_ht_locked(mm, tgid);
	}

	spin_unlock(&mm->lock);

	return tmp;
}

static struct drm_mm_node *
psb_gtt_mm_alloc_mem(struct psb_gtt_mm *mm, uint32_t pages, uint32_t align)
{
	struct drm_mm_node *node;
	int ret;

	do {
		ret = drm_mm_pre_get(&mm->base);
		if (unlikely(ret)) {
			DRM_DEBUG("drm_mm_pre_get error\n");
			return ERR_PTR(ret);
		}

		spin_lock(&mm->lock);
		node = drm_mm_search_free(&mm->base, pages, align, 1);
		if (unlikely(!node)) {
			DRM_DEBUG("No free node found\n");
			spin_unlock(&mm->lock);
			break;
		}

		node = drm_mm_get_block_atomic(node, pages, align);
		spin_unlock(&mm->lock);
	} while (!node);

	if (!node) {
		DRM_DEBUG("Node allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}

	return node;
}

static void psb_gtt_mm_free_mem(struct psb_gtt_mm *mm, struct drm_mm_node *node)
{
	spin_lock(&mm->lock);
	drm_mm_put_block(node);
	spin_unlock(&mm->lock);
}

static struct psb_gtt_mem_mapping *
psb_gtt_find_mapping_for_key(struct drm_device *dev, u32 tgid, u32 key)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt_mm *mm = dev_priv->gtt_mm;
	struct psb_gtt_hash_entry *hentry;
	struct psb_gtt_mem_mapping *mapping;

	spin_lock(&mm->lock);

	hentry = psb_gtt_mm_get_ht_by_pid_locked(mm, tgid);
	if (IS_ERR(hentry)) {
		spin_unlock(&mm->lock);
		return ERR_CAST(hentry);
	}

	mapping = psb_gtt_mm_get_mem_mapping_locked(&hentry->ht, key);

	spin_unlock(&mm->lock);

	return mapping;
}

static struct psb_gtt_mem_mapping *
psb_gtt_add_mapping(struct drm_device *dev, uint32_t pages, u32 tgid, u32 key, u32 align)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt_mm *mm = dev_priv->gtt_mm;
	struct drm_mm_node *node;
	struct psb_gtt_mem_mapping *mapping;

	/* alloc memory in TT apeture */
	node = psb_gtt_mm_alloc_mem(mm, pages, align);
	if (IS_ERR(node)) {
		DRM_DEBUG("alloc TT memory error\n");
		return ERR_CAST(node);
	}

	/* update psb_gtt_mm */
	mapping = psb_gtt_add_node(mm, tgid, key, node);
	if (IS_ERR(mapping)) {
		DRM_DEBUG("add_node failed");
		psb_gtt_mm_free_mem(mm, node);
		return mapping;
	}

	kref_init(&mapping->refcount);
	mapping->dev = dev;
	mapping->tgid = tgid;

	return mapping;
}

static int psb_gtt_remove_mapping(struct psb_gtt_mm *mm,
				  struct psb_gtt_mem_mapping *mapping)
{
	struct drm_mm_node *node;

	node = psb_gtt_remove_node(mm, mapping->tgid, mapping->item.key);
	if (IS_ERR(node)) {
		DRM_DEBUG("remove node failed\n");
		return PTR_ERR(node);
	}

	/* free tt node */

	psb_gtt_mm_free_mem(mm, node);

	return 0;
}

static struct psb_gtt_mem_mapping *
psb_gtt_insert_meminfo(struct drm_device *dev,
		       PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;
	uint32_t size, pages, offset_pages;
	void *kmem;
	struct page **page_list;
	struct psb_gtt_mem_mapping *mapping;
	int ret;

	size = psKernelMemInfo->ui32AllocSize;
	kmem = psKernelMemInfo->pvLinAddrKM;
	pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	DRM_DEBUG("KerMemInfo size %u, cpuVadr %p, pages %u, osMemHdl %p\n",
		  size, kmem, pages, psKernelMemInfo->sMemBlk.hOSMemHandle);

	if (!kmem)
		DRM_DEBUG("kmem is NULL");

	/* get pages */
	ret = psb_get_pages_by_mem_handle(psKernelMemInfo->sMemBlk.hOSMemHandle,
					  &page_list);
	if (ret) {
		DRM_DEBUG("get pages error\n");
		return ERR_PTR(ret);
	}

	DRM_DEBUG("get %u pages\n", pages);

	/* create mapping with node for handle */
	mapping = psb_gtt_add_mapping(dev, pages, psb_get_tgid(),
				      (u32) psKernelMemInfo, 0);
	if (IS_ERR(mapping))
		return ERR_CAST(mapping);

	offset_pages = mapping->node->start;

	/* update gtt */
	psb_gtt_insert_pages(pg, page_list, offset_pages, pages, 0, 0, 0);

	return mapping;
}

int psb_gtt_map_meminfo(struct drm_device *dev,
			PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo,
			uint32_t *offset)
{
	struct psb_gtt_mem_mapping *mapping;

	WARN_ON(!mutex_is_locked(&dev->mode_config.mutex));

	/* check if memory is already mapped */
	mapping = psb_gtt_find_mapping_for_key(dev, psb_get_tgid(),
					       (u32) psKernelMemInfo);
	if (IS_ERR(mapping))
		mapping = psb_gtt_insert_meminfo(dev, psKernelMemInfo);
	else
		kref_get(&mapping->refcount);

	if (IS_ERR(mapping))
		return PTR_ERR(mapping);

	*offset = mapping->node->start;
	return 0;
}

static void do_unmap_meminfo(struct kref *kref)
{
	struct drm_psb_private *dev_priv;
	struct psb_gtt_mm *mm;
	struct psb_gtt *pg;
	uint32_t pages, offset_pages;
	struct psb_gtt_mem_mapping *mapping;

	mapping = container_of(kref, struct psb_gtt_mem_mapping, refcount);
	mm = container_of(mapping->node->mm, struct psb_gtt_mm, base);
	dev_priv = mapping->dev->dev_private;
	pg = dev_priv->pg;

	/* remove gtt entries */
	offset_pages = mapping->node->start;
	pages = mapping->node->size;

	psb_gtt_remove_pages(pg, offset_pages, pages, 0, 0);

	psb_gtt_remove_mapping(mm, mapping);
}

int psb_gtt_unmap_meminfo(struct drm_device *dev,
			  PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo,
			  uint32_t tgid)
{
	struct psb_gtt_mem_mapping *mapping;

	WARN_ON(!mutex_is_locked(&dev->mode_config.mutex));

	mapping = psb_gtt_find_mapping_for_key(dev, tgid,
					       (u32) psKernelMemInfo);
	if (IS_ERR(mapping)) {
		DRM_DEBUG("handle is not mapped\n");
		return PTR_ERR(mapping);
	}

	kref_put(&mapping->refcount, do_unmap_meminfo);

	return 0;
}

int psb_gtt_map_meminfo_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct psb_gtt_mapping_arg *arg
	= (struct psb_gtt_mapping_arg *)data;
	uint32_t *offset_pages = &arg->offset_pages;
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;
	int ret;

	DRM_DEBUG("\n");

	WARN_ON(1);

	mutex_lock(&dev->mode_config.mutex);

	mutex_lock(&gPVRSRVLock);

	ret = psb_get_meminfo_by_handle(arg->hKernelMemInfo, &psKernelMemInfo);

	if (!ret)
		ret = psb_gtt_map_meminfo(dev, psKernelMemInfo, offset_pages);

	mutex_unlock(&gPVRSRVLock);

	mutex_unlock(&dev->mode_config.mutex);

	return ret;
}

int psb_gtt_unmap_meminfo_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{

	struct psb_gtt_mapping_arg *arg
	= (struct psb_gtt_mapping_arg *)data;
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;
	int ret;

	DRM_DEBUG("\n");

	WARN_ON(1);

	mutex_lock(&dev->mode_config.mutex);

	mutex_lock(&gPVRSRVLock);

	ret = psb_get_meminfo_by_handle(arg->hKernelMemInfo, &psKernelMemInfo);

	if (!ret)
		ret = psb_gtt_unmap_meminfo(dev, psKernelMemInfo, psb_get_tgid());

	mutex_unlock(&gPVRSRVLock);

	mutex_unlock(&dev->mode_config.mutex);

	return ret;
}

int psb_gtt_map_pvr_memory(struct drm_device *dev,
			   void *hHandle,
			   uint32_t ui32TaskId,
			   IMG_CPU_PHYADDR *pPages,
			   unsigned int ui32PagesNum,
			   unsigned int *ui32Offset,
			   unsigned int ui32Align)
{
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct psb_gtt * pg = dev_priv->pg;

	uint32_t size, pages, offset_pages;
	struct psb_gtt_mem_mapping * mapping = NULL;

	size = ui32PagesNum * PAGE_SIZE;
	pages = 0;

	/* check if memory is already mapped */
	mapping = psb_gtt_find_mapping_for_key(dev, ui32TaskId,
					       (u32) hHandle);
	if (!IS_ERR(mapping)) {
		*ui32Offset = mapping->node->start;
		kref_get(&mapping->refcount);
		return 0;
	}

	mapping = psb_gtt_add_mapping(dev, ui32PagesNum, ui32TaskId,
				      (u32) hHandle, ui32Align);
	if (IS_ERR(mapping))
		return PTR_ERR(mapping);

	offset_pages = mapping->node->start;

	DRM_DEBUG("get free node for %u pages, offset %u pages", pages, offset_pages);

	/* update gtt */
	psb_gtt_insert_phys_addresses(pg, pPages, offset_pages,
				      ui32PagesNum, 0);

	*ui32Offset = offset_pages;
	return 0;
}


static void do_unmap_pvr_memory(struct kref *kref)
{
	struct drm_psb_private *dev_priv;
	struct psb_gtt_mm *mm;
	struct psb_gtt *pg;
	uint32_t pages, offset_pages;
	struct psb_gtt_mem_mapping *mapping;

	mapping = container_of(kref, struct psb_gtt_mem_mapping, refcount);
	mm = container_of(mapping->node->mm, struct psb_gtt_mm, base);
	dev_priv = (struct drm_psb_private *) mapping->dev->dev_private;
	pg = dev_priv->pg;

	/* remove gtt entries */
	offset_pages = mapping->node->start;
	pages = mapping->node->size;

	psb_gtt_remove_pages(pg, offset_pages, pages, 0, 0);

	psb_gtt_remove_mapping(mm, mapping);
}

int psb_gtt_unmap_pvr_memory(struct drm_device *dev, void *hHandle,
			     uint32_t ui32TaskId)
{
	struct psb_gtt_mem_mapping *mapping;

	mapping = psb_gtt_find_mapping_for_key(dev, ui32TaskId,
					       (u32) hHandle);
	if (IS_ERR(mapping)) {
		DRM_DEBUG("cannot find mapping for pvr memory\n");
		return PTR_ERR(mapping);
	}

	kref_put(&mapping->refcount, do_unmap_pvr_memory);

	return 0;
}

/**************************************************************************
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
 **************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/console.h>

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>

#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_drv.h"
#include "psb_ttm_userobj_api.h"
#include "psb_fb.h"
#include "psb_pvr_glue.h"
#include "psb_page_flip.h"

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_output.h"
#include "mdfld_output.h"
#include "pvr_trace_cmd.h"

#include "ossync.h"

extern struct mutex gPVRSRVLock;

extern int MRSTLFBHandleChangeFB(struct drm_device* dev, struct psb_framebuffer *psbfb);

static void psb_user_framebuffer_destroy(struct drm_framebuffer *fb);
static int psb_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					      struct drm_file *file_priv,
					      unsigned int *handle);
static int psb_fb_ref_locked(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);

static const struct drm_framebuffer_funcs psb_fb_funcs = {
	.destroy = psb_user_framebuffer_destroy,
	.create_handle = psb_user_framebuffer_create_handle,
};

#define CMAP_TOHW(_val, _width) ((((_val) << (_width)) + 0x7FFF - (_val)) >> 16)

static int psbfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	uint32_t v;

	if (!fb)
		return -ENOMEM;

	if (regno > 255)
		return 1;

	red = CMAP_TOHW(red, info->var.red.length);
	blue = CMAP_TOHW(blue, info->var.blue.length);
	green = CMAP_TOHW(green, info->var.green.length);
	transp = CMAP_TOHW(transp, info->var.transp.length);

	v = (red << info->var.red.offset) |
	    (green << info->var.green.offset) |
	    (blue << info->var.blue.offset) |
	    (transp << info->var.transp.offset);

	if (regno < 16) {
		switch (fb->bits_per_pixel) {
		case 16:
			((uint32_t *) info->pseudo_palette)[regno] = v;
			break;
		case 24:
		case 32:
			((uint32_t *) info->pseudo_palette)[regno] = v;
			break;
		}
	}

	return 0;
}

static int psbfb_kms_off(struct drm_device *dev, int suspend)
{
	struct drm_framebuffer *fb = NULL;
	struct psb_framebuffer * psbfb = to_psb_fb(fb);
	DRM_DEBUG("psbfb_kms_off_ioctl\n");

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(fb, &dev->mode_config.fb_list, head) {
		struct fb_info *info = psbfb->fbdev;

		if (suspend) {
			fb_set_suspend(info, 1);
			drm_fb_helper_blank(FB_BLANK_POWERDOWN, info);
		}
	}
	mutex_unlock(&dev->mode_config.mutex);
	return 0;
}

int psbfb_kms_off_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	int ret;

	if (drm_psb_no_fb)
		return 0;
	console_lock();
	ret = psbfb_kms_off(dev, 0);
	console_unlock();

	return ret;
}

static int psbfb_kms_on(struct drm_device *dev, int resume)
{
	struct drm_framebuffer *fb = NULL;
	struct psb_framebuffer * psbfb = to_psb_fb(fb);

	DRM_DEBUG("psbfb_kms_on_ioctl\n");

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(fb, &dev->mode_config.fb_list, head) {
		struct fb_info *info = psbfb->fbdev;

		if (resume) {
			fb_set_suspend(info, 0);
			drm_fb_helper_blank(FB_BLANK_UNBLANK, info);
		}
	}
	mutex_unlock(&dev->mode_config.mutex);

	return 0;
}

int psbfb_kms_on_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	int ret;

	if (drm_psb_no_fb)
		return 0;
	console_lock();
	ret = psbfb_kms_on(dev, 0);
	console_unlock();
	drm_helper_disable_unused_functions(dev);
	return ret;
}

static void psbfb_suspend(struct drm_device *dev)
{
	console_lock();
	psbfb_kms_off(dev, 1);
	console_unlock();
}

static void psbfb_resume(struct drm_device *dev)
{
	console_lock();
	psbfb_kms_on(dev, 1);
	console_unlock();
	drm_helper_disable_unused_functions(dev);
}

static struct fb_ops psbfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcolreg = psbfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static struct drm_framebuffer *psb_framebuffer_create
			(struct drm_device *dev, struct drm_mode_fb_cmd2 *r,
			 void *mm_private)
{
	struct psb_framebuffer *fb;
	int ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return NULL;

	ret = drm_framebuffer_init(dev, &fb->base, &psb_fb_funcs);

	if (ret)
		goto err;

	drm_helper_mode_fill_fb_struct(&fb->base, r);

	fb->pvrBO = mm_private;

	return &fb->base;

err:
	kfree(fb);
	return NULL;
}

static bool need_gtt(struct drm_device *dev,
		     PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;

	if (!psKernelMemInfo)
		return false;

	/* FIXME can this actually be true for user framebuffers? */
	return psKernelMemInfo->pvLinAddrKM != pg->vram_addr;
}

static struct drm_framebuffer *psb_user_framebuffer_create
			(struct drm_device *dev, struct drm_file *filp,
			 struct drm_mode_fb_cmd2 *r)
{
	struct psb_framebuffer *psbfb;
	struct drm_framebuffer *fb;
	struct fb_info *info;
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo = IMG_NULL;
	IMG_HANDLE hKernelMemInfo = (IMG_HANDLE)r->handles[0];
	struct drm_psb_private *dev_priv
		= (struct drm_psb_private *) dev->dev_private;
	struct psb_fbdev * fbdev = dev_priv->fbdev;
	int ret;
	uint32_t offset;
	uint64_t sizes[4] = {};
	int i;

	mutex_lock(&gPVRSRVLock);

	ret = psb_get_meminfo_by_handle(hKernelMemInfo, &psKernelMemInfo);

	if (!ret) {
		psKernelMemInfo = PVRSRVGetSrcMemInfo(psKernelMemInfo);
		ret = psb_fb_ref_locked(psKernelMemInfo);
	}

	mutex_unlock(&gPVRSRVLock);

	if (ret) {
		DRM_ERROR("Cannot get meminfo for handle 0x%x\n",
			  (IMG_UINT32)hKernelMemInfo);
		goto out;
	}

	DRM_DEBUG("Got Kernel MemInfo for handle %p\n", hKernelMemInfo);

	sizes[0] = psKernelMemInfo->ui32AllocSize;

	for (i = 1; i < drm_format_num_planes(r->pixel_format); i++) {
		/* support only one handle per fb for now */
		if (r->handles[i] != r->handles[0]) {
			DRM_ERROR("bad handle 0x%x (expected 0x%x).\n",
				  r->handles[i], r->handles[0]);
			ret = -EINVAL;
			goto unref_fb;
		}
		sizes[i] = sizes[0];
	}

	ret = drm_framebuffer_check(r, sizes);
	if (ret) {
		DRM_ERROR("framebuffer layout check failed.\n");
		goto unref_fb;
	}

	/* JB: TODO not drop, refcount buffer */
	/* return psb_framebuffer_create(dev, r, bo); */

	fb = psb_framebuffer_create(dev, r, (void *)psKernelMemInfo);
	if (!fb) {
		DRM_ERROR("failed to allocate fb.\n");
		ret = -ENOMEM;
		goto unref_fb;
	}

	psbfb = to_psb_fb(fb);
	psbfb->size = sizes[0];

	psbfb->tgid = psb_get_tgid();

	DRM_DEBUG("Mapping to gtt..., KernelMemInfo %p\n", psKernelMemInfo);

	/*if not VRAM, map it into tt aperture*/
	if (need_gtt(dev, psKernelMemInfo)) {
		ret = psb_gtt_map_meminfo(dev, psKernelMemInfo, &offset);
		if (ret) {
			DRM_ERROR("map meminfo for 0x%x failed\n",
				  (IMG_UINT32)hKernelMemInfo);
			goto free_fb;
		}
		psbfb->offset = (offset << PAGE_SHIFT);
	} else {
		psbfb->offset = 0;
	}

	/* FIXME user framebuffers should not touch fbdev state */
	info = framebuffer_alloc(0, &dev->pdev->dev);
	if (!info) {
		ret = -ENOMEM;
		goto unmap_gtt;
	}

	info->par = fbdev;
	strcpy(info->fix.id, "psbfb");

	info->flags = FBINFO_DEFAULT;
	info->fbops = &psbfb_ops;

	info->fix.smem_start = dev->mode_config.fb_base;
	info->fix.smem_len = sizes[0];

	info->screen_base = psKernelMemInfo->pvLinAddrKM;
	info->screen_size = sizes[0];

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &fbdev->psb_fb_helper, fb->width, fb->height);

	info->fix.mmio_start = pci_resource_start(dev->pdev, 0);
	info->fix.mmio_len = pci_resource_len(dev->pdev, 0);

	info->pixmap.size = 64 * 1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	psbfb->fbdev = info;
	fbdev->pfb = psbfb;

	fb->helper_private = &(fbdev->psb_fb_helper);
	fbdev->psb_fb_helper.fb = fb;
	fbdev->psb_fb_helper.fbdev = info;
	MRSTLFBHandleChangeFB(dev, psbfb);

	return fb;

 unmap_gtt:
	if (need_gtt(dev, psKernelMemInfo))
		psb_gtt_unmap_meminfo(dev, psKernelMemInfo, psbfb->tgid);
 free_fb:
	kfree(psbfb);
 unref_fb:
	psb_fb_unref(psKernelMemInfo, psb_get_tgid());
 out:
	return ERR_PTR(ret);
}

static int psbfb_create(struct psb_fbdev * fbdev, struct drm_fb_helper_surface_size * sizes) 
{
	struct drm_device * dev = fbdev->psb_fb_helper.dev;
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;
	struct fb_info * info;
	struct drm_framebuffer *fb;
	struct psb_framebuffer * psbfb;
	struct drm_mode_fb_cmd2 mode_cmd = {};
	struct device * device = &dev->pdev->dev;
	int size, aligned_size;
	int ret;
	unsigned int depth;
	int bpp;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	mode_cmd.pixel_format = DRM_FORMAT_XRGB8888;
	drm_helper_get_fb_bpp_depth(mode_cmd.pixel_format, &depth, &bpp);

	/* HW requires pitch to be 64 byte aligned */
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * bpp / 8, 64);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	aligned_size = ALIGN(size, PAGE_SIZE);

	if (aligned_size > pg->vram_stolen_size) {
		DRM_ERROR("not enough stolen memory for fb.\n");
		return -ENOMEM;
	}

	mutex_lock(&dev->struct_mutex);
        fb = psb_framebuffer_create(dev, &mode_cmd, NULL);
        if (!fb) {
                DRM_ERROR("failed to allocate fb.\n");
                ret = -ENOMEM;
                goto out_err0;
        }
        psbfb = to_psb_fb(fb);
        psbfb->size = size;

	info = framebuffer_alloc(sizeof(struct psb_fbdev), device);
	if(!info) {
		ret = -ENOMEM;
		goto out_err1;
	}

	info->par = &fbdev->psb_fb_helper;

	psbfb->fbdev = info;

	fb->helper_private = &(fbdev->psb_fb_helper);
	fbdev->psb_fb_helper.fb = fb;
	fbdev->psb_fb_helper.fbdev = info;
	fbdev->pfb = psbfb;

	strcpy(info->fix.id, "psbfb");

	info->flags = FBINFO_DEFAULT;
	info->fbops = &psbfb_ops;
	info->fix.smem_start = dev->mode_config.fb_base;
	info->fix.smem_len = size;
	info->screen_base = (char *)pg->vram_addr;
	info->screen_size = size;
	memset(info->screen_base, 0, size);

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &fbdev->psb_fb_helper, sizes->fb_width, sizes->fb_height);

	info->pixmap.size = 64 * 1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	DRM_DEBUG("fb depth is %d\n", fb->depth);
	DRM_DEBUG("   pitch is %d\n", fb->pitches[0]);

	printk(KERN_INFO"allocated %dx%d fb\n", psbfb->base.width, psbfb->base.height);	

	mutex_unlock(&dev->struct_mutex);

	return 0;
out_err1:
	fb->funcs->destroy(fb);
out_err0:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

static void psbfb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green, u16 blue, int regno)
{
	DRM_DEBUG("%s\n", __FUNCTION__);
}

static void psbfb_gamma_get(struct drm_crtc *crtc, u16 *red, u16 *green, u16 *blue, int regno)
{
	DRM_DEBUG("%s\n", __FUNCTION__);
}

static int psbfb_probe(struct drm_fb_helper *helper, struct drm_fb_helper_surface_size *sizes)
{
	struct psb_fbdev * psb_fbdev = (struct psb_fbdev *)helper;
	int new_fb = 0;
	int ret;
	struct drm_device *dev = helper->dev;
	struct drm_display_mode *dsi_mode = NULL;

	DRM_DEBUG("%s\n", __FUNCTION__);

	/* Mode management between local and external displays:
	 * Frame buffer and surface dimensions will always be equal to
	 * local display mode value.
	 */

	if (sizes != NULL) {
		/* local display desired mode */
		dsi_mode = helper->crtc_info[0].desired_mode;
		if (dsi_mode) {
			DRM_INFO("setting fb and surface dimensions to %dx%d\n",
					dsi_mode->hdisplay, dsi_mode->vdisplay);
			sizes->fb_width  = dsi_mode->hdisplay;
			sizes->fb_height = dsi_mode->vdisplay;
			sizes->surface_width  = dsi_mode->hdisplay;
			sizes->surface_height = dsi_mode->vdisplay;
		}
	}

	if(!helper->fb) {
		ret = psbfb_create(psb_fbdev, sizes);
		if(ret) {
			return ret;
		}

		new_fb = 1;
	}

	return new_fb;
}

static struct drm_fb_helper_funcs psb_fb_helper_funcs = {
	.gamma_set = psbfb_gamma_set,
	.gamma_get = psbfb_gamma_get,
	.fb_probe = psbfb_probe,
};

static int psb_fbdev_destroy(struct drm_device * dev, struct psb_fbdev * fbdev)
{
	struct fb_info * info;
	struct psb_framebuffer * psbfb = fbdev->pfb;

	if(fbdev->psb_fb_helper.fbdev) {
		info = fbdev->psb_fb_helper.fbdev;
		unregister_framebuffer(info);
		framebuffer_release(info);
	}

	drm_fb_helper_fini(&fbdev->psb_fb_helper);

	drm_framebuffer_cleanup(&psbfb->base);
	
	return 0;
}

int psb_fbdev_init(struct drm_device * dev) 
{
	struct psb_fbdev * fbdev;
	struct drm_psb_private * dev_priv = 
		(struct drm_psb_private *)dev->dev_private;
	int num_crtc;
	
	fbdev = kzalloc(sizeof(struct psb_fbdev), GFP_KERNEL);
	if(!fbdev) {
		DRM_ERROR("no memory\n");
		return -ENOMEM;
	}

	dev_priv->fbdev = fbdev;
	fbdev->psb_fb_helper.funcs = &psb_fb_helper_funcs;

	/*FIXME: how many crtc will MDFL support?*/
	num_crtc = 3;

	drm_fb_helper_init(dev, &fbdev->psb_fb_helper, num_crtc, INTELFB_CONN_LIMIT);

	drm_fb_helper_single_add_all_connectors(&fbdev->psb_fb_helper);
	drm_fb_helper_initial_config(&fbdev->psb_fb_helper, 32);
	return 0;
}

static void psb_fbdev_fini(struct drm_device * dev)
{
	struct drm_psb_private * dev_priv = 
		(struct drm_psb_private *)dev->dev_private;

	if(!dev_priv->fbdev) {
		return;
	}

	psb_fbdev_destroy(dev, dev_priv->fbdev);
	kfree(dev_priv->fbdev);
	dev_priv->fbdev = NULL;
}

static void psbfb_output_poll_changed(struct drm_device * dev)
{
	struct drm_psb_private * dev_priv = (struct drm_psb_private *)dev->dev_private;
	struct psb_fbdev * fbdev = (struct psb_fbdev *)dev_priv->fbdev;
	drm_fb_helper_hotplug_event(&fbdev->psb_fb_helper);
}

int psbfb_remove(struct drm_device *dev, struct drm_framebuffer *fb)
{
	struct fb_info *info;
	struct psb_framebuffer * psbfb = to_psb_fb(fb);

	if (drm_psb_no_fb)
		return 0;

	info = psbfb->fbdev;
	psbfb->pvrBO = NULL;
	MRSTLFBHandleChangeFB(dev, psbfb);

	if (info) {
		framebuffer_release(info);
	}

	return 0;
}
/*EXPORT_SYMBOL(psbfb_remove); */

static int psb_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					      struct drm_file *file_priv,
					      unsigned int *handle)
{
	/* JB: TODO currently we can't go from a bo to a handle with ttm */
	(void) file_priv;
	*handle = 0;
	return 0;
}

static void psb_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct drm_device *dev = fb->dev;
	struct psb_framebuffer *psbfb = to_psb_fb(fb);

	/*ummap gtt pages*/
	if (need_gtt(dev, psbfb->pvrBO))
		psb_gtt_unmap_meminfo(dev, psbfb->pvrBO, psbfb->tgid);

	psb_fb_unref(psbfb->pvrBO, psbfb->tgid);

	if (psbfb->fbdev)
		psbfb_remove(dev, fb);

	/* JB: TODO not drop, refcount buffer */
	drm_framebuffer_cleanup(fb);

	kfree(psbfb);
}

static const struct drm_mode_config_funcs psb_mode_funcs = {
	.fb_create = psb_user_framebuffer_create,
	.output_poll_changed = psbfb_output_poll_changed,
};

static int psb_create_backlight_property(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv
				= (struct drm_psb_private *) dev->dev_private;
	struct drm_property *backlight;

	if (dev_priv->backlight_property)
		return 0;

	backlight = drm_property_create(dev,
					DRM_MODE_PROP_RANGE,
					"backlight",
					2);
	backlight->values[0] = 0;
	backlight->values[1] = 100;

	dev_priv->backlight_property = backlight;

	return 0;
}

static void psb_setup_outputs(struct drm_device *dev)
{
	struct drm_connector *connector;

	PSB_DEBUG_ENTRY("\n");

	drm_mode_create_scaling_mode_property(dev);

	psb_create_backlight_property(dev);

	mdfld_output_init(dev);

	list_for_each_entry(connector, &dev->mode_config.connector_list,
			    head) {
		struct psb_intel_output *psb_intel_output =
		    to_psb_intel_output(connector);
		struct drm_encoder *encoder = &psb_intel_output->enc;
		int crtc_mask = 0, clone_mask = 0;

		/* valid crtcs */
		switch (psb_intel_output->type) {
		case INTEL_OUTPUT_SDVO:
			crtc_mask = ((1 << 0) | (1 << 1));
			clone_mask = (1 << INTEL_OUTPUT_SDVO);
			break;
		case INTEL_OUTPUT_LVDS:
			PSB_DEBUG_ENTRY("LVDS. \n");
			crtc_mask = (1 << 1);

			clone_mask = (1 << INTEL_OUTPUT_LVDS);
			break;
		case INTEL_OUTPUT_MIPI:
			PSB_DEBUG_ENTRY("MIPI. \n");
			crtc_mask = (1 << 0);
			clone_mask = (1 << INTEL_OUTPUT_MIPI);
			break;
		case INTEL_OUTPUT_MIPI2:
			PSB_DEBUG_ENTRY("MIPI2. \n");
			crtc_mask = (1 << 2);
			clone_mask = (1 << INTEL_OUTPUT_MIPI2);
			break;
		case INTEL_OUTPUT_HDMI:
			PSB_DEBUG_ENTRY("HDMI. \n");
			crtc_mask = (1 << 1);
			clone_mask = (1 << INTEL_OUTPUT_HDMI);
			break;
		}

		encoder->possible_crtcs = crtc_mask;
		encoder->possible_clones =
		    psb_intel_connector_clones(dev, clone_mask);

	}
}

static void *psb_bo_from_handle(struct drm_device *dev,
				struct drm_file *file_priv,
				unsigned int handle)
{
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo = IMG_NULL;
	IMG_HANDLE hKernelMemInfo = (IMG_HANDLE)handle;
	int ret;

	mutex_lock(&gPVRSRVLock);

	ret = psb_get_meminfo_by_handle(hKernelMemInfo, &psKernelMemInfo);

	if (!ret) {
		psKernelMemInfo = PVRSRVGetSrcMemInfo(psKernelMemInfo);
		ret = psb_fb_ref_locked(psKernelMemInfo);
	}

	mutex_unlock(&gPVRSRVLock);

	if (ret) {
		DRM_ERROR("Cannot get meminfo for handle 0x%x\n",
			  (IMG_UINT32)hKernelMemInfo);
		return NULL;
	}

	return (void *)psKernelMemInfo;
}

static void psb_bo_unref(struct drm_device *dev, void *bof)
{
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo	= (PVRSRV_KERNEL_MEM_INFO *)bof;

	psb_fb_unref(psKernelMemInfo, psb_get_tgid());
}

static size_t psb_bo_size(struct drm_device *dev, void *bof)
{
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo	= (PVRSRV_KERNEL_MEM_INFO *)bof;
	return (size_t)psKernelMemInfo->ui32AllocSize;
}

static int psb_bo_pin_for_scanout(struct drm_device *dev, void *bo)
{
	 return 0;
}

static int psb_bo_unpin_for_scanout(struct drm_device *dev, void *bo) 
{
	return 0;
}

void psb_modeset_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *) dev->dev_private;
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;
	int i;

	PSB_DEBUG_ENTRY("\n");
	/* Init mm functions */
	mode_dev->bo_from_handle = psb_bo_from_handle;
	mode_dev->bo_unref = psb_bo_unref;
	mode_dev->bo_size = psb_bo_size;
	mode_dev->bo_pin_for_scanout = psb_bo_pin_for_scanout;
	mode_dev->bo_unpin_for_scanout = psb_bo_unpin_for_scanout;

	psb_page_flip_init(dev);

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	dev->mode_config.funcs = (void *) &psb_mode_funcs;

	/* set memory base */
	/* MRST and PSB should use BAR 2*/
	pci_read_config_dword(dev->pdev, PSB_BSM, (uint32_t *) &(dev->mode_config.fb_base));

	for (i = 0; i < dev_priv->num_pipe; i++)
		psb_intel_crtc_init(dev, i, mode_dev);

	for (i = 0; i < 2; i++)
		mdfld_overlay_init(dev, i);

	dev->mode_config.max_width = dev->mode_config.num_crtc * MDFLD_PLANE_MAX_WIDTH;
	dev->mode_config.max_height = dev->mode_config.num_crtc * MDFLD_PLANE_MAX_HEIGHT;

	psb_setup_outputs(dev);

	/* setup fbs */
	/* drm_initial_config(dev); */
}

void psb_modeset_cleanup(struct drm_device *dev)
{
	mutex_lock(&dev->struct_mutex);

	drm_kms_helper_poll_fini(dev);
	psb_fbdev_fini(dev);
	
	drm_mode_config_cleanup(dev);

	psb_page_flip_fini(dev);

	mutex_unlock(&dev->struct_mutex);
}

static int psb_fb_ref_locked(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	u32 tgid = psb_get_tgid();

	if (!psKernelMemInfo)
		return 0;

	/*
	 * Make sure we have per process data. If we just call
	 * PVRSRVPerProcessDataConnect() w/o per process data,
	 * it will allocate the data for us, which is not what
	 * we want.
	 */
	if (!PVRSRVPerProcessData(tgid) ||
	    PVRSRVPerProcessDataConnect(tgid, 0) != PVRSRV_OK)
		return -ESRCH;

	PVRSRVRefDeviceMemKM(psKernelMemInfo);

	return 0;
}

int psb_fb_ref(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	int ret;

	if (!psKernelMemInfo)
		return 0;

	mutex_lock(&gPVRSRVLock);

	ret = psb_fb_ref_locked(psKernelMemInfo);

	mutex_unlock(&gPVRSRVLock);

	return ret;
}

void psb_fb_unref(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo, u32 tgid)
{
	if (!psKernelMemInfo)
		return;

	mutex_lock(&gPVRSRVLock);
	PVRSRVUnrefDeviceMemKM(psKernelMemInfo);
	PVRSRVPerProcessDataDisconnect(tgid);
	mutex_unlock(&gPVRSRVLock);
}

int psb_fb_gtt_ref(struct drm_device *dev,
		   PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	uint32_t offset;
	int ret;

	if (!psKernelMemInfo)
		return 0;

	ret = psb_fb_ref(psKernelMemInfo);
	if (ret)
		goto out;

	if (need_gtt(dev, psKernelMemInfo)) {
		ret = psb_gtt_map_meminfo(dev, psKernelMemInfo, &offset);
		if (ret)
			goto unref_fb;
	}

	return 0;

 unref_fb:
	psb_fb_unref(psKernelMemInfo, psb_get_tgid());
 out:
	return ret;
}

void psb_fb_gtt_unref(struct drm_device *dev,
		      PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo,
		      u32 tgid)
{
	if (!psKernelMemInfo)
		return;

	if (need_gtt(dev, psKernelMemInfo))
		psb_gtt_unmap_meminfo(dev, psKernelMemInfo, tgid);

	psb_fb_unref(psKernelMemInfo, tgid);
}

void
psb_fb_increase_read_ops_pending(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo,
		struct psb_pending_values *pending)
{
	PVRSRV_SYNC_DATA *sync_data;
	if (!psKernelMemInfo || !psKernelMemInfo->psKernelSyncInfo)
		return;

	sync_data = psKernelMemInfo->psKernelSyncInfo->psSyncData;
	pending->write = sync_data->ui32WriteOpsPending;
	pending->read = sync_data->ui32ReadOpsPending++;
}

int
psb_fb_increase_read_ops_completed(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo,
		struct psb_pending_values *pending,
		struct pvr_pending_sync *pending_sync)
{
	PVRSRV_SYNC_DATA *sync_data;
	if (!psKernelMemInfo || !psKernelMemInfo->psKernelSyncInfo)
		return 0;

	sync_data = psKernelMemInfo->psKernelSyncInfo->psSyncData;
	if (unlikely(
	    pvr_ops_after(pending->write, sync_data->ui32WriteOpsComplete) ||
	    pvr_ops_after(pending->read, sync_data->ui32ReadOpsComplete))) {
		/* Has to wait SGX micorkernel to complete parallel reads to
		 * avoid deadlock.
		 */

		pending_sync->sync_info = psKernelMemInfo->psKernelSyncInfo;
		pending_sync->pending_read_ops = pending->read;
		pending_sync->pending_write_ops = pending->write;
		pending_sync->flags = PVRSRV_SYNC_WRITE | PVRSRV_SYNC_READ;

		PVRSRVCallbackOnSync2(pending_sync);

		return -EBUSY;
	}
	sync_data->ui32ReadOpsComplete++;
	pvr_trcmd_check_syn_completions(PVR_TRCMD_FLPCOMP);
	return 0;
}

void
psb_fb_flip_trace(PVRSRV_KERNEL_MEM_INFO *old, PVRSRV_KERNEL_MEM_INFO *new)
{
	struct pvr_trcmd_flpreq *fltrace;

	fltrace = pvr_trcmd_reserve(PVR_TRCMD_FLPREQ, task_tgid_nr(current),
				  current->comm, sizeof(*fltrace));
	if (old && old->psKernelSyncInfo)
		pvr_trcmd_set_syn(&fltrace->old_syn, old->psKernelSyncInfo);
	else
		pvr_trcmd_clear_syn(&fltrace->old_syn);
	if (new && new->psKernelSyncInfo)
		pvr_trcmd_set_syn(&fltrace->new_syn, new->psKernelSyncInfo);
	else
		pvr_trcmd_clear_syn(&fltrace->new_syn);
	pvr_trcmd_commit(fltrace);
}

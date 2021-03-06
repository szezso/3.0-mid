/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Jim Bish <jim.bish@@intel.com>
 */

#ifdef CONFIG_MDFD_GL3

#include "mdfld_gl3.h"
#include "psb_powermgmt.h"

void gl3_enable(void)
{
	struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_ENTRY("gl3_enable called on platform %x\n",	dev_priv->platform_rev_id);
	if (!ospm_power_using_hw_begin(OSPM_GRAPHICS_ISLAND, true))
		return;
	MDFLD_GL3_WRITE(MDFLD_GL3_ENABLE_CACHE, MDFLD_GL3_CONTROL);
	PSB_DEBUG_GENERAL("gl3 cache enabled with mask %x\n",
			MDFLD_GL3_ENABLE_CACHE);
	ospm_power_using_hw_end(OSPM_GRAPHICS_ISLAND);
}

void gl3_disable(void)
{
	struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_ENTRY("gl3_disable called on platform %x\n",	dev_priv->platform_rev_id);
	if (!ospm_power_using_hw_begin(OSPM_GRAPHICS_ISLAND, true))
		return;
	MDFLD_GL3_WRITE(MDFLD_GL3_DISABLE_CACHE, MDFLD_GL3_CONTROL);
	PSB_DEBUG_GENERAL("gl3 cache disabled with mask %x\n",
			MDFLD_GL3_DISABLE_CACHE);
	ospm_power_using_hw_end(OSPM_GRAPHICS_ISLAND);
}

void gl3_invalidate(void)
{
	struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_ENTRY("gl3_invalidate called on platform %x\n",	dev_priv->platform_rev_id);
	if (!ospm_power_using_hw_begin(OSPM_GRAPHICS_ISLAND, true))
		return;
	// Invalidate the cache
	MDFLD_GL3_WRITE(MDFLD_GL3_INVALIDATE_CACHE, MDFLD_GL3_CONTROL);
	PSB_DEBUG_GENERAL("gl3 cache invalidated with mask %x\n",
			MDFLD_GL3_INVALIDATE_CACHE);
	ospm_power_using_hw_end(OSPM_GRAPHICS_ISLAND);
}

void gl3_flush(void)
{
	struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_ENTRY("gl3_flush called on platform %x\n",	dev_priv->platform_rev_id);
	if (!ospm_power_using_hw_begin(OSPM_GRAPHICS_ISLAND, true))
		return;
	// Flush the cache
	MDFLD_GL3_WRITE(MDFLD_GL3_FLUSH_CACHE, MDFLD_GL3_CONTROL);
	PSB_DEBUG_GENERAL("gl3 cache flushed with mask %x\n",
			MDFLD_GL3_FLUSH_CACHE);
	ospm_power_using_hw_end(OSPM_GRAPHICS_ISLAND);
}

void gl3_reset(void)
{
	struct drm_psb_private *dev_priv =
		    (struct drm_psb_private *) gpDrmDevice->dev_private;

	PSB_DEBUG_ENTRY("gl3_reset called on platform %x\n",	dev_priv->platform_rev_id);
	if (!ospm_power_using_hw_begin(OSPM_GRAPHICS_ISLAND, true))
		return;
	// Reset the cache
	MDFLD_GL3_WRITE(MDFLD_GL3_SOFT_RESET_ENABLE, MDFLD_GL3_G_CONTROL);
	PSB_DEBUG_GENERAL("gl3 cache soft reset with mas %x\n",
			MDFLD_GL3_SOFT_RESET_ENABLE);
	ospm_power_using_hw_end(OSPM_GRAPHICS_ISLAND);
}

#endif /* CONFIG_MDFD_GL3 */

/*****************************************************************************
 *
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
 ******************************************************************************/

#ifndef __PSB_VIDEO_BIND_H__
#define __PSB_VIDEO_BIND_H__

#include <linux/ioctl.h>

struct PSB_Video_ioctl_package {
	int ioctl_cmd;
	int device_id;
	int inputparam;
	int outputparam;
};

#define	PVR_MAX_VIEW_NUM  2
struct BC_Video_bind_input_t {
	IMG_UINT32 hTTMBuffer;
	void *pVAddr;
	IMG_UINT32 view_n;
	IMG_HANDLE view_ids[PVR_MAX_VIEW_NUM];
};


#define PSB_Video_ioctl_bind_st_gfx_buffer       0
#define PSB_Video_ioctl_unbind_st_gfx_buffer     1

#endif

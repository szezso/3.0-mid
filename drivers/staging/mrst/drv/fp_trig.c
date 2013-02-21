/*
 * Copyright (C) 2011 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/types.h>

#include "fp_trig.h"

enum {
	COS_TAB_SIZE = 128,
	LERP_ONE = 1 << 12,
};

/*
 * cos_tab was generated like this:
 *
 * for (i = 0; i < COS_TAB_SIZE; i++)
 *   cos_tab[i] = trunc(cos(M_PI * i / ((COS_TAB_SIZE - 1) * 2)) * (1 << FP_FRAC_BITS));
 *
 * The table has one extra element to avoid out of
 * bounds access when computing cosine at FP_PI/2.
 */
static const uint16_t cos_tab[COS_TAB_SIZE + 1] = {
	0x8000, 0x7ffd, 0x7ff5, 0x7fe9, 0x7fd7, 0x7fc1, 0x7fa5, 0x7f85,
	0x7f5f, 0x7f35, 0x7f05, 0x7ed1, 0x7e97, 0x7e59, 0x7e15, 0x7dcd,
	0x7d80, 0x7d2e, 0x7cd7, 0x7c7b, 0x7c1a, 0x7bb4, 0x7b4a, 0x7adb,
	0x7a66, 0x79ed, 0x7970, 0x78ed, 0x7866, 0x77da, 0x7749, 0x76b4,
	0x761a, 0x757c, 0x74d9, 0x7431, 0x7385, 0x72d4, 0x721e, 0x7165,
	0x70a6, 0x6fe4, 0x6f1d, 0x6e51, 0x6d82, 0x6cae, 0x6bd5, 0x6af9,
	0x6a18, 0x6934, 0x684b, 0x675e, 0x666d, 0x6578, 0x647e, 0x6382,
	0x6281, 0x617c, 0x6073, 0x5f67, 0x5e57, 0x5d43, 0x5c2c, 0x5b11,
	0x59f2, 0x58d0, 0x57ab, 0x5682, 0x5555, 0x5425, 0x52f2, 0x51bc,
	0x5083, 0x4f46, 0x4e06, 0x4cc3, 0x4b7e, 0x4a35, 0x48e9, 0x479b,
	0x4649, 0x44f5, 0x439e, 0x4245, 0x40e9, 0x3f8a, 0x3e29, 0x3cc6,
	0x3b60, 0x39f8, 0x388d, 0x3721, 0x35b2, 0x3441, 0x32ce, 0x3159,
	0x2fe2, 0x2e69, 0x2cef, 0x2b72, 0x29f4, 0x2874, 0x26f3, 0x2570,
	0x23ec, 0x2266, 0x20df, 0x1f57, 0x1dcd, 0x1c43, 0x1ab7, 0x192a,
	0x179c, 0x160d, 0x147e, 0x12ed, 0x115c, 0x0fca, 0x0e38, 0x0ca5,
	0x0b11, 0x097d, 0x07e9, 0x0654, 0x04bf, 0x032a, 0x0195, 0x0000,
};

static unsigned int lerp(unsigned int x1, unsigned int x2, unsigned int alpha)
{
	return ((x1 * (LERP_ONE - alpha)) + (x2 * alpha)) / LERP_ONE;
}

int fp_cos(unsigned int angle)
{
	unsigned int x, i, quadrant;
	int y;

	/* limit angle to [0:FP_PI/2] range */
	quadrant = (angle / (FP_PI / 2)) & 3;
	angle &= FP_PI / 2 - 1;
	if (quadrant & 1)
		angle = FP_PI / 2 - angle;

	/*
	 * Perform linear interpolation between the points in the table.
	 * Round x up to keep fp_cos(x) <= cos(x).
	 */
	x = DIV_ROUND_UP(angle * (COS_TAB_SIZE - 1) * LERP_ONE, FP_PI / 2);
	i = x / LERP_ONE;
	y = lerp(cos_tab[i], cos_tab[i+1], x & (LERP_ONE - 1));

	if (quadrant == 1 || quadrant == 2)
		y = -y;

	return y;
}

int fp_sin(unsigned int angle)
{
	return -fp_cos(angle + FP_PI / 2);
}

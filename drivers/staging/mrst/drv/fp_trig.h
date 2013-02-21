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

#ifndef FP_TRIG_H
#define FP_TRIG_H

enum {
	/* number of fractional bits in results */
	FP_FRAC_BITS = 15,

	/* value of PI */
	FP_PI = 1 << 14,
};

/**
 * fp_cos - Fixed point cosine
 * @angle: positive angle (period 2*FP_PI)
 *
 * RETURNS:
 * Cosine in signed 1.<FP_FRAC_BITS> fixed point format
 */
int fp_cos(unsigned int angle);

/**
 * fp_sin - Fixed point sine
 * @angle: positive angle (period 2*FP_PI)
 *
 * RETURNS:
 * Sine in signed 1.<FP_FRAC_BITS> fixed point format
 */
int fp_sin(unsigned int angle);

#endif

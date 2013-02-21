/**********************************************************************
 *
 * Copyright (c) 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 *     Intel Corporation
 *     2200 Mission College Blvd.
 *     Santa Clara, CA  97052
 *
 ******************************************************************************/
#ifdef INTEL_D3_PM

#if !defined(__GRAPHICS_PM_H__)
#define __GRAPHICS_PM_H__

#define GRAPHICS_PM_OK           0
#define GRAPHICS_PM_ERR_FAILED   1

/* Initialize power management for graphics component */
int graphics_pm_init(void );

/* De-initialize power management for graphics component */
int graphics_pm_deinit(void );

/* Set pm state to idle */
void graphics_pm_set_idle(void );

/* Set pm state to busy */
void graphics_pm_set_busy(void );

/* Returns when the pm state is not suspended */
void graphics_pm_wait_not_suspended(void );

#endif
#endif

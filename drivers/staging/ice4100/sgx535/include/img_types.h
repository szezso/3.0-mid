/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
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
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#ifndef __IMG_TYPES_H__
#define __IMG_TYPES_H__

#include <linux/types.h>
#if !defined(IMG_ADDRSPACE_CPUVADDR_BITS)
#define IMG_ADDRSPACE_CPUVADDR_BITS		32
#endif

#if !defined(IMG_ADDRSPACE_PHYSADDR_BITS)
#define IMG_ADDRSPACE_PHYSADDR_BITS		32
#endif

#if !defined(u32_MAX)
	#define u32_MAX 0xFFFFFFFFUL
#endif

typedef void *IMG_CPU_VIRTADDR;

typedef struct
{

	u32 uiAddr;
#define IMG_CAST_TO_DEVVADDR_UINT(var)		(u32)(var)

} IMG_DEV_VIRTADDR;

typedef struct _IMG_CPU_PHYADDR
{

	u32 uiAddr;
} IMG_CPU_PHYADDR;

typedef struct _IMG_DEV_PHYADDR
{
#if IMG_ADDRSPACE_PHYSADDR_BITS == 32

	u32 uiAddr;
#else
	u32 uiAddr;
	u32 uiHighAddr;
#endif
} IMG_DEV_PHYADDR;

typedef struct _IMG_SYS_PHYADDR
{

	u32 uiAddr;
} IMG_SYS_PHYADDR;



#endif

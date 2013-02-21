/**********************************************************************
 *
 * Copyright (c) 2008-2009 Intel Corporation.
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
 *     Intel Corporation
 *     2200 Mission College Blvd.
 *     Santa Clara, CA  97052
 *
 ******************************************************************************/

/* This file was based on the Imagination Technologies sample implementation. */

#if !defined(__SOCCONFIG_H__)
#define __SOCCONFIG_H__

#include "syscommon.h"

#define VS_PRODUCT_NAME	"Intel(R) GMA 500 based on PowerVR SGX 535"

#define SYS_SGX_USSE_COUNT					(2)

#define PCI_BASEREG_OFFSET_DWORDS			4

#define SYS_SGX_REG_PCI_BASENUM				1
#define SYS_SGX_REG_PCI_OFFSET		        (SYS_SGX_REG_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)

#define SYS_SGX_REG_OFFSET                  0x0
#define SYS_SGX_REG_SIZE 				    0x4000

#define SYS_SGX_SP_OFFSET				    0x4000
#define SYS_SGX_SP_SIZE					    0x4000

#define SYS_SGX_REG_REGION_SIZE				0x8000


#define SYS_SGX_MEM_PCI_BASENUM				2
#define SYS_SGX_MEM_PCI_OFFSET		        (SYS_SGX_MEM_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)

#define SYS_SGX_MEM_REGION_SIZE				0x20000000


#define SYS_SGX_DEV_VENDOR_ID  0x8086
#define SYS_SGX_DEV_DEVICE_ID  0x2E5B

#define SYS_LOCALMEM_FOR_SGX_RESERVE_SIZE   (220*1024*1024)


#define MEMTEST_MAP_SIZE					(1024*1024)


typedef struct
{
	union
	{
		u8	aui8PCISpace[256];
		u16	aui16PCISpace[128];
		u32	aui32PCISpace[64];
		struct
		{
			u16	ui16VenID;
			u16	ui16DevID;
			u16	ui16PCICmd;
			u16	ui16PCIStatus;
		}s;
	}u;

} PCICONFIG_SPACE, *PPCICONFIG_SPACE;

#endif


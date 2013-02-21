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

#if !defined(__SYSLOCAL_H__)
#define __SYSLOCAL_H__


void SysEnableInterrupts(SYS_DATA *psSysData);
void SysDisableInterrupts(SYS_DATA *psSysData);

char *SysCreateVersionString(IMG_CPU_PHYADDR sRegRegion);



#define SYS_SPECIFIC_DATA_ENABLE_IRQ		0x00000001UL
#define SYS_SPECIFIC_DATA_ENABLE_LISR		0x00000002UL
#define SYS_SPECIFIC_DATA_ENABLE_MISR		0x00000004UL
#define SYS_SPECIFIC_DATA_ENABLE_ENVDATA	0x00000008UL
#define SYS_SPECIFIC_DATA_ENABLE_PCINIT		0x00000010UL
#define SYS_SPECIFIC_DATA_ENABLE_LOCATEDEV	0x00000020UL
#define SYS_SPECIFIC_DATA_ENABLE_RA_ARENA	0x00000040UL
#define SYS_SPECIFIC_DATA_ENABLE_INITREG	0x00000080UL
#define SYS_SPECIFIC_DATA_ENABLE_REGDEV		0x00000100UL
#define SYS_SPECIFIC_DATA_ENABLE_PDUMPINIT	0x00000200UL
#define SYS_SPECIFIC_DATA_ENABLE_INITDEV	0x00000400UL
#define SYS_SPECIFIC_DATA_ENABLE_PCI_MEM	0x00000800UL
#define SYS_SPECIFIC_DATA_ENABLE_PCI_REG	0x00001000UL
#define SYS_SPECIFIC_DATA_ENABLE_PCI_ATL	0x00002000UL
#define SYS_SPECIFIC_DATA_ENABLE_PCI_DEV	0x00004000UL
#ifdef INTEL_D3_PM
#define SYS_SPECIFIC_DATA_ENABLE_GRAPHICS_PM 0x00008000UL
#endif

#define	SYS_SPECIFIC_DATA_PM_UNMAP_SGX_REGS	0x00020000UL
#define	SYS_SPECIFIC_DATA_PM_UNMAP_SGX_HP	0x00080000UL
#define	SYS_SPECIFIC_DATA_PM_IRQ_DISABLE	0x00100000UL
#define	SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR	0x00200000UL

#define	SYS_SPECIFIC_DATA_SET(psSysSpecData, flag) \
            ((void)((psSysSpecData)->ui32SysSpecificData |= (flag)))

#define	SYS_SPECIFIC_DATA_CLEAR(psSysSpecData, flag) \
            ((void)((psSysSpecData)->ui32SysSpecificData &= ~(flag)))

#define	SYS_SPECIFIC_DATA_TEST(psSysSpecData, flag) \
            (((psSysSpecData)->ui32SysSpecificData & (flag)) != 0)

typedef struct _SYS_SPECIFIC_DATA_TAG_
{
	u32 ui32SysSpecificData;

	struct pci_dev *psSGXPCIDev;

#ifdef	LDM_PCI
	struct pci_dev *psPCIDev;
#endif
} SYS_SPECIFIC_DATA;

#endif



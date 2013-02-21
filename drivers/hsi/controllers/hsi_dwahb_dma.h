/*
 * ssi_dwahb_dma.h
 *
 * Implements interface for DW ahb DMA controller.
 *
 * Copyright (C) 2009 Intel Corporation. All rights reserved.
 *
 * Contact: Jim Stanley <jim.stanley@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _DWAHB_DMA_H_
#define _DWAHB_DMA_H_

#define DWAHB_OFFSET					0x58

/* channel registers */
#define HSI_DWAHB_SAR(dma_base, channel)		(dma_base +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_DAR(dma_base, channel)		(dma_base + 0x008 +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_LLP(dma_base, channel)		(dma_base + 0x010 +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_CTL_LO(dma_base, channel)		(dma_base + 0x018 +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_CTL_HI(dma_base, channel)		(dma_base + 0x01C +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_CFG_LO(dma_base, channel)		(dma_base + 0x040 +\
						((channel)*DWAHB_OFFSET))
#define HSI_DWAHB_CFG_HI(dma_base, channel)		(dma_base + 0x044 +\
						((channel)*DWAHB_OFFSET))

/* dma configuration and channel enable registers */
#define HSI_DWAHB_DMACFG_DISABLED			0
#define HSI_DWAHB_DMACFG_ENABLED			1

#define HSI_DWAHB_DMACFG(dma_base)			(dma_base+0x398)
/* bits 0-7:ch enable, 8-15:write enable 
 * cleared when global enable is cleared
 */
/* use write enable bits to affect particular channel enable bits;
 * read-modify-write not required
 */
#define HSI_DWAHB_CHEN_DISABLED				0
#define HSI_DWAHB_CHEN_ENABLED				1

#define HSI_DWAHB_CHEN(dma_base)			(dma_base+0x3A0)

/* interrupt registers */
#define HSI_DWAHB_STATUSINT(dma_base)			(dma_base+0x360)

#define HSI_DWAHB_STATUSTFR(dma_base)			(dma_base+0x2E8)
#define HSI_DWAHB_STATUSBLOCK(dma_base)			(dma_base+0x2F0)
#define HSI_DWAHB_STATUSSRCTRAN(dma_base)		(dma_base+0x2F8)
#define HSI_DWAHB_STATUSDSTTRAN(dma_base)		(dma_base+0x300)
#define HSI_DWAHB_STATUSERR(dma_base)			(dma_base+0x308)

#define HSI_DWAHB_MASKTFR(dma_base)			(dma_base+0x310)
#define HSI_DWAHB_MASKBLOCK(dma_base)			(dma_base+0x318)
#define HSI_DWAHB_MASKSRCTRAN(dma_base)			(dma_base+0x320)
#define HSI_DWAHB_MASKDSTTRAN(dma_base)			(dma_base+0x328)
#define HSI_DWAHB_MASKERR(dma_base)			(dma_base+0x330)

#define HSI_DWAHB_CLEARTFR(dma_base)			(dma_base+0x338)
#define HSI_DWAHB_CLEARBLOCK(dma_base)			(dma_base+0x340)
#define HSI_DWAHB_CLEARSRCTRAN(dma_base)		(dma_base+0x348)
#define HSI_DWAHB_CLEARDSTTRAN(dma_base)		(dma_base+0x350)
#define HSI_DWAHB_CLEARERR(dma_base)			(dma_base+0x358)

/* Channel CTL register  parameter definitions */
#define HSI_DWAHB_CTL_MSIZE_1				0
#define HSI_DWAHB_CTL_MSIZE_4				1
#define HSI_DWAHB_CTL_MSIZE_64				5
#define HSI_DWAHB_CTL_MSIZE_128				6
#define HSI_DWAHB_CTL_MSIZE_256				7

#define HSI_DWAHB_CTL_TR_WIDTH_32			2

/* transfer type/flow control */
#define HSI_DWAHB_CTL_TT_FC_M2P_DW			1
#define HSI_DWAHB_CTL_TT_FC_M2P_P			6
#define HSI_DWAHB_CTL_TT_FC_P2M_DW			2
#define HSI_DWAHB_CTL_TT_FC_P2M_P			4

#define HSI_DWAHB_DISABLE_DMA				0
#define HSI_DWAHB_ENABLE_DMA				1

#define HSI_DWAHB_CTL_ADR_INC				0
#define HSI_DWAHB_CTL_ADR_DEC				1
#define HSI_DWAHB_CTL_ADR_NOC				2

/* Channel CTL field masks */
#define HSI_DWAHB_CTL_INT_EN				0x1
#define HSI_DWAHB_CTL_DST_TR_WIDTH			0xE
#define HSI_DWAHB_CTL_SRC_TR_WIDTH			0x70
#define HSI_DWAHB_CTL_DINC				0x180
#define HSI_DWAHB_CTL_SINC				0x600
#define HSI_DWAHB_CTL_DST_MSIZE				0x3800
#define HSI_DWAHB_CTL_SRC_MSIZE				0x1C000
#define HSI_DWAHB_CTL_SRC_GATHER			0x20000
#define HSI_DWAHB_CTL_DST_SCATTER			0x40000
#define HSI_DWAHB_CTL_TT_FC				0x700000
#define HSI_DWAHB_CTL_DMS				0x1800000
#define HSI_DWAHB_CTL_SMS				0x6000000
#define HSI_DWAHB_CTL_LLP_DST				0x8000000
#define HSI_DWAHB_CTL_LLP_SRC				0x10000000
#define HSI_DWAHB_CTL_BLOCK_TS				0xFFF
#define HSI_DWAHB_CTL_DONE				0x1000

#define HSI_DWAHB_CTL_MAX_TS				4095

/* Channel CFG register parameter definitions */
#define HSI_DWAHB_CFG_CH_NOT_SUSPENDED			0
#define HSI_DWAHB_CFG_CH_SUSPEND			1

#define HSI_DWAHB_CFG_FIFO_NOT_EMPTY			0
#define HSI_DWAHB_CFG_FIFO_EMPTY			1

#define HSI_DWAHB_CFG_HW_HSHAKE				0
#define HSI_DWAHB_CFG_SW_HSHAKE				1

#define HSI_DWAHB_CFG_CH_LOCK_ENTIRE			0
#define HSI_DWAHB_CFG_CH_LOCK_BLOCK			1
#define HSI_DWAHB_CFG_CH_LOCK_TRANS			2

#define HSI_DWAHB_CFG_CH_LOCK_DISABLE			0
#define HSI_DWAHB_CFG_CH_LOCK_ENABLE			1

#define HSI_DWAHB_CFG_HS_ACTIVE_LOW			0
#define HSI_DWAHB_CFG_HS_ACTIVE_HIGH			1

#define HSI_DWAHB_CFG_FCMODE_NO_PREFETCH		1

#define HSI_DWAHB_CFG_DSINC_INC				0
#define HSI_DWAHB_CFG_DSINC_DEC				1
#define HSI_DWAHB_CFG_DSINC_NC				2

#define HSI_DWAHB_CFG_FIFO_SINGLE_XFER			0
#define HSI_DWAHB_CFG_FIFO_HALF_FIFO			1

/* Channel CFG field masks LOW DWORD */
#define HSI_DWAHB_CFG_CH_PRIO				0xE0
#define HSI_DWAHB_CFG_CH_SUSP				0x100
#define HSI_DWAHB_CFG_FIFO_EMPTY_CHECK			0x200
#define HSI_DWAHB_CFG_HS_DST				0x400
#define HSI_DWAHB_CFG_HS_SRC				0x800
#define HSI_DWAHB_CFG_CH_LOCK				0x3000
#define HSI_DWAHB_CFG_B_LOCK				0xC000
#define HSI_DWAHB_CFG_LOCK_CH				0x10000
#define HSI_DWAHB_CFG_LOCK_B				0x20000
#define HSI_DWAHB_CFG_DST_HS				0x40000
#define HSI_DWAHB_CFG_SRC_HS				0x80000
#define HSI_DWAHB_CFG_MAX_ABRST				0x3FF00000

/* Channel CFG field masks HIGH DWORD */
#define HSI_DWAHB_CFG_FCMODE				0x1
#define HSI_DWAHB_CFG_FIFO_MODE				0x2
#define HSI_DWAHB_CFG_PROT_CTL				0x1C
#define HSI_DWAHB_CFG_SRC_PER				0x780
#define HSI_DWAHB_CFG_DST_PER				0x7800

/* used by DMA logical channel mapping */
#define HSI_LOGICAL_READ	0
#define HSI_LOGICAL_WRITE	1

#define HSI_MAX_LOGICAL_CHANNEL	7

/* map address of HSI controller port rx/tx register */
#define HSI_DWAHB_TX_BASE	0xFF600000
#define HSI_DWAHB_RX_BASE	0xFF400000

#define HSI_DWAHB_TX_ADDRESS(channel) \
				(HSI_DWAHB_TX_BASE + 0x40000*(channel))
#define HSI_DWAHB_RX_ADDRESS(channel) \
				(HSI_DWAHB_RX_BASE + 0x40000*(channel))

static inline void set_hsi_dwahb_dma_reg(void __iomem *reg, u32 data,
	u8 bit_position, u32 field_mask)
{
	u32 temp = ioread32(reg);

	temp &= ~field_mask;
	temp |= data << bit_position;
	iowrite32(temp, reg);
}

static inline u32 get_hsi_dwahb_dma_reg(void __iomem *reg, u8 bit_position,
						u32 field_mask)
{
	u32 temp = ioread32(reg);
	return (temp & field_mask) >> bit_position;
}

static inline void hsi_dwahb_masked_write(void __iomem *reg,
						u32 mask, u32 data)
{
	u32 temp = (mask << 8) | data;
	iowrite32(temp, reg);
}

static inline void hsi_dwahb_single_masked_clear(void __iomem *reg, u32 data)
{
	u32 temp = (data << 8);
	iowrite32(temp, reg);
}

static inline void hsi_dwahb_single_masked_write(void __iomem *reg, u32 data)
{
	u32 temp = (data << 8) | data;
	iowrite32(temp, reg);
}

#endif /* _DWAHB_DMA_H_ */

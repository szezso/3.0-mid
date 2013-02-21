/*
 * hsi_arasan.h
 *
 * Implements HSI interface for Arasan controller.
 *
 * Copyright (C) 2010 Intel Corporation. All rights reserved.
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

#ifndef _HSI_ARASAN_H_
#define _HSI_ARASAN_H_

/* platform device parameters */
#define HSI_IOMEM_NAME			"HSI_HSI_BASE"
#define HSI_DMA_NAME			"HSI_DMA_BASE"

#define ARASAN_HSI_DMA_CONFIG(base, channel)		(base+((channel)*4))
#define ARASAN_HSI_DMA_TX_FIFO_SIZE(base)		(base+0x40)
#define ARASAN_HSI_DMA_TX_FIFO_THRESHOLD(base)		(base+0x44)
#define ARASAN_HSI_DMA_RX_FIFO_SIZE(base)		(base+0x48)
#define ARASAN_HSI_DMA_RX_FIFO_THRESHOLD(base)		(base+0x4C)

#define ARASAN_HSI_CLOCK_CONTROL(base)			(base+0x50)

#define ARASAN_HSI_HSI_STATUS(base)			(base+0x54)
#define ARASAN_HSI_HSI_STATUS1(base)			(base+0xC4)
#define ARASAN_HSI_INTERRUPT_STATUS(base)		(base+0x58)
#define ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base)	(base+0x5C)
#define ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base)	(base+0x60)

#define ARASAN_HSI_PROGRAM(base)			(base+0x64)
#define ARASAN_HSI_PROGRAM1(base)			(base+0xC8)

#define ARASAN_HSI_ARBITER_PRIORITY(base)		(base+0x68)
#define ARASAN_HSI_ARBITER_BANDWIDTH1(base)		(base+0x6C)
#define ARASAN_HSI_ARBITER_BANDWIDTH2(base)		(base+0x70)

#define ARASAN_HSI_CAPABILITY(base)			(base+0x74)

#define ARASAN_HSI_TX_DATA(base, channel)	(base+((channel)*4)+0x78)
#define ARASAN_HSI_RX_DATA(base, channel)	(base+((channel)*4)+0x98)

#define ARASAN_HSI_ERROR_INTERRUPT_STATUS(base)		(base+0xB8)
#define ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base)	(base+0xBC)
#define ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base)	(base+0xC0)

#define ARASAN_HSI_VERSION(base)			(base+0xFC)
#define ARASAN_HSI_RTL_VERSION_RTL			0xff
#define ARASAN_HSI_RTL_VERSION_HSI			0xff00

/* Clock control */
#define HSI_TX_INTERNAL_CLOCK_ENABLE_OSCILLATE	1
#define HSI_TX_INTERNAL_CLOCK_ENABLE_STOP	0
#define HSI_TX_CLOCK_STABLE			1
#define HSI_TX_CLOCK_ENABLE			1
#define HSI_TX_CLOCK_DISABLE			0

/* tx clock can take 150us to become stable */
#define HSI_MAX_TX_CLOCK_WAIT			200

/* dma config parameters */
#define HSI_DMA_TXRX_TRANSMIT			0
#define HSI_DMA_TXRX_RECEIVE			1

#define HSI_DMA_CFG_ENABLE			1
#define HSI_DMA_CFG_DISABLE			0

#define HSI_DMA_CFG_BURST_4			0
#define HSI_DMA_CFG_BURST_8			1
#define HSI_DMA_CFG_BURST_16			2
#define HSI_DMA_CFG_BURST_32			3

/* tx fifo control */
#define HSI_TX_FIFO_THRESHOLD_HALF_EMPTY	0
#define HSI_TX_FIFO_THRESHOLD_ALMOST_EMPTY	1

/* rx fifo control */
#define HSI_RX_FIFO_THRESHOLD_HALF_FULL		0
#define HSI_RX_FIFO_THRESHOLD_ALMOST_FULL	1

#define HSI_TX_FIFO_SIZE_1			0
#define HSI_TX_FIFO_SIZE_2			1
#define HSI_TX_FIFO_SIZE_4			2
#define HSI_TX_FIFO_SIZE_8			3
#define HSI_TX_FIFO_SIZE_16			4
#define HSI_TX_FIFO_SIZE_32			5
#define HSI_TX_FIFO_SIZE_64			6
#define HSI_TX_FIFO_SIZE_128			7
#define HSI_TX_FIFO_SIZE_256			8
#define HSI_TX_FIFO_SIZE_512			9

#define HSI_RX_FIFO_SIZE_1			0
#define HSI_RX_FIFO_SIZE_2			1
#define HSI_RX_FIFO_SIZE_4			2
#define HSI_RX_FIFO_SIZE_8			3
#define HSI_RX_FIFO_SIZE_16			4
#define HSI_RX_FIFO_SIZE_32			5
#define HSI_RX_FIFO_SIZE_64			6
#define HSI_RX_FIFO_SIZE_128			7
#define HSI_RX_FIFO_SIZE_256			8
#define HSI_RX_FIFO_SIZE_512			9

/* channel offsets in HSI status register -- read only */
#define HSI_STATUS_RX_FIFO_NOT_EMPTY		8
#define HSI_STATUS_TX_FIFO_FULL			16
#define HSI_STATUS_TX_FIFO_EMPTY		24

/* channel offsets in HSI interrupt status register -- write 1 to clear */
#define HSI_INT_STATUS_TX_THR_REACHED		0
#define HSI_INT_STATUS_RX_THR_REACHED		8
#define HSI_INT_STATUS_WKUP_INT			16
#define HSI_INT_STATUS_DMA_COMPLETE		17
#define HSI_INT_STATUS_ERROR			31

/* Interrupt status register -- write 1 to clear a bit */
#define HSI_INTERRUPT_STATUS_ERROR_MASK		(1 << HSI_INT_STATUS_ERROR)
#define HSI_INTERRUPT_STATUS_WKUP_MASK		(1 << HSI_INT_STATUS_WKUP_INT)

/* Interrupt status enable register */
#define HSI_STATUS_ENABLE			1
#define HSI_STATUS_DISABLE			0
#define HSI_STATUS_CLEAR			1

/* Program register channel enable/disable */
#define HSI_PROGRAM_CHANNEL_ENABLED		1
#define HSI_PROGRAM_CHANNEL_DISABLED		0

/* Program register soft reset */
#define HSI_PROGRAM_SOFTRESET_RESET		1
#define HSI_PROGRAM_SOFTRESET_DONE		0

#define HSI_PROGRAM_TX_MODE_STREAM		0
#define HSI_PROGRAM_TX_MODE_FRAME		1

#define HSI_PROGRAM_RX_MODE_STREAM		0
#define HSI_PROGRAM_RX_MODE_FRAME		1

#define HSI_PROGRAM_RX_MODE_SYNCH		0
#define HSI_PROGRAM_RX_MODE_PIPELINED	1
#define HSI_PROGRAM_RX_MODE_RT			2

#define HSI_PROGRAM_RX_SLEEP			0
#define HSI_PROGRAM_RX_WAKE			1

#define HSI_PROGRAM_TX_SLEEP			0
#define HSI_PROGRAM_TX_WAKE			1

#define HSI_ARBITER_ROUNDROBIN			0
#define HSI_ARBITER_FIXED			1

#define HSI_ARBITER_TX_BW_256			0
#define HSI_ARBITER_TX_BW_1			1
#define HSI_ARBITER_TX_BW_2			2

#define HSI_CLOCK_DIVISOR_BASE			0

#define HSI_CLOCK_DATA_TIMEOUT_2_13		0
#define HSI_CLOCK_DATA_TIMEOUT_2_14		1
#define HSI_CLOCK_DATA_TIMEOUT_2_15		2
#define HSI_CLOCK_DATA_TIMEOUT_2_16		3
#define HSI_CLOCK_DATA_TIMEOUT_2_17		4
#define HSI_CLOCK_DATA_TIMEOUT_2_27		0x0E

#define HSI_RCV_TIMEOUT_14400			0
#define HSI_RCV_TIMEOUT_7200			1
#define HSI_RCV_TIMEOUT_112			0x40

#define HSI_CLOCK_BREAK_SET			1
#define HSI_CLOCK_BREAK_CLEAR			0

#define HSI_CLOCK_TX_FRAME_256			0
#define HSI_CLOCK_TX_FRAME_1			1
#define HSI_CLOCK_TX_FRAME_2			2

#define HSI_CLOCK_TAIL_400			0
#define HSI_CLOCK_TAIL_200			1
#define HSI_CLOCK_TAIL_100			2
#define HSI_CLOCK_TAIL_50			4

/* Error interrupt signal enable register */
#define HSI_ERROR_BREAK_DETECTED		1
#define HSI_ERROR_RX_ERROR			2
#define HSI_ERROR_ERROR_MASK			2
#define HSI_ERROR_TIMEOUT_MASK			4
#define HSI_ERROR_ENABLED			1
#define HSI_ERROR_DISABLED			0
#define HSI_ERROR_CLEAR				1

/* dma config register parameters */
#define HSI_DMA_CFG_TX_RX_WRITE			0
#define HSI_DMA_CFG_TX_RX_READ			1

#define HSI_DMA_CFG_BURST_4			0
#define HSI_DMA_CFG_BURST_8			1
#define HSI_DMA_CFG_BURST_16			2
#define HSI_DMA_CFG_BURST_32			3

#define HSI_DMA_CFG_ENABLE			1
#define HSI_DMA_CFG_DISABLE			0

#define HSI_SIGNAL_ASSERTED		1
#define HSI_SIGNAL_DEASSERTED	0

static inline void set_hsi_reg(void __iomem *reg, u32 data, u8 bit_position,
	u32 field_mask)
{
	u32 temp = ioread32(reg);
	u32 mask = field_mask; /* actually (2^field_width)-1 */

	mask = mask << bit_position;
	temp &= ~mask;
	temp |= data << bit_position;
	iowrite32(temp, reg);
}

static inline u32 get_hsi_reg(void __iomem *reg, u8 bit_position,
	u32 field_mask)
{
	u32 temp = ioread32(reg);
	return (temp >> bit_position) & field_mask;
}


/* Slave DMA config register */
static inline void set_hsi_sdma_tx_rx(void __iomem *base, u8 channel,
	u32 tx_rx)
{
	set_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), tx_rx, 0, 1);
}

static inline u32 get_hsi_sdma_tx_rx(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), 0, 1);
}

static inline void set_hsi_sdma_logical_channel(void __iomem *base,
	u8 channel, u32 log_chan)
{
	set_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), log_chan, 1, 0x7);
}

static inline u32 get_hsi_sdma_logical_channel(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), 1, 0x7);
}

static inline void set_hsi_sdma_transfer_count(void __iomem *base, u8 channel,
							u32 count)
{
	set_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), count, 4, 0xfffff);
}

static inline u32 get_hsi_sdma_transfer_count(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), 4, 0xfffff);
}

static inline void set_hsi_sdma_burst_size(void __iomem *base, u8 channel,
						u32 burst)
{
	set_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), burst, 24, 0x7);
}

static inline u32 get_hsi_sdma_burst_size(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), 24, 0x7);
}

static inline void set_hsi_sdma_enable(void __iomem *base, u8 channel,
						u32 enable)
{
	set_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), enable, 31, 0x1);
}

static inline u32 get_hsi_sdma_enable(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_CONFIG(base, channel), 31, 0x1);
}

/* FIFO Registers */
static inline void set_hsi_tx_fifo_size(void __iomem *base, u8 channel,
						u32 size_index)
{
	set_hsi_reg(ARASAN_HSI_DMA_TX_FIFO_SIZE(base), size_index,
		channel*4, 0xf);
}

static inline u32 get_hsi_tx_fifo_size(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_TX_FIFO_SIZE(base), channel*4, 0xf);
}

static inline void set_hsi_tx_fifo_threshold(void __iomem *base, u8 channel,
						u32 threshold)
{
	set_hsi_reg(ARASAN_HSI_DMA_TX_FIFO_THRESHOLD(base), threshold, channel,
		0x1);
}

static inline u32 get_hsi_tx_fifo_threshold(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_TX_FIFO_THRESHOLD(base),
				channel, 0x1);
}

static inline void set_hsi_rx_fifo_size(void __iomem *base, u8 channel,
						u32 size_index)
{
	set_hsi_reg(ARASAN_HSI_DMA_RX_FIFO_SIZE(base), size_index,
		channel*4, 0xf);
}

static inline u32 get_hsi_rx_fifo_size(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_RX_FIFO_SIZE(base), channel*4, 0xf);
}

static inline void set_hsi_rx_fifo_threshold(void __iomem *base, u8 channel,
						u32 threshold)
{
	set_hsi_reg(ARASAN_HSI_DMA_RX_FIFO_THRESHOLD(base), threshold,
			channel, 0x1);
}

static inline u32 get_hsi_rx_fifo_threshold(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_DMA_RX_FIFO_THRESHOLD(base),
			channel, 0x1);
}

static inline void set_hsi_clk_tx_enable(void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), enable, 0, 1);
}

static inline u32 get_hsi_clk_tx_enable(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 0, 1);
}

static inline u32 get_hsi_clk_stable(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 1, 1);
}

static inline void set_hsi_clk_internal_enable(void __iomem *base,
	u32 enable)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), enable, 2, 1);
}

static inline u32 get_hsi_clk_internal_enable(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 2, 1);
}

static inline void set_hsi_clk_clock_divisor(void __iomem *base, u32 index)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), index, 3, 0xff);
}

static inline u32 get_hsi_clk_clock_divisor(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 3, 0xff);
}

static inline void set_hsi_clk_data_timeout(void __iomem *base, u32 count)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), count, 11, 0xf);
}

static inline u32 get_hsi_clk_data_timeout(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 11, 0xf);
}

static inline void set_hsi_clk_tx_break(void __iomem *base, u32 txbreak)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), txbreak, 15, 0x1);
}

static inline u32 get_hsi_clk_tx_break(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 15, 0x1);
}

static inline void set_hsi_clk_rx_frame_burst(void __iomem *base, u32 count)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), count, 16, 0xff);
}

static inline u32 get_hsi_clk_rx_frame_burst(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 16, 0xff);
}

static inline void set_hsi_clk_rx_tailing_bit_count(void __iomem *base,
	u32 count)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), count, 24, 0x7);
}

static inline u32 get_hsi_clk_tx_tailing_bit_count(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), 24, 0x7);
}

static inline void set_hsi_clk_tap_delay(void __iomem *base,
	u32 value)
{
	set_hsi_reg(ARASAN_HSI_CLOCK_CONTROL(base), value, 27, 0x7);
}

/* Status register */
static inline u32 get_hsi_status_rx_wake(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 4, 0x1);
}

static inline u32 get_hsi_status_rx_fifo_not_empty(void __iomem *base,
						u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 8 + channel, 0x1);
}

static inline u32 get_hsi_status_rx_fifos_empty(void __iomem *base)
{
	return (~get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 8, 0xff)) & 0xff;
}

static inline u32 get_hsi_status_tx_fifo_full(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 16 + channel, 0x1);
}

static inline u32 get_hsi_status_tx_fifo_empty(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 24 + channel, 0x1);
}

static inline u32 get_hsi_status_tx_fifos_empty(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS(base), 24, 0xff);
}

/* Status1 register */
static inline u32 get_hsi_status1_tx_idle(void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS1(base), channel, 0x1);
}

static inline u32 get_hsi_status1_rx_idle(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_HSI_STATUS1(base), 8, 0x1);
}

static inline u32 get_hsi_interrupt_status(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), 0, 0xffffffff);
}

static inline u32 get_hsi_interrupt_status_tx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), channel, 0x1);
}

static inline void clear_hsi_interrupt_status_tx_threshold_reached(
	void __iomem *base, u8 channel)
{
	iowrite32(1 << (channel), ARASAN_HSI_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_interrupt_status_rx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), channel+8, 0x1);
}

static inline void clear_hsi_interrupt_status_rx_threshold_reached(
	void __iomem *base, u8 channel)
{
	iowrite32(1 << (channel+8), ARASAN_HSI_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_interrupt_status_wkup_interrupt(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), 16, 0x1);
}

static inline void clear_hsi_interrupt_status_wkup_interrupt(
	void __iomem *base)
{
	iowrite32(1 << 16, ARASAN_HSI_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_interrupt_dma_transfer_complete(void __iomem *base,
	u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), channel+17, 0x1);
}

static inline void clear_hsi_interrupt_dma_transfer_complete(
	void __iomem *base, u8 channel)
{
	iowrite32(1 << (channel+17), ARASAN_HSI_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_interrupt_error_interrupt(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS(base), 31, 0x1);
}

static inline void clear_hsi_interrupt_error_interrupt(void __iomem *base)
{
	iowrite32(1 << 31, ARASAN_HSI_INTERRUPT_STATUS(base));
}

/* Interrupt status enable register */
static inline u32 get_hsi_interrupt_status_enable_tx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base),
		channel, 0x1);
}

static inline void set_hsi_interrupt_status_enable_tx_threshold_reached(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base), enable,
		channel, 0x1);
}

static inline u32 get_hsi_interrupt_status_enable_rx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base),
		channel+8, 0x1);
}

static inline void set_hsi_interrupt_status_enable_rx_threshold_reached(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base), enable,
		channel+8, 0x1);
}

static inline u32 get_hsi_interrupt_status_enable_WKUP_interrupt(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base), 16, 0x1);
}

static inline void set_hsi_interrupt_status_enable_WKUP_interrupt(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base), enable, 16, 0x1);
}

static inline u32 get_hsi_interrupt_status_enable_transfer_complete(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base),
		channel+17, 0x1);
}

static inline void set_hsi_interrupt_status_enable_transfer_complete(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base), enable,
		channel+17, 0x1);
}

/* Interrupt signal enable register */
static inline u32 get_hsi_int_stat_sig_enable_tx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), channel,
		0x1);
}

static inline void set_hsi_int_stat_sig_enable_tx_threshold_reached(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), enable, channel,
		0x1);
}

static inline u32 get_hsi_int_stat_sig_enable_rx_threshold_reached(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base),
		channel+8, 0x1);
}

static inline void set_hsi_int_stat_sig_enable_rx_threshold_reached(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), enable,
		channel+8, 0x1);
}

static inline u32 get_hsi_int_stat_sig_enable_WKUP_interrupt(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), 16, 0x1);
}

static inline void set_hsi_int_stat_sig_enable_WKUP_interrupt(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), enable, 16, 0x1);
}

static inline u32 get_hsi_int_stat_sig_enable_transfer_complete(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base),
		channel+17, 0x1);
}

static inline void set_hsi_int_stat_sig_enable_transfer_complete(
		void __iomem *base,
	u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base), enable,
		channel+17, 0x1);
}

/* Program register */
static inline u32 get_hsi_program_software_reset(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 0, 1);
}

static inline void set_hsi_program_software_reset(void __iomem *base)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), 1, 0, 0x1);
}

static inline u32 get_hsi_program_receive_timeout(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 1, 0x7f);
}

static inline void set_hsi_program_receive_timeout(void __iomem *base,
	u32 timeout)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), timeout, 1, 0x7f);
}

static inline u32 get_hsi_program_tx_transmit_mode(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 8, 0x1);
}

static inline void set_hsi_program_tx_transmit_mode(void __iomem *base,
	u32 mode)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), mode, 8, 0x1);
}

static inline u32 get_hsi_program_rx_data_flow(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 9, 0x3);
}

static inline void set_hsi_program_rx_data_flow(void __iomem *base,
	u32 data_flow)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), data_flow, 9, 0x3);
}

static inline u32 get_hsi_program_rx_wake(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 11, 0x1);
}

static inline void set_hsi_program_rx_wake(void __iomem *base, u32 wake)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), wake, 11, 0x1);
}

static inline u32 get_hsi_program_tx_channel_enable(void __iomem *base,
	u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), channel+12, 0x1);
}

static inline void set_hsi_program_tx_channel_enable(void __iomem *base,
	u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), enable, channel+12, 0x1);
}

static inline u32 get_hsi_program_rx_channel_enable(void __iomem *base,
	u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), channel+20, 0x1);
}

static inline void set_hsi_program_rx_channel_enable(void __iomem *base,
	u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), enable, channel+20, 0x1);
}

static inline u32 get_hsi_program_num_tx_id_bits(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM1(base), 0, 0x3);
}

static inline void set_hsi_program_num_tx_id_bits(void __iomem *base,
	u32 num3or4)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM1(base), num3or4, 0, 0x3);
}

static inline u32 get_hsi_program_num_rx_id_bits(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM1(base), 2, 0x3);
}

static inline void set_hsi_program_num_rx_id_bits(void __iomem *base,
	u32 num3or4)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM1(base), num3or4, 2, 0x3);
}

static inline u32 get_hsi_program_rx_receive_mode(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 30, 0x1);
}

static inline void set_hsi_program_rx_receive_mode(void __iomem *base,
	u32 streamorframe)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), streamorframe, 30, 0x1);
}

static inline u32 get_hsi_program_tx_wakeup(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_PROGRAM(base), 31, 0x1);
}

static inline void set_hsi_program_tx_wakeup(void __iomem *base, u32 wake)
{
	set_hsi_reg(ARASAN_HSI_PROGRAM(base), wake, 31, 0x1);
}

/* Arbiter priority */
static inline u32 get_hsi_arbiter_priority_policy(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ARBITER_PRIORITY(base), 0, 0x1);
}

static inline void set_hsi_arbiter_priority_policy(
	void __iomem *base, u32 roundrobinorfixed)
{
	set_hsi_reg(ARASAN_HSI_ARBITER_PRIORITY(base), roundrobinorfixed, 0,
		 0x1);
}

static inline u32 get_hsi_arbiter_priority_tx_channel_priority(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ARBITER_PRIORITY(base), 1+channel*3,
		0x7);
}

static inline void set_hsi_arbiter_priority_tx_channel_priority(
	void __iomem *base, u8 channel, u32 priority)
{
	set_hsi_reg(ARASAN_HSI_ARBITER_PRIORITY(base), priority, 1+channel*3,
		 0x7);
}

/* Arbiter bandwidth1 -- channels 0 - 3 */
static inline u32 get_hsi_arbiter_bandwidth1_tx_channel_bandwidth(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ARBITER_BANDWIDTH1(base),
					channel*8, 0xff);
}

static inline void set_hsi_arbiter_bandwidth1_tx_channel_bandwidth(
	void __iomem *base, u8 channel, u32 bandwidth_index)
{
	set_hsi_reg(ARASAN_HSI_ARBITER_BANDWIDTH1(base), bandwidth_index,
		channel*8, 0xff);
}

/* Arbiter bandwidth2 -- channels 4 - 7 */
/* note the channel arguments to these macros are 4-7, not 0-3 */
static inline u32 get_hsi_arbiter_bandwidth2_tx_channel_bandwidth(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ARBITER_BANDWIDTH2(base), (channel-4)*8,
		0xff);
}

static inline void set_hsi_arbiter_bandwidth2_tx_channel_bandwidth(
	void __iomem *base, u8 channel, u32 bandwidth_index)
{
	set_hsi_reg(ARASAN_HSI_ARBITER_BANDWIDTH2(base), bandwidth_index,
		(channel-4)*8, 0xff);
}

/* Error interrupt status register -- write 1 to clear a bit */
static inline u32 get_hsi_error_interrupt_status_rx_break(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS(base), 0, 0x1);
}

static inline void clear_hsi_error_interrupt_status_rx_break(
	void __iomem *base)
{
	iowrite32(1, ARASAN_HSI_ERROR_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_error_interrupt_status_rx_error(void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS(base), 1, 0x1);
}

static inline void clear_hsi_error_interrupt_status_rx_error(
	void __iomem *base)
{
	iowrite32(2, ARASAN_HSI_ERROR_INTERRUPT_STATUS(base));
}

static inline u32 get_hsi_error_interrupt_status_data_timeout(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS(base),
		channel+2, 0x1);
}

static inline void clear_hsi_error_interrupt_status_data_timeout(
	void __iomem *base, u8 channel)
{
	iowrite32(1 << (channel+2),
				ARASAN_HSI_ERROR_INTERRUPT_STATUS(base));
}

/* Error interrupt status enable register */
static inline u32 get_hsi_error_interrupt_status_enable_rx_break(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base),
							0, 0x1);
}

static inline void set_hsi_error_interrupt_status_enable_rx_break(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base), enable, 0,
		 0x1);
}

static inline u32 get_hsi_error_interrupt_status_enable_rx_error(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base), 1,
		 0x1);
}

static inline void set_hsi_error_interrupt_status_enable_rx_error(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base), enable, 1,
		 0x1);
}

static inline u32 get_hsi_error_interrupt_status_enable_data_timeout(
	void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base),
		 channel+2, 0x1);
}

static inline void set_hsi_error_interrupt_status_enable_data_timeout(
	void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base), enable,
		 channel+2, 0x1);
}

static inline u32 get_hsi_error_interrupt_signal_enable_rx_break(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base),
		0, 0x1);
}

static inline void set_hsi_error_interrupt_signal_enable_rx_break(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base), enable,
		0, 0x1);
}

static inline u32 get_hsi_error_interrupt_signal_enable_rx_error(
	void __iomem *base)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base),
		1, 0x1);
}

static inline void set_hsi_error_interrupt_signal_enable_rx_error(
	void __iomem *base, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base), enable,
		1, 0x1);
}

static inline u32 get_hsi_error_interrupt_signal_enable_data_timeout(
		void __iomem *base, u8 channel)
{
	return get_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base),
	 channel+2, 0x1);
}

static inline void set_hsi_error_interrupt_signal_enable_data_timeout(
		void __iomem *base, u8 channel, u32 enable)
{
	set_hsi_reg(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base), enable,
	 channel+2, 0x1);
}

#endif /* _ARASAN_H */

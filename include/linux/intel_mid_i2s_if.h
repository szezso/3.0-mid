/*
  * <Driver for I2S protocol on SSP (Moorestown and Medfield hardware)>
  * Copyright (c) 2010, Intel Corporation.
  * Louis LE GALL <louis.le.gall intel.com>
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU General Public License,
  * version 2, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  *
  * You should have received a copy of the GNU General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  */

#ifndef _LINUX_INTEL_MID_I2S_H
#define _LINUX_INTEL_MID_I2S_H

#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/intel_mid_dma.h>

#include <linux/interrupt.h>

/*
 *	Structure Definitions
 */


/*
 *	SSCR0 settings
 */
enum mrst_ssp_mode {
	SSP_IN_NORMAL_MODE = 0x0,
	SSP_IN_NETWORK_MODE
};

enum mrst_ssp_rx_fifo_over_run_int_mask {
	SSP_RX_FIFO_OVER_INT_ENABLE = 0x0,
	SSP_RX_FIFO_OVER_INT_DISABLE
};

enum mrst_ssp_tx_fifo_under_run_int_mask {
	SSP_TX_FIFO_UNDER_INT_ENABLE = 0x0,
	SSP_TX_FIFO_UNDER_INT_DISABLE
};

enum mrst_ssp_frame_format {
	MOTOROLA_SPI_FORMAT = 0x0,
	TI_SSP_FORMAT,
	MICROWIRE_FORMAT,
	PSP_FORMAT

};

enum mrst_ssp_master_mode_clock_selection {
	SSP_ONCHIP_CLOCK = 0x0,
	SSP_NETWORK_CLOCK,
	SSP_EXTERNAL_CLOCK,
	SSP_ONCHIP_AUDIO_CLOCK,
	SSP_MASTER_CLOCK_UNDEFINED = 0xFF
};

/*
 *	SSCR1 settings
 */
enum mrst_ssp_txd_tristate_last_phase {
	TXD_TRISTATE_LAST_PHASE_OFF =  0x0,
	TXD_TRISTATE_LAST_PHASE_ON
};

enum mrst_ssp_txd_tristate_enable {
	TXD_TRISTATE_OFF =  0x0,
	TXD_TRISTATE_ON
};

enum mrst_ssp_slave_sspclk_free_running {
	SLAVE_SSPCLK_ON_ALWAYS =  0x0,
	SLAVE_SSPCLK_ON_DURING_TRANSFER_ONLY
};

enum mrst_ssp_sspsclk_direction {
	SSPSCLK_MASTER_MODE = 0x0,
	SSPSCLK_SLAVE_MODE
};

enum mrst_ssp_sspsfrm_direction {
	SSPSFRM_MASTER_MODE = 0x0,
	SSPSFRM_SLAVE_MODE
};

enum mrst_ssp_rx_without_tx {
	RX_AND_TX_MODE = 0x0,
	RX_WITHOUT_TX_MODE
};

enum mrst_trailing_byte_mode {
	SSP_TRAILING_BYTE_HDL_BY_IA = 0x0,
	SSP_TRAILING_BYTE_HDL_BY_DMA
};

enum mrst_ssp_tx_dma_status {
	SSP_TX_DMA_MASK = 0x0,
	SSP_TX_DMA_ENABLE
};

enum mrst_ssp_rx_dma_status {
	SSP_RX_DMA_MASK = 0x0,
	SSP_RX_DMA_ENABLE
};

enum mrst_ssp_rx_timeout_int_status {
	SSP_RX_TIMEOUT_INT_DISABLE = 0x0,
	SSP_RX_TIMEOUT_INT_ENABLE
};

enum mrst_ssp_trailing_byte_int_status {
	SSP_TRAILING_BYTE_INT_DISABLE = 0x0,
	SSP_TRAILING_BYTE_INT_ENABLE
};

enum mrst_ssp_loopback_mode_status {
	SSP_LOOPBACK_OFF = 0x0,
	SSP_LOOPBACK_ON
};


/*
 *	SSPSP settings: for PSP Format ONLY!!!!!!!!
 */

enum mrst_ssp_frame_sync_relative_timing_bit {
	NEXT_FRMS_ASS_AFTER_END_OF_T4 =  0x0,
	NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM
};

enum mrst_ssp_frame_sync_polarity_bit {
	SSP_FRMS_ACTIVE_LOW =  0x0,
	SSP_FRMS_ACTIVE_HIGH
};

enum mrst_ssp_end_of_transfer_data_state {
	SSP_END_DATA_TRANSFER_STATE_LOW = 0x0,
	SSP_END_DATA_TRANSFER_STATE_PEVIOUS_BIT
};

enum mrst_ssp_clk_mode {
	SSP_CLK_MODE_0 = 0x0,
	SSP_CLK_MODE_1,
	SSP_CLK_MODE_2,
	SSP_CLK_MODE_3
};


/*
 *	list of differents types of SSP, value depends of adid entry of
 *	capability ID of the PCI
 */

/*
 *
 * The PCI header associated to SSP devices now includes a configuration
 * register. It provides information to a driver which is probed for the
 * SSP, specifying in which way the SSP is supposed to be used. Here is
 * the format of this byte register:
 *
 *	bits 2..0: Mode
 *		000=0x0 : Invalid, the register should be ignored
 *		001=0x1 : SSP to be used as SPI controller
 *		010=0x2: SSP to be used in I2S/ISS mode
 *		other: Reserved
 *
 *	bits 5..3: Configuration
 *	In I2S/ISS mode:
 *		000=0x0: Invalid
 *		001=0x1: Bluetooth
 *		010=0x2: Modem
 *		other: Reserved
 *	In SPI mode:
 *		Value is the SPI bus number connected to the SSP.
 *		To be used for registration to the Linux SPI
 *		framework.
 *	bit 6: SPI slave
 *	Relevant in SPI mode only. If set, indicates the SPI clock
 *	is not provided by the SSP: SPI slave mode.
 *
 *	bit 7: Reserved (0)
 *
 * This configuration register is implemented in the adid field of the
 * Vendor Specific PCI capability associated to the SSP.
 *
 */

#define PCI_ADID_SSP_MODE_SPI  (1)
#define PCI_ADID_SSP_MODE_I2S  (2)

#define PCI_ADID_SSP_CONF_BT_FM  (1<<3)
#define PCI_ADID_SSP_CONF_MODEM  (2<<3)


#define PCI_CAP_ADID_I2S_BT_FM  ((PCI_ADID_SSP_CONF_BT_FM) | (PCI_ADID_SSP_MODE_I2S))
#define PCI_CAP_ADID_I2S_MODEM  ((PCI_ADID_SSP_CONF_MODEM) | (PCI_ADID_SSP_MODE_I2S))

enum intel_mid_i2s_ssp_usage {
	SSP_USAGE_UNASSIGNED = 0x00,
	SSP_USAGE_BLUETOOTH_FM = 0x01,
	SSP_USAGE_MODEM = 0x02
};

/*
 *	Structure used to configure the SSP Port
 *	Please note that only the PSP format and the DMA transfer are supported
 */

struct intel_mid_i2s_settings {
	enum mrst_ssp_mode                         mode;
	enum mrst_ssp_rx_fifo_over_run_int_mask    rx_fifo_interrupt;
	enum mrst_ssp_tx_fifo_under_run_int_mask   tx_fifo_interrupt;
	enum mrst_ssp_frame_format                 frame_format;
	enum mrst_ssp_master_mode_clock_selection  master_mode_clk_selection;              /* for Master Mode Only */
	u8                                         frame_rate_divider_control;
	u16                                        master_mode_serial_clock_rate;          /* for Master Mode Only */
	u16                                        data_size;

	enum mrst_ssp_txd_tristate_last_phase      tx_tristate_phase;
	enum mrst_ssp_txd_tristate_enable          tx_tristate_enable;
	enum mrst_ssp_slave_sspclk_free_running    slave_clk_free_running_status;
	enum mrst_ssp_sspsclk_direction            sspslclk_direction;
	enum mrst_ssp_sspsfrm_direction            sspsfrm_direction;
	enum mrst_ssp_rx_without_tx                ssp_duplex_mode;
	enum mrst_trailing_byte_mode               ssp_trailing_byte_mode;
	enum mrst_ssp_tx_dma_status                ssp_tx_dma;
	enum mrst_ssp_rx_dma_status                ssp_rx_dma;
	enum mrst_ssp_rx_timeout_int_status        ssp_rx_timeout_interrupt_status;
	enum mrst_ssp_trailing_byte_int_status     ssp_trailing_byte_interrupt_status;
	enum mrst_ssp_loopback_mode_status         ssp_loopback_mode_status;
	u8                                         ssp_rx_fifo_threshold;
	u8                                         ssp_tx_fifo_threshold;


	enum mrst_ssp_frame_sync_relative_timing_bit  ssp_frmsync_timing_bit;
	enum mrst_ssp_frame_sync_polarity_bit      ssp_frmsync_pol_bit;
	enum mrst_ssp_end_of_transfer_data_state   ssp_end_transfer_state;
	enum mrst_ssp_clk_mode                     ssp_serial_clk_mode;
	u8                                         ssp_psp_T1;
	u8                                         ssp_psp_T2;
	u8                                         ssp_psp_T4;   /* DMYSTOP */
	u8                                         ssp_psp_T5;   /* SFRMDLY */
	u8                                         ssp_psp_T6;   /* SFRMWDTH */

	u8                                         ssp_active_tx_slots_map;
	u8                                         ssp_active_rx_slots_map;
};

/*
 *	Provided Interface
 */


struct intel_mid_i2s_hdl *intel_mid_i2s_open(enum intel_mid_i2s_ssp_usage usage, const struct intel_mid_i2s_settings *ps_settings);
void  intel_mid_i2s_close(struct intel_mid_i2s_hdl *handle);

int intel_mid_i2s_rd_req(struct intel_mid_i2s_hdl *handle, u32 *dst, size_t len, void *param);
int intel_mid_i2s_wr_req(struct intel_mid_i2s_hdl *handle, u32 *src, size_t len, void *param);
int intel_mid_i2s_enable_ssp(struct intel_mid_i2s_hdl *handle);


int intel_mid_i2s_set_wr_cb(struct intel_mid_i2s_hdl *handle, int (*write_callback)(void *param));
int intel_mid_i2s_set_rd_cb(struct intel_mid_i2s_hdl *handle, int (*read_callback)(void *param));


int intel_mid_i2s_get_tx_fifo_level(struct intel_mid_i2s_hdl *handle);
int intel_mid_i2s_get_rx_fifo_level(struct intel_mid_i2s_hdl *handle);
int intel_mid_i2s_flush(struct intel_mid_i2s_hdl *handle);

#endif /* _LINUX_INTEL_MID_I2S_H */

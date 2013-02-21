/*
 * intel_mid_hsi.h
 *
 * Header for the Intel HSI controller driver.
 *
 * Copyright (C) 2010, 2011 Intel Corporation. All rights reserved.
 *
 * Contact: Jim Stanley <jim.stanley@intel.com>
 * Modified from OMAP SSI driver
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
#ifndef __INTEL_MID_HSI_H__
#define __INTEL_MID_HSI_H__

#define HSI_MID_MAX_CHANNELS	8
#define HSI_MAX_GDD_LCH		8

struct hsi_mid_platform_data {
	int	rx_dma_channels[HSI_MID_MAX_CHANNELS];
	int	rx_fifo_sizes[HSI_MID_MAX_CHANNELS];
	int	tx_dma_channels[HSI_MID_MAX_CHANNELS];
	int	tx_fifo_sizes[HSI_MID_MAX_CHANNELS];
	int	gpio_mdm_rst_out;
	int	gpio_mdm_pwr_on;
	int	gpio_mdm_rst_bbn;
	int	gpio_fcdp_rb;
};

#endif /* __INTEL_MID_HSI_H__ */

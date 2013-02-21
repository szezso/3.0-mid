/****************************************************************************
 *
 * Driver IOCTLs for the IFX spi modem.
 *
 * Copyright (C) 2009 Intel Corp
 * Jim Stanley <jim.stanley@intel.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA
 *
 *
 *
 *****************************************************************************/
#ifndef _IFX6X60_IOCTL_H
#define _IFX6X60_IOCTL_H

#include <linux/ioctl.h>

#define IFX_SPI_MAGIC	0x77

/*
 * IFX_GET_SILENT_RESET	- get the number of silent (unsolicited) resets
 *			  since the last request. This is a clearing operation
 */
#define IFX_GET_SILENT_RESET	_IOR(IFX_SPI_MAGIC, 1, int)

/*
 * IFX_GET_SPI_TRANSFER_SIZE - gets the current spi message transfer size
 */
#define IFX_GET_SPI_TRANSFER_SIZE	_IOR(IFX_SPI_MAGIC, 2, int)

/*
 * IFX_SET_SPI_TRANSFER_SIZE - set the spi message transfer size to
 *                             any size <= 4096.
 */
#define IFX_SET_SPI_TRANSFER_SIZE	_IOW(IFX_SPI_MAGIC, 3, int)

/*
 * IFX_MODEM_RESET	- reset the modem (solicited reset)
 */
#define IFX_MODEM_RESET		_IO(IFX_SPI_MAGIC, 4)

/*
 * IFX_MODEM_STATE	- return 1 if first_srdy_received, else 0 if not
 */
#define IFX_MODEM_STATE		_IOR(IFX_SPI_MAGIC, 5,int)

#endif /* _IFX6X60_IOCTL_H */

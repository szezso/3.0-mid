/*
 *  intel_sst_loopback.c
 *
 *  Copyright (C) 2008-11	Intel Corp
 *  Authors:	Vikas Gupta <vikas.gupta@intel.com>
 *		Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Hongbing Hu <hongbing.hu@intel.com>
 *		Prem Sasidharan <prem.sasidharan@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file contains all the defines related to loopback
 */

#ifndef _INTEL_SST_LOOPBACK_H_
#define _INTEL_SST_LOOPBACK_H_

struct loopback_app_if {
	/* pointer to memory for the ring buffer */
	void *mem;
	/* user instance count */
	unsigned int user;
	/* flag to signal data availability */
	unsigned int flag;
	/* size of the ring buffer */
	unsigned int size;
	/* read offset */
	unsigned int rd_offset;
	/* length of valid data */
	unsigned int valid_len;
	/* used size for the ring buffer */
	unsigned int valid_size;

	struct mutex lock;
	spinlock_t spin_lock;
	wait_queue_head_t sleep;
};

extern struct loopback_app_if loopback_if;

int loopback_register(void);

int loopback_deregister(void);

void intel_sst_job_processed_notify(void);

#endif

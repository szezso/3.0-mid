/*
 * Copyright (C) 2011 Intel Corporation
 * Author: Imre Deak <imre.deak@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>

#include "sgx_bridge_km.h"
#include "sgxutils.h"
#include "pvr_debug_core.h"

int sgx_print_fw_status_code(char *buf, size_t buf_size, uint32_t status_code)
{
	return scnprintf(buf, buf_size, "pvr firmware status code %08x\n",
			 status_code);
}

int sgx_print_fw_trace_rec(char *buf, size_t buf_size,
			  const struct sgx_fw_state *state, int rec_idx)
{
	const struct sgx_fw_trace_rec *rec;

	rec_idx = (state->write_ofs + rec_idx) % ARRAY_SIZE(state->trace);
	rec = &state->trace[rec_idx];

	return scnprintf(buf, buf_size, "%08X %08X %08X %08X\n",
			rec->v[2], rec->v[3], rec->v[1], rec->v[0]);
}

int sgx_save_fw_state(PVRSRV_DEVICE_NODE *dev_node, struct sgx_fw_state *state)
{
	PVRSRV_SGXDEV_INFO *sgx_info = dev_node->pvDevice;
	struct sgx_fw_state *src;

	if (!sgx_info->psKernelEDMStatusBufferMemInfo)
		return -ENODEV;

	src = sgx_info->psKernelEDMStatusBufferMemInfo->pvLinAddrKM;
	*state = *src;

	return 0;
}

#ifdef CONFIG_PVR_TRACE_FW_DUMP_TO_CONSOLE
static void dump_fw_trace(const struct sgx_fw_state *state)
{
	char buf[48];
	int i;

	for (i = 0; i < ARRAY_SIZE(state->trace); i++) {
		sgx_print_fw_trace_rec(buf, sizeof(buf), state, i);
		printk(KERN_ERR "%s", buf);
	}
}
#else
static void dump_fw_trace(const struct sgx_fw_state *state)
{
}
#endif

void sgx_dump_fw_state(PVRSRV_DEVICE_NODE *dev_node)
{
	struct sgx_fw_state *state;
	char buf[48];

	state = vmalloc(sizeof(*state));
	if (!state)
		return;

	if (sgx_save_fw_state(dev_node, state) < 0) {
		pr_info("pvr: fw state not available\n");
		vfree(state);

		return;
	}

	sgx_print_fw_status_code(buf, sizeof(buf), state->status_code);
	printk(KERN_ERR "%s", buf);
	dump_fw_trace(state);
	vfree(state);
}

int sgx_trigger_reset(PVRSRV_DEVICE_NODE *dev_node)
{
	PVRSRV_ERROR err;
	int r = 0;

	err = PVRSRVSetDevicePowerStateKM(dev_node->sDevId.ui32DeviceIndex,
					  PVRSRV_DEV_POWER_STATE_ON,
					  KERNEL_ID, IMG_TRUE);
	if (err != PVRSRV_OK)
		return -EIO;

	HWRecoveryResetSGXNoLock(dev_node);

	PVRSRVPowerUnlock(KERNEL_ID);
	/* power down if no activity */
	SGXTestActivePowerEvent(dev_node, KERNEL_ID);

	return r;
}

void sgx_save_registers_no_pwron(PVRSRV_DEVICE_NODE *dev_node,
				 struct sgx_registers *regs)
{
	int reg;

	for (reg = 0; reg < ARRAY_SIZE(regs->v); reg++)
		regs->v[reg] = sgx_read_reg(dev_node, reg * 4);
}

int sgx_save_registers(PVRSRV_DEVICE_NODE *dev_node, struct sgx_registers *regs)
{
	PVRSRV_ERROR err;

	err = PVRSRVSetDevicePowerStateKM(dev_node->sDevId.ui32DeviceIndex,
					  PVRSRV_DEV_POWER_STATE_ON,
					  KERNEL_ID, IMG_TRUE);
	if (err != PVRSRV_OK)
		return -EIO;

	sgx_save_registers_no_pwron(dev_node, regs);

	PVRSRVPowerUnlock(KERNEL_ID);
	/* power down if no activity */
	SGXTestActivePowerEvent(dev_node, KERNEL_ID);

	return 0;
}

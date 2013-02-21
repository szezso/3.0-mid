#ifndef _PVR_DEBUG_CORE_H
#define _PVR_DEBUG_CORE_H

#include "sgxinfokm.h"
#include "sgx_mkif_km.h"

#define SGX_SAVE_REG_COUNT (0x1000 / 4)

struct sgx_fw_trace_rec {
	uint32_t v[4];
};

struct sgx_fw_state {
	uint32_t status_code;
	uint32_t write_ofs;
	struct sgx_fw_trace_rec trace[SGXMK_TRACE_BUFFER_SIZE];
};

struct sgx_registers {
	uint32_t v[SGX_SAVE_REG_COUNT];
};

int sgx_print_fw_status_code(char *buf, size_t buf_size, uint32_t status_code);
int sgx_print_fw_trace_rec(char *buf, size_t buf_size,
			  const struct sgx_fw_state *state, int rec_idx);
int sgx_save_fw_state(PVRSRV_DEVICE_NODE *dev_node, struct sgx_fw_state *state);
void sgx_dump_fw_state(PVRSRV_DEVICE_NODE *dev_node);
int sgx_trigger_reset(PVRSRV_DEVICE_NODE *dev_node);
void sgx_save_registers_no_pwron(PVRSRV_DEVICE_NODE *dev_node,
				 struct sgx_registers *regs);
int sgx_save_registers(PVRSRV_DEVICE_NODE *dev_node,
			struct sgx_registers *regs);

#endif

#ifndef _ASM_X86_INTEL_SCU_IPC_H_
#define  _ASM_X86_INTEL_SCU_IPC_H_

#include <linux/notifier.h>

/* IPC defines the following message types */
#define IPCMSG_CHAABI_REVISION	0xE2 /* Get CHAABI revision */
#define IPCMSG_BATTERY          0xEF /* Coulomb Counter Accumulator */
#define IPCMSG_MIP_ACCESS       0xEC /* IA MIP access */
#define IPCMSG_WARM_RESET	0xF0
#define IPCMSG_COLD_RESET	0xF1
#define IPCMSG_SOFT_RESET	0xF2
#define IPCMSG_COLD_BOOT	0xF3
#define IPCMSG_FW_REVISION      0xF4 /* Get firmware revision */
#define IPCMSG_WATCHDOG_TIMER   0xF8 /* Set Kernel Watchdog Threshold */
#define IPCMSG_VRTC		0xFA	 /* Set vRTC device */
#define IPCMSG_FW_UPDATE        0xFE /* Firmware update */
#define IPCMSG_PCNTRL           0xFF /* Power controller unit read/write */

#define IPC_CMD_UMIP_RD     0
#define IPC_CMD_UMIP_WR     1
#define IPC_CMD_SMIP_RD     2

/* Command id associated with message IPCMSG_PCNTRL */
#define IPC_CMD_PCNTRL_W      0 /* Register write */
#define IPC_CMD_PCNTRL_R      1 /* Register read */
#define IPC_CMD_PCNTRL_M      2 /* Register read-modify-write */

/* Command id associated with message IPCMSG_VRTC */
#define IPC_CMD_VRTC_SETTIME      1 /* Set time */
#define IPC_CMD_VRTC_SETALARM     2 /* Set alarm */
#define IPC_CMD_VRTC_SYNC_RTC     3 /* Sync MSIC/PMIC RTC to VRTC */

/* Command buffer offset of revision info */
#define IPC_CMD_BUF_SIZE 16

#define IPC_CMD_REV_IFWI_MAJOR	15 /* Integrated image major version */
#define IPC_CMD_REV_IFWI_MINOR	14 /* Integrated image minor version */

#define IPC_CMD_REV_VAL_MAJOR	11 /* Validation hooks major version */
#define IPC_CMD_REV_VAL_MINOR	10 /* Validation hooks minor version */
#define IPC_CMD_REV_IASUP_MAJOR	 9 /* Supplemental IA FW major version */
#define IPC_CMD_REV_IASUP_MINOR	 8 /* Supplemental IA FW minor version */

#define IPC_CMD_REV_IAFW_MAJOR	 7 /* x86 boot FW major version */
#define IPC_CMD_REV_IAFW_MINOR	 6 /* x86 boot FW minor version */
#define IPC_CMD_REV_PUNIT_MAJOR	 5 /* pUnit FW major version */
#define IPC_CMD_REV_PUNIT_MINOR	 4 /* pUnit FW minor version */

#define IPC_CMD_REV_SCUBT_MAJOR	 3 /* SCU bootstrap FW major version */
#define IPC_CMD_REV_SCUBT_MINOR	 2 /* SCU bootstrap FW minor version */
#define IPC_CMD_REV_SCURT_MAJOR	 1 /* SCU runtime FW major version */
#define IPC_CMD_REV_SCURT_MINOR	 0 /* SCU runtime FW minor version */

#define IPC_CMD_CHAABI_EXT_MAJOR	7 /* Chaabi Extended FW major version */
#define IPC_CMD_CHAABI_EXT_MINOR	6 /* Chaabi Extended FW minor version */

#define IPC_CMD_CHAABI_ICACHE_MAJOR	3 /* Chaabi i-cache major version */
#define IPC_CMD_CHAABI_ICACHE_MINOR	2 /* Chaabi i-cache minor version */
#define IPC_CMD_CHAABI_RESIDENT_MAJOR	1 /* Chaabi resident major version */
#define IPC_CMD_CHAABI_RESIDENT_MINOR	0 /* Chaabi resident minor version */

/* Read single register */
int intel_scu_ipc_ioread8(u16 addr, u8 *data);

/* Read two sequential registers */
int intel_scu_ipc_ioread16(u16 addr, u16 *data);

/* Read four sequential registers */
int intel_scu_ipc_ioread32(u16 addr, u32 *data);

/* Read a vector */
int intel_scu_ipc_readv(u16 *addr, u8 *data, int len);

/* Write single register */
int intel_scu_ipc_iowrite8(u16 addr, u8 data);

/* Write two sequential registers */
int intel_scu_ipc_iowrite16(u16 addr, u16 data);

/* Write four sequential registers */
int intel_scu_ipc_iowrite32(u16 addr, u32 data);

/* Write a vector */
int intel_scu_ipc_writev(u16 *addr, u8 *data, int len);

/* Update single register based on the mask */
int intel_scu_ipc_update_register(u16 addr, u8 data, u8 mask);

/* Issue commands to the SCU with or without data */
int intel_scu_ipc_simple_command(int cmd, int sub);
int intel_scu_ipc_command(int cmd, int sub, u32 *in, int inlen,
							u32 *out, int outlen);
/* Reboot */
void intel_scu_ipc_restart(char *);
void intel_scu_ipc_emergency_restart(void);

/* I2C control api */
int intel_scu_ipc_i2c_cntrl(u32 addr, u32 *data);

/* Update FW version */
int intel_scu_ipc_mrstfw_update(u8 *buffer, u32 length);
int intel_scu_ipc_medfw_ota(void  __user *arg);
int intel_scu_ipc_read_mip(u8 *data, int len, int offset, int issigned);
int intel_scu_ipc_write_umip(u8 *data, int len, int offset);

/* OSHOB-OS Handoff Buffer read */
int intel_scu_ipc_read_oshob(u8 *data, int len, int offset);
/* OSNIB-OS No Init Buffer write */
int intel_scu_ipc_write_osnib(u8 *data, int len, int offset, u32 mask);

extern struct blocking_notifier_head intel_scu_notifier;

static inline void intel_scu_notifier_add(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&intel_scu_notifier, nb);
}

static inline void intel_scu_notifier_remove(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&intel_scu_notifier, nb);
}

static inline int intel_scu_notifier_post(unsigned long v, void *p)
{
	return blocking_notifier_call_chain(&intel_scu_notifier, v, p);
}

#define		SCU_AVAILABLE		1
#define		SCU_DOWN		2

#define MSIC_VPROG1_CTRL	0xD6
#define MSIC_VPROG2_CTRL	0xD7
#define MSIC_VPROG_ON		0xFF
#define MSIC_VPROG_OFF		0

#define MSIC_REG_PWM0CLKDIV1 0x61
#define MSIC_REG_PWM0CLKDIV0 0x62
#define MSIC_REG_PWM0DUTYCYCLE 0x67

/* Helpers to turn on/off msic vprog1 and vprog2 */
static inline int intel_scu_ipc_msic_vprog1(int on)
{
	return intel_scu_ipc_iowrite8(MSIC_VPROG1_CTRL,
			on ? MSIC_VPROG_ON : MSIC_VPROG_OFF);
}

static inline int intel_scu_ipc_msic_vprog2(int on)
{
	return intel_scu_ipc_iowrite8(MSIC_VPROG2_CTRL,
			on ? MSIC_VPROG_ON : MSIC_VPROG_OFF);
}

#define IPCMSG_OSC_CLK	0xE6 /* Turn on/off osc clock */

/*
 * Penwell has 4 osc clocks:
 * 0:   AUDIO
 * 1,2: CAMERA SENSORS
 * 3:   DISP_BUF_CLK
 */
#define OSC_CLK_CAM0	1
#define OSC_CLK_CAM1	2

/* SCU IPC COMMAND(osc clk on/off) definition:
 * ipc_wbuf[0] = clock to act on {0, 1, 2, 3}
 * ipc_wbuf[1] =
 * bit 0 - 1:on  0:off
 * bit 1 - if 1, read divider setting from bits 3:2 as follows:
 * bit [3:2] - 00: clk/1, 01: clk/2, 10: clk/4, 11: reserved
 */
static inline int intel_scu_ipc_osc_clk(u8 clk, u8 on)
{
	u8 ipc_wbuf[16];
	int ipc_ret;

	ipc_wbuf[0] = clk & 0x3;
	ipc_wbuf[1] = on & 1; /* no divider */

	ipc_ret = intel_scu_ipc_command(IPCMSG_OSC_CLK, 0,
					(u32 *)ipc_wbuf, 2, NULL, 0);
	if (ipc_ret != 0)
		pr_err("%s: failed to set osc clk(%d) output\n", __func__, clk);

	return ipc_ret;
}

#endif

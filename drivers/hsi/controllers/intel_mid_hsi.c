/*
 * intel_mid_hsi.c
 *
 * Implements the Intel HSI driver.
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 * Copyright (C) 2010, 2011 Intel Corporation.
 *
 * Contact: Jim Stanley <jim.stanley@intel.com>
 * Modified from OMAP HSI driver
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
#define DEFER_IRQ_CLEARING
/*#define USE_MASTER_DMA_IRQ*/

#include <linux/err.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/interrupt.h>
#include <linux/hsi/hsi.h>
#include <linux/hsi/intel_mid_hsi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/pm_runtime.h>

#include "intel_mid_hsi.h"
#include "hsi_dwahb_dma.h"

#define HSI_RTPM_INACTIVITY_DELAY	1000	/* idle time before
						 * suspending in ms */
#define HSI_RTPM_TX_DRAIN_DELAY	100	/* wait time before flush tx data ms */
#define HSI_RTPM_RX_DRAIN_DELAY	100	/* wait time before flush rx data ms */
#define HSI_RTPM_TIMER_CNT_MAX	10

enum hsi_rtpm_state {
	HSI_RTPM_ACTIVE,		/* D0: active */
	HSI_RTPM_D0I3,			/* D0i3: runtime suspend state */
	HSI_RTPM_D3,			/* D3: system suspend state */
	HSI_RTPM_TXRX_ON,		/* D0: tx/rx is going on */
	HSI_RTPM_TXRX_PENDING,	/* D0: tx/rx is completed,
				* but wait before assuring idle */
	HSI_RTPM_TXRX_IDLE,		/* D0: tx/rx is assured to be idle */
};

#define HSI_MPU_IRQ_NAME	"HSI_CONTROLLER_IRQ"
#define HSI_DMA_IRQ_NAME	"HSI_MASTER_DMA_IRQ"

#define HSI_RESETDONE_TIMEOUT	10	/* 10 ms */
#define HSI_RESETDONE_RETRIES	20	/* => max 200 ms waiting for reset */

#define HSI_INTEL_MAX_CHANNELS	8
#define HSI_MAX_GDD_LCH		8
#define HSI_MAX_FIFO_BITS	10
#define HSI_BYTES_TO_FRAMES(x) (((x) + 3) >> 2)

#define HSI_BREAK_DELAY		70	/* approx usec for break */

#define HSI_MASTER_DMA_ID	0x834	/* PCI id for master dma */

#define TX_THRESHOLD_HALF
#ifdef TX_THRESHOLD_HALF
#define TX_THRESHOLD		HSI_TX_FIFO_THRESHOLD_HALF_EMPTY
#define NUM_TX_FIFO_DWORDS(x)	(x / 2)
#else
#define TX_THRESHOLD		HSI_TX_FIFO_THRESHOLD_ALMOST_EMPTY
#define NUM_TX_FIFO_DWORDS(x)		((3 * x) / 4)
#endif

#define RX_THRESHOLD_HALF
#ifdef RX_THRESHOLD_HALF
#define RX_THRESHOLD		HSI_RX_FIFO_THRESHOLD_HALF_FULL
#define NUM_RX_FIFO_DWORDS(x)	(x / 2)
#else
#define RX_THRESHOLD		HSI_RX_FIFO_THRESHOLD_ALMOST_FULL
#define NUM_RX_FIFO_DWORDS(x)		((3 * x) / 4)
#endif

#ifdef USE_MASTER_DMA_IRQ
#define DMA_IRQ 30
#endif

/**
 * struct intel_hsi_controller - Arasan HSI controller data
 * @dev: device associated to the controller (HSI controller)
 * @sys: HSI I/O base address
 * @gdd: GDD I/O base address
 * @pdev: PCI dev* for HSI controller
 * @dmac: PCI dev* for master DMA controller
 * @gdd_trn: Array of GDD transaction data for ongoing GDD transfers
 * @loss_count: To follow if we need to restore context or not
 * @dir: Debugfs HSI root directory
 * @pm_lock: lock to serialize access to power states
 * @pmstate: runtime power management state of HSI controller,
 *			may be one of ACTIVE, D0I3, or D3.
 * @d3_delay_wq: a wait queue to delay system suspend when there
 *			is still data pending for either tx or rx.
 * @delay_interrupt: a flag to show if a resume is going on for an
 *			interrupt (when 1). If yes, the device is already on
 *			the way to resume, thus	no need to resume again.
 */
struct intel_hsi_controller {
	struct device		*dev;
	void __iomem		*sys;
	void __iomem		*gdd;
	struct pci_dev		*pdev;
	struct pci_dev		*dmac;
	struct hsi_msg		*gdd_trn[HSI_MAX_GDD_LCH];
	int			loss_count;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dir;
#endif
	spinlock_t		pm_lock;
	int				pmstate;
	wait_queue_head_t       d3_delay_wq;
	int				delay_interrupt;
};

/**
 * struct intel_ssm_ctx - Arasan synchronous serial module (TX/RX) context
 * @mode: Bit transmission mode -- stream/frame
 * @channels: Bit field coding number of channels (0, 1, 2, 3)
 * @wake: TX/RX wake or asleep
 * @threshold: TX/RX FIFO threshold
 * @timeout: RX frame timeout
 * @flow: RX flow -- sync/pipeline
 * @divisot: TX divider
 * @arb_mode: Arbitration mode for TX frame (Round robin, priority)
 * @state: tx/rx power management state of HSI port,
 *			may be one of T(R)X_ON, T(R)X_PENDING, or T(R)X_IDLE.
 * @timer: tx drain timer for sst -- check if tx is idle periodically
 *			rx drain timer for ssr -- check if rx is idle
 *			periodically
 * @timer_cnt: counter for tx/rx drain timer. When the number that
 *			drain timer is scheduled reaches a max threshold,
 *			the state machine will be reset to the original state
 *			regardless of CA_WAKE. This makes the code more robust.
 * @delay_activate: a flag to delay tx/rx activation to resume time
 *			if set.
 * @delay_senddata: a flag to delay tx/rx data transfer to resume
 *			time if set.
 */
struct intel_ssm_ctx {
	u32	mode;
	u32	channels;
	u32	threshold;
	union	{
			u32	timeout; /* Rx Only */
			u32	flow;
			struct	{
					u32	arb_mode;
					u32	divisor;
			}; /* Tx only */
	};
	u32 state;
	struct timer_list	timer;
	u32 timer_cnt;
	unsigned int	delay_activate:1;
	unsigned int	delay_senddata:1;
};

/**
 * struct intel_hsi_port - HSI port data
 * @dev: device associated to the port (HSI port)
 * @lock: Spin lock to serialize access to the HSI port
 * @channels: Current number of channels configured (1,2,4 or 8)
 * @txqueue: TX message queues
 * @rxqueue: RX message queues
 * @tx_brkqueue: Queue of outgoing HWBREAK requests (FRAME mode)
 * @rx_brkqueue: Queue of incoming HWBREAK requests (FRAME mode)
 * @irq: IRQ number
 * @pio_tasklet: Bottom half for PIO transfers and events
 * @sys_mpu_enable: Context for the interrupt enable register for irq 0
 * @sst: Context for the synchronous serial transmitter
 * @ssr: Context for the synchronous serial receiver
 * @pm_lock: Spin lock to serialize access to runtime pm state of a port
 * @wk_refcount: Reference count for start_tx and stop_tx calls from
 *				the protocol driver.
 * @setup_completed: A flag to show if a client setup is completed (1)
 *				or not (0).
 */
struct intel_hsi_port {
	struct device		*dev;
	spinlock_t		lock;
	unsigned int		channels;
	struct list_head	txqueue[HSI_INTEL_MAX_CHANNELS];
	struct list_head	rxqueue[HSI_INTEL_MAX_CHANNELS];
	struct list_head	tx_brkqueue;
	struct list_head	rx_brkqueue;
	struct list_head	fwdqueue;
	unsigned int		irq;
#ifdef DEFER_IRQ_CLEARING
	struct tasklet_struct	pio_tasklet;
#endif
	struct tasklet_struct	fwd_tasklet;
#ifdef USE_MASTER_DMA_IRQ
	struct tasklet_struct	dma_tasklet;
#endif
	/* HSI port context */
	u32			sys_mpu_enable; /* We use only one irq */
	struct intel_ssm_ctx	sst;
	struct intel_ssm_ctx	ssr;
	spinlock_t		pm_lock;
	atomic_t		wk_refcount;
	int				setup_completed;
};

static u32 fifo_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

/* save fifo sizes given when client calls setup */
static u32 rx_fifo_size[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static u32 tx_fifo_size[8] = {0, 0, 0, 0, 0, 0, 0, 0};

/* map of HSI logical channels read/write to DMA channels */
/* arrays are indexed by HSI channel numbers 0 - 7 */
static s32 dma_read_channels[]	= {-1, -1, -1, -1, -1, -1, -1, -1};
static s32 dma_write_channels[] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* forward references */
static int __devinit hsi_hw_init(struct hsi_controller *hsi);
static void hsi_clear_controller_status(struct hsi_controller *hsi);
static void hsi_enable_error_interrupts(struct hsi_controller *hsi);
static void hsi_init_slave_dma_channel(void __iomem *slave_base,
						u8 tx_rx,
						u8 dma_channel,
						u8 logical_channel);

static void hsi_init_master_dma_channel(void __iomem *dma_base, u8 tx_rx,
						u8 dma_channel,
						u8 logical_channel);
static void hsi_rtpm_txdraintimer_fn(unsigned long data);
static void hsi_rtpm_rxdraintimer_fn(unsigned long data);
static void set_hsi_ports_default(struct intel_hsi_controller *hsi);
static int hsi_start_dma(struct hsi_msg *msg, int lch);
static int hsi_softreset(struct hsi_controller *hsi);
static void hsi_async_data_transfer(struct hsi_msg *msg,
		struct intel_hsi_port *intel_port);
static int hsi_enable_tx_clock(void __iomem *base);

/**
 * hsi_set_ctrl_pmstate - helper function to set power state
 * @intel_hsi: pointer to intel_hsi_controller structure
 * @state: state to set to.
 */
static inline void hsi_set_ctrl_pmstate(
		struct intel_hsi_controller *intel_hsi,
		int state)
{
	unsigned long flags;

	spin_lock_irqsave(&intel_hsi->pm_lock, flags);
	intel_hsi->pmstate = state;
	spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);
}

/**
 * hsi_set_ctrl_pmstate - helper function to check power state
 * @intel_hsi: pointer to intel_hsi_controller structure
 * @state: state to check.
 */
static inline int hsi_ctrl_is_state(
		struct intel_hsi_controller *intel_hsi,
		int state)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&intel_hsi->pm_lock, flags);
	ret = (intel_hsi->pmstate == state);
	spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);

	return ret;
}

/**
 * hsi_setup_completed - check if setup is completed.
 * @intel_port: intel_hsi_port structure
 *
 * Return 1 if completed or 0 if not.
 */
static inline int hsi_setup_completed(
		struct intel_hsi_port *intel_port)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	ret = intel_port->setup_completed;
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	return ret;
}

/**
 * _hsi_rtpm_txrx_is_idle - check if tx/rx idle conditions are satisfied.
 * @read_write: HSI_LOGICAL_READ or HSI_LOGICAL_WRITE
 * @port: the port to check
 *
 * Return 1 when satisfied, and 0 when not satisfied.
 * Need to be guarded by lock.
 */
static int _hsi_rtpm_txrx_is_idle(u8 read_write, struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(intel_port->dev->parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	int i;
	u32 tmp;

	/* check if fifos are empty */
	tmp = (read_write == HSI_LOGICAL_READ) ? \
			(get_hsi_status_rx_fifos_empty(base) != 0xff) : \
			(get_hsi_status_tx_fifos_empty(base) != 0xff);
	if (tmp)
		return 0;

	/* Check if queues are empty (for tx only)
	 * After checking the queues, we do not have to check the pending
	 * DMA operations.
	 */
	if (read_write == HSI_LOGICAL_WRITE) {
		for (i = 0; i < intel_port->channels; i++) {
			if (!list_empty(&intel_port->txqueue[i]))
				return 0;
		}
	}

	/* Check if fwdqueue is empty
	 * Since it's impossible to distinguish tx/rx for fwdqueue,
	 * we check the entire fwdqueue for both tx and rx.
	 */
	if (!list_empty(&intel_port->fwdqueue))
		return 0;

	/* Check if tx brkqueue is empty */
	if ((read_write == HSI_LOGICAL_WRITE) && \
		!list_empty(&intel_port->tx_brkqueue))
		return 0;

	/* Check if there's any pending interrupt
	 * Like fwdqueue, we check the entire interrupt status register
	 * for both tx and rx to simplify the codes.
	 */
	if (get_hsi_interrupt_status(base) != 0)
		return 0;

	return 1;
}

/**
 * hsi_rtpm_rx_activate - transfer rx to RX_ON state.
 * @port: port to enable rx
 *
 * Serve as a bottom half for rx state transition triggered by
 * CA_WAKE interrupt.
 */
static inline void hsi_rtpm_rx_activate(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	set_hsi_program_rx_wake(base, HSI_PROGRAM_RX_WAKE);
	del_timer(&intel_port->ssr.timer);
	mod_timer(&intel_port->ssr.timer, jiffies + \
		msecs_to_jiffies(HSI_RTPM_RX_DRAIN_DELAY));
}

/**
 * hsi_rtpm_tx_activate - transfer tx to TX_ON state.
 * @port: port to enable tx
 *
 * Assert AC_WAKE when the device is fully active.
 */
static inline void hsi_rtpm_tx_activate(struct hsi_port *port)
{
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	int err;

	/* enable the TX clock if it's not already running */
	err = hsi_enable_tx_clock(base);
	if (err < 0) {
		/* clock won't come up -- signal error */
		dev_crit(&hsi->device, "clock won't start\n");
		return;
	}

	set_hsi_program_tx_wakeup(base, HSI_PROGRAM_TX_WAKE);
}

/**
 * _hsi_rtpm_put_tx_to_idle - transfer tx to TX_IDLE state.
 * @port: port to enable tx
 *
 * Transfer tx sub-state machine to TX_IDLE state, and decrement
 * runtime power management usage count.
 */
static void _hsi_rtpm_put_tx_to_idle(struct hsi_port *port,
		unsigned long *flags)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	set_hsi_program_tx_wakeup(base, HSI_PROGRAM_TX_SLEEP);
	intel_port->sst.timer_cnt = 0;

	if (intel_port->sst.state != HSI_RTPM_TXRX_IDLE) {
		intel_port->sst.state = HSI_RTPM_TXRX_IDLE;
		spin_unlock_irqrestore(&intel_port->pm_lock, *flags);

		pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
		pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

		spin_lock_irqsave(&intel_port->pm_lock, *flags);
	}
}

/**
 * _hsi_rtpm_put_rx_to_idle - transfer rx to RX_IDLE state.
 * @port: port to enable rx
 *
 * Transfer rx sub-state machine to RX_IDLE state, and decrement
 * runtime power management usage count.
 */
static void _hsi_rtpm_put_rx_to_idle(struct hsi_port *port,
		unsigned long *flags)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	set_hsi_program_rx_wake(base, HSI_PROGRAM_RX_SLEEP);
	intel_port->ssr.timer_cnt = 0;

	if (intel_port->ssr.state != HSI_RTPM_TXRX_IDLE) {
		intel_port->ssr.state = HSI_RTPM_TXRX_IDLE;
		spin_unlock_irqrestore(&intel_port->pm_lock, *flags);

		hsi_event(port, HSI_EVENT_STOP_RX);
		pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
		pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

		spin_lock_irqsave(&intel_port->pm_lock, *flags);
	}
}

/**
 * hsi_rtpm_port_fsm_reset - reset runtime pm state machine for a port
 * @port: port to reset
 *
 * Reset TX/RX sub state machine to IDLE state, and reset other data
 * fields related to a port's power management.
 */
static void hsi_rtpm_port_fsm_reset(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	unsigned long flags;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	_hsi_rtpm_put_tx_to_idle(port, &flags);
	_hsi_rtpm_put_rx_to_idle(port, &flags);
	intel_port->setup_completed = 0;

	intel_port->sst.delay_activate = 0;
	intel_port->ssr.delay_activate = 0;

	intel_port->sst.delay_senddata = 0;
	intel_port->ssr.delay_senddata = 0;
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	atomic_set(&intel_port->wk_refcount, 0);
}

/**
 * hsi_rtpm_ctrl_fsm_reset - reset runtime pm state machine for HSI
 * controller.
 * @intel_hsi: pointer to intel_hsi_controller structure
 *
 * Reset state machine to ACTIVE state, and reset other data
 * fields related to a controller's power management.
 */
static void hsi_rtpm_ctrl_fsm_reset(struct intel_hsi_controller *intel_hsi)
{
	unsigned long flags;

	spin_lock_irqsave(&intel_hsi->pm_lock, flags);
	intel_hsi->pmstate = HSI_RTPM_ACTIVE;
	intel_hsi->delay_interrupt = 0;
	spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);
}

/**
 * hsi_rtpm_fsm_reset - reset runtime pm state machine for a port
 * and the whole controller if needed.
 * @port: port to reset
 *
 * Reset the port to IDLE state, and reset the controller's state machine as
 * needed.
 */
static void hsi_rtpm_fsm_reset(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);

	hsi_rtpm_port_fsm_reset(port);
	del_timer(&intel_port->sst.timer);
	del_timer(&intel_port->ssr.timer);
	hsi_rtpm_ctrl_fsm_reset(intel_hsi);
}

/**
 * hsi_rtpm_port_fsm_init - init runtime pm state machine for a port
 * @port: port to reset
 *
 * Init TX/RX sub state machine to IDLE state, and reset other data
 * fields related to a port's power management.
 */
static void hsi_rtpm_port_fsm_init(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	unsigned long flags;

	spin_lock_init(&intel_port->pm_lock);
	spin_lock_irqsave(&intel_port->pm_lock, flags);
	intel_port->sst.state = HSI_RTPM_TXRX_IDLE;
	intel_port->ssr.state = HSI_RTPM_TXRX_IDLE;
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	hsi_rtpm_port_fsm_reset(port);

	setup_timer(&intel_port->sst.timer, hsi_rtpm_txdraintimer_fn,
			(unsigned long)intel_port);
	setup_timer(&intel_port->ssr.timer, hsi_rtpm_rxdraintimer_fn,
			(unsigned long)intel_port);
}

/**
 * hsi_rtpm_ctrl_fsm_init - init runtime pm state machine for HSI
 * controller.
 * @intel_hsi: pointer to intel_hsi_controller structure
 *
 * Reset state machine to ACTIVE state, and reset other data
 * fields related to a controller's power management.
 */
static void hsi_rtpm_ctrl_fsm_init(struct intel_hsi_controller *intel_hsi)
{
	spin_lock_init(&intel_hsi->pm_lock);
	init_waitqueue_head(&intel_hsi->d3_delay_wq);
	hsi_rtpm_ctrl_fsm_reset(intel_hsi);
}

/**
 * hsi_rtpm_txdraintimer_fn - Timer function for tx drain timer.
 * @data: a pointer to intel_port structure.
 *
 * Check if tx idle conditions are satisfied, and make corresponding
 * state transitions.
 */
static void hsi_rtpm_txdraintimer_fn(unsigned long data)
{
	struct intel_hsi_port *intel_port = (struct intel_hsi_port *)data;
	struct hsi_port *port = to_hsi_port(intel_port->dev);
	unsigned long flags;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	if (intel_port->sst.state == HSI_RTPM_TXRX_IDLE) {
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
		return;
	}

	if ((!_hsi_rtpm_txrx_is_idle(HSI_LOGICAL_WRITE, port)) && \
		(intel_port->sst.timer_cnt++ < HSI_RTPM_TIMER_CNT_MAX))
		/* Tx state remains in TX_PENDING */
		mod_timer(&intel_port->sst.timer, jiffies + \
			msecs_to_jiffies(HSI_RTPM_TX_DRAIN_DELAY));
	else
		/* TX_PENDING -> TX_IDLE */
		_hsi_rtpm_put_tx_to_idle(port, &flags);

	spin_unlock_irqrestore(&intel_port->pm_lock, flags);
}

/**
 * hsi_rtpm_rxdraintimer_fn - Timer function for rx drain timer.
 * @data: a pointer to intel_port structure.
 *
 * Check if rx idle conditions are satisfied, and make corresponding
 * state transitions.
 */
static void hsi_rtpm_rxdraintimer_fn(unsigned long data)
{
	struct intel_hsi_port *intel_port = (struct intel_hsi_port *)data;
	struct hsi_port *port = to_hsi_port(intel_port->dev);
	struct hsi_controller *hsi = to_hsi_controller(intel_port->dev->parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	unsigned long flags;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	if (intel_port->ssr.state == HSI_RTPM_TXRX_IDLE)
		goto out;

	if (intel_port->ssr.state == HSI_RTPM_TXRX_ON) {
		if ((get_hsi_status_rx_wake(base) == HSI_SIGNAL_ASSERTED) && \
			(intel_port->ssr.timer_cnt++ < \
					HSI_RTPM_TIMER_CNT_MAX)) {
			/* Rx state remains in RX_ON */
			mod_timer(&intel_port->ssr.timer, jiffies + \
				msecs_to_jiffies( \
				HSI_RTPM_RX_DRAIN_DELAY));
			goto out;
		}

		if (!_hsi_rtpm_txrx_is_idle(HSI_LOGICAL_READ, port)) {
			set_hsi_program_rx_wake(base, HSI_PROGRAM_RX_SLEEP);

			/* RX_ON -> RX_PENDING */
			intel_port->ssr.state = HSI_RTPM_TXRX_PENDING;
			mod_timer(&intel_port->ssr.timer, jiffies + \
				msecs_to_jiffies(HSI_RTPM_RX_DRAIN_DELAY));
			intel_port->ssr.timer_cnt = 0;
			goto out;
		}

		goto to_idle;
	}

	if ((intel_port->ssr.state == HSI_RTPM_TXRX_PENDING) && \
		(intel_port->ssr.timer_cnt++ < HSI_RTPM_TIMER_CNT_MAX) && \
		(!_hsi_rtpm_txrx_is_idle(HSI_LOGICAL_READ, port))) {
		/* remain in RX_PENDING */
		mod_timer(&intel_port->ssr.timer, jiffies + \
			msecs_to_jiffies(HSI_RTPM_RX_DRAIN_DELAY));
		goto out;
	}

to_idle:
	/* RX_ON/RX_PENDING -> RX_IDLE */
	_hsi_rtpm_put_rx_to_idle(port, &flags);
out:
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);
}

/**
 * hsi_reg_config - Configure the controller registers
 * @hsi: hsi_controller struct
 */
static void hsi_reg_config(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	void __iomem *base = intel_hsi->sys;
	void __iomem *dma_base = intel_hsi->gdd;
	int i;

	/* Config FIFO size */
	/* fifo changes need to be done carefully */
	/* first enable channel so it has an allocation */
	/* then set fifo size; this must be done sequentially by channel */
	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		if (tx_fifo_size[i] != 0) {
			set_hsi_program_tx_channel_enable(base, i,
					HSI_PROGRAM_CHANNEL_ENABLED);
			set_hsi_tx_fifo_size(base, i, tx_fifo_size[i]);
			set_hsi_tx_fifo_threshold(base, i, TX_THRESHOLD);
		}
		if (rx_fifo_size[i] != 0) {
			set_hsi_program_rx_channel_enable(base, i,
					HSI_PROGRAM_CHANNEL_ENABLED);
			set_hsi_rx_fifo_size(base, i, rx_fifo_size[i]);
			set_hsi_rx_fifo_threshold(base, i, RX_THRESHOLD);
		}
	}

	/* Config dma (master and slave) */
	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		if (dma_write_channels[i] >= 0) {
			hsi_init_slave_dma_channel(base, HSI_LOGICAL_WRITE,
						dma_write_channels[i], i);
			hsi_init_master_dma_channel(dma_base, HSI_LOGICAL_WRITE,
						dma_write_channels[i], i);
			iowrite32(0, HSI_DWAHB_LLP(dma_base, \
					dma_write_channels[i]));
#ifdef USE_MASTER_DMA_IRQ
			hsi_dwahb_single_masked_write( \
					HSI_DWAHB_MASKTFR(dma_base),
					(1<<dma_write_channels[i]));
			set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base,
						dma_write_channels[i]),
						1, 0, 1);
#else
			/* enable tx dma completion interrupt */
			set_hsi_interrupt_status_enable_transfer_complete(base,
				dma_write_channels[i], HSI_STATUS_ENABLE);
			set_hsi_int_stat_sig_enable_transfer_complete(base,
				dma_write_channels[i], HSI_STATUS_ENABLE);
#endif
		}

		if (dma_read_channels[i] < 0) {
			/* PIO channel -- enable timeout int */
			set_hsi_error_interrupt_status_enable_data_timeout(
				base, i, HSI_STATUS_ENABLE);
			continue;
		} else {
			hsi_init_slave_dma_channel(base, HSI_LOGICAL_READ,
						dma_read_channels[i], i);
			hsi_init_master_dma_channel(dma_base, HSI_LOGICAL_READ,
						dma_read_channels[i], i);
			iowrite32(0,
			      HSI_DWAHB_LLP(dma_base, dma_read_channels[i]));
#ifdef USE_MASTER_DMA_IRQ
			hsi_dwahb_single_masked_write( \
					HSI_DWAHB_MASKTFR(dma_base),
					(1<<dma_read_channels[i]));
			set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base,
						dma_read_channels[i]),
						1, 0, 1);
#else
			/* enable rx dma completion interrupt */
			set_hsi_interrupt_status_enable_transfer_complete(base,
				dma_read_channels[i], HSI_STATUS_ENABLE);
			set_hsi_int_stat_sig_enable_transfer_complete(base,
				dma_read_channels[i], HSI_STATUS_ENABLE);
#endif
		}
	}

	/* now setup hardware */
	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);

		if (intel_port->sst.mode == HSI_MODE_STREAM)
			set_hsi_program_tx_transmit_mode(base,
						HSI_PROGRAM_TX_MODE_STREAM);
		else if (intel_port->sst.mode == HSI_MODE_FRAME)
			set_hsi_program_tx_transmit_mode(base,
						HSI_PROGRAM_TX_MODE_FRAME);
		else
			set_hsi_program_tx_transmit_mode(base,
						HSI_PROGRAM_TX_MODE_FRAME);

		if (intel_port->ssr.mode == HSI_MODE_STREAM)
			set_hsi_program_rx_receive_mode(base,
						HSI_PROGRAM_RX_MODE_STREAM);
		else if (intel_port->ssr.mode == HSI_MODE_FRAME)
			set_hsi_program_rx_receive_mode(base,
						HSI_PROGRAM_RX_MODE_FRAME);
		else
			set_hsi_program_rx_receive_mode(base,
						HSI_PROGRAM_RX_MODE_FRAME);

		set_hsi_program_num_tx_id_bits(base, intel_port->sst.channels);
		set_hsi_program_num_rx_id_bits(base, intel_port->ssr.channels);
		set_hsi_arbiter_priority_policy(base, intel_port->sst.arb_mode);
		set_hsi_program_rx_data_flow(base, intel_port->ssr.flow);

		/* set up and start clock */
		set_hsi_clk_clock_divisor(base, intel_port->sst.divisor);
		set_hsi_clk_internal_enable(base,
					HSI_TX_INTERNAL_CLOCK_ENABLE_OSCILLATE);
	}

	/* set up general error status interrupts */
	set_hsi_error_interrupt_status_enable_rx_break(base,
						HSI_STATUS_ENABLE);
	set_hsi_error_interrupt_status_enable_rx_error(base,
						HSI_STATUS_ENABLE);
}

/**
 * hsi_reg_post_config - Re-enable interrupts after register
 * configuration is completed.
 * @hsi: hsi_controller struct
 */
static void hsi_reg_post_config(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	/* Enable CA_WAKE interrupts */
	set_hsi_interrupt_status_enable_WKUP_interrupt(base,
				HSI_STATUS_ENABLE);
	set_hsi_int_stat_sig_enable_WKUP_interrupt(base, HSI_STATUS_ENABLE);

	/* Restart communications */
	hsi_enable_error_interrupts(hsi);
}

/* hsi_suspend - save HSI controller context before suspending
 * (system or runtime)
 * @intel_hsi: pointer to intel_hsi_controller struct
 */
static void hsi_suspend(struct intel_hsi_controller *intel_hsi)
{
	struct hsi_controller *hsi = to_hsi_controller(intel_hsi->dev);
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	int i;

	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);
#ifdef DEFER_IRQ_CLEARING
		tasklet_disable(&intel_port->pio_tasklet);
#endif
		tasklet_disable(&intel_port->fwd_tasklet);
#ifdef USE_MASTER_DMA_IRQ
		tasklet_disable(&intel_port->dma_tasklet);
#endif
	}
}

/* hsi_resume - Called when resuming from system suspend or runtime suspend
 * @pdev: PCI device
 */
static void hsi_resume(struct intel_hsi_controller *intel_hsi)
{
	struct hsi_controller *hsi = to_hsi_controller(intel_hsi->dev);
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	struct hsi_msg *msg;
	void __iomem *base = intel_hsi->sys;
	unsigned long flags;
	int i;
	int j;

	/* Step 1: disable error interrupts */
	iowrite32(0, ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base));

	/* Step 2: configure device-wide registers */
	set_hsi_ports_default(intel_hsi);

	/* Step 3: enable tasklets (interrupt handlers) */
	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);
#ifdef DEFER_IRQ_CLEARING
		tasklet_enable(&intel_port->pio_tasklet);
#endif
		tasklet_enable(&intel_port->fwd_tasklet);
#ifdef USE_MASTER_DMA_IRQ
		tasklet_enable(&intel_port->dma_tasklet);
#endif
	}

	/* If setup is completed, re-init the device. Otherwise
	 * if setup is not completed yet, hsi_mid_setup() will
	 * handle this later.
	 */
	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);
		if (!hsi_setup_completed(intel_port))
			continue;
		/* Step 4: configure client-specific registers */
		hsi_reg_config(hsi);

		/* Step 5: continue the DMAs pending before suspend */
		for (j = 0; j < HSI_MAX_GDD_LCH; j++) {
			if (intel_hsi->gdd_trn[j] != NULL) {
				msg = intel_hsi->gdd_trn[j];
				hsi_start_dma(msg, j);
			}
		}

		/* Step 6: handle interrupts that are pending before resume */
		spin_lock_irqsave(&intel_hsi->pm_lock, flags);
		if (intel_hsi->delay_interrupt) {
			intel_hsi->delay_interrupt = 0;
			/* recover the runtime pm usage count incremented in
			 * hsi_pio_isr() when setting delay_interrupt
			 */
			pm_runtime_put_noidle(&intel_hsi->pdev->dev);
			spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);
			for (i = 0; i < hsi->num_ports; i++) {
				port = &hsi->port[i];
				intel_port = hsi_port_drvdata(port);
#ifdef DEFER_IRQ_CLEARING
				disable_irq_nosync(intel_port->irq);
				tasklet_hi_schedule(&intel_port->pio_tasklet);
#else
				hsi_pio_tasklet((unsigned long) port);
#endif
			}
			spin_lock_irqsave(&intel_hsi->pm_lock, flags);
		}
		spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);

		/* Step 7: re-enable the interrupts */
		hsi_reg_post_config(hsi);
		break;
	}

	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);
		/* Step 8: activate the state machine based on the flags */
		spin_lock_irqsave(&intel_port->pm_lock, flags);
		if (intel_port->sst.delay_activate) {
			hsi_rtpm_tx_activate(port);
			intel_port->sst.delay_activate = 0;
		}
		if (intel_port->ssr.delay_activate) {
			hsi_rtpm_rx_activate(port);
			intel_port->ssr.delay_activate = 0;
		}
		/* Step 9: transfer data pending before suspend */
		if (intel_port->sst.delay_senddata || \
			intel_port->ssr.delay_senddata) {
			spin_unlock_irqrestore(&intel_port->pm_lock, flags);
			hsi_async_data_transfer(NULL, intel_port);
			spin_lock_irqsave(&intel_port->pm_lock, flags);
			intel_port->sst.delay_senddata = 0;
			intel_port->ssr.delay_senddata = 0;
		}
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
	}
}

#ifdef CONFIG_PM_RUNTIME
/**
 * hsi_rtpm_init - init runtime pm for HSI controller
 * @pdev: HSI pci device
 *
 * Init runtime pm for HSI PCI device, and ignore runtime pm
 * all its children.
 */
static void hsi_rtpm_init(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_allow(dev);

	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, HSI_RTPM_INACTIVITY_DELAY);
	pm_runtime_mark_last_busy(dev);

	pm_suspend_ignore_children(dev, true);

	/* wake up the device for initialization */
	pm_runtime_get_sync(dev);
}

/* hsi_pm_runtime_idle - runtime power management idle callback
 * @dev: HSI controller device (pdev->dev)
 *
 * Return: just return success as it will not be called during the
 * autosuspend process. Instead, it's called only at booting up.
 */
static int hsi_pm_runtime_idle(struct device *dev)
{
	return 0;
}

/* hsi_pm_runtime_suspend - runtime power management suspend callback
 * @dev: HSI controller device (pdev->dev)
 */
static int hsi_pm_runtime_suspend(struct device *dev)
{
	struct intel_hsi_controller *intel_hsi =
		(struct intel_hsi_controller *)dev_get_drvdata(dev);

	/* ACTIVE -> STANDBY */
	if (pm_runtime_suspended(dev))
		return 0;

	hsi_suspend(intel_hsi);
	hsi_set_ctrl_pmstate(intel_hsi, HSI_RTPM_D0I3);
	return 0;
}

/* hsi_pm_runtime_resume - runtime power management resume callback
 * @dev: HSI controller device (pdev->dev)
 */
static int hsi_pm_runtime_resume(struct device *dev)
{
	struct intel_hsi_controller *intel_hsi =
		(struct intel_hsi_controller *)dev_get_drvdata(dev);

	/* STANDBY -> ACTIVE */
	hsi_set_ctrl_pmstate(intel_hsi, HSI_RTPM_ACTIVE);
	hsi_resume(intel_hsi);

	return 0;
}
#else /* CONFIG_PM_RUNTIME */
static inline void hsi_rtpm_init(struct pci_dev *pdev) {}
#define hsi_pm_runtime_idle NULL
#define hsi_pm_runtime_suspend NULL
#define hsi_pm_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_SUSPEND
/* hsi_pm_prepare - power management prepare callback
 * @dev: HSI controller device (pdev->dev)
 *
 * Block the system suspend if there is still data pending.
 * Increment usage count so that runtime power management core
 * cannot put the device into D0i3 until system resume is completed.
 * Return: just return success.
 */
static int hsi_pm_prepare(struct device *dev)
{
	struct intel_hsi_controller *intel_hsi =
		(struct intel_hsi_controller *)dev_get_drvdata(dev);
	struct hsi_controller *hsi = to_hsi_controller(intel_hsi->dev);
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	unsigned int i;
	unsigned long flags;

	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = hsi_port_drvdata(port);
		spin_lock_irqsave(&intel_port->pm_lock, flags);
		if ((intel_port->sst.state != HSI_RTPM_TXRX_IDLE) || \
				(intel_port->ssr.state != HSI_RTPM_TXRX_IDLE)) {
			wait_event_interruptible(intel_hsi->d3_delay_wq, \
					((intel_port->sst.state == \
					HSI_RTPM_TXRX_IDLE) || \
					(intel_port->ssr.state == \
					HSI_RTPM_TXRX_IDLE)));
			spin_unlock_irqrestore(&intel_port->pm_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
	}

	pm_runtime_get_noresume(&intel_hsi->pdev->dev);
	return 0;
}

/* hsi_pm_complete - power management complete callback
 * @dev: HSI controller device (pdev->dev)
 *
 * Decrement usage count that was incremented before D3 suspending
 * for preventing D0i3 suspend during D3 suspend/resume.
 */
static void hsi_pm_complete(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}

/**
 * hsi_pm_suspend - Called at system suspend request
 * @dev: hsi controller device
 *
 * Suspend HSI controller (system suspend to RAM)
 */
static int hsi_pm_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct intel_hsi_controller *intel_hsi =
		(struct intel_hsi_controller *)dev_get_drvdata(&pdev->dev);

	hsi_suspend(intel_hsi);
	hsi_set_ctrl_pmstate(intel_hsi, HSI_RTPM_D3);

	return 0;
}

/**
 * hsi_pm_resume - Called at system resume request
 * @dev: hsi controller device
 *
 * Resume HSI controller (system resume from RAM)
 */
static int hsi_pm_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct intel_hsi_controller *intel_hsi =
		(struct intel_hsi_controller *)dev_get_drvdata(&pdev->dev);

	hsi_resume(intel_hsi);
	hsi_set_ctrl_pmstate(intel_hsi, HSI_RTPM_ACTIVE);

	return 0;
}

#else /* CONFIG_SUSPEND */
#define hsi_pm_prepare NULL
#define hsi_pm_complete NULL
#define hsi_pm_suspend NULL
#define hsi_pm_resume  NULL
#endif /* CONFIG_SUSPEND */

/**
 * hsi_get_dma_channel - Return DMA channel for HSI channel read/write
 * @read_write: HSI_LOGICAL_READ or HSI_LOGICAL_WRITE
 * @hsi_channel: HSI channel for which to find DMA channel
 *
 * Return the DMA channel associated with an HSI channel for read or write.
 * Return -1 in case there is no associated DMA channel.
 */
static int hsi_get_dma_channel(u8 read_write, u32 hsi_channel)
{
	if (hsi_channel > HSI_MAX_LOGICAL_CHANNEL)
		return -1;

	return (read_write == HSI_LOGICAL_READ) ?
		dma_read_channels[hsi_channel] :
		dma_write_channels[hsi_channel];
}

/**
 * hsi_enable_tx_clock - Start Tx clock
 * @base: base address of HSI controller
 *
 * If clock is not enabled already, start it and wait for it to stabilize,
 * then enable it.
 *
 * Return 0 if successful, -ETIME if clock won't stabilize.
 */
static int hsi_enable_tx_clock(void __iomem *base)
{
	unsigned int clock_wait = 0;

	if (!get_hsi_clk_tx_enable(base)) {
		set_hsi_clk_internal_enable(base,
			 HSI_TX_INTERNAL_CLOCK_ENABLE_OSCILLATE);
		/* wait here until clock comes up -- keeps from waiting
		in tasklet for this */
		while (!get_hsi_clk_stable(base) &&
			clock_wait < HSI_MAX_TX_CLOCK_WAIT) {
			udelay(1);
			clock_wait++;
		}

		if (clock_wait >= HSI_MAX_TX_CLOCK_WAIT) {
			/* clock won't come up -- signal error */
			pr_debug("tx clock won't start\n");
			return -ETIME;
		}

		set_hsi_clk_tx_enable(base, HSI_TX_CLOCK_ENABLE);
	}
	return 0;
}

/* debug fs set up */
#ifdef CONFIG_DEBUG_FS
static int hsi_debug_show(struct seq_file *m, void *p)
{
	struct hsi_controller *hsi = m->private;
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	seq_printf(m, "REVISION\t: 0x%08x\n",
		ioread32(ARASAN_HSI_VERSION(base)));

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

static int hsi_debug_port_show(struct seq_file *m, void *p)
{
	struct hsi_port *port = m->private;
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	int ch;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	for (ch = 0; ch < HSI_MAX_GDD_LCH; ch++) {
		seq_printf(m, "DMA CONFIG %d\t\t: 0x%08x\n", ch,
			ioread32(ARASAN_HSI_DMA_CONFIG(base, ch)));
	}
	seq_printf(m, "TXFIFO CTL1\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_DMA_TX_FIFO_SIZE(base)));
	seq_printf(m, "TXFIFO CTL2\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_DMA_TX_FIFO_THRESHOLD(base)));
	seq_printf(m, "RXFIFO CTL1\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_DMA_RX_FIFO_SIZE(base)));
	seq_printf(m, "RXFIFO CTL2\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_DMA_RX_FIFO_THRESHOLD(base)));
	seq_printf(m, "CLOCK CONTROL\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_CLOCK_CONTROL(base)));
	seq_printf(m, "STATUS\t\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_HSI_STATUS(base)));
	seq_printf(m, "STATUS1\t\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_HSI_STATUS1(base)));
	seq_printf(m, "PROGRAM\t\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_PROGRAM(base)));
	seq_printf(m, "PROGRAM1\t\t: 0x%08x\n",
		ioread32(ARASAN_HSI_PROGRAM1(base)));
	seq_printf(m, "INTERRUPT STATUS\t: 0x%08x\n",
		ioread32(ARASAN_HSI_INTERRUPT_STATUS(base)));
	seq_printf(m, "INTERRUPT STATUS ENABLE\t: 0x%08x\n",
		ioread32(ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base)));
	seq_printf(m, "INTERRUPT SIGNAL ENABLE\t: 0x%08x\n",
		ioread32(ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base)));
	seq_printf(m, "ARBITER PRIORITY\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ARBITER_PRIORITY(base)));
	seq_printf(m, "ARBITER BANDWIDTH1\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ARBITER_BANDWIDTH1(base)));
	seq_printf(m, "ARBITER BANDWIDTH2\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ARBITER_BANDWIDTH2(base)));
	seq_printf(m, "ERROR INT STATUS\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ERROR_INTERRUPT_STATUS(base)));
	seq_printf(m, "ERROR INT STATUS ENABLE\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base)));
	seq_printf(m, "ERROR INT SIGNAL ENABLE\t: 0x%08x\n",
		ioread32(ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base)));

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

#define HSI_DEBUG_GDD_PRINT(F) \
	seq_printf(m, #F "\t\t: 0x%08x\n", ioread32(HSI_DWAHB_ ## F(dma_base)))
#define HSI_DEBUG_GDD_PRINT2(F, i) \
	seq_printf(m, #F " %d\t\t: 0x%08x\n", i,\
		   ioread32(HSI_DWAHB_ ## F(dma_base, i)))

static int hsi_debug_gdd_show(struct seq_file *m, void *p)
{
	struct hsi_controller *hsi = m->private;
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *dma_base = intel_hsi->gdd;
	int i;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	for (i = 0; i < HSI_MAX_GDD_LCH; i++) {
		HSI_DEBUG_GDD_PRINT2(SAR, i);
		HSI_DEBUG_GDD_PRINT2(DAR, i);
		HSI_DEBUG_GDD_PRINT2(CTL_LO, i);
		HSI_DEBUG_GDD_PRINT2(CTL_HI, i);
		HSI_DEBUG_GDD_PRINT2(CFG_LO, i);
		HSI_DEBUG_GDD_PRINT2(CFG_HI, i);
	}

	HSI_DEBUG_GDD_PRINT(DMACFG);
	HSI_DEBUG_GDD_PRINT(CHEN);
	HSI_DEBUG_GDD_PRINT(STATUSINT);

	HSI_DEBUG_GDD_PRINT(STATUSTFR);
	HSI_DEBUG_GDD_PRINT(STATUSBLOCK);
	HSI_DEBUG_GDD_PRINT(STATUSSRCTRAN);
	HSI_DEBUG_GDD_PRINT(STATUSDSTTRAN);
	HSI_DEBUG_GDD_PRINT(STATUSERR);

	HSI_DEBUG_GDD_PRINT(MASKTFR);
	HSI_DEBUG_GDD_PRINT(MASKBLOCK);
	HSI_DEBUG_GDD_PRINT(MASKSRCTRAN);
	HSI_DEBUG_GDD_PRINT(MASKDSTTRAN);
	HSI_DEBUG_GDD_PRINT(MASKERR);

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

static int hsi_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, hsi_debug_show, inode->i_private);
}

static int hsi_port_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, hsi_debug_port_show, inode->i_private);
}

static int hsi_gdd_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, hsi_debug_gdd_show, inode->i_private);
}

static const struct file_operations hsi_regs_fops = {
	.open		= hsi_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations hsi_port_regs_fops = {
	.open		= hsi_port_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations hsi_gdd_regs_fops = {
	.open		= hsi_gdd_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __devinit hsi_debug_add_port(struct device *dev, void *data)
{
	struct hsi_port *port = to_hsi_port(dev);
	struct dentry *dir = data;

	dir = debugfs_create_dir(dev_name(dev), dir);
	if (IS_ERR(dir))
		return PTR_ERR(dir);
	debugfs_create_file("regs", S_IRUGO, dir, port, &hsi_port_regs_fops);

	return 0;
}

static int __devinit hsi_debug_add_ctrl(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct dentry *dir;
	int err;

	/* HSI controller */
	intel_hsi->dir = debugfs_create_dir(dev_name(&hsi->device), NULL);
	if (IS_ERR(intel_hsi->dir))
		return PTR_ERR(intel_hsi->dir);
	debugfs_create_file("regs", S_IRUGO, intel_hsi->dir, hsi,
							&hsi_regs_fops);

	/* HSI GDD (DMA) */
	dir = debugfs_create_dir("dma", intel_hsi->dir);
	if (IS_ERR(dir))
		goto rback;
	debugfs_create_file("regs", S_IRUGO, dir, hsi, &hsi_gdd_regs_fops);

	/* HSI ports */
	err = device_for_each_child(&hsi->device, intel_hsi->dir,
							hsi_debug_add_port);
	if (err < 0)
		goto rback;

	return 0;
rback:
	debugfs_remove_recursive(intel_hsi->dir);

	return PTR_ERR(dir);
}
#endif /* CONFIG_DEBUG_FS */

/**
 * hsi_start_pio - Start PIO data transfer
 * @msg: Pointer to message to transfer
 *
 * Set up controller registers to begin PIO operation.
 *
 * Return 0 if successful, error if Tx clock can't be enabled.
 */
static int hsi_start_pio(struct hsi_msg *msg)
{
	struct hsi_port *port = hsi_get_port(msg->cl);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	int channel = msg->channel;

	msg->actual_len = 0;

	if (msg->ttype == HSI_MSG_WRITE) {
		/* enable threshold reached signal, status, interrupt
			-- this should generate tx interrupt */
		set_hsi_int_stat_sig_enable_tx_threshold_reached(
			base,
			channel, HSI_STATUS_ENABLE);
		set_hsi_interrupt_status_enable_tx_threshold_reached(base,
			channel, HSI_STATUS_ENABLE);
	} else {
		set_hsi_int_stat_sig_enable_rx_threshold_reached(
			base,
			channel, HSI_STATUS_ENABLE);
		set_hsi_interrupt_status_enable_rx_threshold_reached(base,
			channel, HSI_STATUS_ENABLE);
	}
	return 0;
}

/**
 * hsi_start_dma - Start DMA data transfer
 * @msg: Pointer to message to transfer
 * @lch: DMA channel to use for transfer
 *
 * Set up controller registers to begin DMA operation.
 *
 * Return 0 if successful, error if Tx clock can't be enabled or DMA channel
 * can't be claimed.
 */
static int hsi_start_dma(struct hsi_msg *msg, int lch)
{
	struct hsi_port *port = hsi_get_port(msg->cl);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *dma_base = intel_hsi->gdd;
	void __iomem *base = intel_hsi->sys;
	dma_addr_t s_addr;
	dma_addr_t d_addr;
	u32 size;
	u32 mask = (1<<lch);
	u32 sdma_dir;

	intel_hsi->gdd_trn[lch] = msg;
	size = HSI_BYTES_TO_FRAMES(msg->sgt.sgl->length);

	if (msg->ttype == HSI_MSG_READ) {
		s_addr = (dma_addr_t) HSI_DWAHB_RX_ADDRESS(msg->channel);
		d_addr = sg_dma_address(msg->sgt.sgl);
		sdma_dir = 1;
	} else {
		s_addr = sg_dma_address(msg->sgt.sgl);
		d_addr = (dma_addr_t) HSI_DWAHB_TX_ADDRESS(msg->channel);
		sdma_dir = 0;
	}

	/* Clear 'done' bit and set the transfer size */
	iowrite32(size, HSI_DWAHB_CTL_HI(dma_base, lch));

	/* Set up transfer addresses */
	iowrite32(s_addr, HSI_DWAHB_SAR(dma_base, lch));
	iowrite32(d_addr, HSI_DWAHB_DAR(dma_base, lch));

	/*
	 * GO !
	 * First enable master DMA then slave to start transfers;
	 */
	hsi_dwahb_single_masked_write(HSI_DWAHB_CHEN(dma_base), mask);
	iowrite32(sdma_dir|(msg->channel<<1)|(size<<4)|(0x3<<24)|(1<<31),
		      ARASAN_HSI_DMA_CONFIG(base, lch));

	return 0;
}

/**
 * hsi_start_transfer - Start data transfer from read/write queue
 * @intel_port: intel_hsi_port structure pointer
 * @queue: Pointer to message queue
 *
 * If a message is ready to transfer, perform DMA or PIO transfer,
 * depending on DMA channel maps and message DMA/PIO flag.
 *
 * Return 0 if no messages, message at head of list is already being
 * transferred, or DMA/PIO transfer start is successful.
 *
 * Note this is called with HSI port bh_lock acquired.
 */
static int hsi_start_transfer(struct intel_hsi_port *intel_port,
			      struct list_head *queue)
{
	struct hsi_msg	*msg;
	u8		dir;
	int		err;
	int lch = -1;

	if (list_empty(queue))
		return 0;

	msg = list_first_entry(queue, struct hsi_msg, link);
	if (msg->status != HSI_STATUS_QUEUED)
		return 0;

	msg->status = HSI_STATUS_PROCEEDING;

	if (msg->sgt.nents) {
		dir = (msg->ttype == HSI_MSG_READ) ? \
		      HSI_LOGICAL_READ : HSI_LOGICAL_WRITE;
		lch = hsi_get_dma_channel(dir, msg->channel);

		if (lch < 0)
			err = hsi_start_pio(msg);
		else
			err = hsi_start_dma(msg, lch);
	} else {
		err = hsi_start_pio(msg);
	}

	return err;
}

/**
 * hsi_transfer - Start data transfer from read/write queue
 * @intel_port: Pointer to intel_hsi_port for transfer
 * @queue: Pointer to message queue
 *
 * Lock port and start transfer; if there is an error, signal with call to
 * message completion routine.
 */
static void hsi_transfer(struct intel_hsi_port *intel_port,
					struct list_head *queue)
{
	struct hsi_msg *msg;
	int err = -1;
	unsigned long flags;

	spin_lock_irqsave(&intel_port->lock, flags);
	while (err < 0) {
		err = hsi_start_transfer(intel_port, queue);
		if (err < 0) {
			msg = list_first_entry(queue, struct hsi_msg, link);
			msg->status = HSI_STATUS_ERROR;
			msg->actual_len = 0;
			list_del(&msg->link);
			spin_unlock_irqrestore(&intel_port->lock, flags);
			msg->complete(msg);
			spin_lock_irqsave(&intel_port->lock, flags);
		}
	}
	spin_unlock_irqrestore(&intel_port->lock, flags);
}

/**
 * hsi_clear_channel_receive_ints - Clear interrupt status for receive
 * @ch: HSI channel to clear
 * @base: Base address of HSI controller
 *
 * Clear receive threashold enable and signal enable, clear Rx threashold
 * status.
 */
static void hsi_clear_channel_receive_ints(unsigned int ch, void __iomem *base)
{
	set_hsi_interrupt_status_enable_rx_threshold_reached(base, ch, 0);
	set_hsi_int_stat_sig_enable_rx_threshold_reached(base,
							ch, 0);
	clear_hsi_interrupt_status_rx_threshold_reached(base, ch);
}

/**
 * hsi_break_complete - Handle break interrupt
 * @port: HSI port
 *
 * Handle HSI break -- clear condition and run message completion on
 * any messages in the break queue.
 */
static void hsi_break_complete(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	struct hsi_msg *msg;
	struct hsi_msg *tmp;
	unsigned long flags;

	dev_dbg(&port->device, "HWBREAK received\n");

	spin_lock_irqsave(&intel_port->lock, flags);
	clear_hsi_error_interrupt_status_rx_break(base);
	spin_unlock_irqrestore(&intel_port->lock, flags);

	list_for_each_entry_safe(msg, tmp, &intel_port->rx_brkqueue, link) {
		msg->status = HSI_STATUS_COMPLETED;
		spin_lock_irqsave(&intel_port->lock, flags);
		list_del(&msg->link);
		spin_unlock_irqrestore(&intel_port->lock, flags);
		msg->complete(msg);
	}
}

/**
 * hsi_error - Handle an error interrupt
 * @port: HSI port
 *
 * Handle HSI break, receive errors, and data timeout errors.
 */
static void hsi_error(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	struct list_head *queue;
	struct hsi_msg *msg = NULL;
	unsigned int i;
	u32 val = 0;
	u32 *buf;
	u32 error_reg = ioread32(ARASAN_HSI_ERROR_INTERRUPT_STATUS(base));
	u32 timeout_mask;
	unsigned long flags;

	/* handle break */
	if (error_reg & HSI_ERROR_BREAK_DETECTED)
		hsi_break_complete(port);

	/* Signal receive error for all current pending read requests */
	/* first clear rx interrupt enable and status */
	if (error_reg & HSI_ERROR_RX_ERROR) {
		clear_hsi_error_interrupt_status_rx_error(base);
		for (i = 0; i < intel_port->channels; i++) {
			hsi_clear_channel_receive_ints(i, base);
			spin_lock_irqsave(&intel_port->lock, flags);
			if (list_empty(&intel_port->rxqueue[i])) {
				spin_unlock_irqrestore(&intel_port->lock,
						       flags);
				continue;
			}
			msg = list_first_entry(&intel_port->rxqueue[i],
					struct hsi_msg, link);
			msg->status = HSI_STATUS_ERROR;
			/* complete msg if this is serviced by PIO */
			if (dma_read_channels[i] == -1) {
				list_del(&msg->link);
				spin_unlock_irqrestore(&intel_port->lock,
						flags);
				msg->complete(msg);
				/* Now restart queued reads if any */
				hsi_transfer(intel_port,
						&intel_port->rxqueue[i]);
			} else
				spin_unlock_irqrestore(&intel_port->lock,
						flags);
		}
	}

	/* handle data timeout errors */
	/* ch0 timeout is bit 2 of int status reg */
	timeout_mask = HSI_ERROR_TIMEOUT_MASK;
	for (i = 0; i < intel_port->channels; i++, timeout_mask <<= 1) {
		if (!(error_reg & timeout_mask))
			continue;
		spin_lock_irqsave(&intel_port->lock, flags);
		queue = &intel_port->rxqueue[i];
		/* if no msg waiting for read, throw data away */
		if (list_empty(&intel_port->rxqueue[i])) {
			while (get_hsi_status_rx_fifo_not_empty(base, i)) {
				val = ioread32(ARASAN_HSI_RX_DATA(
							base, i));
			}
			clear_hsi_error_interrupt_status_data_timeout(base, i);
			spin_unlock_irqrestore(&intel_port->lock, flags);
			continue;
		}
		msg = list_first_entry(queue, struct hsi_msg, link);
		if ((!msg->sgt.nents) || (!msg->sgt.sgl->length)) {
			msg->actual_len = 0;
			msg->status = HSI_STATUS_PENDING;
		}
		if (msg->status == HSI_STATUS_PROCEEDING) {
			buf = sg_virt(msg->sgt.sgl) + msg->actual_len;
			while (get_hsi_status_rx_fifo_not_empty(
				base, msg->channel) &&
				(msg->actual_len < msg->sgt.sgl->length)) {
				*buf = ioread32(ARASAN_HSI_RX_DATA(base,
							msg->channel));
				val = *buf;
				msg->actual_len += sizeof(*buf);
				buf++;
			}
			if (msg->actual_len >= msg->sgt.sgl->length)
				msg->status = HSI_STATUS_COMPLETED;
			if (msg->status == HSI_STATUS_PROCEEDING) {
				clear_hsi_error_interrupt_status_data_timeout(
					base, i);
				spin_unlock_irqrestore(&intel_port->lock,
						       flags);
				continue;
			}
		}

		/* Transfer completed at this point */
		list_del(&msg->link);
		spin_unlock_irqrestore(&intel_port->lock, flags);
		msg->complete(msg);
		clear_hsi_error_interrupt_status_data_timeout(base, i);
	}

	/* clear error */
	clear_hsi_interrupt_error_interrupt(base);
}

/**
 * hsi_send_tx_break - Send tx break message to the modem
 * @msg: HSI message
 *
 * Send tx break message.
 */
static void hsi_send_tx_break(struct hsi_msg *msg)
{
	struct hsi_port *port = hsi_get_port(msg->cl);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	/* break will be about HSI_BREAK_DELAYus sending 0s */
	set_hsi_clk_tx_break(base, 1);
	udelay(HSI_BREAK_DELAY);
	set_hsi_clk_tx_break(base, 0);
	msg->status = HSI_STATUS_COMPLETED;
	msg->complete(msg);
}

/**
 * hsi_async_data_transfer - Transfer the massage passed in, or start
 * to transfer tx data/rx data/tx break for intel_port.
 * @msg: HSI message
 * @intel_port: intel_hsi_port structure
 *
 * Asynchronously transfer data for tx break, tx data or rx data. May
 * be called synchronously when the device is active, or asynchronously
 * in the resume routine when the device resumes back.
 */
static void hsi_async_data_transfer(struct hsi_msg *msg,
		struct intel_hsi_port *intel_port)
{
	struct hsi_controller *hsi = to_hsi_controller(intel_port->dev->parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct hsi_msg *tmpmsg;
	struct hsi_msg *tmp;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&intel_port->lock, flags);
	if (msg) {
		/* when msg!=NULL, this function is called synchronously */
		if (msg->break_frame) {
			if ((msg->ttype == HSI_MSG_WRITE) && \
				(intel_port->sst.mode == HSI_MODE_FRAME)) {
				list_del(&msg->link);
				spin_unlock_irqrestore( \
						&intel_port->lock, flags);
				hsi_send_tx_break(msg);
				spin_lock_irqsave(&intel_port->lock, flags);
			}
		} else if (msg->ttype == HSI_MSG_WRITE) {
			(void) hsi_start_transfer(intel_port, \
					&intel_port->txqueue[msg->channel]);
		} else { /* (msg->ttype == HSI_MSG_READ) */
			(void) hsi_start_transfer(intel_port, \
					&intel_port->rxqueue[msg->channel]);
		}
		goto out;
	}

	/* When msg==NULL, this function is called asynchronously from
	 * resume routine.
	 */
	list_for_each_entry_safe(tmpmsg, tmp, &intel_port->tx_brkqueue, link) {
		list_del(&tmpmsg->link);
		spin_unlock_irqrestore(&intel_port->lock, flags);
		hsi_send_tx_break(tmpmsg);
		spin_lock_irqsave(&intel_port->lock, flags);
	}

	for (i = 0; i < intel_port->channels; i++) {
		(void) hsi_start_transfer(intel_port, &intel_port->txqueue[i]);
		(void) hsi_start_transfer(intel_port, &intel_port->rxqueue[i]);
	}

out:
	spin_unlock_irqrestore(&intel_port->lock, flags);

	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);
}
/**
 * hsi_mid_async - Queue read/write/break message and start transfer
 * @msg: HSI message
 *
 * Queue message to send when possible.
 *
 * Returns 0 if successful, -EINVAL if message pointer is NULL or channel
 * number is invalid, -ENOSYS if SG list has more than one element.  Returns
 * transfer error if any.
 */
static int hsi_mid_async(struct hsi_msg *msg)
{
	struct hsi_port *port = hsi_get_port(msg->cl);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct list_head *queue;
	int err;
	unsigned long flags;

	err = 0;

	if (!msg)
		return -EINVAL;

	if (msg->sgt.nents > 1)
		return -ENOSYS;

	if (msg->break_frame) {
		spin_lock_irqsave(&intel_port->lock, flags);
		msg->status = HSI_STATUS_PROCEEDING;
		queue = (msg->ttype == HSI_MSG_WRITE) ? \
				&intel_port->tx_brkqueue : \
				&intel_port->rx_brkqueue;
		list_add_tail(&msg->link, queue);
		spin_unlock_irqrestore(&intel_port->lock, flags);
		goto do_get;
	}

	/* note ssx.channels is bit field coding of number of channels */
	if (msg->ttype == HSI_MSG_WRITE) {
		if (msg->channel >= (1 << intel_port->sst.channels)) {
			err = -EINVAL;
			goto out;
		}
		queue = &intel_port->txqueue[msg->channel];

		if (hsi_get_dma_channel(HSI_LOGICAL_WRITE,
			msg->channel) >= 0) {
			err = dma_map_sg(hsi->device.parent,
					msg->sgt.sgl, msg->sgt.nents,
					DMA_TO_DEVICE);
			if (err < 0)
				goto out;
		}
	} else {
		if (msg->channel >= (1 << intel_port->ssr.channels)) {
			err = -EINVAL;
			goto out;
		}
		queue = &intel_port->rxqueue[msg->channel];

		if (hsi_get_dma_channel(HSI_LOGICAL_READ,
			msg->channel) >= 0) {
			err = dma_map_sg(hsi->device.parent,
					msg->sgt.sgl, msg->sgt.nents,
					DMA_FROM_DEVICE);
			if (err < 0)
				goto out;
		}
	}

	BUG_ON(!queue);
	msg->status = HSI_STATUS_QUEUED;

	spin_lock_irqsave(&intel_port->lock, flags);
	list_add_tail(&msg->link, queue);
	spin_unlock_irqrestore(&intel_port->lock, flags);

do_get:
	if (hsi_ctrl_is_state(intel_hsi, HSI_RTPM_D0I3)) {
		/* delay the tx/rx activation to resume callback */
		spin_lock_irqsave(&intel_port->pm_lock, flags);
		if ((intel_port->sst.delay_senddata == 0) && \
			(intel_port->ssr.delay_senddata == 0)) {
			if (msg->ttype == HSI_MSG_WRITE)
				intel_port->sst.delay_senddata = 1;
			else
				intel_port->ssr.delay_senddata = 1;
			spin_unlock_irqrestore(&intel_port->pm_lock, flags);
			pm_runtime_get(&intel_hsi->pdev->dev);
			spin_lock_irqsave(&intel_port->pm_lock, flags);
		}
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
	} else {
		pm_runtime_get_noresume(&intel_hsi->pdev->dev);
		/* activate rx immediately if device is active */
		hsi_async_data_transfer(msg, intel_port);
	}
out:
	if (err > 0)
		err = 0;
	return err;
}

/**
 * _hsi_flush_queue - Flush message queue
 * @queue: Message queue to flush
 * @cl: HSI client
 *
 * For each message in client queue, remove and run destructor if any.
 * Note this is called with spin_lock_bh locked.
 */
static void _hsi_flush_queue(struct list_head *queue, struct hsi_client *cl,
			     struct hsi_port *port, unsigned long *flags)
__releases(intel_port->lock)
__acquires(intel_port->lock)
{
	u8 dir, hsi_dir;
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct list_head *node;
	struct hsi_msg *msg;

	do {
		node = queue->next;
		while (node != queue) {
			msg = list_entry(node, struct hsi_msg, link);
			BUG_ON(!msg);
			if ((unlikely(!cl)) || (cl == msg->cl)) {
				list_del(node);
				spin_unlock_irqrestore(&intel_port->lock,
						       *flags);
				dir = (msg->ttype == HSI_MSG_READ) ? \
				      DMA_FROM_DEVICE : DMA_TO_DEVICE;
				hsi_dir = (msg->ttype == HSI_MSG_READ) ? \
				      HSI_LOGICAL_READ : HSI_LOGICAL_WRITE;
				if (hsi_get_dma_channel(hsi_dir,
					msg->channel) >= 0) {
					dma_unmap_sg(hsi->device.parent,
						     msg->sgt.sgl,
						     msg->sgt.nents, dir);
				}
				if (msg->destructor)
					msg->destructor(msg);
				else
					hsi_free_msg(msg);
				spin_lock_irqsave(&intel_port->lock, *flags);
				break;
			}
			node = node->next;
		}
	} while (node != queue);
}

/**
 * translate_fifo_size - Find number of bits to use to program FIFO size
 * @size: Size of FIFO required in DWORDS
 *
 * Return number of bits needed to program HSI controller for FIFO size.
 * Maxes out at max allowable FIFO size.
 */
static int translate_fifo_size(s32 size)
{
	int idx;
	int idx_max = 0;

	/* find highest bit set, up to max allowable */
	/* that's power of 2 for required fifo size */
	for (idx = 0; idx <= HSI_MAX_FIFO_BITS; idx++) {
		if (size & 1)
			idx_max = idx;
		size >>= 1;
	}

	return idx_max;
}

/**
 * hsi_mid_setup - Set up controller from client values
 * @cl: HSI client
 *
 * Translate between external representations of parameters and what
 * Arasan controller uses.
 *
 * Side effects enable channels and set their FIFO sizes, set port
 * activity states to Tx and Rx WAKE
 *
 * Return 0.
 */
static int hsi_mid_setup(struct hsi_client *cl)
{
	struct hsi_port *port = to_hsi_port(cl->device.parent);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct hsi_mid_platform_data *pd;
	void __iomem *base = intel_hsi->sys;
	unsigned int ip_freq = 200000;	/* in kHz */
	u32 chan;
	u32 i;
	u32 idx;
	int err = 0;
	unsigned long flags;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	spin_lock_irqsave(&intel_port->lock, flags);

	/* prepare to change interrupt status values for channels */
	iowrite32(0, ARASAN_HSI_ERROR_INTERRUPT_STATUS_ENABLE(base));

	/* Cleanup the break queue if we leave FRAME mode */
	if ((intel_port->sst.mode == HSI_MODE_FRAME) &&
		(cl->tx_cfg.mode != HSI_MODE_FRAME)) {
		_hsi_flush_queue(&intel_port->tx_brkqueue, cl, port, &flags);
	}
	if ((intel_port->ssr.mode == HSI_MODE_FRAME) &&
		(cl->rx_cfg.mode != HSI_MODE_FRAME)) {
		_hsi_flush_queue(&intel_port->rx_brkqueue, cl, port, &flags);
	}
	spin_unlock_irqrestore(&intel_port->lock, flags);

	/* get our platform data -- put there in hsi_new_client */
	pd = (struct hsi_mid_platform_data *)(cl->device.platform_data);
	if (pd == NULL) {
		dev_dbg(&port->device, "platform data not found\n");
		err = -1;
		goto out;
	}

	intel_port->channels = max(cl->rx_cfg.channels, cl->tx_cfg.channels);
	if (intel_port->channels == 1)
		chan = 0;
	else if (intel_port->channels <= 2)
		chan = 1;
	else if (intel_port->channels <= 4)
		chan = 2;
	else
		chan = 3;

	/* transmit */
	if (cl->tx_cfg.speed <= 0)
		cl->tx_cfg.speed = 1;
	if (cl->tx_cfg.speed > ip_freq/2) {
		intel_port->sst.divisor = 0;
	} else {
		intel_port->sst.divisor =
			rounddown_pow_of_two(ip_freq/(2*cl->tx_cfg.speed));
		if (intel_port->sst.divisor > 0x80)
			intel_port->sst.divisor = 0x80;
	}

	intel_port->sst.channels = chan;
	intel_port->sst.arb_mode = cl->tx_cfg.arb_mode;
	intel_port->sst.mode = cl->tx_cfg.mode;
	intel_port->sst.threshold = TX_THRESHOLD;

	/* save tx FIFO size */
	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		if ((i < intel_port->channels) &&
		    (pd->tx_fifo_sizes[i] > 0)) {
			idx = translate_fifo_size(pd->tx_fifo_sizes[i]);
			tx_fifo_size[i] = idx;
		} else {
			tx_fifo_size[i] = 0;
		}
	}

	/* save dma write channels */
	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		dma_write_channels[i] =
			(i < intel_port->channels) ? \
				pd->tx_dma_channels[i] :
				-1;
	}

	/* receive */
	intel_port->ssr.timeout = 0;
	intel_port->ssr.channels = chan;
	intel_port->ssr.mode = cl->rx_cfg.mode;
	intel_port->ssr.flow = cl->rx_cfg.flow;
	intel_port->ssr.threshold = RX_THRESHOLD;

	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		if ((i < intel_port->channels) &&
		    (pd->rx_fifo_sizes[i] > 0)) {
			idx = translate_fifo_size(pd->rx_fifo_sizes[i]);
			rx_fifo_size[i] = idx;
		} else {
			rx_fifo_size[i] = 0;
		}
	}

	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		dma_read_channels[i] =
			(i < intel_port->channels) ? \
				pd->rx_dma_channels[i] :
				-1;
	}

	/* configure registers */
	hsi_reg_config(hsi);

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	intel_port->setup_completed = 1;
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	/* re-enable the interrupts */
	hsi_reg_post_config(hsi);
out:
	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return err;
}

/**
 * hsi_cleanup_queues - Flush all client queues
 * @cl: HSI client
 *
 * Flush client tx, rx, and break queues.
 * This acquires the port bh lock.
 */
static void hsi_cleanup_queues(struct hsi_client *cl)
{
	struct hsi_port *port = hsi_get_port(cl);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	struct hsi_msg *msg;
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&intel_port->lock, flags);
	_hsi_flush_queue(&intel_port->tx_brkqueue, cl, port, &flags);
	_hsi_flush_queue(&intel_port->rx_brkqueue, cl, port, &flags);
	_hsi_flush_queue(&intel_port->fwdqueue, cl, port, &flags);

	for (i = 0; i < intel_port->channels; i++) {
		if (list_empty(&intel_port->txqueue[i]))
			continue;
		msg = list_first_entry(&intel_port->txqueue[i], struct hsi_msg,
									link);
		if ((msg) && (msg->cl == cl) &&
			(msg->status == HSI_STATUS_PROCEEDING)) {
			set_hsi_int_stat_sig_enable_tx_threshold_reached(
				base, i, HSI_STATUS_DISABLE);
			set_hsi_interrupt_status_enable_tx_threshold_reached(
				base,
				i, HSI_STATUS_DISABLE);
			/*set_hsi_program_tx_channel_enable(base,
				i, HSI_PROGRAM_CHANNEL_DISABLED);*/
		}
		_hsi_flush_queue(&intel_port->txqueue[i], cl, port, &flags);
	}

	for (i = 0; i < intel_port->channels; i++) {
		if (list_empty(&intel_port->rxqueue[i]))
			continue;

		msg = list_first_entry(&intel_port->rxqueue[i], struct hsi_msg,
									link);
		if ((msg) && (msg->cl == cl) &&
			(msg->status == HSI_STATUS_PROCEEDING)) {
			set_hsi_int_stat_sig_enable_rx_threshold_reached(
				base, i, HSI_STATUS_DISABLE);
			set_hsi_interrupt_status_enable_rx_threshold_reached(
				base,
				i, HSI_STATUS_DISABLE);
			/*set_hsi_program_rx_channel_enable(base,
				i, HSI_PROGRAM_CHANNEL_DISABLED);*/
		}
		_hsi_flush_queue(&intel_port->rxqueue[i], cl, port, &flags);
	}
	spin_unlock_irqrestore(&intel_port->lock, flags);
}

/**
 * hsi_disable_dma_channel - Stop DMA and disable interrupts
 * @dma_base: DMA address
 * @base: controller address
 * @lch: DMA channel number
 *
 * Stop current DMA activity for client, disable DMA channel and interrupts.
 */
static inline void hsi_disable_dma_channel(void __iomem	*dma_base,
					   void __iomem	*base,
					   int		lch)
{
	u32 mask;

	/* Disable dma channel and interrupt setup */
	mask = 1 << lch;
	hsi_dwahb_single_masked_clear(HSI_DWAHB_CHEN(dma_base), mask);
	set_hsi_sdma_enable(base, lch, HSI_DMA_CFG_DISABLE);
#ifdef USE_MASTER_DMA_IRQ
	hsi_dwahb_single_masked_clear(HSI_DWAHB_MASKTFR(dma_base), mask);
#else
	set_hsi_interrupt_status_enable_transfer_complete(base, lch,
							  HSI_STATUS_DISABLE);
	set_hsi_int_stat_sig_enable_transfer_complete(base, lch,
						      HSI_STATUS_DISABLE);
#endif
}

/**
 * hsi_cleanup_gdd - Stop DMA activities related to client
 * @hsi: hsi_controller struct
 * @cl: HSI client
 *
 * Stop current DMA activity for client, disable DMA channel and interrupts.
 */
static void hsi_cleanup_gdd(struct hsi_controller *hsi, struct hsi_client *cl)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct hsi_mid_platform_data *pd;
	void __iomem	*dma_base = intel_hsi->gdd;
	void __iomem	*base = intel_hsi->sys;
	unsigned int	i;
	int		lch;

	/* GDD clean-up cannot be made on message, as there might not be any
	 * left. It is much safer to use the DMA and FIFO configuration of
	 * the client */
	/* get our platform data -- put there in hsi_new_client */
	pd = (struct hsi_mid_platform_data *)(cl->device.platform_data);
	if (pd == NULL)
		return;

	for (i = 0; i < HSI_MAX_GDD_LCH; i++) {
		/* Look at the TX configuration */
		lch = pd->tx_dma_channels[i];
		if (lch >= 0)
			hsi_disable_dma_channel(dma_base, base, lch);

		/* Look at the RX configuration */
		lch = pd->rx_dma_channels[i];
		if (lch >= 0)
			hsi_disable_dma_channel(dma_base, base, lch);
	}
}

/**
 * hsi_disable_fifos - Disable fifos
 * @hsi: hsi_controller struct
 * @cl: HSI client
 *
 * Disable any channel with fifo size > 0, set size to 0.
 */
static inline void hsi_disable_fifos(struct hsi_controller *hsi,
				     struct hsi_client *cl)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct hsi_mid_platform_data *pd;
	void __iomem *base = intel_hsi->sys;
	unsigned int i;

	pd = (struct hsi_mid_platform_data *)(cl->device.platform_data);
	if (pd == NULL)
		return;

	for (i = 0; i < HSI_INTEL_MAX_CHANNELS; i++) {
		/* Look at the TX configuration */
		if (pd->tx_fifo_sizes[i] > 0) {
			set_hsi_program_tx_channel_enable(base, i,
						HSI_PROGRAM_CHANNEL_DISABLED);
			set_hsi_tx_fifo_size(base, i, 0);
		}

		/* Look at the RX configuration */
		if (pd->rx_fifo_sizes[i] > 0) {
			set_hsi_program_tx_channel_enable(base, i,
						HSI_PROGRAM_CHANNEL_DISABLED);
			set_hsi_rx_fifo_size(base, i, 0);
		}
	}
}

/**
 * hsi_mid_release - Port release routine
 * @cl: HSI client
 *
 * Cleanup DMA, clear controller status, clean up queues, release DMA channels.
 *
 * Return 0.
 */
static int hsi_mid_release(struct hsi_client *cl)
{
	struct hsi_port *port = hsi_get_port(cl);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	/* Stop all the pending DMA requests for that client */
	hsi_cleanup_gdd(hsi, cl);
	/* Clear and disable interrupts -- stop pending PIO transfers */
	hsi_clear_controller_status(hsi);
	/* Now cleanup all the queues */
	hsi_cleanup_queues(cl);

	/* Disable the FIFO */
	hsi_disable_fifos(hsi, cl);

	/* Disable the clock */
	set_hsi_clk_tx_enable(base, HSI_TX_CLOCK_DISABLE);
	set_hsi_clk_internal_enable(base, HSI_TX_INTERNAL_CLOCK_ENABLE_STOP);

	/* reset runtime power management state machine */
	hsi_rtpm_fsm_reset(port);

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

/**
 * hsi_mid_flush - Port flush routine
 * @cl: HSI client
 *
 * Cleanup DMA, clear controller status, clean up queues and pending msgs.
 *
 * Return 0.
 */
static int hsi_mid_flush(struct hsi_client *cl)
{
	struct hsi_port *port = hsi_get_port(cl);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	unsigned int i;
	unsigned long flags;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_sync(&intel_hsi->pdev->dev);

	/* Stop all DMA transfers */
	hsi_cleanup_gdd(hsi, cl);

	/* Clear and disable interrupts */
	hsi_clear_controller_status(hsi);

	/* Dequeue all pending requests */
	spin_lock_irqsave(&intel_port->lock, flags);
	for (i = 0; i < intel_port->channels; i++) {
		_hsi_flush_queue(&intel_port->txqueue[i], cl, port, &flags);
		_hsi_flush_queue(&intel_port->rxqueue[i], cl, port, &flags);
	}
	_hsi_flush_queue(&intel_port->tx_brkqueue, cl, port, &flags);
	_hsi_flush_queue(&intel_port->rx_brkqueue, cl, port, &flags);
	_hsi_flush_queue(&intel_port->fwdqueue, cl, port, &flags);
	spin_unlock_irqrestore(&intel_port->lock, flags);

	/* Restart communications */
	hsi_enable_error_interrupts(hsi);

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

/**
 * hsi_mid_start_tx - Port start_tx routine
 * @cl: HSI client
 *
 * Set desired state to TX_WAKE and program TX_WAKE bit in controller.
 *
 * Return 0 or err code when tx clock does not come up.
 */
static int hsi_mid_start_tx(struct hsi_client *cl)
{
	struct hsi_port *port = hsi_get_port(cl);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	unsigned long flags;
	int prev;

	if (atomic_sub_and_test(0, &intel_port->wk_refcount)) {
		spin_lock_irqsave(&intel_port->pm_lock, flags);
		prev = intel_port->sst.state;
		intel_port->sst.state = HSI_RTPM_TXRX_ON;
		if (prev == HSI_RTPM_TXRX_IDLE) {
			/* TX_IDLE -> TX_ON */
			spin_unlock_irqrestore(&intel_port->pm_lock, flags);

			if (hsi_ctrl_is_state(intel_hsi, HSI_RTPM_D0I3)) {
				/* delay the tx activation to resume callback */
				spin_lock_irqsave(&intel_port->pm_lock, flags);
				if (intel_port->sst.delay_activate == 0) {
					intel_port->sst.delay_activate = 1;
					spin_unlock_irqrestore( \
						&intel_port->pm_lock, flags);
					pm_runtime_get(&intel_hsi->pdev->dev);
					spin_lock_irqsave( \
						&intel_port->pm_lock, flags);
				}
				spin_unlock_irqrestore( \
						&intel_port->pm_lock, flags);
			} else {
				pm_runtime_get_noresume(&intel_hsi->pdev->dev);
				hsi_rtpm_tx_activate(port);
			}
			spin_lock_irqsave(&intel_port->pm_lock, flags);
		} else {
			/* TX_PENDING -> TX_ON */
			del_timer(&intel_port->sst.timer);
		}
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
	}
	atomic_inc(&intel_port->wk_refcount);

	return 0;
}

/**
 * hsi_mid_stop_tx - Port stop_tx routine
 * @cl: HSI client
 *
 * Set desired state to TX_SLEEP and program TX_WAKE bit in controller.
 *
 * Controlled by wk_refcount -- don't stop tx unless
 * refcount is 0.
 *
 * Return 0.
 */
static int hsi_mid_stop_tx(struct hsi_client *cl)
{
	struct hsi_port *port = hsi_get_port(cl);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	unsigned long flags;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	if (intel_port->sst.state != HSI_RTPM_TXRX_ON) {
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	/* Tx state remains in TX_ON */
	if (!atomic_dec_and_test(&intel_port->wk_refcount))
		return 0;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	if (_hsi_rtpm_txrx_is_idle(HSI_LOGICAL_WRITE, port)) {
		/* TX_ON -> TX_IDLE */
		_hsi_rtpm_put_tx_to_idle(port, &flags);
	} else {
		/* TX_ON -> TX_PENDING */
		intel_port->sst.state = HSI_RTPM_TXRX_PENDING;
		intel_port->sst.timer_cnt = 0;
		mod_timer(&intel_port->sst.timer, jiffies + \
			msecs_to_jiffies(HSI_RTPM_TX_DRAIN_DELAY));
	}
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	return 0;
}

/**
 * hsi_wake - CA_WAKE handler
 * @port: HSI port
 *
 * Clear wake interrupt state and signal HSI_EVENT_START_RX event.
 */
static void hsi_wake(struct hsi_port *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	unsigned long flags;
	int prev;

	spin_lock_irqsave(&intel_port->pm_lock, flags);
	prev = intel_port->ssr.state;
	intel_port->ssr.state = HSI_RTPM_TXRX_ON;
	intel_port->ssr.timer_cnt = 0;
	if (prev == HSI_RTPM_TXRX_IDLE) {
		/* RX_IDLE -> RX_ON */
		spin_unlock_irqrestore(&intel_port->pm_lock, flags);

		if (hsi_ctrl_is_state(intel_hsi, HSI_RTPM_D0I3)) {
			/* delay the rx activation to resume callback */
			spin_lock_irqsave(&intel_port->pm_lock, flags);
			if (intel_port->ssr.delay_activate == 0) {
				intel_port->ssr.delay_activate = 1;
				spin_unlock_irqrestore( \
						&intel_port->pm_lock, flags);
				pm_runtime_get(&intel_hsi->pdev->dev);
				spin_lock_irqsave(&intel_port->pm_lock, flags);
			}
			spin_unlock_irqrestore(&intel_port->pm_lock, flags);
		} else {
			pm_runtime_get_noresume(&intel_hsi->pdev->dev);
			/* activate rx immediately if device is active */
			hsi_rtpm_rx_activate(port);
		}

		/* signal listeners that wake has occurred */
		hsi_event(port, HSI_EVENT_START_RX);
		spin_lock_irqsave(&intel_port->pm_lock, flags);
	} else {
		/* RX_PENDING -> RX_ON */
		hsi_rtpm_rx_activate(port);
	}
	spin_unlock_irqrestore(&intel_port->pm_lock, flags);

	/* clear interrupt */
	clear_hsi_interrupt_status_wkup_interrupt(base);
}

/**
 * hsi_pio_complete - Handle Tx/Rx threshold reached status
 * @port: HSI port
 * @queue: Queue for completion.
 *
 * Check message at head of queue to see if it's completed; if so, call
 * msg completion routine, otherwise send out the next block of message data.
 */
static void hsi_pio_complete(struct hsi_port *port, struct list_head *queue)
{
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	void __iomem *base = intel_hsi->sys;
	void __iomem *fastaddress;
	struct hsi_msg *msg = NULL;
	u32 *buf;
	u32 dwords;
	unsigned long flags;

	if (!queue)
		return;

	spin_lock_irqsave(&intel_port->lock, flags);
	if (list_empty(queue)) {
		spin_unlock_irqrestore(&intel_port->lock, flags);
		return;
	}
	msg = list_first_entry(queue, struct hsi_msg, link);
	if ((!msg->sgt.nents) || (!msg->sgt.sgl->length)) {
		msg->actual_len = 0;
		msg->status = HSI_STATUS_PENDING;
	}
	if (msg->status == HSI_STATUS_PROCEEDING) {
		buf = sg_virt(msg->sgt.sgl) + msg->actual_len;
		if (msg->ttype == HSI_MSG_WRITE) {
			dwords = min(NUM_TX_FIFO_DWORDS(
				fifo_sizes[tx_fifo_size[msg->channel]]),
				(msg->sgt.sgl->length - msg->actual_len)
					/ sizeof(*buf));
			msg->actual_len += dwords * sizeof(*buf);
			fastaddress = ARASAN_HSI_TX_DATA(base, msg->channel);
			for (; dwords > 0; dwords--) {
				iowrite32(*buf, fastaddress);
				buf++;
			}
			clear_hsi_interrupt_status_tx_threshold_reached(
				base, msg->channel);
		} else {
			fastaddress = ARASAN_HSI_RX_DATA(base,
							msg->channel);
			dwords = min(NUM_RX_FIFO_DWORDS(
				fifo_sizes[rx_fifo_size[msg->channel]]),
				(msg->sgt.sgl->length - msg->actual_len)
					/ sizeof(*buf));
			msg->actual_len += dwords * sizeof(*buf);
			for (; dwords > 0; dwords--) {
				*buf = ioread32(fastaddress);
				buf++;
			}
			clear_hsi_interrupt_status_rx_threshold_reached(
				base, msg->channel);
		}
		if (msg->actual_len >= msg->sgt.sgl->length)
			msg->status = HSI_STATUS_COMPLETED;
		/*
		 * Wait for the last written frame to be really sent before
		 * we call the complete callback
		 */
		if ((msg->status == HSI_STATUS_PROCEEDING) ||
			((msg->status == HSI_STATUS_COMPLETED) &&
				(msg->ttype == HSI_MSG_WRITE))) {
			spin_unlock_irqrestore(&intel_port->lock, flags);
			return;
		}
	}

	/* Transfer completed at this point */
	if (msg->ttype == HSI_MSG_WRITE) {
		set_hsi_int_stat_sig_enable_tx_threshold_reached(
			base, msg->channel, HSI_STATUS_DISABLE);
		set_hsi_interrupt_status_enable_tx_threshold_reached(base,
			msg->channel, HSI_STATUS_DISABLE);
		clear_hsi_interrupt_status_tx_threshold_reached(base,
			msg->channel);
	} else {
		set_hsi_int_stat_sig_enable_rx_threshold_reached(
			base, msg->channel, HSI_STATUS_DISABLE);
		set_hsi_interrupt_status_enable_rx_threshold_reached(base,
			msg->channel, HSI_STATUS_DISABLE);
		clear_hsi_interrupt_status_rx_threshold_reached(base,
			msg->channel);
	}
	list_del(&msg->link);
	spin_unlock_irqrestore(&intel_port->lock, flags);

	hsi_transfer(intel_port, queue);
	msg->complete(msg);
}

/**
 * hsi_gdd_complete - Handle DMA transfer complete status
 * @hsi: HSI controller
 * @lch: DMA channel.
 *
 * Disable DMA channel, remove message from DMA control structure, call
 * msg completion routine, call transfer again for next message in channel's
 * queue.
 */
static void hsi_gdd_complete(struct hsi_controller *hsi, unsigned int lch)
{
	struct intel_hsi_controller *intel_hsi;
	struct hsi_msg *msg;
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	void __iomem *base;
	void __iomem *dma_base;
	struct list_head *queue;
	unsigned long flags;

	intel_hsi = hsi_controller_drvdata(hsi);

	msg = intel_hsi->gdd_trn[lch];
	if (!msg) {
		pr_debug("gdd_complete bad/no message %d\n", lch);
		return;
	}
	intel_hsi->gdd_trn[lch] = NULL; /* release GDD lch */

	port		= to_hsi_port(msg->cl->device.parent);
	intel_port	= hsi_port_drvdata(port);
	dma_base	= intel_hsi->gdd;
	base		= intel_hsi->sys;

	spin_lock_irqsave(&intel_port->lock, flags);

	/* clear completion interrupt and disable slave DMA channel */
#ifdef USE_MASTER_DMA_IRQ
	hsi_dwahb_single_masked_write(HSI_DWAHB_CLEARTFR(dma_base), (1<<lch));
#else
	clear_hsi_interrupt_dma_transfer_complete(base, lch);
#endif
	iowrite32(0, ARASAN_HSI_DMA_CONFIG(base, lch));

	list_del(&msg->link);
	queue = (msg->ttype == HSI_MSG_READ) ? \
			&intel_port->rxqueue[msg->channel] : \
			&intel_port->txqueue[msg->channel];

	while (hsi_start_transfer(intel_port, queue) < 0) {
		list_add_tail(&msg->link, &intel_port->fwdqueue);
		msg = list_first_entry(queue, struct hsi_msg, link);
		msg->status = HSI_STATUS_ERROR;
		msg->actual_len = 0;
		list_del(&msg->link);
	}
	list_add_tail(&msg->link, &intel_port->fwdqueue);

	spin_unlock_irqrestore(&intel_port->lock, flags);
}

/**
 * hsi_pio_tasklet - Called following HSI controller interrupt
 * @port: HSI port
 *
 * Handle PIO, error, DMA completion (if used), break conditions.
 */
static void hsi_pio_tasklet(unsigned long h_port)
{
	struct hsi_port *port = (struct hsi_port *)h_port;
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	unsigned int ch;
	u32 status_reg;
	u32 dma_mask	= 1 << HSI_INT_STATUS_DMA_COMPLETE;
	u32 tx_mask	= 1 << HSI_INT_STATUS_TX_THR_REACHED;
	u32 rx_mask	= 1 << HSI_INT_STATUS_RX_THR_REACHED;
	u32 wake_mask	= 1 << HSI_INT_STATUS_WKUP_INT;
	u32 error_mask	= 1 << HSI_INT_STATUS_ERROR;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_noresume(&intel_hsi->pdev->dev);

	status_reg = ioread32(ARASAN_HSI_INTERRUPT_STATUS(base));

	for (ch = 0; ch < HSI_MAX_GDD_LCH; ch++) {
		if (status_reg & dma_mask)
			hsi_gdd_complete(hsi, ch);
		dma_mask <<= 1;
	}

	for (ch = 0; ch < intel_port->channels; ch++) {
		if (status_reg & tx_mask)
			hsi_pio_complete(port,
					 &intel_port->txqueue[ch]);
		if (status_reg & rx_mask)
			hsi_pio_complete(port,
					 &intel_port->rxqueue[ch]);
		tx_mask <<= 1;
		rx_mask <<= 1;
	}

	if (status_reg & wake_mask)
		hsi_wake(port);

	if (status_reg & error_mask)
		hsi_error(port);

	/* Allow runtime suspend after register access is over */
	pm_runtime_put_noidle(&intel_hsi->pdev->dev);

#ifdef DEFER_IRQ_CLEARING
	tasklet_schedule(&intel_port->fwd_tasklet);
	enable_irq(intel_port->irq);
#else
	tasklet_hi_schedule(&intel_port->fwd_tasklet);
#endif
}

/**
 * hsi_fwd_tasklet - Called to complete message handling
 * @port: HSI port
 *
 */
static void hsi_fwd_tasklet(unsigned long h_port)
{
	struct hsi_msg *msg;
	struct hsi_port *port = (struct hsi_port *)h_port;
	struct hsi_controller *hsi = to_hsi_controller(port->device.parent);
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	unsigned int dir;
	unsigned long flags;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_noresume(&intel_hsi->pdev->dev);

	spin_lock_irqsave(&intel_port->lock, flags);
	while (!list_empty(&intel_port->fwdqueue)) {
		msg = list_first_entry(&intel_port->fwdqueue,
				       struct hsi_msg, link);
		list_del(&msg->link);
		spin_unlock_irqrestore(&intel_port->lock, flags);
		dir = (msg->ttype == HSI_MSG_READ) ? \
		      DMA_FROM_DEVICE : DMA_TO_DEVICE;
		dma_unmap_sg(hsi->device.parent, msg->sgt.sgl,
			     msg->sgt.nents, dir);
		msg->actual_len = sg_dma_len(msg->sgt.sgl);
		if (msg->status != HSI_STATUS_ERROR)
			msg->status = HSI_STATUS_COMPLETED;
		msg->complete(msg);
		spin_lock_irqsave(&intel_port->lock, flags);
	}
	spin_unlock_irqrestore(&intel_port->lock, flags);

	/* Allow runtime suspend after register access is over */
	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);
}

/**
 * hsi_pio_isr - Called for HSI controller interrupt
 * @irq: IRQ number
 * @port: HSI port
 *
 * Schedule tasklet for HSI controller IRQ.
 */
static irqreturn_t hsi_pio_isr(int irq, void *port)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi = to_hsi_controller( \
			((struct hsi_port *)port)->device.parent);
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	unsigned long flags;

	if (!hsi_setup_completed(intel_port))
		return IRQ_HANDLED;

	if (hsi_ctrl_is_state(intel_hsi, HSI_RTPM_D0I3)) {
		spin_lock_irqsave(&intel_hsi->pm_lock, flags);
		if (intel_hsi->delay_interrupt == 0) {
			intel_hsi->delay_interrupt = 1;
			spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);
			pm_runtime_get(&intel_hsi->pdev->dev);
			spin_lock_irqsave(&intel_hsi->pm_lock, flags);
		}
		spin_unlock_irqrestore(&intel_hsi->pm_lock, flags);
		return IRQ_HANDLED;
	}

#ifdef DEFER_IRQ_CLEARING
	disable_irq_nosync(intel_port->irq);
	tasklet_hi_schedule(&intel_port->pio_tasklet);
#else
	hsi_pio_tasklet((unsigned long) port);
#endif
	return IRQ_HANDLED;
}

#ifdef USE_MASTER_DMA_IRQ
/**
 * hsi_dma_tasklet - Called for HSI master DMA interrupt
 * @irq: IRQ number
 * @port: HSI port
 *
 * Handles master DMA interrupt -- call completion for related channel,
 * schedule tasklet to complete messages.
 */
static void hsi_dma_tasklet(unsigned long h_port)
{
	struct hsi_port *port = (struct hsi_port *)h_port;
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	struct hsi_controller *hsi;
	struct intel_hsi_controller *intel_hsi;
	void __iomem *dma_base;
	unsigned int ch;
	u32 status_reg;

	hsi = to_hsi_controller(port->device.parent);
	intel_hsi = hsi_controller_drvdata(hsi);
	dma_base = intel_hsi->gdd;

	/* Assure the device is active when accessing the registers */
	pm_runtime_get_noresume(&intel_hsi->pdev->dev);

	while ((status_reg = ioread32(HSI_DWAHB_STATUSTFR(dma_base)))) {
		for (ch = 0; ch < HSI_MAX_GDD_LCH; ch++) {
			if (status_reg & (1<<ch))
				hsi_gdd_complete(hsi, ch);
		}
	}

	/* Allow runtime suspend after register access is over */
	pm_runtime_put_noidle(&intel_hsi->pdev->dev);

#ifdef DEFER_IRQ_CLEARING
	tasklet_schedule(&intel_port->fwd_tasklet);
	enable_irq(DMA_IRQ /*intel_hsi->dmac->irq*/);
#else
	tasklet_hi_schedule(&intel_port->fwd_tasklet);
#endif
}

/**
 * hsi_dma_isr - Master DMA ISR
 * @irq: IRQ number
 * @port: HSI port
 *
 * Handles master DMA IRQ -- schedule DMA tasklet.
 */
static irqreturn_t hsi_dma_isr(int irq, void *port)
{
#ifdef DEFER_IRQ_CLEARING
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
#endif
	struct hsi_controller *hsi;
	struct intel_hsi_controller *intel_hsi;
	void __iomem *dma_base;

	hsi = to_hsi_controller(((struct hsi_port *)port)->device.parent);
	intel_hsi = hsi_controller_drvdata(hsi);
	dma_base = intel_hsi->gdd;

	if (!ioread32(HSI_DWAHB_STATUSINT(dma_base)))
		return IRQ_NONE;

#ifdef DEFER_IRQ_CLEARING
	disable_irq_nosync(DMA_IRQ /*intel_hsi->dmac->irq*/);
	tasklet_hi_schedule(&intel_port->dma_tasklet);
#else
	hsi_dma_tasklet((unsigned long) port);
#endif
	return IRQ_HANDLED;
}
#endif

/**
 * hsi_port_irq - Request IRQ for HSI controller interrupt
 * @port: HSI port
 *
 * Call request_irq for HSI controller.
 *
 * Returns value returned by request.
 */
static int __devinit hsi_port_irq(struct hsi_port *port,
				struct intel_hsi_controller *intel_hsi)
{
	struct intel_hsi_port *intel_port = hsi_port_drvdata(port);
	int err;

	dev_dbg(&port->device, "request port/controller IRQ\n");

#ifdef DEFER_IRQ_CLEARING
	tasklet_init(&intel_port->pio_tasklet, hsi_pio_tasklet,
			(unsigned long)port);
#endif
	tasklet_init(&intel_port->fwd_tasklet, hsi_fwd_tasklet,
			(unsigned long)port);
#ifdef USE_MASTER_DMA_IRQ
	tasklet_init(&intel_port->dma_tasklet, hsi_dma_tasklet,
			(unsigned long)port);
#endif

	intel_port->irq = intel_hsi->pdev->irq;
	dev_dbg(&port->device, "request controller IRQ %d\n",
			intel_port->irq);
	err = request_irq(intel_port->irq, hsi_pio_isr,
			IRQF_TRIGGER_RISING, HSI_MPU_IRQ_NAME, port);
	if (err < 0) {
		dev_err(&port->device, "Request IRQ %d failed (%d)\n",
			intel_port->irq, err);
		return err;
	}

#ifdef USE_MASTER_DMA_IRQ
	dev_dbg(&port->device, "request DMA IRQ %d (30)\n",
			intel_hsi->dmac->irq);
	err = request_irq(DMA_IRQ /*intel_hsi->dmac->irq*/, hsi_dma_isr,
			IRQF_SHARED, HSI_DMA_IRQ_NAME, port);
	if (err < 0)
		dev_err(&port->device, "Request IRQ %d failed (%d)\n",
			DMA_IRQ /*intel_hsi->dmac->irq*/, err);
#endif

	return err;
}

/**
 * hsi_clear_controller_status - Clear interrupt and error bits
 * @hsi: HSI controller
 *
 * Clear all interrupt, signal, and error bits.  Note error int
 * status enable is set when channel is initialized, not bothered here
 */
static void hsi_clear_controller_status(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi =
	    (struct intel_hsi_controller *)hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	iowrite32(0, ARASAN_HSI_INTERRUPT_STATUS_ENABLE(base));
	iowrite32(0, ARASAN_HSI_INTERRUPT_SIGNAL_ENABLE(base));
	iowrite32(0, ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base));
	iowrite32(0xffffffff, ARASAN_HSI_INTERRUPT_STATUS(base));
	iowrite32(0xffffffff, ARASAN_HSI_ERROR_INTERRUPT_STATUS(base));
}

/**
 * hsi_enable_error_interrupts - Enable error interrupt status and signal
 * @hsi: HSI controller
 *
 * Set all error interrupt status and signal enable bits.
 */
static void hsi_enable_error_interrupts(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi =
	    (struct intel_hsi_controller *)hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;

	iowrite32(0x3ff, ARASAN_HSI_ERROR_INTERRUPT_SIGNAL_ENABLE(base));
}

/**
 * hsi_softreset - Perform soft reset
 * @hsi: HSI controller
 *
 * Perform soft reset, wait in loop until it is done.
 *
 * Returns 0 if successful, -EIO if it doesn't work.
 */
static int hsi_softreset(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi =
	    (struct intel_hsi_controller *)hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	void __iomem *dma_base = intel_hsi->gdd;
	u32 status = HSI_PROGRAM_SOFTRESET_RESET;
	int ind = 0;

	/* perform software reset then loop until controller is ready */
	set_hsi_program_software_reset(base);
	while ((status == HSI_PROGRAM_SOFTRESET_RESET) &&
			(ind < HSI_RESETDONE_RETRIES)) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(HSI_RESETDONE_TIMEOUT));
		status = get_hsi_program_software_reset(base);
		ind++;
	}

	if (ind >= HSI_RESETDONE_RETRIES) {
		dev_err(&hsi->device, "hsi_softreset failed");
		return -EIO;
	}

	/* disable all master DMA channels */
	hsi_dwahb_masked_write(HSI_DWAHB_CHEN(dma_base), 0xFF, 0x00);
	return 0;
}

/**
 * hsi_queues_init - Init channel's tx/rx/break queues
 * @port: Intel port
 *
 * Init channel tx, rx, break queues.
 */
static void __devinit hsi_queues_init(struct intel_hsi_port *intel_port)
{
	unsigned int ch;

	for (ch = 0; ch < HSI_INTEL_MAX_CHANNELS; ch++) {
		INIT_LIST_HEAD(&intel_port->txqueue[ch]);
		INIT_LIST_HEAD(&intel_port->rxqueue[ch]);
	}
	INIT_LIST_HEAD(&intel_port->tx_brkqueue);
	INIT_LIST_HEAD(&intel_port->rx_brkqueue);
	INIT_LIST_HEAD(&intel_port->fwdqueue);
}

/**
 * hsi_ports_init - Map memory for HSI and DMA controllers
 * @hsi: HSI controller
 * @pdev: PCI device initializing from
 *
 * Allocate memory for intel HSI port and init port basics; attach
 * intel port to hsi controller structure; get port IRQ.
 * Note port device not filled in until register_controller.
 *
 * Return 0 if successful, -ENOMEM if no memory for intel_port alloc,
 * error if can't get HSI IRQ.
 */
static int __devinit hsi_ports_init(struct hsi_controller *hsi,
				struct intel_hsi_controller *intel_hsi)
{
	struct hsi_port *port;
	struct intel_hsi_port *intel_port;
	unsigned int i;
	int err;

	for (i = 0; i < hsi->num_ports; i++) {
		port = &hsi->port[i];
		intel_port = kzalloc(sizeof(*intel_port), GFP_KERNEL);
		if (!intel_port)
			return -ENOMEM;
		port->async = hsi_mid_async;
		port->setup = hsi_mid_setup;
		port->flush = hsi_mid_flush;
		port->start_tx = hsi_mid_start_tx;
		port->stop_tx = hsi_mid_stop_tx;
		port->release = hsi_mid_release;
		hsi_port_set_drvdata(port, intel_port);

		/* initialize intel_port basics */
		hsi_queues_init(intel_port);
		spin_lock_init(&intel_port->lock);
		intel_port->dev = &port->device;

		hsi_rtpm_port_fsm_init(port);

		err = hsi_port_irq(port, intel_hsi);
		if (err < 0) {
			kfree(intel_port);
			return err;
		}
	}

	pm_runtime_mark_last_busy(&intel_hsi->pdev->dev);
	pm_runtime_put_autosuspend(&intel_hsi->pdev->dev);

	return 0;
}

/**
 * hsi_ports_exit - Stop port activity
 * @hsi: HSI controller
 *
 * Stop Tx and Rx, kill active tasklet for port.
 */
static void hsi_ports_exit(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi =
	    (struct intel_hsi_controller *)hsi_controller_drvdata(hsi);
	void __iomem *base = intel_hsi->sys;
	void __iomem *dma_base = intel_hsi->gdd;
	struct intel_hsi_port *intel_port;
	unsigned int i;

	/* disable the DMA controller */
	iowrite32(HSI_DWAHB_DMACFG_DISABLED, HSI_DWAHB_DMACFG(dma_base));

	/* stop tx, rx */
	set_hsi_program_rx_wake(base, HSI_PROGRAM_RX_SLEEP);
	set_hsi_program_tx_wakeup(base, HSI_PROGRAM_TX_SLEEP);

	for (i = 0; i < hsi->num_ports; i++) {
		intel_port = hsi_port_drvdata(&hsi->port[i]);
		WARN_ON(!atomic_sub_and_test(0, &intel_port->wk_refcount));
#ifdef DEFER_IRQ_CLEARING
		tasklet_kill(&intel_port->pio_tasklet);
#endif
		tasklet_kill(&intel_port->fwd_tasklet);
#ifdef USE_MASTER_DMA_IRQ
		tasklet_kill(&intel_port->dma_tasklet);
#endif
	}
}

/**
 * hsi_add_controller - Make and init intel_hsi controller
 * @hsi: HSI controller
 * @pdev: PCI device
 *
 * Allocate intel_hsi controller, attach to hsi_controller, activate
 * PCI device and map memory for HSI and master DMA, init ports, and
 * register controller with HSI (perform board info scan there).
 *
 * Return 0 or errors for alloc, mapping, irq, init, registration
 */
static int __devinit hsi_add_controller(struct hsi_controller *hsi,
					struct pci_dev *pdev)
{
	struct intel_hsi_controller *intel_hsi;
	struct device *dev = &pdev->dev;
	int err = 0;
	int pci_bar = 0;
	unsigned long paddr;
	u32 iolen;

	/* set runtime status active at an early stage */
	pm_runtime_set_active(&pdev->dev);

	intel_hsi = kzalloc(sizeof(*intel_hsi), GFP_KERNEL);
	if (!intel_hsi) {
		pr_err("not enough memory for intel hsi\n");
		return -ENOMEM;
	}
	hsi->id = 0;
	hsi->num_ports = 1;
	hsi->device.parent = &pdev->dev;
	hsi->device.dma_mask = pdev->dev.dma_mask;

	dev_set_name(&hsi->device, "hsi%d", hsi->id);
	hsi_controller_set_drvdata(hsi, intel_hsi);
	intel_hsi->dev = &hsi->device;

	err = pci_enable_device(pdev);
	if (err) {
		pr_err("pci enable fail %d\n", err);
		goto error_ret;
	}

	/* get hsi controller io resource and map it */
	intel_hsi->pdev = pdev;
	paddr = pci_resource_start(pdev, pci_bar);
	iolen = pci_resource_len(pdev, pci_bar);
	pr_debug("hsi controller base, length %lx %x\n", paddr, iolen);

	err = pci_request_region(pdev, pci_bar, dev_name(dev));
	if (err)
		goto error_ret;

	intel_hsi->sys = ioremap_nocache(paddr, iolen);
	if (!intel_hsi->sys)
		goto error_ret;

	/* get master DMA info */
	intel_hsi->dmac = pci_get_device(PCI_VENDOR_ID_INTEL,
					HSI_MASTER_DMA_ID, NULL);
	if (!intel_hsi->dmac) {
		pr_debug("Can't find DMAC\n");
		err = -1;
		goto error_ret;
	}

	paddr = pci_resource_start(intel_hsi->dmac, pci_bar);
	iolen = pci_resource_len(intel_hsi->dmac, pci_bar);
	pr_debug("dma controller base, length %lx %x\n", paddr, iolen);
	err = pci_request_region(intel_hsi->dmac, pci_bar,
					dev_name(&intel_hsi->dmac->dev));
	if (err)
		goto error_ret;

	intel_hsi->gdd = ioremap_nocache(paddr, iolen);
	if (!intel_hsi->gdd) {
		err = -1;
		goto error_ret;
	}

	/* note this fills in port device info */
	err = hsi_register_controller(hsi);
	if (err < 0)
		goto error_ret;

	hsi_rtpm_ctrl_fsm_init(intel_hsi);

	err = hsi_ports_init(hsi, intel_hsi);
	if (err < 0)
		goto error_unreg;

	/* runtime power management needs to be enabled before
	 * any access to HSI controller registers
	 */
	hsi_rtpm_init(pdev);

	err = hsi_hw_init(hsi);
	if (err < 0)
		goto error_unreg;

	dev_set_drvdata(&pdev->dev, intel_hsi);

	return err;

error_unreg:
	hsi_unregister_controller(hsi);

error_ret:
	kfree(intel_hsi);
	return err;
}

/**
 * hsi_init_slave_dma_channel - Set up HSI slave DMA channel
 * @base: HSI controller
 * @tx_rx: HSI_LOGICAL_READ or HSI_LOGICAL_WRITE
 * @dma_channel: DMA channel number
 * @logical_channel: HSI channel number
 *
 * Set up HSI controller slave DMA channel
 */
static void hsi_init_slave_dma_channel(void __iomem *slave_base,
						u8 tx_rx,
						u8 dma_channel,
						u8 logical_channel)
{
	u8 read_write = (tx_rx == HSI_LOGICAL_READ) ?
						HSI_DMA_CFG_TX_RX_READ :
						HSI_DMA_CFG_TX_RX_WRITE;

	/* set up slave config register */
	set_hsi_sdma_tx_rx(slave_base, dma_channel, read_write);

	set_hsi_sdma_logical_channel(slave_base, dma_channel,
						logical_channel);

	set_hsi_sdma_burst_size(slave_base, dma_channel, HSI_DMA_CFG_BURST_32);
}

/**
 * hsi_init_master_dma_channel - Set up master DMA channel
 * @dma_base: HSI controller
 * @tx_rx: HSI_LOGICAL_READ or HSI_LOGICAL_WRITE
 * @dma_channel: DMA channel number
 * @logical_channel: HSI channel number
 *
 * Set master dma channel ctl and cfg registers to predefined values
 */
static void hsi_init_master_dma_channel(void __iomem *dma_base, u8 tx_rx,
						u8 dma_channel,
						u8 logical_channel)
{
	/* ensure channel config starts off 0 */
	iowrite32(0, HSI_DWAHB_CFG_LO(dma_base, dma_channel));
	iowrite32(0, HSI_DWAHB_CFG_HI(dma_base, dma_channel));

	set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_LO(dma_base, dma_channel),
					dma_channel,
					5, HSI_DWAHB_CFG_CH_PRIO);

	/* use hardware handshake */
	set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_HW_HSHAKE,
					10, HSI_DWAHB_CFG_HS_DST);
	set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_HW_HSHAKE,
					11, HSI_DWAHB_CFG_HS_SRC);

	/* set config source/dest handshake interface for channel */
	/* assign dma channel its own handshake interface number */
	if (tx_rx == HSI_LOGICAL_READ) {
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_HI(dma_base, dma_channel),
					dma_channel,
					7, HSI_DWAHB_CFG_SRC_PER);
	} else {
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_HI(dma_base, dma_channel),
					dma_channel,
					11, HSI_DWAHB_CFG_DST_PER);
	}

	/* set channel fifo mode */
	set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_HI(dma_base, dma_channel),
					1,
					0, HSI_DWAHB_CFG_FCMODE);

	set_hsi_dwahb_dma_reg(HSI_DWAHB_CFG_HI(dma_base, dma_channel),
					0,
					1, HSI_DWAHB_CFG_FIFO_MODE);

	/* ensure channel control reg starts off 0 */
	iowrite32(0, HSI_DWAHB_CTL_LO(dma_base, dma_channel));
	iowrite32(0, HSI_DWAHB_CTL_HI(dma_base, dma_channel));

	set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CTL_TR_WIDTH_32,
					1, HSI_DWAHB_CTL_DST_TR_WIDTH);

	set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CTL_TR_WIDTH_32,
					4, HSI_DWAHB_CTL_SRC_TR_WIDTH);

	/* logical read -> peripheral to memory, per is flow controller */
	/* logical write -> memory to peripheral, per is flow controller */
	/* set address increments accordingly */
	if (tx_rx == HSI_LOGICAL_READ) {
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CTL_TT_FC_P2M_P,
					20, HSI_DWAHB_CTL_TT_FC);
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_DSINC_INC,
					7, HSI_DWAHB_CTL_DINC);
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_DSINC_INC,
					9, HSI_DWAHB_CTL_SINC);
	} else {
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CTL_TT_FC_M2P_P,
					20, HSI_DWAHB_CTL_TT_FC);
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_DSINC_INC,
					7, HSI_DWAHB_CTL_DINC);
		set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
					HSI_DWAHB_CFG_DSINC_INC,
					9, HSI_DWAHB_CTL_SINC);
	}

	/* set channel ctl reg dest/src msize related to FIFO size
	 * and watermark of HSI controller dma
	 */
	set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
			4,
			14, HSI_DWAHB_CTL_SRC_MSIZE);
	set_hsi_dwahb_dma_reg(HSI_DWAHB_CTL_LO(dma_base, dma_channel),
			4,
			11, HSI_DWAHB_CTL_DST_MSIZE);
}


/**
 * set_hsi_ports_default - Set up ports default values
 * @hsi: Intel controller
 *
 * Set HSI controller defaults, default activity to RX/TX_WAKE, set up
 * channel defaults
 */
static void set_hsi_ports_default(struct intel_hsi_controller *hsi)
{
	void __iomem *base = hsi->sys;
	void __iomem *dma_base = hsi->gdd;
	int idx;

	set_hsi_arbiter_priority_policy(base, HSI_ARBITER_ROUNDROBIN);
	set_hsi_clk_data_timeout(base, 5);
	set_hsi_clk_rx_frame_burst(base, HSI_CLOCK_TX_FRAME_256);
	/* set clock tap delay to workable value */
	set_hsi_clk_tap_delay(base, 3);
	set_hsi_program_receive_timeout(base, 0x01);
	set_hsi_clk_rx_tailing_bit_count(base, HSI_CLOCK_TAIL_400);

	/* disable all channels */
	for (idx = 0; idx < HSI_INTEL_MAX_CHANNELS; idx++) {
		set_hsi_program_tx_channel_enable(base, idx,
					HSI_PROGRAM_CHANNEL_DISABLED);
		set_hsi_program_rx_channel_enable(base, idx,
					HSI_PROGRAM_CHANNEL_DISABLED);
	}

	/* enable the DMA controller -- following routines set DMA params */
	iowrite32(HSI_DWAHB_DMACFG_ENABLED, HSI_DWAHB_DMACFG(dma_base));
}

/**
 * hsi_hw_init - Init hardware
 * @hsi: HSI controller
 *
 * Soft reset the controller, set ports default values
 *
 * Return error if soft reset fails.
 */
static int __devinit hsi_hw_init(struct hsi_controller *hsi)
{
	struct intel_hsi_controller *intel_hsi = hsi_controller_drvdata(hsi);
	int err;

	err = hsi_softreset(hsi);
	if (err)
		return err;

	set_hsi_ports_default(intel_hsi);
	return 0;
}

/**
 * hsi_remove_controller - Stop controller and unregister with HSI
 * @hsi: HSI controller
 *
 * Stop controller and unregister with HSI
 */
static void hsi_remove_controller(struct hsi_controller *hsi)
{
	hsi_ports_exit(hsi);
	hsi_unregister_controller(hsi);
}

/**
 * intel_hsi_probe - Called during PCI probe
 * @pdev: PCI device
 * @ent: PCI device id
 *
 * Allocate and add controller, init hardware.
 *
 * Return error if any init routine fails.
 */
static int __devinit intel_hsi_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct hsi_controller *hsi;
	int err = 0;

	pr_debug("hsi controller probe entered");

	pr_debug("hsi controller irq %d\n", pdev->irq);
	hsi = hsi_alloc_controller(1, GFP_KERNEL);
	if (!hsi) {
		pr_err("No memory for hsi controller\n");
		return -ENOMEM;
	}
	err = hsi_add_controller(hsi, pdev);
	if (err < 0)
		goto out1;
#ifdef CONFIG_DEBUG_FS
	err = hsi_debug_add_ctrl(hsi);
	if (err < 0)
		goto out2;
#endif

	/* put the device to D0i3 after initialization */
	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	pr_debug("hsi controller probe exit %d", err);
	return err;
out2:
	hsi_remove_controller(hsi);
out1:
	hsi_free_controller(hsi);
	pr_debug("hsi controller probe error exit");
	return err;
}

/**
 * intel_hsi_remove - Called during PCI device exit
 * @pdev: PCI device
 *
 * Set PCI drv data null.
 */
static void __devexit intel_hsi_remove(struct pci_dev *pdev)
{
	struct driver_data *drv_data = pci_get_drvdata(pdev);

	if (!drv_data)
		return;

	pci_set_drvdata(pdev, NULL);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
}

/**
 * struct intel_mid_hsi_rtpm - Runtime power management callbacks
 */
static const struct dev_pm_ops intel_mid_hsi_rtpm = {
	.prepare = hsi_pm_prepare,
	.complete = hsi_pm_complete,
	.suspend = hsi_pm_suspend,
	.resume = hsi_pm_resume,
	.runtime_idle = hsi_pm_runtime_idle,
	.runtime_suspend = hsi_pm_runtime_suspend,
	.runtime_resume = hsi_pm_runtime_resume,
};

/**
 * struct pci_ids - PCI IDs handled by the driver -- ID of HSI controller
 */
static const struct pci_device_id pci_ids[] __devinitdata = {
	{ PCI_VDEVICE(INTEL, 0x833) },	/* HSI */
	{ }
};

/**
 * struct intel_hsi_driver - PCI structure for driver
 * @driver.pm: runtime power management ops
 * @name: driver name
 * @id_table: ID table
 * @probe: PCI probe routine
 * @remove: PCI remove routine
 */
static struct pci_driver intel_hsi_driver = {
	.driver = {
		.pm = &intel_mid_hsi_rtpm,
	},
	.name =		"intel_hsi",
	.id_table =	pci_ids,
	.probe =	intel_hsi_probe,
	.remove =	__devexit_p(intel_hsi_remove),
};

static int __init intel_hsi_init(void)
{
	pr_info("init Intel HSI controller driver\n");
	return pci_register_driver(&intel_hsi_driver);
}
module_init(intel_hsi_init);

static void __exit intel_hsi_exit(void)
{
	pr_info("Intel HSI controller driver removed\n");
	pci_unregister_driver(&intel_hsi_driver);
}
module_exit(intel_hsi_exit);

MODULE_ALIAS("pci:intel_hsi");
MODULE_AUTHOR("Jim Stanley <jim.stanley@intel.com>");
MODULE_DESCRIPTION("Intel mid HSI Controller Driver");
MODULE_LICENSE("GPL");

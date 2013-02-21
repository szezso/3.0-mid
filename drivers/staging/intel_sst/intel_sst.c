/*
 *  intel_sst.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10	Intel Corp
 *  Authors:	Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Anurag Kansal <anurag.kansal@intel.com>
 *		Vikas Gupta <vikas.gupta@intel.com>
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
 *  This driver exposes the audio engine functionalities to the ALSA
 *	 and middleware.
 *
 *  This file contains all init functions
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>
#include <linux/async.h>
#include <asm/intel_scu_ipc.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/current.h>
#include <asm/siginfo.h> /* siginfo */
#include <linux/rcupdate.h> /* rcu_read_lock */
#include "intel_sst.h"
#include "intel_sst_ioctl.h"
#include "soc_audio_bu_config.h"
#include "intel_sst_common.h"
#include "soc_audio_api.h"
#include "soc_ipc.h"
#include "soc_audio_pipeline_specific.h"
#include "soc_audio_bu_config.h"
#ifdef INCLUDE_LOOPBACK_IF
#include "intel_sst_loopback.h"
#endif

MODULE_AUTHOR("Lomesh Agarwal <lomesh.agarwal@intel.com>");
MODULE_AUTHOR("Anurag Kansal <anurag.kansal@intel.com>");
MODULE_AUTHOR("Vikas Gupta <vikas.gupta@intel.com>");
MODULE_AUTHOR("Priyanka Kharat <priyanka.kharat@intel.com>");
MODULE_AUTHOR("Chen-hsiang Feng <chen-hsiang.feng@intel.com>");
MODULE_DESCRIPTION("Intel (R) SST(R) Audio Engine Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SST_DRIVER_VERSION);

struct intel_sst_drv *sst_drv_ctx;
static struct mutex drv_ctx_lock;

/* fops Routines */
static const struct file_operations intel_sst_fops = {
	.owner = THIS_MODULE,
	.open = intel_sst_open,
	.release = intel_sst_release,
	.read = intel_sst_read,
	.write = intel_sst_write,
	.unlocked_ioctl = intel_sst_ioctl,
	.aio_read = intel_sst_aio_read,
	.aio_write = intel_sst_aio_write,
	.poll = intel_sst_poll,
	.fasync = intel_sst_fasync,
};
static const struct file_operations intel_sst_fops_cntrl = {
	.owner = THIS_MODULE,
	.open = intel_sst_open_cntrl,
	.release = intel_sst_release_cntrl,
	.unlocked_ioctl = intel_sst_ioctl,
};

static struct miscdevice lpe_dev = {
	.minor = MISC_DYNAMIC_MINOR,/* dynamic allocation */
	.name = "intel_sst",/* /dev/intel_sst */
	.fops = &intel_sst_fops
};


static struct miscdevice lpe_ctrl = {
	.minor = MISC_DYNAMIC_MINOR,/* dynamic allocation */
	.name = "intel_sst_ctrl",/* /dev/intel_sst_ctrl */
	.fops = &intel_sst_fops_cntrl
};

static void notify_stream_data_consumed(struct stream_info *stream)
{
	if (stream->codec != SST_CODEC_TYPE_PCM) {
		pr_debug("notify_stream_data_consumed: user notfification");
		kill_fasync(&stream->async_queue, SIGIO, POLL_IN);
		if (stream->poll_mode) {
			stream->flag = 1;
			wake_up(&stream->sleep);
		}
	} else if (stream->period_elapsed)
		stream->period_elapsed(stream->pcm_substream);
	return;
}

static void intel_sst_job_consumed_notify(void)
{
	unsigned int i, str_id;
	unsigned int data_consumed = 0;
	struct stream_info *stream = NULL;
	struct soc_audio_input_wl *input_wl = NULL;
	struct ipc_ia_time_stamp_t *fw_tstamp = NULL;

	for (i = 0; i < SOC_AUDIO_MAX_INPUTS; i++) {
		str_id = sst_drv_ctx->active_streams[i];
		if (SND_SST_DEVICE_NONE == str_id)
			continue;
		stream = &sst_drv_ctx->streams[str_id];
		if (stream->status == STREAM_UN_INIT)
			continue;

		input_wl = (struct soc_audio_input_wl *)stream->input_wl;
		if (!input_wl)
			continue;
		data_consumed = 0;

		/* All streams use time stamp */
		fw_tstamp = (struct ipc_ia_time_stamp_t *)
			((sst_drv_ctx->mailbox +
			  SOC_AUDIO_MAILBOX_TIMESTAMP_OFFSET));
		fw_tstamp += stream->soc_input_id;

		data_consumed = fw_tstamp->data_consumed;
		if (data_consumed) {
			fw_tstamp->data_consumed = 0;
			notify_stream_data_consumed(stream);
		}
	}
}

/**
* intel_sst_interrupt - Interrupt service routine for SST
*
* @irq:	irq number of interrupt
* @context: pointer to device structre
*
* This function is called by OS when SST device raises
* an interrupt. This will be result of write in IPC register
* Source can be busy or done interrupt
*/
static irqreturn_t intel_sst_interrupt(int irq, void *context)
{
	union interrupt_reg isr;
	uint32_t header, msg_id;
	union interrupt_reg imr;
	struct intel_sst_drv *drv = (struct intel_sst_drv *)context;

	/* Interrupt arrived, check src */
	isr.full = sst_shim_read(drv->shim, SST_ISRX);
	/* mask busy inetrrupt */
	imr.full = sst_shim_read(drv->shim, SST_IMRX);
	imr.full |= 0x3;
	sst_shim_write(sst_drv_ctx->shim, SST_IMRX, imr.full);
	if (isr.part.ipcd) {
		header = sst_shim_read(drv->shim, SST_IPCD);
		msg_id = soc_ipc_get_msg_id(header);

		/* buffer/period consumed handling */
		if (msg_id == SOC_IPC_IA_INPUT_JOB_CONSUMED) {
			intel_sst_job_consumed_notify();
			sst_clear_ipcd_interrupt();
			return IRQ_HANDLED;
		} else if (msg_id == SOC_IPC_IA_OUTPUT_JOB_AVAILABLE) {
#ifdef INCLUDE_LOOPBACK_IF
			intel_sst_job_processed_notify();
#endif
			sst_clear_ipcd_interrupt();
			return IRQ_HANDLED;
		} else {
			sst_drv_ctx->ipc_process_msg.header = header;
			memcpy_fromio(sst_drv_ctx->ipc_process_msg.mailbox,
				      drv->mailbox +
				      SOC_AUDIO_MAILBOX_IPCD_RCV_OFFSET,
				      SOC_AUDIO_MAILBOX_SIZE_IPCD_RCV);
			queue_work(sst_drv_ctx->process_msg_wq,
				   &sst_drv_ctx->ipc_process_msg.wq);
			return IRQ_HANDLED;
		}
	}
	if (isr.full & SOC_AUDIO_DSP_IPCX_INTR) {
		header = sst_shim_read(sst_drv_ctx->shim, SST_IPCX);
		sst_drv_ctx->ipc_process_reply.header = header;
		memcpy_fromio(sst_drv_ctx->ipc_process_reply.mailbox,
			      drv->mailbox + SOC_AUDIO_MAILBOX_IPCX_RCV_OFFSET,
			      SOC_AUDIO_MAILBOX_SIZE_IPCX_RCV);
		queue_work(sst_drv_ctx->process_reply_wq,
			   &sst_drv_ctx->ipc_process_reply.wq);
		return IRQ_HANDLED;
	}
	/* un mask busy interrupt */
	imr.full &= ~(0x3);
	sst_shim_write(sst_drv_ctx->shim, SST_IMRX, imr.full);
	return IRQ_NONE;
}


/*
* intel_sst_probe - PCI probe function
*
* @pci:	PCI device structure
* @pci_id: PCI device ID structure
*
* This function is called by OS when a device is found
* This enables the device, interrupt etc
*/
static int __devinit intel_sst_probe(struct pci_dev *pci,
			const struct pci_device_id *pci_id)
{
	int i, ret = 0;
	enum soc_result result;
	u32 csr;
	u32 csr2;
	u32 clkctl;

	pr_debug("Probe for DID %x\n", pci->device);
	mutex_lock(&drv_ctx_lock);
	if (sst_drv_ctx) {
		pr_err("Only one sst handle is supported\n");
		mutex_unlock(&drv_ctx_lock);
		return -EBUSY;
	}

	sst_drv_ctx = kzalloc(sizeof(*sst_drv_ctx), GFP_KERNEL);
	if (!sst_drv_ctx) {
		pr_err("intel_sst malloc fail\n");
		mutex_unlock(&drv_ctx_lock);
		return -ENOMEM;
	}
	mutex_unlock(&drv_ctx_lock);

	sst_drv_ctx->pci_id = pci->device;

	mutex_init(&sst_drv_ctx->stream_lock);

	sst_drv_ctx->stream_cnt = 0;
	sst_drv_ctx->encoded_cnt = 0;
	sst_drv_ctx->am_cnt = 0;
	sst_drv_ctx->pb_streams = 0;
	sst_drv_ctx->cp_streams = 0;
	sst_drv_ctx->pmic_port_instance = SST_DEFAULT_PMIC_PORT;
	sst_drv_ctx->fw = NULL;
	sst_drv_ctx->proc_hdl = SOC_BAD_HANDLE;
	sst_drv_ctx->compressed_slot = 0x0C;	/* default IHF */
	sst_drv_ctx->codec = SST_CODEC_TYPE_UNKNOWN;

	INIT_WORK(&sst_drv_ctx->ipc_process_msg.wq, sst_process_message);
	INIT_WORK(&sst_drv_ctx->ipc_process_reply.wq, sst_process_reply);
	init_waitqueue_head(&sst_drv_ctx->wait_queue);

	sst_drv_ctx->mad_wq = create_workqueue("sst_mad_wq");
	if (!sst_drv_ctx->mad_wq)
		goto do_free_drv_ctx;

	sst_drv_ctx->process_msg_wq = create_workqueue("sst_process_msg_wqq");
	if (!sst_drv_ctx->process_msg_wq)
		goto free_mad_wq;

	sst_drv_ctx->process_reply_wq = create_workqueue("sst_proces_reply_wq");
	if (!sst_drv_ctx->process_reply_wq)
		goto free_process_msg_wq;

	soc_audio_g_init_data.alloc_mem = allocate_shared_memory;
	soc_audio_g_init_data.free_mem = free_shared_memory;
	result = soc_init(&soc_audio_g_init_data);
	if (result != SOC_SUCCESS) {
		pr_err("soc_init failure\n");
		goto free_process_reply_wq;
	}

	for (i = 0; i < SND_SST_MAX_AUDIO_DEVICES; i++) {
		struct stream_info *stream = &sst_drv_ctx->streams[i];
		INIT_LIST_HEAD(&stream->bufs);
		mutex_init(&stream->lock);
		init_waitqueue_head(&stream->sleep);
		stream->flag = 0;
		/* init up per input volume */
		sst_drv_ctx->volume[i] = SOC_AUDIO_GAIN_0_DB;
	}

	sst_drv_ctx->mmap_mem = NULL;
	sst_drv_ctx->mmap_len = SST_MMAP_PAGES * PAGE_SIZE;
	while (sst_drv_ctx->mmap_len > 0) {
		sst_drv_ctx->mmap_mem =
		    kzalloc(sst_drv_ctx->mmap_len, GFP_KERNEL);
		if (sst_drv_ctx->mmap_mem) {
			pr_debug("Got memory %p size 0x%x\n",
				 sst_drv_ctx->mmap_mem, sst_drv_ctx->mmap_len);
			break;
		}
		if (sst_drv_ctx->mmap_len < (SST_MMAP_STEP * PAGE_SIZE)) {
			pr_err("mem alloc fail...abort!!\n");
			ret = -ENOMEM;
			goto free_process_reply_wq;
		}
		sst_drv_ctx->mmap_len -= (SST_MMAP_STEP * PAGE_SIZE);
		pr_err("mem alloc failed...trying %d\n",
		       sst_drv_ctx->mmap_len);
	}
	/* Init the device */
	ret = pci_enable_device(pci);
	if (ret) {
		pr_err("device cant be enabled\n");
		goto do_free_mem;
	}
	sst_drv_ctx->pci = pci_dev_get(pci);
	ret = pci_request_regions(pci, SST_DRV_NAME);
	if (ret)
		goto do_disable_device;
	/* map registers */
	/* SST Shim */
	sst_drv_ctx->shim = pci_ioremap_bar(pci, 1);
	if (!sst_drv_ctx->shim)
		goto do_release_regions;
	pr_debug("SST Shim Ptr %p\n", sst_drv_ctx->shim);

	/* Shared SRAM */
	sst_drv_ctx->mailbox = pci_ioremap_bar(pci, 2);
	if (!sst_drv_ctx->mailbox)
		goto do_unmap_shim;
	pr_debug("SRAM Ptr %p\n", sst_drv_ctx->mailbox);

	/* IRAM */
	sst_drv_ctx->iram = pci_ioremap_bar(pci, 3);
	if (!sst_drv_ctx->iram)
		goto do_unmap_sram;
	pr_debug("IRAM Ptr %p\n", sst_drv_ctx->iram);

	/* DRAM */
	sst_drv_ctx->dram = pci_ioremap_bar(pci, 4);
	if (!sst_drv_ctx->dram)
		goto do_unmap_iram;
	pr_debug("DRAM Ptr %p\n", sst_drv_ctx->dram);

	/* Register the ISR */
	ret = request_irq(pci->irq, intel_sst_interrupt,
		IRQF_SHARED, SST_DRV_NAME, sst_drv_ctx);
	if (ret)
		goto do_unmap_dram;
	pr_debug("Registered IRQ 0x%x\n", pci->irq);

	/*Register LPE Control as misc driver*/
	ret = misc_register(&lpe_ctrl);
	if (ret) {
		pr_err("couldn't register control device\n");
		goto do_free_irq;
	}

	ret = misc_register(&lpe_dev);
	if (ret) {
		pr_err("couldn't register LPE device\n");
		goto do_free_misc;
	}

#ifdef INCLUDE_LOOPBACK_IF
	ret = loopback_register();
	if (ret) {
		pr_err("sst: couldn't register loopback device\n");
		goto do_free_dev;
	}
#endif

	/*set lpe start clock and ram size*/
	csr = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr |= 0x30000;
	/*make sure clksel set to OSC for SSP0,1 (default)*/
	csr &= 0xFFFFFFF3;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr);

	/*set clock output enable for SSP0,1,3*/
	clkctl = sst_shim_read(sst_drv_ctx->shim, SST_CLKCTL);
	clkctl |= ((1<<16)|(1<<17));
	sst_shim_write(sst_drv_ctx->shim, SST_CLKCTL, clkctl);

	/* set SSP0 & SSP1 disable DMA Finish*/
	csr2 = sst_shim_read(sst_drv_ctx->shim, SST_CSR2);
	csr2 |= BIT(1)|BIT(2);
	sst_shim_write(sst_drv_ctx->shim, SST_CSR2, csr2);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0)
	pm_runtime_enable(&pci->dev);
	pm_runtime_set_suspended(&pci->dev);
#endif
	pci_set_drvdata(pci, sst_drv_ctx);
	pm_runtime_allow(&pci->dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	pm_runtime_put_noidle(&pci->dev);
#endif
	pr_debug("...probe successfully done!!!\n");
	return ret;
#ifdef INCLUDE_LOOPBACK_IF
do_free_dev:
	misc_deregister(&lpe_dev);
#endif
do_free_misc:
	misc_deregister(&lpe_ctrl);
do_free_irq:
	free_irq(pci->irq, sst_drv_ctx);
do_unmap_dram:
	iounmap(sst_drv_ctx->dram);
do_unmap_iram:
	iounmap(sst_drv_ctx->iram);
do_unmap_sram:
	iounmap(sst_drv_ctx->mailbox);
do_unmap_shim:
	iounmap(sst_drv_ctx->shim);
do_release_regions:
	pci_release_regions(pci);
do_disable_device:
	pci_disable_device(pci);
do_free_mem:
	soc_destroy();
	kfree(sst_drv_ctx->mmap_mem);
free_process_reply_wq:
	destroy_workqueue(sst_drv_ctx->process_reply_wq);
free_process_msg_wq:
	destroy_workqueue(sst_drv_ctx->process_msg_wq);
free_mad_wq:
	destroy_workqueue(sst_drv_ctx->mad_wq);
do_free_drv_ctx:
	kfree(sst_drv_ctx);
	sst_drv_ctx = NULL;
	pr_err("Probe failed with %d\n", ret);
	return ret;
}

/**
* intel_sst_remove - PCI remove function
*
* @pci:	PCI device structure
*
* This function is called by OS when a device is unloaded
* This frees the interrupt etc
*/
static void __devexit intel_sst_remove(struct pci_dev *pci)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	pm_runtime_get_noresume(&pci->dev);
	pm_runtime_forbid(&pci->dev);
#endif
	pci_dev_put(sst_drv_ctx->pci);
	misc_deregister(&lpe_ctrl);
	misc_deregister(&lpe_dev);
#ifdef INCLUDE_LOOPBACK_IF
	loopback_deregister();
#endif
	kfree(sst_drv_ctx->mmap_mem);
	free_irq(pci->irq, sst_drv_ctx);
	iounmap(sst_drv_ctx->dram);
	iounmap(sst_drv_ctx->iram);
	iounmap(sst_drv_ctx->mailbox);
	iounmap(sst_drv_ctx->shim);
	flush_scheduled_work();
	destroy_workqueue(sst_drv_ctx->process_reply_wq);
	destroy_workqueue(sst_drv_ctx->process_msg_wq);
	soc_destroy();
	destroy_workqueue(sst_drv_ctx->mad_wq);
	if (sst_drv_ctx->fw) {
		release_firmware(sst_drv_ctx->fw);
		sst_drv_ctx->fw = NULL;
	}
	kfree(pci_get_drvdata(pci));
	sst_drv_ctx = NULL;
	pci_release_regions(pci);
	pci_disable_device(pci);
	pci_set_drvdata(pci, NULL);
}

int intel_sst_set_pll(unsigned int enable, enum intel_sst_pll_mode mode)
{
	u32 *ipc_wbuf, ret = 0;
	u8 cbuf[16] = { '\0' };

	pr_debug("set_pll, Enable %x, Mode %x\n", enable, mode);
	ipc_wbuf = (u32 *)&cbuf;
	cbuf[0] = 0; /* OSC_CLK_OUT0 */

	mutex_lock(&drv_ctx_lock);
	if (enable == true) {
		cbuf[1] = 1; /* enable the clock, preserve clk ratio */
		if (sst_drv_ctx->pll_mode) {
			/* clock is on, so just update and return */
			sst_drv_ctx->pll_mode |= mode;
			goto out;
		}
		sst_drv_ctx->pll_mode |= mode;
		pr_debug("set_pll, enabling pll %x\n", sst_drv_ctx->pll_mode);
	} else {
		/* for turning off only, we check device state and turn off only
		 * when device is supspended
		 */
		sst_drv_ctx->pll_mode &= ~mode;
		pr_debug("set_pll, disabling pll %x\n", sst_drv_ctx->pll_mode);
		if (sst_drv_ctx->pll_mode)
			goto out;
		cbuf[1] = 0; /* Disable the clock */
	}
	/* send ipc command to configure the PNW clock to MSIC PLLIN */
	pr_debug("configuring clock now\n");
	ret = intel_scu_ipc_command(0xE6, 0, ipc_wbuf, 2, NULL, 0);

	if (ret)
		pr_err("ipc clk disable command failed: %d\n", ret);
out:
	mutex_unlock(&drv_ctx_lock);
	return ret;
}
EXPORT_SYMBOL(intel_sst_set_pll);

/*
 * The runtime_suspend/resume is pretty much similar to the legacy
 * suspend/resume with the noted exception below: The PCI core takes care of
 * taking the system through D3hot and restoring it back to D0 and so there is
 * no need to duplicate that here.
 */
int intel_sst_get_pll(void)
{
	u32 ret = 0;
	mutex_lock(&drv_ctx_lock);
	ret = sst_drv_ctx->pll_mode;
	mutex_unlock(&drv_ctx_lock);
	return ret;
}
EXPORT_SYMBOL(intel_sst_get_pll);

static int intel_sst_runtime_suspend(struct device *dev)
{
	union config_status_reg csr;
	enum soc_result result;

	pr_debug("runtime_suspend called\n");
	if (sst_drv_ctx->stream_cnt) {
		pr_err("active streams,not able to suspend\n");
		return -EBUSY;
	}

	/*PM for decoder pipe */
	sst_drv_ctx->codec = SST_CODEC_TYPE_UNKNOWN;

	/* Close global processor if it is allocated */
	if (SOC_BAD_HANDLE != sst_drv_ctx->proc_hdl) {
		pr_debug("soc_audio_close_processor called\n");
		result = soc_audio_close_processor(sst_drv_ctx->proc_hdl);
		if (result != SOC_SUCCESS) {
			pr_err("close processor failed\n");
			return -EIO;
		}
		sst_drv_ctx->proc_hdl = SOC_BAD_HANDLE;
	}

	/*Assert RESET on LPE Processor*/
	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	sst_drv_ctx->csr_value = csr.full;
	csr.full = csr.full | 0x2;

	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);
	intel_sst_set_pll(false, SST_PLL_AUDIO);
	sst_drv_ctx->fw_present = 0;
	return 0;
}
static int intel_sst_runtime_resume(struct device *dev)
{
	u32 csr;

	pr_debug("runtime_resume called\n");
	csr = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	/*To restore the csr_value after S0ix and S3 states
	* The value 0x30000 is to enable Lpe dram high and low addresses
	* Reference - Penwell Audio Voice Module HAS 1.61
	* Section - 13.12.1 - CSR - Configuration and Status Register*/
	csr |= (sst_drv_ctx->csr_value | 0x30000);
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr);

	intel_sst_set_pll(true, SST_PLL_AUDIO);
	return 0;
}

static int intel_sst_runtime_idle(struct device *dev)
{
	pr_debug("rtpm_idle called\n");
	if (sst_drv_ctx->stream_cnt == 0 && sst_drv_ctx->am_cnt == 0)
		pm_schedule_suspend(dev, SST_SUSPEND_DELAY);
	return -EBUSY;
}

static const struct dev_pm_ops intel_sst_pm = {
	.suspend = intel_sst_runtime_suspend,
	.resume = intel_sst_runtime_resume,
	.runtime_suspend = intel_sst_runtime_suspend,
	.runtime_resume = intel_sst_runtime_resume,
	.runtime_idle = intel_sst_runtime_idle,
};

/* PCI Routines */
static DEFINE_PCI_DEVICE_TABLE(intel_sst_ids) = {
	{PCI_VDEVICE(INTEL, SST_MFLD_PCI_ID), 5},
	{0,}
};
MODULE_DEVICE_TABLE(pci, intel_sst_ids);

static struct pci_driver driver = {
	.name = SST_DRV_NAME,
	.id_table = intel_sst_ids,
	.probe = intel_sst_probe,
	.remove = __devexit_p(intel_sst_remove),
#ifdef CONFIG_PM
	.driver = {
		.pm = &intel_sst_pm,
	},
#endif
};

/**
* intel_sst_init - Module init function
*
* Registers with PCI
* Registers with /dev
* Init all data strutures
*/
static int __init intel_sst_init(void)
{
	/* Init all variables, data structure etc....*/
	int ret = 0;
	pr_debug("INFO: ******** SST DRIVER loading.. Ver: %s\n",
				       SST_DRIVER_VERSION);

	mutex_init(&drv_ctx_lock);
	/* Register with PCI */
	ret = pci_register_driver(&driver);
	if (ret)
		pr_err("PCI register failed\n");
	return ret;
}

/**
* intel_sst_exit - Module exit function
*
* Unregisters with PCI
* Unregisters with /dev
* Frees all data strutures
*/
static void __exit intel_sst_exit(void)
{
	pci_unregister_driver(&driver);

	pr_debug("driver unloaded\n");
	return;
}

module_init(intel_sst_init);
module_exit(intel_sst_exit);

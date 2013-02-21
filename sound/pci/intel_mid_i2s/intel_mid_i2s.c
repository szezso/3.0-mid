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
  * ANY WARRANTY; without evenp the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  *
  * You should have received a copy of the GNU General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  */
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/pci_regs.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/device.h>

#include <linux/intel_mid_i2s_if.h>
#include "intel_mid_i2s.h"

MODULE_AUTHOR("Louis LE GALL <louis.le.gall intel.com>");
MODULE_DESCRIPTION("Intel MID I2S/PCM SSP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");


/*
 * structures for pci probing
 */
#ifdef CONFIG_PM
static const struct dev_pm_ops intel_mid_i2s_pm_ops = {
	.runtime_suspend = intel_mid_i2s_runtime_suspend,
	.runtime_resume = intel_mid_i2s_runtime_resume,
};
#endif
static DEFINE_PCI_DEVICE_TABLE(pci_ids) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, MFLD_SSP1_DEVICE_ID) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, MFLD_SSP0_DEVICE_ID) },
	{ 0, }, /* terminate list */
};
static struct pci_driver intel_mid_i2s_driver = {
#ifdef CONFIG_PM
	.driver = {
		.pm = &intel_mid_i2s_pm_ops,
	},
#endif
	.name = DRIVER_NAME,
	.id_table = pci_ids,
	.probe = intel_mid_i2s_probe,
	.remove = __devexit_p(intel_mid_i2s_remove),
#ifdef CONFIG_PM
	.suspend = intel_mid_i2s_driver_suspend,
	.resume = intel_mid_i2s_driver_resume,
#endif
};


/*
 * POWER MANAGEMENT FUNCTIONS
 */

#ifdef CONFIG_PM
/**
 * intel_mid_i2s_driver_suspend - driver power management suspend activity
 * @device_ptr : pointer of the device to resume
 *
 * Output parameters
 *      error : 0 means no error
 */
static int  intel_mid_i2s_driver_suspend(struct pci_dev *dev,
							pm_message_t state)
{
	struct intel_mid_i2s_hdl *drv_data = pci_get_drvdata(dev);
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return 0;
	dev_dbg(&dev->dev, "SUSPEND SSP ID %d\n", dev->device);
	drv_data->in_suspend = 1;
	pci_save_state(dev);
	pci_disable_device(dev);
	pci_set_power_state(dev, PCI_D3hot);
	return 0;
}

/**
 * intel_mid_i2s_driver_resume - driver power management suspend activity
 * @device_ptr : pointer of the device to resume
 *
 * Output parameters
 *      error : 0 means no error
 */
static int intel_mid_i2s_driver_resume(struct pci_dev *dev)
{
	int err;
	int ret = 0;
	struct intel_mid_i2s_hdl *drv_data = pci_get_drvdata(dev);

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	dev_dbg(&dev->dev, "RESUME SSP ID %d\n", dev->device);

	err = pci_enable_device(dev);
	if (err)
		dev_err(&drv_data->pdev->dev,
			"Unable to re-enable device, trying to continue.\n");
	dev_dbg(&drv_data->pdev->dev, "resuming\n");
	pci_set_power_state(dev, PCI_D0);
	pci_restore_state(dev);
	ret = pci_enable_device(dev);
	if (ret)
		dev_err(&drv_data->pdev->dev, "I2S: device can't be enabled");
	drv_data->in_suspend = 0;
	dev_dbg(&drv_data->pdev->dev, "resumed in D3\n");
	return ret;
}

/**
 * intel_mid_i2s_runtime_suspend - runtime power management suspend activity
 * @device_ptr : pointer of the device to resume
 *
 * Output parameters
 *      error : 0 means no error
 */
static int intel_mid_i2s_runtime_suspend(struct device *device_ptr)
{
	struct pci_dev *pdev;
	struct intel_mid_i2s_hdl *drv_data;

	pdev = to_pci_dev(device_ptr);
	WARN(!pdev, "Pci dev=NULL\n");
	if (!pdev)
		return -EFAULT;
	drv_data = (struct intel_mid_i2s_hdl *) pci_get_drvdata(pdev);
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	if (test_bit(I2S_PORT_OPENED, &drv_data->flags)) {
		dev_err(device_ptr,
			"Trying to suspend a device that is opened\n");
		return -ENODEV;
	}
	dev_dbg(&drv_data->pdev->dev, "Suspend of SSP requested !!\n");

	return 0;
}

/**
 * intel_mid_i2s_runtime_resume - runtime power management resume activity
 * @device_ptr : pointer of the device to resume
 *
 * Output parameters
 *      error : 0 means no error
 */
static int intel_mid_i2s_runtime_resume(struct device *device_ptr)
{
	struct pci_dev *pdev;
	struct intel_mid_i2s_hdl *drv_data;
	pdev = to_pci_dev(device_ptr);
	WARN(!pdev, "Pci dev=NULL\n");
	if (!pdev)
		return -EFAULT;
	drv_data = (struct intel_mid_i2s_hdl *) pci_get_drvdata(pdev);
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	dev_dbg(&drv_data->pdev->dev, "RT RESUME SSP ID\n");
	return 0;
}

#endif
/*
 * INTERFACE FUNCTIONS
 */

/**
 * intel_mid_i2s_flush - This is the I2S flush request
 * @drv_data : pointer on private i2s driver data (by i2s_open function)
 *
 * It will flush the TX FIFO
 * WARNING: this function is used in a Burst Mode context where it is called
 * between Bursts i.e. when there is no FMSYNC, no transfer ongoing at
 * that time
 * If you need a flush while SSP configured in I2S is BUSY and FMSYNC are
 * generated, you have to write another function
 * (loop on BUSY bit and do not limit the flush to at most 16 samples)
 *
 * Output parameters
 *      int : number of samples flushed
 */
int intel_mid_i2s_flush(struct intel_mid_i2s_hdl *drv_data)
{
	u32 sssr, data;
	u32 num = 0;
	void __iomem *reg;

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return 0;
	reg = drv_data->ioaddr;
	sssr = read_SSSR(reg);
	dev_warn(&drv_data->pdev->dev, "in flush sssr=0x%08X\n", sssr);
	/*
	 * Flush "by hand" was generating spurious DMA SERV REQUEST
	 * from SSP to DMA => then buggy retrieval of data for next dma_req
	 * Disable: RX Service Request from RX fifo to DMA
	 * as we will flush by hand
	 */
	clear_SSCR1_reg(reg, RSRE);
	/* i2s_flush is called in between 2 bursts
	 * => no FMSYNC at that time (i.e. SSP not busy)
	 * => at most 16 samples in the FIFO */
	while ((read_SSSR(reg) & (SSSR_RNE_MASK<<SSSR_RNE_SHIFT))
			&& (num < FIFO_SIZE)) {
		data = read_SSDR(reg);
		num++;
	}
	/* Enable: RX Service Request from RX fifo to DMA
	 * as flush by hand is done
	 */
	set_SSCR1_reg(reg, RSRE);
	sssr = read_SSSR(reg);
	dev_dbg(&drv_data->pdev->dev, "out flush sssr=0x%08X\n", sssr);
	return num;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_flush);

/**
 * intel_mid_i2s_get_rx_fifo_level - returns I2S rx fifo level
 * @drv_data : pointer on private i2s driver data (by i2s_open function)
 *
 * Output parameters
 *      int : Number of samples currently in the RX FIFO (negative = error)
 */
int intel_mid_i2s_get_rx_fifo_level(struct intel_mid_i2s_hdl *drv_data)
{
	u32 sssr;
	u32 rne, rfl;
	void __iomem *reg;

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	reg = drv_data->ioaddr;
	sssr = read_SSSR(reg);
	rfl = GET_SSSR_val(sssr, RFL);
	rne = GET_SSSR_val(sssr, RNE);
	if (!rne)
		return 0;
	else
		return rfl+1;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_get_rx_fifo_level);

/**
 * intel_mid_i2s_get_tx_fifo_level - returns I2S tx fifo level
 * @drv_data : pointer on private i2s driver data (by i2s_open function)
 *
 * Output parameters
 *      int : number of samples currently in the TX FIFO (negative = error)
 */
int intel_mid_i2s_get_tx_fifo_level(struct intel_mid_i2s_hdl *drv_data)
{
	u32 sssr;
	u32 tnf, tfl;
	void __iomem *reg;
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	reg = drv_data->ioaddr;
	sssr = read_SSSR(reg);
	tfl = GET_SSSR_val(sssr, TFL);
	tnf = GET_SSSR_val(sssr, TFN);
	if (!tnf)
		return 16;
	else
		return tfl;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_get_tx_fifo_level);

/**
 * intel_mid_i2s_set_rd_cb - set the callback function after read is done
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open function)
 * @read_callback : pointer of callback function
 *
 * Output parameters
 *      error : 0 means no error
 */
int intel_mid_i2s_set_rd_cb(struct intel_mid_i2s_hdl *drv_data,
					int (*read_callback)(void *param))
{
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	mutex_lock(&drv_data->mutex);
	if (!test_bit(I2S_PORT_OPENED, &drv_data->flags)) {
		dev_WARN(&drv_data->pdev->dev, "set WR CB I2S_PORT NOT_OPENED");
		mutex_unlock(&drv_data->mutex);
		return -EPERM;
	}
	/* Do not change read parameters in the middle of a READ request */
	if (test_bit(I2S_PORT_READ_BUSY, &drv_data->flags)) {
		dev_WARN(&drv_data->pdev->dev, "CB reject I2S_PORT_READ_BUSY");
		mutex_unlock(&drv_data->mutex);
		return -EBUSY;
	}
	drv_data->read_callback = read_callback;
	drv_data->read_len = 0;
	mutex_unlock(&drv_data->mutex);
	return 0;

}
EXPORT_SYMBOL_GPL(intel_mid_i2s_set_rd_cb);

/**
 * intel_mid_i2s_set_wr_cb - set the callback function after write is done
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open  function)
 * @write_callback : pointer of callback function
 *
 * Output parameters
 *      error : 0 means no error
 */
int intel_mid_i2s_set_wr_cb(struct intel_mid_i2s_hdl *drv_data,
					int (*write_callback)(void *param))
{
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	mutex_lock(&drv_data->mutex);
	if (!test_bit(I2S_PORT_OPENED, &drv_data->flags)) {
		dev_warn(&drv_data->pdev->dev, "set WR CB I2S_PORT NOT_OPENED");
		mutex_unlock(&drv_data->mutex);
		return -EPERM;
	}
	/* Do not change write parameters in the middle of a WRITE request */
	if (test_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags)) {
		dev_warn(&drv_data->pdev->dev, "CB reject I2S_PORT_WRITE_BUSY");
		mutex_unlock(&drv_data->mutex);
		return -EBUSY;
	}
	drv_data->write_callback = write_callback;
	drv_data->write_len = 0;
	mutex_unlock(&drv_data->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_set_wr_cb);

/**
 * intel_mid_i2s_enable_ssp() : start the ssp right after the read/write req
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open function)
 *
 * This enable the read/write to be started synchronously
 *
 * Output parameters
 *      error : 0 means no error
 */
int intel_mid_i2s_enable_ssp(struct intel_mid_i2s_hdl *drv_data)
{
	void __iomem *reg = drv_data->ioaddr;
	set_SSCR0_reg(reg, SSE);
	return 0;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_enable_ssp);

/**
 * intel_mid_i2s_rd_req - request a read from i2s peripheral
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open function)
 * @dst : destination buffer where the read sample should be put
 * @len : number of sample to be read (160 samples only right now)
 * @param : private context parameter to give back to read callback
 *
 * Output parameters
 *      error : 0 means no error
 */
int intel_mid_i2s_rd_req(struct intel_mid_i2s_hdl *drv_data,
				u32 *destination, size_t len, void *param)
{
	struct dma_async_tx_descriptor *rxdesc = NULL;
	struct dma_chan *rxchan = drv_data->rxchan;
	enum dma_ctrl_flags flag;
	dma_addr_t ssdr_addr;
	dma_addr_t dst;
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	if (!rxchan) {
		dev_WARN(&(drv_data->pdev->dev), "rd_req FAILED no rxchan\n");
		return -EINVAL;
	}
	if (!len) {
		dev_WARN(&drv_data->pdev->dev, "rd req invalid len=0");
		return -EINVAL;
	}

	dev_dbg(&drv_data->pdev->dev, "I2S_READ() dst=%p, len=%d, drv_data=%p",
						destination, len, drv_data);
	dst = dma_map_single(NULL, destination, len, DMA_FROM_DEVICE);
	if (!dst) {
		dev_WARN(&drv_data->pdev->dev, "can't map DMA address %p",
								destination);
		return -ENOMEM;
	}

	drv_data->read_dst = dst;
	drv_data->read_len = len;
	/* get Data Read/Write address */
	ssdr_addr = (drv_data->paddr + OFFSET_SSDR);
	set_SSCR1_reg((drv_data->ioaddr), RSRE);
	change_SSCR0_reg((drv_data->ioaddr), RIM,
			 ((drv_data->current_settings).rx_fifo_interrupt));
	flag = DMA_PREP_INTERRUPT | DMA_CTRL_ACK;
	/* Start the RX dma transfer */
	rxdesc = rxchan->device->device_prep_dma_memcpy(
			rxchan,		/* DMA Channel */
			dst,		/* DAR */
			ssdr_addr,	/* SAR */
			len,		/* Data Length */
			flag);		/* Flag */
	if (!rxdesc) {
		dev_WARN(&drv_data->pdev->dev, "can not prep dma memcpy");
		return -EFAULT;
	}
	/* Only 1 READ at a time allowed. do it at end to avoid clear&wakeup*/
	if (test_and_set_bit(I2S_PORT_READ_BUSY, &drv_data->flags)) {
		dma_unmap_single(NULL, dst, len, DMA_FROM_DEVICE);
		dev_WARN(&drv_data->pdev->dev, "RD reject I2S_PORT READ_BUSY");
		return -EBUSY;
	}
	dev_dbg(&(drv_data->pdev->dev), "RD dma tx submit\n");
	rxdesc->callback = i2s_read_done;
	drv_data->read_param = param;
	rxdesc->callback_param = drv_data;
	rxdesc->tx_submit(rxdesc);
	return 0;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_rd_req);

/**
 * intel_mid_i2s_wr_req - request a write to i2s peripheral
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open function)
 * @src : source buffer where the samples to wrote should be get
 * @len : number of sample to be read (160 samples only right now)
 * @param : private context parameter to give back to write callback
 *
 * Output parameters
 *      error : 0 means no error
 */
int intel_mid_i2s_wr_req(struct intel_mid_i2s_hdl *drv_data, u32 *source,
						size_t len, void *param)
{
	dma_addr_t ssdr_addr;
	struct dma_async_tx_descriptor *txdesc = NULL;
	struct dma_chan *txchan = drv_data->txchan;
	enum dma_ctrl_flags flag;
	dma_addr_t src;
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return -EFAULT;
	if (!txchan) {
		dev_WARN(&(drv_data->pdev->dev), "wr_req but no txchan\n");
		return -EINVAL;
	}
	if (!len) {
		dev_WARN(&drv_data->pdev->dev, "invalid len 0");
		return -EINVAL;
	}

	dev_dbg(&drv_data->pdev->dev,
			"I2S_WRITE() src=%p, len=%d, drv_data=%p",
							source, len, drv_data);

	src = dma_map_single(NULL, source, len, DMA_TO_DEVICE);
	if (!src) {
		dev_WARN(&drv_data->pdev->dev, "can't map DMA address %p",
								source);
		return -EFAULT;
	}
	drv_data->write_src = src;
	drv_data->write_len = len;
	/* get Data Read/Write address */
	ssdr_addr = (dma_addr_t)(u32)(drv_data->paddr + OFFSET_SSDR);
	set_SSCR1_reg((drv_data->ioaddr), TSRE);
	change_SSCR0_reg((drv_data->ioaddr), TIM,
			 ((drv_data->current_settings).tx_fifo_interrupt));
	flag = DMA_PREP_INTERRUPT | DMA_CTRL_ACK;
	txdesc = txchan->device->device_prep_dma_memcpy(
			txchan,		/* DMA Channel */
			ssdr_addr,	/* DAR */
			src,		/* SAR */
			len,		/* Data Length */
			flag);		/* Flag */
	if (!txdesc) {
		dev_WARN(&(drv_data->pdev->dev),
			"wr_req dma memcpy FAILED(src=%08x,len=%d,txchan=%p)\n",
			(unsigned int) src, len, txchan);
		return -1;
	}
	dev_dbg(&(drv_data->pdev->dev), "WR dma tx summit\n");
	/* Only 1 WRITE at a time allowed */
	if (test_and_set_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags)) {
		dma_unmap_single(NULL, src, len, DMA_TO_DEVICE);
		dev_WARN(&drv_data->pdev->dev, "WR reject I2S_PORT WRITE_BUSY");
		return -EBUSY;
	}
	txdesc->callback = i2s_write_done;
	drv_data->write_param = param;
	txdesc->callback_param = drv_data;
	txdesc->tx_submit(txdesc);
	dev_dbg(&(drv_data->pdev->dev), "wr dma req programmed\n");
	return 0;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_wr_req);

/**
 * intel_mid_i2s_open - reserve and start a SSP depending of it's usage
 * @usage : select which ssp i2s you need by giving usage (BT,MODEM...)
 * @ps_settings : hardware settings to configure the SSP module
 *
 * May sleep (driver_find_device) : no lock permitted when called.
 *
 * Output parameters
 *      handle : handle of the selected SSP, or NULL if not found
 */
struct intel_mid_i2s_hdl *intel_mid_i2s_open(enum intel_mid_i2s_ssp_usage usage,
			 const struct intel_mid_i2s_settings *ps_settings)
{
	struct pci_dev *pdev;
	struct intel_mid_i2s_hdl *drv_data = NULL;
	struct device *found_device = NULL;

	pr_debug("%s : open called,searching for device with usage=%x !\n",
							DRIVER_NAME, usage);
	found_device = driver_find_device(&(intel_mid_i2s_driver.driver),
						NULL, &usage, check_device);
	if (!found_device) {
		pr_debug("%s : open can not found with usage=0x%02X\n",
						DRIVER_NAME, (int)usage);
		return NULL;
	}
	pdev = to_pci_dev(found_device);
	drv_data = pci_get_drvdata(pdev);
	drv_data->current_settings = *ps_settings;

	if (!drv_data) {
		dev_err(found_device, "no drv_data in open pdev=%p\n", pdev);
		put_device(found_device);
		return NULL;
	}
	mutex_lock(&drv_data->mutex);
	dev_dbg(found_device, "Open found pdevice=0x%p\n", pdev);
	/* pm_runtime */
	pm_runtime_get_sync(&drv_data->pdev->dev);
	/* dmac1 is already set during probe */
	if (i2s_dma_start(drv_data) != 0) {
		dev_err(found_device, "Can not start DMA\n");
		goto open_error;
	}
	/* if we restart after stop without suspend, we start ssp faster */
	set_ssp_i2s_hw(drv_data, ps_settings);

	drv_data->mask_sr = ((SSSR_BCE_MASK << SSSR_BCE_SHIFT) |
			(SSSR_EOC_MASK << SSSR_EOC_SHIFT) |
			(SSSR_ROR_MASK << SSSR_ROR_SHIFT) |
			(SSSR_TUR_MASK << SSSR_TUR_SHIFT) |
			(SSSR_TINT_MASK << SSSR_TINT_SHIFT) |
			(SSSR_PINT_MASK << SSSR_PINT_SHIFT));
	if (test_bit(I2S_PORT_CLOSING, &drv_data->flags)) {
		dev_err(&drv_data->pdev->dev, "Opening a closing I2S!");
		goto open_error;
	}
	/* there is no need to "wake up" as we can not close an opening i2s */
	clear_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags);
	clear_bit(I2S_PORT_READ_BUSY, &drv_data->flags);
	mutex_unlock(&drv_data->mutex);
	return drv_data;

open_error:
	pm_runtime_put(&drv_data->pdev->dev);
	put_device(found_device);
	mutex_unlock(&drv_data->mutex);
	return NULL;
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_open);

/**
 * intel_mid_i2s_close - release and stop the SSP
 * @drv_data : handle of corresponding ssp i2s (given by i2s_open function)
 *
 * WARNING: This is not -yet- allowed to call close from a read/write callback !
 *
 * Output parameters
 *      none
 */
void intel_mid_i2s_close(struct intel_mid_i2s_hdl *drv_data)
{
	void __iomem *reg;
	struct intel_mid_i2s_settings *ssp_settings =
						&(drv_data->current_settings);
	int s;
	struct dma_chan *channel;

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return;
	mutex_lock(&drv_data->mutex);
	if (!test_bit(I2S_PORT_OPENED, &drv_data->flags)) {
		dev_err(&drv_data->pdev->dev, "not opened but closing?");
		mutex_unlock(&drv_data->mutex);
		return;
	}

	set_bit(I2S_PORT_CLOSING, &drv_data->flags);
	dev_info(&drv_data->pdev->dev, "Status bit pending write=%d read=%d\n",
			test_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags),
			test_bit(I2S_PORT_READ_BUSY, &drv_data->flags));
	if (test_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags) ||
	     test_bit(I2S_PORT_READ_BUSY, &drv_data->flags)) {
		dev_info(&drv_data->pdev->dev, "Pending callback in close...\n");
	}

	if (ssp_settings->ssp_active_tx_slots_map) {
		channel = drv_data->txchan;
		s = channel->device->device_control(channel,
							DMA_TERMINATE_ALL, 0);
		dev_info(&drv_data->pdev->dev, "DMA_TERMINATE tx=%d\n", s);
	}
	if (ssp_settings->ssp_active_rx_slots_map) {
		channel = drv_data->rxchan;
		s = channel->device->device_control(channel,
							DMA_TERMINATE_ALL, 0);
		dev_info(&drv_data->pdev->dev, "DMA_TERMINATE rx=%d\n", s);
	}

	/* release DMA Channel.. */
	i2s_dma_stop(drv_data);
	reg = drv_data->ioaddr;
	dev_dbg(&drv_data->pdev->dev, "Stopping the SSP\n");
	i2s_ssp_stop(drv_data);
	put_device(&drv_data->pdev->dev);
	write_SSCR0(0, reg);

	dev_dbg(&(drv_data->pdev->dev), "SSP Stopped.\n");
	clear_bit(I2S_PORT_CLOSING, &drv_data->flags);
	clear_bit(I2S_PORT_OPENED, &drv_data->flags);

	/* pm runtime */
	pm_runtime_put(&drv_data->pdev->dev);

	mutex_unlock(&drv_data->mutex);
}
EXPORT_SYMBOL_GPL(intel_mid_i2s_close);
/*
 * INTERNAL FUNCTIONS
 */

/**
 * check_device -  return if the device is the usage we want (usage =*data)
 * @device_ptr : pointer on device struct
 * @data : pointer pointer on usage we are looking for
 *
 * this is called for each device by find_device() from intel_mid_i2s_open()
 * Info : when found, the flag of driver is set to I2S_PORT_OPENED
 *
 * Output parameters
 *      integer : return 0 means not the device or already started. go next
 *		  return != 0 means stop the search and return this device
 */
static int
check_device(struct device *device_ptr, void *data)
{
	struct pci_dev *pdev;
	struct intel_mid_i2s_hdl *drv_data;
	enum intel_mid_i2s_ssp_usage usage;
	enum intel_mid_i2s_ssp_usage usage_to_find;

	pdev = to_pci_dev(device_ptr);
	WARN(!pdev, "Pci device=NULL\n");
	if (!pdev)
		return 0;
	drv_data = (struct intel_mid_i2s_hdl *) pci_get_drvdata(pdev);
	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return 0;
	dev_dbg(&(pdev->dev), "Check device pci_dev ptr = 0X%p\n", pdev);
	usage_to_find = *((enum intel_mid_i2s_ssp_usage *) data);
	usage = drv_data->usage;

	/* Can be done in one "if" but separated in purpose : take care of
	 * test_and_set_bit that need to be done AFTER the check on usage.
	 */
	if (usage == usage_to_find) {
		if (!test_and_set_bit(I2S_PORT_OPENED, &drv_data->flags))
			return 1;  /* Already opened, do not use this result */
	};
	return 0; /* not usage we look for, or already opened */
}

/**
 * i2s_read_done - callback from the _dma tasklet_ after read
 * @arg : void pointer to that should be driver data (context)
 *
 * Output parameters
 *      none
 */
static void i2s_read_done(void *arg)
{
	int status = 0;

	struct intel_mid_i2s_hdl *drv_data = arg;
	void *param_complete;
	void __iomem *reg ;

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return;
	if (!test_bit(I2S_PORT_READ_BUSY, &drv_data->flags))
		dev_WARN(&drv_data->pdev->dev, "spurious read dma complete");

	dma_unmap_single(NULL, drv_data->read_dst,
			 drv_data->read_len, DMA_FROM_DEVICE);
	drv_data->read_len = 0;
	reg = drv_data->ioaddr;
	/* Rx fifo overrun Interrupt */
	change_SSCR0_reg(reg, RIM, SSP_RX_FIFO_OVER_INT_DISABLE);
	param_complete = drv_data->read_param;
	/* Do not change order sequence:
	 * READ_BUSY clear, then test PORT_CLOSING
	 * wakeup for close() function
	 */
	clear_bit(I2S_PORT_READ_BUSY, &drv_data->flags);
	wake_up(&drv_data->wq_chan_closing);
	if (test_bit(I2S_PORT_CLOSING, &drv_data->flags))
		return;
	if (drv_data->read_callback != NULL)
		status = drv_data->read_callback(param_complete);
	else
		dev_warn(&drv_data->pdev->dev, "RD done but not callback set");

}

/**
 * i2s_write_done() : callback from the _dma tasklet_ after write
 * @arg : void pointer to that should be driver data (context)
 *
 * Output parameters
 *      none
 */
static void i2s_write_done(void *arg)
{
	int status = 0;
	void *param_complete;
	struct intel_mid_i2s_hdl *drv_data = arg;
	void __iomem *reg ;

	WARN(!drv_data, "Driver data=NULL\n");
	if (!drv_data)
		return;
	if (!test_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags))
		dev_warn(&drv_data->pdev->dev, "spurious write dma complete");
	dma_unmap_single(NULL, drv_data->read_dst,
			 drv_data->read_len, DMA_TO_DEVICE);
	drv_data->read_len = 0;
	reg = drv_data->ioaddr;
	change_SSCR0_reg(reg, TIM, SSP_TX_FIFO_UNDER_INT_DISABLE);
	dev_dbg(&(drv_data->pdev->dev), "DMA channel disable..\n");
	param_complete = drv_data->write_param;
	/* Do not change order sequence:
	 * WRITE_BUSY clear, then test PORT_CLOSING
	 * wakeup for close() function
	 */
	clear_bit(I2S_PORT_WRITE_BUSY, &drv_data->flags);
	wake_up(&drv_data->wq_chan_closing);
	if (test_bit(I2S_PORT_CLOSING, &drv_data->flags))
		return;
	if (drv_data->write_callback != NULL)
		status = drv_data->write_callback(param_complete);
	else
		dev_warn(&drv_data->pdev->dev, "WR done but no callback set");
}

static bool chan_filter(struct dma_chan *chan, void *param)
{
	struct intel_mid_i2s_hdl *drv_data = (struct intel_mid_i2s_hdl *)param;
	bool ret = false;

	if (!drv_data->dmac1)
		goto out;
	if (chan->device->dev == &drv_data->dmac1->dev)
		ret = true;
out:
	return ret;
}
static int i2s_compute_dma_width(u16 ssp_data_size,
					enum dma_slave_buswidth *dma_width)
{
	if (ssp_data_size <= 8)
		*dma_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	else if (ssp_data_size <= 16)
		*dma_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	else if (ssp_data_size <= 32)
		*dma_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	else
		return -EINVAL;


	return 0;
}
static int i2s_compute_dma_msize(u8 ssp_threshold,
					enum intel_mid_dma_msize *dma_msize)
{

	switch (ssp_threshold) {
	case 1:
		*dma_msize = LNW_DMA_MSIZE_1;
		break;
	case 4:
		*dma_msize = LNW_DMA_MSIZE_4;
		break;
	case 8:
		*dma_msize = LNW_DMA_MSIZE_8;
		break;
	case 16:
		*dma_msize = LNW_DMA_MSIZE_16;
		break;
	case 32:
		*dma_msize = LNW_DMA_MSIZE_32;
		break;
	case 64:
		*dma_msize = LNW_DMA_MSIZE_64;
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

/**
 * i2s_dma_start - prepare and reserve dma channels
 * @arg : intel_mid_i2s_hdl pointer to that should be driver data (context)
 *
 * "ssp open" context and dmac1 should already be filled in drv_data
 *
 * Output parameters
 *      int : should be zero, else it means error code
 */
static int i2s_dma_start(struct intel_mid_i2s_hdl *drv_data)
{
	struct intel_mid_dma_slave *rxs, *txs;
	struct pci_dev *l_pdev;
	struct intel_mid_i2s_settings *ssp_settings =
						&(drv_data->current_settings);
	dma_cap_mask_t mask;
	int retval = 0;
	int temp = 0;

	dev_dbg(&drv_data->pdev->dev, "DMAC1 start\n");
	drv_data->txchan = NULL;
	drv_data->rxchan = NULL;
	l_pdev = drv_data->pdev;

	if (ssp_settings->ssp_active_rx_slots_map) {
		/* 1. init rx channel */
		rxs = &drv_data->dmas_rx;
		rxs->dma_slave.direction = DMA_FROM_DEVICE;
		rxs->hs_mode = LNW_DMA_HW_HS;
		rxs->cfg_mode = LNW_DMA_PER_TO_MEM;
		temp = i2s_compute_dma_width(ssp_settings->data_size,
					&rxs->dma_slave.src_addr_width);

		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"RX DMA Channel Bad data_size = %d\n",
				ssp_settings->data_size);
			retval = -1;
			goto err_exit;

		}
		rxs->dma_slave.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

		temp = i2s_compute_dma_msize(
					ssp_settings->ssp_rx_fifo_threshold,
					&rxs->dma_slave.src_maxburst);
		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"RX DMA Channel Bad RX FIFO Threshold src= %d\n",
				ssp_settings->ssp_rx_fifo_threshold);
			retval = -2;
			goto err_exit;

		}

		temp = i2s_compute_dma_msize(
					ssp_settings->ssp_rx_fifo_threshold,
					&rxs->dma_slave.dst_maxburst);
		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"RX DMA Channel Bad RX FIFO Threshold dst= %d\n",
				ssp_settings->ssp_rx_fifo_threshold);
			retval = -3;
			goto err_exit;

		}

		rxs->device_instance = drv_data->device_instance;
		dma_cap_zero(mask);
		dma_cap_set(DMA_MEMCPY, mask);
		dma_cap_set(DMA_SLAVE, mask);
		drv_data->rxchan = dma_request_channel(mask,
							chan_filter, drv_data);
		if (!drv_data->rxchan) {
			dev_err(&(drv_data->pdev->dev),
				"Could not get Rx channel\n");
			retval = -4;
			goto err_exit;
		}

		temp = drv_data->rxchan->device->device_control(
					drv_data->rxchan, DMA_SLAVE_CONFIG,
					(unsigned long) &rxs->dma_slave);
		if (temp) {
			dev_err(&(drv_data->pdev->dev),
				"Rx slave control failed\n");
			retval = -5;
			goto err_exit;
		}

	}

	if (ssp_settings->ssp_active_tx_slots_map) {
		/* 2. init tx channel */
		txs = &drv_data->dmas_tx;
		txs->dma_slave.direction = DMA_TO_DEVICE;
		txs->hs_mode = LNW_DMA_HW_HS;
		txs->cfg_mode = LNW_DMA_MEM_TO_PER;

		txs->dma_slave.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

		temp = i2s_compute_dma_width(ssp_settings->data_size,
					&txs->dma_slave.dst_addr_width);
		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"TX DMA Channel Bad data_size = %d\n",
				ssp_settings->data_size);
			retval = -6;
			goto err_exit;

		}

		temp = i2s_compute_dma_msize(
				ssp_settings->ssp_tx_fifo_threshold + 1,
				&txs->dma_slave.src_maxburst);
		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"TX DMA Channel Bad TX FIFO Threshold src= %d\n",
				ssp_settings->ssp_tx_fifo_threshold);
			retval = -7;
			goto err_exit;

		}

		temp = i2s_compute_dma_msize(
				ssp_settings->ssp_tx_fifo_threshold + 1,
						&txs->dma_slave.dst_maxburst);
		if (temp != 0) {
			dev_err(&(drv_data->pdev->dev),
				"TX DMA Channel Bad TX FIFO Threshold dst= %d\n",
				ssp_settings->ssp_tx_fifo_threshold);
			retval = -8;
			goto err_exit;

		}

		txs->device_instance = drv_data->device_instance;
		dma_cap_set(DMA_SLAVE, mask);
		dma_cap_set(DMA_MEMCPY, mask);
		drv_data->txchan = dma_request_channel(mask,
							chan_filter, drv_data);

		if (!drv_data->txchan) {
			dev_err(&(drv_data->pdev->dev),
				"Could not get Tx channel\n");
			retval = -10;
			goto err_exit;
		}

		temp = drv_data->txchan->device->device_control(
					drv_data->txchan, DMA_SLAVE_CONFIG,
					(unsigned long) &txs->dma_slave);
		if (temp) {
			dev_err(&(drv_data->pdev->dev),
				"Tx slave control failed\n");
			retval = -9;
			goto err_exit;
		}
	}

	return retval;

err_exit:
	if (drv_data->txchan)
		dma_release_channel(drv_data->txchan);
	if (drv_data->rxchan)
		dma_release_channel(drv_data->rxchan);
	drv_data->rxchan = NULL;
	drv_data->txchan = NULL;
	return retval;
}

/**
 * i2s_dma_stop - release dma channels
 * @arg : struct intel_mid_i2s_hdl pointer to that should be driver data (context)
 *
 * called by intel_mid_i2s_close() context
 *
 * Output parameters
 *      none
 */
static void i2s_dma_stop(struct intel_mid_i2s_hdl *drv_data)
{
	struct intel_mid_i2s_settings *ssp_settings =
					&(drv_data->current_settings);

	dev_dbg(&drv_data->pdev->dev, "DMAC1 stop\n");
	if (ssp_settings->ssp_active_rx_slots_map)
		dma_release_channel(drv_data->rxchan);
	if (ssp_settings->ssp_active_tx_slots_map)
		dma_release_channel(drv_data->txchan);

}

static void i2s_ssp_stop(struct intel_mid_i2s_hdl *drv_data)
{
	void __iomem *reg = drv_data->ioaddr;
	dev_dbg(&drv_data->pdev->dev, "Stop SSP\n");
	clear_SSCR0_reg(reg, SSE);
}

static void ssp1_dump_registers(struct intel_mid_i2s_hdl *drv_data)
{
	u32 irq_status;
	void __iomem *reg = drv_data->ioaddr;
	struct device *ddbg = &(drv_data->pdev->dev);
	u32 status;
	irq_status = read_SSSR(reg);
	dev_dbg(ddbg, "dump SSSR=0x%08X\n", irq_status);
	status = read_SSCR0(reg);
	dev_dbg(ddbg, "dump SSCR0=0x%08X\n", status);
	status = read_SSCR1(reg);
	dev_dbg(ddbg, "dump SSCR1=0x%08X\n", status);
	status = read_SSPSP(reg);
	dev_dbg(ddbg, "dump SSPSP=0x%08X\n", status);
	status = read_SSTSA(reg);
	dev_dbg(ddbg, "dump SSTSA=0x%08X\n", status);
	status = read_SSRSA(reg);
	dev_dbg(ddbg, "dump SSRSA=0x%08X\n", status);
	status = read_SSTO(reg);
	dev_dbg(ddbg, "dump SSTO=0x%08X\n", status);
	status = read_SSITR(reg);
	dev_dbg(ddbg, "dump SSITR=0x%08X\n", status);
	status = read_SSTSS(reg);
	dev_dbg(ddbg, "dump SSTSS=0x%08X\n", status);
	status = read_SSACD(reg);
	dev_dbg(ddbg, "dump SSACD=0x%08X\n", status);
}

/**
 * i2s_int(): function that handles the SSP Interrupts (errors)
 * @irq : IRQ Number
 * @dev_id : structure that contains driver information
 *
 * This interrupts do nothing but warnings in case there is some problems
 * in I2S connection (underruns, overruns...). This may be reported by adding a
 * new interface to the driver, but not yet requested by "users" of this driver
 *
 * Output parameters
 *      NA
 */
static irqreturn_t i2s_int(int irq, void *dev_id)
{
	struct intel_mid_i2s_hdl *drv_data = dev_id;
	void __iomem *reg;
	u32 irq_status = 0;
	u32 mask_status = 0;
	struct device *ddbg = &(drv_data->pdev->dev);


	if (drv_data->in_suspend)
		return IRQ_NONE;
#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_suspended(ddbg))
		return IRQ_NONE;
#endif

	reg = drv_data->ioaddr;
	irq_status = read_SSSR(reg);

	if (!(irq_status & (drv_data->mask_sr))) {
		return IRQ_NONE;
	} else {
		/* may be improved by using a tasklet to send the error
		 * (underrun,...) to client by using callback
		 */
		if (irq_status & (SSSR_ROR_MASK << SSSR_ROR_SHIFT)) {
			dev_warn(ddbg,
				"ssp_int RX FIFO OVER RUN SSSR=0x%08X\n",
				irq_status);
			mask_status |= (SSSR_ROR_MASK << SSSR_ROR_SHIFT);

		}
		if (irq_status & (SSSR_TUR_MASK << SSSR_TUR_SHIFT)) {
			dev_warn(ddbg,
				"ssp_int TX FIFO UNDER RUN SSSR=0x%08X\n",
				irq_status);
			mask_status |= (SSSR_TUR_MASK << SSSR_TUR_SHIFT);

		}
		if (irq_status & (SSSR_TINT_MASK << SSSR_TINT_SHIFT)) {
			dev_warn(ddbg,
				"ssp_int RX TIME OUT SSSR=0x%08X\n",
				irq_status);
			mask_status |= (SSSR_TINT_MASK << SSSR_TINT_SHIFT);

		}
		if (irq_status & (SSSR_PINT_MASK << SSSR_PINT_SHIFT)) {
			dev_warn(ddbg,
				"ssp_int TRAILING BYTE SSSR=0x%08X\n",
				irq_status);
			mask_status |= (SSSR_PINT_MASK << SSSR_PINT_SHIFT);
		}
		if (irq_status & (SSSR_EOC_MASK << SSSR_EOC_SHIFT)) {
			dev_warn(ddbg,
				"ssp_int END OF CHAIN SSSR=0x%08X\n",
				irq_status);
			mask_status |= (SSSR_EOC_MASK << SSSR_EOC_SHIFT);
		}
		/* clear sticky bits */
		write_SSSR((irq_status & mask_status), reg);
	}
	return IRQ_HANDLED;
}

/**
 * calculate_sspsp_psp - separate function that calculate sspsp register
 * @ps_settings : pointer of the settings struct
 *
 * this function is to simplify/clarify set_ssp_i2s_hw function
 *
 *
 * Output parameters
 *      u32 : calculated SSPSP register
 */
u32 calculate_sspsp_psp(const struct intel_mid_i2s_settings *ps_settings)
{
	u32 sspsp;
	sspsp = SSPSP_reg(FSRT,	ps_settings->ssp_frmsync_timing_bit)
		|SSPSP_reg(ETDS,	ps_settings->ssp_end_transfer_state)
		|SSPSP_reg(SCMODE,	ps_settings->ssp_serial_clk_mode)
		|SSPSP_reg(DMYSTOP,	ps_settings->ssp_psp_T4)
		|SSPSP_reg(SFRMDLY,	ps_settings->ssp_psp_T5)
		|SSPSP_reg(SFRMWDTH,	ps_settings->ssp_psp_T6)
		|SSPSP_reg(SFRMP,	ps_settings->ssp_frmsync_pol_bit);
	return sspsp;
}

/*
 * calculate_sscr0_psp: separate function that calculate sscr0 register
 * @ps_settings : pointer of the settings struct
 *
 * this function is to simplify/clarify set_ssp_i2s_hw function
 *
 * Output parameters
 *      u32 : calculated SSCR0 register
 */
u32 calculate_sscr0_psp(const struct intel_mid_i2s_settings *ps_settings)
{
	u16 l_ssp_data_size = ps_settings->data_size;
	u32 sscr0;
	if (l_ssp_data_size > 16) {
		sscr0 =   SSCR0_reg(DSS, SSCR0_DataSize(l_ssp_data_size - 16))
			| SSCR0_reg(EDSS, 1);
	} else {
		sscr0 =   SSCR0_reg(DSS, SSCR0_DataSize(l_ssp_data_size))
			| SSCR0_reg(EDSS, 0);
	}
/*
Can be replaced by code below :
sscr0 = SSCR0_reg(DSS, (l_ssp_data_size - 1) & 0x0F)
| SSCR0_reg(EDSS, ((l_ssp_data_size - 1) & 0x10) >> 8);
*/
	sscr0 |= SSCR0_reg(MOD,	ps_settings->mode)
		|SSCR0_reg(FRF,	ps_settings->frame_format)
		|SSCR0_reg(RIM,	SSP_RX_FIFO_OVER_INT_DISABLE)
		|SSCR0_reg(TIM,	SSP_TX_FIFO_UNDER_INT_DISABLE);
	return sscr0;
}

/**
 * calculate_sscr1_psp - separate function that calculate sscr1 register
 * @ps_settings : pointer of the settings struct
 *
 * this function is to simplify/clarify set_ssp_i2s_hw function
 *
 * Output parameters
 *      u32 : calculated SSCR1 register
 */
u32 calculate_sscr1_psp(const struct intel_mid_i2s_settings *ps_settings)
{
	u32 sscr1;
	sscr1 = SSCR1_reg(SFRMDIR, ps_settings->sspsfrm_direction)
		|SSCR1_reg(SCLKDIR, ps_settings->sspslclk_direction)
		|SSCR1_reg(TTELP, ps_settings->tx_tristate_phase)
		|SSCR1_reg(TTE,	ps_settings->tx_tristate_enable)
		|SSCR1_reg(TRAIL, ps_settings->ssp_trailing_byte_mode)
		|SSCR1_reg(TINTE, ps_settings->ssp_rx_timeout_interrupt_status)
		|SSCR1_reg(PINTE,
			ps_settings->ssp_trailing_byte_interrupt_status)
		|SSCR1_reg(LBM,	ps_settings->ssp_loopback_mode_status)
		|SSCR1_reg(RWOT, ps_settings->ssp_duplex_mode)
		|SSCR1_reg(RFT,
			SSCR1_RxTresh(ps_settings->ssp_rx_fifo_threshold))
		|SSCR1_reg(TFT,
			SSCR1_TxTresh(ps_settings->ssp_tx_fifo_threshold));
	return sscr1;
}

/**
 * set_ssp_i2s_hw - configure the SSP driver according to the ps_settings
 * @drv_data : structure that contains all details about the SSP Driver
 * @ps_settings : structure that contains SSP Hardware settings
 *
 * it also store ps_settings the drv_data
 *
 * Output parameters
 *      NA
 */
static void set_ssp_i2s_hw(struct intel_mid_i2s_hdl *drv_data,
			const struct intel_mid_i2s_settings *ps_settings)
{
	u32 sscr0 = 0;
	u32 sscr1 = 0;
	u32 sstsa = 0;
	u32 ssrsa = 0;
	u32 sspsp = 0;
	u32 sssr = 0;
	/* Get the SSP Settings */
	u16 l_ssp_clk_frm_mode = 0xFF;
	void __iomem *reg = drv_data->ioaddr;
	struct device *ddbg = &(drv_data->pdev->dev);
	dev_dbg(ddbg,
		"setup SSP I2S PCM1 configuration\n");
	if ((ps_settings->sspsfrm_direction == SSPSFRM_MASTER_MODE)
	   && (ps_settings->sspslclk_direction == SSPSCLK_MASTER_MODE)) {
		l_ssp_clk_frm_mode = SSP_IN_MASTER_MODE;
	} else if ((ps_settings->sspsfrm_direction == SSPSFRM_SLAVE_MODE)
	   && (ps_settings->sspslclk_direction == SSPSCLK_SLAVE_MODE)) {
		l_ssp_clk_frm_mode = SSP_IN_SLAVE_MODE;
	} else {
		dev_err(ddbg, "Unsupported I2S PCM1 configuration\n");
		goto leave;
	}
	dev_dbg(ddbg, "SSPSFRM_DIRECTION:%d:\n",
		ps_settings->sspsfrm_direction);
	dev_dbg(ddbg, "SSPSCLK_DIRECTION:%d:\n",
		ps_settings->sspslclk_direction);
	if (ps_settings->frame_format != PSP_FORMAT) {
		dev_err(ddbg, "UNSUPPORTED FRAME FORMAT:%d:\n",
						ps_settings->frame_format);
		goto leave;
	}
	if ((ps_settings->ssp_tx_dma != SSP_TX_DMA_ENABLE)
	|| (ps_settings->ssp_rx_dma != SSP_RX_DMA_ENABLE)) {
		dev_err(ddbg, "ONLY DMA MODE IS SUPPORTED");
		goto leave;
	}
	/*********** DMA Transfer Mode ***********/
	dev_dbg(ddbg, "FORMAT :%d:\n", ps_settings->frame_format);
	sscr0 = calculate_sscr0_psp(ps_settings);
	dev_dbg(ddbg, " sscr0 :0x%08X\n", sscr0);
	sscr1 = calculate_sscr1_psp(ps_settings);
	dev_dbg(ddbg, " sscr1 :0x%08X\n", sscr1);
	if (ps_settings->mode == SSP_IN_NETWORK_MODE) {
		dev_dbg(ddbg, "MODE :%d:\n", ps_settings->mode);
		sscr0 |= SSCR0_reg(FRDC,
			SSCR0_SlotsPerFrm(ps_settings->
						frame_rate_divider_control));
		dev_dbg(ddbg, "sscr0 :0x%08X\n", sscr0);
		sspsp = calculate_sspsp_psp(ps_settings);
		dev_dbg(ddbg, "sspsp :0x%08X\n", sspsp);
		/* set the active TX time slot (bitmap) */
		sstsa = SSTSA_reg(TTSA, ps_settings->ssp_active_tx_slots_map);
		/* set the active RX time slot (bitmap) */
		ssrsa = SSRSA_reg(RTSA, ps_settings->ssp_active_rx_slots_map);
		if (l_ssp_clk_frm_mode == SSP_IN_MASTER_MODE) {
			switch (ps_settings->master_mode_clk_selection) {
			case SSP_ONCHIP_CLOCK:
				break;
			case SSP_NETWORK_CLOCK:
				sscr0 |= SSCR0_reg(NCS, 1);
				break;
			case SSP_EXTERNAL_CLOCK:
				sscr0 |= SSCR0_reg(ECS, 1);
				break;
			case SSP_ONCHIP_AUDIO_CLOCK:
				sscr0 |= SSCR0_reg(ACS, 1);
				break;
			default:
				dev_err(ddbg, "Master Mode clk selection UNKNOWN");
				break;
			}
			sspsp |= SSPSP_reg(STRTDLY, ps_settings->ssp_psp_T1)
				|SSPSP_reg(DMYSTRT, ps_settings->ssp_psp_T2);
		} else {	/* Set the Slave Clock Free Running Status */
			sscr1 |= SSCR1_reg(SCFR,
				ps_settings->slave_clk_free_running_status);
		}
	} else {  /* SSP_IN_NORMAL_MODE */
		dev_err(ddbg, "UNSUPPORTED MODE");
		goto leave;
	}

	/* Clear status */
	sssr = (SSSR_BCE_MASK << SSSR_BCE_SHIFT)
	     | (SSSR_TUR_MASK << SSSR_TUR_SHIFT)
	     | (SSSR_TINT_MASK << SSSR_TINT_SHIFT)
	     | (SSSR_PINT_MASK << SSSR_PINT_SHIFT)
	     | (SSSR_ROR_MASK << SSSR_ROR_SHIFT);
	/* disable SSP */
	clear_SSCR0_reg(reg, SSE);
	dev_dbg(ddbg, "WRITE SSCR0 DISABLE\n");
	/* Clear status */
	write_SSSR(sssr, reg);
	dev_dbg(ddbg, "WRITE SSSR: 0x%08X\n", sssr);
	write_SSCR0(sscr0, reg);
	dev_dbg(ddbg, "WRITE SSCR0\n");
	/* first set CR1 without interrupt and service enables */
	write_SSCR1(sscr1, reg);
	write_SSPSP(sspsp, reg);
	write_SSTSA(sstsa, reg);
	write_SSRSA(ssrsa, reg);
	/* set the time out for the reception */
	write_SSTO(0, reg);
	ssp1_dump_registers(drv_data);
leave:
	return;
}

static int
intel_mid_i2s_find_usage(struct pci_dev *pdev,
			 struct intel_mid_i2s_hdl *drv_data,
			 enum intel_mid_i2s_ssp_usage *usage)
{
	int pos;
	u8  adid;
	int status = 0;

	*usage = SSP_USAGE_UNASSIGNED;
	pos = pci_find_capability(pdev, PCI_CAP_ID_VNDR);
	dev_info((&pdev->dev),
		"Probe/find capability (VNDR %d pos=0x%x)\n",
		PCI_CAP_ID_VNDR, pos);
	if (pos > 0) {
		pos += PCI_CAP_OFFSET_ADID;
		pci_read_config_byte(pdev, pos, &adid);
		dev_info(&(pdev->dev), "Vendor capability adid = 0x%x\n", adid);
		if (adid == PCI_CAP_ADID_I2S_BT_FM)
			*usage	= SSP_USAGE_BLUETOOTH_FM;
		else if (adid == PCI_CAP_ADID_I2S_MODEM)
			*usage	= SSP_USAGE_MODEM;
		else
			*usage	= SSP_USAGE_UNASSIGNED;
	}
	/* If there is no capability, check with old PCI_ID */
#ifdef BYPASS_ADID
	if (*usage == SSP_USAGE_UNASSIGNED) {
		dev_warn(&(pdev->dev), "Vendor capability not present/invalid\n");
		switch (pdev->device) {
		case MFLD_SSP1_DEVICE_ID:
			*usage	= SSP_USAGE_BLUETOOTH_FM;
			break;
		case MFLD_SSP0_DEVICE_ID:
			*usage	= SSP_USAGE_MODEM;
			break;
		}
	}
#endif
	if (*usage == SSP_USAGE_UNASSIGNED) {
		dev_info((&pdev->dev),
			"No probe for I2S PCI-ID: %04x:%04x, ADID(0x%x)=0x%x\n",
			pdev->vendor, pdev->device, pos, adid);
		status = -ENODEV;
		goto err_find_usage;
	}
	dev_dbg(&(pdev->dev),
		"Detected PCI SSP (ID: %04x:%04x) usage =%x\n",
		pdev->vendor, pdev->device, *usage);
	dev_dbg(&(pdev->dev),
		" found PCI SSP controller(ID: %04x:%04x)\n",
		pdev->vendor, pdev->device);
	/* Init the driver data structure fields*/
	switch (pdev->device) {
	case MFLD_SSP1_DEVICE_ID:
		drv_data->device_instance = DMA1C_DEVICE_INSTANCE_SSP1;
		break;
	case MFLD_SSP0_DEVICE_ID:
		drv_data->device_instance = DMA1C_DEVICE_INSTANCE_SSP0;
		break;
	default:
		dev_err(&(pdev->dev),
			"Can not determine dma device instance (PCI ID:%04x)\n",
			pdev->device);
		status = -ENODEV;
		goto err_find_usage;
	}
	status = pci_enable_device(pdev);
	if (status)
		dev_err((&pdev->dev), "Can not enable device.Err=%d\n", status);
err_find_usage:
	return status;
}

/**
 * intel_mid_i2s_probe - probing function for the pci selected
 * @pdev : pci_dev pointer that is probed
 * @ent : pci_device_id
 *
 * Output parameters
 *      NA
 */
static int intel_mid_i2s_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct intel_mid_i2s_hdl *drv_data;
	int status = 0;
	enum intel_mid_i2s_ssp_usage usage;

	drv_data = kzalloc(sizeof(struct intel_mid_i2s_hdl), GFP_KERNEL);
	dev_dbg(&(pdev->dev), "%s Probe, drv_data =%p\n",
						DRIVER_NAME, drv_data);
	if (!drv_data) {
		dev_err((&pdev->dev), "Can't alloc driver data in probe\n");
		status = -ENOMEM;
		goto leave;
	}
	dev_info((&pdev->dev), "Detected PCI SSP (ID: %04x:%04x)\n",
						pdev->vendor, pdev->device);
	status = intel_mid_i2s_find_usage(pdev, drv_data, &usage);
	if (status)
		goto err_i2s_probe0;
	mutex_init(&drv_data->mutex);
	drv_data->pdev = pdev;
	drv_data->usage = usage;
	/*
	 * Get basic io resource and map it for SSP1 [BAR=0]
	 */
	if ((pdev->device == MFLD_SSP1_DEVICE_ID) ||
	    (pdev->device == MFLD_SSP0_DEVICE_ID)) {
		drv_data->paddr = pci_resource_start(pdev, MRST_SSP_BAR);
		drv_data->iolen = pci_resource_len(pdev, MRST_SSP_BAR);
		status = pci_request_region(pdev, MRST_SSP_BAR,
						dev_name(&pdev->dev));
		/* map bus memory into CPU space */
		drv_data->ioaddr = pci_ioremap_bar(pdev, MRST_SSP_BAR);
	} else {
		dev_err(&pdev->dev,
			"Don't know which BAR to usefor this SSP PCDID=%x\n",
			pdev->device);
		status = -ENODEV;
		goto err_i2s_probe1;
	}
	dev_dbg(&(pdev->dev), "paddr = : %x\n", (unsigned int) drv_data->paddr);
	dev_dbg(&(pdev->dev), "iolen = : %d\n", drv_data->iolen);
	if (status) {
		dev_err((&pdev->dev), "Can't request region. err=%d\n", status);
		goto err_i2s_probe1;
	}
	if (!drv_data->ioaddr) {
		dev_err((&pdev->dev), "ioremap_nocache error\n");
		status = -ENOMEM;
		goto err_i2s_probe2;
	}
	dev_dbg(&(pdev->dev), "ioaddr = : %p\n", drv_data->ioaddr);
	/* prepare for DMA channel allocation */
	/* get the pci_dev structure pointer */
	/* Check the SSP, if SSP3, then another DMA is used (GPDMA..) */
	if ((pdev->device == MFLD_SSP1_DEVICE_ID) ||
	    (pdev->device == MFLD_SSP0_DEVICE_ID)) {
		drv_data->dmac1 = pci_get_device(PCI_VENDOR_ID_INTEL,
						 MFLD_LPE_DMA_DEVICE_ID,
						 NULL);
	} else {
		dev_err(&pdev->dev,
			"Don't know dma device ID for this SSP PCDID=%x\n",
			pdev->device);
		goto err_i2s_probe3;
	}
	/* in case the stop dma have to wait for end of callbacks   */
	/* This will be removed when TERMINATE_ALL available in DMA */
	init_waitqueue_head(&drv_data->wq_chan_closing);
	if (!drv_data->dmac1) {
		dev_err(&(drv_data->pdev->dev), "Can't find DMAC1, dma init failed\n");
		status = -ENODEV;
		goto err_i2s_probe3;
	}
	/* increment ref count of pci device structure already done by */
	/* pci_get_device. will do a pci_dev_put when exiting the module */
	pci_set_drvdata(pdev, drv_data);
	/* set SSP FrameSync and CLK direction in INPUT mode in order
	 * to avoid disturbing peripherals
	 */
	write_SSCR1((SSCR1_SFRMDIR_MASK<<SSCR1_SFRMDIR_SHIFT)
		  | (SSCR1_SCLKDIR_MASK<<SSCR1_SCLKDIR_SHIFT),
	drv_data->ioaddr);
	/* Attach to IRQ */
	drv_data->irq = pdev->irq;
	dev_dbg(&(pdev->dev), "attaching to IRQ: %04x\n", pdev->irq);

	status = request_irq(drv_data->irq, i2s_int, IRQF_SHARED,
							"i2s ssp", drv_data);
	if (status < 0)	{
		dev_err(&pdev->dev, "can not get IRQ. status err=%d\n", status);
		goto err_i2s_probe3;
	}
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&(drv_data->pdev->dev));

	goto leave;
err_i2s_probe3:
	iounmap(drv_data->ioaddr);
err_i2s_probe2:
	pci_release_region(pdev, MRST_SSP_BAR);
err_i2s_probe1:
	pci_disable_device(pdev);
err_i2s_probe0:
	kfree(drv_data);
leave:
	return status;
}

static void __devexit intel_mid_i2s_remove(struct pci_dev *pdev)
{
	struct intel_mid_i2s_hdl *drv_data;

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_forbid(&pdev->dev);

	drv_data = pci_get_drvdata(pdev);
	if (!drv_data) {
		dev_err(&pdev->dev, "no drv_data in pci device to remove!\n");
		goto leave;
	}
	if (test_bit(I2S_PORT_OPENED, &drv_data->flags)) {
		dev_warn(&pdev->dev, "Not closed before removing pci_dev!\n");
		intel_mid_i2s_close(drv_data);
	}
	pci_set_drvdata(pdev, NULL);
	/* Stop DMA is already done during close()  */
	pci_dev_put(drv_data->dmac1);
	/* Disable the SSP at the peripheral and SOC level */
	write_SSCR0(0, drv_data->ioaddr);
	free_irq(drv_data->irq, drv_data);
	iounmap(drv_data->ioaddr);
	pci_release_region(pdev, MRST_SSP_BAR);
	pci_release_region(pdev, MRST_LPE_BAR);
	pci_disable_device(pdev);
	kfree(drv_data);
leave:
	return;
}

/**
 * intel_mid_i2s_init - register pci driver
 *
 */
static int __init intel_mid_i2s_init(void)
{
	return pci_register_driver(&intel_mid_i2s_driver);
}

static void __exit intel_mid_i2s_exit(void)
{
	pci_unregister_driver(&intel_mid_i2s_driver);
}


module_init(intel_mid_i2s_init);
module_exit(intel_mid_i2s_exit);

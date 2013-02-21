/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Jim Liu <jim.liu@intel.com>
 */

#include "mdfld_msic.h"
#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_intel_hdmi.h"
#include <asm/intel_scu_ipc.h>

#define SRAM_MSIC_VRINT_ADDR 0xFFFF7FCB
static u8 __iomem *sram_vreg_addr;
/*
 *
 */
static struct android_hdmi_priv *hdmi_priv;

void mdfld_msic_init(struct android_hdmi_priv *p_hdmi_priv)
{
	hdmi_priv = p_hdmi_priv;
}

/**
 *  hpd_notify_um
 */
void hpd_notify_um (void)
{
	struct drm_device *dev = hdmi_priv ? hdmi_priv->dev : NULL;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct pci_dev *pdev = NULL;
	struct device *ddev = NULL;
	struct kobject *kobj = NULL;

	if (dev)
		PSB_DEBUG_ENTRY("\n");

	dev_priv->hdmi_done_reading_edid = false;

	/*find handle to drm kboject*/
	if(dev == NULL){
		DRM_ERROR("%s: dev == NULL. \n", __FUNCTION__);
		return;
	}
	pdev = dev->pdev;

	if(pdev == NULL){
		DRM_ERROR("%s: pdev == NULL. \n", __FUNCTION__);
		return;
	}
	ddev = &pdev->dev;

	if(ddev == NULL){
		DRM_ERROR("%s: ddev == NULL. \n", __FUNCTION__);
		return;
	}
	kobj = &ddev->kobj;

	if(kobj == NULL){
		DRM_ERROR("%s: kobj == NULL. \n", __FUNCTION__);
		return;
	}

	if (dev_priv->psb_hotplug_state) {
		DRM_INFO("%s: HPD interrupt. \n", __FUNCTION__);
		psb_hotplug_notify_change_um("hpd_hdmi", dev_priv->psb_hotplug_state);
	} else {
		DRM_ERROR("%s: Hotplug comm layer isn't initialized! \n", __FUNCTION__);
	}

	return;
}

/**
 *  msic_vreg_handler
 */
static irqreturn_t msic_vreg_handler(int irq, void *dev_id)
{
	struct drm_device *dev = hdmi_priv ? hdmi_priv->dev : NULL;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	u8 data = 0;

	/* Need to add lock later.*/

	/* Read VREG interrupt status register */
	if (sram_vreg_addr)
		data = readb(sram_vreg_addr);
	else
		DRM_ERROR("%s: sram_vreg_addr = %p\n", __FUNCTION__, sram_vreg_addr);

	if (dev)
		PSB_DEBUG_ENTRY("data = 0x%x.\n", data);

	/* handle HDMI HPD interrupts. */
	if (data & HDMI_HPD_STATUS) {
		DRM_INFO("%s: HPD interrupt. data = 0x%x. \n", __FUNCTION__, data);
	
		if (dev_priv->xserver_start)
			hpd_notify_um();
	}

	/* handle other msic vreq interrupts when necessary. */

	return IRQ_HANDLED;
}

/**
 *  msic_probe
 */
static int __devinit msic_probe(struct pci_dev *pdev,
			const struct pci_device_id *ent)
{
	struct drm_device *dev = hdmi_priv ? hdmi_priv->dev : NULL;
	int ret = 0;

	if (dev)
		PSB_DEBUG_ENTRY("\n");

	/* enable msic hdmi device */
	ret = pci_enable_device(pdev);

	if (!ret) {

		if (pdev->device == MSIC_PCI_DEVICE_ID) {
			sram_vreg_addr = ioremap_nocache(SRAM_MSIC_VRINT_ADDR, 0x2);
			ret = request_irq(pdev->irq, msic_vreg_handler, IRQF_SHARED, "msic_hdmi_driver",(void *)&hdmi_priv);
		} else
			DRM_ERROR("%s: pciid = 0x%x is not msic_hdmi pciid. \n", __FUNCTION__, pdev->device);

		if (!ret) {
#if 0 /* #ifdef CONFIG_X86_MRST may still be necessary*/
			u8 data = 0;

			/* clear HDMI HPD */
			intel_scu_ipc_ioread8(MSIC_VRINT_STATUS, &data);
			intel_scu_ipc_iowrite8(MSIC_VRINT_STATUS, data);

			/* clear MSIC first level VREG interrupt. */
			intel_scu_ipc_ioread8(MSIC_IRQLVL1_STATUS, &data);
			data &= VREG_STATUS;
			intel_scu_ipc_iowrite8(MSIC_IRQLVL1_STATUS, data);

			/* enable HDMI HPD */
			intel_scu_ipc_ioread8(MSIC_VRINT_MASK, &data);
			data &= ~HDMI_HPD_MASK;
			intel_scu_ipc_iowrite8(MSIC_VRINT_MASK, data);

			/* enable MSIC first level VREG interrupt. */
			intel_scu_ipc_ioread8(MSIC_IRQLVL1_MASK, &data);
			data &= ~VREG_MASK;
			intel_scu_ipc_iowrite8(MSIC_IRQLVL1_MASK, data);
			/* Enable and handle other msic vreq interrupts when necessary. */
#endif
		} else {
			pci_dev_put(pdev);
			DRM_ERROR("%s: request_irq failed. ret = 0x%x. \n", __FUNCTION__, ret);
		}
	}

	return ret;
}

static struct pci_device_id msic_pci_id_list[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, MSIC_PCI_DEVICE_ID) },
	{ 0 }
};

/*MODULE_DEVICE_TABLE(pci, msic_pci_id_list);*/


/* field for registering driver to PCI device */
static struct pci_driver msic_pci_driver = {
	.name = "msic_hdmi_driver",
	.id_table = msic_pci_id_list,
	.probe = msic_probe
};

/**
 *  msic_regsiter_driver - register the msic hdmi device to PCI system.
 */
int msic_regsiter_driver(void)
{
	return pci_register_driver(&msic_pci_driver);
}

/**
 *  msic_unregsiter_driver - unregister the msic hdmi device from PCI system.
 */
int msic_unregister_driver(void)
{
	if (!sram_vreg_addr) {
		iounmap(sram_vreg_addr);
		sram_vreg_addr = NULL;
	}
	pci_unregister_driver(&msic_pci_driver);
	return 0;
}

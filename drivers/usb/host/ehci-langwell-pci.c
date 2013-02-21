/*
 * Intel MID Platform Langwell/Penwell OTG EHCI Controller PCI Bus Glue.
 *
 * Copyright (c) 2008 - 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/usb/otg.h>
#include <linux/wakelock.h>
#include <linux/usb/intel_mid_otg.h>
#include <asm/intel_scu_ipc.h>

static int usb_otg_suspend(struct usb_hcd *hcd)
{
	struct otg_transceiver *otg;
	struct intel_mid_otg_xceiv *iotg;
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	u32 temp;

	/*
	 * We know this can only be called when root hub is suspended.
	 * Stop the controller or root hub will not be resumed.
	 */
	temp = ehci_readl(ehci, &ehci->regs->command);
	temp &= ~CMD_RUN;
	ehci_writel(ehci, temp, &ehci->regs->command);

	otg = otg_get_transceiver();
	if (otg == NULL) {
		printk(KERN_ERR "%s Failed to get otg transceiver\n", __func__);
		return -EINVAL;
	}
	iotg = otg_to_mid_xceiv(otg);
	printk(KERN_INFO "%s OTG HNP update suspend\n", __func__);

	atomic_notifier_call_chain(&iotg->iotg_notifier,
				MID_OTG_NOTIFY_HSUSPEND, iotg);
	otg_put_transceiver(otg);
	return 0;
}

static int usb_otg_resume(struct usb_hcd *hcd)
{
	struct otg_transceiver *otg;
	struct intel_mid_otg_xceiv *iotg;

	otg = otg_get_transceiver();
	if (otg == NULL) {
		printk(KERN_ERR "%s Failed to get otg transceiver\n", __func__);
		return -EINVAL;
	}
	iotg = otg_to_mid_xceiv(otg);
	printk(KERN_INFO "%s OTG HNP update resume\n", __func__);

	atomic_notifier_call_chain(&iotg->iotg_notifier,
				MID_OTG_NOTIFY_HRESUME, iotg);
	otg_put_transceiver(otg);
	return 0;
}

/* the root hub will call this callback when device added/removed */
static void otg_notify(struct usb_device *udev, unsigned action)
{
	struct otg_transceiver *otg;
	struct intel_mid_otg_xceiv *iotg;

	/* Ignore root hub add/remove event */
	if (!udev->parent) {
		printk(KERN_INFO "%s Ignore root hub otg_notify\n", __func__);
		return;
	}

	/* Ignore USB devices on external hub */
	if (udev->parent && udev->parent->parent)
		return;

	otg = otg_get_transceiver();
	if (otg == NULL) {
		printk(KERN_ERR "%s Failed to get otg transceiver\n", __func__);
		return;
	}
	iotg = otg_to_mid_xceiv(otg);

	switch (action) {
	case USB_DEVICE_ADD:
		pr_debug("Notify OTG HNP add device\n");
		atomic_notifier_call_chain(&iotg->iotg_notifier,
					MID_OTG_NOTIFY_CONNECT, iotg);
		break;
	case USB_DEVICE_REMOVE:
		pr_debug("Notify OTG HNP delete device\n");
		atomic_notifier_call_chain(&iotg->iotg_notifier,
					MID_OTG_NOTIFY_DISCONN, iotg);
		break;
	default:
		otg_put_transceiver(otg);
		return ;
	}
	otg_put_transceiver(otg);
	return;
}

static int ehci_mid_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	struct hc_driver *driver;
	struct otg_transceiver *otg;
	struct intel_mid_otg_xceiv *iotg;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	int irq;
	int retval;

	pr_debug("initializing Intel MID USB OTG Host Controller\n");

	/* we need not call pci_enable_dev since otg transceiver already take
	 * the control of this device and this probe actaully gets called by
	 * otg transceiver driver with HNP protocol.
	 */
	irq = pdev->irq;

	if (!id)
		return -EINVAL;
	driver = (struct hc_driver *)id->driver_data;
	if (!driver)
		return -EINVAL;

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto err1;
	}

	hcd->self.otg_port = 1;
	ehci = hcd_to_ehci(hcd);
	/* this will be called in ehci_bus_suspend and ehci_bus_resume */
	ehci->otg_suspend = usb_otg_suspend;
	ehci->otg_resume = usb_otg_resume;
	/* this will be called by root hub code */
	hcd->otg_notify = otg_notify;
	otg = otg_get_transceiver();
	if (otg == NULL) {
		printk(KERN_ERR "%s Failed to get otg transceiver\n", __func__);
		retval = -EINVAL;
		goto err1;
	}

	iotg = otg_to_mid_xceiv(otg);
	hcd->regs = iotg->base;

	hcd->rsrc_start = pci_resource_start(pdev, 0);
	hcd->rsrc_len = pci_resource_len(pdev, 0);

	if (hcd->regs == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto err2;
	}
	retval = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED);
	if (retval != 0)
		goto err2;
	retval = otg_set_host(otg, &hcd->self);
	if (!otg->default_a)
		hcd->self.is_b_host = 1;
	otg_put_transceiver(otg);
	return retval;

err2:
	usb_put_hcd(hcd);
err1:
	dev_err(&pdev->dev, "init %s fail, %d\n", dev_name(&pdev->dev), retval);
	return retval;
}

void ehci_mid_remove(struct pci_dev *dev)
{
	struct usb_hcd *hcd = pci_get_drvdata(dev);

	if (!hcd)
		return;
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
}

static int ehci_mid_start_host(struct intel_mid_otg_xceiv *iotg)
{
	struct pci_dev	*pdev;
	int		retval;

	if (iotg == NULL)
		return -EINVAL;

	pdev = to_pci_dev(iotg->otg.dev);

	retval = ehci_mid_probe(pdev, ehci_pci_driver.id_table);
	if (retval)
		dev_dbg(iotg->otg.dev, "Failed to start host\n");
	else
		wake_lock(&iotg->wake_lock);

	return retval;
}

static int ehci_mid_stop_host(struct intel_mid_otg_xceiv *iotg)
{
	struct pci_dev	*pdev;

	if (iotg == NULL)
		return -EINVAL;
	wake_unlock(&iotg->wake_lock);
	pdev = to_pci_dev(iotg->otg.dev);

	ehci_mid_remove(pdev);

	return 0;
}

static int intel_mid_ehci_driver_register(struct pci_driver *host_driver)
{
	struct otg_transceiver		*otg;
	struct intel_mid_otg_xceiv	*iotg;

	otg = otg_get_transceiver();
	if (otg == NULL)
		return -EINVAL;

	iotg = otg_to_mid_xceiv(otg);
	iotg->start_host = ehci_mid_start_host;
	iotg->stop_host = ehci_mid_stop_host;

	/* notify host driver is registered */
	atomic_notifier_call_chain(&iotg->iotg_notifier,
				MID_OTG_NOTIFY_HOSTADD, iotg);

	otg_put_transceiver(otg);

	return 0;
}

static void intel_mid_ehci_driver_unregister(struct pci_driver *host_driver)
{
	struct otg_transceiver		*otg;
	struct intel_mid_otg_xceiv	*iotg;

	otg = otg_get_transceiver();
	if (otg == NULL)
		return ;

	iotg = otg_to_mid_xceiv(otg);
	iotg->start_host = NULL;
	iotg->stop_host = NULL;

	/* notify host driver is unregistered */
	atomic_notifier_call_chain(&iotg->iotg_notifier,
				MID_OTG_NOTIFY_HOSTREMOVE, iotg);

	otg_put_transceiver(otg);
}

static int
intel_mid_ehci_start(struct notifier_block *this, unsigned long code,
		     void *data)
{
	if (code == SCU_AVAILABLE)
		(void)intel_mid_ehci_driver_register(&ehci_pci_driver);

	return NOTIFY_OK;
}

struct notifier_block intel_mid_ehci_nb = {
	.notifier_call = intel_mid_ehci_start,
};

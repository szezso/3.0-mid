/*
 * mid_pci.c
 * Copyright (c) 2010, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/bootmem.h>
#include <linux/intel_mid_pm.h>
#include "pci.h"
#include "mid_pci.h"

static struct pci_platform_pm_ops pmu_pci_platform_pm = {
	.is_manageable = pmu_pci_power_manageable,
	.set_state = pmu_pci_set_power_state,
	.choose_state = pmu_pci_choose_state,
	.can_wakeup = pmu_pci_can_wakeup,
	.sleep_wake = pmu_pci_sleep_wake,
};

/* Define weak platform_pm callbacks
 * the actual ones are in mid_pmu.c
 */
pci_power_t __weak pmu_pci_choose_state(struct pci_dev *pdev)
{
	return PCI_D3hot;
}

bool __weak pmu_pci_power_manageable(struct pci_dev *dev)
{
	return true;
}

int __weak pmu_pci_set_power_state(struct pci_dev *pdev, pci_power_t state)
{
	return 0;
}

bool __weak pmu_pci_can_wakeup(struct pci_dev *dev)
{
	return true;
}

int __weak pmu_pci_sleep_wake(struct pci_dev *dev, bool enable)
{
	return 0;
}

/**
 * mid_pci_init - It registers callback function for all the PCI devices
 * for platform specific device power on/shutdown activities.
 */
static int __init mid_pci_init(void)
{
	int ret = 0;

	pr_info("mid_pci_init is called\n");

	/* register pmu driver call back function for platform specific
	 * set power state for pci devices
	 */
	pci_set_platform_pm(&pmu_pci_platform_pm);

	return ret;
}
arch_initcall(mid_pci_init);

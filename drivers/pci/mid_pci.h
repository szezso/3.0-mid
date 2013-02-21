/*
 * mid_pci.h
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
#ifndef MID_PCI_H
#define MID_PCI_H

extern void ppm_resume(void);
extern void ppm_resume_end(void);
extern int pmu_pci_set_power_state(struct pci_dev *pdev, pci_power_t state);
extern pci_power_t pmu_pci_choose_state(struct pci_dev *pdev);
extern bool pmu_pci_power_manageable(struct pci_dev *pdev);
extern bool pmu_pci_can_wakeup(struct pci_dev *pdev);
extern int pmu_pci_sleep_wake(struct pci_dev *pdev, bool enable);

#endif /* define MID_PCI_H */

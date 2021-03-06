/*
 * Moorestown PCI support
 *   Copyright (c) 2008 Intel Corporation
 *     Jesse Barnes <jesse.barnes@intel.com>
 *
 * Moorestown has an interesting PCI implementation:
 *   - configuration space is memory mapped (as defined by MCFG)
 *   - Lincroft devices also have a real, type 1 configuration space
 *   - Early Lincroft silicon has a type 1 access bug that will cause
 *     a hang if non-existent devices are accessed
 *   - some devices have the "fixed BAR" capability, which means
 *     they can't be relocated or modified; check for that during
 *     BAR sizing
 *
 * So, we use the MCFG space for all reads and writes, but also send
 * Lincroft writes to type 1 space.  But only read/write if the device
 * actually exists, otherwise return all 1s for reads and bit bucket
 * the writes.
 */

#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/dmi.h>

#include <asm/acpi.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/pci_x86.h>
#include <asm/hw_irq.h>
#include <asm/io_apic.h>

#define PCIE_CAP_OFFSET	0x100

/* Fixed BAR fields */
#define PCIE_VNDR_CAP_ID_FIXED_BAR 0x00	/* Fixed BAR (TBD) */
#define PCI_FIXED_BAR_0_SIZE	0x04
#define PCI_FIXED_BAR_1_SIZE	0x08
#define PCI_FIXED_BAR_2_SIZE	0x0c
#define PCI_FIXED_BAR_3_SIZE	0x10
#define PCI_FIXED_BAR_4_SIZE	0x14
#define PCI_FIXED_BAR_5_SIZE	0x1c

/* Lincroft and Penwell have just one PCI bus (00) */
#define MID_PCI_BUS	0

/*
 * Use type 1 PCI config access for host bridge(00:00.0), gfx(00:02.0)
 * and ISP(00:03.0) and MMCFG for all other devices.
 */
#define MID_PCI_DEV_HOST_BRIDGE	PCI_DEVFN(0, 0)
#define MID_PCI_DEV_GFX		PCI_DEVFN(2, 0)
#define MID_PCI_DEV_ISP		PCI_DEVFN(3, 0)
#define ISP_PCI_PMCS		0xD4

/**
 * fixed_bar_cap - return the offset of the fixed BAR cap if found
 * @bus: PCI bus
 * @devfn: device in question
 *
 * Look for the fixed BAR cap on @bus and @devfn, returning its offset
 * if found or 0 otherwise.
 */
static int fixed_bar_cap(struct pci_bus *bus, unsigned int devfn)
{
	int pos;
	u32 pcie_cap = 0, cap_data;

	pos = PCIE_CAP_OFFSET;

	if (!raw_pci_ext_ops)
		return 0;

	while (pos) {
		if (raw_pci_ext_ops->read(pci_domain_nr(bus), bus->number,
					  devfn, pos, 4, &pcie_cap))
			return 0;

		if (PCI_EXT_CAP_ID(pcie_cap) == 0x0000 ||
			PCI_EXT_CAP_ID(pcie_cap) == 0xffff)
			break;

		if (PCI_EXT_CAP_ID(pcie_cap) == PCI_EXT_CAP_ID_VNDR) {
			raw_pci_ext_ops->read(pci_domain_nr(bus), bus->number,
					      devfn, pos + 4, 4, &cap_data);
			if ((cap_data & 0xffff) == PCIE_VNDR_CAP_ID_FIXED_BAR)
				return pos;
		}

		pos = PCI_EXT_CAP_NEXT(pcie_cap);
	}

	return 0;
}

static int pci_device_update_fixed(struct pci_bus *bus, unsigned int devfn,
				   int reg, int len, u32 val, int offset)
{
	u32 size;
	unsigned int domain, busnum;
	int bar = (reg - PCI_BASE_ADDRESS_0) >> 2;

	domain = pci_domain_nr(bus);
	busnum = bus->number;

	if (val == ~0 && len == 4) {
		unsigned long decode;

		raw_pci_ext_ops->read(domain, busnum, devfn,
			       offset + 8 + (bar * 4), 4, &size);

		/* Turn the size into a decode pattern for the sizing code */
		if (size) {
			decode = size - 1;
			decode |= decode >> 1;
			decode |= decode >> 2;
			decode |= decode >> 4;
			decode |= decode >> 8;
			decode |= decode >> 16;
			decode++;
			decode = ~(decode - 1);
		} else {
			decode = 0;
		}

		/*
		 * If val is all ones, the core code is trying to size the reg,
		 * so update the mmconfig space with the real size.
		 *
		 * Note: this assumes the fixed size we got is a power of two.
		 */
		return raw_pci_ext_ops->write(domain, busnum, devfn, reg, 4,
				       decode);
	}

	/* This is some other kind of BAR write, so just do it. */
	return raw_pci_ext_ops->write(domain, busnum, devfn, reg, len, val);
}

/**
 * type1_access_ok - check whether to use type 1
 * @bus: bus number
 * @devfn: device & function in question
 *
 * If the bus is on a Lincroft chip and it exists, or is not on a Lincroft at
 * all, the we can go ahead with any reads & writes.  If it's on a Lincroft,
 * but doesn't exist, avoid the access altogether to keep the chip from
 * hanging.
 */
static bool type1_access_ok(unsigned int bus, unsigned int devfn, int reg)
{
	/* This is a workaround for A0 LNC bug where PCI status register does
	 * not have new CAP bit set. can not be written by SW either.
	 *
	 * PCI header type in real LNC indicates a single function device, this
	 * will prevent probing other devices under the same function in PCI
	 * shim. Therefore, use the header type in shim instead.
	 */
	if (reg >= 0x100 || reg == PCI_STATUS || reg == PCI_HEADER_TYPE)
		return 0;

	/*
	 * Workaround for ISP driver.
	 * When runtime pm is enabled, PCI core needs access PMCS register when
	 * ISP power island is off, this will cause pci_set_power_state() to
	 * fail, thus block the system to enter S0i3.
	 * We force MMCFG for PCI config access to ISP PMCS register to
	 * make sure pci_set_power_state() can succeed. All other config
	 * access is routed to type 1.
	 */
	if (bus == MID_PCI_BUS && devfn == MID_PCI_DEV_ISP &&
			reg == ISP_PCI_PMCS)
		return 0;

	/* Use type 1 for real PCI devices(0:0:0, 0:2:0 and 0:3:0) */
	if (bus == MID_PCI_BUS && (devfn == MID_PCI_DEV_HOST_BRIDGE ||
					devfn == MID_PCI_DEV_GFX ||
					devfn == MID_PCI_DEV_ISP))
		return 1;

	return 0; /* langwell on others */
}

static int pci_read(struct pci_bus *bus, unsigned int devfn, int where,
		    int size, u32 *value)
{
	if (type1_access_ok(bus->number, devfn, where))
		return pci_direct_conf1.read(pci_domain_nr(bus), bus->number,
					devfn, where, size, value);
	return raw_pci_ext_ops->read(pci_domain_nr(bus), bus->number,
			      devfn, where, size, value);
}

static int pci_write(struct pci_bus *bus, unsigned int devfn, int where,
		     int size, u32 value)
{
	int offset;

	/* On MRST, there is no PCI ROM BAR, this will cause a subsequent read
	 * to ROM BAR return 0 then being ignored.
	 */
	if (where == PCI_ROM_ADDRESS)
		return 0;

	/*
	 * Devices with fixed BARs need special handling:
	 *   - BAR sizing code will save, write ~0, read size, restore
	 *   - so writes to fixed BARs need special handling
	 *   - other writes to fixed BAR devices should go through mmconfig
	 */
	offset = fixed_bar_cap(bus, devfn);
	if (offset &&
	    (where >= PCI_BASE_ADDRESS_0 && where <= PCI_BASE_ADDRESS_5)) {
		return pci_device_update_fixed(bus, devfn, where, size, value,
					       offset);
	}

	/*
	 * On Moorestown update both real & mmconfig space
	 * Note: early Lincroft silicon can't handle type 1 accesses to
	 *       non-existent devices, so just eat the write in that case.
	 */
	if (type1_access_ok(bus->number, devfn, where))
		return pci_direct_conf1.write(pci_domain_nr(bus), bus->number,
					      devfn, where, size, value);
	return raw_pci_ext_ops->write(pci_domain_nr(bus), bus->number, devfn,
			       where, size, value);
}

static int mrst_pci_irq_enable(struct pci_dev *dev)
{
	u8 pin;
	struct io_apic_irq_attr irq_attr;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);

	/* MRST only have IOAPIC, the PCI irq lines are 1:1 mapped to
	 * IOAPIC RTE entries, so we just enable RTE for the device.
	 */
	irq_attr.ioapic = mp_find_ioapic(dev->irq);
	irq_attr.ioapic_pin = dev->irq;
	irq_attr.trigger = 1; /* level */
	irq_attr.polarity = 1; /* active low */
	io_apic_set_pci_routing(&dev->dev, dev->irq, &irq_attr);

	return 0;
}

struct pci_ops pci_mrst_ops = {
	.read = pci_read,
	.write = pci_write,
};

/**
 * pci_mrst_init - installs pci_mrst_ops
 *
 * Moorestown has an interesting PCI implementation (see above).
 * Called when the early platform detection installs it.
 */
int __init pci_mrst_init(void)
{
	printk(KERN_INFO "Moorestown platform detected, using MRST PCI ops\n");
	pci_mmcfg_late_init();
	pcibios_enable_irq = mrst_pci_irq_enable;
	pci_root_ops = pci_mrst_ops;
	/* Continue with standard init */
	return 1;
}

/*
 * Langwell devices reside at fixed offsets, don't try to move them.
 */
static void __devinit pci_fixed_bar_fixup(struct pci_dev *dev)
{
	unsigned long offset;
	u32 size;
	int i;

	/* Must have extended configuration space */
	if (dev->cfg_size < PCIE_CAP_OFFSET + 4)
		return;

	/* Fixup the BAR sizes for fixed BAR devices and make them unmoveable */
	offset = fixed_bar_cap(dev->bus, dev->devfn);
	if (!offset || PCI_DEVFN(2, 0) == dev->devfn ||
	    PCI_DEVFN(2, 2) == dev->devfn)
		return;

	for (i = 0; i < PCI_ROM_RESOURCE; i++) {
		pci_read_config_dword(dev, offset + 8 + (i * 4), &size);
		dev->resource[i].end = dev->resource[i].start + size - 1;
		dev->resource[i].flags |= IORESOURCE_PCI_FIXED;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_fixed_bar_fixup);

/* Langwell devices are not true pci devices, they are not subject to 10 ms
 * d3 to d0 delay required by pci spec.
 */
static void __devinit pci_d3delay_fixup(struct pci_dev *dev)
{
	/* true pci devices in lincroft should allow type 1 access, the rest
	 * are langwell fake pci devices.
	 */
	if (type1_access_ok(dev->bus->number, dev->devfn, PCI_DEVICE_ID))
		return;
	dev->d3_delay = 0;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_d3delay_fixup);

static void __devinit pci_d1support_fixup(struct pci_dev *dev)
{
	/* South complex devices supports d0i1 which maps to pci d1. before
	 * fw fixes pci shim in pci PMC (power management cap), we need this
	 * workaround to ensure proper transition of d0-d0i1 for the devices
	 * that does not do d0i3, such as OTG. PMC should be changed from
	 * 0x48 to 0x5a.
	 */
	if (type1_access_ok(dev->bus->number, dev->devfn, PCI_DEVICE_ID))
		return;
	dev->d1_support = true;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_ANY_ID, pci_d1support_fixup);

static void __devinit mrst_power_off_unused_dev(struct pci_dev *dev)
{
	pci_set_power_state(dev, PCI_D3cold);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0801, mrst_power_off_unused_dev);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0809, mrst_power_off_unused_dev);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x080C, mrst_power_off_unused_dev);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0812, mrst_power_off_unused_dev);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0815, mrst_power_off_unused_dev);

/*
 * The Firmware should program the Langwell keypad registers to indicate
 * system specific configuration.  This quirk can be removed when firmware
 * actually does this properly.
 */
static void __devinit langwell_keypad_fixup(struct pci_dev *dev)
{
	void __iomem *base;
	u32 val;

	base = pci_ioremap_bar(dev, 0);

	if (base == NULL)
		return;

	val = readl(base);

	/*
	 * set the KPC register.  Please see the Langwell Docs
	 * for more detail.
	 *
	 * Bit 31: Reserved
	 * Bit 30: Automatic Scan Bit
	 * Bit 29: Automatic Scan on Activity bit
	 * Bit 28:26 : Matrix keypad row number
	 *		000 == 1, 001 == 2, ... 111 == 8
	 * Bit 25:23 : Matrix keypad column number
	 *		000 ==1, 001 == 2, ... 111 == 8
	 * Bit 22: Matrix Interrupt Bit
	 * Bit 21: Ignore Multiple Key Press
	 * Bit 20: Manual Matrix Scan line 7
	 * Bit 19: Manual Matrix Scan line 6
	 * Bit 18: Manual Matrix Scan line 5
	 * Bit 17: Manual Matrix Scan line 4
	 * Bit 16: Manual Matrix Scan line 3
	 * Bit 15: Manual Matrix Scan line 2
	 * Bit 14: Manual Matrix Scan line 1
	 * Bit 13: Manual Matrix Scan line 0
	 * Bit 12: Matrix Keypad Enable
	 * Bit 11: Matrix Interrupt Enable
	 * Bit 10:9  : Reserved
	 * Bit 8:6   : Direct Key Number + Roatry encoder sensor input
	 *		000 == 1, 001 == 2, ... 111 == 8
	 * Bit 5: Direct Interrupt bit
	 * Bit 4: Reserved
	 * Bit 3: Rotary Encoder 1 enable
	 * Bit 2: Rotary Encoder 0 enable
	 * Bit 1: Direct Keypad enable
	 * Bit 0: Direct Keypad Interrupt enable
	 */
	if (val == 0)
		writel(0x3f9ff800, base);

	val = readl(base + 0x24);

	/* set the debounce interval (KPKDI) to 100ms */
	if (val == 0)
		writel(100, base + 0x24);

	iounmap(base);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0805, langwell_keypad_fixup);

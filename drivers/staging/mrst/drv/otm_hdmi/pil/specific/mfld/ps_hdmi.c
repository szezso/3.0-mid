/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include "otm_hdmi_types.h"

#include <asm/io.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"
#include "ps_hdmi.h"

#define PS_HDMI_MMIO_RESOURCE 0
#define PS_VDC_OFFSET 0x00000000
#define PS_VDC_SIZE 0x000080000
#define PS_MSIC_PCI_DEVICE_ID 0x0831
#define PS_MSIC_VRINT_ADDR 0xFFFF7FCB
#define PS_MSIC_VRINT_IOADDR_LEN 0x02

otm_hdmi_ret_t ps_hdmi_pci_dev_init(void *context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int result = 0;
	struct pci_dev *msic_pdev = NULL;
	unsigned int vdc_start;
	uint32_t pci_address = 0;
	uint8_t pci_dev_revision = 0;
	hdmi_context_t *ctx = NULL;

	if (pdev == NULL || context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	pr_debug("\nget resource start\n");
	result = pci_read_config_dword(pdev, 16, &vdc_start);
	if (result != 0) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	pci_address = vdc_start + PS_VDC_OFFSET;

	pr_debug("\nmap IO region\n");
	/* Map IO region and save its length */
	ctx->io_length = PS_VDC_SIZE;
	ctx->io_address = ioremap_cache(pci_address, ctx->io_length);
	if (!ctx->io_address) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	/* Map IO region for IRQ registers */
	ctx->dev.irq_io_address = ioremap_nocache(PS_MSIC_VRINT_ADDR,
						PS_MSIC_VRINT_IOADDR_LEN);
	if (!ctx->dev.irq_io_address) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	pr_debug("\nget PCI dev revision\n");
	result = pci_read_config_byte(pdev, 8, &pci_dev_revision);
	if (result != 0) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	ctx->dev.id = pci_dev_revision;

	pr_debug("pci_get_device for 0x%x\n", PS_MSIC_PCI_DEVICE_ID);
	msic_pdev = pci_get_device(PCI_VENDOR_INTEL,
					PS_MSIC_PCI_DEVICE_ID, msic_pdev);
	if (msic_pdev == NULL) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	pr_debug("pci_enable_device for 0x%x\n",
					PS_MSIC_PCI_DEVICE_ID);
	result = pci_enable_device(msic_pdev);
	if (result) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	pr_debug("IRQ number assigned = %d\n", msic_pdev->irq);
	ctx->irq_number = msic_pdev->irq;

exit:
	return rc;
}

otm_hdmi_ret_t ps_hdmi_pci_dev_deinit(void *context)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	struct pci_dev *msic_pdev = NULL;
	hdmi_context_t *ctx = NULL;

	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	/* unmap IO region */
	iounmap(ctx->io_address) ;

	/* TODO: Enable this on DV1 once PCI issue is resolved */
	msic_pdev = pci_get_device(PCI_VENDOR_INTEL,
					PS_MSIC_PCI_DEVICE_ID, msic_pdev);
	if (msic_pdev == NULL) {
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	pci_disable_device(msic_pdev);
exit:
	return rc;
}

otm_hdmi_ret_t ps_hdmi_i2c_edid_read(void *ctx, unsigned int sp,
				  unsigned int offset, void *buffer,
				  unsigned int size)
{
	hdmi_context_t *context = (hdmi_context_t *)ctx;

	char *src = context->edid_raw + sp * SEGMENT_SIZE + offset;
	memcpy(buffer, src, size);

	return OTM_HDMI_SUCCESS;
}

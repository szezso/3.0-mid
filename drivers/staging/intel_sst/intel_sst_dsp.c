/*
 *  intel_sst_dsp.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10	Intel Corp
 *  Authors:	Lomesh Agarwal <lomesh.agarwal@intel.com>
 *		Anurag Kansal <anurag.kansal@intel.com>
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
 *	and middleware.
 *
 *  This file contains all dsp controlling functions like firmware download,
 * setting/resetting dsp cores, etc
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/firmware.h>
#include <linux/poll.h>
#include "intel_sst.h"
#include "intel_sst_ioctl.h"
#include "intel_sst_common.h"

/**
 * intel_sst_reset_dsp_medfield - Resetting SST DSP
 *
 * This resets DSP in case of Medfield platfroms
 */
static int intel_sst_reset_dsp_medfield(void)
{
	union config_status_reg csr;

	pr_debug("Resetting the DSP in medfield\n");
	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.full |= 0x382;
	csr.part.run_stall = 0x1;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);

	return 0;
}

/**
 * sst_start_medfield - Start the SST DSP processor
 *
 * This starts the DSP in MRST platfroms
 */
static int sst_start_medfield(void)
{
	union config_status_reg csr;

	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.bypass = 0;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);
	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.mfld_strb = 1;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);
	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.run_stall = 0;
	csr.part.sst_reset = 0;
	pr_debug("Starting the DSP_medfld %x\n", csr.full);
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);

	return 0;
}

/**
 * sst_stop_dsp: Pause the SST DSP processor.
 *
 * Should be called before changing DSP IRAM/DRAM.
 */
int sst_pause_dsp(void)
{
	union config_status_reg csr;

	csr.full = readl(sst_drv_ctx->shim + SST_CSR);
	csr.part.run_stall = 1;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);

	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.bypass = 0x7;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);

	return 0;
}

/**
 * sst_resume_dsp: Resume the SST DSP processor.
 *
 * Should be called after changing DSP IRAM/DRAM.
 */
int sst_resume_dsp(void)
{
	union config_status_reg csr;

	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.bypass = 0x0;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);
	csr.full = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
	csr.part.run_stall = 0;
	sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr.full);

	return 0;
}

/**
 * sst_parse_module - Parse audio FW modules
 *
 * @module: FW module header
 *
 * Parses modules that need to be placed in SST IRAM and DRAM
 * returns error or 0 if module sizes are proper
 */
static int sst_parse_module(struct fw_module_header *module)
{
	struct dma_block_info *block;
	u32 count;
	void __iomem *ram;

	pr_debug("module sign %s size %x blocks %x type %x\n",
			module->signature, module->mod_size,
			module->blocks, module->type);
	pr_debug("module entrypoint 0x%x\n", module->entry_point);

	block = (void *)module + sizeof(*module);

	for (count = 0; count < module->blocks; count++) {
		if (block->size <= 0) {
			pr_err("block size invalid\n");
			return -EINVAL;
		}
		switch (block->type) {
		case SST_IRAM:
			ram = sst_drv_ctx->iram;
			break;
		case SST_DRAM:
			ram = sst_drv_ctx->dram;
			break;
		default:
			pr_err("wrong ram type0x%x in block0x%x\n",
					block->type, count);
			return -EINVAL;
		}
		memcpy_toio(ram + block->ram_offset,
			    (void *)block + sizeof(*block), block->size);
		block = (void *)block + sizeof(*block) + block->size;
	}
	return 0;
}

/**
 * sst_parse_fw_image - parse and load FW
 *
 * @sst_fw: pointer to audio fw
 *
 * This function is called to parse and download the FW image
 */
static int sst_parse_fw_image(const struct firmware *sst_fw)
{
	struct fw_header *header;
	u32 count;
	int ret_val;
	struct fw_module_header *module;

	BUG_ON(!sst_fw);

	/* Read the header information from the data pointer */
	header = (struct fw_header *)sst_fw->data;

	/* verify FW */
	if ((strncmp(header->signature, SST_FW_SIGN, 4) != 0) ||
	    (sst_fw->size != header->file_size + sizeof(*header))) {
		/* Invalid FW signature */
		pr_err("InvalidFW sign/filesize mismatch\n");
		return -EINVAL;
	}
	pr_debug("header sign=%s size=%x modules=%x fmt=%x size=%x\n",
		 header->signature, header->file_size, header->modules,
		 header->file_format, sizeof(*header));
	module = (void *)sst_fw->data + sizeof(*header);
	for (count = 0; count < header->modules; count++) {
		/* module */
		ret_val = sst_parse_module(module);
		if (ret_val)
			return ret_val;
		module = (void *)module + sizeof(*module) + module->mod_size ;
	}

	return 0;
}

/**
 * sst_load_fw - function to load FW into DSP
 *
 * @fw: Pointer to driver loaded FW
 * @context: driver context
 *
 * This function is called by OS when the FW is loaded into kernel
 */
int sst_load_fw(const struct firmware *fw, void *context)
{
	int ret_val = 0;

	pr_debug("load_fw called\n");
	BUG_ON(!fw);

	ret_val = intel_sst_reset_dsp_medfield();
	if (ret_val)
		return ret_val;

	ret_val = sst_parse_fw_image(fw);
	if (ret_val)
		return ret_val;
	/*  bring sst out of reset  */
	ret_val = sst_start_medfield();
	if (ret_val)
		return ret_val;

	pr_debug("fw loaded successful!!!\n");
	return ret_val;
}

/**
 * sst_load_codec - function to load FW into DSP
 *
 * @fw: Pointer to driver loaded FW
 * @context: driver context
 *
 * This function is called by OS when the FW is loaded into kernel
 */
int sst_load_codec(const struct firmware *sst_lib, void *context)
{
	int ret_val;

	pr_debug("load_codec called\n");
	BUG_ON(!sst_lib);

	sst_pause_dsp();
	ret_val = sst_parse_fw_image(sst_lib);
	if (ret_val)
		return ret_val;

	sst_resume_dsp();

	pr_debug("Codec loaded successful!!!\n");
	return ret_val;
}

/* This function is called befoer downloading the codec/postprocessing
library is set for download to SST DSP*/
static int sst_validate_library(const struct firmware *fw_lib,
		struct lib_slot_info *slot,
		u32 *entry_point)
{
	struct fw_header *header;
	struct fw_module_header *module;
	struct dma_block_info *block;
	unsigned int n_blk, isize = 0, dsize = 0;
	int err = 0;

	header = (struct fw_header *)fw_lib->data;
	if (header->modules != 1) {
		pr_err("Module no mismatch found\n ");
		err = -EINVAL;
		goto exit;
	}
	module = (void *)fw_lib->data + sizeof(*header);
	*entry_point = module->entry_point;
	pr_debug("Module entry point 0x%x\n", *entry_point);
	pr_debug("Module Sign %s, Size 0x%x, Blocks 0x%x Type 0x%x\n",
			module->signature, module->mod_size,
			module->blocks, module->type);

	block = (void *)module + sizeof(*module);
	for (n_blk = 0; n_blk < module->blocks; n_blk++) {
		switch (block->type) {
		case SST_IRAM:
			isize += block->size;
			break;
		case SST_DRAM:
			dsize += block->size;
			break;
		default:
			pr_err("Invalid block type for 0x%x\n", n_blk);
			err = -EINVAL;
			goto exit;
		}
		block = (void *)block + sizeof(*block) + block->size;
	}
	if (isize > slot->iram_size || dsize > slot->dram_size) {
		pr_err("library exceeds size allocated\n");
		err = -EINVAL;
		goto exit;
	} else
		pr_debug("Library is safe for download...\n");

	pr_debug("iram 0x%x, dram 0x%x, iram 0x%x, dram 0x%x\n",
			isize, dsize, slot->iram_size, slot->dram_size);
exit:
	return err;

}

/* This function is called when FW requests for a particular libary download
This function prepares the library to download*/
int sst_load_library(struct snd_sst_lib_download *lib, u8 ops)
{
	char buf[20];
	const char *type, *dir;
	int len = 0, error = 0;
	u32 entry_point;
	const struct firmware *fw_lib;
	struct snd_sst_lib_download_info dload_info = {{{0},},};

	memset(buf, 0, sizeof(buf));

	pr_debug("Lib Type 0x%x, Slot 0x%x, ops 0x%x\n",
			lib->lib_info.lib_type, lib->slot_info.slot_num, ops);
	pr_debug("Version 0x%x, name %s, caps 0x%x media type 0x%x\n",
		lib->lib_info.lib_version, lib->lib_info.lib_name,
		lib->lib_info.lib_caps, lib->lib_info.media_type);

	pr_debug("IRAM Size 0x%x, offset 0x%x\n",
		lib->slot_info.iram_size, lib->slot_info.iram_offset);
	pr_debug("DRAM Size 0x%x, offset 0x%x\n",
		lib->slot_info.dram_size, lib->slot_info.dram_offset);

	switch (lib->lib_info.lib_type) {
	case SST_CODEC_TYPE_MP3:
		type = "mp3_";
		break;
	case SST_CODEC_TYPE_AAC:
		type = "aac_";
		break;
	case SST_CODEC_TYPE_AACP:
		type = "aac_v1_";
		break;
	case SST_CODEC_TYPE_eAACP:
		type = "aac_v2_";
		break;
	case SST_CODEC_TYPE_WMA9:
		type = "wma9_";
		break;
	default:
		pr_err("Invalid codec type\n");
		error = -EINVAL;
		goto wake;
	}

	if (ops == STREAM_OPS_CAPTURE)
		dir = "enc_";
	else
		dir = "dec_";
	len = strlen(type) + strlen(dir);
	strncpy(buf, type, sizeof(buf)-1);
	strncpy(buf + strlen(type), dir, sizeof(buf)-strlen(type)-1);
	len += snprintf(buf + len, sizeof(buf) - len, "%d",
			lib->slot_info.slot_num);
	len += snprintf(buf + len, sizeof(buf) - len, ".bin");

	pr_debug("Requesting %s\n", buf);

	error = request_firmware(&fw_lib, buf, &sst_drv_ctx->pci->dev);
	if (error) {
		pr_err("library load failed %d\n", error);
		goto wake;
	}
	error = sst_validate_library(fw_lib, &lib->slot_info, &entry_point);
	if (error)
		goto wake_free;

	lib->mod_entry_pt = entry_point;
	memcpy(&dload_info.dload_lib, lib, sizeof(*lib));

	/* lib is downloaded and init send alloc again */
	pr_debug("Library is downloaded now...\n");
wake_free:
	release_firmware(fw_lib);
wake:
	return error;
}

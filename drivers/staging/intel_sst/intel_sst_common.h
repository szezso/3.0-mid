#ifndef __INTEL_SST_COMMON_H__
#define __INTEL_SST_COMMON_H__
/*
 *  intel_sst_common.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
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
 *  Common private declarations for SST
 */
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/poll.h>
#include "soc_audio_bu_config.h"
#include "soc_ipc_defs.h"

#define SST_DRIVER_VERSION "2.0.04"
#define SST_VERSION_NUM 0x2004

/* driver names */
#define SST_DRV_NAME "intel_sst_driver"
#define SST_MFLD_PCI_ID 0x082F
#define PCI_ID_LENGTH 4
#define SST_SUSPEND_DELAY 2000
#define SST_STEREO 2

#define MAX_ACTIVE_STREAM	3
#define MAX_ENC_STREAM		2
#define MAX_AM_HANDLES		1
#define ALLOC_TIMEOUT		5000

#define RX_TIMESLOT_UNINIT	-1

/* SST register map */
#define SST_CSR			0x00
#define SST_PISR		0x08
#define SST_PIMR		0x10
#define SST_ISRX		0x18
#define SST_IMRX		0x28
#define SST_IPCX		0x38 /* IPC IA-SST */
#define SST_IPCD		0x40 /* IPC SST-IA */
#define SST_ISRD		0x20 /* dummy register for shim workaround */
#define SST_SHIM_SIZE		0X44
#define SST_CLKCTL		0x78
#define SST_CSR2		0x80

#define SPI_MODE_ENABLE_BASE_ADDR 0xffae4000
#define FW_SIGNATURE_SIZE	4

/* stream states */
enum sst_stream_states {
	STREAM_UN_INIT	= 0,	/* Freed/Not used stream */
	STREAM_RUNNING	= 1,	/* Running */
	STREAM_INIT	= 3,	/* stream init, waiting for data */
};


enum sst_ram_type {
	SST_IRAM	= 1,
	SST_DRAM	= 2,
};
/* SST shim registers to structure mapping  */
union config_status_reg {
	struct {
		u32 mfld_strb:1;
		u32 sst_reset:1;
		u32 clk_sel:3;
		u32 sst_clk:2;
		u32 bypass:3;
		u32 run_stall:1;
		u32 rsvd1:2;
		u32 strb_cntr_rst:1;
		u32 rsvd:18;
	} part;
	u32 full;
};

union interrupt_reg {
	struct {
		u32 ipcx:1;
		u32 ipcd:1;
		u32 rsvd:30;
	} part;
	u32 full;
};

union sst_pisr_reg {
	struct {
		u32 pssp0:1;
		u32 pssp1:1;
		u32 rsvd0:3;
		u32 dmac:1;
		u32 rsvd1:26;
	} part;
	u32 full;
};

union sst_pimr_reg {
	struct {
		u32 ssp0:1;
		u32 ssp1:1;
		u32 rsvd0:3;
		u32 dmac:1;
		u32 rsvd1:10;
		u32 ssp0_sc:1;
		u32 ssp1_sc:1;
		u32 rsvd2:3;
		u32 dmac_sc:1;
		u32 rsvd3:10;
	} part;
	u32 full;
};


struct sst_stream_bufs {
	struct list_head	node;
	u32			size;
	const char		*addr;
	u32			data_copied;
	bool			in_use;
	u32			offset;
};

struct snd_sst_user_cap_list {
	unsigned int iov_index; /* index of iov */
	unsigned long iov_offset; /* offset in iov */
	unsigned long offset; /* offset in kmem */
	unsigned long size; /* size copied */
	struct list_head node;
};

enum snd_src {
	SST_DRV = 1,
	MAD_DRV = 2
};

/**
 * struct stream_info - structure that holds the stream information
 *
 * @status : stream current state
 * @codec : stream codec
 * @ops : stream operation pb/cp/drm...
 * @bufs: stream buffer list
 * @lock : stream mutex for protecting state
 * @sg_index : current stream user buffer index
 * @cur_ptr : stream user buffer pointer
 * @buf_entry : current user buffer
 * @pcm_substream : PCM substream
 * @period_elapsed : PCM period elapsed callback
 * @sfreq : stream sampling freq
 * @str_type : stream type
 * @curr_bytes : current bytes decoded
 * @cumm_bytes : cummulative bytes decoded
 * @str_type : stream type
 * @src : stream source
 * @device : output device type (medfield only)
 * @pcm_slot : pcm slot value
 */
struct stream_info {
	unsigned int status;
	unsigned int soc_input_id;
	unsigned int ops;
	struct list_head bufs;
	struct mutex lock;	/* mutex */
	unsigned int sg_index;	/*  current buf Index  */
	unsigned char __user *cur_ptr;	/*  Current static bufs  */
	struct snd_sst_buf_entry __user *buf_entry;
	void *pcm_substream;
	void (*period_elapsed) (void *pcm_substream);
	unsigned int str_type;
	u32 curr_bytes;
	u32 cumm_bytes;
	u32 src;
	enum snd_sst_audio_device_type device;
	u8 pcm_slot;
	u8 poll_mode;
	wait_queue_head_t sleep;
	uint32_t flag;
	void *input_wl;
	uint32_t codec;
	uint32_t pcm_wd_sz;
	uint32_t num_chan;
	uint32_t sfreq;
	uint32_t ring_buffer_size;
	uint32_t ring_buffer_addr;
	uint32_t period_count;
	struct fasync_struct *async_queue;
};

#define SST_FW_SIGN "$SST"
#define SST_FW_LIB_SIGN "$LIB"

/*
 * struct fw_header - FW file headers
 *
 * @signature : FW signature
 * @modules : # of modules
 * @file_format : version of header format
 * @reserved : reserved fields
 */
struct fw_header {
	unsigned char signature[FW_SIGNATURE_SIZE]; /* FW signature */
	u32 file_size; /* size of fw minus this header */
	u32 modules; /*  # of modules */
	u32 file_format; /* version of header format */
	u32 reserved[4];
};

struct fw_module_header {
	unsigned char signature[FW_SIGNATURE_SIZE]; /* module signature */
	u32 mod_size; /* size of module */
	u32 blocks; /* # of blocks */
	u32 type; /* codec type, pp lib */
	u32 entry_point;
};

struct dma_block_info {
	enum sst_ram_type	type;	/* IRAM/DRAM */
	u32			size;	/* Bytes */
	u32			ram_offset; /* Offset in I/DRAM */
	u32			rsvd;	/* Reserved field */
};

struct ioctl_pvt_data {
	int str_id;
};

struct sst_ipc_msg_wq {
	uint32_t header;
	char mailbox[SOC_AUDIO_MAILBOX_SIZE_IPCD_RCV];
	struct work_struct wq;
};

struct mad_ops_wq {
	int stream_id;
	enum sst_controls control_op;
	struct work_struct	wq;

};

#define SST_MMAP_PAGES	(256*1024 / PAGE_SIZE)
#define SST_MMAP_STEP	(8*1024 / PAGE_SIZE)

/***
 * struct intel_sst_drv - driver ops
 *
 * @pmic_vendor : pmic vendor detected
 * @pci_id : PCI device id loaded
 * @shim : SST shim pointer
 * @mailbox : SST mailbox pointer
 * @iram : SST IRAM pointer
 * @dram : SST DRAM pointer
 * @ipc_dispatch_list : ipc messages dispatched
 * @ipc_post_msg_wq : wq to post IPC messages context
 * @ipc_process_msg : wq to process msgs from FW context
 * @ipc_process_reply : wq to process reply from FW context
 * @ipc_post_msg : wq to post reply from FW context
 * @mad_wq : MAD driver wq
 * @post_msg_wq : wq to post IPC messages
 * @process_msg_wq : wq to process msgs from FW
 * @process_reply_wq : wq to process reply from FW
 * @streams : sst stream contexts
 * @scard_ops : sst card ops
 * @pci : sst pci device struture
 * @stream_lock : sst stream lock
 * @stream_cnt : total sst active stream count
 * @pb_streams : total active pb streams
 * @cp_streams : total active cp streams
 * @pmic_port_instance : active pmic port instance
 * @rx_time_slot_status : active rx slot
 */
struct intel_sst_drv {
	int pmic_vendor;
	unsigned int pci_id;
	void __iomem *shim;
	void __iomem *mailbox;
	void __iomem *iram;
	void __iomem *dram;
	struct sst_ipc_msg_wq ipc_process_msg;
	struct sst_ipc_msg_wq ipc_process_reply;
	wait_queue_head_t wait_queue;
	struct workqueue_struct *mad_wq;
	struct workqueue_struct *process_msg_wq;
	struct workqueue_struct *process_reply_wq;
	struct stream_info streams[SND_SST_MAX_AUDIO_DEVICES];
	enum snd_sst_audio_device_type active_streams[SOC_AUDIO_MAX_INPUTS];

	struct snd_pmic_ops *scard_ops;
	struct pci_dev *pci;
	void *mmap_mem;
	struct mutex stream_lock;
	unsigned int mmap_len;
	unsigned int stream_cnt;	/* total streams */
	unsigned int encoded_cnt;	/* enocded streams only */
	unsigned int am_cnt;
	unsigned int pb_streams;	/* pb streams active */
	unsigned int cp_streams;	/* cp streams active */
	unsigned int pmic_port_instance;	/*pmic port instance */
	int rx_time_slot_status;
	unsigned int compressed_slot;
	unsigned int csr_value;
	unsigned int pll_mode;
	const struct firmware *fw;
	uint32_t proc_hdl;
	void *output_wl;
	uint32_t codec;
	/* For per input volume control.note that this is user gain*/
	int32_t volume[SND_SST_MAX_AUDIO_DEVICES];
	int fw_present;
};

extern struct intel_sst_drv *sst_drv_ctx;

#define SST_DEFAULT_PMIC_PORT 1 /*audio port*/
#define MAX_STREAM_FIELD 255

int sst_play_frame(int streamID);
int sst_capture_frame(int streamID);
int sst_get_stream(struct snd_sst_params *str_param, unsigned int poll_mode);
int sst_get_stream_allocated(struct snd_sst_params *str_param,
			     struct snd_sst_lib_download **lib_dnld,
			     unsigned int poll_mode);
int sst_set_vol(struct snd_sst_vol *set_vol);
int sst_set_mute(struct snd_sst_mute *set_mute);

void sst_process_message(struct work_struct *work);
void sst_process_reply(struct work_struct *work);
void sst_process_mad_ops(struct work_struct *work);

long intel_sst_ioctl(struct file *file_ptr, unsigned int cmd,
			unsigned long arg);
int intel_sst_open(struct inode *i_node, struct file *file_ptr);
int intel_sst_open_cntrl(struct inode *i_node, struct file *file_ptr);
int intel_sst_release(struct inode *i_node, struct file *file_ptr);
int intel_sst_release_cntrl(struct inode *i_node, struct file *file_ptr);
int intel_sst_read(struct file *file_ptr, char __user *buf,
			size_t count, loff_t *ppos);
int intel_sst_write(struct file *file_ptr, const char __user *buf,
			size_t count, loff_t *ppos);
int intel_sst_fasync(int fd, struct file *file_ptr, int mode);
ssize_t intel_sst_aio_write(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr_segs, loff_t  offset);
ssize_t intel_sst_aio_read(struct kiocb *kiocb, const struct iovec *iov,
			   unsigned long nr_segs, loff_t offset);
unsigned int intel_sst_poll(struct file *file, poll_table * wait);

int sst_pause_dsp(void);
int sst_resume_dsp(void);

int sst_load_fw(const struct firmware *fw, void *context);
int sst_load_codec(const struct firmware *fw, void *context);
int sst_load_library(struct snd_sst_lib_download *lib, u8 ops);
void sst_clear_ipcd_interrupt(void);
void sst_clear_ipcx_interrupt(void);
int sst_download_fw(void);
int sst_download_codec(int format, int operation);
void free_stream_context(unsigned int str_id);

enum soc_result audio_set_dma_registers(void *handle,
				struct stream_info *stream);

/*
 * sst_init_stream - this function initialzes stream context
 *
 * @stream : stream struture
 * @codec : codec for stream
 * @sst_id : stream id
 * @ops : stream operation
 * @slot : stream pcm slot
 * @device : device type
 *
 * this inline function initialzes stream context for allocated stream
 */
static inline void sst_init_stream(struct stream_info *stream,
		int codec, int ops, u8 slot,
		enum snd_sst_audio_device_type device)
{
	stream->status = STREAM_INIT;
	stream->codec = codec;
	stream->str_type = 0;
	stream->ops = ops;
	stream->pcm_slot = slot;
	stream->device = device;
}


/*
 * sst_validate_strid - this function validates the stream id
 *
 * @str_id : stream id to be validated
 *
 * returns 0 if valid stream
 */
static inline int sst_validate_strid(int str_id)
{
	if (str_id <= 0 || str_id >= SND_SST_MAX_AUDIO_DEVICES) {
		pr_err("SST ERR: invalid stream id : %d\n",
					str_id);
		return -EINVAL;
	} else
		return 0;
}

static inline int sst_shim_write(void __iomem *addr, int offset, int value)
{

	writel(value, addr + offset);
	return 0;
}

static inline int sst_shim_read(void __iomem *addr, int offset)
{
	return readl(addr + offset);
}

static inline void sst_pm_runtime_get_sync(struct device *x)
{
	pm_runtime_get_sync(x);

	pr_debug("after rtpm_get_sync, usage count = %d\n",
	atomic_read(&((x)->power.usage_count)));
}

static inline void sst_pm_runtime_put(struct device *x)
{
	pm_runtime_put(x);

	pr_debug("after rtpm_put, usage count = %d\n",
	atomic_read(&((x)->power.usage_count)));
}

void register_usr_pid(struct stream_info *stream, int pid);

int sst_prepare_fw(void);

/* #define pr_debug pr_err */

void sst_print_sram(void);
void sst_reset_sram(void);
#endif /* __INTEL_SST_COMMON_H__ */

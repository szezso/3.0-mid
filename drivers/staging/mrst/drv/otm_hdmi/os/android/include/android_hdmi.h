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

#ifndef __ANDROID_HDMI_H
#define __ANDROID_HDMI_H

#include <linux/types.h>
#include <drm/drmP.h>

#define CEA_EXT     0x02
#define VTB_EXT     0x10
#define DI_EXT      0x40
#define LS_EXT      0x50
#define MI_EXT      0x60

/* TODO: move cea_861b_adb_t and hdmi_eeld_t into PIL */
/* Header = 4, Baseline Data = 80 and Vendor (INTEL) specific = 2 as per
 * EELD spec
 * 4 + 80 + = 84
 */
#define HDMI_EELD_SIZE 84
typedef union _hdmi_eeld {
	uint8_t eeld[HDMI_EELD_SIZE];
	#pragma pack(1)
	struct {
		/* Byte[0] = ELD Version Number */
		union {
			uint8_t   byte0;
			struct {
				uint8_t reserved:3; /* Reserf */
				uint8_t eld_ver:5; /* ELD Version Number */
						/* 00000b - reserved
						 * 00001b - first rev
						 * 00010b:11111b - reserved
						 * for future
						 */
			};
		};

		/* Byte[1] = Vendor Version Field */
		union {
			uint8_t vendor_version;
			struct {
				uint8_t reserved1:3;
				uint8_t veld_ver:5; /* Version number of the ELD
						     * extension. This value is
						     * provisioned and unique to
						     * each vendor.
						     */
			};
		};

		/* Byte[2] = Baseline Lenght field */
		uint8_t baseline_eld_length; /* Length of the Baseline structure
					      *	divided by Four.
					      */

		/* Byte [3] = Reserved for future use */
		uint8_t byte3;

		/* Starting of the BaseLine EELD structure
		 * Byte[4] = Monitor Name Length
		 */
		union {
			uint8_t byte4;
			struct {
				uint8_t mnl:5;
				uint8_t cea_edid_rev_id:3;
			};
		};

		/* Byte[5] = Capabilities */
		union {
			uint8_t capabilities;
			struct {
				uint8_t hdcp:1; /* HDCP support */
				uint8_t ai_support:1;   /* AI support */
				uint8_t connection_type:2; /* Connection type
							    * 00 - HDMI
							    * 01 - DP
							    * 10 -11  Reserved
							    * for future
							    * connection types
							    */
				uint8_t sadc:4; /* Indicates number of 3 bytes
						 * Short Audio Descriptors.
						 */
			};
		};

		/* Byte[6] = Audio Synch Delay */
		uint8_t audio_synch_delay; /* Amount of time reported by the
					    * sink that the video trails audio
					    * in milliseconds.
					    */

		/* Byte[7] = Speaker Allocation Block */
		union {
			uint8_t speaker_allocation_block;
			struct {
				uint8_t flr:1; /*Front Left and Right channels*/
				uint8_t lfe:1; /*Low Frequency Effect channel*/
				uint8_t fc:1;  /*Center transmission channel*/
				uint8_t rlr:1; /*Rear Left and Right channels*/
				uint8_t rc:1; /*Rear Center channel*/
				uint8_t flrc:1; /*Front left and Right of Center
						 *transmission channels
						 */
				uint8_t rlrc:1; /*Rear left and Right of Center
						 *transmission channels
						 */
				uint8_t reserved3:1; /* Reserved */
			};
		};

		/* Byte[8 - 15] - 8 Byte port identification value */
		uint8_t port_id_value[8];

		/* Byte[16 - 17] - 2 Byte Manufacturer ID */
		uint8_t manufacturer_id[2];

		/* Byte[18 - 19] - 2 Byte Product ID */
		uint8_t product_id[2];

		/* Byte [20-83] - 64 Bytes of BaseLine Data */
		uint8_t mn_sand_sads[64]; /* This will include
					   * - ASCII string of Monitor name
					   * - List of 3 byte SADs
					   * - Zero padding
					   */

		/* Vendor ELD Block should continue here!
		 * No Vendor ELD block defined as of now.
		 */
	};
	#pragma pack()
} hdmi_eeld_t;

typedef struct _cea_861b_adb {
#pragma pack(1)
	union {
		uint8_t byte1;
		struct {
			uint8_t   max_channels:3; /* Bits[0-2] */
			uint8_t   audio_format_code:4; /* Bits[3-6],
							see AUDIO_FORMAT_CODES*/
			uint8_t   b1reserved:1; /* Bit[7] - reserved */
		};
	};
	union {
		uint8_t	byte2;
		struct {
			uint8_t sp_rate_32kHz:1; /*Bit[0] sample rate=32kHz*/
			uint8_t sp_rate_44kHz:1; /*Bit[1] sample rate=44kHz*/
			uint8_t sp_rate_48kHz:1; /*Bit[2] sample rate=48kHz*/
			uint8_t sp_rate_88kHz:1; /*Bit[3] sample rate=88kHz*/
			uint8_t sp_rate_96kHz:1; /*Bit[4] sample rate=96kHz*/
			uint8_t sp_rate_176kHz:1; /*Bit[5] sample rate=176kHz*/
			uint8_t sp_rate_192kHz:1; /*Bit[6] sample rate=192kHz*/
			uint8_t sp_rate_b2reserved:1; /* Bit[7] - reserved*/
		};
	};
	union {
		uint8_t   byte3; /* maximum bit rate divided by 8kHz */
		/* following is the format of 3rd byte for
		 * uncompressed(LPCM) audio
		 */
		struct {
			uint8_t	bit_rate_16bit:1;	/* Bit[0] */
			uint8_t	bit_rate_20bit:1;	/* Bit[1] */
			uint8_t	bit_rate_24bit:1;	/* Bit[2] */
			uint8_t	bit_rate_b3reserved:5;	/* Bits[3-7] */
		};
	};
#pragma pack()

} cea_861b_adb_t;

struct android_hdmi_priv {
	/* common */
	struct drm_device *dev;

	/* oaktrail specific */
	/* PCI related parameters are not needed to be exposed.
	 * Need to remove.
	 */
	void *regs;
	int dpms_mode;
	struct hdmi_i2c_dev *i2c_dev;


	/*medfield specific */
	u32 hdmib_reg;
	u32 save_HDMIB;
	bool has_hdmi_sink;
	/* Should set this when detect hotplug */
	bool hdmi_device_connected;
	struct mdfld_hdmi_i2c *i2c_bus;
	/* EELD packet holder*/
	hdmi_eeld_t eeld;
	u32 hdmi_eeld_size;
	cea_861b_adb_t lpcm_sad;
	bool is_hdcp_supported;
	struct i2c_adapter *hdmi_i2c_adapter;   /* for control functions */
	/*TODO: remove this and use from context when set_property is
	 * implemented in OTM.
	 */
	int monitor_type;
	void *context;
};

/* Global devices for switch class used for hotplug notification */
extern struct switch_dev g_switch_hdmi_dev;
extern struct switch_dev g_switch_dvi_dev;

/* TODO: remove this once all code moved into OTM */
extern const struct drm_encoder_funcs psb_intel_lvds_enc_funcs;
extern const struct drm_encoder_helper_funcs mdfld_hdmi_helper_funcs;
extern const struct drm_connector_funcs mdfld_hdmi_connector_funcs;
extern const struct drm_connector_helper_funcs
			mdfld_hdmi_connector_helper_funcs;

/* TODO: do this down the layers. */
extern int psb_intel_panel_fitter_pipe(struct drm_device *dev);

/* TODO: medfiled specific */
extern void mdfld_hdmi_audio_init(struct android_hdmi_priv *p_hdmi_priv);

extern void mdfld_msic_init(struct android_hdmi_priv *p_hdmi_priv);

#ifdef CONFIG_MDFD_HDMI

extern void android_hdmi_driver_init(struct drm_device *dev,
						void *mode_dev);

extern void android_hdmi_enable_hotplug(struct drm_device *dev);

extern void android_hdmi_driver_setup(struct drm_device *dev);

extern void android_hdmi_context_init(void *context);

extern int android_hdmi_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode);

extern int android_hdmi_get_modes(struct drm_connector *connector);

/*
 * Description: crtc mode set for hdmi pipe.
 *
 * @crtc:		crtc
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @x, y, old_fb:	old frame buffer values used for flushing old plane.
 *
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */
extern int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb);

/*
 * Description: encoder mode set for hdmi pipe.
 *
 * @encoder:		hdmi encoder
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	none.
 */
extern void android_hdmi_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);

/*
 * Allocates the hdmi buffers of specified dimensions.
 * Initializes Scaling related paramters.
 * input parameters:
 *	psDrmDev: Drm Device.
 *	ui32HdmiWidth, ui32HdmiHeight: HDMI mode
 *	ui32BufferCount,: number of HDMI buffers to allocate.
 *	bpp: bits per pixel to consider for allocating the buffer.
 *	ui32LvdsWidth, ui32LvdsHeight: Native display (LVDS) Mode.
 * returns '0' on success and other values on failure.
 */
extern int android_hdmi_setup_hdmibuffers(struct drm_device *psDrmDev,
				u32 ui32HdmiWidth, u32 ui32HdmiHeight,
				u32 ui32BufferCount, int bpp, u32 ui32LvdsWidth,
				u32 ui32LvdsHeight);

/*
 * Store the HDMI registers and enable the display
 * Input parameters:
 *	psDrmDev: Drm Device.
 * Returns: none
 */
extern void android_hdmi_restore_and_enable_display(struct drm_device *dev);

/*
 * Save the HDMI display registers
 * Input parameters:
 *	psDrmDev: Drm Device.
 * Returns: none
 */
extern void android_hdmi_save_display_registers(struct drm_device *dev);

/*
 * disable HDMI display
 * Input parameters:
 *	psDrmDev: Drm Device.
 * Returns: none
 */
extern void android_disable_hdmi(struct drm_device *dev);


/*
 * Description: hdmi helper function to detect whether hdmi/dvi
 *		is connected or not.
 *
 * @connector:	hdmi connector
 *
 * Returns:	connector_status_connected if hdmi/dvi is connected.
 *		connector_status_disconnected if hdmi/dvi is not connected.
 */
extern enum drm_connector_status android_hdmi_detect(struct drm_connector
								*connector);

/*
 * Description: hdmi helper function to manage power to the display (dpms)
 *
 * @encoder:	hdmi encoder
 * @mode:	dpms on or off
 *
 * Returns:	none
 */
extern void android_hdmi_dpms(struct drm_encoder *encoder,
				int mode);

#else /* CONFIG_MDFD_HDMI */

static inline void android_hdmi_driver_init(struct drm_device *dev,
						void *mode_dev) {}

static inline void android_hdmi_enable_hotplug(struct drm_device *dev) {}

static inline void android_hdmi_driver_setup(struct drm_device *dev) {}

static inline void android_hdmi_context_init(void *context) {}

static inline int android_hdmi_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode) { return 0; }

static inline int android_hdmi_get_modes(struct drm_connector *connector) { return 0; }

static inline int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb) { return 0; }

static inline void android_hdmi_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode) {}

static inline int android_hdmi_setup_hdmibuffers(struct drm_device *psDrmDev,
				u32 ui32HdmiWidth, u32 ui32HdmiHeight,
				u32 ui32BufferCount, int bpp, u32 ui32LvdsWidth,
				u32 ui32LvdsHeight) { return 0; }

static inline void android_hdmi_restore_and_enable_display(struct drm_device *dev) {}

static inline void android_hdmi_save_display_registers(struct drm_device *dev) {}

static inline void android_disable_hdmi(struct drm_device *dev) {}

static inline enum drm_connector_status android_hdmi_detect(struct drm_connector
								*connector)
{ return connector_status_disconnected; }
static inline void android_hdmi_dpms(struct drm_encoder *encoder,
				int mode) {}


#endif /* CONFIG_MDFD_HDMI */

/*
 * Description: hdmi helper function to parse cmdline option
 *		from hdmicmd tool
 *
 * @cmdoption:	cmdline option
 *
 * Returns:	error codes 0(success),-1(cmd option),-2(invalid input)
 */
int otm_cmdline_parse_option(char *cmdoption);

/*
 * Description: hdmi helper function to parse vic option
 *		from hdmicmd tool
 *
 * @cmdoption:	cmdline option
 *
 * Returns:	error codes 0(success),-1(error)
 */
int otm_cmdline_set_vic_option(int vic);

/*
 * Description: hdmi helper function to print cmdline options
 *		from hdmicmd tool
 *
 * Returns:	none
 */
void otm_print_cmdline_option(void);

/*
 * Description: hdmi helper function to print edid information
 *		from report_edid tool
 *
 * Returns:	none
 */
void test_otm_hdmi_report_edid_full(void);

#endif /* __ANDROID_HDMI_H */

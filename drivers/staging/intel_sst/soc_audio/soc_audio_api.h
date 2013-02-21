/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

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

  Copyright(c) 2006-2011 Intel Corporation. All rights reserved.
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

#ifndef _SOC_AUDIO_API_H
#define _SOC_AUDIO_API_H

#include "soc_audio_common.h"

/*! \file soc_audio_api.h
 * \brief SOC Audio Processor Interface
 * \author Lomesh Agarwal, Arun Kannan
 * \version 1.0
 *
 * The following diagram shows the typical stack in the modular driver
 * architecture framework.
 *
 * <CENTER>
 *
 *             Application
 *
 *     <BR>---------------------------------------------------------
 *
 *              <BR>Middleware
 *
 *     <BR>---------------------------------------------------------
 *
 *           <BR>OS Specific Layer
 *
 *    <BR><I> ----------------Audio Processor Interface----------------</I>
 *
 *       <BR>OS & IP Independent Layer
 *
 *     <BR><I>---------Inter Processor Communication Interface---------</I>
 *
 *           <BR>IP Specific Layer
 *
 *     <BR>---------------------------------------------------------
 *
 *             <BR>Audio DSP Firmware
 *
 * </CENTER>
 *
 * This file defines the interface for the IP and OS independent layer for the
 * audio driver. If the interfaces can be used for all drivers like audio,
 * security, graphics to abstract part of processing that is independent of
 * hardware and OS.
 * Rest of the interfaces are specific to audio (like pipeline management,
 *  volume control).
 * The interfaces defined here for the audio driver are based on the concept
 * of an Audio Processor. The Audio Processor can be viewed as a black
 * box by the upper layers of software. The Audio processor takes multiple
 * inputs and outputs. Each of these inputs/outputs can specify the data
 * format and specific parameters.
 * The Audio processor uses this information to create a suitable
 * audio pipeline to be executed by the  audio DSP.
 *
 * The Interfaces defined in this file can be grouped into four categories.
 *   -# Generic System Level Interface
 *   -# Audio Processor Interface
 *   -# Audio Processor Input Interface
 *   -# Audio Processor Output Interface
 *
 * The following sections detail each of the Interface Categories and the APIs
 * contained.
 */

/*! @name Generic System Level Interface */

/**
This API is used to initialize state and structures internal to this layer.

@param[in,out] in_out_init_data
		This structure is used to provide init time arguments including
		function callbacks required by the driver.

@returns SOC_SUCCESS if the initialization is successful.
@returns SOC_ERROR_INVALID_PARAMETER if in_init_data is not a valid pointer.
@returns SOC_FAILURE if any of the required parameters are invalid.
*/
enum soc_result soc_init(struct soc_audio_init_data *in_out_init_data);

/**
This API is used during final driver clean-up to release all internal
structures and state.

@returns None
*/
void soc_destroy(void);

/**
This API opens and returns a handle to an Audio Processor if an instance is
available. The handle will be used to manage the audio processing and
alows the user to add inputs and outputs to the instance. Only a single
global processor instance can be opened on the system.
If the global processor is already created, the handle to the already
created global processor is returned.
If other audio processors are already open, then the physical inputs
and outputs assigned to other processors cannot be added to this processor.

@param[in]  in_is_global
			Boolean value to indicate if the requested instance is a
			global processor.
@param[out] out_processor_h
			Handle to the Audio Processor instance.

@retval SOC_SUCCESS if the Audio Processor instance was created successfully.
@retval SOC_FAILURE if no handles are available or unable to allocate
					required memory.
*/
enum soc_result
soc_audio_open_processor(bool in_is_global, uint32_t *out_processor_h);

/**
This API frees up the instance of the processor specifed by the handle.
It cleans up any resources associated with the instance.
A call to close processor will also remove and inputs or outputs connected.

@param[in] in_processor_h
			Handle to the Audio Processor instance.

@retval SOC_SUCCESS if the Audio Processor instance was closed and
			resources freed.
@retval SOC_ERROR_INVALID_PARAMETER if the handle supplied is not a valid
				 processor handle.
@retval SOC_FAILURE if there was an internal error during the close operation.

*/
enum soc_result soc_audio_close_processor(uint32_t in_processor_h);

/**
Enables watermark detection on the Audio Processor.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] enable : flag to enable/disable watermark.
@retval SOC_AUDIO_SUCCESS : Watermark detection was successfully enabled.
@retval SOC_AUDIO_FAILURE : Error in enabling watermark detection.
@retval SOC_AUDIO_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result
soc_audio_set_watermark_detection(uint32_t processor_h,
				  void *audio_wm_params,
				  uint32_t offset, uint32_t length);

/**
Get last detected watermark from the Audio Processor. If no watermark is
present the
fucntion returns in error.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[out] studio_watermark : Studio watermark return value.
@param[out] theatrical_watermark : Theatrical watermark return value.

@retval SOC_AUDIO_SUCCESS : Watermark was present and returned successfully.
@retval SOC_AUDIO_FAILURE : No watermark was present in the stream.
@retval SOC_AUDIO_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result
soc_audio_get_watermarks(uint32_t processor_h,
			 void *audio_wm_params,
			 uint32_t offset, uint32_t length);

/**
Enable and set master volume of the mixed output of the Audio Processor.
Call this function multiple times to adjust volume as desired. When the audio
processor is muted, calls to this API succeed, but they do not take effect
until the processor is un-muted. The master volume is added to each per-
channel volume setting to create the overall volume of each channel of the
mixed output.

Note that this API does not guarantee audio quality when gain values greater
than 0dB are specified for the master_volume.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] master_volume : Master volume level.

@retval SOC_SUCCESS : Setting was successfully applied to the Audio Processor.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result
soc_audio_set_master_volume(uint32_t processor_h, int32_t master_volume);

/**
Retrieve the current master volume setting.  Note that this function will
return the last value specified in a call to soc_audio_set_master_volume,
even when the audio processor is muted.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[out] master_volume : Current master volume level.

@retval SOC_SUCCESS : Successfully get the info.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result
soc_audio_get_master_volume(uint32_t processor_h,
			    int32_t *maseter_volume);

/**
Disable master volume control on the Audio Processor.  Calling this function
sets the master valume value to 0dB (no change).  When the audio processor is
muted, calls to this function succeed, but they do not take effect until the
processor is un-muted.

@param[in] processor_h : Handle to the Audio Processor instance.

@retval SOC_SUCCESS : Setting was successfully applied to the Audio Processor.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result soc_audio_disable_master_volume(uint32_t processor_h);

/**
Mute or unmute the mixed output of the Audio Processor. A call to this
function with mute set to true effectively mutes all outputs of the audio
processor simultaneously. The previous mixer output gain settings are saved
and will be restored when this function is called with mute set to false.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] mute : Set true to mute, set false to un-mute.

@retval SOC_SUCCESS : Setting was successfully applied to the Audio Processor.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result soc_audio_mute(uint32_t processor_h, bool mute);

/**
Enable and set the volume of each channel of the mixed output of the audio
processor. Call this function multiple times to adjust per channel volume
as desired. The per-channel volumes are added to the master volume setting
to create the overall volume fore each channel of the mixed output. When
the audio processor is muted, calls to this API succeed, but they do not
take effect until the processor is un-muted.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] channel_volume : Structure indicating the volume for each
		channel of the mixed output.

@retval SOC_SUCCESS : Setting was successfully applied to the Audio Processor.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result
soc_audio_set_per_channel_volume(uint32_t processor_h,
				 struct soc_audio_per_channel_volume
				 channel_volume);

/**
Retrieve the per-channel volume control settings of the mixed output of
the audio processor.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[out] channel_volume : Struture to hold the volume for the each channel.

@retval SOC_SUCCESS : Successfully get the info.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result soc_audio_get_per_channel_volume(uint32_t processor_h,
						 struct
						 soc_audio_per_channel_volume
						 *channel_volume);

/**
Disable per-channel volume control of the mixed output of the Audio Processor.
This function sets the per-channel volume back to 0dB (no change) for all
channels.  When the audio processor is muted, calls to this API succeed,
but they do not take effect until the processor is un-muted again.

@param[in] processor_h : Handle to the Audio Processor instance.

@retval SOC_SUCCESS : Setting was successfully applied to the Audio Processor.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameters.
*/
enum soc_result soc_audio_disable_per_channel_volume(uint32_t processor_h);

/******************************************************************************/
/*! @name Audio Delay Management APIs*/

/**
Set and enable a per-channel delay in the range of 0 - 255 ms (1 ms step).
Call this function multiple times to change delay.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] channel_delay : Structure of delay values for each channel,
			   range: 0 - 255 ms (1 ms step).

@retval SOC_SUCCESS : The per channel delay was applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : One or more of the delay values
				      specified was out of range.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_set_per_channel_delay(uint32_t processor_h, struct
						soc_audio_per_channel_delay
						channel_delay);

/**
Get current per-channel delay values.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[out] channel_delay : Structure of delay values for each channel,
			    range: 0 - 255 ms (1 ms step).

@retval SOC_SUCCESS : The per channel delay values were returned successfully.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_get_per_channel_delay(uint32_t processor_h, struct
						soc_audio_per_channel_delay
						*channel_delay);

/**
Disables the per channel delay on the Audio Processor.
No delay is applied after a call to this function.

@param[in] processor_h : Handle to the Audio Processor instance.

@retval SOC_SUCCESS : Per channel delay was successfully disabled.
@retval SOC_ERROR_OPERATION_FAILED : An unexpected error occured while
				    attempting to disable per-channel dealy.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_disable_per_channel_delay(uint32_t processor_h);

/******************************************************************************/
/*! @name Audio Output Quality Control APIs*/

/**
Mute the volume of the specified output.
@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] output_h : Handle to the audio output instance.
@param[in] mute : True to mute, false to un-mute.
@param[in] ramp_ms : Ramp up/down in units of 0.1 milliseconds.

@retval SOC_SUCCESS : Output audio successfully muted.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_mute(uint32_t processor_h,
				      void *output_h,
				      bool mute, int32_t ramp_ms);

/**
Get the current mute status of the output.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] output_h : Handle to the audio output instance.
@param[out] is_muted : True if muted, false if not muted.

@retval SOC_SUCCESS : Output audio successfully muted.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_is_muted(uint32_t processor_h,
					  void *output_h, bool * is_muted);

/**
Set the volume of the output.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] output_h : Handle to the audio output instance.
@param[in] gain : The gain value to be applied to the output.
@param[in] ramp_ms : Ramp up/down in milliseconds.

@retval SOC_SUCCESS : The volume setting was applied.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_set_volume(uint32_t processor_h,
					    void *output_h,
					    int32_t gain, int32_t ramp_ms);

/**
Get the current volume setting on the output.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] output_h : Handle to the audio output instance.
@param[out] gain : The current gain value being applied to the output.

@retval SOC_SUCCESS : The volume setting was returned.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_get_volume(uint32_t processor_h,
					    void *output_h, int32_t * gain);

/**
Releases all allocations of all filter handles and control handles and puts
the Audio Quality pipeline in bypass mode.This function is the starting point
when there is a need to set up an entirely new Audio Quality pipeline. It also
can be used to disable the Audio Quality pipeline. Changes made by this
function will go into effect after calling the \ref
soc_audio_output_quality_start function.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.

@retval SOC_SUCCESS : The resources associated to the Audio Quality
pipeline are released.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_reset(uint32_t processor_h,
					       void *output_h);

/**
After makeing changes to the Audio Quality pipeline configuration, or flow,
and the parameters and types for each of the stages, this function is called
to make the latest configuration effective.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.

@retval SOC_SUCCESS : The resources associated to the Audio Quality pipeline
		      are released.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_start(uint32_t processor_h,
					       void *output_h);

/**
Allocate an audio filter to be used a audio quality pipeline and return the
handle to that filter.
See \ref SOC_AUDIO_QUALITY_MAX_FILTERS for the number of filters available.
Multiple channels and stages in the quality pipeline can reference this filter.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] type : Type of filter.
@param[out]  filter_h: Handle use to reference this filter instance.

@retval SOC_SUCCESS : Filter is allocated and handle returned successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
@retval SOC_ERROR_NO_RESOURCES : No more filter instances available.
*/
enum soc_result soc_audio_output_quality_filter_alloc(uint32_t processor_h,
		      void *output_h, enum
		      soc_audio_quality_filter_type
		      type, int32_t *filter_h);

/**
Allocate an audio control to be used a audio quality pipeline and return the
handle to that control.
See \ref SOC_AUDIO_QUALITY_MAX_CONTROLS for the number of controls available.
Multiple channels and stages in the quality pipeline can reference this control.
There are some controls that can only be allocated once they are Volume, Mute
and the AVL control.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] type : Type of control.
@param[out]  filter_h: Handle use to reference this filter instance.

@retval SOC_SUCCESS : Control is allocated and handle returned successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
@retval SOC_ERROR_NO_RESOURCES : No more control instances available.

*/
enum soc_result soc_audio_output_quality_control_alloc(uint32_t processor_h,
				void *output_h, enum
				soc_audio_quality_control_type
				type,
				int32_t *control_h);

/**
Set the parameters of a audio filter. A valid handle to the filter must be
allocated first. The type of the filter describes whether this is a peaking,
or shelf, or low or high pass, and of what order, etc..For some filters,
the Q factor and/or Gain are not relevant. In this case, the parameters
are ignored, set to 0.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] filter_h: Handle use to reference this filter instance.
@param[in] type: Specifies the type of filter to be used.
@param[in] params: Specifies the filter parameters.
		    See \ref soc_audio_quality_filter_params_t.

@retval SOC_SUCCESS : Filter settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_filter_config(uint32_t processor_h,
					void *output_h,
					int32_t filter_h, struct
					soc_audio_quality_filter_params
					params);

/**
Sets the parameters of an audio control. A valid handle to a control must be
obtained first. The number of parameters can vary by control.
Use \ref soc_audio_quality_control_params_t structure to specify the paramters
and the parameters count. Also refer to ismd_audio_quality_control_params_t for
detailed description of params needed for each control.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] filter_h: Handle use to reference this filter instance.
@param[in] params: Specifies the control parameters.
		    See \ref soc_audio_quality_filter_params_t.

@retval SOC_SUCCESS : Control settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_control_config(uint32_t processor_h,
				void *output_h,
				int32_t control_h, union
				soc_audio_quality_control_params
				params);

/**
Sets a particular channel and stage of the Audio Quality pipeline within that
channel to a control.Several channels or stages may all refer to the same
control with no confusion. The stage numbering starts at 1.
Note that stage 1 is the first stage the audio encounters for a given channel,
stage 2 is next,and so on. This function sets up the order stages are called
for each channel. In other words, the pipeline configuration.If a stage is set
to the invalid handle - handle 0, it signals that that is the end of the line
for processing.Therefore a long chain of stages can be cut short by setting
one of them to handle 0. At that point, the data is nolonger passed down to
other stages.
NOTE: The ISMD_AUDIO_CONTROL_AVL type control should NOT be added using this
function. Once the AVL control is allocated and configured. It will then be a
part of the pipeline and always be processed as the last stage.
There is no need to track the stage number of the AVL control.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] control_h: Handle to a filter or control.
@param[in] channel:  The channel to apply the control to.
@param[in] stage: The stage location of the control.

@retval SOC_SUCCESS : Settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor handle, output handle
				      or control handle.
*/
enum soc_result soc_audio_output_quality_stage_config_control(uint32_t
							      processor_h,
							      void *output_h,
							      int32_t control_h,
							      enum
							      soc_audio_channel
							      channel,
							      int32_t stage);

/**
Sets a particular channel and stage of the Audio Quality pipeline within that
channel to a filter.Several channels or stages may all refer to the same filter
with no confusion. The stage numbering starts at 1. Note that stage 1 is the
first stage the audio encounters for a given channel, stage 2 is next,
and so on. This function sets up the order stages are called for each channel.
In other words, the pipeline configuration.
If a stage is set to the invalid handle - handle 0, it signals that that is
the end of the line for processing.
Therefore a long chain of stages can be cut short by setting one of them
to handle 0. At that point, the data is no longer passed down to other stages.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] filter_h: Handle to a filter.
@param[in] channel:  The channel to apply the filter.
@param[in] stage: The stage location of the filter.

@retval SOC_SUCCESS : Settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_stage_config_filter(uint32_t
							     processor_h,
							     void *output_h,
							     int32_t filter_h,
							     enum
							     soc_audio_channel
							     channel,
							     int32_t stage);

/**
Turn bypass mode on or off for a given stage in the given channel. This will
result in no application of the control/filter for the given channel.
If a stage is bypassed, and later bypassing is turned off, it reverts to the
control/filter assignment given previously.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] channel:  The channel to operate on.
@param[in] stage: The stage location of the control.
@param[in] bypass: Specify true or false to bypass or turn off bypass.

@retval SOC_SUCCESS : Settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_quality_stage_bypass(uint32_t processor_h,
						      void *output_h,
						      enum soc_audio_channel
						      channel, int32_t stage,
						      bool bypass);

/**
Set the parameters of any of the fixed (in stage position) audio controls.
Since these are already instantiated, they require no allocation or handle.
There is only one instance for fixed controls and they can be referenced
by their ISMD type identifier. The ISMD type of the control references the AVL,
MUTE, Master volume, DUP stage or MiniMixer. The parameters are a union of
several structures - each structure with the appropriate parms for the type
of control being set. Some controls have a different set point for the
parameters for each channel, and this function is called once for each channel
to be configured. In the case of the ones that do not have per channel
parameters, the OutChannel parameter is ignored.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] OutChannel: output channel to use for this set of parameters
@param[in] type: Specifies the type of control to be set.
@param[in] params: Specifies the filter parameters.
		    See \ref soc_audio_quality_filter_params_t.

@retval SOC_SUCCESS : Filter settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid parameter or type.

*/
enum soc_result
soc_audio_output_quality_set_fixed_controls(uint32_t processor_h,
					    void *output_h, int32_t OutChannel,
					    enum soc_audio_quality_control_type
					    type, union
					    soc_audio_quality_control_params
					    params);

/**
Set the bypass switch for the fixed controls.  of a audio filter. Since these
are already instantiated,they require no allocation or handle. There is only
one instance for fixed controls and they can be referenced by their ISMD type
identifier. The SOC_AUDIO type of the control references the AVL, MUTE,
Master volume, DUP stage or MiniMixer. The control is set to be bypassed
(turned off) or activated (bypass = false).

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] output_h : Output handle.
@param[in] fctype: Fixed control type to be bypassed, or activated.
@param[in] bypass: true if fixed control is to be bypassed, false otherwise.

@retval SOC_SUCCESS : Filter settings applied successfully.
@retval SOC_ERROR_INVALID_PARAMETER : Control not a fixed control or does not
				      have a bypass.
*/

enum soc_result
soc_audio_output_quality_fc_bypass(uint32_t processor_h,
				   void *output_h,
				   enum soc_audio_quality_control_type fctype,
				   bool bypass);

/**
Enable or disable processing of mixing metadata in this processor.  Mixing
metadata is extracted from the secondary stream and is generally embedded
in the compressed bitstream (e.g., DTS-LBR or Dolby Digital Plus).  If
mixing metadata is enabled, then the mixer will use gains specified by
the stream. If mixing metadata is enabled but the secondary stream has no
mixing metadata, the user-specified gain values will continue to be used
until mixing metadata is available from the stream.  When mixing metadata
is disabled, the user-specified gains are always used.

If mixing metadata is disabled, the mixer will used gains
specified by the user in soc_audio_input_set_channel_mix.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] enable : Enable (true) or disable (false) mix metadata.

@retval SOC_SUCCESS : Mix metadata was successfully enabled or disabled.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor handle.
*/
enum soc_result
soc_audio_enable_mix_metadata(uint32_t processor_h, bool enable);

/**
Set the channel configuration mode of the mixer. This optional call allows
control over how the mixer will operate on its inputs and the channel
configuration it will output.

If ch_cfg_mode is set to SOC_AUDIO_MIX_CH_CFG_MODE_AUTO
the processor will determine the least computationally intensive (MIPS saving)
setup based on the input and output configurations. This is the default mode
of the mixer.

If ch_cfg_mode is set to SOC_AUDIO_MIX_CH_CFG_MODE_PRIMARY
this means the mixer will always look at the primary input to the mixer and
output it's channel configuration (mode used in Blu-Ray), if no primary input
has been selected the mixer will fall back to SOC_AUDIO_MIX_CH_CFG_MODE_AUTO
until a primary input is specified.

If ch_cfg_mode is set to SOC_AUDIO_MIX_CH_CFG_MODE_FIXED the ch_cfg
variable will be used as the configuration the mixer operates on and outputs. An
example of a valid fixed ch_cfg (channel config) would be 0xFFFFFF20
(Left and Right only).
Please see \ref soc_audio_channel for channel defines.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] ch_cfg_mode : The channel configuration mode the mixer will
operate on.
@param[in] ch_cfg : The channel configuration used when ch_cfg_mode is
set to SOC_AUDIO_MIX_CH_CFG_MODE_FIXED.

@retval SOC_SUCCESS : The mixer channel configuration was set.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor handle.
@retval SOC_ERROR_INVALID_PARAMETER : ch_cfg_mode or ch_cfg is invalid.
*/
enum soc_result
soc_audio_set_mixing_channel_config(uint32_t processor_h,
				    enum soc_audio_mix_ch_cfg_mode
				    ch_cfg_mode, int ch_cfg);

/**
Use this optional function call to set the sampling rate mode of the mixer.

If sample_rate_mode is set to SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO,
the processor will determine the least computationally intensive setup
based on the input and output configurations. This is the default mode
of the mixer.

If sample_rate_mode is set to SOC_AUDIO_MIX_SAMPLE_RATE_MODE_PRIMARY
this means the processor will always look at the primary input to the
mixer and use it's sampling frequency and convert all other inputs to
that frequency before mixing. If no primary input has been selected,
the mixer will fall back to SOC_AUDIO_MIX_SAMPLE_RATE_MODE_AUTO
until a primary input is specified.

If sample_rate_mode is set to SOC_AUDIO_MIX_SAMPLE_RATE_MODE_FIXED,
the sample_rate variable will be used to specify the sampling rate
that all inputs to the mixer will be converted to before mixing.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] sample_rate_mode: The mixing sample rate mode.
@param[in] sample_rate: The sample rate used when the mode is
SOC_AUDIO_MIX_SAMPLE_RATE_MODE_FIXED.

@retval SOC_AUDIO_SUCCESS : The mixing sample rate was successfully set.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid arguments.
*/
enum soc_result
soc_audio_set_mixing_sample_rate(uint32_t processor_h,
				 enum soc_audio_mix_sample_rate_mode
				 sample_rate_mode, int sample_rate);

/**
Use this optional function call to get the sampling rate mode of the mixer.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] mixer_input_index: Index of the input to be got.
@param[out] sample_rate_mode: The mixing sample rate mode.
@param[out] sample_rate: The sample rate.

@retval SOC_AUDIO_SUCCESS : The mixing sample rate was successfully got.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid arguments.
*/
enum soc_result
soc_audio_get_mixing_sample_rate(uint32_t processor_h,
				 uint32_t mixer_input_index,
				 enum soc_audio_mix_sample_rate_mode
				 *sample_rate_mode, int *sample_rate);
/**
Enable bass managment, set the crossover frequency and speaker
setting of each speaker. Can be called multiple times to change setting as
needed. Bass management is disabled by default if no call is made to this
function.

@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] speaker_settings : Specify which speakers are large or small.
					(LFE not considered)
@param[in] crossover_freq : Variable to specify desired crossover frequency.

@retval SOC_AUDIO_SUCCESS : Bass management was enabled and setting was
applied.
@retval SOC_AUDIO_FAILURE : Error in applying settings.
@retval SOC_AUDIO_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_set_bass_management(uint32_t processor_h, struct
					      soc_audio_per_speaker_setting
					      speaker_settings,
					      enum soc_audio_crossover_freq
					      crossover_freq);

/**
Get bass management settings.
@param[in] processor_h : Handle for the Audio Processor instance.
@param[out] speaker_settings : Specify which speakers are large or small.
					(LFE not considered)
@param[out] crossover_freq : Variable to specify desired crossover frequency.
@retval SOC_AUDIO_SUCCESS : Settings successfully returned.
@retval SOC_AUDIO_FAILURE : Bass management is not enabled.
@retval SOC_AUDIO_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_get_bass_management(uint32_t processor_h,
			struct soc_audio_per_speaker_setting
			*speaker_settings,
			enum soc_audio_crossover_freq
			*crossover_freq);

/**
Disable bass managment in the Audio Processor.

@param[in] processor_h : Handle to the Audio Processor instance.

@retval SOC_AUDIO_SUCCESS : Bass management was successfully disabled.
@retval SOC_AUDIO_FAILURE : Error in disabling bass management.
@retval SOC_AUDIO_INVALID_HANDLE : Invalid processor handle.
*/
enum soc_result soc_audio_disable_bass_management(uint32_t processor_h);

/*****************************************************************************/
/*! @name Audio Processor Input Interface */

/**
Add an input to an instance of the Audio Processor. The call to open an
instance of the processor must be called before adding any inputs.
The input must be configured in either a timed or non-timed mode.
If configured as a timed input, the audio data will be rendered
out based on the presentation time stamp (PTS) value associated with
each audio frame.

@param[in] in_processor_h : Handle to the Audio Processor instance.
@param[out] out_handle : Handle to the audio input instance.

@retval SOC_SUCCESS : Input was successfully added to the Audio Processor
			instance.
@retval SOC_FAILURE : Could not create input or no inputs available.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid handle.
@retval SOC_OUT_OF_MEMORY : Out of memory
*/
enum soc_result soc_audio_input_add(uint32_t in_processor_h,
				    void **out_handle);

/**
This API removes an input from the currently playing stream. This
configuration command can be issued at any time. It will remove a previously
added input stream and not disrupt the playback of any other input streams.

@param[in] in_processor_h : Handle to the Audio Processor instance.
@param[in] in_handle : Handle of input to remove.

@retval SOC_SUCCESS : Input was successfully removed
@retval SOC_FAILURE : Error in removing input.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid parameters.
*/
enum soc_result soc_audio_input_remove(uint32_t in_processor_h,
				       void *in_handle);

/**
This API sets the state of a particular input to one of the valid states
defined by soc_dev_state.

@param[in] in_processor_h
			Handle for the Audio Processor instance.
@param[in] in_handle
			Handle to the audio processor input.
@param[in] in_state
			state of input. valid values are PAUSE, STOP or PLAY

@retval SOC_SUCCESS : State was successfully changed for this input.
@retval SOC_FAILURE : Error in setting state for the input.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid assigned state.
*/
enum soc_result
soc_audio_input_set_state(uint32_t in_processor_h,
			  void *in_handle, enum soc_dev_state in_state);

/**
Get decoder parameters on an input configured to decode the input audio data.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] input_handle : Handle to the audio input instance.
@param[in] param_type : ID of the decoder parameter to set.
@param[out] param_value : Buffer to contain the decoder parameter value.

@retval SOC_SUCCESS : Decoder parameter was returned successfully.
@retval SOC_ERROR_INVALID_PARAMETER : The decoder parameter specified is
invalid.
@retval SOC_FAILURE : An unspecified error occurred.
*/
enum soc_result soc_audio_input_get_decoder_param(uint32_t processor_h,
						  void *input_handle,
						  uint32_t param_type,
						  void *param_value);

/**
Set sample rate, sample size, channel config and channel count for pcm stream.

@param[in] in_handle: Handle to the audio input instance.
@param[out] stream_info: Audio stream info to be set.

@retval SOC_SUCCESS: The bit rate was successfully obtained.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid processor or input handle.
*/
enum soc_result soc_audio_input_set_pcm_stream_info(void *in_handle,
						    struct soc_audio_stream_info
						    *stream_info);

/**
Get bit rate, sample rate, sample size, channel config and audio format
(algorithm) for the incoming stream returned all in one structure.
If there is no input audio stream present at the input, the function will
return an error.

@param[in] in_handle: Handle to the audio input instance.
@param[out] stream_info: Current audio stream info.

@retval SOC_SUCCESS: The bit rate was successfully obtained.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid processor or input handle.
*/
enum soc_result
soc_audio_input_get_stream_info(void *in_handle,
				struct soc_audio_stream_info *stream_info);

/**
Flushes the stream out of the pipeline and sets the input/output jobs to
valid status
Returns success or invalid parameter.

@param[in] processor_h: Handle to the Audio Processor instance.
@param[in] handle: Handle to the audio input instance.
@retval SOC_SUCCESS: The stream was flushed successfully
@retval SOC_ERROR_INVALID_PARAMETER: Invalid processor or input handle.
*/
enum soc_result
soc_audio_input_flush_stream(uint32_t processor_h, void *handle);

/**
Get extened format specific stream information of the current input stream.

@param[in] in_handle: Handle to the audio input instance.
@param[out] fmt_info: Current format specific stream info.

@retval SOC_SUCCESS: The stream information was successfully obtained.
@retval SOC_FAILURE: There was no audio data in the stream.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid processor or input handle.
*/
enum soc_result soc_audio_input_get_format_specific_info(void *in_handle,
				struct soc_audio_format_specific_info
				*fmt_info);

/**
Set decoder parameters on an input configured to decode the input audio data.
Call this function after calling \ref soc_audio_input_set_data_format and
before setting the input to a playing state.

@param[in] processor_h : Handle to the Audio Processor instance.
@param[in] input_handle : Handle to the audio input instance.
@param[in] param_type : ID of the decoder parameter to set.
@param[in] param_value : Buffer containing the decoder parameter to set.

@retval SOC_SUCCESS : Decoder parameter was set successfully.
@retval SOC_ERROR_INVALID_PARAMETER : The decoder parameter specified is
invalid.
@retval SOC_FAILURE : An unspecified error occurred.
*/
enum soc_result soc_audio_input_set_decoder_param(uint32_t processor_h,
						  void *input_handle,
						  uint32_t param_type,
						  void *param_value);

/**
Sets the input to be the primary input and will be the source for outputs
set in pass through mode.  If the output does not support the incoming stream,
the Audio Processor will decode the stream and send out LPCM data instead.
Call this function again on the same input to update the list of supported
algorithms or to enable/disable pass through mode.

If application software calls this function  to either select an input source
or change the input source for pass-through mode while the Audio Processor
is in the middle of streaming audio data, the Audio Processor starts processing
the input audio data from the newly selected input source for pass-through
mode. The pass-through audio data from the newly selected input source
traverses through the audio pipeline and eventually reaches pass-through
output point

@param[in] in_handle : Handle to the audio input instance.
@param[in] input_index: Index of this input instance.
@param[in] pass_through : flag to indicate if input is Pass through.

@retval SOC_SUCCESS : The algorithm was successfully returned.
@retval SOC_INVALID_REQUEST : No outputs are configured to recieve the
			pass through data, or another input is already
			assigned as the primary.
@retval SOC_FAILURE : Could not perform operation, unexpected failure.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor or input handle.
*/
enum soc_result
soc_audio_input_set_as_primary(void *in_handle,
			       int32_t input_index, bool pass_through);

/**
Sets the input to be the secondary input on the processor.  This input can
supply metadata that describes how the primary and secondary streams should be
 mixed together.  Currently, this metadata must be encoded into either a
DTS-LBR or Dolby Digital Plus stream and there will be no affect when other
stream types are set as secondary inputs.

@param[in] in_handle : Handle to the audio input instance.
@param[in] input_index: Index of this input instance.

@retval SOC_SUCCESS : The algorithm was successfully returned.
@retval SOC_INVALID_REQUEST : A secondary input already exists on this processor
    or the specified input is a physical input or the primary input on this
    processor.
@retval SOC_FAILURE : Could not perform operation, unexpected failure.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor or input handle.
*/
enum soc_result
soc_audio_input_set_as_secondary(void *in_handle, int32_t input_index);

/**
This API sets which algorithm is used for decode of new data into the
audio pipeline. In the event that data has been already put into the audio
pipeline of another algorithm, all old data will be played out.
To avoid this behavior, call the flush function prior to changing decode
algorithm. It is required to call this function at least once before using
the audio subsystem.

@param[in] in_handle : Handle to the audio input instance.
@param[in] in_format : Algorithm of audio stream about to enter the pipeline.

@retval SOC_SUCCESS : The algorithm was successfully set.
@retval SOC_ERROR_INVALID_PARAMETER : Algorithm supplied is not supported or
				is invalid.
@retval SOC_ERROR_FEATURE_NOT_SUPPORTED : format does not supported
@retval SOC_FAILURE : The algorithm was not set successfully.
*/
enum soc_result
soc_audio_input_set_data_format(uint32_t in_processor_h,
				void *in_handle,
				enum soc_audio_ops_type ops_type,
				enum soc_audio_format in_format);

/**
Set destination channel location and signal strength of input channels when
mixing.

@param[in] processor_h: Handle to the audio processor.
@param[in] input_index: Index of this input instance.
@param[in] ch_mix_config: Stucture to specify mix configuration.

@retval SOC_SUCCESS : Input mix configuration was successfully applied.
@retval SOC_FAILURE : Could not set mix configuration.
@retval SOC_ERROR_INVALID_HANDLE : Invalid processor or input handle.
*/
enum soc_result
soc_audio_input_set_channel_mix(uint32_t processor_h,
				uint32_t input_index,
				struct soc_audio_channel_mix_config
				*ch_mix_config);

/**
 Enable watermark detection on the specified input.

 @param[in] processor_h : Handle to the Audio Processor instance.
 @param[in] input_h : Handle to the audio input instance.

 @retval SOC_SUCCESS : Watermark detection was successfully enabled.
 @retval SOC_FAILURE : Could not enable watermark detection.
 @retval SOC_ERROR_INVALID_HANDLE : Invalid processor or input handle.
 */
enum soc_result
soc_audio_enable_watermark_detection(uint32_t processor_h,
				     void *audio_wm_params,
				     uint32_t offset, uint32_t length);
enum soc_result
soc_audio_disable_watermark_detection(uint32_t processor_h,
				      void *audio_wm_params,
				      uint32_t offset, uint32_t length);

/****************************************************************************/

/*! @name Audio Processor Output Interface */

/**
soc_audio_output_add adds an output to an instance of the audio processor.
It can be called when the processor is in any state.  In the event that the
processor is currently in the playing state the audio processor tries to
get stream output enabled as fast as possible without impacting the other
currently playing streams. An instance of the audio processor must have been
created before calling this function.

@params[in] in_processor_h : Handle to the Audio Processor instance.
@param[in] in_config       : Structure containing the desired configuration
for the audio output.
	@param[out] out_output_h   : Handle to the audio output instance.

	@retval SOC_SUCCESS        : Output was successfully added to the audio
	processor instance.
	@retval SOC_FAILURE        : Error while adding output.
	@retval SOC_ERROR_NO_RESOURCES   : No resources available to addoutput.
@retval SOC_ERROR_INVALID_PARAMETER  : Invalid parameters.
*/
enum soc_result
soc_audio_output_add(uint32_t in_processor_h,
		     struct soc_audio_output_config in_config,
		     void **out_output_h);

/**
soc_audio_output_remove removes an output from the currently playing
stream. This configuration command can be issued at any time.  Any other outputs
from the same stream will continue to play un-interrupted.

	@param[in] in_output_h : Handle of output to remove.

	@retval SOC_SUCCESS : Output was successfully removed for this audio
processor instance.
	@retval SOC_FAILURE : Error in removing the output.
@retval SOC_ERROR_INVALID_PARAMETER: Invalid parameters.
*/
enum soc_result soc_audio_output_remove(uint32_t processor_h,
					void *in_output_h);

/**
This API disables the output. App can reconfigure the output after disabling it.
The output must have been previously added to the Audio Processor instance.

@param[in] in_output_h : Handle to the audio output instance.

@retval SOC_SUCCESS : Output instance was disabled.
@retval SOC_FAILURE : Error in disabling the output.
@retval SOC_ERROR_INVALID_PARAMETER : Invalid processor or output handle.
*/
enum soc_result soc_audio_output_disable(uint32_t processor_h,
					 void *in_output_h);

/**
This API enables the output on a processor.After the output is enabled no
configuration changes can be made untill the output is disabled again.
@param[in] processor_h : Handle for the Audio Processor instance.
@param[in] in_output_h : Handle to the audio output instance.

@retval SOC_SUCCESS : Output instance was enabled.
@retval SOC_FAILURE: Error in enabling the output.
@retval SOC_ERROR_INVALID_PARAMETER : Handle passed in was invalid.
*/
enum soc_result soc_audio_output_enable(uint32_t processor_h,
					void *in_output_h);
enum soc_result soc_audio_output_get_sample_rate(uint32_t processor_h,
						 void *handle,
						 int *sample_rate);

/**
Sets the channel config of the output_wl to the ch_config
@param[in] processor_h :  Handle to the Audio Processor instance.
@param[in] output_h :  Handle to the audio output instance.
@param[in] ch_config : Enum containing the values of channel config
@retval SOC_AUDIO_CHAN_CONFIG_INVALID
@retval SOC_AUDIO_STEREO
@retval SOC_AUDIO_5_1
@retval SOC_AUDIO_7_1
*/
enum soc_result soc_audio_output_set_channel_config(uint32_t processor_h,
						    void *output_h, enum
						    soc_audio_channel_config
						    ch_config);
/**
Sets the output_wl sample size to sample_size
@param[in] processor_h :  Handle to the Audio Processor instance.
@param[in] output_h :  Handle to the audio output instance.
@param[in] sample_size : Sets the output_wl sample size

*/
enum soc_result soc_audio_output_set_sample_size(uint32_t processor_h,
						 void *output_h,
						 uint32_t sample_size);
/**
Sets the output_wl sample rate to sample_rate
@param[in] processor_h :  Handle to the Audio Processor instance.
@param[in] output_h :  Handle to the audio output instance.
@param[in] sample_rate : Sets the output_wl sample rate

*/
enum soc_result soc_audio_output_set_sample_rate(uint32_t processor_h,
						 void *output_h,
						 uint32_t sample_rate);
/**
Sets the output_wl sample rate to sample_rate
@param[in] processor_h :  Handle to the Audio Processor instance.
@param[in] output_h :  Handle to the audio output instance.
@param[in] mode : Enum containing the values of out_mode
@retval SOC_AUDIO_OUTPUT_INVALID
@retval SOC_AUDIO_OUTPUT_PCM
@retval SOC_AUDIO_OUTPUT_PASSTHROUGH
@retval SOC_AUDIO_OUTPUT_ENCODED_DOLBY_DIGITAL
@retval SOC_AUDIO_OUTPUT_ENCODED_DTS
*/
enum soc_result soc_audio_output_set_mode(uint32_t processor_h,
					  void *output_h,
					  enum soc_audio_output_mode mode);

/**
This is an OPTIONAL call to specify a specific downmix to be transmitted via
an output. The caller must ensure that the output interface that is configured
to receive a specific downmix can support the number of channels in
the downmix by calling \ref soc_audio_output_set_channel_config to setup the
output with the appropriate number of channels. This call is especially useful
if the caller wants to recieve LtRt or LoRo  on a stereo output. The value set
here will also determine which coefficients are extracted from the decoders
when the decoder parameter, for example, \ref SOC_AUDIO_DTS_DOWNMIXMODE is set
to \ref SOC_AUDIO_DTS_DOWNMIX_MODE_EXTERNAL. The audio driver will make every
attempt possible to output the correct downmix mode, however if the caller
sets up the decoder params incorrectly and/or the output cannot support
the downmix mode set, the default downmixer will be used for that output.

@param[in] processor_h :  Handle to the Audio Processor instance.
@param[in] output_h :  Handle to the audio output instance.
@param[in] dmix_mode : Type of downmix configuration to be received.

@retval SOC_SUCCESS : The downmix mode was set to the output instance.
@retval SOC_ERROR_INVALID_PARAMETER : The parameters passed in was invalid.
*/
enum soc_result soc_audio_output_set_downmix_mode(uint32_t processor_h,
						  void *output_h,
						  enum soc_audio_downmix_mode
						  dmix_mode);

extern struct soc_audio_init_data soc_audio_g_init_data;

enum soc_result soc_audio_pm_recover_pipe(uint32_t proc_h);

#endif /* _SOC_AUDIO_API_H */

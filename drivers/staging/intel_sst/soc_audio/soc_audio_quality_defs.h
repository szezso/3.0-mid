
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

#ifndef _SOC_AUDIO_QUALITY_DEFS_H_
#define _SOC_AUDIO_QUALITY_DEFS_H_

#ifdef ACEDEVENV
#define SOC_AUDIO_MAX_INPUT_CHANNELS 8
#define SOC_AUDIO_MAX_OUTPUT_CHANNELS 8
#define SOC_AUDIO_QUALITY_MAX_FILTERS 40
#define SOC_AUDIO_QUALITY_MAX_CONTROLS 16
#else
#include "soc_audio_bu_config.h"
#endif

#define SOC_AUDIO_ACE_INVALID_HANDLE -1
#define SOC_AUDIO_ACE_INVALID_STAGE -1
#define SOC_AUDIO_ACE_INVALID_CHANNEL -1

enum SOC_AUDIO_ACETYP {
	SOC_AUDIO_ACETYP_NONE = 0,
	SOC_AUDIO_ACETYP_HPF2,
	SOC_AUDIO_ACETYP_LPF2,
	SOC_AUDIO_ACETYP_HPF1,
	SOC_AUDIO_ACETYP_LPF1,
	SOC_AUDIO_ACETYP_SHLF_L2,
	SOC_AUDIO_ACETYP_SHLF_H2,
	SOC_AUDIO_ACETYP_SHLF_L1,
	SOC_AUDIO_ACETYP_SHLF_H1,
	SOC_AUDIO_ACETYP_PEQ2,
	SOC_AUDIO_ACETYP_AVLCTL,
	SOC_AUDIO_ACETYP_PCVOLCTL,
	SOC_AUDIO_ACETYP_LOUDBASSCTL,
	SOC_AUDIO_ACETYP_LOUDTREBCTL
};

#define SOC_AUDIO_NUMACE_CHANNELS SOC_AUDIO_MAX_OUTPUT_CHANNELS
#define SOC_AUDIO_NUMACE_FLTDSCR SOC_AUDIO_QUALITY_MAX_FILTERS
#define SOC_AUDIO_NUMACE_CTLDSCR SOC_AUDIO_QUALITY_MAX_CONTROLS
#define SOC_AUDIO_NUMACE_STAGES_PER_CHAN 22
#define SOC_AUDIO_NUMACE_STATE_DEPTH 6
#define SOC_AUDIO_NUMACE_EVENTS 8
#define SOC_AUDIO_ACE_INTERNAL_DATASZ 256

/* for errors, or SUCCESS*/
#define SOC_AUDIO_ACE_SUCCESS 0
#define SOC_AUDIO_ACE_ERROR   1

struct soc_audio_acectlprm {
	int32_t type;
	int32_t Parms[12];
};

struct soc_audio_acefltprm {
	/* the order is important, assembly code*/
	/* uses this order*/
	int32_t b[3];
	int32_t a[2];
	int32_t tmp;
};

struct soc_audio_aceavldscr {
	int32_t GainRatioX;
	int32_t K2;
	int32_t T2;
	int32_t T2Gain;
	int32_t T1;
	int32_t T1Gain;
	int32_t K0;
	int32_t Offset;
	int32_t Attack;
	int32_t Release;
};

struct soc_audio_aceavlprm {
	int32_t WindowSz;
	int32_t WindowCnt;
	int32_t EnergyWindowSz;
	int32_t EnergyWindowCnt;
	int32_t AvgEstFactor;
	int32_t CurGainRatio;

	/* these are order sensitive, do not move*/
	int32_t T3;
	int32_t Set3;
	int32_t K2;
	int32_t T2;
	int32_t Set2;
	int32_t K1;
	int32_t T1;
	int32_t Set1;
	int32_t K0;
	int32_t Offset;
	/* end sensitive order*/

	int32_t Attack;
	int32_t Release;
	uint32_t AccumL[SOC_AUDIO_NUMACE_CHANNELS];
	uint32_t AccumH[SOC_AUDIO_NUMACE_CHANNELS];
};

struct soc_audio_acepimix4dscr {
	/* which channels, channels start with zero*/
	int32_t MixSrc[4];
	int32_t dBx10[4];
	int32_t PhInv[4];
	/* the units of the ramp are naperian time consstant in
	0.1 milliseconds*/
	int32_t RampTC[4];
};

struct soc_audio_acepimix4prm {
	int32_t IsMix;
	int32_t MixSrc[4];
	int32_t TargetGain[4];
	int32_t CurCoeff[4];
	int32_t RampCoeff[4];
};

struct soc_audio_acereverbdscr {
	int32_t cnt;
	int32_t Delay[4];
	int32_t Mix[4];
};

struct soc_audio_acereverbprm {
	int32_t *pLastBuffElem;
	int32_t *pFirstBuffElem;
	int32_t Init;
	int32_t *pLastEntry;
	int32_t MixCnt;
	int32_t Mix1LastAddr;
	int32_t Mix1Coeff;
	int32_t Mix2LastAddr;
	int32_t Mix2Coeff;
	int32_t Mix3LastAddr;
	int32_t Mix3Coeff;
	int32_t Mix4LastAddr;
	int32_t Mix4Coeff;
};

struct soc_audio_acefltdscr {
	enum SOC_AUDIO_ACETYP type;	/* type, use ACETYP_**/
	int32_t Fc;		/* cutoff frequency*/
	int32_t Q;		/* Q factor of filter*/
	int32_t Gain;		/* Gain of filter*/
};

struct soc_audio_acectldscr {
	enum SOC_AUDIO_ACETYP type;	/* type of control, use ACETYP_**/
	int32_t NumParms;
	/* number of valid parms, fixed per control type*/
	int32_t Parms[8];	/* contents vary per control type*/
};

struct soc_audio_acechan {
	int32_t *pIn;
	int32_t *pOut;
	int32_t nSamples;
	int32_t Bypass[SOC_AUDIO_NUMACE_STAGES_PER_CHAN];
	void (*pFunc[SOC_AUDIO_NUMACE_STAGES_PER_CHAN]) (int32_t *pIn,
							 int32_t *pOut,
							 int32_t nSmpls,
							 void *Parms,
							 void *State);
	void *pParms[SOC_AUDIO_NUMACE_STAGES_PER_CHAN];
	int32_t State[SOC_AUDIO_NUMACE_STAGES_PER_CHAN]
	    [SOC_AUDIO_NUMACE_STATE_DEPTH];
};

struct soc_audio_aceuser {
	/* these are descriptors, for each channel there are
	SOC_AUDIO_NUMACE_STAGES_PER_CHAN*/
	/* of stages, the stages contain handles to descriptors, as follows:*/
	/* 0        means no dscr, processing ends with this stage*/
	/* 1-100    for filter descriptors 0-99*/
	/* 101-200  for control descriptors 0-99*/
	int32_t IAChannels[SOC_AUDIO_NUMACE_CHANNELS]
	    [SOC_AUDIO_NUMACE_STAGES_PER_CHAN];

	/* Bypass*/
	uint8_t IABypass[SOC_AUDIO_NUMACE_CHANNELS]
	    [SOC_AUDIO_NUMACE_STAGES_PER_CHAN];

	/* these are the filter descriptors, the handles range from*/
	/* (1 to SOC_AUDIO_NUMACE_FLTDSCR + 1), the index to the
	array is one less than*/
	/* the handle. This is where the type of filter desired
	and its parameters*/
	/* are stored. Several stages for several channels may all
	reference the*/
	/* same Filter*/
	struct soc_audio_acefltdscr IAFilters[SOC_AUDIO_NUMACE_FLTDSCR];

	/* these are the control descriptors, the handles range from*/
	/* 101 to (SOC_AUDIO_NUMACE_CTLDSCR + 100), the index to the
	array is 101 less than*/
	/* the handle. This is where the type of control is stored,
	and its parameters.*/
	/* Several stages for several channels may all reference the
	same Control.*/
	struct soc_audio_acectldscr IAControls[SOC_AUDIO_NUMACE_CTLDSCR];

	/* this is for channel duplication / splits*/
	int32_t DupBypass;
	int32_t DupBeforeStage;
	int32_t DupFrom[SOC_AUDIO_NUMACE_CHANNELS];
	int32_t PIMix4Bypass;
	struct soc_audio_acepimix4dscr PIMix4[SOC_AUDIO_NUMACE_CHANNELS];
	int32_t IAAVLBypass;
	struct soc_audio_aceavldscr IACtlAVL;

	/* master volume control*/
	int32_t MasterVolBypass;
	int32_t MasterVol;
	int32_t MasterVolRamp;

	/* master mute control*/
	int32_t MasterMuteBypass;
	int32_t MasterMuteOn;
	int32_t MasterMuteRamp;

	int32_t IAClampBypass;
	int32_t IAClamp10xdB[SOC_AUDIO_NUMACE_CHANNELS];

	/* for Reverb*/
	int32_t IAReverbBypass;
	int32_t *DelayLine[SOC_AUDIO_NUMACE_CHANNELS];
	int32_t DelayLineSz;
	struct soc_audio_acereverbdscr IAReverb[SOC_AUDIO_NUMACE_CHANNELS];
};

struct soc_audio_acesystem {
	/* IA side*/
	struct soc_audio_aceuser IAParams;

	/* IA and DSP side*/
	int32_t CacheBuffZone1[16];	/* cache line buffer, to separate
	the two regions*/

	/* used for callbacks, the queue is 8 long, and there can be
	up to 5 values*/
	/* per event, the first value is the event type*/
	int32_t NumCallBacks;	/* tells how many callback events are
	valid, the DSP*/
	/* side sets this, the IA side clears this after pushing the*/
	/* messages up to the application*/
	int32_t CallBackEvent[SOC_AUDIO_NUMACE_EVENTS][5];

	int32_t CacheBuffZone2[16];	/* cache line buffer, to separate
	the two regions*/

	/* DSP side*/

	/* we keep a copy of the sample rate for which all of the parameters*/
	/* where last computed. If this value changes, all of the
	parameters on*/
	/* the DSP side will need to be recomputed*/
	int32_t CurSampleRate;

	/* dsp area change flags dkl 8-27-2010*/
	uint8_t dsp_bypass_change;
	uint8_t dsp_flt_param_change;
	uint8_t dsp_ctl_param_change;
	uint8_t dsp_config_change;
	uint8_t dsp_avl_config_change;
	uint8_t dsp_ace_init;
	uint8_t ctag0;
	uint8_t ctag1;

	int32_t AVLBypass;
	int32_t PIMix4WindowSz;
	int32_t PIMix4WindowCnt;
	int32_t PIMix4Bypass;
	struct soc_audio_acepimix4prm PIMix4Parms[SOC_AUDIO_NUMACE_CHANNELS];

	/* one of these for each channel,*/
	/* they hold history info, pointers to buffers for each*/
	/* channel, and the list of stages parameters, and stage functions*/
	struct soc_audio_acechan Channels[SOC_AUDIO_NUMACE_CHANNELS];

	/* filter parameter references - for storing filter parms*/
	/* a single one of these can be referenced by multiple stages*/
	struct soc_audio_acefltprm FilterParms[SOC_AUDIO_NUMACE_FLTDSCR];

	struct soc_audio_acectlprm CtlParms[SOC_AUDIO_NUMACE_CTLDSCR];

	/* for Master volume*/
	int32_t MasterVolBypass;
	struct soc_audio_acectlprm MasterVolume;
	int32_t MasterVolState[SOC_AUDIO_NUMACE_CHANNELS]
	    [SOC_AUDIO_NUMACE_STATE_DEPTH];

	/* for Master Mute*/
	int32_t MasterMuteBypass;
	struct soc_audio_acectlprm MasterMute;
	int32_t MasterMuteState[SOC_AUDIO_NUMACE_CHANNELS]
	    [SOC_AUDIO_NUMACE_STATE_DEPTH];

	/* for the one allowable AVL module*/
	struct soc_audio_aceavlprm AVLParms;

	int32_t ClampBypass;
	int32_t ClampHi[SOC_AUDIO_NUMACE_CHANNELS];
	int32_t ClampLo[SOC_AUDIO_NUMACE_CHANNELS];

	/* Reverb*/
	int32_t ReverbBypass;
	struct soc_audio_acereverbprm ReverbParms[SOC_AUDIO_NUMACE_CHANNELS];

	/* this is for channel duplication / splits*/
	int32_t DupBypass;
	int32_t DupBeforeStage;
	int32_t DupFrom[SOC_AUDIO_NUMACE_CHANNELS];

	int32_t BuffLn;
	int32_t Buff[SOC_AUDIO_NUMACE_CHANNELS][SOC_AUDIO_ACE_INTERNAL_DATASZ];

	/* internal use for the DSP side only, contains precomputed logs base*/
	/* 2 of certain value sets*/
	int32_t LogTables[100];

	int32_t XCoeffcients[16];

	/* to make even number of cache lines*/
	int32_t padspace[6];
};

/** Maximum audio quality filters available. */
#define SOC_AUDIO_QUALITY_MAX_FILTERS 40
/** Maximum audio quality controls available. */
#define SOC_AUDIO_QUALITY_MAX_CONTROLS 16
/** Maximum audio quality stages available. */
#define SOC_AUDIO_QUALITY_MAX_STAGES 22

/** Min for AVL attack time parameter, in milliseconds */
#define SOC_AUDIO_QUALITY_ATTACK_TIME_MIN 0
/** Max for AVL attack time parameter, in milliseconds */
#define SOC_AUDIO_QUALITY_ATTACK_TIME_MAX 255

/** Min for AVL release time parameter, in milliseconds */
#define SOC_AUDIO_QUALITY_REL_TIME_MIN 0
/** Max for AVL release time parameter, in milliseconds */
#define SOC_AUDIO_QUALITY_REL_TIME_MAX 7650

/** Min value for Fc filter parameter */
#define SOC_AUDIO_QUALITY_FILTER_FC_MIN 0
/** Max value  for Fc filter parameter */
#define SOC_AUDIO_QUALITY_FILTER_FC_MAX 96000

/** Use this enum to specify a type of filter.*/
enum soc_audio_quality_filter_type {
	SOC_AUDIO_FILTER_INVALID,
	SOC_AUDIO_FILTER_HPF2,
	SOC_AUDIO_FILTER_LPF2,
	SOC_AUDIO_FILTER_HPF1,
	SOC_AUDIO_FILTER_LPF1,
	SOC_AUDIO_FILTER_SHLF_L2,
	SOC_AUDIO_FILTER_SHLF_H2,
	SOC_AUDIO_FILTER_SHLF_L1,
	SOC_AUDIO_FILTER_SHLF_H1,
	SOC_AUDIO_FILTER_PEQ2,
	SOC_AUDIO_FILTER_COUNT
};

/** Use this struct to specify filter parameters. */
struct soc_audio_quality_filter_params {
	int32_t fc;
	int32_t q;
	int32_t gain;
};

struct soc_audio_quality_dup_params {
	int32_t DupFromChan;
	int32_t DupBeforeStage;
};

/** Use this enum to specify a type of control. */
enum soc_audio_quality_control_type {
	SOC_AUDIO_CONTROL_INVALID,
	SOC_AUDIO_CONTROL_PC_VOLUME,
	SOC_AUDIO_CONTROL_AVL,
	SOC_AUDIO_CONTROL_VOLUME,
	SOC_AUDIO_CONTROL_MUTE,
	SOC_AUDIO_CONTROL_DUP,
	SOC_AUDIO_CONTROL_MINIMIX,
	SOC_AUDIO_CONTROL_CLAMP,
	SOC_AUDIO_CONTROL_BASS_LIMITER,
	SOC_AUDIO_CONTROL_TREBLE_LIMITER,
	SOC_AUDIO_CONTROL_COUNT
};

/** Pameters required by the AVL control. */
struct soc_audio_quality_avl_params {
	int32_t t1;    /** < Changing point 1 (dB)	(0.5dB/step)	*/
	int32_t t2;    /** < Changing point 2 (dB)	(0.5dB/step)*/
	int32_t t1_gain;/** < Gain at changing point 1 (dB)	(0.5dB/step)*/
	int32_t t2_gain;/** < Gain at changing point 2 (dB)	(0.5dB/step)*/
	int32_t k0;    /** < Gradient less than T1 area	(0.1/step)	*/
	int32_t k2;    /** < Gradient more than T2 area	(0.1/step)	*/
	int32_t attack_time;
	/** < Attack Time (mSec)	(1mSec/step)	0~255mSec*/
	int32_t release_time;
	/** < Release Time (mSec)	(30mSec/step)	0~7650mSec*/
	int32_t offset_gain;/** < Offset gain (dB)	(0.1dB/step)	*/
};

/** Pameters required by the volume control.*/
struct soc_audio_quality_pc_volume_params {
	int32_t gain;
		 /** < Gain in 0.1dB steps*/
	int32_t ramp;
		/** < Time in milliseconds for one naperian time constant. */
};

/** Pameters required by the volume control.*/
struct soc_audio_quality_volume_params {
	int32_t gain; /** < Gain in 0.1dB steps*/
	int32_t ramp;
	/** < Time in milliseconds for one naperian time constant. */
};

/** Pameters required by the mute control.*/
struct soc_audio_quality_mute_params {
	int32_t mute;
		   /** < If non-zero, mute is on, otherwise mute is off*/
	int32_t ramp;
		/** < Time in milliseconds for one naperian time constant.*/
};

struct soc_audio_quality_minimix_params {
	int32_t mix_src[4];
	int32_t gain[4];
	int32_t ramp[4];
	uint8_t pi[4];
};

/** Pameters required by bass/treble limiter controls.*/
struct soc_audio_quality_loudness_params {
	int32_t volume;/** < Volume Control Set point (dB) (0.5dB/step) */
	int32_t set_p;/** < Changing point 1 (dB)	(0.5dB/step)*/
	int32_t set_l;/** < Changing point 2 (dB)	(0.5dB/step)*/
	int32_t set_h;/** < Changing point 3 (dB)	(0.5dB/step)*/
	int32_t k0;   /** < Gradient less than set_p area (dB)	(0.01dB/step)*/
	int32_t k1;
	/**Gradient more than set_p & less than set_l area (dB)(0.01dB/step)*/
	int32_t k2;   /** < Gradient more than set_h area (dB)	(0.01dB/step)*/
	int32_t fc;	/** < Cut off frequency (150-3200, 1Hz/step) */
};

struct soc_audio_quality_clamp_params {
	int32_t level;
};

/** Used to specify parameters depending on which
 control/filter it references.*/
union soc_audio_quality_control_params {
	struct soc_audio_quality_avl_params avl;
	struct soc_audio_quality_dup_params dup;
	struct soc_audio_quality_loudness_params bass_loud;
	struct soc_audio_quality_loudness_params treble_loud;
	struct soc_audio_quality_minimix_params minimix;
	struct soc_audio_quality_clamp_params clamp;
	struct soc_audio_quality_pc_volume_params pc_volume;
};

#endif	/*_SOC_AUDIO_QUALITY_DEFS_H_ */

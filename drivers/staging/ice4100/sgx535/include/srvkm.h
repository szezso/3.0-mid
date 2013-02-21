/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#ifndef SRVKM_H
#define SRVKM_H


#if defined(__cplusplus)
extern "C" {
#endif


	#ifdef PVR_DISABLE_LOGGING
	#define PVR_LOG(X)
	#else
	#define PVR_LOG(X)			PVRSRVReleasePrintf X
	#endif

	 void  PVRSRVReleasePrintf(const char *pszFormat,
										...);

	 PVRSRV_ERROR  PVRSRVProcessConnect(u32	ui32PID);
	 void  PVRSRVProcessDisconnect(u32	ui32PID);

	void  PVRSRVSetDCState(u32 ui32State);

	PVRSRV_ERROR  PVRSRVSaveRestoreLiveSegments(void *hArena, unsigned char *pbyBuffer, u32 *puiBufSize, int bSave);

#if defined (__cplusplus)
}
#endif

#define LOOP_UNTIL_TIMEOUT(TIMEOUT) \
{\
	u32 uiOffset, uiStart, uiCurrent, uiNotLastLoop;								\
	for(uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1, uiNotLastLoop = 1;\
		((uiCurrent - uiStart + uiOffset) < TIMEOUT) || uiNotLastLoop--;				\
		uiCurrent = OSClockus(),														\
		uiOffset = uiCurrent < uiStart ? u32_MAX - uiStart : uiOffset,			\
		uiStart = uiCurrent < uiStart ? 0 : uiStart)

#define END_LOOP_UNTIL_TIMEOUT() \
}


#endif

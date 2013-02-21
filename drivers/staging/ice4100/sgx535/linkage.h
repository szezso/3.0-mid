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

#ifndef __LINKAGE_H__
#define __LINKAGE_H__

#if !defined(SUPPORT_DRI_DRM)
s32 PVRSRV_BridgeDispatchKM(struct file *file, u32 cmd, u32 arg);
#endif

void PVRDPFInit(void);
PVRSRV_ERROR PVROSFuncInit(void);
void PVROSFuncDeInit(void);

#ifdef DEBUG
int PVRDebugProcSetLevel(struct file *file, const char *buffer, u32 count, void *data);
void PVRDebugSetLevel(u32 uDebugLevel);

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowDebugLevel(struct seq_file *sfile,void* el);
#else
int PVRDebugProcGetLevel(char *page, char **start, off_t off, int count, int *eof, void *data);
#endif

#ifdef PVR_MANUAL_POWER_CONTROL
int PVRProcSetPowerLevel(struct file *file, const char *buffer, u32 count, void *data);

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowPowerLevel(struct seq_file *sfile,void* el);
#else
int PVRProcGetPowerLevel(char *page, char **start, off_t off, int count, int *eof, void *data);
#endif


#endif
#endif

#endif

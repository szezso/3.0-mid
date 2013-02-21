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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/hardirq.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/mutex.h>
#include <stdarg.h>
#include "img_types.h"
#include "servicesext.h"
#include "pvr_debug.h"
#include "proc.h"
#include "linkage.h"

#if defined(PVRSRV_NEED_PVR_DPF)

#define PVR_MAX_FILEPATH_LEN 256

static u32 gPVRDebugLevel = DBGPRIV_WARNING;

#endif

#define	PVR_MAX_MSG_LEN PVR_MAX_DEBUG_MESSAGE_LEN

static char gszBufferNonIRQ[PVR_MAX_MSG_LEN + 1];

static char gszBufferIRQ[PVR_MAX_MSG_LEN + 1];

static struct mutex gsDebugMutexNonIRQ;

static DEFINE_SPINLOCK(gsDebugLockIRQ);

#define	USE_SPIN_LOCK (in_interrupt() || !preemptible())

static inline void GetBufferLock(unsigned long *pulLockFlags)
{
	if (USE_SPIN_LOCK) {
		spin_lock_irqsave(&gsDebugLockIRQ, *pulLockFlags);
	} else {
		mutex_lock(&gsDebugMutexNonIRQ);
	}
}

static inline void ReleaseBufferLock(unsigned long ulLockFlags)
{
	if (USE_SPIN_LOCK) {
		spin_unlock_irqrestore(&gsDebugLockIRQ, ulLockFlags);
	} else {
		mutex_unlock(&gsDebugMutexNonIRQ);
	}
}

static inline void SelectBuffer(char **ppszBuf, u32 * pui32BufSiz)
{
	if (USE_SPIN_LOCK) {
		*ppszBuf = gszBufferIRQ;
		*pui32BufSiz = sizeof(gszBufferIRQ);
	} else {
		*ppszBuf = gszBufferNonIRQ;
		*pui32BufSiz = sizeof(gszBufferNonIRQ);
	}
}

static int VBAppend(char *pszBuf, u32 ui32BufSiz, const char *pszFormat,
		    va_list VArgs)
{
	u32 ui32Used;
	u32 ui32Space;
	s32 i32Len;

	ui32Used = strlen(pszBuf);
	BUG_ON(ui32Used >= ui32BufSiz);
	ui32Space = ui32BufSiz - ui32Used;

	i32Len = vsnprintf(&pszBuf[ui32Used], ui32Space, pszFormat, VArgs);
	pszBuf[ui32BufSiz - 1] = 0;

	return (i32Len < 0 || i32Len >= ui32Space);
}

void PVRDPFInit(void)
{
	mutex_init(&gsDebugMutexNonIRQ);
}

void PVRSRVReleasePrintf(const char *pszFormat, ...)
{
	va_list vaArgs;
	unsigned long ulLockFlags = 0;
	char *pszBuf;
	u32 ui32BufSiz;

	SelectBuffer(&pszBuf, &ui32BufSiz);

	va_start(vaArgs, pszFormat);

	GetBufferLock(&ulLockFlags);
	strncpy(pszBuf, "PVR_K: ", (ui32BufSiz - 1));

	if (VBAppend(pszBuf, ui32BufSiz, pszFormat, vaArgs)) {
		printk(KERN_INFO "PVR_K:(Message Truncated): %s\n", pszBuf);
	} else {
		printk(KERN_INFO "%s\n", pszBuf);
	}

	ReleaseBufferLock(ulLockFlags);
	va_end(vaArgs);

}

#if defined(PVRSRV_NEED_PVR_ASSERT)

void PVRSRVDebugAssertFail(const char *pszFile, u32 uLine)
{
	PVRSRVDebugPrintf(DBGPRIV_FATAL, pszFile, uLine,
			  "Debug assertion failed!");
	BUG();
}

#endif

#if defined(PVRSRV_NEED_PVR_TRACE)

void PVRSRVTrace(const char *pszFormat, ...)
{
	va_list VArgs;
	unsigned long ulLockFlags = 0;
	char *pszBuf;
	u32 ui32BufSiz;

	SelectBuffer(&pszBuf, &ui32BufSiz);

	va_start(VArgs, pszFormat);

	GetBufferLock(&ulLockFlags);

	strncpy(pszBuf, "PVR: ", (ui32BufSiz - 1));

	if (VBAppend(pszBuf, ui32BufSiz, pszFormat, VArgs)) {
		printk(KERN_INFO "PVR_K:(Message Truncated): %s\n", pszBuf);
	} else {
		printk(KERN_INFO "%s\n", pszBuf);
	}

	ReleaseBufferLock(ulLockFlags);

	va_end(VArgs);
}

#endif

#if defined(PVRSRV_NEED_PVR_DPF)

static int BAppend(char *pszBuf, u32 ui32BufSiz, const char *pszFormat, ...)
{
	va_list VArgs;
	int bTrunc;

	va_start(VArgs, pszFormat);

	bTrunc = VBAppend(pszBuf, ui32BufSiz, pszFormat, VArgs);

	va_end(VArgs);

	return bTrunc;
}

void PVRSRVDebugPrintf(u32 ui32DebugLevel,
		       const char *pszFullFileName,
		       u32 ui32Line, const char *pszFormat, ...
    )
{
	int bTrace, bDebug;
	const char *pszFileName = pszFullFileName;
	char *pszLeafName;

	bTrace = gPVRDebugLevel & ui32DebugLevel & DBGPRIV_CALLTRACE;
	bDebug = ((gPVRDebugLevel & DBGPRIV_ALLLEVELS) >= ui32DebugLevel);

	if (bTrace || bDebug) {
		va_list vaArgs;
		unsigned long ulLockFlags = 0;
		char *pszBuf;
		u32 ui32BufSiz;

		SelectBuffer(&pszBuf, &ui32BufSiz);

		va_start(vaArgs, pszFormat);

		GetBufferLock(&ulLockFlags);

		if (bDebug) {
			switch (ui32DebugLevel) {
			case DBGPRIV_FATAL:
				{
					strncpy(pszBuf, "PVR_K:(Fatal): ",
						(ui32BufSiz - 1));
					break;
				}
			case DBGPRIV_ERROR:
				{
					strncpy(pszBuf, "PVR_K:(Error): ",
						(ui32BufSiz - 1));
					break;
				}
			case DBGPRIV_WARNING:
				{
					strncpy(pszBuf, "PVR_K:(Warning): ",
						(ui32BufSiz - 1));
					break;
				}
			case DBGPRIV_MESSAGE:
				{
					strncpy(pszBuf, "PVR_K:(Message): ",
						(ui32BufSiz - 1));
					break;
				}
			case DBGPRIV_VERBOSE:
				{
					strncpy(pszBuf, "PVR_K:(Verbose): ",
						(ui32BufSiz - 1));
					break;
				}
			default:
				{
					strncpy(pszBuf,
						"PVR_K:(Unknown message level)",
						(ui32BufSiz - 1));
					break;
				}
			}
		} else {
			strncpy(pszBuf, "PVR_K: ", (ui32BufSiz - 1));
		}

		if (VBAppend(pszBuf, ui32BufSiz, pszFormat, vaArgs)) {
			printk(KERN_INFO "PVR_K:(Message Truncated): %s\n",
			       pszBuf);
		} else {

			if (!bTrace) {
#ifdef DEBUG_LOG_PATH_TRUNCATE

				static char
				    szFileNameRewrite[PVR_MAX_FILEPATH_LEN];

				char *pszTruncIter;
				char *pszTruncBackInter;

				pszFileName =
				    pszFullFileName +
				    strlen(DEBUG_LOG_PATH_TRUNCATE) + 1;

				strncpy(szFileNameRewrite, pszFileName,
					PVR_MAX_FILEPATH_LEN);

				if (strlen(szFileNameRewrite) ==
				    PVR_MAX_FILEPATH_LEN - 1) {
					char szTruncateMassage[] =
					    "FILENAME TRUNCATED";
					strcpy(szFileNameRewrite +
					       (PVR_MAX_FILEPATH_LEN - 1 -
						strlen(szTruncateMassage)),
					       szTruncateMassage);
				}

				pszTruncIter = szFileNameRewrite;
				while (*pszTruncIter++ != 0) {
					char *pszNextStartPoint;

					if (!
					    ((*pszTruncIter == '/'
					      && (pszTruncIter - 4 >=
						  szFileNameRewrite))
					     && (*(pszTruncIter - 1) == '.')
					     && (*(pszTruncIter - 2) == '.')
					     && (*(pszTruncIter - 3) == '/'))
					    )
						continue;

					pszTruncBackInter = pszTruncIter - 3;
					while (*(--pszTruncBackInter) != '/') {
						if (pszTruncBackInter <=
						    szFileNameRewrite)
							break;
					}
					pszNextStartPoint = pszTruncBackInter;

					while (*pszTruncIter != 0) {
						*pszTruncBackInter++ =
						    *pszTruncIter++;
					}
					*pszTruncBackInter = 0;

					pszTruncIter = pszNextStartPoint;
				}

				pszFileName = szFileNameRewrite;

				if (*pszFileName == '/')
					pszFileName++;
#endif

#if !defined(__sh__)
				pszLeafName =
				    (char *)strrchr(pszFileName, '\\');

				if (pszLeafName) {
					pszFileName = pszLeafName;
				}
#endif

				if (BAppend
				    (pszBuf, ui32BufSiz, " [%lu, %s]", ui32Line,
				     pszFileName)) {
					printk(KERN_INFO
					       "PVR_K:(Message Truncated): %s\n",
					       pszBuf);
				} else {
					printk(KERN_INFO "%s\n", pszBuf);
				}
			} else {
				printk(KERN_INFO "%s\n", pszBuf);
			}
		}

		ReleaseBufferLock(ulLockFlags);

		va_end(vaArgs);
	}
}

#endif

#if defined(DEBUG)

void PVRDebugSetLevel(u32 uDebugLevel)
{
	printk(KERN_INFO "PVR: Setting Debug Level = 0x%x\n",
	       (u32) uDebugLevel);

	gPVRDebugLevel = uDebugLevel;
}

int PVRDebugProcSetLevel(struct file *file, const char *buffer, u32 count,
			 void *data)
{
#define	_PROC_SET_BUFFER_SZ		2
	char data_buffer[_PROC_SET_BUFFER_SZ];

	if (count != _PROC_SET_BUFFER_SZ) {
		return -EINVAL;
	} else {
		if (copy_from_user(data_buffer, buffer, count))
			return -EINVAL;
		if (data_buffer[count - 1] != '\n')
			return -EINVAL;
		PVRDebugSetLevel(data_buffer[0] - '0');
	}
	return (count);
}

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowDebugLevel(struct seq_file *sfile, void *el)
{
	seq_printf(sfile, "%lu\n", gPVRDebugLevel);
}

#else
int PVRDebugProcGetLevel(char *page, char **start, off_t off, int count,
			 int *eof, void *data)
{
	if (off == 0) {
		*start = (char *)1;
		return printAppend(page, count, 0, "%lu\n", gPVRDebugLevel);
	}
	*eof = 1;
	return 0;
}
#endif

#endif

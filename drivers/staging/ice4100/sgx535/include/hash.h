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

#ifndef _HASH_H_
#define _HASH_H_

#include "img_types.h"
#include "osfunc.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef u32 HASH_FUNC(u32 uKeySize, void *pKey, u32 uHashTabLen);
typedef int HASH_KEY_COMP(u32 uKeySize, void *pKey1, void *pKey2);

typedef struct _HASH_TABLE_ HASH_TABLE;

u32 HASH_Func_Default (u32 uKeySize, void *pKey, u32 uHashTabLen);

int HASH_Key_Comp_Default (u32 uKeySize, void *pKey1, void *pKey2);

HASH_TABLE * HASH_Create_Extended (u32 uInitialLen, u32 uKeySize, HASH_FUNC *pfnHashFunc, HASH_KEY_COMP *pfnKeyComp);

HASH_TABLE * HASH_Create (u32 uInitialLen);

void HASH_Delete (HASH_TABLE *pHash);

int HASH_Insert_Extended (HASH_TABLE *pHash, void *pKey, u32 v);

int HASH_Insert (HASH_TABLE *pHash, u32 k, u32 v);

u32 HASH_Remove_Extended(HASH_TABLE *pHash, void *pKey);

u32 HASH_Remove (HASH_TABLE *pHash, u32 k);

u32 HASH_Retrieve_Extended (HASH_TABLE *pHash, void *pKey);

u32 HASH_Retrieve (HASH_TABLE *pHash, u32 k);

#ifdef HASH_TRACE
void HASH_Dump (HASH_TABLE *pHash);
#endif

#if defined (__cplusplus)
}
#endif

#endif


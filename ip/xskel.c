/* libhpojip -- HP OfficeJet image-processing library. */

/* Copyright (C) 1995-2002 HP Company
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * In addition, as a special exception, HP Company
 * gives permission to link the code of this program with any
 * version of the OpenSSL library which is distributed under a
 * license identical to that listed in the included LICENSE.OpenSSL
 * file, and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

/* Original author: Mark Overton and others.
 *
 * Ported to Linux by David Paschal.
 */

/******************************************************************************\
 *
 * xskel.c - A skeleton xform driver on which new xforms can be based
 *
 * You must change "skel" and "SKEL" to your actual xform name.
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    skelTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_SKEL_SPEC_1] = fully describe these options here, if any
 *    aXformInfo[IP_SKEL_SPEC_2] =
 *
 * Capabilities and Limitations:
 *
 *    What does this xform do?  What limitations of data types, ranges, etc?
 *
 * Default Input Traits, and Output Traits:
 *
 *    Describe what you do with the default input traits, and how the
 *    output traits are determined.
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel   * passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
\******************************************************************************/

// Use the #define below if this transform will exist in a dll outside of the
// image pipeline.  This will allow the functions to be exported.
// #define EXPORT_TRANFORM 1

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */


#if 0
    #include "stdio.h"
    #include <tchar.h>
    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x4ba1dace

#ifdef EXPORT_TRANSFORM
#define FUNC_STATUS __declspec (dllexport)
#else
#define FUNC_STATUS static
#endif

typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the input and output image */
    DWORD    dwBytesPerRow;   /* # of bytes in each row */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
} SKEL_INST, *PSKEL_INST;



/*****************************************************************************\
 *
 * skel_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PSKEL_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(SKEL_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(SKEL_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skel_setDefaultInputTraits - Specifies default input image traits
 *
 *****************************************************************************
 *
 * The header of the file-type handled by the transform probably does
 * not include *all* the image traits we'd like to know.  Those not
 * specified in the file-header are filled in from info provided by
 * this routine.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow>0 && pTraits->iBitsPerPixel>0);
    g->traits = *pTraits;   /* a structure copy */
    g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skel_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Check your options in aXformInfo here, and save them.
     * Use the INSURE macro like you'd use assert.  INSURE jumps to
     * fatal_error below if it fails.
     */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skel_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * skel_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;
    g->dwInNextPos   = 0;

    *pInTraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * skel_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD skel_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skel_convert - Converts one row
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_convert (
    IP_XFORM_HANDLE hXform,
    DWORD           dwInputAvail,     /* in:  # avail bytes in in-buf */
    PBYTE           pbInputBuf,       /* in:  ptr to in-buffer */
    PDWORD          pdwInputUsed,     /* out: # bytes used from in-buf */
    PDWORD          pdwInputNextPos,  /* out: file-pos to read from next */
    DWORD           dwOutputAvail,    /* in:  # avail bytes in out-buf */
    PBYTE           pbOutputBuf,      /* in:  ptr to out-buffer */
    PDWORD          pdwOutputUsed,    /* out: # bytes written in out-buf */
    PDWORD          pdwOutputThisPos) /* out: file-pos to write the data */
{
    PSKEL_INST g;
    int       nBytes;
    PBYTE     pIn, pOut;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("skel_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = g->dwBytesPerRow;
    INSURE (dwInputAvail  >= (DWORD)nBytes );
    INSURE (dwOutputAvail >= (DWORD)nBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;

    /* At this point, pIn is your input buffer, and pOut is your output buffer.
     * Do whatever you are going to do here.
     */
    memcpy(pOut,pIn,nBytes);

    *pdwInputUsed     = nBytes;
    g->dwInNextPos   += nBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = nBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += nBytes;

    g->dwRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skel_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * skel_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_newPage (
    IP_XFORM_HANDLE hXform)
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * skel_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD skel_closeXform (IP_XFORM_HANDLE hXform)
{
    PSKEL_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * skelTbl - Jump-table for transform driver
 *
\*****************************************************************************/
#ifdef EXPORT_TRANSFORM
__declspec (dllexport)
#endif
IP_XFORM_TBL skelTbl = {
    skel_openXform,
    skel_setDefaultInputTraits,
    skel_setXformSpec,
    skel_getHeaderBufSize,
    skel_getActualTraits,
    skel_getActualBufSizes,
    skel_convert,
    skel_newPage,
    skel_insertedData,
    skel_closeXform
};

/* End of File */


/*****************************************************************************\
 *
 * ipGetXformTable - Returns pointer to the jump table
 *
\*****************************************************************************/

#ifdef EXPORT_TRANSFORM
EXPORT(WORD) ipGetXformTable (LPIP_XFORM_TBL pXform)
{
    WORD wRet = IP_DONE;

    if (pXform)
    {
        *pXform = clrmapTbl;
    }
    else
    {
        wRet = IP_FATAL_ERROR;
    }

    return wRet;
}
#endif

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
 * xyxtract.c - Y-extract - Extracts Y-component from YCC color data
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    yXtractTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    xXformInfo[IP_Y_EXTRACT_COLOR_SPACE] tells us something about the color-space
 *       of input data:  See the enum IP_Y_EXTRACT_WHICH_SPACE:
 *       IP_Y_EXTRACT_LUM_CHROME = luminance-chrominance, we merely fetch 1st component,
 *       IP_Y_EXTRACT_RGB        = input is RGB,
 *       IP_Y_EXTRACT_BGR        = input is BGR.
 *
 * Capabilities and Limitations:
 *
 *    Inputs rows of 24-bit color data, and outputs rows of 8-bit gray
 *    consisting of the first component of the color data.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 24           8
 *    iComponentsPerPixel   * must be 3            1
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Jan  1998 Mark Overton -- wrote code
 *
\******************************************************************************/

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

#define CHECK_VALUE 0x1ce5ca7e

typedef struct {
    IP_IMAGE_TRAITS inTraits; /* traits of the input image */
    IP_Y_EXTRACT_WHICH_SPACE eInputType;  /* type of input data */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
} YEX_INST, *PYEX_INST;



/*****************************************************************************\
 *
 * yXtract_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD yXtract_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PYEX_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(YEX_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(YEX_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtract_setDefaultInputTraits - Specifies default input image traits
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

static WORD yXtract_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PYEX_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (pTraits->iBitsPerPixel == 24);
    INSURE (pTraits->iComponentsPerPixel == 3);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtract_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD yXtract_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PYEX_INST g;
    HANDLE_TO_PTR (hXform, g);
    g->eInputType = (IP_Y_EXTRACT_WHICH_SPACE)aXformInfo[IP_Y_EXTRACT_COLOR_SPACE].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtract_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD yXtract_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * yXtract_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD yXtract_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PYEX_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pInTraits  = g->inTraits;
    *pOutTraits = g->inTraits;
    pOutTraits->iBitsPerPixel = 8;
    pOutTraits->iComponentsPerPixel = 1;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * yXtract_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD yXtract_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PYEX_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = 3 * g->inTraits.iPixelsPerRow;
    *pdwMinOutBufLen =     g->inTraits.iPixelsPerRow;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtract_convert - Converts one row
 *
\*****************************************************************************/

static WORD yXtract_convert (
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
    PYEX_INST g;
    int       inBytes, outBytes;
    PBYTE     pIn, pOut, pOutAfter;
    UINT      red, grn, blu;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("yXtract_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    outBytes = g->inTraits.iPixelsPerRow;
    inBytes  = 3 * outBytes;
    INSURE (dwInputAvail  >= (DWORD)inBytes );
    INSURE (dwOutputAvail >= (DWORD)outBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pOutAfter = pOut + outBytes;

    switch (g->eInputType) {
        case IP_Y_EXTRACT_LUM_CHROME:
            while (pOut < pOutAfter) {
                *pOut++ = *pIn;
                pIn  += 3;
            }
            break;

        case IP_Y_EXTRACT_RGB:
            while (pOut < pOutAfter) {
                red = (UINT)(*pIn++);
                grn = (UINT)(*pIn++);
                blu = (UINT)(*pIn++);
                /* the formula below is:  Y = (5*R + 9*G + 2*B) / 16  */
                *pOut++ =
                  (BYTE)((((red<<2)+red) + ((grn<<3)+grn) + (blu<<1) + 8) >> 4);
            }
            break;

        case IP_Y_EXTRACT_BGR:
            while (pOut < pOutAfter) {
                blu = (UINT)(*pIn++);
                grn = (UINT)(*pIn++);
                red = (UINT)(*pIn++);
                /* the formula below is:  Y = (5*R + 9*G + 2*B) / 16  */
                *pOut++ =
                  (BYTE)((((red<<2)+red) + ((grn<<3)+grn) + (blu<<1) + 8) >> 4);
            }
            break;

        default:
            goto fatal_error;
    }

    *pdwInputUsed     = inBytes;
    g->dwInNextPos   += inBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = outBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += outBytes;

    g->dwRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtract_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD yXtract_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * yXtract_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD yXtract_newPage (
    IP_XFORM_HANDLE hXform)
{
    PYEX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * yXtract_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD yXtract_closeXform (IP_XFORM_HANDLE hXform)
{
    PYEX_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * yXtractTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL yXtractTbl = {
    yXtract_openXform,
    yXtract_setDefaultInputTraits,
    yXtract_setXformSpec,
    yXtract_getHeaderBufSize,
    yXtract_getActualTraits,
    yXtract_getActualBufSizes,
    yXtract_convert,
    yXtract_newPage,
    yXtract_insertedData,
    yXtract_closeXform
};

/* End of File */

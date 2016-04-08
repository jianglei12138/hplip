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
 * xbi2gray.c - Converts bilevel into gray (8-bit gray or 24-bit gray)
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    yBi2GrayTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_BI_2_GRAY_OUTPUT_BPP] = format of output:
 *          8  = output 8-bit gray,
 *          24 = output 24-bit gray.
 *    aXformInfo[IP_BI_2_GRAY_WHITE_PIXEL] =
 *            an RGBQUAD representing a white output pixel.
 *    aXformInfo[IP_BI_2_GRAY_BLACK_PIXEL] =
 *            an RGBQUAD representing a black output pixel.
 *
 *    Each bilevel pixel is output as a full white or full black pixel.
 *    This xform needs to know what values to use for those white and
 *    black output pixels.  Hence the two RGBQUAD items above.
 *    For 8-bit gray output, only the rgbRed field is used in each.
 *
 * Capabilities and Limitations:
 *
 *    Translates each white or black bilevel input pixel into an 8-bit
 *      or 24-bit output white or black output pixel.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 1            8 or 24
 *    iComponentsPerPixel   * must be 1            1 or 3
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Mar 1998 Mark Overton -- wrote code
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
    IP_IMAGE_TRAITS inTraits;  /* traits of the input image */
    UINT     uRowsDone;        /* number of rows converted so far */
    WORD     wOutBitsPerPixel; /* bits/pixel to output (8 or 24) */
    RGBQUAD  rgbWhite;         /* value of an output white */
    RGBQUAD  rgbBlack;         /* value of an output black */
    DWORD    dwInRowBytes;     /* # bytes per input row */
    DWORD    dwOutRowBytes;    /* # bytes per output row */
    DWORD    dwInNextPos;      /* file pos for subsequent input */
    DWORD    dwOutNextPos;     /* file pos for subsequent output */
    DWORD    dwValidChk;       /* struct validity check value */
} B2G_INST, *PB2G_INST;



/*****************************************************************************\
 *
 * bi2gray_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD bi2gray_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PB2G_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(B2G_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(B2G_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2gray_setDefaultInputTraits - Specifies default input image traits
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

static WORD bi2gray_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PB2G_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (pTraits->iBitsPerPixel == 1);
    INSURE (pTraits->iComponentsPerPixel == 1);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2gray_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD bi2gray_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PB2G_INST g;
    UINT      nBits;

    HANDLE_TO_PTR (hXform, g);
    nBits = aXformInfo[IP_BI_2_GRAY_OUTPUT_BPP].dword;
    INSURE (nBits==8 || nBits==24);
    g->wOutBitsPerPixel = (WORD)nBits;
    g->rgbWhite = aXformInfo[IP_BI_2_GRAY_WHITE_PIXEL].rgbquad;
    g->rgbBlack = aXformInfo[IP_BI_2_GRAY_BLACK_PIXEL].rgbquad;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2gray_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD bi2gray_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * bi2gray_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD bi2gray_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PB2G_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pInTraits  = g->inTraits;   /* structure copies */
    *pOutTraits = g->inTraits;
    pOutTraits->iBitsPerPixel       = g->wOutBitsPerPixel;
    pOutTraits->iComponentsPerPixel = g->wOutBitsPerPixel==8 ? 1 : 3;

    g->dwInRowBytes  = (g->inTraits.iPixelsPerRow+7) / 8;
    g->dwOutRowBytes = g->inTraits.iPixelsPerRow * pOutTraits->iComponentsPerPixel;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * bi2gray_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD bi2gray_getActualBufSizes (
    IP_XFORM_HANDLE hXform,            /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PB2G_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->dwInRowBytes;
    *pdwMinOutBufLen = g->dwOutRowBytes;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2gray_convert - Converts one row
 *
\*****************************************************************************/

static WORD bi2gray_convert (
    IP_XFORM_HANDLE hXform,
    DWORD           dwInputAvail,      /* in:  # avail bytes in in-buf */
    PBYTE           pbInputBuf,        /* in:  ptr to in-buffer */
    PDWORD          pdwInputUsed,      /* out: # bytes used from in-buf */
    PDWORD          pdwInputNextPos,   /* out: file-pos to read from next */
    DWORD           dwOutputAvail,     /* in:  # avail bytes in out-buf */
    PBYTE           pbOutputBuf,       /* in:  ptr to out-buffer */
    PDWORD          pdwOutputUsed,     /* out: # bytes written in out-buf */
    PDWORD          pdwOutputThisPos)  /* out: file-pos to write the data */
{
    PB2G_INST g;
    PBYTE     pIn, pOut, pInAfter;
    BYTE      bMask, bBilevel;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("bi2gray_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed     = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    INSURE (dwInputAvail  >= g->dwInRowBytes );
    INSURE (dwOutputAvail >= g->dwOutRowBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pInAfter = pIn + g->dwInRowBytes;

    while (pIn < pInAfter) {
        bBilevel = *pIn++;

        if (g->wOutBitsPerPixel == 24) {
            for (bMask=0x80u; bMask!=0; bMask>>=1) {
                *(RGBQUAD*)pOut = (bMask & bBilevel)
                             ? g->rgbBlack
                             : g->rgbWhite;
                pOut += 3;
            }
        } else {    /* 8 bits per output pixel */
            for (bMask=0x80u; bMask!=0; bMask>>=1) {
                *pOut++ = (bMask & bBilevel)
                             ? g->rgbBlack.rgbRed
                             : g->rgbWhite.rgbRed;
            }
        }
    }

    *pdwInputUsed     = g->dwInRowBytes;
    g->dwInNextPos   += g->dwInRowBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = g->dwOutRowBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += g->dwOutRowBytes;

    g->uRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2gray_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD bi2gray_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * bi2gray_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD bi2gray_newPage (
    IP_XFORM_HANDLE hXform)
{
    PB2G_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * bi2gray_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD bi2gray_closeXform (IP_XFORM_HANDLE hXform)
{
    PB2G_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * bi2grayTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL bi2grayTbl = {
    bi2gray_openXform,
    bi2gray_setDefaultInputTraits,
    bi2gray_setXformSpec,
    bi2gray_getHeaderBufSize,
    bi2gray_getActualTraits,
    bi2gray_getActualBufSizes,
    bi2gray_convert,
    bi2gray_newPage,
    bi2gray_insertedData,
    bi2gray_closeXform
};

/* End of File */

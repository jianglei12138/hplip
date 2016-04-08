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
 * xcrop.c - Crops all four sides of the input image
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    cropTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_CROP_LEFT      ] = left:       # of pixels to remove from left side
 *    aXformInfo[IP_CROP_RIGHT     ] = right:      # of pixels to remove from right side
 *    aXformInfo[IP_CROP_TOP       ] = top:        # of rows to remove from top
 *    aXformInfo[IP_CROP_MAXOUTROWS] = maxOutRows: max # of rows to output
 *
 *    Any (or even all) of the above values may be zero.  If maxOutRows is
 *    zero, then an unlimited number of rows can be output.
 *
 * Capabilities and Limitations:
 *
 *    Crops all four sides of the image.
 *    The image data must be fixed-length rows of uncompressed pixels.
 *    For bilevel data, the "left" value is changed to the nearest multiple
 *    of 8, and the "right" value is changed so the resulting row-width
 *    does not change.
 *    If all crop-amounts above are 0, this xform becomes merely a pass-thru.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * used                 input width - horiz crop
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                used if known        output height, if input known
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
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

#define CHECK_VALUE 0x4ba1dace


typedef struct {
    IP_IMAGE_TRAITS traits;    /* traits of the input image */
    DWORD    dwLeft, dwRight;  /* # pixels to crop, left and right sides */
    DWORD    dwTop;            /* # rows to crop from top */
    DWORD    dwMaxOutRows;     /* max # rows to output */
    DWORD    dwInBytesPerRow;  /* # bytes in each input row */
    DWORD    dwOutBytesPerRow; /* # bytes in each output row */
    DWORD    dwLeftCropBytes;  /* # bytes to toss from left side of each row */
    DWORD    dwInRows;         /* number of rows input so far */
    DWORD    dwOutRows;        /* number of rows output so far */
    DWORD    dwInNextPos;      /* file pos for subsequent input */
    DWORD    dwOutNextPos;     /* file pos for subsequent output */
    DWORD    dwValidChk;       /* struct validity check value */
} CROP_INST, *PCROP_INST;



/*****************************************************************************\
 *
 * crop_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD crop_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PCROP_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(CROP_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(CROP_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * crop_setDefaultInputTraits - Specifies default input image traits
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

static WORD crop_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PCROP_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow>0 && pTraits->iBitsPerPixel>0);
    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * crop_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD crop_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PCROP_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwLeft        = aXformInfo[IP_CROP_LEFT].dword;
    g->dwRight       = aXformInfo[IP_CROP_RIGHT].dword;
    g->dwTop         = aXformInfo[IP_CROP_TOP].dword;
    g->dwMaxOutRows  = aXformInfo[IP_CROP_MAXOUTROWS].dword;

    if (g->dwMaxOutRows == 0)
        g->dwMaxOutRows = 0x7ffffffful;   /* 0 -> infinite */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * crop_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD crop_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    DWORD           *pdwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * crop_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD crop_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PCROP_INST g;
    int        left, right, shift;
    int        inWidth, outWidth;
    int        bpp;
    long       actualOut, maxOut;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */

    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Compute the crop info */

    bpp      = g->traits.iBitsPerPixel;
    left     = g->dwLeft;
    right    = g->dwRight;
    inWidth  = g->traits.iPixelsPerRow;
    outWidth = inWidth - left - right;
    INSURE (outWidth >= 0);

    if (bpp == 1) {
        /* shift to start at nearest byte boundary */
        shift = ((left+4) & ~7) - left;
        left  += shift;   /* this is now a multiple of 8 */
        right += shift;
    }

    g->dwInBytesPerRow  = (bpp*inWidth  + 7) / 8;
    g->dwOutBytesPerRow = (bpp*outWidth + 7) / 8;
    g->dwLeftCropBytes  = (bpp*left     + 7) / 8;

    /* Report the traits */

    *pInTraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;
    pOutTraits->iPixelsPerRow = outWidth;

    /* compute the output lNumRows, if possible */
    if (pInTraits->lNumRows > 0) {
        maxOut = g->dwMaxOutRows;
        actualOut = pInTraits->lNumRows - (long)g->dwTop;
        INSURE (actualOut >= 0);
        pOutTraits->lNumRows = actualOut<maxOut ? actualOut : maxOut;
    }

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * crop_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD crop_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PCROP_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen  = g->dwInBytesPerRow;
    *pdwMinOutBufLen = g->dwOutBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * crop_convert - Converts one row
 *
\*****************************************************************************/

static WORD crop_convert (
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
    PCROP_INST g;
    DWORD      dwOutBytes;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("crop_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Check if we should discard the row (vertical cropping) ****/

    dwOutBytes = (g->dwInRows < g->dwTop  ||  g->dwOutRows >= g->dwMaxOutRows)
                  ? 0 : g->dwOutBytesPerRow;

    /**** Output a Row ****/

    INSURE (dwInputAvail  >= g->dwInBytesPerRow);
    INSURE (dwOutputAvail >= dwOutBytes);

    if (dwOutBytes > 0) {
        memcpy (pbOutputBuf, pbInputBuf+g->dwLeftCropBytes, dwOutBytes);
        g->dwOutRows += 1;
    }

    g->dwInRows += 1;

    *pdwInputUsed     = g->dwInBytesPerRow;
    g->dwInNextPos   += g->dwInBytesPerRow;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = dwOutBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += dwOutBytes;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * crop_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD crop_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * crop_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD crop_newPage (
    IP_XFORM_HANDLE hXform)
{
    PCROP_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * crop_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD crop_closeXform (IP_XFORM_HANDLE hXform)
{
    PCROP_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * cropTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL cropTbl = {
    crop_openXform,
    crop_setDefaultInputTraits,
    crop_setXformSpec,
    crop_getHeaderBufSize,
    crop_getActualTraits,
    crop_getActualBufSizes,
    crop_convert,
    crop_newPage,
    crop_insertedData,
    crop_closeXform
};

/* End of File */

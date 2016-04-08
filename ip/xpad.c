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
 * xpad.c - Pads all four sides of the input image with extra pixels
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    padTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_PAD_LEFT  ] = left:    # of pixels to add to left side (or negative)
 *    aXformInfo[IP_PAD_RIGHT ] = right:   # of pixels to add to right side
 *    aXformInfo[IP_PAD_TOP   ] = top:     # of rows to add to top (or negative)
 *    aXformInfo[IP_PAD_BOTTOM] = bottom:  # of rows to add to bottom
 *    aXformInfo[IP_PAD_VALUE ] = pad value (0-255)
 *    aXformInfo[IP_PAD_MIN_HEIGHT] = minimum # of input rows, pad if needed
 *
 *    The pad value above (at index 4) is the value of the added pad-pixels.
 *    For color data, all three bytes will be set to this value.
 *    For bi-level data, only the lsb is used.
 *
 *    If 'left' is negative, then the output width will be forced to be a
 *    multiple of abs(left) pixels.
 *    If 'top' is negative, the same thing is done with the height.
 *
 * Capabilities and Limitations:
 *
 *    Pads all four sides of the image.
 *    The image data must be fixed-length rows of uncompressed pixels.
 *    For bilevel data, the "left" value is changed to the nearest multiple
 *    of 8, and the "right" value is changed so the resulting row-width
 *    does not change.
 *    If all pad-amounts above are 0, this xform becomes merely a pass-thru.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * used                 input width + horiz pad
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
    DWORD    dwLeft, dwRight;  /* # pixels to pad, left and right sides */
    DWORD    dwTop;            /* # rows to pad to top */
    DWORD    dwBottom;         /* # rows to pad to bottom */
    DWORD    dwMinInRows;      /* minimum # input rows, pad if necessary */
    DWORD    dwInBytesPerRow;  /* # bytes in each input row */
    DWORD    dwOutBytesPerRow; /* # bytes in each output row */
    DWORD    dwLeftPadBytes;   /* # bytes to add to left side of each row */
    DWORD    dwRightPadBytes;  /* # bytes to add to right side of each row */
    BYTE     bPadVal;          /* value of pad pixels */
    DWORD    dwInRows;         /* number of rows input so far */
    DWORD    dwOutRows;        /* number of rows output so far */
    DWORD    dwInNextPos;      /* file pos for subsequent input */
    DWORD    dwOutNextPos;     /* file pos for subsequent output */
    DWORD    dwValidChk;       /* struct validity check value */
} PAD_INST, *PPAD_INST;



/*****************************************************************************\
 *
 * pad_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD pad_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PPAD_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(PAD_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(PAD_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pad_setDefaultInputTraits - Specifies default input image traits
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

static WORD pad_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PPAD_INST g;

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
 * pad_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD pad_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PPAD_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwLeft   = aXformInfo[IP_PAD_LEFT].dword;
    g->dwRight  = aXformInfo[IP_PAD_RIGHT].dword;
    g->dwTop    = aXformInfo[IP_PAD_TOP].dword;
    g->dwBottom = aXformInfo[IP_PAD_BOTTOM].dword;
    g->bPadVal  = (BYTE)aXformInfo[IP_PAD_VALUE].dword;
    g->dwMinInRows = aXformInfo[IP_PAD_MIN_HEIGHT].dword;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pad_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD pad_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    DWORD           *pdwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * pad_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD pad_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PPAD_INST g;
    int        left, right, shift, more;
    int        inWidth, outWidth;
    int        bpp;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */

    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Compute the pad info */

    bpp      = g->traits.iBitsPerPixel;
    left     = g->dwLeft;
    right    = g->dwRight;
    inWidth  = g->traits.iPixelsPerRow;

    if (left < 0) {
        left = -left;
        // pad horizontally so that width is a multiple of left
        outWidth = ((inWidth+left-1) / left) * left;
        more = outWidth - inWidth;
        left = more >> 1;
        right = more - left;
        g->dwLeft  = left;
        g->dwRight = right;
    }

    outWidth = inWidth + left + right;

    if (bpp == 1) {
        /* shift to start at nearest byte boundary */
        shift = ((left+4) & ~7) - left;
        left  += shift;   /* this is now a multiple of 8 */
        right += shift;

        g->bPadVal = g->bPadVal & 1 ? 0xffu : 0;
    }

    g->dwInBytesPerRow  = (bpp*inWidth  + 7) / 8;
    g->dwOutBytesPerRow = (bpp*outWidth + 7) / 8;
    g->dwLeftPadBytes   = (bpp*left     + 7) / 8;
    g->dwRightPadBytes  = g->dwOutBytesPerRow - g->dwInBytesPerRow - g->dwLeftPadBytes;

    /* Report the traits */

    *pInTraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;
    pOutTraits->iPixelsPerRow = outWidth;

    if (pInTraits->lNumRows > 0) {
        if ((int)g->dwTop < 0) {
            int top, bot, inHeight, outHeight;
            top = - (int)g->dwTop;
            // pad vertically so that #rows is a multiple of top
            inHeight = pInTraits->lNumRows;
            outHeight = ((inHeight+top-1) / top) * top;
            more = outHeight - inHeight;
            top = more >> 1;
            bot = more - top;
            g->dwTop    = top;
            g->dwBottom = bot;
        }

        pOutTraits->lNumRows = pInTraits->lNumRows + g->dwTop + g->dwBottom;
    }

    INSURE ((int)g->dwLeft >= 0);
    INSURE ((int)g->dwTop  >= 0);

    return IP_DONE | (g->dwTop>0 ? 0 : IP_READY_FOR_DATA);

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * pad_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD pad_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PPAD_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen  = g->dwInBytesPerRow;
    *pdwMinOutBufLen = g->dwOutBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pad_convert - Converts one row
 *
\*****************************************************************************/

static WORD pad_convert (
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
    PPAD_INST g;
    BOOL      bWhiteRow;

    HANDLE_TO_PTR (hXform, g);
    bWhiteRow = FALSE;

    /**** Decide what to do ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("pad_convert: Told to flush.\n"), 0, 0);

        if (g->dwInRows < g->dwMinInRows) {
            /* We need to output another pad row on the bottom */
            bWhiteRow = TRUE;
            g->dwInRows += 1;
        } else if (g->dwBottom > 0) {
            /* We need to output another pad row on the bottom */
            bWhiteRow = TRUE;
            g->dwBottom -= 1;
        } else {
            /* We are done */
            *pdwInputUsed = *pdwOutputUsed = 0;
            *pdwInputNextPos  = g->dwInNextPos;
            *pdwOutputThisPos = g->dwOutNextPos;
            return IP_DONE;
        }
    } else if (g->dwOutRows < g->dwTop) {
        /* We need to output another pad row on the top */
        bWhiteRow = TRUE;
    }

    /**** Output a Row ****/

    INSURE (bWhiteRow || (dwInputAvail >= g->dwInBytesPerRow));
    INSURE (dwOutputAvail >= g->dwOutBytesPerRow);

    if (bWhiteRow)
        memset (pbOutputBuf, g->bPadVal, g->dwOutBytesPerRow);
    else {
        BYTE *p = pbOutputBuf;
        memset (p, g->bPadVal, g->dwLeftPadBytes);
        p += g->dwLeftPadBytes;
        memcpy (p, pbInputBuf, g->dwInBytesPerRow);
        p += g->dwInBytesPerRow;
        memset (p, g->bPadVal, g->dwRightPadBytes);

        g->dwInRows += 1;
        *pdwInputUsed     = g->dwInBytesPerRow;
        g->dwInNextPos   += g->dwInBytesPerRow;
    }

    g->dwOutRows += 1;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwOutputUsed    = g->dwOutBytesPerRow;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += g->dwOutBytesPerRow;

    return (bWhiteRow ? 0 : IP_CONSUMED_ROW) |
           IP_PRODUCED_ROW |
           (g->dwOutRows < g->dwTop ? 0 : IP_READY_FOR_DATA);

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pad_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD pad_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * pad_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD pad_newPage (
    IP_XFORM_HANDLE hXform)
{
    PPAD_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * pad_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD pad_closeXform (IP_XFORM_HANDLE hXform)
{
    PPAD_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * padTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL padTbl = {
    pad_openXform,
    pad_setDefaultInputTraits,
    pad_setXformSpec,
    pad_getHeaderBufSize,
    pad_getActualTraits,
    pad_getActualBufSizes,
    pad_convert,
    pad_newPage,
    pad_insertedData,
    pad_closeXform
};

/* End of File */

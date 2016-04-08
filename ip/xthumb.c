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
 * xthumb.c - Downscales quickly by large factor for generating thumbnail image
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    thumbTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_THUMB_SCALE_SPEC] determines scale-factor indirectly or directly,
 *    depending on whether it's positive or negative.
 *
 *    positive:  value is the maximum output-width in pixels. xthumb will
 *               select the N for the scale-factor of the form 1/N which
 *               results in the largest possible output width not exceeding
 *               the value in aXformInfo[IP_THUMB_SCALE_SPEC].
 *
 *    negative:  absolute value is N for the scale-factor of the form 1/N.
 *               the scale-factor is being specified directly.
 *
 * Capabilities and Limitations:
 *
 *    Downscales any type of raw data quickly by a large scale-factor
 *    specified by aXformInfo[IP_THUMB_SCALE_SPEC].  The pixels are averaged together.
 *    For bilevel input, the output is 8-bit gray, where each gray pixel
 *    is the average blackness of an area of the input bilevel image.
 *    For 8-bit gray and 24-bit color input data, the output data is
 *    the same type as the input.
 *
 *    The scale-factor is of the form 1/N, where N is an integer.
 *    Warning: If N is 1 or almost 1, the actual output width will be
 *    quite smaller than that requested.
 *
 *    If input width is smaller than output width, N will be 1 (no
 *    scaling).
 *
 *    ipGetImageTraits must be called to determine the actual output width.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * used                 based on scale factor
 *    iBitsPerPixel         * must be 1, 8 or 24   8 or 24
 *    iComponentsPerPixel   * must be 1 or 3       same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                used if valid        based on scale factor
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Mar 1998 Mark Overton -- wrote original code
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
    IP_IMAGE_TRAITS inTraits; /* traits of the input image */
    int       iFactorSpec;    /* factor spec in aXformInfo[0] */
    WORD      wScale;         /* the N in scale-factor of 1/N */
    WORD      wPreShift;      /* # bits to shift sum right before multiply */
    DWORD     dwSumFac;       /* factor to multiply sum by (16.16 fixed-pt) */
    DWORD     dwOutputWidth;  /* # pixels per row in output */
    DWORD     dwInRowBytes;   /* # bytes in each input row */
    DWORD     dwOutRowBytes;  /* # bytes in each output row */
    WORD      wMoreRows2Sum;  /* # more rows to be summed together */
    PULONG    pulSums;        /* pixel-sums; each pixel is a ULONG */
    ULONG     ulRowsInput;    /* # of rows input  so far */
    ULONG     ulRowsOutput;   /* # of rows output so far */
    DWORD     dwInNextPos;    /* file pos for subsequent input */
    DWORD     dwOutNextPos;   /* file pos for subsequent output */
    DWORD     dwValidChk;     /* struct validity check value */
} TN_INST, *PTN_INST;



/*****************************************************************************\
 *
 * thumb_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD thumb_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PTN_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(TN_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(TN_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumb_setDefaultInputTraits - Specifies default input image traits
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

static WORD thumb_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PTN_INST g;

    HANDLE_TO_PTR (hXform, g);

    INSURE (pTraits->iPixelsPerRow > 0);
    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumb_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD thumb_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PTN_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->iFactorSpec = (int)aXformInfo[IP_THUMB_SCALE_SPEC].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumb_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD thumb_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * thumb_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD thumb_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PTN_INST g;
    int      N;
    long     lMaxSum;
    long     nBytes;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Compute N in our scale-factor of 1/N */
    if (g->iFactorSpec <= 0) {
        N = -(g->iFactorSpec);
    } else {
        /* the +iFactorSpec-1 below biases N high (cieling function),
         * so the output width is biased low, so that we'll never go
         * larger than the requested output width.
         */
        N = (g->inTraits.iPixelsPerRow + g->iFactorSpec - 1) / g->iFactorSpec;
    }

    if (N < 1) N = 1;
    g->wScale = N;

    /* Compute max summation of N-by-N input pixels */
    lMaxSum = (long)N*N * (g->inTraits.iBitsPerPixel==1 ? 1 : 255);

    /* Compute pre-shift so that max sum does not exceed 16 bits */
         if (lMaxSum >= (1L<<28)) g->wPreShift = 16;
    else if (lMaxSum >= (1L<<24)) g->wPreShift = 12;
    else if (lMaxSum >= (1L<<20)) g->wPreShift = 8;
    else if (lMaxSum >= (1L<<16)) g->wPreShift = 4;
    else                          g->wPreShift = 0;

    /* Now do the pre-shift on max sum */
    lMaxSum >>= g->wPreShift;
    INSURE (lMaxSum < (1L<<16));  /* make sure it fits in 16 bits */

    /* And compute the factor for converting a sum into a 0..255 pixel */
    g->dwSumFac = (DWORD)((255.0/(float)lMaxSum) * (float)(1L<<16));

    *pInTraits  = g->inTraits;   /* structure copies */
    *pOutTraits = g->inTraits;

    if (pOutTraits->iBitsPerPixel == 1)
        pOutTraits->iBitsPerPixel = 8;
    pOutTraits->iPixelsPerRow = g->dwOutputWidth = pInTraits->iPixelsPerRow / N;
    if (pOutTraits->lNumRows >= 0)
        pOutTraits->lNumRows /= N;

    g->wMoreRows2Sum = N;
    g->dwInRowBytes =
        (pInTraits->iPixelsPerRow * pInTraits->iBitsPerPixel + 7) / 8;
    g->dwOutRowBytes = g->dwOutputWidth * pInTraits->iComponentsPerPixel;

    nBytes = g->dwOutRowBytes * sizeof(ULONG);
    IP_MEM_ALLOC (nBytes, g->pulSums);
    memset (g->pulSums, 0, nBytes);

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * thumb_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD thumb_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PTN_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->dwInRowBytes;
    *pdwMinOutBufLen = g->dwOutRowBytes;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumb_convert - Converts one row
 *
\*****************************************************************************/

static WORD thumb_convert (
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
    PTN_INST g;
    PBYTE    pIn, pOut;
    BYTE     bMask, bVal=0;
    ULONG    ulSum, sum0, sum1, sum2;
    ULONG    *pulSum, *pulSumAfter;
    UINT     u;
    BOOL     fSentRow;

    HANDLE_TO_PTR (hXform, g);
    pulSumAfter = g->pulSums + g->dwOutRowBytes;

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("thumb_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Sum this Input Row ****/

    INSURE (dwInputAvail >= g->dwInRowBytes);
    pIn = pbInputBuf;

    switch (g->inTraits.iBitsPerPixel) {
        case 1:   /* bilevel input */
            bMask = 0;
            for (pulSum=g->pulSums; pulSum<pulSumAfter; pulSum++) {
                ulSum = *pulSum;
                for (u=g->wScale; u>0; u--) {
                    if (bMask == 0) {
                        bMask = 0x80u;
                        bVal = *pIn++;
                    }
                    if ((bMask & bVal) == 0) {
                        /* since the sum is a measure of overall whiteness,
                         * increment it for a *white* input pixel (0)  */
                        ulSum += 1;
                    }
                    bMask >>= 1;
                }
                *pulSum = ulSum;
            }
        break;

        case 8:   /* 8-bit gray input */
            for (pulSum=g->pulSums; pulSum<pulSumAfter; pulSum++) {
                ulSum = *pulSum;
                for (u=g->wScale; u>0; u--)
                    ulSum += (ULONG)(*pIn++);
                *pulSum = ulSum;
            }
        break;

        case 24:   /* 3-component color input */
            for (pulSum=g->pulSums; pulSum<pulSumAfter; pulSum+=3) {
                sum0 = pulSum[0];
                sum1 = pulSum[1];
                sum2 = pulSum[2];
                for (u=g->wScale; u>0; u--) {
                    sum0 += (ULONG)(*pIn++);
                    sum1 += (ULONG)(*pIn++);
                    sum2 += (ULONG)(*pIn++);
                }
                pulSum[0] = sum0;
                pulSum[1] = sum1;
                pulSum[2] = sum2;
            }
        break;
    } /* switch */

    *pdwInputUsed     = g->dwInRowBytes;
    g->dwInNextPos   += g->dwInRowBytes;
    *pdwInputNextPos  = g->dwInNextPos;
    g->ulRowsInput += 1;

    /**** If it's time, Compute Output Row ****/

    *pdwOutputThisPos = g->dwOutNextPos;
    g->wMoreRows2Sum -= 1;

    if (g->wMoreRows2Sum > 0) {
        fSentRow = FALSE;
        *pdwOutputUsed = 0;
    } else {
        g->wMoreRows2Sum = g->wScale;
        fSentRow = TRUE;
        g->ulRowsOutput += 1;
        INSURE (dwOutputAvail >= g->dwOutRowBytes);
        *pdwOutputUsed   = g->dwOutRowBytes;
        g->dwOutNextPos += g->dwOutRowBytes;

        pulSum = g->pulSums;
        pOut = pbOutputBuf;
        for (pulSum=g->pulSums; pulSum<pulSumAfter; pulSum++) {
            *pOut++ = (BYTE)((*pulSum >> g->wPreShift) * g->dwSumFac >> 16);
        }

        memset (g->pulSums, 0, g->dwOutRowBytes*sizeof(ULONG));
    }

    /**** Return ****/   

    return IP_CONSUMED_ROW   |
           IP_READY_FOR_DATA |
           (fSentRow ? IP_PRODUCED_ROW : 0);

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumb_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD thumb_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * thumb_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD thumb_newPage (
    IP_XFORM_HANDLE hXform)
{
    PTN_INST g;

    HANDLE_TO_PTR (hXform, g);
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * thumb_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD thumb_closeXform (IP_XFORM_HANDLE hXform)
{
    PTN_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->pulSums != NULL)
        IP_MEM_FREE (g->pulSums);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * thumbTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL thumbTbl = {
    thumb_openXform,
    thumb_setDefaultInputTraits,
    thumb_setXformSpec,
    thumb_getHeaderBufSize,
    thumb_getActualTraits,
    thumb_getActualBufSizes,
    thumb_convert,
    thumb_newPage,
    thumb_insertedData,
    thumb_closeXform
};

/* End of File */

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
 * xconvolve.c - convolution using any number or rows and columns up to max
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    convolveTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_CONVOLVE_NROWS  ] = # rows in convolution matrix (odd)
 *    aXformInfo[IP_CONVOLVE_NCOLS  ] = # columns in convolution matrix (odd)
 *    aXformInfo[IP_CONVOLVE_MATRIX ] = ptr to convolution matrix
 *    aXformInfo[IP_CONVOLVE_DIVISOR] = divide by this after summing products
 *
 *    The matrix is an array of int's, ordered left to right, top to bottom.
 *    After the pixels are multiplied by the elements in the matrix and these
 *    products summed together, the sum is divided by the integer divisor.
 *
 *    If you set nRows and nCols to 7, then this xform is identical to the
 *    "User defined filter" feature in Paint Shop Pro.
 *
 *    This xform makes a copy of the given matrix, so its memory can be freed
 *    after you've called setXformSpec.
 *
 *    IP_CONVOLVE_MAXSIZE is the max number of rows or columns.
 *
 * Capabilities and Limitations:
 *
 *    The input data must be grayscale (8 or 16 bits/pixel), or color (24 or
 *    48 bits/pixel) in a luminance-chrominance color-space.  This xform only
 *    changes the first component of color data (assumed to be the luminance).
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * 8, 16, 24 or 48      same as default input
 *    iComponentsPerPixel   * 1 or 3               same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
\******************************************************************************/

/* Use the #define below if this transform will exist in a dll outside of the
 * image pipeline.  This will allow the functions to be exported.
 * #define EXPORT_TRANFORM 1
 */

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
    int      iBytesPerPixel;  /* # bytes in each pixel */
    DWORD    dwRowsRead;      /* number of rows read so far */
    DWORD    dwRowsWritten;   /* number of rows output so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    int      nCols;           /* # columns in the matrix (must be odd) */
    int      nRows;           /* # rows in the matrix (must be odd) */
    int      nRowsFilled;     /* # rows filled so far in the matrix */
    int      iDivisor;        /* divide sum of products by this */
    int      matrix[IP_CONVOLVE_MAXSIZE*IP_CONVOLVE_MAXSIZE];  /* the matrix */
    PBYTE    apRows[IP_CONVOLVE_MAXSIZE];  /* ptrs to buffered rows */
    DWORD    dwValidChk;      /* struct validity check value */
} CONV_INST, *PCONV_INST;



/*****************************************************************************\
 *
 * convolve_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PCONV_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(CONV_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(CONV_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * convolve_setDefaultInputTraits - Specifies default input image traits
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

FUNC_STATUS WORD convolve_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PCONV_INST g;
    int        bpp;
    int           comps;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are valid */
    bpp   = pTraits->iBitsPerPixel;
    comps = pTraits->iComponentsPerPixel;
    INSURE ((comps==1 && (bpp==8 || bpp==16))  ||
            (comps==3 && (bpp==24 || bpp==48)));
    INSURE (pTraits->iPixelsPerRow > 0);

    g->traits = *pTraits;   /* a structure copy */
    g->iBytesPerPixel = g->traits.iBitsPerPixel / 8;
    g->dwBytesPerRow = g->traits.iPixelsPerRow * g->iBytesPerPixel;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * convolve_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PCONV_INST g;
    int        i, n;

    HANDLE_TO_PTR (hXform, g);

    g->nRows    = aXformInfo[IP_CONVOLVE_NROWS  ].dword;
    g->nCols    = aXformInfo[IP_CONVOLVE_NCOLS  ].dword;
    g->iDivisor = aXformInfo[IP_CONVOLVE_DIVISOR].dword;

    INSURE ((g->nRows&1)!=0 && g->nRows>0 && g->nRows<=IP_CONVOLVE_MAXSIZE);
    INSURE ((g->nCols&1)!=0 && g->nCols>0 && g->nCols<=IP_CONVOLVE_MAXSIZE);
    INSURE (g->iDivisor != 0);
    INSURE (aXformInfo[IP_CONVOLVE_MATRIX].pvoid != 0);

    n = g->nRows * g->nCols;
    for (i=0; i<n; i++)
        g->matrix[i] = ((int*)(aXformInfo[IP_CONVOLVE_MATRIX].pvoid))[i];

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * convolve_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * convolve_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PCONV_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pIntraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * convolve_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD convolve_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PCONV_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * CopyRow - Copies row to row-buffer, filling over-run zone on sides
 *
\*****************************************************************************/

static void CopyRow (
    PCONV_INST g,       /* ptr to our instance variables */
    PBYTE      pbSrc,   /* input buffer */
    PBYTE      pbDest)  /* output buffer in apRows (with side-zones allocated) */
{
    int nSidePixels;
    int i;

    nSidePixels = g->nCols / 2;

    /* copy leftmost pixel into all pixels in left side-zone */
    for (i=0; i<nSidePixels; i++) {
        memcpy (pbDest, pbSrc, g->iBytesPerPixel);
        pbDest += g->iBytesPerPixel;
    }

    /* copy the buffer */
    memcpy (pbDest, pbSrc, g->dwBytesPerRow);
    pbDest += g->dwBytesPerRow;
    pbSrc  += g->dwBytesPerRow - g->iBytesPerPixel;  /* leave at rightmost pixel */

    /* copy rightmost pixel into all pixels in right side-zone */
    for (i=0; i<nSidePixels; i++) {
        memcpy (pbDest, pbSrc, g->iBytesPerPixel);
        pbDest += g->iBytesPerPixel;
    }
}



/*****************************************************************************\
 *
 * convolve_bytes - performs convolution on a row pixels with 8 or 24 bits each
 *
\*****************************************************************************/

static void convolve_bytes (
    PCONV_INST g,       /* ptr to our instance variables */
    PBYTE      pbDest)  /* output buffer */
{
    PBYTE pbSrc, pbDestAfter;
    int   iSrcOffset, iSideOffset, iSum;
    int  *pMatrix;
    int   row, col;

    pbDestAfter = pbDest + g->dwBytesPerRow;
    iSrcOffset = 0;
    iSideOffset = g->iBytesPerPixel * (g->nCols>>1);

    while (pbDest < pbDestAfter)   /* for each output pixel ... */
    {
        iSum = 0;
        pMatrix = g->matrix;

        for (row=0; row<g->nRows; row++) {
            pbSrc = g->apRows[row] + iSrcOffset;
            for (col=0; col<g->nCols; col++) {
                iSum += (*pMatrix++) * (int)(unsigned)(*pbSrc);
                pbSrc += g->iBytesPerPixel;
            }
        }

        iSum = (iSum + (g->iDivisor>>1)) / g->iDivisor;
        if (iSum < 0) iSum = 0; else if (iSum > 255) iSum = 255;
        *pbDest++ = (BYTE)iSum;

        if (g->iBytesPerPixel == 3) {
            /* copy chrominance values from the center pixel to the output pixel */
            pbSrc = g->apRows[g->nRows>>1] + (iSideOffset + iSrcOffset + 1);
            *pbDest++ = *pbSrc++;
            *pbDest++ = *pbSrc++;
        }

        iSrcOffset += g->iBytesPerPixel;
    }
}



/*****************************************************************************\
 *
 * convolve_words - performs convolution on a row pixels with 16 or 48 bits each
 *
\*****************************************************************************/

static void convolve_words (
    PCONV_INST g,       /* ptr to our instance variables */
    PWORD      pwDest)  /* output buffer */
{
    PWORD pwSrc, pwDestAfter;
    int   iSrcOffset, iSideOffset, iSum;
    int  *pMatrix;
    int   row, col;

    pwDestAfter = pwDest + (g->dwBytesPerRow>>1);
    iSrcOffset = 0;
    iSideOffset = g->traits.iComponentsPerPixel * (g->nCols>>1);

    while (pwDest < pwDestAfter)   /* for each output pixel ... */
    {
        iSum = 0;
        pMatrix = g->matrix;

        for (row=0; row<g->nRows; row++) {
            pwSrc = (WORD*)(g->apRows[row]) + iSrcOffset;
            for (col=0; col<g->nCols; col++) {
                iSum += (*pMatrix++) * (int)(unsigned)(*pwSrc);
                pwSrc += g->traits.iComponentsPerPixel;
            }
        }

        iSum = (iSum + (g->iDivisor>>1)) / g->iDivisor;
        if (iSum < 0) iSum = 0; else if (iSum > 0x00ffff) iSum = 0x00ffff;
        *pwDest++ = (WORD)iSum;

        if (g->traits.iComponentsPerPixel == 3) {
            /* copy chrominance values from the center pixel to the output pixel */
            pwSrc = (WORD*)(g->apRows[g->nRows>>1]) + (iSideOffset + iSrcOffset + 1);
            *pwDest++ = *pwSrc++;
            *pwDest++ = *pwSrc++;
        }

        iSrcOffset += g->traits.iComponentsPerPixel;
    }
}



/*****************************************************************************\
 *
 * convolve_convert - Converts one row
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_convert (
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
    PCONV_INST g;
    WORD       wFlags;

    HANDLE_TO_PTR (hXform, g);

    *pdwInputUsed = *pdwOutputUsed = 0;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwOutputThisPos = g->dwOutNextPos;
    wFlags = 0;

    /**** We'll always consume an input row, so update its vars ****/

    if (pbInputBuf != NULL) {
        INSURE (dwInputAvail >= g->dwBytesPerRow);
        *pdwInputUsed     = g->dwBytesPerRow;
        g->dwInNextPos   += g->dwBytesPerRow;
        *pdwInputNextPos  = g->dwInNextPos;
        g->dwRowsRead += 1;
        wFlags |= IP_CONSUMED_ROW | IP_READY_FOR_DATA;
    }

    /**** Fill the row-buffers ****/

    if (g->nRowsFilled < g->nRows) {    /* This is the initial fill */
        INSURE (pbInputBuf != NULL);  /* not allowed to flush now */
        do {
            IP_MEM_ALLOC (g->dwBytesPerRow + g->iBytesPerPixel*g->nCols,
                          g->apRows[g->nRowsFilled]);
            CopyRow (g, pbInputBuf, g->apRows[g->nRowsFilled]);
            g->nRowsFilled += 1;
        } while (g->nRowsFilled < (g->nRows+1)/2);

        if (g->nRowsFilled < g->nRows)
            return wFlags;  /* we're not done with initial fill */
    } else {
        /* rotate buffer-pointers, and copy new row to bottom row */
        int i;
        PBYTE pBottomRow;

        if (pbInputBuf == NULL) {
            /* flushing */
            if (g->dwRowsRead == g->dwRowsWritten)
                return IP_DONE;
            pbInputBuf = g->apRows[g->nRows-1];  /* duplicate prior row */
        }

        pBottomRow = g->apRows[0];  /* new bottom row overwrites oldest top row */
        for (i=1; i<g->nRows; i++)
            g->apRows[i-1] = g->apRows[i];
        g->apRows[g->nRows-1] = pBottomRow;

        CopyRow (g, pbInputBuf, pBottomRow);
    }

    /**** Output a Row ****/

    INSURE (dwOutputAvail >= g->dwBytesPerRow);

    if (g->traits.iBitsPerPixel==8 || g->traits.iBitsPerPixel==24)
        convolve_bytes (g, pbOutputBuf);
    else
        convolve_words (g, (WORD*)pbOutputBuf);

    *pdwOutputUsed    = g->dwBytesPerRow;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += g->dwBytesPerRow;
    g->dwRowsWritten += 1;
    wFlags |= IP_PRODUCED_ROW;

    return wFlags;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * convolve_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * convolve_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_newPage (
    IP_XFORM_HANDLE hXform)
{
    PCONV_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * convolve_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD convolve_closeXform (IP_XFORM_HANDLE hXform)
{
    PCONV_INST g;
    int        i;

    HANDLE_TO_PTR (hXform, g);

    /* free any rows that were allocated */
    for (i=0; i<IP_CONVOLVE_MAXSIZE; i++)
        if (g->apRows[i] != NULL)
            IP_MEM_FREE (g->apRows[i]);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * convolveTbl - Jump-table for transform driver
 *
\*****************************************************************************/

#ifdef EXPORT_TRANSFORM
__declspec (dllexport)
#endif
IP_XFORM_TBL convolveTbl = {
    convolve_openXform,
    convolve_setDefaultInputTraits,
    convolve_setXformSpec,
    convolve_getHeaderBufSize,
    convolve_getActualTraits,
    convolve_getActualBufSizes,
    convolve_convert,
    convolve_newPage,
    convolve_insertedData,
    convolve_closeXform
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
        *pXform = clrmapTbl;
    else
        wRet = IP_FATAL_ERROR;

    return wRet;
}
#endif

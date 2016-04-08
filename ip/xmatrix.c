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
 * xmatrix.c - runs color data thru a 3x3 matrix (24 or 48 bits/pixel)
 *
 * Mark Overton, June 2000
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    matrixTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[0] = pointer to the matrix, which is an array of nine
 *    signed integers (type int) in 8.24 fixed-point.  If we pretend that
 *    the array is named 'm', then pixels are computed as follows:
 *
 *        r_out = m[0]*r_in + m[1]*g_in + m[2]*b_in
 *        g_out = m[3]*r_in + m[4]*g_in + m[5]*b_in
 *        b_out = m[6]*r_in + m[7]*g_in + m[8]*b_in
 *
 * Capabilities and Limitations:
 *
 *    Passes pixels through a 3x3 matrix.  24 or 48 bits/pixel only.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 24 or 48     same as default input
 *    iComponentsPerPixel   * must be 3            same as default input
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
    DWORD    dwRowsDone;      /* number of rows processed so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    int      mat[9];          /* the matrix */
    DWORD    dwValidChk;      /* struct validity check value */
} MAT_INST, *PMAT_INST;



/*****************************************************************************\
 *
 * mat_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PMAT_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(MAT_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(MAT_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * mat_setDefaultInputTraits - Specifies default input image traits
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

FUNC_STATUS WORD mat_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PMAT_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow > 0);
    INSURE (pTraits->iBitsPerPixel==24 || pTraits->iBitsPerPixel==48);
    INSURE (pTraits->iComponentsPerPixel == 3);

    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * mat_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PMAT_INST g;
    int       *ptr;

    HANDLE_TO_PTR (hXform, g);

    ptr = (int*)aXformInfo[0].pvoid;
    INSURE (ptr != NULL);
    memcpy (g->mat, ptr, sizeof(g->mat));

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * mat_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * mat_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PMAT_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pIntraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    /* At this point, nothing will change, so compute internal variables */
    g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;

    if (g->traits.iBitsPerPixel == 24) {
        int i;
        for (i=0; i<9; i++) {
            /* we'll use 16 bits of fraction instead of 24 */
            g->mat[i] = (g->mat[i]+(1<<7)) >> 8;
        }
    }

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * mat_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD mat_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PMAT_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * mat_convert - Converts one row
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_convert (
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
    PMAT_INST g;
    int       nBytes;
    PBYTE     pIn, pOut, pOutAfter;
    int rIn, gIn, bIn, rOut, gOut, bOut;

    HANDLE_TO_PTR (hXform, g);

    /**** Check for flushing ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("mat_convert: Told to flush.\n"), 0, 0);
        /* we are done */
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = g->dwBytesPerRow;
    INSURE (dwInputAvail  >= (DWORD)nBytes);
    INSURE (dwOutputAvail >= (DWORD)nBytes);

    pIn       = pbInputBuf;
    pOut      = pbOutputBuf;
    pOutAfter = pbOutputBuf + nBytes;

    if (g->traits.iBitsPerPixel == 24)
    {
        while (pOut < pOutAfter)
        {
            rIn = (int)(unsigned)(*pIn++);
            gIn = (int)(unsigned)(*pIn++);
            bIn = (int)(unsigned)(*pIn++);

            /* for this 24-bit case, we assume that the matrix has already
             * been shifted down to just 16 bits of fraction instead of 24 */
            rOut = rIn*g->mat[0] + gIn*g->mat[1] + bIn*g->mat[2];
            gOut = rIn*g->mat[3] + gIn*g->mat[4] + bIn*g->mat[5];
            bOut = rIn*g->mat[6] + gIn*g->mat[7] + bIn*g->mat[8];

            /* we have 16 bits of fraction, so round to integer */
            rOut = (rOut+(1<<15)) >> 16;
            gOut = (gOut+(1<<15)) >> 16;
            bOut = (bOut+(1<<15)) >> 16;

            /* make sure the result fits in a byte */
            if (rOut > 255) rOut = 255; else if (rOut < 0) rOut = 0;
            if (gOut > 255) gOut = 255; else if (gOut < 0) gOut = 0;
            if (bOut > 255) bOut = 255; else if (bOut < 0) bOut = 0;

            *pOut++ = (BYTE)rOut;
            *pOut++ = (BYTE)gOut;
            *pOut++ = (BYTE)bOut;
        }
    }
    else   /* 48 bits per pixel */
    {
        WORD *pwIn, *pwOut;
        pwIn  = (WORD*)pIn;
        pwOut = (WORD*)pOut;

        while (pwOut < (WORD*)pOutAfter)
        {
            int prod0 = 0, prod1 = 0, prod2 = 0;

            /* The fixed-point calculations below are as follows:
             *     17.15 = input pixel
             *      8.24 = factor from matrix
             *     25.39 = result of 32x32->64 multiply of above numbers
             *     25.7  = result of discarding low 32 bits of 64-bit product
             * Finally, we add (1<<6) and shift right 7 to round to integer.
             *
             * The MUL32HIHALF macro does a signed 32x32->64 multiply, and then
             * discards the low 32 bits, giving you the high 32 bits.
             */

            rIn = (int)(unsigned)(*pwIn++) << 15;
            gIn = (int)(unsigned)(*pwIn++) << 15;
            bIn = (int)(unsigned)(*pwIn++) << 15;

            MUL32HIHALF (rIn, g->mat[0], prod0);
            MUL32HIHALF (gIn, g->mat[1], prod1);
            MUL32HIHALF (bIn, g->mat[2], prod2);
            rOut = (prod0 + prod1 + prod2 + (1<<6)) >> 7;
            if (rOut > 0x00ffff) rOut = 0x00ffff; else if (rOut < 0) rOut = 0;
            *pwOut++ = rOut;

            MUL32HIHALF (rIn, g->mat[3], prod0);
            MUL32HIHALF (gIn, g->mat[4], prod1);
            MUL32HIHALF (bIn, g->mat[5], prod2);
            gOut = (prod0 + prod1 + prod2 + (1<<6)) >> 7;
            if (gOut > 0x00ffff) gOut = 0x00ffff; else if (gOut < 0) gOut = 0;
            *pwOut++ = gOut;

            MUL32HIHALF (rIn, g->mat[6], prod0);
            MUL32HIHALF (gIn, g->mat[7], prod1);
            MUL32HIHALF (bIn, g->mat[8], prod2);
            bOut = (prod0 + prod1 + prod2 + (1<<6)) >> 7;
            if (bOut > 0x00ffff) bOut = 0x00ffff; else if (bOut < 0) bOut = 0;
            *pwOut++ = bOut;
        }
    }

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
 * mat_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * mat_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_newPage (
    IP_XFORM_HANDLE hXform)
{
    PMAT_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * mat_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD mat_closeXform (IP_XFORM_HANDLE hXform)
{
    PMAT_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * matTbl - Jump-table for transform driver
 *
\*****************************************************************************/
#ifdef EXPORT_TRANSFORM
__declspec (dllexport)
#endif
IP_XFORM_TBL matrixTbl = {
    mat_openXform,
    mat_setDefaultInputTraits,
    mat_setXformSpec,
    mat_getHeaderBufSize,
    mat_getActualTraits,
    mat_getActualBufSizes,
    mat_convert,
    mat_newPage,
    mat_insertedData,
    mat_closeXform
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

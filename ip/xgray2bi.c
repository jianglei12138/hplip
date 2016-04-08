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
 * xgray2bi.c - Error-diffuser and thresholder that's designed to be fast
 *
 ******************************************************************************
 *
 * Jan  1998 Mark Overton -- ported code into an xform driver
 * June 1995 Mark Overton -- developed and benchmarked algorithm
 *
 * Name of Global Jump-Table:
 *
 *    gray2biTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_GRAY_2_BI_THRESHOLD] = Threshold.
 *        If threshold is zero, error-diffusion is done.
 *        If non-zero, it is the black/white threshold:
 *        A gray pixel >= to threshold becomes white, else black.
 *
 * Capabilities and Limitations:
 *
 *    Inputs rows of 8-bit gray pixels, and outputs rows of bi-level pixels.
 *    The formats are the standard raw formats described in hpojip.h.
 *    The error-diffusion weights are perturbed by small internally-generated
 *    amounts to break up any pixel patterns that would otherwise occur.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 8            1
 *    iComponentsPerPixel   * must be 1            1
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Error-diffusion timings (seconds for one page):
 *
 * 386-33 486-33 486-66 P-90  735  image dim               task
 * ------ ------ ------ ---- ----- --------- ---------------------------------
 *  21.6    9.0    4.4  2.3   1.4  1728x2200  200x200 fax in photo mode
 *  43.    17.9    8.9  4.6   2.9  2400x3150  300x300 local copy, 1/4" margins
 *  86.    36.    17.9  9.3   5.7  4800x3150  600x300 local copy, 1/4" margins
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
    DWORD    dwRowsDone;      /* number of rows converted so far */
    BYTE     bThreshold;      /* white/black threshold; 0 -> diffuse */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
    short   *pErrBuf;         /* error-term buffer */
} G2B_INST, *PG2B_INST;



/****************************************************************************\
 *
 * thresholdRow - work-horse thresholding routine
 *
\****************************************************************************/

static void thresholdRow (
    int   iPixelsPerRow, /* in:  # of pixels in the row */
    BYTE  bThreshold,    /* in:  white/black threshold value */
    BYTE  baInBuf  [],   /* in:  input pixels (0=black, 255=white) */
    BYTE  baOutBuf [])   /* out: output pixels (0=white, 1=black, 8 per byte) */
{
    int   iMore;
    PBYTE pbIn, pbOut;
    BYTE  bOut, bMask;

    pbIn  = baInBuf;
    pbOut = baOutBuf;

    for (iMore=iPixelsPerRow; iMore>0; iMore-=8)
    {
        bOut = 0;

        for (bMask=0x80u; bMask!=0; bMask>>=1)
        {
            if (*pbIn++ < bThreshold)
                bOut |= bMask;
        }
        
        *pbOut++ = bOut;
    }
}



/****************************************************************************\
 *
 * diffuseRow - work-horse error-diffusion routine
 *
\****************************************************************************/

static void diffuseRow (
    int   iPixelsPerRow, /* in:  # of pixels in the row */
    short iaErrBuf [],   /* in/out: error-term buffer from prior call */
    BYTE  baInBuf  [],   /* in:  input pixels (0=black, 255=white) */
    BYTE  baOutBuf [])   /* out: output pixels (0=white, 1=black, 8 per byte) */
{
    int     rr, r, cur;
    int     br, b, bl, bll;
    int     err;
    int     weight4, weight2, weight1;
    short   *eptr;
    int     noise;
    BYTE    *inptr, *inafter;
    BYTE    *outptr;
    BYTE    outvalue;
    int     mask;
    BOOL    second;   /* computing 2nd nibble in byte? */

    /* The diffusion algorithm below uses the following 6 weights:
     *
     *         X 4 2
     *     1 3 4 2
     *
     * These weights add to 16, so there are shifts by 4 in the code.
     * weight4 is the 4 above.  Likewise with weight2 and weight1.
     * eptr is positioned at the 1 above.
     * The macro reads the error at the top 4 above, and writes it at the 1.
     * bll is positioned at the 1 above. bleft at the 3. below at the low 4.
     * The noise perturbations add to zero, so no net noise is injected.
     */
#if 0
    #define DIFFUSE(par_outmask) {                                         \
        mask = cur >> (8*sizeof(cur)-1);   /* signed shift */              \
        outvalue |= par_outmask & mask;                                    \
        weight4 = (cur - (~mask&0x0ff0)) >> 2;                             \
                                                                           \
        noise = (cur & (0x7f<<1)) - 0x7f;                                  \
        cur     = ((unsigned)(*inptr++) << 4)                              \
                   + eptr[3] + weight2 + weight4  + noise    ;             \
        weight2    = (weight4>>1);                                         \
        weight1    = (weight2>>1);                                         \
        *eptr++ = bll        + weight1            - noise    ;             \
        bll     = bleft      + weight2 + weight1  /* + noise */ ;          \
        bleft   = below      + weight4            - noise    ;             \
        below   =              weight2            + noise    ;             \
    }

    cur      = iaErrBuf[2] + ((unsigned)(*inptr++) << 4);
    bll      = 0;
    bleft    = 0;
    below    = 0;
    weight2  = 0;

#endif

    #define DIFFUSE(par_outmask) {                                          \
        /* decide if output pixel is black or white */                      \
        mask = (cur-0x800) >> (8*sizeof(cur)-1); /* all 0's or all 1's */   \
        outvalue |= par_outmask & mask;                                     \
                                                                            \
        /* compute error, and weights of 4/16, 2/16 and 1/16 */             \
        err = cur - (~mask&0x0ff0);                                         \
        /* multiply error by 15/16 so it won't propagate a long distance */ \
        err = err - (err>>4);                                               \
        weight4 = err >> 2;                                                 \
        weight2 = weight4 >> 1;                                             \
        weight1 = weight2 >> 1;                                             \
                                                                            \
        /* distribute error to neighboring pixels */                        \
        noise = err & 0x00ff;                                               \
        rr  += weight2;                                                     \
        r   += weight4 + noise;                                             \
        br   = weight2 + noise;                                             \
        b   += weight4 - noise;                                             \
        bl  += weight2 + weight1;   /* the 3 weight */                      \
        bll += weight1 - noise;                                             \
                                                                            \
        /* advance right one pixel, so move values left one pixel */        \
        cur      = r;                                                       \
        r        = rr;                                                      \
        rr       = ((unsigned)(*inptr++) << 4) + eptr[2];                   \
        eptr[-2] = bll;                                                     \
        bll      = bl;                                                      \
        bl       = b;                                                       \
        b        = br;                                                      \
        eptr += 1;                                                          \
    }

    inptr    = baInBuf;
    inafter  = baInBuf + iPixelsPerRow;
    outptr   = baOutBuf;
    eptr     = iaErrBuf + 2;
    outvalue = 0;
    second   = FALSE;

    cur = ((unsigned)inptr[0] << 4) + eptr[0];
    r   = ((unsigned)inptr[1] << 4) + eptr[1];
    rr  = ((unsigned)inptr[2] << 4) + eptr[2];
    inptr += 3;
    bll = bl = b = br = 0;

    while (inptr < inafter) {
        DIFFUSE (0x08);
        DIFFUSE (0x04);
        DIFFUSE (0x02);
        DIFFUSE (0x01);

        if (! second) {
            /* we just computed the left half of the byte */
            outvalue <<= 4;
            second = TRUE;
        } else {
            /* we just computed the right half of the byte, so store it */
            *outptr++ = outvalue;
            outvalue = 0;
            second = FALSE;
        }
    } /* end of for */

    if (second)
        *outptr = outvalue;    /* store final nibble */

    eptr[-2] = bll;
    eptr[-1] = bl;
    eptr[ 0] = b;

    #undef DIFFUSE
}



/*****************************************************************************\
 *
 * gray2bi_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD gray2bi_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PG2B_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(G2B_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(G2B_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gray2bi_setDefaultInputTraits - Specifies default input image traits
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

static WORD gray2bi_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PG2B_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (pTraits->iBitsPerPixel == 8);
    INSURE (pTraits->iComponentsPerPixel == 1);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->inTraits = *pTraits;   /* a structure copy */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gray2bi_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD gray2bi_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PG2B_INST g;

    HANDLE_TO_PTR (hXform, g);
    INSURE ((DWORD)aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword <= 255);
    g->bThreshold = (BYTE)aXformInfo[IP_GRAY_2_BI_THRESHOLD].dword;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gray2bi_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD gray2bi_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * gray2bi_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD gray2bi_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PG2B_INST g;
    int       nBytes;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pInTraits  = g->inTraits;
    *pOutTraits = g->inTraits;
    pOutTraits->iBitsPerPixel = 1; /* this xform only changes bits/pixel */

    if (g->bThreshold == 0) {
        nBytes = sizeof(short) * g->inTraits.iPixelsPerRow;
        IP_MEM_ALLOC (nBytes, g->pErrBuf);
        memset (g->pErrBuf, 0, nBytes);
    }

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * gray2bi_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD gray2bi_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PG2B_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  =  g->inTraits.iPixelsPerRow;
    *pdwMinOutBufLen = (g->inTraits.iPixelsPerRow + 7) / 8;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gray2bi_convert - error-diffuses one row
 *
\*****************************************************************************/

static WORD gray2bi_convert (
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
    PG2B_INST g;
    int       inBytes, outBytes;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("gray2bi_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Convert and Output a Row ****/

    inBytes  = g->inTraits.iPixelsPerRow;
    outBytes = (inBytes + 7) / 8;
    INSURE (dwInputAvail  >= (DWORD)inBytes );
    INSURE (dwOutputAvail >= (DWORD)outBytes);

    if (g->bThreshold == 0) {
        INSURE (g->pErrBuf != NULL);
        diffuseRow (inBytes, g->pErrBuf, pbInputBuf, pbOutputBuf);
    } else
        thresholdRow (inBytes, g->bThreshold, pbInputBuf, pbOutputBuf);

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
 * gray2bi_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD gray2bi_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * gray2bi_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD gray2bi_newPage (
    IP_XFORM_HANDLE hXform)
{
    PG2B_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * gray2bi_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD gray2bi_closeXform (IP_XFORM_HANDLE hXform)
{
    PG2B_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->pErrBuf != NULL)
        IP_MEM_FREE (g->pErrBuf);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gray2biTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL gray2biTbl = {
    gray2bi_openXform,
    gray2bi_setDefaultInputTraits,
    gray2bi_setXformSpec,
    gray2bi_getHeaderBufSize,
    gray2bi_getActualTraits,
    gray2bi_getActualBufSizes,
    gray2bi_convert,
    gray2bi_newPage,
    gray2bi_insertedData,
    gray2bi_closeXform
};

/* End of File */

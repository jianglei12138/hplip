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
 * xtonemap.c - Performs a 256-byte tonemap to grayscale or color data
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    tonemapTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_TONEMAP_POINTER] = pointer to the 256-byte tonemap.
 *    aXformInfo[IP_TONEMAP_LUM_SPACE] = Is color-space luminance-based?
 *          0: color space is RGB
 *          1: color space has luminance as first byte (eg, YCC)
 *
 *    The tonemap is indexed by luminance (0..255), and returns the new
 *    luminance.  For color data, the luminance is computed based on r-g-b,
 *    and these are updated based on th new luminance.
 *
 *    An internal copy is made of the tonemap.
 *
 * Capabilities and Limitations:
 *
 *    The incoming data can be 8-bit gray or 24-bit color or 48-bit color.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Jan 2000 Mark Overton -- wrote original code
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
    IP_IMAGE_TRAITS traits;   /* traits of the input and output image */
    BOOL     bLumSpace;       /* luminance-based color space? (else RGB) */
    BYTE     tonemap[256];    /* the tonemap */
    DWORD    dwBytesPerRow;   /* # of bytes in each row */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
} TMAP_INST, *PTMAP_INST;



/*****************************************************************************\
 *
 * tonemap_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD tonemap_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PTMAP_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(TMAP_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(TMAP_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tonemap_setDefaultInputTraits - Specifies default input image traits
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

static WORD tonemap_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PTMAP_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow>0 && pTraits->iBitsPerPixel>1);
    g->traits = *pTraits;   /* a structure copy */
    g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tonemap_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD tonemap_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PTMAP_INST g;

    HANDLE_TO_PTR (hXform, g);
    memcpy (g->tonemap, aXformInfo[IP_TONEMAP_POINTER].pvoid, 256);
    g->bLumSpace = aXformInfo[IP_TONEMAP_LUM_SPACE].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tonemap_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD tonemap_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * tonemap_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD tonemap_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PTMAP_INST g;

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
 * tonemap_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD tonemap_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PTMAP_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tonemap_convert - Converts one row
 *
\*****************************************************************************/

static WORD tonemap_convert (
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
    PTMAP_INST g;
    int       nBytes;
    PBYTE     pIn, pOut, pOutAfter;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("tonemap_convert: Told to flush.\n"), 0, 0);
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
    pOutAfter = pOut + nBytes;

    if (g->traits.iBitsPerPixel == 8) {
        /* easiest case */
        while (pOut < pOutAfter) {
            *pOut++ = g->tonemap[*pIn++];
        }
    } else if (g->traits.iBitsPerPixel == 24) {
        if (g->bLumSpace) {
            /* easy case: 24-bit color in a luminance space */
            while (pOut < pOutAfter) {
                *pOut = g->tonemap[*pIn];
                pIn  += 3;
                pOut += 3;
            }
        } else {
            /* 24-bit color in RGB */
            int rv, gv, bv;
            int l_old, l_new, dl;

            while (pOut < pOutAfter) {
                rv = *pIn++;
                gv = *pIn++;
                bv = *pIn++;

                l_old = NTSC_LUMINANCE (rv, gv, bv);
                l_new = g->tonemap[l_old];  /* new luminance */
                dl = l_new - l_old;
                rv += dl;
                gv += dl;
                bv += dl;

                if (rv > 255) rv = 255; else if (rv < 0) rv = 0;
                if (gv > 255) gv = 255; else if (gv < 0) gv = 0;
                if (bv > 255) bv = 255; else if (bv < 0) bv = 0;

                *pOut++ = (BYTE)rv;
                *pOut++ = (BYTE)gv;
                *pOut++ = (BYTE)bv;
            }
        }
    } else {   /* 48 bits/pixel */
        PWORD src      = (PWORD)pIn;
        PWORD dst      = (PWORD)pOut;
        PWORD dstAfter = (PWORD)pOutAfter;

        int rv, gv, bv;
        int l_old, l_old8, l_new1, l_new2, l_new, dl;

        while (dst < dstAfter) {
            rv = *src++;
            gv = *src++;
            bv = *src++;

            /* use linear interpolation between tonemap entries
             * to compute new luminance (l_new) */
            l_old  = g->bLumSpace ? rv : NTSC_LUMINANCE (rv, gv, bv);
            l_old8 = l_old >> 8;
            l_new1 = g->tonemap[l_old8];
            l_new2 = l_old8<255 ? g->tonemap[l_old8+1] : l_new1;
            l_new  = (l_new2-l_new1)*(l_old&0x00ff) + (l_new1<<8);

            dl = l_new - l_old;
            rv += dl;
            if (rv > 65535) rv = 65535; else if (rv < 0) rv = 0;

            if (! g->bLumSpace) {
                gv += dl;
                bv += dl;
                if (gv > 65535) gv = 65535; else if (gv < 0) gv = 0;
                if (bv > 65535) bv = 65535; else if (bv < 0) bv = 0;
            }

            *dst++ = (WORD)rv;
            *dst++ = (WORD)gv;
            *dst++ = (WORD)bv;
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
 * tonemap_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD tonemap_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * tonemap_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD tonemap_newPage (
    IP_XFORM_HANDLE hXform)
{
    PTMAP_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * tonemap_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD tonemap_closeXform (IP_XFORM_HANDLE hXform)
{
    PTMAP_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tonemapTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL tonemapTbl = {
    tonemap_openXform,
    tonemap_setDefaultInputTraits,
    tonemap_setXformSpec,
    tonemap_getHeaderBufSize,
    tonemap_getActualTraits,
    tonemap_getActualBufSizes,
    tonemap_convert,
    tonemap_newPage,
    tonemap_insertedData,
    tonemap_closeXform
};

/* End of File */

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
 * xchangeBPP.c - Changes bits per pixel
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    changeBPPTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_CHANGE_BPP_OUTPUT_BPP] = bits/pixel to output
 *
 * Capabilities and Limitations:
 *
 *    1 bpp is assumed to be bilevel (0=white, 1=black).
 *    8 and 16 bpp are assumed to be grayscale.
 *    24 and 48 bpp are assumed to be 3-component color (rgb is assumed when
 *    we convert from color into grayscale).
 *    Among the above supported bpp values, any bpp can be changed into any
 *    other bpp.  Changing into bi-level (bpp=1) performs simple thresholding.
 *    Use xgray2bi if you want error-diffusion.
 *    If the input and output bpp values are the same, this xform merely passes
 *    the pixels through unexamined.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * anything             changed as specified
 *    iComponentsPerPixel   * 1 or 3               can be changed to 3
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Mar 1998 Mark Overton -- wrote code (for 1 -> 8/24 only)
 * Apr 2000 Mark Overton -- generalized as a change-BPP xform
 *
\******************************************************************************/

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include "assert.h"


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
    WORD     wOutBitsPerPixel; /* bits/pixel to output */
    DWORD    dwInRowBytes;     /* # bytes per input row */
    DWORD    dwOutRowBytes;    /* # bytes per output row */
    DWORD    dwInNextPos;      /* file pos for subsequent input */
    DWORD    dwOutNextPos;     /* file pos for subsequent output */
    DWORD    dwValidChk;       /* struct validity check value */
} CBPP_INST, *PCBPP_INST;



/*****************************************************************************\
 *
 * changeBPP_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD changeBPP_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PCBPP_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(CBPP_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(CBPP_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * changeBPP_setDefaultInputTraits - Specifies default input image traits
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

static WORD changeBPP_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PCBPP_INST g;
    int        bpp;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    bpp = pTraits->iBitsPerPixel;
    INSURE (bpp==1 || bpp==8 || bpp==16 || bpp==24 || bpp==48);
    INSURE (pTraits->iComponentsPerPixel==1 || pTraits->iComponentsPerPixel==3);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * changeBPP_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD changeBPP_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PCBPP_INST g;
    UINT       nBits;

    HANDLE_TO_PTR (hXform, g);
    nBits = aXformInfo[IP_CHANGE_BPP_OUTPUT_BPP].dword;
    g->wOutBitsPerPixel = (WORD)nBits;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * changeBPP_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD changeBPP_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * changeBPP_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD changeBPP_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PCBPP_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pInTraits  = g->inTraits;   /* structure copies */
    *pOutTraits = g->inTraits;
    pOutTraits->iBitsPerPixel       = g->wOutBitsPerPixel;
    pOutTraits->iComponentsPerPixel = g->wOutBitsPerPixel<24 ? 1 : 3;

    g->dwInRowBytes  = (g->inTraits.iPixelsPerRow*g->inTraits.iBitsPerPixel + 7) / 8;
    g->dwOutRowBytes = (g->inTraits.iPixelsPerRow*g->wOutBitsPerPixel       + 7) / 8;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * changeBPP_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD changeBPP_getActualBufSizes (
    IP_XFORM_HANDLE hXform,            /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PCBPP_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->dwInRowBytes;
    *pdwMinOutBufLen = g->dwOutRowBytes;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * changeBPP_convert - Converts one row
 *
\*****************************************************************************/

static WORD changeBPP_convert (
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
    PCBPP_INST g;
    PBYTE     pIn, pOut, pInAfter;
    unsigned  rv, gv, bv;
    BYTE      bMask, bBilevel;
    BYTE      bPixels;
    WORD      wPixels;
    DWORD     dwPixels;

    #define OUTPUT_THRESHOLDED_BIT(grayParam) {        \
        int gray = grayParam;                        \
        if (gray < 128) bBilevel |= bMask;            \
        bMask >>= 1;                                \
        if (bMask == 0) {                            \
            *pOut++ = (BYTE)bBilevel;                \
            bBilevel = 0;                            \
            bMask = (BYTE)0x80;                        \
        }                                            \
    }

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("changeBPP_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed     = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    INSURE (dwInputAvail  >= g->dwInRowBytes );
    INSURE (dwOutputAvail >= g->dwOutRowBytes);

    bMask = 0x80;
    bBilevel = 0;

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pInAfter = pIn + g->dwInRowBytes;

    if (g->inTraits.iBitsPerPixel == g->wOutBitsPerPixel) {
        /* no change in bpp; just copy the buffer */
        memcpy (pOut, pIn, g->dwInRowBytes);
    } else if (g->inTraits.iBitsPerPixel == 1) {
        while (pIn < pInAfter) {
            bBilevel = *pIn++;

            if (g->wOutBitsPerPixel == 48) {
                for (bMask=0x80u; bMask!=0; bMask>>=1) {
                    dwPixels = (bMask & bBilevel)
                                 ? 0            /* black */
                                 : 0xffffffff;  /* white */
                    *(DWORD*)pOut    = dwPixels;
                    *(WORD*)(pOut+4) = (WORD)dwPixels;
                    pOut += 6;
                }
            } else if (g->wOutBitsPerPixel == 24) {
                for (bMask=0x80u; bMask!=0; bMask>>=1) {
                    *(DWORD*)pOut = (bMask & bBilevel)
                                 ? 0            /* black */
                                 : 0xffffffff;  /* white */
                    pOut += 3;
                }
            } else if (g->wOutBitsPerPixel == 16) {
                for (bMask=0x80u; bMask!=0; bMask>>=1) {
                    *(WORD*)pOut = (bMask & bBilevel)
                                 ? 0        /* black */
                                 : 0xffff;  /* white */
                    pOut += 2;
                }
            } else if (g->wOutBitsPerPixel == 8) {
                for (bMask=0x80u; bMask!=0; bMask>>=1) {
                    *pOut++ = (bMask & bBilevel)
                                 ? 0      /* black */
                                 : 0xff;  /* white */
                }
            } else
                assert (0);
        }
    }
    else if (g->inTraits.iBitsPerPixel == 8) {
        if (g->wOutBitsPerPixel == 48) {
            while (pIn < pInAfter) {
                wPixels = (*pIn++ << 8);
                *(WORD*)(pOut+0) = wPixels;
                *(WORD*)(pOut+2) = wPixels;
                *(WORD*)(pOut+4) = wPixels;
                pOut += 6;
            }
        } else if (g->wOutBitsPerPixel == 24) {
            while (pIn < pInAfter) {
                bPixels = *pIn++;
                *pOut++ = bPixels;
                *pOut++ = bPixels;
                *pOut++ = bPixels;
            }
        } else if (g->wOutBitsPerPixel == 16) {
            while (pIn < pInAfter) {
                wPixels = (*pIn++ << 8);
                *(WORD*)pOut = wPixels;
                pOut += 2;
            }
        } else if (g->wOutBitsPerPixel == 1) {
            while (pIn < pInAfter) {
                OUTPUT_THRESHOLDED_BIT (*pIn);
                pIn++;
            }
        } else
            assert (0);
    }
    else if (g->inTraits.iBitsPerPixel == 16) {
        if (g->wOutBitsPerPixel == 48) {
            while (pIn < pInAfter) {
                wPixels = *(WORD*)pIn;
                *(WORD*)(pOut+0) = wPixels;
                *(WORD*)(pOut+2) = wPixels;
                *(WORD*)(pOut+4) = wPixels;
                pIn  += 2;
                pOut += 6;
            }
        } else if (g->wOutBitsPerPixel == 24) {
            while (pIn < pInAfter) {
                bPixels = (*(WORD*)pIn) >> 8;
                pIn += 2;
                *pOut++ = bPixels;
                *pOut++ = bPixels;
                *pOut++ = bPixels;
            }
        } else if (g->wOutBitsPerPixel == 8) {
            while (pIn < pInAfter) {
                *pOut++ = (*(WORD*)pIn) >> 8;
                pIn += 2;
            }
        } else if (g->wOutBitsPerPixel == 1) {
            while (pIn < pInAfter) {
                OUTPUT_THRESHOLDED_BIT ((*(WORD*)pIn) >> 8);
                pIn += 2;
            }
        } else
            assert (0);
    }
    else if (g->inTraits.iBitsPerPixel == 24) {
        if (g->wOutBitsPerPixel == 48) {
            while (pIn < pInAfter) {
                *(WORD*)(pOut+0) = (WORD)(*pIn++) << 8;
                *(WORD*)(pOut+2) = (WORD)(*pIn++) << 8;
                *(WORD*)(pOut+4) = (WORD)(*pIn++) << 8;
                pOut += 6;
            }
        } else if (g->wOutBitsPerPixel == 16) {
            /* converting rgb color (24) to grayscale (16) */
            while (pIn < pInAfter) {
                rv = (*pIn++) << 8;
                gv = (*pIn++) << 8;
                bv = (*pIn++) << 8;
                *(WORD*)pOut = NTSC_LUMINANCE (rv, gv, bv);
                pOut += 2;
            }
        } else if (g->wOutBitsPerPixel == 8) {
            /* converting rgb color (24) to grayscale (8) */
            while (pIn < pInAfter) {
                rv = *pIn++;
                gv = *pIn++;
                bv = *pIn++;
                *pOut++ = NTSC_LUMINANCE (rv, gv, bv);
            }
        } else if (g->wOutBitsPerPixel == 1) {
            /* converting rgb color (24) to bi-level */
            while (pIn < pInAfter) {
                rv = *pIn++;
                gv = *pIn++;
                bv = *pIn++;
                OUTPUT_THRESHOLDED_BIT (NTSC_LUMINANCE(rv,gv,bv));
            }
        } else
            assert (0);
    }
    else if (g->inTraits.iBitsPerPixel == 48) {
        if (g->wOutBitsPerPixel == 24) {
            while (pIn < pInAfter) {
                *pOut++ = ((WORD*)pIn)[0] >> 8;
                *pOut++ = ((WORD*)pIn)[1] >> 8;
                *pOut++ = ((WORD*)pIn)[2] >> 8;
                pIn += 6;
            }
        } else if (g->wOutBitsPerPixel == 16) {
            /* converting rgb color (48) to grayscale (16) */
            while (pIn < pInAfter) {
                rv = ((WORD*)pIn)[0];
                gv = ((WORD*)pIn)[1];
                bv = ((WORD*)pIn)[2];
                *(WORD*)pOut = NTSC_LUMINANCE (rv, gv, bv);
                pIn  += 6;
                pOut += 2;
            }
        } else if (g->wOutBitsPerPixel == 8) {
            /* converting rgb color (48) to grayscale (8) */
            while (pIn < pInAfter) {
                rv = ((WORD*)pIn)[0];
                gv = ((WORD*)pIn)[1];
                bv = ((WORD*)pIn)[2];
                *pOut++ = NTSC_LUMINANCE (rv, gv, bv) >> 8;
                pIn += 6;
            }
        } else if (g->wOutBitsPerPixel == 1) {
            /* converting rgb color (48) to bi-level */
            while (pIn < pInAfter) {
                rv = ((WORD*)pIn)[0];
                gv = ((WORD*)pIn)[1];
                bv = ((WORD*)pIn)[2];
                OUTPUT_THRESHOLDED_BIT (NTSC_LUMINANCE(rv,gv,bv) >> 8);
                pIn += 6;
            }
        } else
            assert (0);
    }

    if (g->inTraits.iBitsPerPixel>1 && g->wOutBitsPerPixel==1 && bMask!=(BYTE)0x80)
        *pOut = bBilevel;   /* output any partially-filled byte */

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
 * changeBPP_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD changeBPP_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * changeBPP_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD changeBPP_newPage (
    IP_XFORM_HANDLE hXform)
{
    PCBPP_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * changeBPP_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD changeBPP_closeXform (IP_XFORM_HANDLE hXform)
{
    PCBPP_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * changeBPPTbl - Jump-table for xform
 *
\*****************************************************************************/

IP_XFORM_TBL changeBPPTbl = {
    changeBPP_openXform,
    changeBPP_setDefaultInputTraits,
    changeBPP_setXformSpec,
    changeBPP_getHeaderBufSize,
    changeBPP_getActualTraits,
    changeBPP_getActualBufSizes,
    changeBPP_convert,
    changeBPP_newPage,
    changeBPP_insertedData,
    changeBPP_closeXform
};

/* End of File */

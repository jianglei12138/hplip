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
 * xrotate.c - Rotates the image
 *
 * Mark Overton, Feb 2000
 *
 ******************************************************************************
 *
 * Imagine a rotated rectangle contained somewhere within the input image, or
 * even partially outside the input image.  This xform outputs that rectangle
 * unrotated.  This xform crops out all image-area outside the rotated rectangle,
 * and white-fills any part of the rectangle outside the image-area.  It is
 * intended for procuring a selected portion of a scan.
 *
 * A rotated rectangle is specified using three points in the input image.
 * Depending on the orientation of these points, horizontal and/or vertical
 * flipping can be performed, as well as rotation by any angle.
 *
 * Scaling is also performed.  The interpolation for rotation also does the
 * scaling in the same step, resulting in higher quality output than the usual
 * use of separate rotation and scaling, as well as being faster.
 *
 * Rotation can be fast or slow, depending on an aXformInfo flag.  If slow,
 * interpolation (anti-aliasing) is done with superb results.  If fast, the
 * results have jaggies.
 *
 * Name of Global Jump-Table:
 *
 *    rotateTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *      aXformInfo[IP_ROTATE_UPPER_LEFT]  = [xUL,yUL] = pos of upper-left corner of rotated rect
 *      aXformInfo[IP_ROTATE_UPPER_RIGHT] = [xUR,yUR] = pos of upper-right corner of rotated rect
 *      aXformInfo[IP_ROTATE_LOWER_LEFT]  = [xLL,yLL] = pos of lower-left corner of rotated rect
 *      aXformInfo[IP_ROTATE_OUTPUT_SIZE] = [outWidth,outHeight] = size of output image (0 = no scaling)
 *      aXformInfo[IP_ROTATE_FAST]        = rotate fast?  (0=no=interpolated, 1=yes=jaggy)
 *
 *    The coordinates (xUL,yUL) are in relation to the *input* image (Y increases
 *    downward), and locates what will be the upper-left corner in the output
 *    (before scaling).  The other two points are defined similarly.  If UL is to
 *    the right of UR, the output image is flipped horizontally.  Vertical flipping
 *    can be done similarly.
 *    This rectangle is then scaled to produce outWidth and outHeight pixels.
 *
 * Capabilities and Limitations:
 *
 *    Crops, white-pads, rotates by any angle, scales, mirrors, and flips vertically.
 *    Input data may be 1 bit/pixel, or any multiple of 8 bits/pixel.
 *    Internally, the miniumum amount of memory required for the rotation is
 *    allocated.  For small angles, internal allocation will be very small.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * width of scan        abs(outWidth)
 *    iBitsPerPixel         * 1, or n*8            same as default input
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows              * height of scan       abs(outHeight)
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
\******************************************************************************/

/* Use the #define below if this transform will exist in a dll outside of the */
/* image pipeline.  This will allow the functions to be exported.             */
/* #define EXPORT_TRANFORM 1                                                  */

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include "math.h"      /* for sqrt */


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
    IP_IMAGE_TRAITS inTraits;    /* traits of the input image */
    IP_IMAGE_TRAITS outTraits;   /* traits of the output image */

    // Items from aXformInfo:
    int      xUL, yUL;           /* UL corner of image */
    int      xUR, yUR;           /* UR corner of image */
    int      xLL, yLL;           /* LL corner of image */
    int      xLR, yLR;           /* LR corner of image (computed) */
    int      iOutWidth;          /* width of output image */
    int      iOutHeight;         /* height of output image */
    BOOL     bRotateFast;        /* rotate fast? (produces worse jaggies) */

    int      hSlopeDx, hSlopeDy; /* input-change for moving right 1 pix in output (16.16) */
    int      vSlopeDx, vSlopeDy; /* input-change for moving down 1 pix in output (16.16) */
    int      xLeft, yLeft;       /* current  left-end of row (input coords) (16.16) */
    int      xRight, yRight;     /* current right-end of row (input coords) (16.16) */
    BYTE    *pStrip;             /* strip with a portion of input image */
    BYTE    *pStripAfter;        /* ptr to 1st byte after the strip-buffer */
    int      stripIndexYTop;     /* row-index of stripYTop in the strip */
    int      stripYTop;          /* Y-value for topmost row in strip */
    int      stripYBottom;       /* Y-value for bottommost row in strip */
    int      stripRows;          /* number of rows allocated in the strip */
    int      stripLoaded;        /* number of rows actually loaded into the strip */
    int      stripSize;          /* number of bytes allocated in the strip */
    int      stripBytesPerRow;   /* bytes/row in strip */
    int      inBytesPerRow;      /* bytes/row of input image */
    int      outBytesPerRow;     /* bytes/row of output image */
    int      bytesPerPixel;      /* bytes/pixel (=1 for bilevel; 1 pixel in each byte) */
    DWORD    dwRowsSent;         /* number of rows output so far */
    DWORD    dwInNextPos;        /* file pos for subsequent input */
    DWORD    dwOutNextPos;       /* file pos for subsequent output */
    DWORD    dwValidChk;         /* struct validity check value */
} ROTATE_INST, *PROTATE_INST;



/*****************************************************************************\
 *
 * rotate_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PROTATE_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(ROTATE_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(ROTATE_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotate_setDefaultInputTraits - Specifies default input image traits
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

FUNC_STATUS WORD rotate_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PROTATE_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow > 0  &&
            pTraits->lNumRows > 0  &&
            pTraits->iBitsPerPixel > 0);
    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotate_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PROTATE_INST g;
    int          i;

    HANDLE_TO_PTR (hXform, g);

    i = aXformInfo[IP_ROTATE_UPPER_LEFT].dword;
    g->xUL         = i >> 16;
    g->yUL         = i & 0xFFFF;

    i = aXformInfo[IP_ROTATE_UPPER_RIGHT].dword;
    g->xUR         = i >> 16;
    g->yUR         = i & 0xFFFF;
    
    i = aXformInfo[IP_ROTATE_LOWER_LEFT].dword;
    g->xLL         = i >> 16;
    g->yLL         = i & 0xFFFF;
    
    i = aXformInfo[IP_ROTATE_OUTPUT_SIZE].dword;
    g->iOutWidth   = i >> 16;
    g->iOutHeight  = i & 0xFFFF;

    g->bRotateFast = aXformInfo[IP_ROTATE_FAST].dword == 1;

    INSURE (g->iOutWidth>=0 && g->iOutHeight>=0);
    g->xLR = g->xUR + (g->xLL - g->xUL);
    g->yLR = g->yUR + (g->yLL - g->yUL);

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotate_getHeaderBufSize - Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * rotate_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PROTATE_INST g;
    int    dxH, dyH, dxV, dyV;
    double rotWidthSq, rotHeightSq, rotWidth, rotHeight;

    HANDLE_TO_PTR (hXform, g);

    /***** Compute Output Traits (and rotation variables) *****/

    memcpy (&g->outTraits, &g->inTraits, sizeof(IP_IMAGE_TRAITS));

    dxH = g->xUR - g->xUL;
    dyH = g->yUR - g->yUL;
    dxV = g->xLL - g->xUL;
    dyV = g->yLL - g->yUL;

    rotWidthSq  = dxH*dxH + dyH*dyH;
    rotWidth    = sqrt (rotWidthSq);
    rotHeightSq = dxV*dxV + dyV*dyV;
    rotHeight   = sqrt (rotHeightSq);
    INSURE (rotWidth>0.0 && rotHeight>0.0);

    if (g->iOutWidth == 0)
        g->iOutWidth = (int)(rotWidth+0.5);
    if (g->iOutHeight == 0)
        g->iOutHeight = (int)(rotHeight+0.5);

    g->outTraits.iPixelsPerRow = g->iOutWidth;
    g->outTraits.lNumRows      = g->iOutHeight;

    g->bytesPerPixel = g->inTraits.iBitsPerPixel / 8;
    if (g->bytesPerPixel == 0)
        g->bytesPerPixel = 1;   /* bi-level is expanded to one byte/pixel */

    g->inBytesPerRow  = (g->inTraits.iPixelsPerRow*g->inTraits.iBitsPerPixel + 7) / 8;
    g->outBytesPerRow = (g->iOutWidth             *g->inTraits.iBitsPerPixel + 7) / 8;

    g->hSlopeDx = (dxH << 16) / g->iOutWidth;
    g->hSlopeDy = (dyH << 16) / g->iOutWidth;
    g->vSlopeDx = (dxV << 16) / g->iOutHeight;
    g->vSlopeDy = (dyV << 16) / g->iOutHeight;

    /* we start outputting at the upper-left corner of output-image */
    /* do NOT add 0x8000 because when we're not scaling or rotating,
     * it causes each pair of rows and each pair of columns to be
     * averaged together, losing sharpness. */
    g->xLeft  = g->xUL << 16;
    g->yLeft  = g->yUL << 16;
    g->xRight = g->xUR << 16;
    g->yRight = g->yUR << 16;

    /* set-up the strip-buffer */
    g->stripBytesPerRow = g->inTraits.iPixelsPerRow * g->bytesPerPixel;
    g->stripIndexYTop   = 0;
    g->stripYTop        = 0;
    g->stripYBottom     = -1;
    g->stripLoaded      = 0;
    if (g->vSlopeDy < 0)   /* we'll proceed *upward* thru the input image */
        g->stripRows = IP_MAX(g->yUL, g->yUR) - IP_MIN(g->yLL, g->yLR) + 1;
    else   /* normal case of proceeding *downward* */
        g->stripRows = IP_MAX(g->yUL, g->yUR) - IP_MIN(g->yUL, g->yUR) + 1;
    g->stripRows += 2;  /* allocate extra row on top & bottom for interpolation */
    g->stripSize = g->stripRows * g->stripBytesPerRow;
    IP_MEM_ALLOC (g->stripSize, g->pStrip);
    g->pStripAfter = g->pStrip + g->stripSize;

    /***** Return Values *****/

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    *pIntraits  = g->inTraits;   /* structure copies */
    *pOutTraits = g->outTraits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * rotate_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD rotate_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PROTATE_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen  = g->inBytesPerRow;
    *pdwMinOutBufLen = g->outBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotate_convert - Converts one row
 *
\*****************************************************************************/



/* ExpandBilevelRow - Expands 8 bits/pixel (input) to 1 byte/pixel (output) */
static void ExpandBilevelRow (
    PBYTE pDest,
    PBYTE pSrc,
    int   nPixels)
{
    BYTE mask, inbyte=0;

    mask = 0;

    while (nPixels > 0) {
        if (mask == 0) {
            mask = 0x80u;
            inbyte = *pSrc++;
        }
        *pDest++ = inbyte & mask ? 0 : 255;
        mask >>= 1;
        nPixels -= 1;
    }
}



FUNC_STATUS WORD rotate_convert (
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
    // We need another row if the Y of either endpoint of the current rotated row
    // is below the bottom of the strip.
    #define NEED_MORE \
        ((g->yLeft >>16) >= g->stripYBottom  ||  (g->yRight>>16) >= g->stripYBottom)

    PROTATE_INST g;
    PBYTE        pSrc, pDest;

    HANDLE_TO_PTR (hXform, g);

    *pdwInputUsed = *pdwOutputUsed = 0;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwOutputThisPos = g->dwOutNextPos;

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("rotate_convert: Told to flush.\n"), 0, 0);
        dwInputAvail = (DWORD)g->inBytesPerRow;
        if ((long)g->dwRowsSent >= g->outTraits.lNumRows)
            return IP_DONE;
    }

    /**** Initial load of strip-buffer ****/

    if (g->stripLoaded < g->stripRows) {
        if (dwInputAvail == 0)
            return IP_READY_FOR_DATA;
        INSURE (dwInputAvail >= (DWORD)g->inBytesPerRow);
        pDest = g->pStrip + g->stripBytesPerRow*g->stripLoaded;
        if (g->inTraits.iBitsPerPixel == 1)
            ExpandBilevelRow (pDest, pbInputBuf, g->inTraits.iPixelsPerRow);
        else if (pbInputBuf == NULL)
            memcpy (pDest, pDest - g->stripBytesPerRow, g->stripBytesPerRow);
        else
            memcpy (pDest, pbInputBuf, g->stripBytesPerRow);
        g->stripLoaded += 1;
        g->stripYBottom += 1;
        if (g->stripLoaded == 1) {
            /* first row is *above* top for interpolation; replicate it */
            memcpy (g->pStrip+g->stripBytesPerRow, g->pStrip, g->stripBytesPerRow);
            g->stripLoaded += 1;
            g->stripYBottom += 1;
        }
        if (pbInputBuf == NULL)
            return 0;   /* wait for next call to replicate another row and start rotation */
        *pdwInputUsed   = (DWORD)g->inBytesPerRow;
        g->dwInNextPos += (DWORD)g->inBytesPerRow;
        *pdwInputNextPos = g->dwInNextPos;
        return ((g->stripLoaded<g->stripRows || NEED_MORE) ? IP_READY_FOR_DATA : 0)
               | IP_CONSUMED_ROW;
    }

    /**** Load next row into strip-buffer ****/

    if (NEED_MORE)
    {
        /* we need to load a row into the strip-buffer (a wrapping load) */
        if (dwInputAvail == 0)
            return IP_READY_FOR_DATA;
        INSURE (dwInputAvail >= (DWORD)g->inBytesPerRow);
        pDest = g->pStrip + g->stripBytesPerRow*g->stripIndexYTop;
        if (pbInputBuf == NULL)  // then white-fill the row
            memset (pDest, 255, g->stripBytesPerRow);
        else if (g->inTraits.iBitsPerPixel == 1)
            ExpandBilevelRow (pDest, pbInputBuf, g->inTraits.iPixelsPerRow);
        else
            memcpy (pDest, pbInputBuf, g->stripBytesPerRow);
        g->stripIndexYTop = (g->stripIndexYTop + 1) % g->stripRows;
        g->stripYTop    += 1;
        g->stripYBottom += 1;
        if (pbInputBuf != NULL) {
            *pdwInputUsed   = (DWORD)g->inBytesPerRow;
            g->dwInNextPos += (DWORD)g->inBytesPerRow;
            *pdwInputNextPos = g->dwInNextPos;
        }
    }

    /**** Output a row ****/

    if ((long)g->dwRowsSent >= g->outTraits.lNumRows)
    {
        /* we've outputted all rows; discard any further input */
        g->stripYBottom = -100000;   /* forces strip-loading logic to keep loading */
    }
    else if (! NEED_MORE)
    {
        int      rowIndex, iPix;
        int      xcur,ycur, xint,yint;
        unsigned xfrac,yfrac;
        PBYTE    pNextRow;
        BYTE     mask;

        INSURE (dwOutputAvail >= (DWORD)g->outBytesPerRow);

        pDest = pbOutputBuf;
        xcur = g->xLeft;
        ycur = g->yLeft;

        /* These two lines set-up bi-level packing: */
        *pDest = 0;
        mask = 0x80u;

        for (iPix=0; iPix<g->outTraits.iPixelsPerRow; iPix++)
        {
            xint =  xcur >> 16;
            yint = (ycur >> 16) - g->stripYTop;

            /* below, unsigned makes negatives huge (therefore not in strip) */
            /* also, we check stripRows-1; the -1 allows the extra interpolation-row */
            if ((unsigned)yint < (unsigned)(g->stripRows-1)  &&
                (unsigned)xint < (unsigned)g->inTraits.iPixelsPerRow)
            {
                /**** We are inside the strip ****/

                /* Compute address of pixel in the strip */
                rowIndex = yint + g->stripIndexYTop;
                if (rowIndex >= g->stripRows)
                    rowIndex -= g->stripRows;
                pSrc = (rowIndex*g->inTraits.iPixelsPerRow + xint) * g->bytesPerPixel
                       + g->pStrip;

                /**** Output a pixel ****/

                /* The byte at ptrSrcCur is the upper-left byte out of a 2x2 group.
                 * These four bytes are interpolated together based on xfrac and yfrac.
                 */
                #define INTERPOLATE_BYTE(ptrOut,ptrSrcCur,ptrSrcNex,bpp) { \
                    int xtop, xbot, val; \
                    xtop = (ptrSrcCur)[0]*(256-xfrac) + (ptrSrcCur)[bpp]*xfrac; \
                    xbot = (ptrSrcNex)[0]*(256-xfrac) + (ptrSrcNex)[bpp]*xfrac; \
                    val = xtop*(256-yfrac) + xbot*yfrac; \
                    val = (val + 0x8000) >> 16;  /* the add rounds result of shift */ \
                    *(ptrOut) = (BYTE)val; \
                }

                #define INTERPOLATE_WORD(ptrOut,ptrSrcCur,ptrSrcNex,bpp) { \
                    WORD *wptrOut    = (PWORD)(ptrOut   ); \
                    WORD *wptrSrcCur = (PWORD)(ptrSrcCur); \
                    WORD *wptrSrcNex = (PWORD)(ptrSrcNex); \
                    unsigned xtop, xbot, val; \
                    xtop = ((wptrSrcCur)[0]*(0x10000-xfrac)>>8) + ((wptrSrcCur)[bpp]*xfrac>>8); \
                    xtop = (xtop+128) >> 8; /* the add rounds result of shift */ \
                    xbot = ((wptrSrcNex)[0]*(0x10000-xfrac)>>8) + ((wptrSrcNex)[bpp]*xfrac>>8); \
                    xbot = (xbot+128) >> 8; \
                    val = (xtop*(0x10000-yfrac)>>8) + (xbot*yfrac>>8); \
                    val = (val+128) >> 8; \
                    *(wptrOut) = (WORD)val; \
                }

                pNextRow = pSrc + g->stripBytesPerRow;
                if (pNextRow >= g->pStripAfter)
                    pNextRow -= g->stripSize;  /* next row wrapped to top of strip */

                xfrac = (xcur>>8) & 0x0000ff;
                yfrac = (ycur>>8) & 0x0000ff;

                if (g->bytesPerPixel == 3) {
                    if (g->bRotateFast) {
                        pDest[0] = pSrc[0];
                        pDest[1] = pSrc[1];
                        pDest[2] = pSrc[2];
                    } else {
                        /* interpolate to eliminate jaggies */
                        INTERPOLATE_BYTE (pDest+0, pSrc+0, pNextRow+0, 3)
                        INTERPOLATE_BYTE (pDest+1, pSrc+1, pNextRow+1, 3)
                        INTERPOLATE_BYTE (pDest+2, pSrc+2, pNextRow+2, 3)
                    }
                    pDest += 3;
                } else if (g->bytesPerPixel == 1) {
                    BYTE byt;

                    if (g->bRotateFast)
                        byt = pSrc[0];
                    else
                        INTERPOLATE_BYTE (&byt, pSrc, pNextRow, 1)

                    if (g->inTraits.iBitsPerPixel == 8)
                        *pDest++ = byt;
                    else {
                        /* bi-level: pack 8 pixels per byte */
                        if (byt < 128)
                            *pDest |= mask;
                        mask >>= 1;
                        if (mask == 0) {
                            mask = 0x80u;
                            pDest += 1;
                            *pDest = 0;
                        }
                    }
                } else if (g->bytesPerPixel==2 || g->bytesPerPixel==6) {
                    /* 16-bit grayscale or 48-bit color */
                    if (g->bRotateFast) {
                        memcpy (pDest, pSrc, g->bytesPerPixel);
                    } else {
                        xfrac = xcur & 0x00ffff;
                        yfrac = ycur & 0x00ffff;
                        INTERPOLATE_WORD (pDest+0, pSrc+0, pNextRow+0, g->inTraits.iComponentsPerPixel)
                        if (g->bytesPerPixel == 6) {
                            INTERPOLATE_WORD (pDest+2, pSrc+2, pNextRow+2, 3)
                            INTERPOLATE_WORD (pDest+4, pSrc+4, pNextRow+4, 3)
                        }
                    }

                    pDest += g->bytesPerPixel;
                } else {
                    /* unsupported bits per pixel */
                    INSURE (FALSE);
                }
            }
            else   /* current pos is outside strip, so output a white pixel (padding) */
            {
                if (g->inTraits.iBitsPerPixel == 1) {
                    mask >>= 1;
                    if (mask == 0) {
                        mask = 0x80u;
                        pDest += 1;
                        *pDest = 0;
                    }
                } else {
                    memset (pDest, 255, g->bytesPerPixel);
                    pDest += g->bytesPerPixel;
                }
            }

            /* Advance to next pixel in input image */
            xcur += g->hSlopeDx;
            ycur += g->hSlopeDy;
        }  /* end of for-each-pixel loop */

        *pdwOutputUsed    = (DWORD)g->outBytesPerRow;
        *pdwOutputThisPos = g->dwOutNextPos;
        g->dwOutNextPos  += (DWORD)g->outBytesPerRow;

        g->dwRowsSent += 1;

        g->xLeft  += g->vSlopeDx;
        g->yLeft  += g->vSlopeDy;
        g->xRight += g->vSlopeDx;
        g->yRight += g->vSlopeDy;
    } 

    return (*pdwInputUsed !=0 ? IP_CONSUMED_ROW   : 0) |
           (*pdwOutputUsed!=0 ? IP_PRODUCED_ROW   : 0) |
           (NEED_MORE         ? IP_READY_FOR_DATA : 0);

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotate_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * rotate_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_newPage (
    IP_XFORM_HANDLE hXform)
{
    PROTATE_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * rotate_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD rotate_closeXform (IP_XFORM_HANDLE hXform)
{
    PROTATE_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->pStrip != NULL)
        IP_MEM_FREE (g->pStrip);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * rotateTbl - Jump-table for transform driver
 *
\*****************************************************************************/

#ifdef EXPORT_TRANSFORM
__declspec (dllexport)
#endif
IP_XFORM_TBL rotateTbl = {
    rotate_openXform,
    rotate_setDefaultInputTraits,
    rotate_setXformSpec,
    rotate_getHeaderBufSize,
    rotate_getActualTraits,
    rotate_getActualBufSizes,
    rotate_convert,
    rotate_newPage,
    rotate_insertedData,
    rotate_closeXform
};



/*****************************************************************************\
 *
 * ipGetXformTable - Returns pointer to the jump table
 *
\*****************************************************************************/

#ifdef EXPORT_TRANSFORM
EXPORT(WORD) rotateGetXformTable (LPIP_XFORM_TBL pXform)
{
    if (pXform == NULL)
        return IP_FATAL_ERROR;

    *pXform = clrmapTbl;
    return IP_DONE;
}
#endif

/* End of File */

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

/*****************************************************************************\
 *
 * xpcx.c - encoder and decoder for PCX files for Image Processor
 *
 *****************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    pcxEncodeTbl = the encoder,
 *    pcxDecodeTbl = the decoder.
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    none
 *
 * Capabilities and Limitations:
 *
 *    Handles 1 or 4 bits per pixel.
 *    Decoder discards the palette in the PCX file (todo: keep it).
 *    Encoder assumes input is internal raw format, which means that
 *    1 and 4 bits/pixel are assumed to be grayscales, where
 *    1 bit/pixel is [0=white, 1=black], 4 bits/pixel is [0=black, 15=white].
 *    Encoder outputs a palette for the preceding gray-ranges.
 *    If # rows is not known in input traits (negative), the encoder will
 *    re-write the header when # rows is known at the end of the page. So
 *    the output file will have a valid row-count when conversion is done.
 *
 * Default Input Traits, and Output Traits:
 *
 *    For decoder:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow           ignored              based on header
 *    iBitsPerPixel           ignored              based on header (1 or 4)
 *    iComponentsPerPixel     ignored              1
 *    lHorizDPI               ignored              based on header
 *    lVertDPI                ignored              based on header
 *    lNumRows                ignored              based on header
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    For encoder:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 1 or 4       same as default input
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Mark Overton, Feb 1998
 *
\*****************************************************************************/

/* todo: this PCX code encodes/decodes 4-bit gray as one pixel per byte.
 *       it should be two pixels per byte.
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

#define CHECK_VALUE 0x1ce5ca7e

/* TODO: Make this work for big and little endian. */
#define LITTLE_ENDIAN  (! (defined SNAKES))


/* PCX_INST - our instance variables */

typedef struct {
    IP_IMAGE_TRAITS traits;  /* traits of the image */
    DWORD  dwInNextPos;      /* file pos for subsequent input */
    DWORD  dwOutNextPos;     /* file pos for subsequent output */
    BOOL   fDidHeader;       /* already processed the header? */
    BYTE  *pPlanes;          /* buffer containing separated planes */
    UINT   uBytesPerRawRow;  /* number of bytes per unencoded row */
    UINT   uBytesPerPlane;   /* number of bytes per plane per row */
    UINT   uRowsDone;        /* number of rows converted so far */
    DWORD  dwValidChk;       /* struct validity check value */
} PCX_INST, *PPCX_INST;



/*____________________________________________________________________________
 |              |                                                             |
 | pcx_header_t | Each PCX file starts with this 128-byte header              |
 |______________|_____________________________________________________________|
*/
typedef struct {
    BYTE PcxId;           /* must be 0Ah, which means PC Paintbrush */
    BYTE Version;         /* 2=v2.8 w/ palette; 3=v2.8 w/o pal; 5=v3 w/ pal */
    BYTE EncodingMethod;  /* encoding method == 1 */
    BYTE BitsPerPixel;    /* = 1 for fax mode transfers */
    WORD XMin;            /* X position of the upper left corner */
    WORD YMin;            /* Y position of the upper left corner */
    WORD XMax;            /* X position of the bottom right corner */
    WORD YMax;            /* Y position of the bottom right corner */
    WORD HorizResolution; /* horizontal resolution */
    WORD VertResolution;  /* vertical resolution */
    BYTE PaletteInfo[48]; /* Palette information */
    BYTE Reserved1;       /* reserved, must be zero */
    BYTE ColorPlanes;     /* number of color planes == 1 */
    WORD BytesPerPlane;   /* bytes per plane per row */
    BYTE Reserved2[60];   /* reserved, should be zero */
} pcx_header_t;

#define PCX_ID           0x0A
#define PCX_VERSION      2
#define PCX_HEADER_SIZE  128


/*____________________________________________________________________________
 |                   |                                                        |
 | swap_header_bytes | if big-endian machine, swaps bytes in WORD items       |
 |___________________|________________________________________________________|
*/
static void swap_header_bytes (pcx_header_t *head_p)
{
#if ! LITTLE_ENDIAN
    #define SWAP_IT(item)  \
        head_p->item = (head_p->item << 8) | (head_p->item >> 8)

    SWAP_IT (XMin);
    SWAP_IT (YMin);
    SWAP_IT (XMax);
    SWAP_IT (YMax);
    SWAP_IT (HorizResolution);
    SWAP_IT (VertResolution);
    SWAP_IT (BytesPerPlane);
#endif
}



/*____________________________________________________________________________
 |               |                                                            |
 | encode_buffer | encodes inbuf (separated planes) into run-length PCX data  |
 |_______________|____________________________________________________________|
*/
static UINT encode_buffer (    /* ret-val is # bytes written to outbuf */
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to inbuf having separated planes */
    BYTE     *outbuf_p)        /* in:  ptr to outbuf to get PCX run-lens */
{
    BYTE *out_p;
    BYTE *beg_run_p;
    BYTE *aft_run_p;
    BYTE *inbuf_aft_p;
    BYTE  byt;
    UINT  run_len;

    out_p = outbuf_p;
    beg_run_p = inbuf_p;
    inbuf_aft_p = inbuf_p + g->uBytesPerPlane * g->traits.iBitsPerPixel;

    while (beg_run_p < inbuf_aft_p)
    {
        byt = *beg_run_p;

        for (aft_run_p = beg_run_p+1;
             aft_run_p<inbuf_aft_p && *aft_run_p==byt;
             aft_run_p++) ;

        run_len = aft_run_p - beg_run_p;
        if (run_len > 63) {
            run_len = 63;
            aft_run_p = beg_run_p + 63;
        }

        if (run_len>1 || byt>=(BYTE)0xc0)
            *out_p++ = (BYTE)run_len | (BYTE)0xc0;
        *out_p++ = byt;

        beg_run_p = aft_run_p;
    }

    return out_p - outbuf_p;
}



/*____________________________________________________________________________
 |               |                                                            |
 | decode_buffer | decodes PCX run-length data into separated planes          |
 |_______________|____________________________________________________________|
*/
static UINT decode_buffer (    /* ret-val is # bytes read from inbuf */
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    BYTE *in_p;
    BYTE *out_p;
    BYTE *out_aft_p;
    UINT  run_len;
    BYTE  byt;

    in_p = inbuf_p;
    out_p = outbuf_p;
    out_aft_p = outbuf_p + g->uBytesPerPlane*g->traits.iBitsPerPixel;

    while (out_p < out_aft_p)
    {
        byt = *in_p++;

        if (byt < (BYTE)0xc0)
            *out_p++ = byt;
        else {
            run_len = byt & (BYTE)0x3f;
            if (run_len > (UINT) (out_aft_p - out_p))
                run_len = out_aft_p-out_p;   /* run went past end of outbuf */
            memset (out_p, *in_p++, run_len);
            out_p += run_len;
        }
    }

    return in_p - inbuf_p;
}



/*____________________________________________________________________________
 |                                                                            |
 |                     Encoding/Decoding Bilevel                              |
 |____________________________________________________________________________|
*/



/*____________________________________________________________________________
 |             |                                                              |
 | flip_pixels | in PCX, 0=black and 1=white, so this flips all the bits      |
 |_____________|______________________________________________________________|
*/
static void flip_pixels (
    PCX_INST *g,            /* our instance variables      */
    BYTE     *byte_buf_p)   /* ptr to buffer to be flipped */
{
    ULONG *buf_p;
    ULONG *buf_aft_p;

    buf_p = (ULONG*)byte_buf_p;
    buf_aft_p = buf_p + ((g->uBytesPerRawRow+3) >> 2);
    for ( ; buf_p<buf_aft_p; buf_p++)
        *buf_p = ~ *buf_p;
}



static UINT encode_1 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    UINT n_bytes;

    flip_pixels (g, inbuf_p);
    n_bytes = encode_buffer (g, inbuf_p, outbuf_p);
    flip_pixels (g, inbuf_p);
    return n_bytes;
}



static UINT decode_1 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    UINT used;

    used = decode_buffer (g, inbuf_p, outbuf_p);
    flip_pixels (g, outbuf_p);
    return used;
}



/*____________________________________________________________________________
 |                                                                            |
 |                     Encoding/Decoding 16-Level                             |
 |____________________________________________________________________________|
*/


#if 0

/*____________________________________________________________________________
 |            |                                                               |
 | pcxtable.c | Outputs table for PCX's 16-level decoder to stdout            |
 |____________|_______________________________________________________________|
*/

#include <stdio.h>
#define little_endian 1

void main (void)
{
    unsigned nib;
    long unsigned outlong;

    for (nib=0; nib<=15; nib++) {
        outlong = 0;
        if (nib & 0x08) outlong |= 0x10000000;
        if (nib & 0x04) outlong |= 0x00100000;
        if (nib & 0x02) outlong |= 0x00001000;
        if (nib & 0x01) outlong |= 0x00000010;

        if (little_endian)
            outlong = (outlong & 0xff000000) >> 24  |
                      (outlong & 0x00ff0000) >>  8  |
                      (outlong & 0x0000ff00) <<  8  |
                      (outlong & 0x000000ff) << 24;

        _tprintf (_T("0x%08x, "), outlong);
        if ((nib%4) == 3) _tputs(_T(""));
    }
    _tputs (_T(""));
}

#endif



static UINT encode_4 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    #if LITTLE_ENDIAN
        #define NIB_0 0x000000f0
        #define NIB_1 0x0000f000
        #define NIB_2 0x00f00000
        #define NIB_3 0xf0000000
    #else
        #define NIB_0 0xf0000000
        #define NIB_1 0x00f00000
        #define NIB_2 0x0000f000
        #define NIB_3 0x000000f0
    #endif

    ULONG *in_p;
    ULONG *in_aft_p;
    BYTE  *plane_p;
    ULONG  quad;
    ULONG  mask;
    BYTE   byt;

    /* Separate the four planes (into pPlanes) */

    in_aft_p = (ULONG *) (inbuf_p + g->uBytesPerRawRow);
    plane_p = g->pPlanes;
    mask = 0x10101010;

    while (TRUE)
    {
        for (in_p=(ULONG*)inbuf_p; in_p<in_aft_p; ) {
            byt = 0;
            quad = *in_p++ & mask;
            if (quad & NIB_0) byt  = 0x80;
            if (quad & NIB_1) byt |= 0x40;
            if (quad & NIB_2) byt |= 0x20;
            if (quad & NIB_3) byt |= 0x10;

            quad = *in_p++ & mask;
            if (quad & NIB_0) byt |= 0x08;
            if (quad & NIB_1) byt |= 0x04;
            if (quad & NIB_2) byt |= 0x02;
            if (quad & NIB_3) byt |= 0x01;
            *plane_p++ = byt;
        }

        if (mask == 0x80808080)
            break;
        mask <<= 1;
    }

    return encode_buffer (g, g->pPlanes, outbuf_p);
}



static UINT decode_4 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    #if LITTLE_ENDIAN
        static const ULONG unscramble[16] = {
            0x00000000, 0x10000000, 0x00100000, 0x10100000,
            0x00001000, 0x10001000, 0x00101000, 0x10101000,
            0x00000010, 0x10000010, 0x00100010, 0x10100010,
            0x00001010, 0x10001010, 0x00101010, 0x10101010,
        };
    #else
        static const ULONG unscramble[16] = {
            0x00000000, 0x00000010, 0x00001000, 0x00001010,
            0x00100000, 0x00100010, 0x00101000, 0x00101010,
            0x10000000, 0x10000010, 0x10001000, 0x10001010,
            0x10100000, 0x10100010, 0x10101000, 0x10101010,
        };
    #endif

    UINT    used;
    BYTE  *plane_p;
    BYTE  *plane_aft_p;
    ULONG *out_p;

    used = decode_buffer (g, inbuf_p, g->pPlanes);

    /* Combine the separated planes (in pPlanes) back into pixels */

    plane_p = g->pPlanes;

    out_p = (ULONG*)outbuf_p;
    plane_aft_p = plane_p + g->uBytesPerPlane;
    for (; plane_p<plane_aft_p; plane_p++) {
        *out_p++ = unscramble[*plane_p >> 4];
        *out_p++ = unscramble[*plane_p & 15];
    }

    out_p = (ULONG*)outbuf_p;
    plane_aft_p = plane_p + g->uBytesPerPlane;
    for (; plane_p<plane_aft_p; plane_p++) {
        *out_p++ |= unscramble[*plane_p >> 4] << 1;
        *out_p++ |= unscramble[*plane_p & 15] << 1;
    }

    out_p = (ULONG*)outbuf_p;
    plane_aft_p = plane_p + g->uBytesPerPlane;
    for (; plane_p<plane_aft_p; plane_p++) {
        *out_p++ |= unscramble[*plane_p >> 4] << 2;
        *out_p++ |= unscramble[*plane_p & 15] << 2;
    }

    out_p = (ULONG*)outbuf_p;
    plane_aft_p = plane_p + g->uBytesPerPlane;
    for (; plane_p<plane_aft_p; plane_p++) {
        *out_p++ |= unscramble[*plane_p >> 4] << 3;
        *out_p++ |= unscramble[*plane_p & 15] << 3;
    }

    return used;
}



/*____________________________________________________________________________
 |                                                                            |
 |                     Encoding/Decoding 256-Level                            |
 |____________________________________________________________________________|
*/


#if 0

static UINT encode_8 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    assert (0);
    return 0;
}



static UINT decode_8 (
    PCX_INST *g,               /* in:  our instance variables */
    BYTE     *inbuf_p,         /* in:  ptr to input buffer    */
    BYTE     *outbuf_p)        /* in:  ptr to output buffer   */
{
    assert (0);
    return 0;
}

#endif   /* 256-level stuff */



/*
******************************************************************************
******************************************************************************
**
**
**                              E N C O D E R
**
**
******************************************************************************
******************************************************************************
*/



/*****************************************************************************\
 *
 * pcxEncode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD pcxEncode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PPCX_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(PCX_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(PCX_INST));
    g->dwValidChk = CHECK_VALUE;
    INSURE (sizeof(pcx_header_t) == PCX_HEADER_SIZE);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncode_setDefaultInputTraits - Specifies default input image traits
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

static WORD pcxEncode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    INSURE (pTraits->iBitsPerPixel==1 || pTraits->iBitsPerPixel==4);
    INSURE (pTraits->iPixelsPerRow > 0);
    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD pcxEncode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* do nothing, because we don't have any xform-specific info */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD pcxEncode_getHeaderBufSize (
    IP_XFORM_HANDLE   hXform,         /* in:  handle for xform */
    DWORD            *pdwInBufLen)    /* out: buf size for parsing header */
{
    *pdwInBufLen = 0;   /* no header on raw input data */
    return IP_DONE;
}



/*****************************************************************************\
 *
 * pcxEncode_getActualTraits - Parses header, and returns input & output traits
 *
 *****************************************************************************
 *
 * If depth is 1, a bilevel PCX will be encoded/decoded from/to a
 * RASTER_BITMAP image.
 *
 * If depth is 4, a 16-level PCX will be encoded/decoded from/to gray data.
 * The 4-bit gray value is in the upper nibble of each byte in the raw gray
 * data.
 *
 * If depth is 8, a 256-level PCX will be encoded/decoded from/to gray data.
 *
 * If depth is 24, the PCX will be encoded/decoded from/to RGB data.
 *
\*****************************************************************************/

static WORD pcxEncode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no input header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Since we don't change traits, just copy out the default traits */
    *pInTraits  = g->traits;
    *pOutTraits = g->traits;

      /***************************/
     /* Process the traits info */
    /***************************/

    g->uBytesPerPlane = (g->traits.iPixelsPerRow + 7) / 8;
    g->uBytesPerRawRow = g->traits.iBitsPerPixel == 1
                             ? g->uBytesPerPlane
                             : g->traits.iPixelsPerRow;

    PRINT (_T("pcx_encode_output_header: pixels/row=%d, n_rows=%d\n"),
        g->traits.iPixelsPerRow, g->traits.lNumRows);
    PRINT (_T("pcx_encode_output_header: depth=%d, uBytesPerRawRow=%d\n"),
        g->traits.iBitsPerPixel, g->uBytesPerRawRow);

    if (g->traits.iBitsPerPixel > 1)
        IP_MEM_ALLOC (g->uBytesPerPlane*g->traits.iBitsPerPixel, g->pPlanes);

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * pcxEncode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD pcxEncode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,           /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->uBytesPerRawRow;
    *pdwMinOutBufLen = g->traits.iBitsPerPixel * g->uBytesPerPlane * 2;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * outputHeader - Only called by pcxEncode_convert
 *
\****************************************************************************/

static WORD outputHeader (
    PPCX_INST g,                /* in:  ptr to instance structure */
    DWORD     dwOutputAvail,    /* in:  # avail bytes in out-buf */
    PBYTE     pbOutputBuf,      /* in:  ptr to out-buffer */
    PDWORD    pdwOutputUsed,    /* out: # bytes written in out-buf */
    PDWORD    pdwOutputThisPos) /* out: file-pos to write the data */
{
    pcx_header_t    *pPCXHeader;
    PIP_IMAGE_TRAITS pTr;
    BYTE            *pal_p;
    UINT             i;

    *pdwOutputThisPos = 0;
    *pdwOutputUsed  = PCX_HEADER_SIZE;
    g->dwOutNextPos = PCX_HEADER_SIZE;
    INSURE (dwOutputAvail >= PCX_HEADER_SIZE);

    pTr = &g->traits;
    pPCXHeader = (pcx_header_t *)pbOutputBuf;

    pPCXHeader->PcxId           = PCX_ID;
    pPCXHeader->Version         = PCX_VERSION;
    pPCXHeader->EncodingMethod  = 1;
    pPCXHeader->BitsPerPixel    = 1;
    pPCXHeader->XMin            = 0;
    pPCXHeader->YMin            = 0;
    pPCXHeader->XMax            = pTr->iPixelsPerRow - 1;
    pPCXHeader->YMax            = pTr->lNumRows<=0 ? 0 : pTr->lNumRows-1;
    pPCXHeader->HorizResolution = (USHORT)(pTr->lHorizDPI>>16); /* todo: use width   */
    pPCXHeader->VertResolution  = (USHORT)(pTr->lVertDPI >>16); /*       and height? */
    pPCXHeader->Reserved1       = 0;
    pPCXHeader->ColorPlanes     = pTr->iBitsPerPixel;
    pPCXHeader->BytesPerPlane   = g->uBytesPerPlane;

    memset (pPCXHeader->Reserved2, 0, 60);

    pal_p = pPCXHeader->PaletteInfo;
    if (pTr->iBitsPerPixel == 1) {
        memset (pal_p, 0, 48);
        pal_p[3] = pal_p[4] = pal_p[5] = 255;    /* 0=black, 1=white */
    } else {
        /* set the palette to a black-to-white gray ramp */
        for (i=0; i<=15; i++) {
            *pal_p++ = i << 4;  /* red   */
            *pal_p++ = i << 4;  /* green */
            *pal_p++ = i << 4;  /* blue  */
        }
    }

    swap_header_bytes (pPCXHeader);
    return IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD pcxEncode_convert (
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
    PPCX_INST g;
    UINT      out_used = 0;  /* init to zap stupid compiler warning */

    HANDLE_TO_PTR (hXform, g);

    /**** Output the Header if we haven't already ****/

    if (! g->fDidHeader) {
        g->fDidHeader = TRUE;
        *pdwInputUsed    = 0;
        *pdwInputNextPos = 0;
        return outputHeader (g, dwOutputAvail, pbOutputBuf,
                             pdwOutputUsed, pdwOutputThisPos);
    }

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("pcx_encode_convert_row: Told to flush.\n"), 0, 0);
        if (g->traits.lNumRows < 0) {
            /* # rows wasn't known at first, so output header again
             * now that we know the number of rows */
            g->traits.lNumRows = g->uRowsDone;
            *pdwInputUsed    = 0;
            *pdwInputNextPos = g->dwInNextPos;
            return outputHeader (g, dwOutputAvail, pbOutputBuf,
                                 pdwOutputUsed, pdwOutputThisPos);
        }

        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    switch (g->traits.iBitsPerPixel) {
        case 1:  out_used = encode_1 (g, pbInputBuf, pbOutputBuf);
                 break;

        case 4:  out_used = encode_4 (g, pbInputBuf, pbOutputBuf);
                 break;
        #if 0
        case 8:  out_used = encode_8 (g, pbInputBuf, pbOutputBuf);
                 break;
        #endif
    }

    INSURE (dwInputAvail  >= g->uBytesPerRawRow);
    INSURE (dwOutputAvail >= out_used);

    g->dwInNextPos   += g->uBytesPerRawRow;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwInputUsed     = g->uBytesPerRawRow;
    *pdwOutputUsed    = out_used;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += out_used;

    g->uRowsDone += 1;

    PRINT (_T("pcx_encode_convert_row: Returning, out used = %d\n"), out_used, 0);
    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD pcxEncode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * pcxEncode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD pcxEncode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * pcxEncode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD pcxEncode_closeXform (IP_XFORM_HANDLE hXform)
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    if (g->pPlanes != NULL)
        IP_MEM_FREE (g->pPlanes);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxEncodeTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL pcxEncodeTbl = {
    pcxEncode_openXform,
    pcxEncode_setDefaultInputTraits,
    pcxEncode_setXformSpec,
    pcxEncode_getHeaderBufSize,
    pcxEncode_getActualTraits,
    pcxEncode_getActualBufSizes,
    pcxEncode_convert,
    pcxEncode_newPage,
    pcxEncode_insertedData,
    pcxEncode_closeXform
};



/*
******************************************************************************
******************************************************************************
**
**
**                              D E C O D E R
**
**
******************************************************************************
******************************************************************************
*/



/*****************************************************************************\
 *
 * pcxDecode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD pcxDecode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    return pcxEncode_openXform(pXform);   /* allocs & zeroes a new instance */
}



/*****************************************************************************\
 *
 * pcxDecode_setDefaultInputTraits - Specifies default input image traits
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

static WORD pcxDecode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* the PCX header will overwrite most items in traits below */
    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxDecode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD pcxDecode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* do nothing, because we don't have any xform-specific info */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxDecode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD pcxDecode_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    *pdwInBufLen = PCX_HEADER_SIZE;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * pcxDecode_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD pcxDecode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PPCX_INST         g;
    pcx_header_t     *pcxhead_p;
    PIP_IMAGE_TRAITS  pTr;
    WORD              ret_val;

    ret_val = IP_DONE | IP_READY_FOR_DATA;
    HANDLE_TO_PTR (hXform, g);

    INSURE (dwInputAvail >= PCX_HEADER_SIZE);
    *pdwInputUsed        = PCX_HEADER_SIZE;
    *pdwInputNextPos     = PCX_HEADER_SIZE;
    g->dwInNextPos       = PCX_HEADER_SIZE;

    pcxhead_p = (pcx_header_t*)pbInputBuf;
    pTr = &g->traits;

    swap_header_bytes (pcxhead_p);
        pTr->lNumRows            = pcxhead_p->YMax - pcxhead_p->YMin + 1;
        pTr->iPixelsPerRow       = pcxhead_p->XMax - pcxhead_p->XMin + 1;
        pTr->iBitsPerPixel       = pcxhead_p->ColorPlanes;
        pTr->lHorizDPI           = (ULONG)pcxhead_p->HorizResolution << 16;
        pTr->lVertDPI            = (ULONG)pcxhead_p->VertResolution  << 16;
        pTr->iComponentsPerPixel = 1;
        g->uBytesPerPlane        = pcxhead_p->BytesPerPlane;
    swap_header_bytes (pcxhead_p);

    g->uBytesPerRawRow = pTr->iBitsPerPixel == 1
                             ? g->uBytesPerPlane
                             : g->traits.iPixelsPerRow;

    if (! (pcxhead_p->PcxId == PCX_ID
        && pTr->iPixelsPerRow > 1
        && g->uBytesPerPlane == ((UINT)pTr->iPixelsPerRow+7)/8
        && (pTr->iBitsPerPixel==1 || pTr->iBitsPerPixel==4)))
        ret_val |= IP_INPUT_ERROR;

    if (pTr->iBitsPerPixel > 1)
        IP_MEM_ALLOC (g->uBytesPerPlane*pTr->iBitsPerPixel, g->pPlanes);

    if (pTr->lNumRows <= 1)
        pTr->lNumRows = -1; /* both YMax and YMin were 0; # rows is unknown */

    *pInTraits = *pOutTraits = g->traits;   /* structure copy */

    PRINT (_T("pcx_decode_parse_header: depth=%d, n_rows=%d\n"),
           g->traits.iBitsPerPixel, g->traits.lNumRows);

    return ret_val;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * pcxDecode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD pcxDecode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,           /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->traits.iBitsPerPixel * g->uBytesPerPlane * 2;
    *pdwMinOutBufLen = g->uBytesPerRawRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxDecode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD pcxDecode_convert (
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
    PPCX_INST g;
    DWORD     in_used=0;

    HANDLE_TO_PTR (hXform, g);

    if (pbInputBuf == NULL) {
        /* we are being told to flush */
        PRINT (_T("pcx_decode_convert_row: Told to flush; doing nothing.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    if (g->traits.lNumRows>=0 && g->uRowsDone==(UINT)g->traits.lNumRows) {
        /* discard extra data after final row */
        *pdwInputUsed     = dwInputAvail;
        g->dwInNextPos   += dwInputAvail;
        *pdwOutputUsed    = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return 0;
    }

    switch (g->traits.iBitsPerPixel) {
        case 1:  in_used = decode_1 (g, pbInputBuf, pbOutputBuf);
                 break;

        case 4:  in_used = decode_4 (g, pbInputBuf, pbOutputBuf);
                 break;
        #if 0
        case 8:  in_used = decode_8 (g, pbInputBuf, pbOutputBuf);
                 break;
        #endif
    }

    INSURE (dwInputAvail  >= in_used);
    INSURE (dwOutputAvail >= g->uBytesPerRawRow);

    g->dwInNextPos   += in_used;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwInputUsed     = in_used;
    *pdwOutputUsed    = g->uBytesPerRawRow;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += g->uBytesPerRawRow;

    g->uRowsDone += 1;

    PRINT (_T("pcx_decode_convert_row: Returning, in used = %d\n"), in_used, 0);
    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxDecode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD pcxDecode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * pcxDecode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD pcxDecode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PPCX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pcxDecode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD pcxDecode_closeXform (IP_XFORM_HANDLE hXform)
{
    return pcxEncode_closeXform (hXform);
}



/*****************************************************************************\
 *
 * pcxDecodeTbl - Jump-table for Decoder
 *
\*****************************************************************************/

IP_XFORM_TBL pcxDecodeTbl = {
    pcxDecode_openXform,
    pcxDecode_setDefaultInputTraits,
    pcxDecode_setXformSpec,
    pcxDecode_getHeaderBufSize,
    pcxDecode_getActualTraits,
    pcxDecode_getActualBufSizes,
    pcxDecode_convert,
    pcxDecode_newPage,
    pcxDecode_insertedData,
    pcxDecode_closeXform
};

/* End of File */

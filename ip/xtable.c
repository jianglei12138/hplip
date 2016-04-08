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
 * xtable.c - Performs a 256-entry table lookup on all bytes
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    tableTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_TABLE_WHICH] = contents of table to use:
 *          IP_TABLE_USER            = user-supplied pointer to a 256-byte table,
 *          IP_TABLE_USER_WORD       = user-supplied pointer to a 4096-word table,
 *          IP_TABLE_PASS_THRU       = pass-thru table that does nothing,
 *          IP_TABLE_GAMMA           = gamma function,
 *          IP_TABLE_THRESHOLD       = threshold (snaps each incoming byte to 0 or 255),
 *          IP_TABLE_MIRROR          = mirror-image each incoming byte,
 *          IP_TABLE_USER_THREE      = three user-supplied table pointers,
 *          IP_TABLE_USER_THREE_WORD = three user-supplied table pointers for 48-bit (see format below),
 *          IP_TABLE_BW_CLIP         = white/black clipper (2 thresholds below)
 *
 *    aXformInfo[IP_TABLE_OPTION] =
 *          parameter based on type of table to use.  values:
 *                       - (type IP_TABLE_USER) pointer to user table,
 *                         this table is copied into this xform's instance,
 *                       - (type IP_TABLE_GAMMA) gamma value, in 16.16 fixed-point, a value
 *                         less than 1.0 does an inverse gamma, (for
 *                         type IP_TABLE_GAMMA above),
 *                       - (type IP_TABLE_THRESHOLD) threshold value. if an incoming byte is >=
 *                         this value, it changes to 255, else it changes
 *                         to 0 (for type IP_TABLE_THRESHOLD above),
 *                       - (type IP_TABLE_BW_CLIP) hi word = number of 0 entries at start,
 *                                                 lo word = number of 255 entries at end,
 *
 *    For option IP_TABLE_USER_THREE above (three user-supplied table pointers),
 *    these three 256-byte tables are for 3-component color data.  The pointers are in
 *    aXformInfo[IP_TABLE_COLOR_1], aXformInfo[IP_TABLE_COLOR_2], and aXformInfo[IP_TABLE_COLOR_3].
 *
 *    For option IP_TABLE_USER_THREE_WORD (three user tables for 48-bit color data),
 *    the pointers are in aXformInfo as with IP_TABLE_USER_THREE.
 *    Each table consists of 4096 words (8192 bytes) which are indexed by the high 12 bits
 *    of each color-channel.  The low four bits are interpolated herein.  Each table-entry
 *    contains a 16-bit pixel-value, even though it's indexed by just 12 bits.
 *
 *    IP_TABLE_USER_WORD is like IP_TABLE_USER_THREE_WORD described above.
 *
 *    For option IP_TABLE_BW_CLIP, the white/black clipper, the table starts with the given
 *    number of 0's, and ends with the given number of 255's, and linearly
 *    progresses between 1 and 254 in between them.  This serves to snap
 *    almost-black to black, and almost-white to white.  If these numbers
 *    are large, this table also boosts contrast.
 *
 * Capabilities and Limitations:
 *
 *    The incoming data can be any kind of raw pixels of 1, 8, 16, 24 or 48
 *    bits/pixel.  Bi-level (1 bit/pixel) data is treated as 8 pixels per byte.
 *    All table-types support all these forms of input data. For 16 bits/channel
 *    data, the tables are interpolated.
 *
 *    For improved precision, the following define larger (12-bit index) tables:
 *           IP_TABLE_USER_WORD       - 16 bits per pixel (grayscale)
 *           IP_TABLE_USER_THREE_WORD - 48 bits per pixel (color)
 *
 *    Also, IP_TABLE_GAMMA will create a 12-bit table when the input data
 *    is 16 bits/channel.
 *
 *    12-bit tables work with 8-bit/channel data, and 8-bit tables work with
 *    16-bit/channel data.  Truncation or interpolation is done as needed.
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
 * Apr 1998 Mark Overton -- wrote original code
 *
\******************************************************************************/

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include "math.h"      /* for pow for generating gamma table */


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
    BYTE     bWhich;          /* which table to generate */
    BYTE     bTables[3][256]; /* the 8-bit look-up tables */
    WORD    *pwTables[3];     /* ptrs to 12-bit-index tables (for 16-bits/pixel) */
    BOOL     bBigTable;       /* are we using the 12-bit tables? */
    int      nTables;         /* # of tables defined (1 or 3) */
    DWORD    dwBytesPerRow;   /* # of bytes in each row */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
} TBL_INST, *PTBL_INST;


typedef enum {
    TBL_USER,
    TBL_PASS_THRU,
    TBL_GAMMA,
    TBL_THRESHOLD,
    TBL_MIRROR,
    TBL_USER_THREE,
    TBL_BW_CLIP
} TABLE_TYPE;



static BOOL generateTable (
    PTBL_INST g,
    DWORD_OR_PVOID aXformInfo[])
{
    IP_TABLE_TYPE which;
    DWORD         dwparam;
    PVOID         pvparam;
    float         flparam;
    PBYTE         pTable;
    PWORD         pwTable;
    int           nTable;

    g->nTables = 1;
    g->bBigTable = FALSE;
    pTable = g->bTables[0];
    dwparam =                aXformInfo[IP_TABLE_OPTION].dword;
    pvparam =                aXformInfo[IP_TABLE_OPTION].pvoid;
    flparam =                aXformInfo[IP_TABLE_OPTION].fl;
    which = (IP_TABLE_TYPE)aXformInfo[IP_TABLE_WHICH].dword;
    g->bWhich = (BYTE)which;

    switch (which)
    {
        case IP_TABLE_USER:
            if (pvparam == 0)
                return FALSE;
            memcpy (pTable, (PBYTE)pvparam, 256);
        break;

        case IP_TABLE_USER_WORD:
            if (pvparam == 0)
                return FALSE;
            IP_MEM_ALLOC (4097*sizeof(WORD), pwTable);  /* 4097: extra entry at end */
            g->pwTables[0] = pwTable;
            memcpy (pwTable, (PWORD)pvparam, 4096*sizeof(WORD));
            pwTable[4096] = pwTable[4095];  /* extra entry to help interpolation */
            g->bBigTable = TRUE;
        break;

        case IP_TABLE_USER_THREE:
            if (aXformInfo[IP_TABLE_COLOR_1].pvoid==0 ||
                aXformInfo[IP_TABLE_COLOR_2].pvoid==0 ||
                aXformInfo[IP_TABLE_COLOR_3].pvoid==0)
                return FALSE;
            memcpy (g->bTables[0], (PBYTE)aXformInfo[IP_TABLE_COLOR_1].pvoid, 256);
            memcpy (g->bTables[1], (PBYTE)aXformInfo[IP_TABLE_COLOR_2].pvoid, 256);
            memcpy (g->bTables[2], (PBYTE)aXformInfo[IP_TABLE_COLOR_3].pvoid, 256);
            g->nTables = 3;
        break;

        case IP_TABLE_USER_THREE_WORD:
            if (aXformInfo[IP_TABLE_COLOR_1].pvoid==0 ||
                aXformInfo[IP_TABLE_COLOR_2].pvoid==0 ||
                aXformInfo[IP_TABLE_COLOR_3].pvoid==0)
                return FALSE;

            for (nTable=0; nTable<3; nTable++) {
                IP_MEM_ALLOC (4097*sizeof(WORD), pwTable);  /* 4097: extra entry at end */
                g->pwTables[nTable] = pwTable;
                memcpy (pwTable, (PWORD)aXformInfo[IP_TABLE_COLOR_1+nTable].pvoid, 4096*sizeof(WORD));
                pwTable[4096] = pwTable[4095];  /* extra entry to help interpolation */
            }
            g->nTables = 3;
            g->bBigTable = TRUE;
        break;

        case IP_TABLE_PASS_THRU:
        {
            int i;

            for (i=0; i<=255; i++)
                pTable[i] = i;
        }
        break;

        case IP_TABLE_GAMMA:
        {
            int   index;
            float fval;
            float gamma;
            float gamval;

            gamma = (float)flparam / (float)(1L<<16);
            if (gamma<=0.0f || gamma>=10.0f)
                return FALSE;
            gamma = 1.0f / gamma;

            if (g->traits.iBitsPerPixel==16 || g->traits.iBitsPerPixel==48) {
                WORD *pwTable;
                IP_MEM_ALLOC (4097*sizeof(WORD), pwTable);  /* 4097: extra entry at end */
                g->pwTables[0] = pwTable;

                for (index=0; index<=4095u; index++) {
                    fval = (float)index / 4095.0f;
                    gamval = 65535.0f * (float)pow(fval, gamma);
                    pwTable[index] = (WORD)(gamval + 0.5f);
                }

                pwTable[4096] = pwTable[4095];  /* extra entry to help interpolation */
                g->bBigTable = TRUE;
            } else {
                for (index=0; index<=255u; index++) {
                    fval = (float)index / 255.0f;
                    gamval = 255.0f * (float)pow(fval, gamma);
                    pTable[index] = (BYTE)(gamval + 0.5f);
                }
            }
        }
        break;

        case IP_TABLE_THRESHOLD:
            if (dwparam<1 || dwparam>255)
                return FALSE;
            memset (pTable, 0, dwparam);
            memset (pTable+dwparam, 255, 256-dwparam);
        break;

        case IP_TABLE_MIRROR:
        {
            UINT index, mask;

            for (index=0; index<=255; index++) {
                for (mask=0x01u; mask<=0x80u; mask<<=1) {
                    pTable[index] <<= 1;
                    if (index & mask)
                        pTable[index] += 1;
                }
            }
        }
        break;

        case IP_TABLE_BW_CLIP:
        {
            DWORD nBlack, nWhite, nMid, slopeMid, posMid, index;

            nBlack = dwparam >> 16;
            nWhite = dwparam & 0xFFFFu;

            if (nBlack+nWhite > 256)
                return FALSE;

            for (index=0; index<nBlack; index++)
                pTable[index] = 0;

            for (index=256-nWhite; index<=255; index++)
                pTable[index] = 255;

            nMid = 256 - nBlack - nWhite;
            slopeMid = (255ul<<16) / (nMid + 1);
            posMid = 0x8000u;   /* offset for rounding */

            for (index=nBlack; index<256-nWhite; index++) {
                posMid += slopeMid;
                pTable[index] = (BYTE)(posMid >> 16);
            }
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;

fatal_error:
    return FALSE;
}



/*****************************************************************************\
 *
 * table_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD table_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PTBL_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(TBL_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(TBL_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * table_setDefaultInputTraits - Specifies default input image traits
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

static WORD table_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PTBL_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* insure that traits we care about are known */
    INSURE (pTraits->iPixelsPerRow>0 && pTraits->iBitsPerPixel>0);
    g->traits = *pTraits;   /* a structure copy */
    g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * table_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD table_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PTBL_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (! generateTable(g,aXformInfo))
        goto fatal_error;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * table_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD table_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * table_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD table_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PTBL_INST g;

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
 * table_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD table_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PTBL_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * table_convert - Converts one row
 *
\*****************************************************************************/

static WORD table_convert (
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
    PTBL_INST g;
    int       nBytes, nTable;
    PBYTE     pIn, pOut, pOutAfter;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("table_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = g->dwBytesPerRow;
    INSURE (dwInputAvail  >= (DWORD)nBytes);
    INSURE (dwOutputAvail >= (DWORD)nBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pOutAfter = pOut + nBytes;

    if (g->bWhich == IP_TABLE_PASS_THRU)
    {
        memcpy (pOut, pIn, nBytes);
    }
    else if (g->traits.iBitsPerPixel==16 || g->traits.iBitsPerPixel==48)
    {
        /* 16 bits per channel -- interpolate between table-entries */
        int   x, xHi, y1, y2;
        WORD *pwTable, *pwIn, *pwOut, *pwOutAfter;
        BYTE *pbTable;

        pwIn       = (WORD*)pIn;
        pwOut      = (WORD*)pOut;
        pwOutAfter = (WORD*)pOutAfter;

        if (g->bBigTable)
        {
            /* we're using 12-bit table(s) */
            while (pwOut < pwOutAfter) {
                for (nTable=0; nTable<g->nTables; nTable++) {
                    pwTable = g->pwTables[nTable];
                    x = (unsigned)(*pwIn++);
                    xHi = x >> 4;  /* hi 12 bits is used for indexing into the table */
                    y1 = (unsigned)pwTable[xHi  ];  /* index is in 0..4095 */
                    y2 = (unsigned)pwTable[xHi+1];  /* extra entry in table is for index=4096 */
                    /* interpolate the lowest 4 bits */
                    *pwOut++ = (WORD)(((y2-y1)*(x&0x0f)>>4) + y1);
                }
            }
        }
        else   /* we're using 8-bit table(s) */
        {
            while (pwOut < pwOutAfter) {
                for (nTable=0; nTable<g->nTables; nTable++) {
                    pbTable = g->bTables[nTable];
                    x = (unsigned)(*pwIn++);
                    xHi = x >> 8;  /* hi 8 bits is used for indexing into the table */
                    y1 = (unsigned)pbTable[xHi];  /* index is in 0..255 for both */
                    y2 = (unsigned)pbTable[xHi==255 ? 255 : xHi+1];
                    /* interpolate the lowest 8 bits */
                    *pwOut++ = (WORD)((y2-y1)*(x&0x0ff) + (y1<<8));
                }
            }
        }
    }
    else   /* 8 bits per channel -- the normal case */
    {
        if (g->bBigTable)
        {
            /* using a big table for 8- or 24-bit data, for some reason */
            while (pOut < pOutAfter)
                for (nTable=0; nTable<g->nTables; nTable++)
                    *pOut++ = (BYTE)(g->pwTables[nTable][(unsigned)(*pIn++)<<4] >> 8);
        }
        else if (g->nTables == 3)
        {
            while (pOut < pOutAfter) {
                /* process two pixels at a time for improved speed */
                pOut[0] = g->bTables[0][pIn[0]];
                pOut[1] = g->bTables[1][pIn[1]];
                pOut[2] = g->bTables[2][pIn[2]];
                pOut[3] = g->bTables[0][pIn[3]];
                pOut[4] = g->bTables[1][pIn[4]];
                pOut[5] = g->bTables[2][pIn[5]];

                pIn  += 6;
                pOut += 6;
            }
        }
        else   /* using a single table */
        {
            while (pOut < pOutAfter) {
                /* process eight pixels at a time for improved speed */
                pOut[0] = g->bTables[0][pIn[0]];
                pOut[1] = g->bTables[0][pIn[1]];
                pOut[2] = g->bTables[0][pIn[2]];
                pOut[3] = g->bTables[0][pIn[3]];
                pOut[4] = g->bTables[0][pIn[4]];
                pOut[5] = g->bTables[0][pIn[5]];
                pOut[6] = g->bTables[0][pIn[6]];
                pOut[7] = g->bTables[0][pIn[7]];

                pIn  += 8;
                pOut += 8;
            }
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
 * table_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD table_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * table_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD table_newPage (
    IP_XFORM_HANDLE hXform)
{
    PTBL_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * table_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD table_closeXform (IP_XFORM_HANDLE hXform)
{
    PTBL_INST g;
    int       i;

    HANDLE_TO_PTR (hXform, g);

    for (i=0; i<3; i++)
        if (g->pwTables[i] != NULL)
            IP_MEM_FREE (g->pwTables[i]);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tableTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL tableTbl = {
    table_openXform,
    table_setDefaultInputTraits,
    table_setXformSpec,
    table_getHeaderBufSize,
    table_getActualTraits,
    table_getActualBufSizes,
    table_convert,
    table_newPage,
    table_insertedData,
    table_closeXform
};

/* End of File */

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

/* Original author: David Paschal (based on Mark Overton's "skel" template
 * and a few bits and pieces from xjpg_{enc,dec}.c). */

/******************************************************************************\
 *
 * xjpg_fix.c - Fixes JPEG files to have a standard JFIF APP0 header and
 *              a correct row-count value in the SOF header.
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    jpgFixTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    None.
 *
 * Capabilities and Limitations:
 *
 *    Looks at the header, and converts an OfficeJet APP1 short header to
 *    a standard JFIF APP0 header.  Passes the resulting header and data
 *    through to the JPEG decoder in order to count the number of rows in
 *    the image.  Discards the decompressed data and passes the original
 *    compressed data (possibly with modified header) to the output.  At
 *    the end of the file, seeks back to the SOF header and rewrites the
 *    row count field.
 *
 *    Able to handle JPEG files with JFIF APP0 or OfficeJet APP1 headers,
 *    but not color-fax APP1 headers or Denali-style compression (in
 *    these cases, you must do the full decode, any necessary color-space
 *    conversion, and encode).
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  -------------------  ------------------------
 *    iPixelsPerRow           ignored              based on header
 *    iBitsPerPixel           ignored              based on header
 *    iComponentsPerPixel     ignored              based on header
 *    lHorizDPI               ignored              based on header
 *    lVertDPI                ignored              based on header
 *    lNumRows                ignored              based on header
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
\******************************************************************************/

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include <stdio.h>

#define PRINTF(args...) fprintf(stderr,args)

/* TODO: Move this to a separate .h file: */
extern IP_XFORM_TBL jpgDecodeTbl;
extern WORD jpgDecode_getRowCountInfo(IP_XFORM_HANDLE hXform,
    int *pRcCountup,int *pRcTraits,int *pSofOffset);

#if 0
    #include "stdio.h"
    #include <tchar.h>
    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x4ba1dace

#define FUNC_STATUS static

#define BEND_GET_SHORT(s) (((s)[0]<<8)|((s)[1]))
#define BEND_SET_SHORT(s,x) ((s)[0]=((x)>>8)&0xFF,(s)[1]=(x)&0xFF)

typedef struct {
    IP_XFORM_HANDLE pSlaveXform;   /* JPEG-decoder slave transformer. */
    PBYTE    headerBuffer;
    DWORD    outNextPos;
    DWORD    lenHeader;
    DWORD    lenAddedHeader;
    DWORD    lenHeaderBuffer;
    DWORD    readyForSofRewrite;
    DWORD    dwValidChk;      /* struct validity check value */
} JFIX_INST, *PJFIX_INST;


/* TODO: Move the tables to a separate common file. */

/* Since the firmware does not supply tables in its header,
 * the tables used in the firmware are supplied below.  */

/*____________________________________________________________________________
 |                                                                            |
 | Zigzag of Normal Quantization Tables                                       |
 |____________________________________________________________________________|
*/ 

static const unsigned char orig_lum_quant[64] = {
     16,  11,  12,  14,  12,  10,  16,  14,
     13,  14,  18,  17,  16,  19,  24,  40,
     26,  24,  22,  22,  24,  49,  35,  37,
     29,  40,  58,  51,  61,  60,  57,  51,
     56,  55,  64,  72,  92,  78,  64,  68,
     87,  69,  55,  56,  80, 109,  81,  87,
     95,  98, 103, 104, 103,  62,  77, 113,
    121, 112, 100, 120,  92, 101, 103,  99
};

static const unsigned char orig_chrom_quant[64] = {
    17,  18,  18,  24,  21,  24,  47,  26,
    26,  47,  99,  66,  56,  66,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
};

/*____________________________________________________________________________
 |                                                                            |
 | Huffman Tables                                                             |
 |____________________________________________________________________________|
*/
 
static const unsigned char lum_DC_counts[16] = {
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
 
static const unsigned char lum_DC_values[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
};
 
static const unsigned char chrom_DC_counts[16] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};
 
static const unsigned char chrom_DC_values[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
};
 
static const unsigned char lum_AC_counts[16] = {
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
    0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d
};

static const unsigned char lum_AC_values[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};
 
static const unsigned char chrom_AC_counts[16] = {
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
    0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
};

static const unsigned char chrom_AC_values[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

/*____________________________________________________________________________
 |               |                                                            |
 | scale_q_table | scales a q-table according to the q-factors                |
 |_______________|____________________________________________________________|
*/
/* TODO: Reference this from a common .h file. */
void scale_q_table(int dc_q_factor,int ac_q_factor,int ident,
  unsigned char *out) {
    static const int Q_DEFAULT=20;
    static const int FINAL_DC_INDEX=9;
    int i,val;
    int q=dc_q_factor;
    const unsigned char *in=orig_lum_quant;
    if (ident) in=orig_chrom_quant;
 
    for (i=0; i<64; i++) {
        val = ((*in++)*q + Q_DEFAULT/2) / Q_DEFAULT;
        if (val < 1)   val = 1;
        if (val > 255) val = 255;
        *out++ = (unsigned char)val;
        if (i == FINAL_DC_INDEX) {
            q = ac_q_factor;
        }
    }
}


/*****************************************************************************\
 *
 * jpgFix_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PJFIX_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(JFIX_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(JFIX_INST));
    if (jpgDecodeTbl.openXform(&g->pSlaveXform)!=IP_DONE) {
        IP_MEM_FREE(g);
        goto fatal_error;
    }
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_setDefaultInputTraits - Specifies default input image traits
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

FUNC_STATUS WORD jpgFix_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);

    return jpgDecodeTbl.setDefaultInputTraits(g->pSlaveXform,pTraits);

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Check your options in aXformInfo here, and save them.
     * Use the INSURE macro like you'd use assert.  INSURE jumps to
     * fatal_error below if it fails.
     */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    WORD r;
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);

    r=jpgDecodeTbl.getHeaderBufSize(g->pSlaveXform,pdwInBufLen);
    if (r!=IP_DONE) return r;

    g->lenHeaderBuffer=*pdwInBufLen;
    IP_MEM_ALLOC(g->lenHeaderBuffer,g->headerBuffer);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/


//#define MYLOCATE(p) (void *)(p)=(g->headerBuffer+lenAddedHeader)
#define MYLOCATE(p) (p)=(void *)(g->headerBuffer+lenAddedHeader)
#define MYWRITE(p) lenAddedHeader+=sizeof(*(p))
#define MYWRITEBUF(data,datalen) \
    do { \
        INSURE((datalen)<=g->lenHeaderBuffer-lenAddedHeader); \
        memcpy(g->headerBuffer+lenAddedHeader,(char *)(data),(datalen)); \
        lenAddedHeader+=(datalen); \
    } while(0)

FUNC_STATUS WORD jpgFix_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    struct shortHeader_s {
        unsigned char soi[2];              /* 0xFF, 0xD8 */

        unsigned char app1[2];             /* 0xFF, 0xE1 */
        unsigned char app1Length[2];       /* 0x00, 0x12 */
        unsigned char height[2];
        unsigned char width[2];
        unsigned char xres[2];
        unsigned char yres[2];
        unsigned char ac_q_factor;
        unsigned char numComponents;       /* 1=gray, 3=color */
        unsigned char xSampleFactors[2];
        unsigned char ySampleFactors[2];
        unsigned char dc_q_factor;
        unsigned char reserved;            /* 0x00 */
    } *pInputHeader=(struct shortHeader_s *)pbInputBuf;
    struct outputSoiApp0_s {
        unsigned char soi[2];              /* 0xFF, 0xD8 */

        unsigned char app0[2];             /* 0xFF, 0xE0 */
        unsigned char app0Length[2];       /* 0x00, 0x10 */
        unsigned char jfif[5];             /* "JFIF\0" */
        unsigned char majorVersion;        /* 0x01 */
        unsigned char minorVersion;        /* 0x00 */
        unsigned char units;               /* 0x01 = DPI */
        unsigned char xres[2];
        unsigned char yres[2];
        unsigned char xthumb;              /* 0x00 */
        unsigned char ythumb;              /* 0x00 */
    } *pOutputSoiApp0;
    struct {
        unsigned char sof0[2];             /* 0xFF, 0xC0 */
        unsigned char sof0Length[2];       /* 0x00, 0x?? */
        unsigned char eight;               /* 0x08 */
        unsigned char height[2];
        unsigned char width[2];
        unsigned char numComponents;       /* 1=gray, 3=color */
    } *pOutputSof0Part1;
    struct {
        unsigned char iComponent;
        unsigned char xySampleFactors;
        unsigned char isNotFirstComponent;
    } *pOutputSofComponent;
    struct {
        unsigned char dqt[2];              /* 0xFF, 0xDB */
        unsigned char dqtLength[2];        /* 0x00, 0x43 */
        unsigned char ident;               /* 0=lum., 1=chrom. */
        unsigned char elements[64];
    } *pOutputDqt;
    struct {
        unsigned char dht[2];              /* 0xFF, 0xC4 */
        unsigned char dhtLength[2];        /* 0x00, 0x?? */
    } *pOutputDhtPart1;
    struct {
        unsigned char hclass_ident;
        unsigned char counts[16];
        /* Variable-length huffval table follows. */
    } *pOutputDhtPart2;
    static const struct {
        unsigned char hclass_ident;
        const unsigned char *counts;
        const unsigned char *huffval;
    } dhtInfo[4]={
        {0x00,lum_DC_counts,lum_DC_values},
        {0x10,lum_AC_counts,lum_AC_values},
        {0x01,chrom_DC_counts,chrom_DC_values},
        {0x11,chrom_AC_counts,chrom_AC_values}
    };
    int dhtCountCounts[4];
    struct {
        unsigned char sos[2];              /* 0xFF, 0xDA */
        unsigned char sosLength[2];        /* 0x00, 0x?? */
        unsigned char numComponents;       /* 1=gray, 3=color */
    } *pOutputSosPart1;
    struct {
        unsigned char iComponent;
        unsigned char x00x11;              /* (i==0 ? 0x00 : 0x11) */
    } *pOutputSosComponent;
    struct {
        unsigned char zero1;               /* 0x00 */
        unsigned char sixtythree;          /* 0x3F */
        unsigned char zero2;               /* 0x00 */
    } *pOutputSosPart2;

    PJFIX_INST g;
    DWORD lenRemovedHeader=0,lenAddedHeader=0;
    int imax,i,j,x,y,r,len;

    HANDLE_TO_PTR (hXform, g);

    /* Validate APP1 (OfficeJet short header) record.
     * If we actually get a JFIF standard APP0 record, then
     * skip parsing the header. */
    if (dwInputAvail>=sizeof(*pInputHeader) &&
        pInputHeader->soi[0]==0xFF && pInputHeader->soi[1]==0xD8 &&
        pInputHeader->app1[0]==0xFF && pInputHeader->app1[1]==0xE1 &&
        !pInputHeader->app1Length[0] && pInputHeader->app1Length[1]==0x12) {

        /* Write standard JPEG/JFIF records... */
        lenRemovedHeader=sizeof(*pInputHeader);

        /* Start Of Image record. */
        MYLOCATE(pOutputSoiApp0);
        pOutputSoiApp0->soi[0]=0xFF;
        pOutputSoiApp0->soi[1]=0xD8;
        /* APP0 (JFIF header) record. */
        pOutputSoiApp0->app0[0]=0xFF;
        pOutputSoiApp0->app0[1]=0xE0;
        pOutputSoiApp0->app0Length[0]=0x00;
        pOutputSoiApp0->app0Length[1]=sizeof(*pOutputSoiApp0)-4;
        strcpy((char *)pOutputSoiApp0->jfif,"JFIF");
        pOutputSoiApp0->majorVersion=0x01;
        pOutputSoiApp0->minorVersion=0x00;
        pOutputSoiApp0->units=0x01;
        pOutputSoiApp0->xres[0]=pInputHeader->xres[0];
        pOutputSoiApp0->xres[1]=pInputHeader->xres[1];
        pOutputSoiApp0->yres[0]=pInputHeader->yres[0];
        pOutputSoiApp0->yres[1]=pInputHeader->yres[1];
        pOutputSoiApp0->xthumb=0;
        pOutputSoiApp0->ythumb=0;
        MYWRITE(pOutputSoiApp0);

        /* Start Of Frame record. */
        MYLOCATE(pOutputSof0Part1);
        pOutputSof0Part1->sof0[0]=0xFF;
        pOutputSof0Part1->sof0[1]=0xC0;
        pOutputSof0Part1->sof0Length[0]=0x00;
        pOutputSof0Part1->sof0Length[1]=sizeof(*pOutputSof0Part1)-2+
            pInputHeader->numComponents*sizeof(*pOutputSofComponent);
        pOutputSof0Part1->eight=0x08;
        pOutputSof0Part1->height[0]=pInputHeader->height[0];
        pOutputSof0Part1->height[1]=pInputHeader->height[1];
        pOutputSof0Part1->width[0]=pInputHeader->width[0];
        pOutputSof0Part1->width[1]=pInputHeader->width[1];
        pOutputSof0Part1->numComponents=pInputHeader->numComponents;
        MYWRITE(pOutputSof0Part1);
        x=BEND_GET_SHORT(pInputHeader->xSampleFactors);
        y=BEND_GET_SHORT(pInputHeader->ySampleFactors);
        for (i=0;i<pInputHeader->numComponents;i++) {
            MYLOCATE(pOutputSofComponent);
            pOutputSofComponent->iComponent=i;
            pOutputSofComponent->xySampleFactors=
                (((x>>(4*(3-i)))&0x0F)<<4) | ((y>>(4*(3-i)))&0x0F);
            pOutputSofComponent->isNotFirstComponent=(!i?0:1);
            MYWRITE(pOutputSofComponent);
        }

        /* Define Quantization Table record. */
        imax=pInputHeader->numComponents>1?1:0;
        for (i=0;i<=imax;i++) {
            MYLOCATE(pOutputDqt);
            pOutputDqt->dqt[0]=0xFF;
            pOutputDqt->dqt[1]=0xDB;
            pOutputDqt->dqtLength[0]=0x00;
            pOutputDqt->dqtLength[1]=sizeof(*pOutputDqt)-2;
            pOutputDqt->ident=i;    /* Upper nibble=0 for 8-bit table. */
            scale_q_table(pInputHeader->dc_q_factor,pInputHeader->ac_q_factor,
                i,pOutputDqt->elements);
            MYWRITE(pOutputDqt);
        }
        imax=pInputHeader->numComponents>1?4:2;
        x=sizeof(*pOutputDhtPart1)-2;
        for (i=0;i<imax;i++) {
            dhtCountCounts[i]=0;
            x+=sizeof(*pOutputDhtPart2);
            for (j=0;j<16;j++) {
                y=dhtInfo[i].counts[j];
                dhtCountCounts[i]+=y;
                x+=y;
            }
        }
        MYLOCATE(pOutputDhtPart1);
        pOutputDhtPart1->dht[0]=0xFF;
        pOutputDhtPart1->dht[1]=0xC4;
        BEND_SET_SHORT(pOutputDhtPart1->dhtLength,x);
        MYWRITE(pOutputDhtPart1);
        for (i=0;i<imax;i++) {
            MYLOCATE(pOutputDhtPart2);
            pOutputDhtPart2->hclass_ident=dhtInfo[i].hclass_ident;
            memcpy(pOutputDhtPart2->counts,dhtInfo[i].counts,16);
            MYWRITE(pOutputDhtPart2);
            MYWRITEBUF(dhtInfo[i].huffval,dhtCountCounts[i]);
        }

        /* Start Of Scan record. */
        MYLOCATE(pOutputSosPart1);
        imax=pInputHeader->numComponents;
        pOutputSosPart1->sos[0]=0xFF;
        pOutputSosPart1->sos[1]=0xDA;
        pOutputSosPart1->sosLength[0]=0;
        pOutputSosPart1->sosLength[1]=sizeof(*pOutputSosPart1)-2+
            imax*sizeof(*pOutputSosComponent)+sizeof(*pOutputSosPart2);
        pOutputSosPart1->numComponents=imax;
        MYWRITE(pOutputSosPart1);
        for (i=0;i<imax;i++) {
            MYLOCATE(pOutputSosComponent);
            pOutputSosComponent->iComponent=i;
            pOutputSosComponent->x00x11=(!i?0x00:0x11);
            MYWRITE(pOutputSosComponent);
        }
        MYLOCATE(pOutputSosPart2);
        pOutputSosPart2->zero1=0;
        pOutputSosPart2->sixtythree=63;
        pOutputSosPart2->zero2=0;
        MYWRITE(pOutputSosPart2);
    }

    /* Save a copy of the (possibly rewritten) header (plus any residual
     * data) so we can emit it later. */
    len=dwInputAvail-(lenAddedHeader-lenRemovedHeader);
    if (len>g->lenHeaderBuffer-lenAddedHeader) {
        len=g->lenHeaderBuffer-lenAddedHeader;
    }
    memcpy(g->headerBuffer+lenAddedHeader,pbInputBuf+lenRemovedHeader,len);

    /* Pass the (possibly rewritten) header to the slave transformer. */
    r=jpgDecodeTbl.getActualTraits(g->pSlaveXform,
        len+lenAddedHeader,g->headerBuffer,
        pdwInputUsed,pdwInputNextPos,
        pInTraits,pOutTraits);
    if ((r&(IP_DONE|IP_READY_FOR_DATA))!=(IP_DONE|IP_READY_FOR_DATA)) {
        return (r|IP_FATAL_ERROR);
    }

    g->lenHeader=*pdwInputUsed;
    g->lenAddedHeader=lenAddedHeader-lenRemovedHeader;
    *pdwInputUsed-=g->lenAddedHeader;
    *pdwInputNextPos-=g->lenAddedHeader;

    return r;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * jpgFix_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD jpgFix_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    WORD r;
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);

    r=jpgDecodeTbl.getActualBufSizes(g->pSlaveXform,
        pdwMinInBufLen,pdwMinOutBufLen);
    if (r==IP_DONE) {
        INSURE(*pdwMinOutBufLen<=g->lenHeaderBuffer);
    }
    *pdwMinOutBufLen=g->lenHeaderBuffer;
    return r;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_convert - Converts one row
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_convert (
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
    PJFIX_INST g;
    WORD r=0;

    HANDLE_TO_PTR (hXform, g);

    if (g->outNextPos<g->lenHeader) {
        DWORD len=g->lenHeader-g->outNextPos;
        if (len>dwOutputAvail) len=dwOutputAvail;
        if (len) {
            memcpy(pbOutputBuf,g->headerBuffer+g->outNextPos,len);
        }
        *pdwInputUsed=0;
        *pdwInputNextPos=g->lenHeader-g->lenAddedHeader;
        *pdwOutputUsed=len;
        *pdwOutputThisPos=g->outNextPos;
        g->outNextPos+=len;
        r|=IP_READY_FOR_DATA;

    } else if (!g->readyForSofRewrite) {
        DWORD dwOutputUsed,dwOutputThisPos;
        r=jpgDecodeTbl.convert(g->pSlaveXform,
          dwInputAvail,pbInputBuf,pdwInputUsed,pdwInputNextPos,
          g->lenHeaderBuffer,g->headerBuffer,
          &dwOutputUsed,&dwOutputThisPos);
        *pdwInputNextPos-=g->lenAddedHeader;
        if (r&IP_DONE) {
            g->readyForSofRewrite=1;
            if (!*pdwInputUsed) goto rewriteSof;
        }
        if (*pdwInputUsed) {
            INSURE(*pdwInputUsed<=dwOutputAvail);
            memcpy(pbOutputBuf,pbInputBuf,*pdwInputUsed);
        }
        *pdwOutputUsed=*pdwInputUsed;
        *pdwOutputThisPos=g->outNextPos;
        g->outNextPos+=*pdwInputUsed;

    } else {
rewriteSof:
        r|=IP_DONE;
        *pdwInputUsed=0;
        *pdwInputNextPos=g->outNextPos-g->lenAddedHeader;
        *pdwOutputUsed=0;
        *pdwOutputThisPos=g->outNextPos;
        if (g->readyForSofRewrite>0) {
            int rcCountup,rcTraits,sofOffset;
            r=jpgDecode_getRowCountInfo(g->pSlaveXform,
                &rcCountup,&rcTraits,&sofOffset);
            BEND_SET_SHORT(pbOutputBuf,rcCountup);
            *pdwOutputUsed=2;
            *pdwOutputThisPos=sofOffset;
            g->readyForSofRewrite=-1;
        }
    }

    return r;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFix_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * jpgFix_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_newPage (
    IP_XFORM_HANDLE hXform)
{
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * jpgFix_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD jpgFix_closeXform (IP_XFORM_HANDLE hXform)
{
    PJFIX_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->pSlaveXform) jpgDecodeTbl.closeXform(g->pSlaveXform);
    if (g->headerBuffer) IP_MEM_FREE(g->headerBuffer);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgFixTbl - Jump-table for transform driver
 *
\*****************************************************************************/
IP_XFORM_TBL jpgFixTbl = {
    jpgFix_openXform,
    jpgFix_setDefaultInputTraits,
    jpgFix_setXformSpec,
    jpgFix_getHeaderBufSize,
    jpgFix_getActualTraits,
    jpgFix_getActualBufSizes,
    jpgFix_convert,
    jpgFix_newPage,
    jpgFix_insertedData,
    jpgFix_closeXform
};

/* End of File */

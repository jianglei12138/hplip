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
 * xtiff.c - encoder and decoder for TIFF files for image processor
 *
 *****************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    tifEncodeTbl = the encoder,
 *    tifDecodeTbl = the decoder.
 *
 * Encoder:  Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_TIFF_FILE_PATH] = pointer to a file-path, or NULL
 *
 *        If NULL, a one-page TIFF is output in the normal manner.
 *        If not NULL, this must be a pointer to a string containing a file-path.
 *        If the given file does not exist, it will be created and one image will
 *        be put in it.  If it does exist, the image will be APPENDED to this file,
 *        and no data will be output by this xform.  So if one or more pages are
 *        already in the file, we will add another page to it.  This xform does
 *        the opening and closing of the file.
 *
 * Decoder:  Items in aXformInfo array passed into setXformSpec:
 *
 *       None.
 *
 * Capabilities and Limitations:
 *
 *    Handles 1, 8, 16, 24 and 48 bits per pixel.
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
 * Apr 2000, Mark Overton, ported header-setup from TWAIN source, and added
 *                         multi-page capability
 * Feb 1998, Mark Overton, ported to new Image Processor code
 * May 1996, Mark Overton, wrote original code
 * Jun 2000, Mark Overton, wrote a simple decoder that does no file-seeks
 *
 *****************************************************************************/


#include "stdio.h"    /* for FILE operations */
#include "assert.h"
#include "hpip.h"
#include "ipdefs.h"
#include "string.h"


#if 0
    #include "stdio.h"
    #include <tchar.h>

    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x1ce5ca7e



/*____________________________________________________________________________
 |                                                                            |
 | Constants pertaining to tags                                               |
 |____________________________________________________________________________|
*/


/* TIF_INST - our instance variables */

typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the image */
    int      iBitsPerSample;  /* bits per channel (1, 8 or 16) */
    BOOL     bByteSwap;       /* bytes are in wrong endian order, so must swap? */
    char     sFilePath[200];  /* path to the file (empty string means none) */
    FILE    *fileOut;         /* handle of opened file */
    DWORD    dwRawRowBytes;   /* bytes per raw row */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwValidChk;      /* struct validity check value */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    BOOL     fDidHeader;      /* already sent the header? */
} TIF_INST, *PTIF_INST;


/* Types having known sizes */
typedef unsigned char  TIFF_UBYTE;    /* 8 bits  */
typedef unsigned short TIFF_USHORT;   /* 16 bits */
typedef unsigned int   TIFF_ULONG;    /* 32 bits */
typedef   signed char  TIFF_SBYTE;    /* 8 bits  */
typedef   signed short TIFF_SSHORT;   /* 16 bits */
typedef   signed int   TIFF_SLONG;    /* 32 bits */

/* TIFF file header defines */
#define INTEL                 0x4949
#define TIFF_VERSION          42

/* TIFF field lengths */
#define TIFFBYTE              1
#define TIFFASCII             2
#define TIFFSHORT             3
#define TIFFLONG              4
#define TIFFRATIONAL          5
#define TIFFSBYTE             6
#define TIFFUNDEFINED         7
#define TIFFSSHORT            8
#define TIFFSLONG             9
#define TIFFSRATIONAL         10

/* TIFF compression type */
#define NOCOMPRESSION         1

/* TIFF planar configuration */
#define CONTIGUOUS            1
#define PLANAR                2

/* TIFF photometric interpretations */
#define ZERO_IS_WHITE         0
#define ZERO_IS_BLACK         1
#define RGB_COLOR             2

/* TIFF resolution units */
#define DOTS_PER_INCH         2


/* TIFF tags */
#define NUMTAGS 13
#define BITS_PER_SAMPLE       258    // 0x0102
#define COMPRESSION           259    // 0x0103
#define IMAGE_LENGTH          257    // 0x0101
#define IMAGE_WIDTH           256    // 0x0100
#define NEW_SUBFILE           254    // 0x00fe
#define PHOTO_INTERPRET       262    // 0x0106
#define RESOLUTION_UNIT       296    // 0x0128
#define ROWS_PER_STRIP        278    // 0x0116
#define SAMPLES_PER_PIXEL     277    // 0x0115
#define STRIP_COUNTS          279    // 0x0117
#define STRIP_OFFSETS         273    // 0x0111
#define XRESOLUTION           282    // 0x011A
#define YRESOLUTION           283    // 0x011B

typedef struct {
     TIFF_ULONG n;
     TIFF_ULONG d;
} __attribute__((packed)) RATIONAL;

typedef union {
     TIFF_UBYTE  b[4];
     TIFF_USHORT s[2];
     TIFF_ULONG  l;
     TIFF_ULONG  o;
} __attribute__((packed)) TIFFVALUE;

typedef struct {
     TIFF_USHORT TagID;
     TIFF_USHORT Kind;
     TIFF_ULONG  Length;  /* number of items, NOT number of bytes */
     TIFFVALUE   Value;
} __attribute__((packed)) TIFFTAG;

typedef struct {
    TIFF_USHORT NumTags;
    TIFFTAG     Tag[NUMTAGS];
    TIFF_ULONG  OffsetNextIFD;
} __attribute__((packed)) TIFFIFD;

typedef struct {
    TIFF_UBYTE    ByteOrder[2];
    TIFF_USHORT    Version;
    TIFF_ULONG    OffsetFirstIFD;
} __attribute__((packed)) TIFFHEADER;

#define NUMEXTBYTES (2*sizeof(RATIONAL) + 3*sizeof(TIFF_USHORT))
#define MAX_HEADER_SIZE (sizeof(TIFFHEADER) + sizeof(TIFFIFD) + NUMEXTBYTES)



/*****************************************************************************\
 *
 * tifEncode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD tifEncode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PTIF_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(TIF_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(TIF_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncode_setDefaultInputTraits - Specifies default input image traits
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

static WORD tifEncode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PTIF_INST g;
    int       ppr, bpp;

    HANDLE_TO_PTR (hXform, g);
    ppr = pTraits->iPixelsPerRow;
    bpp = pTraits->iBitsPerPixel;

    /* Insure that values we actually use are known */
    INSURE (ppr > 0);
    INSURE (bpp > 0);

    g->dwRawRowBytes = (ppr*bpp + 7) / 8;
    g->traits = *pTraits;   /* a structure copy */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD tifEncode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PTIF_INST g;
    char      *s;

    HANDLE_TO_PTR (hXform, g);
    s = (char*)aXformInfo[IP_TIFF_FILE_PATH].pvoid;
    if (s != NULL)
        strcpy (g->sFilePath, s);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD tifEncode_getHeaderBufSize (
    IP_XFORM_HANDLE   hXform,         /* in:  handle for xform */
    DWORD            *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * tifEncode_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD tifEncode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Since we don't change traits, just copy out the default traits */
    *pInTraits  = g->traits;
    *pOutTraits = g->traits;
    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * tifEncode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD tifEncode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,  /* out: min input buf size */
    PDWORD           pdwMinOutBufLen) /* out: min output buf size */
{
    PTIF_INST g;
    UINT      len;

    HANDLE_TO_PTR (hXform, g);
    len = g->dwRawRowBytes;
    *pdwMinInBufLen = len;

    if (len < MAX_HEADER_SIZE)
        len = MAX_HEADER_SIZE;
    *pdwMinOutBufLen = len;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * outputHeader - Only called by tifEncode_convert
 *
\****************************************************************************/



static void SetTag (TIFFTAG *pTag, 
             unsigned short TagID, short Kind, long Length, int Value)
{
    pTag->TagID   = (TIFF_USHORT)TagID;
    pTag->Kind    = (TIFF_USHORT)Kind;
    pTag->Length  = (TIFF_ULONG)Length;
    pTag->Value.l = (TIFF_ULONG)Value;   /* assumes little-endian computer */
}



static int WriteFileHeader (PBYTE pTIFF)
{
    TIFFHEADER *pHead;

    pHead = (TIFFHEADER*) pTIFF;
    pHead->ByteOrder[0] = 'I';
    pHead->ByteOrder[1] = 'I';    /* assumes little-endian computer */
    pHead->Version = 42;
    pHead->OffsetFirstIFD = 8;

    return 8;   /* we output 8 bytes */
}



static int WriteIFD (
    PBYTE pTIFF,         /* out: the IFD is written to this buffer */
    int   iStartOffset,  /* in:  file-offset at which this IFD starts */
    int   WidthBytes,    /* in:  row-width in bytes */
    int   WidthPixels,   /* in:  row-width in pixels */
    int   Height,        /* in:  number of rows */
    int   BPP,           /* in:  bits per pixel */
    int   XRes,          /* in:  dpi in X */
    int   YRes)          /* in:  dpi in Y */
{
    TIFFIFD     *pIFD;
    TIFFTAG     *pTag;
    BYTE        *pMore;
    TIFF_USHORT *pBPS;
    int          iBPSOffset, iImageOffset;
    int          SPP, BPS, PI;

    pIFD = (TIFFIFD*)pTIFF;
    pMore = pTIFF + sizeof(TIFFIFD);   /* 1st byte after the IFD */

    /* allocate space for the 3 shorts for the 3 bps values (if needed) */
    iBPSOffset = iStartOffset + (pMore-pTIFF);
    pBPS = (TIFF_USHORT*)pMore;
    pMore += 3*sizeof(TIFF_USHORT);

    /* the pixels are put after the above items */
    iImageOffset = iStartOffset + (pMore-pTIFF);

    switch (BPP) {
        case 1:
            PI = ZERO_IS_WHITE; SPP = 1; BPS = 1;  break;  /* 1-bit bilevel */
        case 8:
            PI = ZERO_IS_BLACK; SPP = 1; BPS = 8;  break;  /* 8-bit grayscale */
        case 16:
            PI = ZERO_IS_BLACK; SPP = 1; BPS = 16; break;  /* 16-bit grayscale */
        case 24:
            PI = RGB_COLOR;     SPP = 3; BPS = 8;  break;  /* 24-bit color */
        case 48:
            PI = RGB_COLOR;     SPP = 3; BPS = 16; break;  /* 48-bit color */
        default:
            PI = RGB_COLOR;     SPP = 3; BPS = 8;   /* guess 24-bit color */
            assert (0);  /* crash if in debug mode */
    }

    pIFD->NumTags = NUMTAGS;
    pTag = pIFD->Tag;
    SetTag (pTag++, NEW_SUBFILE,       TIFFSHORT, 1,   0);
    SetTag (pTag++, IMAGE_WIDTH,       TIFFLONG,  1,   WidthPixels);
    SetTag (pTag++, IMAGE_LENGTH,      TIFFLONG,  1,   Height);
    SetTag (pTag++, BITS_PER_SAMPLE,   TIFFSHORT, SPP, SPP>1 ? iBPSOffset : BPS);
    SetTag (pTag++, COMPRESSION,       TIFFSHORT, 1,   1);
    SetTag (pTag++, PHOTO_INTERPRET,   TIFFSHORT, 1,   PI);
    SetTag (pTag++, STRIP_OFFSETS,     TIFFLONG,  1,   iImageOffset);
    SetTag (pTag++, SAMPLES_PER_PIXEL, TIFFSHORT, 1,   SPP);
    SetTag (pTag++, ROWS_PER_STRIP,    TIFFLONG,  1,   Height);
    SetTag (pTag++, STRIP_COUNTS,      TIFFLONG,  1,   WidthBytes*Height);
    SetTag (pTag++, XRESOLUTION,       TIFFSHORT, 1,   XRes);
    SetTag (pTag++, YRESOLUTION,       TIFFSHORT, 1,   YRes);
    SetTag (pTag++, RESOLUTION_UNIT,   TIFFSHORT, 1,   2);
    assert ((pTag - pIFD->Tag) == NUMTAGS);
    pIFD->OffsetNextIFD = 0L;    /* assume there is no next IFD */

    /* Stick Samples Per Pixel here for color; Only used if pointed to by tag */
    *pBPS++ = BPS;
    *pBPS++ = BPS;
    *pBPS++ = BPS;

    return pMore - pTIFF;
}



static WORD AppendIFDToFile (
    PTIF_INST g,              /* in:  ptr to instance structure */
    PBYTE     pbTempBuf)      /* in:  temp buffer large enough to hold an IFD */
{
    int         result;
    TIFF_ULONG  fileEndPos, IFDPos, pointerPos;
    TIFF_USHORT numTags;
    int         iHeaderLen, iIFDLen, iTotalLen;

    /***** If the file is empty, do usual set-up *****/

    g->fileOut = fopen (g->sFilePath, "a+b");
    INSURE (g->fileOut != NULL);
    result = fseek (g->fileOut, 0, SEEK_END);
    INSURE (result == 0);
    fileEndPos = ftell (g->fileOut);
    INSURE (result >= 0);

    if (fileEndPos == 0) {
        iHeaderLen = WriteFileHeader(pbTempBuf);
        iIFDLen = WriteIFD (pbTempBuf+iHeaderLen, iHeaderLen,
            g->dwRawRowBytes, g->traits.iPixelsPerRow,
            g->traits.lNumRows, g->traits.iBitsPerPixel,
            g->traits.lHorizDPI>>16, g->traits.lVertDPI>>16);

        iTotalLen = iHeaderLen + iIFDLen;
        INSURE (iTotalLen <= MAX_HEADER_SIZE);

        result = fwrite (pbTempBuf, 1, iTotalLen, g->fileOut);
        INSURE (result == iTotalLen);

        return IP_READY_FOR_DATA;
    }

    /***** Find the last IFD *****/

    result = fseek (g->fileOut, 4, SEEK_SET);
    INSURE (result == 0);
    result = fread (&IFDPos, 4, 1, g->fileOut);  /* assumes little-endian file */
    INSURE (result == 1);

    do {    /* hop thru the IFDs until we hit the last one */
        result = fseek (g->fileOut, IFDPos, SEEK_SET);
        INSURE (result == 0);
        result = fread (&numTags, 2, 1, g->fileOut);  /* assumes little-endian file */
        INSURE (result==1 && numTags>0);
        pointerPos = IFDPos + 2 + numTags*sizeof(TIFFTAG);
        result = fseek (g->fileOut, pointerPos, SEEK_SET);
        INSURE (result == 0);
        result = fread (&IFDPos, 4, 1, g->fileOut);  /* assumes little-endian file */
        INSURE (result == 1);
    } while (IFDPos != 0);

    /***** PointerPos is the final IFD offset in the file; change it *****/

    /* switch to writing from now on */
    fclose (g->fileOut);
    g->fileOut = fopen(g->sFilePath, "r+b");
    INSURE (g->fileOut != NULL);

    result = fseek (g->fileOut, pointerPos, SEEK_SET);
    INSURE (result == 0);
    result = fwrite (&fileEndPos, 4, 1, g->fileOut);  /* assumes little-endian file */
    INSURE (result == 1);

    /***** Output a new IFD for the new page *****/

    iIFDLen = WriteIFD (pbTempBuf, fileEndPos,
        g->dwRawRowBytes, g->traits.iPixelsPerRow,
        g->traits.lNumRows, g->traits.iBitsPerPixel,
        g->traits.lHorizDPI>>16, g->traits.lVertDPI>>16);

    result = fseek (g->fileOut, 0, SEEK_END);
    INSURE (result == 0);
    result = fwrite (pbTempBuf, 1, iIFDLen, g->fileOut);
    INSURE (result == iIFDLen);
    /* leave the file-position at the end, where image-data will go */

    return IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



static WORD outputHeader (
    PTIF_INST g,                /* in:  ptr to instance structure */
    DWORD     dwOutputAvail,    /* in:  # avail bytes in out-buf */
    PBYTE     pbOutputBuf,      /* in:  ptr to out-buffer */
    PDWORD    pdwOutputUsed,    /* out: # bytes written in out-buf */
    PDWORD    pdwOutputThisPos) /* out: file-pos to write the data */
{
    int iHeaderLen, iIFDLen, iTotalLen;

    INSURE (dwOutputAvail >= MAX_HEADER_SIZE);
    *pdwOutputThisPos = 0;

    if (g->sFilePath[0] != 0) {
        *pdwOutputUsed = 0;
        return AppendIFDToFile (g, pbOutputBuf);
    }

    iHeaderLen = WriteFileHeader(pbOutputBuf);
    iIFDLen = WriteIFD (pbOutputBuf+iHeaderLen, iHeaderLen,
        g->dwRawRowBytes, g->traits.iPixelsPerRow,
        g->traits.lNumRows, g->traits.iBitsPerPixel,
        g->traits.lHorizDPI>>16, g->traits.lVertDPI>>16);

    iTotalLen = iHeaderLen + iIFDLen;
    INSURE (iTotalLen <= MAX_HEADER_SIZE);
    *pdwOutputUsed    = iTotalLen;
    *pdwOutputThisPos = 0;
    g->dwOutNextPos   = iTotalLen;

    return IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD tifEncode_convert (
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
    PTIF_INST g;
    UINT      n;

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
        PRINT (_T("tif_encode_convert_row: Told to flush.\n"), 0, 0);
        if (g->traits.lNumRows < 0) {
            /* # rows wasn't known at first, so output header again
             * now that we know the number of rows */
            INSURE (g->sFilePath[0] == 0);
            g->traits.lNumRows = g->dwRowsDone;
            *pdwInputUsed = 0;
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

    n = g->dwRawRowBytes;
    INSURE (dwInputAvail  >= n);
    INSURE (dwOutputAvail >= n);

    if (g->sFilePath[0] == 0) {
        memcpy (pbOutputBuf, pbInputBuf, n);

        *pdwOutputUsed    = n;
        *pdwOutputThisPos = g->dwOutNextPos;
        g->dwOutNextPos  += n;
    } else {
        UINT result;
        INSURE (g->fileOut != NULL);
        result = fwrite (pbInputBuf, 1, n, g->fileOut);
        INSURE (result == n);
        
        *pdwOutputUsed    = 0;
        *pdwOutputThisPos = 0;
        g->dwOutNextPos   = 0;
    }

    g->dwInNextPos   += n;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwInputUsed     = n;
    g->dwRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD tifEncode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * tifEncode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD tifEncode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * tifEncode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD tifEncode_closeXform (IP_XFORM_HANDLE hXform)
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    if (g->fileOut != NULL)
        fclose (g->fileOut);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifEncodeTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL tifEncodeTbl = {
    tifEncode_openXform,
    tifEncode_setDefaultInputTraits,
    tifEncode_setXformSpec,
    tifEncode_getHeaderBufSize,
    tifEncode_getActualTraits,
    tifEncode_getActualBufSizes,
    tifEncode_convert,
    tifEncode_newPage,
    tifEncode_insertedData,
    tifEncode_closeXform
};




/*****************************************************************************\
 *****************************************************************************
 *
 *                             D  E  C  O  D  E  R
 *
 *****************************************************************************
\*****************************************************************************/




/*****************************************************************************\
 *
 * tifDecode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD tifDecode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PTIF_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(TIF_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(TIF_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecode_setDefaultInputTraits - Specifies default input image traits
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

static WORD tifDecode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD tifDecode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PTIF_INST g;
    char      *s;

    HANDLE_TO_PTR (hXform, g);
    /* file-path is not used, but might be later, so save it */
    s = (char*)aXformInfo[IP_TIFF_FILE_PATH].pvoid;
    if (s != NULL)
        strcpy (g->sFilePath, s);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecode_getHeaderBufSize - Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD tifDecode_getHeaderBufSize (
    IP_XFORM_HANDLE   hXform,         /* in:  handle for xform */
    DWORD            *pdwInBufLen)    /* out: buf size for parsing header */
{
    *pdwInBufLen = MAX_HEADER_SIZE + 10000;   /* 10000 gives us huge margin */
    return IP_DONE;
}



/*****************************************************************************\
 *
 * ByteSwap - Reverses endian-type of the given variable (1, 2, 4 or 8 bytes)
 *
\*****************************************************************************/

static void ByteSwap (
    void *pvVar,
    int   nBytes)
{
    BYTE b;
    BYTE *pb = (BYTE*)pvVar;

    switch (nBytes) {
        case 1:
            /* do nothing */
            break;
        case 2:
            b = pb[0];  pb[0] = pb[1];  pb[1] = b;
            break;
        case 4:
            b = pb[1];  pb[1] = pb[2];  pb[2] = b;
            b = pb[0];  pb[0] = pb[3];  pb[3] = b;
            break;
        case 8:
            /* this is actually two longs, so fix each one */
            ByteSwap (pvVar, 4);
            ByteSwap ((BYTE*)pvVar+4, 4);
            break;
    }
}



/*****************************************************************************\
 *
 * ParseTag - Parses a tag, putting value in traits or dwImageOffset
 *
\*****************************************************************************/

static BOOL ParseTag (
    PTIF_INST  g,              /* in:  our instance variables */
    TIFFTAG   *pTag,           /* in:  the tag to parse */
    BYTE      *pbInputBuf,     /* in:  input buffer containing TIFF header */
    BYTE      *pbBufAfter,     /* in:  1st byte after the input buffer */
    DWORD     *pdwImageOffset) /* out: image start offset set by STRIP_OFFSETS */
{
    unsigned  id;
    unsigned  kind;
    unsigned  count;
    void     *pValue;
    int       value;
    int       nTypeBytes, nValueBytes;
    int       i;

    if (g->bByteSwap) {
        ByteSwap (&(pTag->TagID ), 2);
        ByteSwap (&(pTag->Kind  ), 2);
        ByteSwap (&(pTag->Length), 4);
    }

    id    = pTag->TagID;
    kind  = pTag->Kind;
    count = pTag->Length;

    switch (kind) {
        case TIFFUNDEFINED: nTypeBytes = 1; break;
        case TIFFBYTE:      nTypeBytes = 1; break;
        case TIFFSBYTE:     nTypeBytes = 1; break;
        case TIFFSHORT:     nTypeBytes = 2; break;
        case TIFFSSHORT:    nTypeBytes = 2; break;
        case TIFFLONG:      nTypeBytes = 4; break;
        case TIFFSLONG:     nTypeBytes = 4; break;
        case TIFFRATIONAL:  nTypeBytes = 8; break;
        case TIFFSRATIONAL: nTypeBytes = 8; break;
        default: INSURE(0);
    }

    nValueBytes = count * nTypeBytes;

    if (nValueBytes <= 4)
        pValue = &(pTag->Value.l);
    else {
        if (g->bByteSwap)
            ByteSwap (&(pTag->Value.l), 4);
        pValue = pbInputBuf + pTag->Value.l;
    }
    INSURE ((BYTE*)pValue>pbInputBuf && (BYTE*)pValue<pbBufAfter);

    if (g->bByteSwap) {
        for (i=0; i<(int)count; i++)
            ByteSwap ((BYTE*)pValue + i*nTypeBytes, nTypeBytes);
    }

    switch (kind) {
        case TIFFUNDEFINED: value = *(TIFF_UBYTE *)pValue;  break;
        case TIFFBYTE:      value = *(TIFF_UBYTE *)pValue;  break;
        case TIFFSBYTE:     value = *(TIFF_SBYTE *)pValue;  break;
        case TIFFSHORT:     value = *(TIFF_USHORT*)pValue;  break;
        case TIFFSSHORT:    value = *(TIFF_SSHORT*)pValue;  break;
        case TIFFLONG:      value = *(TIFF_ULONG *)pValue;  break;
        case TIFFSLONG:     value = *(TIFF_SLONG *)pValue;  break;
        case TIFFRATIONAL:  value = ((RATIONAL*)pValue)->n / ((RATIONAL*)pValue)->d;  break;
        case TIFFSRATIONAL: value = ((RATIONAL*)pValue)->n / ((RATIONAL*)pValue)->d;  break;
        default: INSURE(0);
    }

    switch (id) {
        case NEW_SUBFILE:
            /* do nothing */
            break;
        case IMAGE_WIDTH:
            g->traits.iPixelsPerRow = value;
            break;
        case IMAGE_LENGTH:
            g->traits.lNumRows = value;
            break;
        case BITS_PER_SAMPLE:
            g->iBitsPerSample = value;
            break;
        case COMPRESSION:
            INSURE (value == 1);  /* we only support uncompressed */
            break;
        case PHOTO_INTERPRET:
            /* do nothing */
            break;
        case STRIP_OFFSETS:
            INSURE (count == 1);  /* we only support one strip */
            *pdwImageOffset = value;
            break;
        case SAMPLES_PER_PIXEL:
            g->traits.iComponentsPerPixel = value;
            break;
        case ROWS_PER_STRIP:
            /* do nothing -- we assume entire image is in one strip */
            break;
        case STRIP_COUNTS:
            /* do nothing -- this should be the # bytes in the raw data */
            break;
        case XRESOLUTION:
            g->traits.lHorizDPI = value << 16;
            break;
        case YRESOLUTION:
            g->traits.lVertDPI = value << 16;
            break;
        case RESOLUTION_UNIT:
            /* do nothing -- if it's not DPI, then reported DPI will be wrong */
            break;
        default:
            /* ignore the unknown tag */
            break;
    }

    return TRUE;

    fatal_error:
    return FALSE;
}



/*****************************************************************************\
 *
 * tifDecode_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD tifDecode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PTIF_INST   g;
    DWORD       dwImageOffset;
    PBYTE       pb;
    TIFFHEADER *pHead;
    TIFFIFD    *pIFD;
    int         iTag, nTags;

    HANDLE_TO_PTR (hXform, g);
    pb = pbInputBuf;

    /**** Parse the file-header ****/

    pHead = (TIFFHEADER*)pb;
    INSURE (pHead->ByteOrder[0]=='I' || pHead->ByteOrder[0]=='M');
    INSURE (pHead->ByteOrder[1]=='I' || pHead->ByteOrder[1]=='M');
    g->bByteSwap = pHead->ByteOrder[0] == 'M';
    if (g->bByteSwap)
        ByteSwap (&(pHead->OffsetFirstIFD), 4);
    INSURE (pHead->OffsetFirstIFD < dwInputAvail);
    /* ignore the file-version */
    pb = pbInputBuf + pHead->OffsetFirstIFD;

    /**** Parse the IFD (i.e., the tags), setting traits ****/

    pIFD = (TIFFIFD*)pb;
    if (g->bByteSwap)
        ByteSwap (&(pIFD->NumTags), 2);
    nTags = pIFD->NumTags;
    INSURE (nTags>0 && nTags<100);   /* sanity check */
    INSURE (nTags*sizeof(TIFFTAG) < dwInputAvail);
    dwImageOffset = 0;

    for (iTag=0; iTag<nTags; iTag++) {
        if (! ParseTag (g, pIFD->Tag+iTag, pbInputBuf, pbInputBuf+dwInputAvail, &dwImageOffset))
            goto fatal_error;
    }

    INSURE (g->iBitsPerSample==1 || g->iBitsPerSample==8 || g->iBitsPerSample==16);
    g->traits.iBitsPerPixel = g->iBitsPerSample * g->traits.iComponentsPerPixel;
    INSURE (dwImageOffset <= dwInputAvail);
    g->dwRawRowBytes = (g->traits.iBitsPerPixel*g->traits.iPixelsPerRow + 7) / 8;

    /**** Finish up ****/

    g->dwInNextPos   = dwImageOffset;
    *pdwInputUsed    = dwImageOffset;
    *pdwInputNextPos = dwImageOffset;

    *pInTraits  = g->traits;
    *pOutTraits = g->traits;
    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * tifDecode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD tifDecode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,  /* out: min input buf size */
    PDWORD           pdwMinOutBufLen) /* out: min output buf size */
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->dwRawRowBytes;
    *pdwMinOutBufLen = g->dwRawRowBytes;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD tifDecode_convert (
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
    PTIF_INST g;
    UINT      n;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("tif_decode_convert_row: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    n = g->dwRawRowBytes;

    if (dwInputAvail < n) {
        /* we got a partial row at the end -- just toss it */
        g->dwInNextPos   += dwInputAvail;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwInputUsed     = dwInputAvail;
        *pdwOutputUsed    = 0;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_READY_FOR_DATA;
    }

    INSURE (dwOutputAvail >= n);
    memcpy (pbOutputBuf, pbInputBuf, n);

    if (g->bByteSwap && g->iBitsPerSample==16) {
        /* we need to swap bytes in the 16-bit words in the pixels */
        BYTE *pb, *pbAfter, b;
        pbAfter = pbInputBuf + n;
        for (pb=pbInputBuf; pb<pbAfter; pb+=4) {
            /* process two words at a time for speed */
            b = pb[0];  pb[0] = pb[1];  pb[1] = b;
            b = pb[2];  pb[2] = pb[3];  pb[3] = b;
        }
    }

    *pdwOutputUsed    = n;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += n;

    g->dwInNextPos   += n;
    *pdwInputNextPos  = g->dwInNextPos;
    *pdwInputUsed     = n;
    g->dwRowsDone += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD tifDecode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * tifDecode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD tifDecode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * tifDecode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD tifDecode_closeXform (IP_XFORM_HANDLE hXform)
{
    PTIF_INST g;

    HANDLE_TO_PTR (hXform, g);
    if (g->fileOut != NULL)
        fclose (g->fileOut);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * tifDecodeTbl - Jump-table for decoder
 *
\*****************************************************************************/

IP_XFORM_TBL tifDecodeTbl = {
    tifDecode_openXform,
    tifDecode_setDefaultInputTraits,
    tifDecode_setXformSpec,
    tifDecode_getHeaderBufSize,
    tifDecode_getActualTraits,
    tifDecode_getActualBufSizes,
    tifDecode_convert,
    tifDecode_newPage,
    tifDecode_insertedData,
    tifDecode_closeXform
};

/* End of File */

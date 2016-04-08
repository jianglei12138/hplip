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

/* Original author: David Paschal (based on Mark Overton's "xskel" template). */

/******************************************************************************\
 *
 * xpnm.c - encoder and decoder for PNM (PBM, PGM, PPM) files
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    pnmEncodeTbl = the encoder.
 *    pnmDecodeTbl = the decoder.
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    None.
 *
 * Capabilities and Limitations:
 *
 *    Handles 1, 8, and 24 bits per pixel.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel   * passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
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

typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the input and output image */
    DWORD    dwBytesPerRow;   /* # of bytes in each row */
    DWORD    dwRowsDone;      /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
    BOOL     fIsEncode;       /* false=decode, true=encode */
    BOOL     fDidHeader;      /* already sent/processed the header? */
} PNM_INST, *PPNM_INST;

#define MAX_DECODE_HEADER_SIZE 4096
#define MAX_ENCODE_HEADER_SIZE 128



/*****************************************************************************\
 *
 * pnm{De,En}code_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

FUNC_STATUS WORD pnmDecode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PPNM_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(PNM_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(PNM_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}

FUNC_STATUS WORD pnmEncode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    WORD wResult=pnmDecode_openXform(pXform);
    if (wResult==IP_DONE) {
        PPNM_INST g;

        HANDLE_TO_PTR (*pXform, g);

        g->dwOutNextPos=MAX_ENCODE_HEADER_SIZE;
        g->fIsEncode=TRUE;
    }
    return wResult;

    fatal_error:
    return IP_FATAL_ERROR;
}


/*****************************************************************************\
 *
 * pnm_setDefaultInputTraits - Specifies default input image traits
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

FUNC_STATUS WORD pnm_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->traits = *pTraits;   /* a structure copy */
    if (g->fIsEncode) {
        /* insure that traits we care about are known */
        INSURE (pTraits->iPixelsPerRow>0 && pTraits->iBitsPerPixel>0);
        g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    }
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pnm_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PPNM_INST g;

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
 * pnm_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (!g->fIsEncode) {
        *pdwInBufLen = MAX_DECODE_HEADER_SIZE;
    } else {
        /* since input is raw pixels, there is no header, so set it to zero */
        *pdwInBufLen = 0;
    }
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pnm_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

#define PEEK_CHAR(pc) \
    do { \
        if (*pdwInputUsed>=dwInputAvail) { \
            return IP_INPUT_ERROR; \
        } \
        *(pc)=pbInputBuf[*pdwInputUsed]; \
    } while (0)

#define NEXT_CHAR ((*pdwInputUsed)++)

#define GET_CHAR(pc) \
    do { \
        PEEK_CHAR(pc); \
        NEXT_CHAR; \
    } while (0)

#define SKIP_WS \
    do { \
        unsigned char c; \
        PEEK_CHAR(&c); \
        if (c=='#') { \
            /* NEXT_CHAR; */ \
            do { \
                GET_CHAR(&c); \
            } while (c!='\n'); \
            PEEK_CHAR(&c); \
        } \
        if (c>' ') break; \
        NEXT_CHAR; \
    } while (42)

#define GET_INT(pi) \
    do { \
        unsigned char c; \
        *(pi)=0; \
        SKIP_WS; \
        while (42) { \
            GET_CHAR(&c); \
            c-='0'; \
            if (c>9) { \
                break; \
            } \
            *(pi)=(*(pi)*10)+c; \
        } \
    } while (0)

FUNC_STATUS WORD pnm_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* If there is no header, we'll report no usage of input */
    *pdwInputUsed = 0;

    /* Parse the header for decode operations. */
    if (!g->fIsEncode) {
        unsigned char c;
        unsigned int maxval;

        GET_CHAR(&c);
        if (c!='P') {
            return IP_INPUT_ERROR;
        }
        GET_CHAR(&c);
        if (c=='4') {
            /* PBM */
            g->traits.iComponentsPerPixel=1;
            g->traits.iBitsPerPixel=1;

        } else if (c=='5') {
            /* PGM */
            g->traits.iComponentsPerPixel=1;
            g->traits.iBitsPerPixel=0;

        } else if (c=='6') {
            /* PPM */
            g->traits.iComponentsPerPixel=3;
            g->traits.iBitsPerPixel=0;

        } else {
            /* "Plain" (all-ASCII) formats (1-3) not (yet) supported. */
            return IP_INPUT_ERROR;
        }

        GET_INT(&g->traits.iPixelsPerRow);
        GET_INT(&g->traits.lNumRows);
        if (!g->traits.iBitsPerPixel) {
            GET_INT(&maxval);
            while (maxval) {
                g->traits.iBitsPerPixel++;
                maxval>>=1;
            }
        }
        g->traits.iBitsPerPixel*=g->traits.iComponentsPerPixel;
        g->dwBytesPerRow = (g->traits.iPixelsPerRow*g->traits.iBitsPerPixel + 7) / 8;
    }

    *pdwInputNextPos = *pdwInputUsed;
    g->dwInNextPos   = *pdwInputUsed;

    *pInTraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * pnm_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

FUNC_STATUS WORD pnm_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);

    *pdwMinInBufLen = *pdwMinOutBufLen = g->dwBytesPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pnm_convert - Converts one row
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_convert (
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
    PPNM_INST g;
    int       nBytes;
    PBYTE     pIn, pOut;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("pnm_convert: Told to flush.\n"), 0, 0);
        *pdwInputUsed = *pdwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        if (g->fIsEncode && !g->fDidHeader) {
            BYTE buffer[MAX_ENCODE_HEADER_SIZE];
            DWORD maxval=(2<<((g->traits.iBitsPerPixel/
                g->traits.iComponentsPerPixel)-1))-1;
            int len;

            INSURE(dwOutputAvail>=MAX_ENCODE_HEADER_SIZE);

            memset(pbOutputBuf,' ',MAX_ENCODE_HEADER_SIZE);
            pbOutputBuf[0]='P';
            if (g->traits.iComponentsPerPixel==1) {
                if (maxval==1) {
                    pbOutputBuf[1]='4';
                } else {
                    pbOutputBuf[1]='5';
                }
            } else if (g->traits.iComponentsPerPixel==3) {
                pbOutputBuf[1]='6';
            } else {
                goto fatal_error;
            }

            snprintf((char *)buffer,MAX_ENCODE_HEADER_SIZE,"\n%d %d\n",
                g->traits.iPixelsPerRow,g->dwRowsDone);
            if (g->traits.iComponentsPerPixel>1 || maxval>1) {
                buffer[MAX_ENCODE_HEADER_SIZE-1]=0;
                len=strlen((char *)buffer);
                snprintf((char *)buffer+len,MAX_ENCODE_HEADER_SIZE-len,
                    "%d\n",maxval);
            }

            buffer[MAX_ENCODE_HEADER_SIZE-1]=0;
            len=strlen((char *)buffer);
            memcpy(pbOutputBuf+MAX_ENCODE_HEADER_SIZE-len,buffer,len);

            *pdwOutputUsed=MAX_ENCODE_HEADER_SIZE;
            *pdwOutputThisPos=0;
            g->dwOutNextPos=MAX_ENCODE_HEADER_SIZE;
            g->fDidHeader=1;
        }
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = g->dwBytesPerRow;
    INSURE (dwInputAvail  >= (DWORD)nBytes );
    INSURE (dwOutputAvail >= (DWORD)nBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;

    /* At this point, pIn is your input buffer, and pOut is your output buffer.
     * Do whatever you are going to do here.
     */
    memcpy(pOut,pIn,nBytes);

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
 * pnm_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * pnm_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_newPage (
    IP_XFORM_HANDLE hXform)
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * pnm_closeXform - Destroys this instance
 *
\*****************************************************************************/

FUNC_STATUS WORD pnm_closeXform (IP_XFORM_HANDLE hXform)
{
    PPNM_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * pnmTbl - Jump-table for transform driver
 *
\*****************************************************************************/
IP_XFORM_TBL pnmDecodeTbl = {
    pnmDecode_openXform,
    pnm_setDefaultInputTraits,
    pnm_setXformSpec,
    pnm_getHeaderBufSize,
    pnm_getActualTraits,
    pnm_getActualBufSizes,
    pnm_convert,
    pnm_newPage,
    pnm_insertedData,
    pnm_closeXform
};

IP_XFORM_TBL pnmEncodeTbl = {
    pnmEncode_openXform,
    pnm_setDefaultInputTraits,
    pnm_setXformSpec,
    pnm_getHeaderBufSize,
    pnm_getActualTraits,
    pnm_getActualBufSizes,
    pnm_convert,
    pnm_newPage,
    pnm_insertedData,
    pnm_closeXform
};

/* End of File */

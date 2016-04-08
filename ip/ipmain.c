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
 * ipmain.c - main control code and entry points for image processor
 *
 *****************************************************************************
 *
 * Mark Overton, Dec 1997
 *
\*****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>    /* for memcpy, memset, etc. */
#include <unistd.h>
#include "hpip.h"
#include "ipdefs.h"

//#define HPIP_DEBUG

#ifdef HPIP_DEBUG
    #include <stdio.h>
    #include <assert.h>

    #define _T(msg) msg

    #define PRINT0(args...) fprintf(stderr, args) 

    #if 0
        #define PRINT1(args...) fprintf(stderr, args)
    #else
        #define PRINT1(args...)
    #endif

    #undef INSURE
    #define INSURE(boolexp) \
        do { if (0) goto fatal_error; assert(boolexp); } while(0)

    int infd;
    int outfd;

#else
    #define PRINT0(args...)
    #define PRINT1(args...)
#endif



/*****************************************************************************\
 *
 * Constants
 *
\*****************************************************************************/


#define CHECK_VALUE    0xACEC0DE4U /* for checking validity of instance struc */
#define MIN_GENBUF_LEN 4000        /* arbitrary, but higher boosts speed some */
#define PERMANENT_RESULTS \
           (IP_INPUT_ERROR | IP_FATAL_ERROR | IP_DONE)



/*****************************************************************************\
 *
 * Types
 *
\*****************************************************************************/


/* XFORM_STATE enum - all possible states of an xform */

typedef enum {
    XS_NONEXISTENT=0,  /* xform is not yet instantiated */
    XS_PARSING_HEADER, /* parsing header (always goes thru this state) */
    XS_CONVERTING,     /* inputting and outputting */
    XS_CONV_NOT_RFD,   /* only outputting; not ready for input */
    XS_FLUSHING,       /* outputting buffered stuff; no more input */
    XS_DONE            /* done and de-instantiated */
} XFORM_STATE;


/* XFORM_INFO type - everything we know about an xform */

typedef struct {
    XFORM_STATE      eState;         /* state of this xform */
    PIP_XFORM_TBL    pXform;         /* ptr to jmp-table for xform */
    LPIP_PEEK_FUNC   pfReadPeek;     /* callback when xform dvr reads data */
    LPIP_PEEK_FUNC   pfWritePeek;    /* callback when xform dvr writes data */
    PVOID            pUserData;      /* Data passed to user in peek functions. */
    DWORD_OR_PVOID   aXformInfo[8];  /* xform-specific information */
    IP_XFORM_HANDLE  hXform;         /* handle for the xform */
    IP_IMAGE_TRAITS  inTraits;       /* traits of input data into xform */
    IP_IMAGE_TRAITS  outTraits;      /* traits of output data from xform */
    DWORD            dwMinInBufLen;  /* min # bytes in input buf */
    DWORD            dwMinOutBufLen; /* min # bytes in output buf */
} XFORM_INFO, *PXFORM_INFO;


/* GENBUF type - a general-purpose buffer which allows one to write or read
 * varying amounts of data.
 */
typedef struct {
    PBYTE pbBuf;        /* ptr to beginning of buffer */
    DWORD dwBufLen;     /* size of entire buffer (# of bytes) */
    DWORD dwValidStart; /* index of first valid data byte in buffer */
    DWORD dwValidLen;   /* number of valid data bytes in buffer */
    DWORD dwFilePos;    /* file-pos of start of valid data (starts at 0) */
} GENBUF, *PGENBUF;


/* INST type - all variables in an instance of the image processor */

typedef struct {

    /* genbufs are used for input into the first xform, and to store output
     * from the last xform. The client's input data is first put into gbIn,
     * which is then fed into the first xform. The last xform's output is put
     * into gbOut, which is then copied out to the client's output buffer.
     */
    GENBUF gbIn;
    GENBUF gbOut;

    /* mid-buffers are simple buffers that handle fixed-length raster-rows
     * passed between xforms in the xform-list. For an xform, there's an input
     * and an output buffer. For the next xform, they swap roles, so the old
     * output buffer becomes the new input buffer, and vice versa. When the
     * roles are swapped, the two pointers below are swapped.
     */
    PBYTE pbMidInBuf;         /* ptr to beginning of input mid-buf */
    PBYTE pbMidOutBuf;        /* ptr to beginning of output mid-buf */
    DWORD dwMidLen;           /* size of either buffer (# of bytes) */
    DWORD dwMidValidLen;      /* # of bytes of good data in input mid-buf */
    int   iOwner;             /* index into xfArray of xform owning (using)
                               * the input mid-buf. negative means no owner
                               * and that pbMidInBuf is empty */

    /* variables pertaining to the array of xforms */

    XFORM_INFO xfArray[IP_MAX_XFORMS]; /* the array of xforms */
    WORD    xfCount;                /* number of xforms */

    /* misc variables */

    DWORD   dwValidChk;       /* struct validity check value */
    DWORD   dwForcedHorizDPI; /* horiz DPI override as 16.16; 0=none */
    DWORD   dwForcedVertDPI;  /* vert  DPI override as 16.16; 0=none */
    WORD    wResultMask;      /* desired ipConvert result bits */
    long    lInRows;          /* number of rows we've input */
    long    lOutRows;         /* number of rows we've output */
    int     iInPages;         /* number of pages we've received */
    int     iOutPages;        /* number of pages we've output */
    BOOL    pendingInsert;    /* ret IP_WRITE_INSERT_OK after outbuf empty? */
        
} INST, *PINST;



/*****************************************************************************\
 *
 * xformJumpTables - Array of ptrs to all driver jump tables
 *
 * Warning: This array is indexed by the enum IP_XFORM.  If one changes,
 *          the other must change too.
 *
\*****************************************************************************/

extern IP_XFORM_TBL faxEncodeTbl, faxDecodeTbl;
extern IP_XFORM_TBL pcxEncodeTbl, pcxDecodeTbl;
/* extern IP_XFORM_TBL bmpEncodeTbl, bmpDecodeTbl; */
extern IP_XFORM_TBL jpgEncodeTbl, jpgDecodeTbl, jpgFixTbl;
extern IP_XFORM_TBL tifEncodeTbl, tifDecodeTbl;
extern IP_XFORM_TBL pnmEncodeTbl, pnmDecodeTbl;
extern IP_XFORM_TBL scaleTbl;
extern IP_XFORM_TBL gray2biTbl, bi2grayTbl;
extern IP_XFORM_TBL colorTbl;
extern IP_XFORM_TBL yXtractTbl;
/* extern IP_XFORM_TBL headerTbl; */
extern IP_XFORM_TBL thumbTbl;
extern IP_XFORM_TBL tableTbl;
extern IP_XFORM_TBL cropTbl;
extern IP_XFORM_TBL tonemapTbl;
extern IP_XFORM_TBL saturationTbl;
extern IP_XFORM_TBL rotateTbl;
extern IP_XFORM_TBL padTbl;
extern IP_XFORM_TBL fakeMonoTbl;
extern IP_XFORM_TBL grayOutTbl;
extern IP_XFORM_TBL changeBPPTbl;
extern IP_XFORM_TBL matrixTbl;
extern IP_XFORM_TBL convolveTbl;
extern IP_XFORM_TBL invertTbl;
extern IP_XFORM_TBL skelTbl;

static IP_XFORM_TBL * const xformJumpTables[] = {
    &faxEncodeTbl, &faxDecodeTbl,  /* MH, MR and MMR formats */
    &pcxEncodeTbl, &pcxDecodeTbl,
    /* &bmpEncodeTbl, &bmpDecodeTbl, */
    &jpgEncodeTbl, &jpgDecodeTbl, &jpgFixTbl,
    &tifEncodeTbl, &tifDecodeTbl,
    &pnmEncodeTbl, &pnmDecodeTbl,
    &scaleTbl,
    &gray2biTbl, &bi2grayTbl,
    &colorTbl,
    &yXtractTbl,
    /* &headerTbl, */
    &thumbTbl,
    &tableTbl,
    &cropTbl,
    &tonemapTbl,
    &saturationTbl,
    &rotateTbl,
    &padTbl,
    &fakeMonoTbl,
    &grayOutTbl,
    &changeBPPTbl,
    &matrixTbl,
    &convolveTbl,
    &invertTbl,
    &skelTbl,
};



/*****************************************************************************\
 *
 * fatalBreakPoint - Called when INSURE fails, used for debugger breakpoint
 *
\*****************************************************************************/

void fatalBreakPoint (void)
{
   /* do nothing */
   PRINT0 (_T("\nhit fatalBreakPoint!\n"));
}



/*****************************************************************************\
 *
 * deleteMidBufs - Frees the two mid-buffers, if they've been allocated
 *
\*****************************************************************************/

static void deleteMidBufs (PINST g)
{
    PRINT0 (_T("deleteMidBufs\n"));

    if (g->pbMidInBuf != NULL)
        IP_MEM_FREE (g->pbMidInBuf);

    if (g->pbMidOutBuf != NULL)
        IP_MEM_FREE (g->pbMidOutBuf);

    g->pbMidInBuf  = NULL;
    g->pbMidOutBuf = NULL;
}



/*****************************************************************************\
 *
 * ipOpen - Opens a new conversion job
 *
\*****************************************************************************/

EXPORT(WORD) ipOpen (
    int             nXforms,      /* in:  number of xforms in lpXforms below */
    LPIP_XFORM_SPEC lpXforms,     /* in:  the xforms we should perform */
    int             nClientData,  /* in:  # bytes of additional client data */
    PIP_HANDLE      phJob)        /* out: handle for conversion job */
{ 
    PINST           g;
    int             i;
    PIP_XFORM_SPEC src;
    PXFORM_INFO     dest;

#ifdef HPIP_DEBUG
    char *ipIn = "/tmp/ipIn.dib";
#endif

    PRINT0 (_T("ipOpen: nXforms=%d\n"), nXforms);
    INSURE (nXforms>0 && lpXforms!=NULL && nClientData>=0 && phJob!=NULL);

#ifdef HPIP_DEBUG
    for (i=0; i<nXforms; i++)
    {
       switch (lpXforms[i].eXform)
       {
          case X_FAX_DECODE:
            PRINT0("Fax_format=%d\n", lpXforms[i].aXformInfo[IP_FAX_FORMAT].dword);
            ipIn = "/tmp/ipIn.pbm";
            break;
          case X_JPG_DECODE:
            PRINT0("JPG_decode=%d\n", lpXforms[i].aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword);
            ipIn = "/tmp/ipIn.jpg";
            break;
          case X_CNV_COLOR_SPACE:
            PRINT0("Color_space conversion=%d\n", lpXforms[i].aXformInfo[IP_CNV_COLOR_SPACE_WHICH_CNV].dword);
            PRINT0("Color_space gamma=%d\n", lpXforms[i].aXformInfo[IP_CNV_COLOR_SPACE_GAMMA].dword);
            break;
          case X_CROP:
            PRINT0("Crop_left=%d\n", lpXforms[i].aXformInfo[IP_CROP_LEFT].dword);
            PRINT0("Crop_right=%d\n", lpXforms[i].aXformInfo[IP_CROP_RIGHT].dword);
            PRINT0("Crop_top=%d\n", lpXforms[i].aXformInfo[IP_CROP_TOP].dword);
            PRINT0("Crop_maxoutrows=%d\n", lpXforms[i].aXformInfo[IP_CROP_MAXOUTROWS].dword);
            break;
          case X_PAD:
            PRINT0("Pad_left=%d\n", lpXforms[i].aXformInfo[IP_PAD_LEFT].dword);
            PRINT0("Pad_right=%d\n", lpXforms[i].aXformInfo[IP_PAD_RIGHT].dword);
            PRINT0("Pad_top=%d\n", lpXforms[i].aXformInfo[IP_PAD_TOP].dword);
            PRINT0("Pad_bottom=%d\n", lpXforms[i].aXformInfo[IP_PAD_BOTTOM].dword);
            PRINT0("Pad_value=%d\n", lpXforms[i].aXformInfo[IP_PAD_VALUE].dword);
            PRINT0("Pad_minheight=%d\n", lpXforms[i].aXformInfo[IP_PAD_MIN_HEIGHT].dword);
            break;
          default:
            PRINT0("Unknown xform\n");
            break;
       }
    }

    infd = creat(ipIn, 0600);
    outfd = creat("/tmp/ipOut.ppm", 0600);
#endif

    /**** Create Instance and Init Misc Variables ****/

    IP_MEM_ALLOC (sizeof(INST) + nClientData, g);
    *phJob = g;

    memset (g, 0, sizeof(INST));
    g->dwValidChk = CHECK_VALUE;
    g->iOwner = -1;
    g->wResultMask = PERMANENT_RESULTS;

    /**** Transfer the Xforms to xfArray ****/

    g->xfCount = (WORD)nXforms;

    for (i=0; i<nXforms; i++) {
        src  = &(lpXforms[i]);
        dest = &(g->xfArray[i]);

        dest->eState = XS_NONEXISTENT;
        dest->pXform = (src->pXform != NULL)
                       ? src->pXform
                       : xformJumpTables[src->eXform];
        INSURE (dest->pXform != NULL);
        dest->pfReadPeek  = src->pfReadPeek;
        dest->pfWritePeek = src->pfWritePeek;
        dest->pUserData   = src->pUserData;
        memcpy (dest->aXformInfo, src->aXformInfo, sizeof(dest->aXformInfo));
    }

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipClose - Destroys the given conversion job, deallocating all its memory
 *
\*****************************************************************************/

EXPORT(WORD) ipClose (IP_HANDLE hJob)
{
    PINST       g;
    PXFORM_INFO pXform;
    WORD        n;

    PRINT0 (_T("ipClose: hJob=%p\n"), (void*)hJob);
    HANDLE_TO_PTR (hJob, g);

    /**** Delete All Buffers ****/

    deleteMidBufs (g);
    g->dwMidLen      = 0;
    g->dwMidValidLen = 0;

    if (g->gbIn.pbBuf  != NULL) IP_MEM_FREE (g->gbIn.pbBuf);
    if (g->gbOut.pbBuf != NULL) IP_MEM_FREE (g->gbOut.pbBuf);

    /**** Delete All Xform Instances ****/

    for (n=0; n<g->xfCount; n++) {
        pXform = &(g->xfArray[n]);
        if (pXform->hXform != NULL)
            pXform->pXform->closeXform (pXform->hXform);
    }

    IP_MEM_FREE (g);   /* Delete our instance, and we're done */

#ifdef HPIP_DEBUG
    close(infd); 
    close(outfd); 
#endif

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipGetClientDataPtr - Returns ptr to client's data in conversion instance
 *
\*****************************************************************************/

EXPORT(WORD) ipGetClientDataPtr (
    IP_HANDLE  hJob,            /* in:  handle to conversion job */
    PVOID     *ppvClientData)   /* out: ptr to client's memory area */
{
    PINST g;

    PRINT0 (_T("ipGetClientDataPtr\n"));
    HANDLE_TO_PTR (hJob, g);
    *ppvClientData = (PVOID)((PBYTE)g + sizeof(INST));
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * ipResultMask - Selects bit-results to be returned by ipConvert
 *
\****************************************************************************/

EXPORT(WORD) ipResultMask (
    IP_HANDLE  hJob,     /* in:  handle to conversion job */
    WORD       wMask)    /* in:  result bits you are interested in */
{
    PINST g;

    PRINT0 (_T("ipResultMask: hJob=%p, wMask=%04x\n"), (void*)hJob, wMask);
    HANDLE_TO_PTR (hJob, g);
    g->wResultMask = wMask | PERMANENT_RESULTS;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipSetDefaultInputTraits - Specifies default input image traits
 *
 *****************************************************************************
 *
 * The header of the file-type handled by the first transform might not
 * include *all* the image traits we'd like to know.  Those not specified
 * in the file-header are filled in from info provided by this routine.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipSetDefaultInputTraits (
    IP_HANDLE         hJob,       /* in: handle to conversion job */
    LPIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PINST g;
    PIP_IMAGE_TRAITS p;

    PRINT0 (_T("ipSetDefaultInputTraits: hJob=%p PixelsPerRow=%d BitsPerPixel=%d ComponentsPerPixel=%d HorzDPI=%ld VertDPI=%ld Rows=%ld Pages=%d PageNum=%d\n"), 
                       (void*)hJob, pTraits->iPixelsPerRow, pTraits->iBitsPerPixel, pTraits->iComponentsPerPixel, pTraits->lHorizDPI, 
                        pTraits->lVertDPI, pTraits->lNumRows, pTraits->iNumPages, pTraits->iPageNum);
    HANDLE_TO_PTR (hJob, g);
    INSURE (g->xfArray[0].eState == XS_NONEXISTENT);
    g->xfArray[0].inTraits = *pTraits;   /* a structure copy */

    /* Convert DPI from integer to 16.16 fixed-pt if necessary */
    p = &(g->xfArray[0].inTraits);
    if (p->lHorizDPI < 0x10000) p->lHorizDPI <<= 16;
    if (p->lVertDPI  < 0x10000) p->lVertDPI  <<= 16;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipGetImageTraits - Returns traits of input and output images
 *
 *****************************************************************************
 *
 * After the conversion job is done, these traits will contain the actual
 * number of rows input and output (ipConvert patches traits upon completion).
 *
\*****************************************************************************/

EXPORT(WORD) ipGetImageTraits (
    IP_HANDLE         hJob,          /* in:  handle to conversion job */
    LPIP_IMAGE_TRAITS pInputTraits,  /* out: traits of input image */
    LPIP_IMAGE_TRAITS pOutputTraits) /* out: traits of output image */
{
    PINST       g;
    PXFORM_INFO pTail;

    PRINT0 (_T("ipGetImageTraits: hJob=%p\n"), (void*)hJob);
    HANDLE_TO_PTR (hJob, g);
    INSURE (g->xfCount > 0);
    pTail = &(g->xfArray[g->xfCount-1]);

    if (pInputTraits != NULL) {
        INSURE (g->xfArray[0].eState > XS_PARSING_HEADER);
        *pInputTraits = g->xfArray[0].inTraits;
        PRINT0 (_T("InputTraits: hJob=%p PixelsPerRow=%d BitsPerPixel=%d ComponentsPerPixel=%d HorzDPI=%ld VertDPI=%ld Rows=%ld Pages=%d PageNum=%d\n"), 
                       (void*)hJob, pInputTraits->iPixelsPerRow, pInputTraits->iBitsPerPixel, pInputTraits->iComponentsPerPixel, pInputTraits->lHorizDPI, 
                        pInputTraits->lVertDPI, pInputTraits->lNumRows, pInputTraits->iNumPages, pInputTraits->iPageNum);
    }

    if (pOutputTraits != NULL) {
        INSURE (pTail->eState > XS_PARSING_HEADER);
        *pOutputTraits = pTail->outTraits;
        PRINT0 (_T("OutputTraits: hJob=%p PixelsPerRow=%d BitsPerPixel=%d ComponentsPerPixel=%d HorzDPI=%ld VertDPI=%ld Rows=%ld Pages=%d PageNum=%d\n"), 
                       (void*)hJob, pOutputTraits->iPixelsPerRow, pOutputTraits->iBitsPerPixel, pOutputTraits->iComponentsPerPixel, pOutputTraits->lHorizDPI, 
                        pOutputTraits->lVertDPI, pOutputTraits->lNumRows, pOutputTraits->iNumPages, pOutputTraits->iPageNum);
    }

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipInsertedData - Client inserted some bytes into our output stream
 *
\*****************************************************************************/

EXPORT(WORD) ipInsertedData (
    IP_HANDLE  hJob,         /* in: handle to conversion job */
    DWORD      dwNumBytes)   /* in: # of bytes of additional data written */
{
    PINST       g;
    PXFORM_INFO pTail;

    PRINT0 (_T("ipInsertedData: hJob=%p, dwNumBytes=%d\n"), (void*)hJob, dwNumBytes);
    HANDLE_TO_PTR (hJob, g);
    INSURE (g->xfCount > 0);
    pTail = &(g->xfArray[g->xfCount-1]);
    INSURE (pTail->eState > XS_PARSING_HEADER);
    INSURE (g->gbOut.dwValidLen == 0);   /* output genbuf must be empty */

    pTail->pXform->insertedData (pTail->hXform, dwNumBytes);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipOverrideDPI - Force a different DPI to be reported in the output
 *
\*****************************************************************************/

EXPORT(WORD) ipOverrideDPI (
    IP_HANDLE hJob,        /* in: handle to conversion job */
    DWORD     dwHorizDPI,  /* in: horiz DPI as 16.16; 0 means no override */
    DWORD     dwVertDPI)   /* in: vert  DPI as 16.16; 0 means no override */
{
    PINST g;

    PRINT0 (_T("ipOverrideDPI: dwHorizDPI=%x, dwVertDPI=%x\n"),
            dwHorizDPI, dwVertDPI);
    HANDLE_TO_PTR (hJob, g);

    /* Convert from integer to fixed-pt if necessary */
    if (dwHorizDPI < 0x10000) dwHorizDPI <<= 16;
    if (dwVertDPI  < 0x10000) dwVertDPI  <<= 16;

    g->dwForcedHorizDPI = dwHorizDPI;
    g->dwForcedVertDPI  = dwVertDPI;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipGetFuncPtrs - Loads jump-table with pointers to the ip entry points
 *
\*****************************************************************************/

EXPORT(WORD) ipGetFuncPtrs (LPIP_JUMP_TBL lpJumpTbl)
{
    PRINT0 (_T("ipGetFuncPtrs\n"));
    INSURE (lpJumpTbl!=NULL && lpJumpTbl->wStructSize==sizeof(IP_JUMP_TBL));

    lpJumpTbl->ipOpen                  = (LPVOID) ipOpen;
    lpJumpTbl->ipConvert               = (LPVOID) ipConvert;
    lpJumpTbl->ipClose                 = (LPVOID) ipClose;
    lpJumpTbl->ipGetClientDataPtr      = (LPVOID) ipGetClientDataPtr;
    lpJumpTbl->ipResultMask            = (LPVOID) ipResultMask;
    lpJumpTbl->ipSetDefaultInputTraits = (LPVOID) ipSetDefaultInputTraits;     
    lpJumpTbl->ipGetImageTraits        = (LPVOID) ipGetImageTraits;
    lpJumpTbl->ipInsertedData          = (LPVOID) ipInsertedData;
    lpJumpTbl->ipOverrideDPI           = (LPVOID) ipOverrideDPI;
    lpJumpTbl->ipGetOutputTraits       = (LPVOID) ipGetOutputTraits;

    return IP_DONE;    

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipGetOutputTraits - Returns the output traits before ipConvert is called
 *
 *****************************************************************************
 *
 * If the first xform does not have a header, then you can call this function
 * *after* calling ipSetDefaultInputTraits to get the output image traits.
 * Ordinarily, you'd have to call ipConvert a few times and wait until it tells
 * you that the (non-existent) header has been parsed.  But if you need the
 * output traits before calling ipConvert, this function will return them.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipGetOutputTraits (
    IP_HANDLE         hJob,      /* in:  handle to conversion job */
    LPIP_IMAGE_TRAITS pTraits)   /* out: output image traits */
{
    PINST           g;
    IP_IMAGE_TRAITS inTraits, outTraits;
    int        iXform;
    PXFORM_INFO     pXform;
    WORD        result;
    DWORD        dwHeaderLen;
    DWORD        dwInUsed, dwInNextPos;

    HANDLE_TO_PTR (hJob, g);
    INSURE (g->xfCount>=1);
    inTraits = g->xfArray[0].inTraits;  /* set by SetDefaultInputTraits */

    for (iXform=0; iXform<g->xfCount; iXform++) {
        pXform = &(g->xfArray[iXform]);
        INSURE (pXform->eState == XS_NONEXISTENT);

        /* Open the xform, set it up, get the traits, and close it */

        result = pXform->pXform->openXform (&pXform->hXform);
        INSURE (result == IP_DONE);

        result = pXform->pXform->setDefaultInputTraits (
                    pXform->hXform, &inTraits);
        INSURE (result == IP_DONE);

        result = pXform->pXform->setXformSpec (
                    pXform->hXform, pXform->aXformInfo);
        INSURE (result == IP_DONE);

        result = pXform->pXform->getHeaderBufSize (
                    pXform->hXform, &dwHeaderLen);
        INSURE (result == IP_DONE);
        INSURE (dwHeaderLen == 0);

        result = pXform->pXform->getActualTraits (
                    pXform->hXform,
                    0, NULL, &dwInUsed, &dwInNextPos,
                    &inTraits, &outTraits);
        INSURE (result & IP_DONE);

        result = pXform->pXform->closeXform (pXform->hXform);
        INSURE (result == IP_DONE);

        inTraits = outTraits;
        pXform->hXform = NULL;
    }

    *pTraits = outTraits;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipConvert - Converts some input data (work-horse function)
 *
\*****************************************************************************/

EXPORT(WORD) ipConvert (
    IP_HANDLE  hJob,              /* in:  handle to conversion job */
    DWORD      dwInputAvail,      /* in:  # avail bytes in input buf */
    PBYTE      pbInputBuf,        /* in:  ptr to input buffer */
    PDWORD     pdwInputUsed,      /* out: # bytes used from input buf */
    PDWORD     pdwInputNextPos,   /* out: file-pos to read from next */
    DWORD      dwOutputAvail,     /* in:  # avail bytes in output buf */
    PBYTE      pbOutputBuf,       /* in:  ptr to output buffer */
    PDWORD     pdwOutputUsed,     /* out: # bytes written in out buf */
    PDWORD     pdwOutputThisPos)  /* out: file-pos to write this data */
{
    PINST       g;
    WORD        ipResult;
    int         iXform;
    WORD        result;
    PGENBUF     pgbIn, pgbOut;
    BOOL        atTheHead, atTheTail;
    PXFORM_INFO pXform, pPriorXform, pNextXform;
    BOOL        needInput, selectCnvState;
    DWORD       dwBytes;
    int         i;
    DWORD       n;
    DWORD       dwInUsed, dwOutUsed;
    DWORD       dwInNextPos, dwOutThisPos;
    DWORD       dwTheInLen, dwTheOutLen;
    PBYTE       pbTemp;
    PBYTE       pbTheInBuf, pbTheOutBuf;
    DWORD       dwJunk;

    if (pbOutputBuf == NULL) {
        /* client wants us to discard all data */
        pdwOutputUsed    = &dwJunk;
        pdwOutputThisPos = &dwJunk;
        dwOutputAvail    = 0xfffffffu;
    }

    PRINT0 (_T("ipConvert: hJob=%p, pbInputBuf=%p InputBufSize=%d pbOutputBuf=%p OutputBufSize=%d\n"),
                             (void*)hJob, pbInputBuf, dwInputAvail, pbOutputBuf, dwOutputAvail);
    INSURE (pdwInputUsed !=NULL && pdwInputNextPos !=NULL &&
            pdwOutputUsed!=NULL && pdwOutputThisPos!=NULL);
    HANDLE_TO_PTR (hJob, g);
    INSURE (g->xfCount>=1);

    pgbIn  = &g->gbIn;
    pgbOut = &g->gbOut;

    *pdwInputUsed  = 0;
    *pdwOutputUsed = 0;
    ipResult = 0;

      /**************************/
     /* Beginning of Main Loop */
    /**************************/

    while (TRUE) {

    /**** Output any data in the output genbuf ****/

    if (*pdwOutputUsed == 0)
        *pdwOutputThisPos = pgbOut->dwFilePos;

    if (pgbOut->dwValidLen != 0) {   /* output genbuf is not empty */
        /* Logic below:
         *
         * 1. If next output file-pos doesn't match output genbuf, exit loop.
         *
         * 2. Copy as much as possible from output genbuf to clients output buf.
         *      2.1 decide number of bytes to copy
         *      2.2 copy them
         *      2.3 update misc variables
         *
         * 3. If output genbuf is not empty, exit loop (client's buf is full),
         *    else set wValidStart to 0 because output genbuf is empty.
         */

        if ((*pdwOutputThisPos+*pdwOutputUsed) != pgbOut->dwFilePos) {
            PRINT0 (_T("ipConvert: output seek to %d\n"), pgbOut->dwFilePos);
            goto exitLoop;
        }

        dwBytes = pgbOut->dwValidLen;
        if (dwOutputAvail < dwBytes)
            dwBytes = dwOutputAvail;

        if (pbOutputBuf != NULL) {
            memcpy (pbOutputBuf + *pdwOutputUsed,
                    pgbOut->pbBuf + pgbOut->dwValidStart,
                    dwBytes);
        }

        *pdwOutputUsed       += dwBytes;
        dwOutputAvail        -= dwBytes;
        pgbOut->dwValidStart += dwBytes;
        pgbOut->dwFilePos    += dwBytes;
        pgbOut->dwValidLen   -= dwBytes;

        if (pgbOut->dwValidLen != 0)
            goto exitLoop;

        pgbOut->dwValidStart = 0;   /* output genbuf is now empty */
    }

    if (g->pendingInsert) {
        g->pendingInsert = FALSE;
        ipResult |= IP_WRITE_INSERT_OK;
    }

    /**** If ipResult has any returnable bits set, exit loop now ****/

    if (ipResult & g->wResultMask)
        goto exitLoop;

    /**** Select an xform to run ****/

    /* select the owner of input midbuf, or negative means 'none selected' */
    iXform = g->iOwner;

    if (iXform < 0) {
        for (i=g->xfCount-1; i>=0; i--) {
            if (g->xfArray[i].eState == XS_CONV_NOT_RFD) {
                /* select last xform that's convnotrfd; this xform
                 * is outputting (or thinking) but not inputting */
                iXform = i;
                break;
            }
        }
    }

    if (iXform < 0) {
        for (i=0; i<g->xfCount; i++) {
            if (g->xfArray[i].eState != XS_DONE) {
                /* select first xform that's not done */
                iXform = i;
                break;
            }
        }
    }

    if (iXform < 0) {
        /* all xforms are done: discard all input, and report that we're done */
        *pdwInputUsed = dwInputAvail;
        ipResult |= IP_DONE;
        /* patch the last outtraits with actual # of rows output, so that
         * ipGetImageTraits will return actual row-count after we're done */
        g->xfArray[g->xfCount-1].outTraits.lNumRows = g->lOutRows;
        /* patch first intraits for the same reason */
        g->xfArray[0].inTraits.lNumRows = g->lInRows;
        goto exitLoop;
    }

    /**** Miscellaneous preparation ****/

    pXform = &(g->xfArray[iXform]);
    atTheHead = (iXform == 0);
    atTheTail = (iXform == g->xfCount-1);
    pPriorXform = atTheHead ? NULL : &(g->xfArray[iXform-1]);
    pNextXform  = atTheTail ? NULL : &(g->xfArray[iXform+1]);

    /**** Create the Xform if necessary ****/

    if (pXform->eState == XS_NONEXISTENT) {
        PRINT0 (_T("ipConvert: creating xform %d\n"), iXform);
        INSURE (atTheHead || pPriorXform->eState>=XS_CONVERTING);
        
        if (atTheHead) {
            /* this xform's input traits were set by ipSetDefaultInputTraits */
        } else {
            /* this xform's input traits are prior xform's output traits */
            memcpy (&pXform->inTraits, &pPriorXform->outTraits,
                    sizeof(IP_IMAGE_TRAITS));

            if (atTheTail) {
                /* apply any DPI overrides */
                if (g->dwForcedHorizDPI != 0)
                    pXform->inTraits.lHorizDPI = (long)g->dwForcedHorizDPI;
                if (g->dwForcedVertDPI != 0)
                    pXform->inTraits.lVertDPI  = (long)g->dwForcedVertDPI;
            }
        }
        
        result = pXform->pXform->openXform (&pXform->hXform);
        INSURE (result == IP_DONE);
        result = pXform->pXform->setDefaultInputTraits (
                    pXform->hXform, &pXform->inTraits);
        INSURE (result == IP_DONE);
        result = pXform->pXform->setXformSpec (
                    pXform->hXform, pXform->aXformInfo);
        INSURE (result == IP_DONE);
        result = pXform->pXform->getHeaderBufSize (
                    pXform->hXform, &pXform->dwMinInBufLen);
        INSURE (result == IP_DONE);

        if (! atTheHead) {
            /* a header is only allowed on first xform */
            INSURE (pXform->dwMinInBufLen == 0);
        } else {
            /* allocate the input genbuf */
            if (pXform->dwMinInBufLen == 0)
                pXform->dwMinInBufLen = 1;
            PRINT0 (_T("ipConvert: alloc input genbuf, len=%d\n"),
                pXform->dwMinInBufLen);
            IP_MEM_ALLOC (pXform->dwMinInBufLen, pgbIn->pbBuf);
            pgbIn->dwBufLen     = pXform->dwMinInBufLen;
            pgbIn->dwValidStart = 0;
            pgbIn->dwValidLen   = 0;
            pgbIn->dwFilePos    = 0;
        }

        pXform->eState = XS_PARSING_HEADER;
    }

    /**** Enter flushing state if appropriate ****/

    if (pXform->eState == XS_CONVERTING &&
        (( atTheHead && pbInputBuf==NULL && pgbIn->dwValidLen==0) ||
         (!atTheHead && pPriorXform->eState==XS_DONE && g->iOwner<0))) {
        /* there will never be any more input to this xform: start flushing */
        PRINT0 (_T("ipConvert: xform %d is now flushing\n"), iXform);
        pXform->eState = XS_FLUSHING;
    }

    /**** Check that input data and output space are available ****/

    needInput = (pXform->eState==XS_PARSING_HEADER ||
                 pXform->eState==XS_CONVERTING);

    if (needInput) {
        if (! atTheHead) {
            /* the input midbuf must contain data */
            INSURE (g->iOwner>=0 && g->dwMidValidLen>0);
            PRINT1 (_T("not at head, pixels = %08x\n"), *(DWORD*)(g->pbMidInBuf));
        } else if (pbInputBuf != NULL) {
            DWORD dwUnusedStart;

            /* left-justify data in input genbuf if necessary */

            if (pgbIn->dwBufLen-pgbIn->dwValidStart < pXform->dwMinInBufLen) {
                /* too much wasted space on left end, so left justify */
                memmove (pgbIn->pbBuf, pgbIn->pbBuf + pgbIn->dwValidStart,
                         pgbIn->dwValidLen);
                pgbIn->dwValidStart = 0;
                PRINT1 (_T("left just, pixels = %08x\n"), *(DWORD*)(pgbIn->pbBuf));
            }

            /* put as much client input as possible into the input genbuf */

            dwUnusedStart = pgbIn->dwValidStart + pgbIn->dwValidLen;
            dwBytes = pgbIn->dwBufLen - dwUnusedStart;
            if (dwBytes > dwInputAvail)
                dwBytes = dwInputAvail;

            memcpy (pgbIn->pbBuf + dwUnusedStart,
                    pbInputBuf + *pdwInputUsed,
                    dwBytes);

            pgbIn->dwValidLen += dwBytes;
            *pdwInputUsed     += dwBytes;
            dwInputAvail      -= dwBytes;
            *pdwInputNextPos  += dwBytes;

            /* if input genbuf has insufficient data, exit loop */
            if (pgbIn->dwValidLen < pXform->dwMinInBufLen)
                goto exitLoop;
            PRINT1 (_T("at head, pixels = %08x\n"), *(DWORD*)(pgbIn->pbBuf));
        }
    }

    if (atTheTail && pXform->eState!=XS_PARSING_HEADER) {
        /* output might be produced; output genbuf must be empty */
        INSURE (pgbOut->dwValidLen == 0);
    }

    /**** Call the Conversion Routine ****/

    pbTheInBuf   = atTheHead ? pgbIn->pbBuf + pgbIn->dwValidStart : g->pbMidInBuf;
    dwTheInLen   = atTheHead ? pgbIn->dwValidLen : g->dwMidValidLen;
    pbTheOutBuf  = atTheTail ? pgbOut->pbBuf : g->pbMidOutBuf;
    dwTheOutLen  = atTheTail ? pgbOut->dwBufLen : g->dwMidLen;

    if (pXform->eState == XS_PARSING_HEADER) {
        result = pXform->pXform->getActualTraits (
                    pXform->hXform,
                    dwTheInLen, pbTheInBuf, &dwInUsed, &dwInNextPos,
                    &pXform->inTraits,
                    &pXform->outTraits);
        dwOutUsed = 0;
        dwOutThisPos = 0;
    } else {
        if (pXform->eState == XS_FLUSHING)
            pbTheInBuf = NULL;
        result = pXform->pXform->convert (
                    pXform->hXform,
                    dwTheInLen,  pbTheInBuf,  &dwInUsed,  &dwInNextPos,
                    dwTheOutLen, pbTheOutBuf, &dwOutUsed, &dwOutThisPos);
    }

    PRINT1 (_T("ipConvert: xform %d returned %04x\n"), iXform, result);
    PRINT1 (_T("ipConvert:         consumed %d and produced %d bytes\n"),
           dwInUsed, dwOutUsed);

    if (pbTheOutBuf != NULL)
        PRINT1 (_T("ipConvert: out data = %08x\n"), *(DWORD*)pbTheOutBuf);

    INSURE ((result & IP_FATAL_ERROR) == 0);

    /**** Update Input and Output Buffers ****/

    if (dwInUsed > 0) {
        if (pXform->pfReadPeek != NULL) {
            #if defined _WIN32
                __try {
            #endif
            pXform->pfReadPeek (hJob, &(pXform->inTraits),
                                dwInUsed, pbTheInBuf, pgbIn->dwFilePos, 
                                pXform->pUserData);
            #if defined _WIN32
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    goto fatal_error;
                }
            #endif
        }

        if (! atTheHead) {
            /* We _assume_ that the xform consumed all the data in the midbuf */
            g->iOwner = -1;   /* input midbuf is consumed and un-owned now */
            g->dwMidValidLen = 0;
        }
    }

    if (needInput && atTheHead) {
        if (dwInUsed >= pgbIn->dwValidLen) {
            /* consumed all input; mark input genbuf as empty */
            pgbIn->dwValidLen   = 0;
            pgbIn->dwValidStart = 0;
            pgbIn->dwFilePos    = dwInNextPos;
        } else {
            /* advance counters in genbuf */
            pgbIn->dwValidLen   -= dwInUsed;
            pgbIn->dwValidStart += dwInUsed;
            pgbIn->dwFilePos    += dwInUsed;
        }

        /* if new genbuf file-pos doesn't match what xform wants,
         * discard remainder of buffer */
        if (pgbIn->dwFilePos != dwInNextPos) {
            PRINT0 (_T("ipConvert: input seek to %d\n"), dwInNextPos);
            pgbIn->dwValidLen   = 0;
            pgbIn->dwValidStart = 0;
            pgbIn->dwFilePos    = dwInNextPos;
        }
    }

    if (dwOutUsed > 0) {
        if (pXform->pfWritePeek != NULL) {
            #if defined _WIN32
                __try {
            #endif
            pXform->pfWritePeek (hJob, &(pXform->outTraits),
                                 dwOutUsed, pbTheOutBuf, dwOutThisPos,
                                 pXform->pUserData);
            #if defined _WIN32
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    goto fatal_error;
                }
            #endif
        }

        if (atTheTail) {
            pgbOut->dwFilePos    = dwOutThisPos;
            pgbOut->dwValidStart = 0;
            pgbOut->dwValidLen   = dwOutUsed;
        } else {
            INSURE (g->iOwner < 0);   /* mid inbuf must be unowned here */
            g->iOwner = iXform + 1;   /* next xform hereby owns mid inbuf */
            /* swap input and output midbuf pointers */
            pbTemp = g->pbMidInBuf;
            g->pbMidInBuf    = g->pbMidOutBuf;
            g->pbMidOutBuf   = pbTemp;
            g->dwMidValidLen = dwOutUsed;
        }
    }

    /**** Handle Results of Conversion Call ****/

    selectCnvState = FALSE;

    if (pXform->eState == XS_PARSING_HEADER) {

        if (result & IP_DONE) {
            PRINT0 (_T("ipConvert: xform %d is done parsing header\n"), iXform);
            pXform->pXform->getActualBufSizes (pXform->hXform,
                &pXform->dwMinInBufLen, &pXform->dwMinOutBufLen);

            if (atTheHead) {
                /* allocate new input genbuf, and xfer data into it */
                n = pXform->dwMinInBufLen;
                if (n < MIN_GENBUF_LEN)
                    n = MIN_GENBUF_LEN;
                if (n < pgbIn->dwValidLen)
                    n = pgbIn->dwValidLen;
                PRINT0 (_T("ipConvert: alloc new input genbuf, ")
                       _T("old len=%d, new len=%d\n"), pgbIn->dwBufLen, n);
                PRINT0 (_T("   dwMinInBufLen=%d, dwValidLen=%d\n"),
                    pXform->dwMinInBufLen, pgbIn->dwValidLen);
                IP_MEM_ALLOC (n, pbTemp);
                memcpy (pbTemp,
                        pgbIn->pbBuf + pgbIn->dwValidStart,
                        pgbIn->dwValidLen);
                IP_MEM_FREE (pgbIn->pbBuf);
                pgbIn->pbBuf        = pbTemp;
                pgbIn->dwBufLen     = n;
                pgbIn->dwValidStart = 0;
            }

            /* boost size of midbufs if necessary (also 1st malloc of them) */
            n = atTheHead ? 0 : pXform->dwMinInBufLen;
            if (!atTheTail && pXform->dwMinOutBufLen > n)
                n = pXform->dwMinOutBufLen;
            /* note: the code below (correctly) does not create mid-bufs if
             * n is 0, which occurs if there's only one xform in the list */
            if (n > g->dwMidLen) {
                /* delete both mid-bufs, and (re) allocate them */
                /* copy data from old to new, if necessary */
                PBYTE pbOldMidInBuf = g->pbMidInBuf;
                g->pbMidInBuf = NULL;
                PRINT0 (_T("ipConvert: alloc mid-bufs, old len=%d, new len=%d\n"),
                        g->dwMidLen, n);
                deleteMidBufs (g);
                IP_MEM_ALLOC (n, g->pbMidInBuf );
                IP_MEM_ALLOC (n, g->pbMidOutBuf);
                if (pbOldMidInBuf != NULL) {
                    memcpy (g->pbMidInBuf, pbOldMidInBuf, g->dwMidLen);
                    IP_MEM_FREE (pbOldMidInBuf);
                }
                g->dwMidLen = n;
            }

            if (atTheTail) {
                /* allocate output genbuf */
                n = pXform->dwMinOutBufLen;
                if (n < MIN_GENBUF_LEN)
                    n = MIN_GENBUF_LEN;
                PRINT0 (_T("ipConvert: alloc output genbuf, len=%d, minlen=%d\n"),
                     n, pXform->dwMinOutBufLen);
                IP_MEM_ALLOC (n, pgbOut->pbBuf);
                pgbOut->dwBufLen     = n;
                pgbOut->dwValidStart = 0;
                pgbOut->dwValidLen   = 0;

                /* At this point it is permissible to call ipGetImageTraits
                 * to obtain output traits because all xforms are past the
                 * parsing-header state, and thus the output traits are known.
                 * So tell caller that we're done parsing the header.  */
                ipResult |= IP_PARSED_HEADER;
            }

            selectCnvState = TRUE;
        }

    } else {    /* state is XS_CONVERTING or XS_CONV_NOT_RFD or XS_FLUSHING */

        if (atTheHead) {
            /* handle status bits pertaining to the input data */
            if (result & IP_CONSUMED_ROW) {
               g->lInRows += 1;
               ipResult |= IP_CONSUMED_ROW;
            }
            if (result & IP_NEW_OUTPUT_PAGE) {
                /* a new *output* page for the input xform is a new *input* page
                 * for the IP as a whole */
                g->iInPages += 1;
                ipResult |= IP_NEW_INPUT_PAGE;
            }
        }

        if (atTheTail) {
            /* handle status bits pertaining to the output data */
            ipResult |= (result & (IP_PRODUCED_ROW | IP_NEW_OUTPUT_PAGE));
            if (result & IP_PRODUCED_ROW)
                g->lOutRows += 1;
            if (result & IP_NEW_OUTPUT_PAGE)
                g->iOutPages += 1;
            if (result & IP_WRITE_INSERT_OK & g->wResultMask)
                g->pendingInsert = TRUE;
        } else if (result & IP_NEW_OUTPUT_PAGE) {
            /* this xform hit end of page, so tell next xform about it */
            PRINT0 (_T("ipConvert: xform %d hit end of page\n"), iXform);
            pNextXform->pXform->newPage (pNextXform->hXform);
        }

        if (result & IP_DONE) {
            PRINT0 (_T("ipConvert: xform %d is done\n"), iXform);
            pXform->eState = XS_DONE;
        } else if (pXform->eState != XS_FLUSHING)
            selectCnvState = TRUE;
    } /* if state is 'parsing header' */

    if (selectCnvState) {
        /* go to one of the two 'converting' states */
        if ((result & IP_READY_FOR_DATA) == 0)
            PRINT1 (_T("ipConvert: xform %d is not ready for data\n"), iXform);
        pXform->eState = (result & IP_READY_FOR_DATA)
                         ? XS_CONVERTING : XS_CONV_NOT_RFD;
    }

    }  /* end of while (TRUE) */
    exitLoop: ;

      /********************/
     /* End of Main Loop */
    /********************/

    *pdwInputNextPos = pgbIn->dwFilePos + pgbIn->dwValidLen;

    /* After headers are parsed, parsed-header bit should always be set */
    if (g->xfArray[g->xfCount-1].eState >= XS_CONVERTING)
        ipResult |= IP_PARSED_HEADER;

    PRINT0 (_T("ipConvert: ipResult=%04x, returning %04x, InputUsed=%d InputNextPos=%d OutputUsed=%d OutputThisPos=%d\n"),
            ipResult, ipResult & g->wResultMask, *pdwInputUsed, *pdwInputNextPos, *pdwOutputUsed, *pdwOutputThisPos);

#ifdef HPIP_DEBUG
    if (pbInputBuf && *pdwInputUsed)
       write(infd, pbInputBuf, *pdwInputUsed); 

    if (*pdwOutputUsed)
       write(outfd, pbOutputBuf, *pdwOutputUsed); 
#endif

    return ipResult & g->wResultMask;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * ipMirrorBytes - Swaps bits in each byte of buffer
 *                 (bits 0<->7, 1<->6, etc.)
 *
\*****************************************************************************/

static const BYTE baMirrorImage[256] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

VOID ipMirrorBytes(PBYTE pbInputBuf,DWORD dwInputAvail) {
    while (dwInputAvail>0) {
        *pbInputBuf=baMirrorImage[*pbInputBuf];
        pbInputBuf++;
        dwInputAvail--;
    }
}

/* End of File */

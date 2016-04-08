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
 * hpojip.h - Interface into the Image Processing module
 *
 *****************************************************************************
 *
 * Mark Overton, Dec 1997
 *
\*****************************************************************************/


#if !defined HPIP_INC
#define HPIP_INC

#if defined(__cplusplus)
	extern "C" {
#endif

#include <stdlib.h>

/* TODO: Fix this! */
// #include <endian.h>
#undef LITTLE_ENDIAN

#define FAR
#define WINAPI
#define EXPORT(x) x

typedef unsigned char BYTE, *PBYTE, FAR *LPBYTE;
typedef unsigned short WORD, *PWORD, FAR *LPWORD;
typedef unsigned short USHORT, *PUSHORT, FAR *LPUSHORT;
typedef unsigned int DWORD, *PDWORD, FAR *LPDWORD;
typedef unsigned int UINT, *PUINT, FAR *LPUINT;
typedef unsigned long ULONG, *PULONG, FAR *LPULONG;
typedef enum { FALSE=0, TRUE=1 } BOOL;
typedef void VOID, *PVOID, FAR *LPVOID;
typedef long long int __int64;

typedef struct {
	BYTE rgbRed:8;
	BYTE rgbGreen:8;
	BYTE rgbBlue:8;
} __attribute__((packed)) RGBQUAD;

/****************************************************************************\
 ****************************************************************************
 *
 * COMMON DEFINITIONS for both xform drivers and ip functions
 *
 ****************************************************************************
\****************************************************************************/

#define IP_MAX_XFORMS	20	/* Max number of xforms we can handle. */

/* These bit-values are returned by all transform driver and ip functions.
 * Zero or more of these bits is set, telling you if anything interesting
 * happened.
 */
#define IP_READY_FOR_DATA      0X0001u
#define IP_PARSED_HEADER       0x0002u
#define IP_CONSUMED_ROW        0x0004u
#define IP_PRODUCED_ROW        0x0008u
#define IP_INPUT_ERROR         0x0010u
#define IP_FATAL_ERROR         0x0020u
#define IP_NEW_INPUT_PAGE      0x0040u
#define IP_NEW_OUTPUT_PAGE     0x0080u
#define IP_WRITE_INSERT_OK     0x0100u
#define IP_DONE                0x0200u


/* IP_IMAGE_TRAITS describes everything about an image.  If an item is not
 * known, a negative number is used, meaning "I don't know."  So all items
 * are signed.
 */
typedef struct {
    int  iPixelsPerRow;
    int  iBitsPerPixel;
    int  iComponentsPerPixel;
    long lHorizDPI;           /* 32.0 or 16.16 fixed-point */
    long lVertDPI;            /* 32.0 or 16.16 fixed-point */
    long lNumRows;
    int  iNumPages;
    int  iPageNum;
} IP_IMAGE_TRAITS, *PIP_IMAGE_TRAITS, FAR*LPIP_IMAGE_TRAITS;

typedef union {
	DWORD dword;
	PVOID pvoid;
	__int64 i64;
	RGBQUAD rgbquad;
	float fl;
} DWORD_OR_PVOID;

//#ifdef HPIP_INTERNAL
#include "xform.h"   // this file uses the above definitions
//#else
//typedef struct IP_XFORM_TBL_s FAR *LPIP_XFORM_TBL;
//#endif


/****************************************************************************\
 ****************************************************************************
 *
 * Definitions for Exported IP Routines
 *
 ****************************************************************************
\****************************************************************************/


/* Synopsis of the interface:
 *
 *    Main routines:
 *
 *       ipOpen             - opens a new conversion-job
 *       ipConvert          - converts some data
 *       ipClose            - closes the conversion-job
 *
 *    Ancillary routines:
 *
 *       ipGetFuncPtrs      - loads table with ptrs to these global routines
 *       ipGetClientDataPtr - returns ptr to client data-area in instance
 *       ipInsertedData     - client inserted some data in output data stream
 *       ipGetImageTraits   - returns traits of input and output images
 */

/* handle for a conversion job */
typedef void FAR*IP_HANDLE, *FAR*PIP_HANDLE, FAR*FAR*LPIP_HANDLE;

typedef void (WINAPI *LPIP_PEEK_FUNC)(
    IP_HANDLE         hJob,       /* handle for job making the callback */
    LPIP_IMAGE_TRAITS pTraits,    /* traits of the data being peeked at */
    int               nBytes,     /* # bytes in buffer below */
    LPBYTE            pBuf,       /* data being peeked at */
    long              nFilePos,   /* file-position where data belongs */
    PVOID             pUserdata); /* Data passed to the user in peek calls. */ 


/* IP_XFORM - The list of the standard xforms supplied in image processor
 *
 * Warning: If the list below changes, you must also change an array in
 * ipmain.c that is indexed by this enum.
 */
typedef enum {
    X_FAX_ENCODE,  X_FAX_DECODE,   /* MH, MR and MMR formats */
    X_PCX_ENCODE,  X_PCX_DECODE,
    /* X_BMP_ENCODE,  X_BMP_DECODE, */
    X_JPG_ENCODE,  X_JPG_DECODE, X_JPG_FIX,
    X_TIF_ENCODE,  X_TIF_DECODE,
    X_PNM_ENCODE,  X_PNM_DECODE,
    X_SCALE,
    X_GRAY_2_BI, X_BI_2_GRAY,
    X_CNV_COLOR_SPACE,
    X_Y_EXTRACT,
    /* X_HEADER, */
    X_THUMB,
    X_TABLE,      /* tables are: user1,user3,mirror,gamma,threshold,pass-thru */
    X_CROP,
    X_TONEMAP,
    X_SATURATION,
    X_ROTATE,
    X_PAD,
    X_FAKE_MONO,
    X_GRAYOUT,
    X_CHANGE_BPP,
    X_MATRIX,
    X_CONVOLVE,
    X_INVERT,
    X_SKEL,
} IP_XFORM, *PIP_XFORM, FAR*LPIP_XFORM;


#define IP_MAX_XFORM_INFO	8

/* IP_XFORM_SPEC - Fully specifies an xform
 * Each transform driver documents what goes into aXformInfo.
 */
typedef struct {
    LPIP_XFORM_TBL pXform;        /* ptr to jmp-table for xform to do */
    IP_XFORM       eXform;        /* which xform (used if pXform is NULL) */
    LPIP_PEEK_FUNC pfReadPeek;    /* callback when xform dvr reads data */
    LPIP_PEEK_FUNC pfWritePeek;   /* callback when xform dvr writes data */
    PVOID          pUserData;     /* Data passed to user in peek functions. */
    DWORD_OR_PVOID aXformInfo[IP_MAX_XFORM_INFO]; /* xform-specific info */
} IP_XFORM_SPEC, *PIP_XFORM_SPEC, FAR*LPIP_XFORM_SPEC;



/*****************************************************************************\
 *
 * ipOpen - Opens a new conversion job
 *
 *****************************************************************************
 *
 * This routine allocates some instance data, including space for nClientData.
 * More memory is allocated once row-lengths are such are known.
 * To deallocate all memory, ipClose must be called.
 *
 * The nXforms and pXforms parameters specify the sequence of transforms
 * to go from input to output.  The handle for the new job is returned
 * in *pXform.
 *
 * A restriction on the list of xforms: the data-flows between xforms must be
 * fixed-length buffers. This restricts you to raw raster rows between xforms.
 * The data-flows into and out of ipConvert can be variable-length in nature.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipOpen (
    int             nXforms,      /* in:  number of xforms in lpXforms below */
    LPIP_XFORM_SPEC lpXforms,     /* in:  list of xforms we should perform */
    int             nClientData,  /* in:  # bytes of additional client data */
    LPIP_HANDLE     phJob);       /* out: handle for conversion job */



/*****************************************************************************\
 *
 * ipMirrorBytes - Swaps bits in each byte of buffer
 *                 (bits 0<->7, 1<->6, etc.)
 *
\*****************************************************************************/

VOID ipMirrorBytes(PBYTE pbInputBuf,DWORD dwInputAvail);



/*****************************************************************************\
 *
 * ipConvert - Converts some input data (work-horse function)
 *
 *****************************************************************************
 *
 * This function consumes input data and produces output data via the
 * input- and output-buffer parameters.  And it tells you what's happening
 * via its function return value.
 *
 * On entry, pbInputBuf and wInputAvail specify the location and number of
 * data-bytes in the input buffer.  On return, pwInputUsed tells you how
 * many of those input bytes were consumed.  pdwInputNextPos tells you
 * where in the input file you should read next for the following call;
 * 0 is the beginning of the file.  This is almost always the current file
 * position plus pwInputUsed; if not, a file-seek is being requested.
 *
 * The output buffer parameters are analogous to the input parameters,
 * except that pdwOutputThisPos tells you where the bytes just output
 * should be written in the output file.  That is, it applies to *this*
 * write, not the *next* write, unlike the input arrangement.
 * The output buffer pointers are allowed to be NULL, in which case
 * the output is discarded.
 *
 * The function return value is a bit-mask that tells you if anything
 * interesting happened.  Multiple bits can be set.  This information
 * should be treated as independent of the data-transfers occuring via
 * the parameters.  The IP_CONSUMED_ROW and IP_PRODUCED_ROW bits can
 * be used to count how many rows have been input and output.
 *
 * The IP_NEW_OUTPUT_PAGE bit is set when or after the last row of the
 * page has been sent, and before the first row of the following page (if
 * any) is sent.
 *
 * You may wish to insert secret data, such as thumbnails, into the
 * output stream.  When ipConvert returns the IP_WRITE_INSERT_OK bit,
 * it is giving you permission to write stuff AFTER you write the output
 * buffer it gave you.  After adding your secret data, you must call
 * ipInsertedData to tell us how many bytes were added.
 *
 * When there is no more input data, ipConvert must be called repeatedly
 * with a NULL pbInputBuf parameter, which tells the processor to flush out
 * any buffered rows.  Keep calling it until it returns the IP_DONE bit.
 *
 * Do not call ipConvert again after it has returned either error bit or
 * IP_DONE.
 *
 * At any time after ipConvert returns the IP_PARSED_HEADER bit,
 * ipGetImageTraits may be called to obtain the input and output traits.
 * IP_PARSED_HEADER is returned in *every* call after the header was parsed.
 *
 * Return value:  Zero or more of these bits may be set:
 *
 *     IP_PARSED_HEADER    = input header has been parsed; traits are known
 *     IP_CONSUMED_ROW     = an input row was parsed
 *     IP_PRODUCED_ROW     = an output row was produced
 *     IP_INPUT_ERROR      = syntax error in input data
 *     IP_FATAL_ERROR      = misc error (internal error or bad param)
 *     IP_NEW_INPUT_PAGE   = just encountered end of page on input
 *     IP_NEW_OUTPUT_PAGE  = just finished outputting a page
 *     IP_WRITE_INSERT_OK  = okay to insert data in output file
 *     IP_DONE             = conversion is completed.
 *
\*****************************************************************************/

EXPORT(WORD) ipConvert (
    IP_HANDLE   hJob,              /* in:  handle to conversion job */
    DWORD       dwInputAvail,      /* in:  # avail bytes in input buf */
    LPBYTE      pbInputBuf,        /* in:  ptr to input buffer */
    LPDWORD     pdwInputUsed,      /* out: # bytes used from input buf */
    LPDWORD     pdwInputNextPos,   /* out: file-pos to read from next */
    DWORD       dwOutputAvail,     /* in:  # avail bytes in output buf */
    LPBYTE      pbOutputBuf,       /* in:  ptr to output buffer */
    LPDWORD     pdwOutputUsed,     /* out: # bytes written in out buf */
    LPDWORD     pdwOutputThisPos); /* out: file-pos to write this data */



/*****************************************************************************\
 *
 * ipClose - Destroys the given conversion job
 *
 *****************************************************************************
 *
 * This routine deallocates all memory associated with the given conversion
 * job.  It may be called at any time, which is how you do an abort.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipClose (
    IP_HANDLE hJob);     /* in:  handle to conversion job */


/****************************************************************************\
 *
 * ipResultMask - Selects bit-results to be returned by ipConvert
 *
 ****************************************************************************
 *
 * The mask parameter is the OR of the IP_... bits you want returned by
 * calls to ipConvert.  A 1 means you want that bit; 0 means you don't.
 * By disabling frequently-returned bits, efficiency will improve because
 * fewer ipConvert calls will be made because each call can do more work.
 * 
 * The mask is all zeroes by default.  The IP_DONE, IP_FATAL_ERROR, and
 * IP_INPUT_ERROR bits are always enabled, regardless of their bit-values
 * in the mask.
 *
\*****************************************************************************/

EXPORT(WORD) ipResultMask (
    IP_HANDLE hJob,     /* in:  handle to conversion job */
    WORD      wMask);   /* in:  result bits you are interested in */


/*****************************************************************************\
 *
 * ipGetClientDataPtr - Returns ptr to client's data in conversion instance
 *
 *****************************************************************************
 *
 * ipOpen accepts an nClientData parameter which is the number of extra bytes
 * we allocate for the client for his own use.  This function returns the
 * pointer to that memory in the given conversion instance.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipGetClientDataPtr (
    IP_HANDLE     hJob,            /* in:  handle to conversion job */
    LPVOID    FAR*ppvClientData);  /* out: ptr to client's memory area */



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
    IP_HANDLE         hJob,      /* in: handle to conversion job */
    LPIP_IMAGE_TRAITS pTraits);  /* in: default image traits */



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
    LPIP_IMAGE_TRAITS pTraits);  /* out: output image traits */



/*****************************************************************************\
 *
 * ipGetImageTraits - Returns traits of input and output images
 *
 *****************************************************************************
 *
 * At any time after ipConvert has returned the IP_PARSED_HEADER bit, this
 * function may be called to obtain the traits of the input and output images.
 * If a pointer parameter is NULL, that traits record is not returned.
 *
 * After the conversion job is done, these traits will contain the actual
 * number of rows input and output.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipGetImageTraits (
    IP_HANDLE         hJob,           /* in:  handle to conversion job */
    LPIP_IMAGE_TRAITS pInputTraits,   /* out: traits of input image */
    LPIP_IMAGE_TRAITS pOutputTraits); /* out: traits of output image */



/*****************************************************************************\
 *
 * ipInsertedData - Client inserted some bytes into our output stream
 *
 *****************************************************************************
 *
 * After ipConvert returns the IP_WRITE_INSERT_OK bit, and after the client
 * writes out the output buffer, he is permitted to write out some additional
 * data for his own use, such as a thumbnail image.  After writing the added
 * data, the client calls this function to tell us how much data he wrote.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipInsertedData (
    IP_HANDLE hJob,         /* in: handle to conversion job */
    DWORD     dwNumBytes);  /* in: # of bytes of additional data written */



/*****************************************************************************\
 *
 * ipOverrideDPI - Force a different DPI to be reported in the output
 *
 *****************************************************************************
 *
 * The values supplied only change the DPI that's *reported* in the output
 * traits and in the header (if any) of the output file.  These DPI values
 * do *not* affect the transforms, and do *not* affect or change any scaling.
 *
 * The image processor code supplies these values in the input traits of
 * the last transform in the list of transforms so that they'll make it
 * in the output file header.
 *
 * The DPI values are in fixed-point with 16 bits of fraction.  A value of
 * 0 means "no override; use the normal value".
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipOverrideDPI (
    IP_HANDLE hJob,        /* in: handle to conversion job */
    DWORD     dwHorizDPI,  /* in: horiz DPI as 16.16; 0 means no override */
    DWORD     dwVertDPI);  /* in: vert  DPI as 16.16; 0 means no override */



/*****************************************************************************\
 *
 * ipGetFuncPtrs - Loads jump-table with pointers to the ip entry points
 *
 *****************************************************************************
 *
 * This function loads a jump-table (typedef'ed below) for your convenience
 * so you won't have to call GetProcAddress on every function.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

/*****************************************************************************\
 *
 * ipGetXformTable - Returns ptr to IP_XFORM_TBL for transform.
 *
 *****************************************************************************
 *
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

EXPORT(WORD) ipGetXformTable (LPIP_XFORM_TBL phXform);



/*** Prototype for ipGetFuncPtrs function ***/
typedef WORD (WINAPI *IPOPEN)             (int, LPIP_XFORM_SPEC, 
                                           int, LPIP_HANDLE);
typedef WORD (WINAPI *IPCONVERT)          (IP_HANDLE, DWORD, LPBYTE,    
                                           LPDWORD, LPDWORD, DWORD,      
                                           LPBYTE, LPDWORD, LPDWORD);   
typedef WORD (WINAPI *IPCLOSE)            (IP_HANDLE);
typedef WORD (WINAPI *IPGETCLIENTDATAPTR) (IP_HANDLE, LPVOID FAR *);
typedef WORD (WINAPI *IPRESULTMASK)       (IP_HANDLE, WORD);
typedef WORD (WINAPI *IPSETDEFAULTINPUTTRAITS) (IP_HANDLE,
                                                LPIP_IMAGE_TRAITS);
typedef WORD (WINAPI *IPGETIMAGETRAITS)   (IP_HANDLE,
                                           LPIP_IMAGE_TRAITS, 
                                           LPIP_IMAGE_TRAITS);
typedef WORD (WINAPI *IPINSERTEDDATA)     (IP_HANDLE, DWORD);
typedef WORD (WINAPI *IPOVERRIDEDPI)      (IP_HANDLE, DWORD, DWORD);

typedef WORD (WINAPI *IPGETXFORMTABLE)    (LPIP_XFORM_TBL);

typedef WORD (WINAPI *IPGETOUTPUTTRAITS)  (IP_HANDLE,
                                           LPIP_IMAGE_TRAITS);

typedef struct {
    WORD                    wStructSize;   /* # of bytes in this struct */
    IPOPEN                  ipOpen;
    IPCONVERT               ipConvert;
    IPCLOSE                 ipClose;
    IPGETCLIENTDATAPTR      ipGetClientDataPtr;
    IPRESULTMASK            ipResultMask;
    IPSETDEFAULTINPUTTRAITS ipSetDefaultInputTraits;
    IPGETIMAGETRAITS        ipGetImageTraits;
    IPINSERTEDDATA          ipInsertedData;
    IPOVERRIDEDPI           ipOverrideDPI;
    IPGETOUTPUTTRAITS       ipGetOutputTraits;
} IP_JUMP_TBL, * PIP_JUMP_TBL, FAR * LPIP_JUMP_TBL;

/*** Prototype for ipGetFuncPtrs function ***/
typedef WORD (WINAPI *LPFNIPGETFUNCPTRS)  (LPIP_JUMP_TBL);

EXPORT(WORD) ipGetFuncPtrs (LPIP_JUMP_TBL lpJumpTbl);

/****************************************************************************\
 ****************************************************************************
 *
 * OPTION DEFINITIONS for xform drivers
 *
 ****************************************************************************
\****************************************************************************/

/* xbi2gray.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_BI_2_GRAY_OUTPUT_BPP		0
#define IP_BI_2_GRAY_WHITE_PIXEL	1
#define IP_BI_2_GRAY_BLACK_PIXEL	2


#if 0

/* xbmp.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_BMP_NEGATIVE_HEIGHT		0

#endif


/* xchgbpp.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_CHANGE_BPP_OUTPUT_BPP	0


/* xcolrspc.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_CNV_COLOR_SPACE_WHICH_CNV	0
#define IP_CNV_COLOR_SPACE_GAMMA	1

/* The following conversions are possible: */

typedef enum {
    IP_CNV_YCC_TO_CIELAB = 0,
    IP_CNV_CIELAB_TO_YCC = 1,
    IP_CNV_YCC_TO_SRGB   = 2,
    IP_CNV_SRGB_TO_YCC   = 3,
    IP_CNV_LHS_TO_SRGB   = 4,
    IP_CNV_SRGB_TO_LHS   = 5,
    IP_CNV_BGR_SWAP      = 100
} IP_WHICH_CNV;


/* xconvolve.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_CONVOLVE_NROWS    0
#define IP_CONVOLVE_NCOLS    1
#define IP_CONVOLVE_MATRIX   2
#define IP_CONVOLVE_DIVISOR  3

#define IP_CONVOLVE_MAXSIZE  9  /* up to 9x9 matrix */


/* xcrop.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_CROP_LEFT		0
#define IP_CROP_RIGHT		1
#define IP_CROP_TOP		2
#define IP_CROP_MAXOUTROWS	3


/* xfakemono.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_FAKE_MONO_BPP	0


/* xfax.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_FAX_FORMAT		0
#define IP_FAX_NO_EOLS		1
#define IP_FAX_MIN_ROW_LEN	2

enum {IP_FAX_MH, IP_FAX_MR, IP_FAX_MMR};


/* xgray2bi.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_GRAY_2_BI_THRESHOLD	0


/* xgrayout.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_GRAYOUT_LEFT		0
#define IP_GRAYOUT_RIGHT	1
#define IP_GRAYOUT_TOP		2
#define IP_GRAYOUT_BOTTOM	3


#if 0

/* xheader.h */

/* xheader.h - aXformInfo[0] struct given to xheader.c, the header generator
 *
 * Mark Overton, March 1998
 */

#define IP_HEADER_SPEC  0

/* aXformInfo[IP_HEADER_SPEC] shall be a pointer pointing to this: */

typedef struct {
    PSTR    pszLeftStr;    // ptr to left-justified string; 0 -> none
    PSTR    pszCenterStr;  // ptr to centered string; 0 -> none
    PSTR    pszRightStr;   // ptr to right-justified string; 0 -> none
    WORD    wCharSet;      // character set (may be a two-byte set)
    PSTR    pszTypeFace;   // ptr to name of typeface (required)
    float   fHeightPoints; // point-size of font
    float   fMarginPoints; // left and right margin, in points
    BOOL    bOverlay;      // 0=append header, 1=overlay it on top of page
    RGBQUAD rgbWhite;      // a white pixel
    RGBQUAD rgbBlack;      // a black pixel
} __attribute__((packed)) XHEADER_SPEC, *PXHEADER_SPEC, FAR*LPXHEADER_SPEC;

/* A header is one line of text appearing at the top of faxes.  This is known
 * as a "TTI" in faxland.  We implement this as a left-justified portion, a
 * centered portion, and a right-justified portion.  All three portions are
 * optional.
 *
 * Note that xheader.c copies over the string fields in the struct. Therefore,
 * those strings may go away after the call to cvtOpen.
 *
 * The bOverlay field controls whether the header is concatenated
 * to the top of the page (0), or overlays the top of the page (1).
 *
 * xheader.c needs to know what values to use for white and black pixels,
 * so rgbWhite and rgbBlack are a white pixel and black pixel repectively.
 * If the page data is gray (8 bits/pixel), only the rgbRed field is used.
 * If the page data is bilevel, then only the least significant bit of rgbRed
 * is used.
 */
#endif


/* xjpg.h */

/* This .h file contains symbols xjpg_dec.c and xjpg_enc.c
 * See those .c files for instructions on using these transforms.
 */

/* Specifications for decoder: */

#define IP_JPG_DECODE_OUTPUT_SUBSAMPLED	0
#define IP_JPG_DECODE_FROM_DENALI	1

/* Specifications for encoder: */

#define IP_JPG_ENCODE_QUALITY_FACTORS		0
#define IP_JPG_ENCODE_SAMPLE_FACTORS		1
#define IP_JPG_ENCODE_ALREADY_SUBSAMPLED	2
#define IP_JPG_ENCODE_FOR_DENALI		3
#define IP_JPG_ENCODE_OUTPUT_DNL		4
#define IP_JPG_ENCODE_FOR_COLOR_FAX		5
#define IP_JPG_ENCODE_DUMMY_HEADER_LEN		6


/* xpad.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_PAD_LEFT		0
#define IP_PAD_RIGHT		1
#define IP_PAD_TOP		2
#define IP_PAD_BOTTOM		3
#define IP_PAD_VALUE		4
#define IP_PAD_MIN_HEIGHT	5


/* xrotate.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_ROTATE_UPPER_LEFT	0
#define IP_ROTATE_UPPER_RIGHT	1
#define IP_ROTATE_LOWER_LEFT	2
#define IP_ROTATE_OUTPUT_SIZE	3
#define IP_ROTATE_FAST		4


/* xsaturation.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_SATURATION_FACTOR	0


/* xscale.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_SCALE_HORIZ_FACTOR	0
#define IP_SCALE_VERT_FACTOR	1
#define IP_SCALE_FAST		2


/* xskel.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_SKEL_SPEC_1		0
#define IP_SKEL_SPEC_2		1


/* xtable.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_TABLE_WHICH	0
#define IP_TABLE_OPTION	1

#define IP_TABLE_COLOR_1   1
#define IP_TABLE_COLOR_2   2
#define IP_TABLE_COLOR_3   3

typedef enum {
    IP_TABLE_USER,
    IP_TABLE_PASS_THRU,
    IP_TABLE_GAMMA,
    IP_TABLE_THRESHOLD,
    IP_TABLE_MIRROR,
    IP_TABLE_USER_THREE,
    IP_TABLE_BW_CLIP,
    IP_TABLE_USER_WORD,
    IP_TABLE_USER_THREE_WORD
} IP_TABLE_TYPE;


/* xthumb.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_THUMB_SCALE_SPEC  0


/* xtiff.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_TIFF_FILE_PATH	0


/* xtonemap.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_TONEMAP_POINTER	0
#define IP_TONEMAP_LUM_SPACE	1


/* xyxtract.h */

/* This .h file contains symbols for the transform in the corresponding .c file.
 * See that .c file for instructions on using this transform.
 */

#define IP_Y_EXTRACT_COLOR_SPACE	0

typedef enum {
   IP_Y_EXTRACT_LUM_CHROME,
   IP_Y_EXTRACT_RGB,
   IP_Y_EXTRACT_BGR
} IP_Y_EXTRACT_WHICH_SPACE;

#if defined(__cplusplus)
	}
#endif

#endif


/* End of File */

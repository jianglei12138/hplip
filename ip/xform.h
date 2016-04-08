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

/* xform.h - Interface into the transform drivers
 *
 * Mark Overton, Jan 2000 - Extracted from hpojip.h
 */

#if ! defined XFORM_H_INC
#define XFORM_H_INC


/*****************************************************************************\
 *****************************************************************************
 *
 * TRANSFORM DRIVER interface
 *
 *****************************************************************************
\*****************************************************************************/


/* In some ways, this driver interface is very similar to the main ip
 * interface.  Please don't get them confused.
 *
 * One of the parameters of the ipOpen function is an array of structures.
 * Each such structure defines a transform, and contains, among other things,
 * a pointer to a jump-table for the transform-driver.  The jump-table is
 * a structure containing function-pointers for all the driver's functions.
 * These functions and the jump-table are defined below.
 *
 * Function Call Order
 *
 *     openXform;
 *     setDefaultInputTraits, setXformSpec, getHeaderBufSize (any order);
 *     getActualTraits (possibly multiple calls);
 *     getActualBufSizes;
 *     convert, insertedData, newPage (usually multiple calls);
 *     closeXform.
 *
 * Raw Data Format
 *
 * A decoder outputs raw data, an encoder inputs raw data, and all data passed
 * between xforms consists of raw data.
 * Such raw raster data consists of fixed-length raster rows of packed pixels.
 * The pixels are packed as follows:
 *
 *     bi-level:   8 pixels/byte, left pixel in msb, 0=white, 1=black.
 *     4-bit gray: 2 pixels/byte, left pixel in hi nibble, 0=black, 15=white.
 *     8-bit gray: 1 pixel/byte, 0=black, 255=white.
 *     color:      three bytes per pixel, in some color space.
 *
 * Driver Documentation
 *
 * The .c file for an xform driver starts with a comment-section documenting
 * the following items:
 *
 *     - the capabilities of the driver, and its limitations.
 *     - the name of the global jump-table (of type IP_XFORM_TBL).
 *     - what should be put in the aXformInfo array passed to setXformSpec.
 *     - which items in default input traits are ignored versus used.
 *     - what the output image traits are.
 *
 * Note that image traits, such as pixels per row, should *not* be put in
 * aXformInfo because such info is provided by setDefaultInputTraits.
 * Things like the JPEG quality-factor should be in aXformInfo.
 *
 * An xform driver is allowed to overrun its input or output buffer by
 * up to 12 bytes.  This is allowed because some image processing algorithms
 * are faster if they operate on multiple pixels at a time, which will cause
 * them to read or write a little past the end of the buffer.  Reading or
 * writing before the beginning of a buffer is not allowed.
 */


typedef void* IP_XFORM_HANDLE;  /* handle for an xform driver */


/* IP_XFORM_TBL - Jump-table for a transform driver (all the entry points)
 */
typedef struct IP_XFORM_TBL_s {

    /* openXform - Creates a new instance of the transformer
     *
     * This returns a handle for the new instance to be passed into
     * all subsequent calls.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*openXform) (
        IP_XFORM_HANDLE *pXform);  /* out: returned handle */


    /* setDefaultInputTraits - Specifies default input image traits
     *
     * The header of the file-type handled by the transform probably does
     * not include *all* the image traits we'd like to know.  Those not
     * specified in the file-header are filled in from info provided by
     * this routine.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*setDefaultInputTraits) (
        IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
        PIP_IMAGE_TRAITS pTraits);   /* in: default image traits */


    /* setXformSpec - Provides xform-specific information
     *
     * The aXformInfo array provides the transform-specific info.
     * For example, for a scaling transform, this array would contain
     * the horizontal and vertical scaling factors and possibly additional
     * info about whether to scale quickly by simple pixel-replication.
     * For a JPEG-encode transform, the array would contain the quality
     * factor and subsampling information.
     *
     * Each transform documents what it needs in aXformInfo.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*setXformSpec) (
        IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
        DWORD_OR_PVOID   aXformInfo[]);  /* in: xform information */


    /* getHeaderBufSize- Returns size of input buf needed to hold header
     *
     * Returns size of input buffer that's guaranteed to hold the file
     * header.  If that's too big, the xform code will have to parse the
     * header across several calls, and return a suitable buffer size.
     * If there is no header, this function returns 0 in pwInBufLen.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*getHeaderBufSize) (
        IP_XFORM_HANDLE  hXform,       /* in:  handle for xform */
        DWORD           *pdwInBufLen); /* out: buf size for parsing header */


    /* getActualTraits - Parses header, and returns input & output traits
     *
     * This function may need to be called multiple times to parse the
     * file header.  And it can consume input in each call.
     * Once the header has been parsed, it returns IP_DONE, meaning that
     * it is not to be called again and that the in-traits and out-traits
     * structures have been filled in.  The output traits are computed based
     * on the input traits and the xform information provided by setXformSpec.
     *
     * See the description of 'convert' below for how the input-buffer
     * parameters are used.  The final value returned in pdwInputNextPos
     * (where this returns IP_DONE) is the one that applies to the
     * 'convert' function.  This function MUST be called even if there is
     * no header because you need that pdwInputNextPos value.
     *
     * NOTE:  In addition to IP_DONE, the IP_READY_FOR_DATA bit must also
     * be set if the xform will want data on the first call to 'convert'.
     *
     * Return value:
     *    0              = call again with more input,
     *    IP_DONE        = normal end (IP_READY_FOR_DATA set if data needed)
     *    IP_INPUT_ERROR = error in input data,
     *    IP_FATAL_ERROR = misc error.
     */
    WORD (*getActualTraits) (
        IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
        DWORD             dwInputAvail,   /* in:  # avail bytes in input buf */
        PBYTE             pbInputBuf,     /* in:  ptr to input buffer */
        PDWORD            pdwInputUsed,   /* out: # bytes used from input buf */
        PDWORD            pdwInputNextPos,/* out: file-pos to read from next */
        PIP_IMAGE_TRAITS pInTraits,       /* out: input image traits */
        PIP_IMAGE_TRAITS pOutTraits);     /* out: output image traits */


    /* getActualBufSizes - Returns buf sizes needed for remainder of session
     *
     * Since the input and output row-lengths are now known because
     * getActualTraits has parsed the header, and actual buffer sizes needed
     * for the remainder of the conversion session can now be fetched.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*getActualBufSizes) (
        IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
        PDWORD           pwMinInBufLen,   /* out: min input buf size */
        PDWORD           pwMinOutBufLen); /* out: min output buf size */

    
    /* convert - The work-horse conversion routine
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
     *
     * The function return value is a bit-mask that tells you if anything
     * interesting happened.  Multiple bits can be set.  This information
     * should be treated as independent of the data-transfers occuring via
     * the parameters.  The IP_CONSUMED_ROW and IP_PRODUCED_ROW bits can
     * be used to count how many rows have been input and output.
     *
     * The IP_READY_FOR_DATA bit indicates that the next call to 'convert'
     * definitely will consume data.  If this bit is 0, the next call
     * definitely will NOT consume data.  The main converter code that calls
     * these xform functions uses this ready-for-data info to control the
     * order of xform calls.  The IP_READY_FOR_DATA bit must be set correctly.
     *
     * The IP_NEW_INPUT_PAGE bit is set when or after the last row of the
     * input page has been parsed.  It may be a few rows before you get the
     * corresponding IP_NEW_OUTPUT_PAGE bit.
     *
     * The IP_NEW_OUTPUT_PAGE bit is set when or after the last row of the
     * page has been sent, and before the first row of the following page (if
     * any) is sent.
     *
     * You may wish to insert secret data, such as thumbnails, into the
     * output stream.  When 'convert' returns the IP_WRITE_INSERT_OK bit,
     * it is giving you permission to write stuff AFTER you write the output
     * buffer it gave you.  After adding your secret data, you must call
     * insertedData to tell us how many bytes were added.
     *
     * When there is no more input data, 'convert' must be called repeatedly
     * with a NULL pbInputBuf parameter, which tells the xform to flush out
     * any buffered rows.  Keep calling it until it returns the IP_DONE bit.
     *
     * Do not call 'convert' again after it has returned either error bit or
     * IP_DONE.
     *
     * If the input or output data consists of fixed-length uncompressed rows,
     * then it is permissible for the xform to read up to 8 bytes past the
     * end of the (fixed-length) input row, or to write up to 8 bytes past the
     * end of the (fixed-length) output row.  The caller of the xform routines
     * allocates at least this many extra overrun-zone bytes so that algorithms
     * can process pixels multiple-bytes at a time without worrying about
     * running past the end of the row.  These overrun-zone bytes must NOT be
     * reported by getActualBufSizes, which should report only what's needed
     * for the actual row-data.
     *
     * Return value:  Zero or more of these bits may be set:
     *
     *     IP_CONSUMED_ROW     = an input row was parsed
     *     IP_PRODUCED_ROW     = an output row was produced
     *     IP_INPUT_ERROR      = syntax error in input data
     *     IP_FATAL_ERROR      = misc error (internal error or bad param)
     *     IP_NEW_INPUT_PAGE   = just finished parsing the input page
     *     IP_NEW_OUTPUT_PAGE  = just finished outputting a page
     *     IP_WRITE_INSERT_OK  = okay to insert data in output file
     *     IP_DONE             = conversion is completed.
     */
    WORD (*convert) (
        IP_XFORM_HANDLE hXform,
        DWORD            dwInputAvail,      /* in:  # avail bytes in in-buf */
        PBYTE            pbInputBuf,        /* in:  ptr to in-buffer */
        PDWORD           pdwInputUsed,      /* out: # bytes used from in-buf */
        PDWORD           pdwInputNextPos,   /* out: file-pos to read from next */
        DWORD            dwOutputAvail,     /* in:  # avail bytes in out-buf */
        PBYTE            pbOutputBuf,       /* in:  ptr to out-buffer */
        PDWORD           pdwOutputUsed,     /* out: # bytes written in out-buf */
        PDWORD           pdwOutputThisPos); /* out: file-pos to write the data */


    /* newPage - Tells xform to flush rest of this page, and start a new page
     *
     * Uncompressed input-data formats do not have page-boundary info, so
     * calling this function between rows fed into 'convert' is how you tell
     * xform to output a page-boundary.  After this is called, 'convert' will
     * first flush any buffered rows, then return the IP_NEW_OUTPUT_PAGE bit,
     * and finally start a new page.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*newPage) (IP_XFORM_HANDLE hXform);


    /* insertedData - Tells us that bytes were inserted into output stream
     *
     * See IP_WRITE_INSERT_OK discussion above. You call this routine to tell
     * the xform code how many bytes were secretly added to the output stream.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*insertedData) (
        IP_XFORM_HANDLE  hXform,
        DWORD            dwNumBytes);


    /* closeXform - destroys instance of xform
     *
     * This may be called at any time to remove an instance of the xform.
     * It deallocates all dynamic memory associated with the xform.
     *
     * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
     */
    WORD (*closeXform)(IP_XFORM_HANDLE hXform);

} IP_XFORM_TBL, *PIP_XFORM_TBL, FAR*LPIP_XFORM_TBL;

#endif

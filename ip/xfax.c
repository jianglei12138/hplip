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
 * xfax.c - encoder and decoder for fax data (MH, MR and MMR)
 *
 *****************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    faxEncodeTbl = the encoder,
 *    faxDecodeTbl = the decoder.
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_FAX_FORMAT]:  Format of data (both encoder & decoder).
 *        Values are:  IP_FAX_MH, IP_FAX_MR, IP_FAX_MMR.
 *
 *    aXformInfo[IP_FAX_NO_EOLS]:  No EOLs in the data?
 *                    0 = EOLs are in data as usual;
 *                    1 = no EOLs in data.
 *                    This tells the encoder whether to output EOLs.
 *                    This tells the decoder if there are EOLs in the data.
 *
 *    aXformInfo[IP_FAX_MIN_ROW_LEN]:  Minimum # bits to put in each output row
 *                    (MH & MR only).
 *                    Only the encoder needs this, and guarantees that each
 *                    row it outputs contains at least this many bits.  It
 *                    inserts fill 0's as needed.  The fax standard needs
 *                    this to insure that a row consumes a minimum amout of
 *                    time when sent over the modem, hence there's a minimum
 *                    number of bits to be transmitted per row.
 *
 * Capabilities and Limitations:
 *
 *    Bits per pixel must be 1 (bi-level only).
 *    Encodes and decodes MH, MR and MMR per the fax standard.
 *    Encoding MR uses a k-factor of 2 if vert dpi < 150, else k-factor is 4.
 *    If an error occurs in MH or MR data, the previous good row is returned.
 *
 * Default Input Traits, and Output Traits:
 *
 *    For both encoder and decoder:
 *
 *          trait             default input             output
 *    -------------------  -------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 1            1
 *    iComponentsPerPixel     passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Mark Overton, Jan 1998
 *
\*****************************************************************************/

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */
#include "assert.h"    /* todo: eliminate all asserts */


#if 0
    #include "stdio.h"
    #include <tchar.h>

    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stdout, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE    0x1ce5ca7e




/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@                                                                          @@
@@                                                                          @@
@@                      U  T  I  L  I  T  I  E  S                           @@
@@                                                                          @@
@@                                                                          @@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/


/* baLeftZeroesTbl returns number of leading zeroes in byte index */

static const BYTE baLeftZeroesTbl[256] =
{
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* baRightZeroesTbl returns number of trailing zeroes in byte index */

static const BYTE baRightZeroesTbl[256] =
{
    8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};



/*____________________________________________________________________________
 |         |                                                                  |
 | scan_to | Scan pixels rightward until hitting desired color (white/black)  |
 |_________|__________________________________________________________________|
 |                                                                            |
 | Pos, the initial search position, must not be negative nor greater than    |
 | pixels_in_row+1.  This function will not return a value greater than       |
 | pixels_in_row.                                                             |
 |                                                                            |
 | WARNING:  Before calling this routine, the first byte after the end of the |
 |           buffer must be set to alternating zeroes and ones (such as 0x55).|
 |           This allows us to scan bytes for a pixel-change without doing    |
 |           the end-of-buffer boundary-check.                                |
 |                                                                            |
 | Function return value:  index of first pixel of desired color.             |
 |____________________________________________________________________________|
*/
static int scan_to (
    UINT  skip,          /* color to skip over (FF=black, 00=white) */
    BYTE *buf_p,         /* buffer in which to search */
    int   start_pos,     /* start-index for search (0=leftmost pixel in buf) */
    int   pixels_in_row) /* # pixels in the buffer */
{
    /**************************************************************************
     *
     * PERFORMANCE NOTE
     *
     * This routine skips all-white and all-black areas a *byte* at a time
     * using a three-instruction loop.  The old routine did this a *word*
     * (2 bytes) at a time using a three-instruction loop.  But this routine
     * results in faster G3/G4 encoding/decoding, even for mostly white areas,
     * because it has so little overhead outside the loop.  Also, its small
     * size doesn't toss as much other G3/G4 code out of the cache, improving
     * performance even more.
     *
     * Seconds to MMR encode+decode 1000 rows of 1728 pixels (bench_fax.c):
     *
     *    black density:           0%    10%     20%     50%
     *    old routine:            2.1    10.2    18.8    45.1
     *    this routine:           2.2     7.4    12.8    29.9
     *    inlining this routine:  2.2     7.9    13.8    32.4
     *
     * Inlining this routine *hurt* performance a little because more MMR
     * code was kicked out of the cache.
     *
     *************************************************************************/

    BYTE *cur_p;
    UINT  byte_mask;
    UINT  byte;
    int   pos;

    byte_mask = 0x00FFu;
    cur_p = buf_p + (start_pos >> 3);
    skip &= byte_mask;
    byte = (*cur_p ^ skip) & (byte_mask >> (start_pos & 7));

    if (byte == 0) {
        do {
            cur_p += 1;
            byte = *cur_p;
        } while (byte == skip);
        byte ^= skip;
    }

    pos = ((cur_p-buf_p)<<3) + baLeftZeroesTbl[byte];

    if (pos > pixels_in_row)
        pos = pixels_in_row;
    return pos;
}



/*____________________________________________________________________________
 |                |                                                           |
 | worst_buf_size | calculates worst buffer-usage for a compressed row        |
 |________________|___________________________________________________________|
*/
/* worst-case failure of compression (which is expansion) */
#define WORST_EXPAND_1D  (9<<1)    /* = 4.5 in 14.2 fixed-point */
#define WORST_EXPAND_2D  (6<<2)    /* = 6.0 in 14.2 fixed-point */

static int worst_buf_size (
    WORD  wFmt,        /* fax format (IP_FAX_MH/MR/MMR) */
    int   iRowWidth)   /* width of each row of bitmap (# pixels) */
{
    return  (  (  (wFmt==IP_FAX_MH ? WORST_EXPAND_1D : WORST_EXPAND_2D)
                  * ((iRowWidth+7)/8)
               ) >> 2
            ) + 4;
}



/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@                                                                          @@
@@                                                                          @@
@@                        E  N  C  O  D  E  R                               @@
@@                                                                          @@
@@                                                                          @@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/


/* ENC_INST - our instance variables */

typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the image                 */
    DWORD  dwValidChk;   /* struct validity check value              */
    DWORD  dwInNextPos;  /* file pos for subsequent input            */
    DWORD  dwOutNextPos; /* file pos for subsequent output           */
    int    iRowLen;      /* # pixels in each uncompressed input row  */
    BYTE   wOutFmt;      /* output format (IP_FAX_MH/FAX_MR/FAX_MMR) */
    BOOL   fNoEOLs;      /* don't output any EOLs?                   */
    UINT   w12Cycle;     /* (MR only) # rows in [1d,2d,2d...] cycle  */
    UINT   wMinBits;     /* minimum # bits to output in each row     */
    int    iRowNum;      /* current row-number of output, 0 is first */
    BYTE  *prior_p;      /* (MR/MMR only) the prior row              */

    /* Variables for "Outputting Bits" section */
    BYTE  *pbBufStart;   /* beginning of output buffer                  */
    BYTE  *pbOutByte;    /* current byte in output buffer               */
    DWORD  dwBitBuffer;  /* buffer of bits (to be written to pbOutByte) */
    UINT   wBitsAvail;   /* # of unused bits in dwBitBuffer             */
} ENC_INST, *PENC_INST;



/*****************************************************************************
 *                                                                           *
 *                       O U T P U T T I N G   B I T S                       *
 *                                                                           *
 *****************************************************************************


Interface into this section:
    put_init      - initializes this section
    put_bits      - outputs a variable-number of bits (buffered)
    put_fill_bits - Outputs fill bits (if necessary) to reach the minimum
    put_grab      - gives all bytes except the partial one to caller
    put_new_buf   - tells us to use a new buffer (after put_grab was called)
    put_done      - writes any remaining bits in the buffer
*/



/*____________________________________________________________________________
 |          |                                                                 |
 | put_init | Initializes this 'put' section                                  |
 |__________|_________________________________________________________________|
*/
static void put_init (
    ENC_INST *g,
    BYTE     *pbOutBuf)
{
    g->dwBitBuffer = 0;
    g->wBitsAvail  = 32;
    g->pbOutByte   = pbOutBuf;
    g->pbBufStart  = pbOutBuf;
}



/*____________________________________________________________________________
 |          |                                                                 |
 | put_bits | Outputs (buffered) the low-order 'length' bits in 'bits'        |
 |__________|_________________________________________________________________|
 |                                                                            |
 | 'length' must be no larger than 25.                                        |
 | The bits in 'bits' that are not to be written must be zeroes.              |
 |                                                                            |
 | This routine lets dwBitBuffer fill up until data won't fit.  Then it       |
 | writes out as many bytes as possible from the buffer in a loop.  I         |
 | did it this way so the byte-write code would be in cache for more of       |
 | the writes, and so 'write_bytes' will be called as seldom as possible.     |
 |____________________________________________________________________________|
*/

static void write_bytes (ENC_INST *g)
{
    /* PERFORMANCE NOTE:  inlining this routine slows down the encoder
     * due to worse cache usage.
     */
    BYTE  *byte_p;
    DWORD  bitbuf;
    UINT   bitsavail;

    /* assert (g->wBitsAvail <= 24); */
    byte_p    = g->pbOutByte;
    bitbuf    = g->dwBitBuffer;
    bitsavail = g->wBitsAvail;

    do {
        *byte_p++ = (BYTE )(bitbuf >> 24);
        bitbuf <<= 8;
        bitsavail += 8;
    } while (bitsavail <= 24);

    g->pbOutByte  = byte_p;
    g->dwBitBuffer = bitbuf;;
    g->wBitsAvail = bitsavail;
}


#define put_bits(g, length_par, bits_par)                               \
do {                                                                    \
    UINT   length_loc = length_par;                                     \
    DWORD  bits_loc   = bits_par;                                       \
                                                                        \
    if (length_loc > g->wBitsAvail)                                     \
        write_bytes (g);                                                \
                                                                        \
    g->wBitsAvail -= length_loc;                                        \
    g->dwBitBuffer |= bits_loc << g->wBitsAvail;                         \
} while (0)


static void put_bits_routine (
    ENC_INST *g,
    UINT   length,
    DWORD  bits)
{
    put_bits (g, length, bits);
}



/*____________________________________________________________________________
 |           |                                                                |
 | put_flush | Writes out all bytes containing any data in dwBitBuffer        |
 |___________|________________________________________________________________|
*/
static void put_flush (ENC_INST *g)
{
    if (g->wBitsAvail < 32) {
        g->wBitsAvail &= ~7ul;   /* reduce to a multiple of 8 (byte-boundary) */
        write_bytes (g);
    }

    assert (g->wBitsAvail == 32);
}



/*____________________________________________________________________________
 |               |                                                            |
 | put_fill_bits | Outputs fill bits (if necessary) to reach the minimum      |
 |_______________|____________________________________________________________|
 |                                                                            |
 | We want put_grab to return the minimum # of bits, but it returns an        |
 | array of bytes (not bits).  So this routine insures that at least          |
 | the minimum # of bits have been written (as bytes) into the output         |
 | buffer.                                                                    |
 |____________________________________________________________________________|
*/
static void put_fill_bits (ENC_INST *g)
{
    int iMore;

    put_flush (g);

    /* write out zero-bytes until we're at (or past) the minimum # bits */

    iMore = (int)g->wMinBits - 8*(g->pbOutByte - g->pbBufStart);
    if (iMore > 0) {
        iMore = (iMore+7) / 8;
        memset (g->pbOutByte, 0, iMore);
        g->pbOutByte += iMore;
    }
}



/*____________________________________________________________________________
 |          |                                                                 |
 | put_grab | Returns # of bytes written so far, and restarts at buffer-start |
 |__________|_________________________________________________________________|
 |                                                                            |
 | The caller is expected to copy N bytes from the buffer, where N is the     |
 | number this function returns.                                              |
 |____________________________________________________________________________|
*/
static int  put_grab (ENC_INST *g)
{
    int  n;

    n = g->pbOutByte - g->pbBufStart;
    g->pbOutByte = g->pbBufStart;  /* next byte goes into beginning of buffer */
    return n;
}



/*____________________________________________________________________________
 |             |                                                              |
 | put_new_buf | Use a new buffer (must be called after put_grab)             |
 |_____________|______________________________________________________________|
*/
static void put_new_buf (
    ENC_INST *g,
    BYTE     *pbOutBuf)
{
    assert (g->pbOutByte == g->pbBufStart);
    g->pbOutByte  = pbOutBuf;
    g->pbBufStart = pbOutBuf;
}



/*____________________________________________________________________________
 |          |                                                                 |
 | put_done | Writes any buffered bits, and returns total # of bytes written  |
 |__________|_________________________________________________________________|
*/
static int  put_done (
    ENC_INST *g)
{
    put_flush (g);
    return g->pbOutByte - g->pbBufStart;
}



/*
 *****************************************************************************
 *                                                                           *
 *                        E N C O D I N G   R O W S                          *
 *                                                                           *
 *****************************************************************************


Interface into this section:
    encode_row_1d - compresses a row into 1-dim format for MH and MR
    encode_row_2d - compresses a row into 2-dim format for MR and MMR
*/



/* Structure for storing G3 codes */

typedef struct {
    USHORT bits;
    USHORT length;
} huff_t;


/* run-length = index, index is in 0..63 */
static const huff_t MHWhiteRuns[] = {
   {0x35, 8},  {0x7,  6},  {0x7,  4},  {0x8,  4},
   {0xb,  4},  {0xc,  4},  {0xe,  4},  {0xf,  4},
   {0x13, 5},  {0x14, 5},  {0x7,  5},  {0x8,  5},
   {0x8,  6},  {0x3,  6},  {0x34, 6},  {0x35, 6},
   {0x2a, 6},  {0x2b, 6},  {0x27, 7},  {0xc,  7},
   {0x8,  7},  {0x17, 7},  {0x3,  7},  {0x4,  7},
   {0x28, 7},  {0x2b, 7},  {0x13, 7},  {0x24, 7},
   {0x18, 7},  {0x2,  8},  {0x3,  8},  {0x1a, 8},
   {0x1b, 8},  {0x12, 8},  {0x13, 8},  {0x14, 8},
   {0x15, 8},  {0x16, 8},  {0x17, 8},  {0x28, 8},
   {0x29, 8},  {0x2a, 8},  {0x2b, 8},  {0x2c, 8},
   {0x2d, 8},  {0x4,  8},  {0x5,  8},  {0xa,  8},
   {0xb,  8},  {0x52, 8},  {0x53, 8},  {0x54, 8},
   {0x55, 8},  {0x24, 8},  {0x25, 8},  {0x58, 8},
   {0x59, 8},  {0x5a, 8},  {0x5b, 8},  {0x4a, 8},
   {0x4b, 8},  {0x32, 8},  {0x33, 8},  {0x34, 8}
};


/* run-length = 64*(index+1), index is in 0..26 */
static const huff_t MHMakeupWhite[] = {
   {0x1b, 5},  {0x12, 5},  {0x17, 6},  {0x37, 7},
   {0x36, 8},  {0x37, 8},  {0x64, 8},  {0x65, 8},
   {0x68, 8},  {0x67, 8},  {0xcc, 9},  {0xcd, 9},
   {0xd2, 9},  {0xd3, 9},  {0xd4, 9},  {0xd5, 9},
   {0xd6, 9},  {0xd7, 9},  {0xd8, 9},  {0xd9, 9},
   {0xda, 9},  {0xdb, 9},  {0x98, 9},  {0x99, 9},
   {0x9a, 9},  {0x18, 6},  {0x9b, 9}
};


/* run-length = index, index is in 0..63 */
static const huff_t MHBlackRuns[] = {
   {0x37, 10},  {0x2,   3},  {0x3,   2},  {0x2,   2},
   {0x3,   3},  {0x3,   4},  {0x2,   4},  {0x3,   5},
   {0x5,   6},  {0x4,   6},  {0x4,   7},  {0x5,   7},
   {0x7,   7},  {0x4,   8},  {0x7,   8},  {0x18,  9},
   {0x17, 10},  {0x18, 10},  {0x8,  10},  {0x67, 11},
   {0x68, 11},  {0x6c, 11},  {0x37, 11},  {0x28, 11},
   {0x17, 11},  {0x18, 11},  {0xca, 12},  {0xcb, 12},
   {0xcc, 12},  {0xcd, 12},  {0x68, 12},  {0x69, 12},
   {0x6a, 12},  {0x6b, 12},  {0xd2, 12},  {0xd3, 12},
   {0xd4, 12},  {0xd5, 12},  {0xd6, 12},  {0xd7, 12},
   {0x6c, 12},  {0x6d, 12},  {0xda, 12},  {0xdb, 12},
   {0x54, 12},  {0x55, 12},  {0x56, 12},  {0x57, 12},
   {0x64, 12},  {0x65, 12},  {0x52, 12},  {0x53, 12},
   {0x24, 12},  {0x37, 12},  {0x38, 12},  {0x27, 12},
   {0x28, 12},  {0x58, 12},  {0x59, 12},  {0x2b, 12},
   {0x2c, 12},  {0x5a, 12},  {0x66, 12},  {0x67, 12}
};


/* run-length = 64*(index+1), index is in 0..26 */
static const huff_t MHMakeupBlack[] = {
   {0xf,  10},  {0xc8, 12},  {0xc9, 12},  {0x5b, 12},
   {0x33, 12},  {0x34, 12},  {0x35, 12},  {0x6c, 13},
   {0x6d, 13},  {0x4a, 13},  {0x4b, 13},  {0x4c, 13},
   {0x4d, 13},  {0x72, 13},  {0x73, 13},  {0x74, 13},
   {0x75, 13},  {0x76, 13},  {0x77, 13},  {0x52, 13},
   {0x53, 13},  {0x54, 13},  {0x55, 13},  {0x5a, 13},
   {0x5b, 13},  {0x64, 13},  {0x65, 13}
};


/* run-length = 64*(index+28), index is in 0..12 */
static const huff_t MHExtMakeup[] = {
    {0x8,  11},  {0xc,  11}, {0xd,  11},  {0x12, 12},
    {0x13, 12},  {0x14, 12}, {0x15, 12},  {0x16, 12},
    {0x17, 12},  {0x1c, 12}, {0x1d, 12},  {0x1e, 12},
    {0x1f, 12}
};


/* vertical-offset = index-3 */
static const huff_t VertTbl[] = {
    {2, 7},
    {2, 6},
    {2, 3},
    {1, 1},
    {3, 3},
    {3, 6},
    {3, 7}
};



/*____________________________________________________________________________
 |         |                                                                  |
 | put_run | Outputs a white or black run                                     |
 |_________|__________________________________________________________________|
*/
static void put_run_routine(
    ENC_INST     *g,
    int           iRunLen,
    const huff_t *makeup_tbl,
    const huff_t *code_tbl)
{
    huff_t te;

    while (iRunLen >= 1792) {
        int  tpos;
        tpos = (iRunLen>>6) - (1792>>6);
        if (tpos > 12)
            tpos = 12;
        te = MHExtMakeup [tpos];
        put_bits_routine (g, te.length, te.bits);
        iRunLen -= (tpos+(1792>>6)) << 6;
    }

    if (iRunLen >= 64) {
        te = makeup_tbl [(iRunLen>>6) - 1];
        put_bits_routine (g, te.length, te.bits);
        iRunLen &= 63;
    }

    te = code_tbl [iRunLen];
    put_bits_routine (g, te.length, te.bits);
}

#define put_run(g, par_run_len, par_makeup_tbl, par_code_tbl)           \
do {                                                                    \
    huff_t te;                                                          \
    int    loc_run_len = par_run_len;                                   \
                                                                        \
    if (loc_run_len >= 64)                                              \
        put_run_routine (g, loc_run_len, par_makeup_tbl, par_code_tbl); \
    else {                                                              \
        te = par_code_tbl [loc_run_len];                                \
        put_bits (g, te.length, te.bits);                               \
    }                                                                   \
} while (0)



#define PutWhiteRun(g, iRunLen) \
    put_run (g, iRunLen, MHMakeupWhite, MHWhiteRuns)


#define PutBlackRun(g, iRunLen) \
    put_run (g, iRunLen, MHMakeupBlack, MHBlackRuns)


static void PutEOL(ENC_INST *g)  /* output the EOL code (11 zeroes and a one) */
{
    put_bits_routine (g, 12, 0x001);
}



/*____________________________________________________________________________
 |               |                                                            |
 | encode_row_1d | Converts a pixel-row into MH (CCITT G3) format             |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Before calling this routine, you must call put_init.                       |
 | After  calling this routine, you must call put_grab/put_done.              |
 | This routine puts the EOL at the beginning of the line.                    |
 |____________________________________________________________________________|
*/
static void encode_row_1d (
    ENC_INST *g,
    BYTE     *pbPixelRow, /* ptr to pixel-row */
    int       iPixels,    /* # of pixels in above row */
    BOOL      fDoingMR)   /* Sending MR?  Ie, send a 1d/2d tag-bit after EOL? */
{
    int    iStartPos;
    int    iChange;
    UINT   skip;    /* the color we're skipping over; 0x00=black, 0xFF=white */

    PutEOL (g);
    if (fDoingMR)
        put_bits_routine (g,1,1);  /* tag-bit after EOL means 1-dim row-data */

    pbPixelRow[iPixels>>3] = 0x55u;   /* scan_to requires this */
    iStartPos = 0;
    skip = 0;

    while (iStartPos < iPixels) {
        iChange = scan_to (skip, pbPixelRow, iStartPos, iPixels);
        if (skip) PutBlackRun (g, iChange-iStartPos);
        else      PutWhiteRun (g, iChange-iStartPos);
        iStartPos = iChange;
        skip = ~skip;
    }
}



/*____________________________________________________________________________
 |               |                                                            |
 | encode_row_2d | Converts a pixel-row into 2-dimensional format             |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Before calling this routine, you must call put_init.                       |
 | After  calling this routine, you must call put_grab/put_done.              |
 | For MR data, this routine puts the EOL+tag at the beginning of the line.   |
 |____________________________________________________________________________|
*/
/* 
 * The variable 'need' below is a bit-array telling us which values (a1, b0,
 * etc) are needed.  It is determined by the cases below.
 * 
 * b0 is not in the standard.  I've defined it as, "A pixel on the reference
 * line above or to the right of a0, and to the left of b1."  Since b1 is the
 * first changing pixel, b0 must be the same color as a0.  b0 is the point at
 * which the search for b1 begins.
 * 
 * In the code below, b0 uses the b1 variable because if we're using b0, then
 * b1 is not known, so it's okay to clobber it.
 * 
 * Below, a0, a1, b0, b1 etc denote positions before coding, and A0, A1,
 * B0, B1, etc denote positions after coding.
 * 
 * Pass Mode:
 * 
 *                           B0
 *                  b1       b2
 *         -  -  -  X  X  X  -  -  -
 *         X  -  -  -  -  -  -  X  -
 *            a0                a1
 *                           A0 A1
 * 
 *         A0 = b2, specified by the standard.
 *         A1 = a1, because A0 is to the left of a1, a1 does not move.
 *         B0 = b2, which is known to be the same color as A0.
 * 
 *         Needed:  B1 and B2.
 * 
 * Vertical Mode:
 * 
 *     Normal case:
 * 
 *                           B1
 *                  b1       b2
 *         -  -  -  X  X  X  -  -  -
 *         -  -  X  X  -  -  -  -  -
 *         a0    a1
 *               A0
 * 
 *         A0 = a1, specified by the standard.
 *         B1 = b2, the first changing pixel of opposite color as A0.
 * 
 *         Needed: A1, B2.
 * 
 *     An exception (a1=b2):
 * 
 *                  b1 b2
 *         -  -  -  X  -  -  -  -  -
 *         -  -  -  -  X  -  -  -  -
 *            a0       a1
 *                     A0
 * 
 *         Here, b2 is above A0, and therefore cannot be used as B1.
 *         And its color is opposite A0, so it's not even usable as B0.
 *         So B0 and B1 must be scanned.
 * 
 *         Needed: A1, B0, B1, B2.
 * 
 *     A subtle exception (a1 is at least 2 pixels to the left of b1):
 * 
 *                        B1 B2
 *                           b1 b2
 *         -  -  X  X  X  -  X  -  -
 *         -  -  -  -  X  -  -  -  -
 *               a0    a1
 *                     A0
 * 
 *         b1 and b2 move *backwards* after coding.
 *         B1 = b1-1, and B2=b1.  Since it's not worth the time to check
 *         for this rare pixel-arrangement, we'll just rescan B0, B1 and B2.
 * 
 *         Needed: A1, B0, B1, B2.
 * 
 * Horizontal Mode:
 * 
 *     First case (a2 > b2):
 * 
 *                        b1 b2
 *         -  -  -  -  -  X  -  -  -
 *         -  X  X  X  X  X  X  -  -
 *         a0 a1                a2
 *                              A0
 * 
 *         Since b2 is left of A0, we know nothing about what's above A0.
 *         So everything must be scanned.
 * 
 *         Needed: A1, B0, B1, B2.
 * 
 *     Second case: (a2 <= b2):
 * 
 *                                 B0
 *                        b1       b2
 *         -  -  -  -  -  X  X  X  -
 *         -  X  X  X  X  X  X  -  -
 *         a0 a1                a2
 *                              A0
 * 
 *         A0 = a2, specified by the standard.
 *         B0 = b2, because it's the same color as A0, and not to left of A0.
 * 
 *         Needed:  A1, B1, B2.
 * 
 *     Exception to above case (a2 < b1):
 * 
 *                           B1    B2
 *                           b1    b2
 *         -  -  -  -  -  -  X  X  -
 *         -  X  X  X  X  -  -  -  -
 *         a0 a1          a2
 *                        A0
 * 
 *         b1 is to the right of A0, so b1 and b2 don't move.
 *         This case is common because it occurs whenever a row with some
 *         data follows a blank row.
 * 
 *         Needed: A1.
 */
static void encode_row_2d (
    ENC_INST *g,
    BYTE     *pbPixelRow,  /* ptr to pixel-row                         */
    BYTE     *pbRefRow,    /* ptr to reference-row                     */
    int       iPixels,     /* # of pixels in above row                 */
    BOOL      fDoingMR)    /* Sending MR? Ie, output an EOL + tag-bit? */
{
    #define A1 1
    #define B0 2
    #define B1 4
    #define B2 8

    int   a0, a1, a2;
    int   b1, b2;
    int   iDelta;
    UINT  skip;      /* the color we're skipping over; 00=white, FF=black */
    UINT  need;

    if (fDoingMR) {
        PutEOL (g);
        put_bits_routine (g,1,0);  /* tag-bit after EOL means 2-dim row-data */
    }

    pbPixelRow[iPixels>>3] = 0x55u;   /* scan_to requires this */
    pbRefRow  [iPixels>>3] = 0x55u;   /* scan_to requires this */

    /* The imaginary pixel before the first is considered a white pixel.
     * So if the first pixel in the row is black, it is considered
     * a "changing" pixel (white->black).
     */
    a1 = scan_to (       0x00, pbPixelRow, 0,  iPixels);
    b1 = scan_to (       0x00, pbRefRow,   0,  iPixels);
    b2 = scan_to ((UINT)~0x00, pbRefRow, b1+1, iPixels);
    skip = (UINT)~0x00u;   /* white, initially */
    a0 = 0;

    while (TRUE) {

        /* output one of the modes */

        iDelta = a1 - b1;

        if (b2 < a1) {    /* pass mode */
            put_bits (g,4,1);
            need = B1 | B2;
            a0 = b2;
            b1 = b2;
        } else if (-3<=iDelta && iDelta<=3) {    /* vertical mode */
            huff_t te = VertTbl[iDelta+3];
            put_bits (g, te.length, te.bits);
            need = A1 | B2;
            if (b2==a1 || iDelta<=-2)
                need = A1 | B0 | B1 | B2;
            a0 = a1;
            b1 = b2;
            skip = ~skip;
        } else {          /* horizontal mode */
            a2 = scan_to (skip, pbPixelRow, a1+1, iPixels);
            put_bits (g,3,1);
            if (skip) {
                PutWhiteRun (g,a1-a0);
                PutBlackRun (g,a2-a1);
            } else {
                PutBlackRun (g,a1-a0);
                PutWhiteRun (g,a2-a1);
            }
            if (a2 > b2) {
                need = A1 | B0 | B1 | B2;
            } else if (a2 < b1) {
                need = A1;
            } else {
                b1 = b2;
                need = A1 | B1 | B2;
            }
            a0 = a2;
        }

        if (a0 >= iPixels)
            break;

        /* compute next a1, b1, b2 */

        if (need & A1) a1 = scan_to (~skip, pbPixelRow, a0+1, iPixels);
        if (need & B0) b1 = scan_to ( skip, pbRefRow,   a0,   iPixels);
        if (need & B1) b1 = scan_to (~skip, pbRefRow,   b1+1, iPixels);
        if (need & B2) b2 = scan_to ( skip, pbRefRow,   b1+1, iPixels);
    }
}



/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@                                                                          @@
@@                                                                          @@
@@                           E  N  C  O  D  E  R                            @@
@@                                                                          @@
@@                                                                          @@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/



/*****************************************************************************\
 *
 * faxEncode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD faxEncode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PENC_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(ENC_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(ENC_INST));
    g->dwValidChk = CHECK_VALUE;
    put_init (g, NULL);   /* put_new_buf will be called later */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncode_setDefaultInputTraits - Specifies default input image traits
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

static WORD faxEncode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PENC_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we actually use or care about are known */
    INSURE (pTraits->iPixelsPerRow > 0);   /* we need the row-length */
    INSURE (pTraits->iBitsPerPixel == 1);  /* image must be bi-level */

    g->traits = *pTraits;   /* a structure copy */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD faxEncode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PENC_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->wOutFmt  = (BYTE)aXformInfo[IP_FAX_FORMAT].dword;
    g->fNoEOLs  = (BOOL)aXformInfo[IP_FAX_NO_EOLS].dword;
    g->wMinBits = (WORD)aXformInfo[IP_FAX_MIN_ROW_LEN].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD faxEncode_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,          /* in:  handle for xform */
    DWORD           *pdwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * faxEncode_getActualTraits - Parses header, and returns input & output traits
 *
 *****************************************************************************
 *
 * For this fax xform driver, this routine merely returns input traits.
 *
\*****************************************************************************/

static WORD faxEncode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PENC_INST g;
    int       inBytes;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */

    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Since we don't change traits, just copy out the default traits */

    *pInTraits  = g->traits;
    *pOutTraits = g->traits;

    /* Compute some stuff */

    g->iRowLen = g->traits.iPixelsPerRow;   /* todo: eliminate redundant var */

    /* below, if vert dpi is unknown (negative), we use cycle-len of 2 */
    g->w12Cycle = (g->traits.lVertDPI < (150l<<16)) ? 2 : 4;

    /* Allocate the prior-row buffer, if needed */

    if (g->wOutFmt != IP_FAX_MH) {
        if (g->prior_p != NULL)
            IP_MEM_FREE (g->prior_p);
        inBytes = (g->iRowLen+7) / 8;
        IP_MEM_ALLOC (inBytes, g->prior_p);
        memset (g->prior_p, 0, inBytes);
    }

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * faxEncode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD faxEncode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,           /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PENC_INST g;
    UINT      uWorstBuf, uMinBytes;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen = (g->iRowLen+7) / 8;

    uWorstBuf = worst_buf_size (g->wOutFmt, g->iRowLen);
    uMinBytes = (g->wMinBits+7) / 8;
    *pdwMinOutBufLen = uWorstBuf > uMinBytes ? uWorstBuf : uMinBytes;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncode_convert - the work-horse routine
 *
 *****************************************************************************
 *
 * This routine (actually put_bits) hangs onto the last 1-3 bytes of
 * encoded row-data due to its buffering method.
 *
\*****************************************************************************/

static WORD faxEncode_convert (
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
    PENC_INST g;
    int       inBytes;
    int       i;

    HANDLE_TO_PTR (hXform, g);

    put_new_buf (g, pbOutputBuf);

      /********************************************************/
     /* If we're being told to flush, output the ending EOLs */
    /********************************************************/

    if (dwInputAvail == 0) {
    
        switch (g->wOutFmt) {
            case IP_FAX_MH:
                for (i=6; i>0; i--)
                    PutEOL (g);
            break;
    
            case IP_FAX_MR:
                for (i=6; i>0; i--) {
                    PutEOL (g);
                    put_bits_routine (g,1,1);
                }
            break;
    
            case IP_FAX_MMR:
                PutEOL (g);
                PutEOL (g);
            break;
        }
    
        *pdwInputUsed     = 0;
        *pdwOutputUsed    = put_done (g);
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

      /******************************/
     /* Normal Case (not flushing) */
    /******************************/

    inBytes = (g->iRowLen+7) / 8;
    INSURE (dwInputAvail  >= (DWORD)inBytes);
    INSURE (dwOutputAvail >  0);

    switch (g->wOutFmt) {
        case IP_FAX_MH:
            encode_row_1d (g, pbInputBuf, g->iRowLen, FALSE);
            put_fill_bits (g);
        break;

        case IP_FAX_MR:
            if (g->iRowNum % g->w12Cycle == 0)
                encode_row_1d (g, pbInputBuf, g->iRowLen, TRUE);
            else
                encode_row_2d (g, pbInputBuf, g->prior_p, g->iRowLen, TRUE);
            put_fill_bits (g);
        break;

        case IP_FAX_MMR:
            encode_row_2d (g, pbInputBuf, g->prior_p, g->iRowLen, FALSE);
        break;
    }

    if (g->prior_p != NULL)
        memcpy (g->prior_p, pbInputBuf, inBytes);

    *pdwInputUsed     = inBytes;
    g->dwInNextPos   += inBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = put_grab (g);
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += *pdwOutputUsed;
    g->iRowNum += 1;

    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD faxEncode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * faxEncode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD faxEncode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PENC_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: output EOLs to mark a new page */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * faxEncode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD faxEncode_closeXform (IP_XFORM_HANDLE hXform)
{
    PENC_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->prior_p != NULL)
        IP_MEM_FREE (g->prior_p);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxEncodeTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL faxEncodeTbl = {
    faxEncode_openXform,
    faxEncode_setDefaultInputTraits,
    faxEncode_setXformSpec,
    faxEncode_getHeaderBufSize,
    faxEncode_getActualTraits,
    faxEncode_getActualBufSizes,
    faxEncode_convert,
    faxEncode_newPage,
    faxEncode_insertedData,
    faxEncode_closeXform
};



/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@                                                                          @@
@@                                                                          @@
@@                           D  E  C  O  D  E  R                            @@
@@                                                                          @@
@@                                                                          @@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/


/* white/nonwhite might be put in interface later on */
#define NONWHITE_ROW   0x0000   /* row is not all white */
#define    WHITE_ROW   0x0000   /* row is all white */

#define MAX_CODE_LEN   13  /* length of longest code */
#define EOL_LEN        12  /* EOL is 11 zeroes and a one */
#define EOLS_FOR_MH_MR 3   /* the std says 6, but some might be zapped */
#define EOLS_FOR_MMR   1   /* the std says 2, but we'll stop at first because
                            * we might not be able to fetch the second EOL from
                            * the cache because we won't fetch anything unless
                            * it contains 13 bits, and an EOL is only 12 */

/* Huffman tables (at end of file): */

extern const BYTE   fax_vert_huff_index[];
extern const USHORT fax_vert_huff[];
extern const BYTE   fax_black_huff_index[];
extern const USHORT fax_black_huff[];
extern const BYTE   fax_white_huff_index[];
extern const USHORT fax_white_huff[];

#define MAX_BLACK_CODELEN  13
#define MAX_WHITE_CODELEN  12
#define MAX_VERT_CODELEN   7

#define CODELEN_SHIFT  12
#define VALUE_MASK     0x0fffu

/* items only in fax_vert_huff table: */
#define LAST_VERT  6
#define PASS_MODE  7
#define HORIZ_MODE 8

enum {
    RET_GOT_CODE,      /* we parsed a good code (ret in *piResult)         */
    RET_BAD_CODE,      /* trash in row-data                                */
    RET_FILL,          /* got some fill-zeroes                             */
    RET_HIT_EOL,       /* hit EOL; no row-data was parsed or returned      */
    RET_NEED_MORE      /* need more input-bytes to complete the row        */
};



/*____________________________________________________________________________
 |                                                                            |
 | Type-definition of our instance-variables                                  |
 |____________________________________________________________________________|
*/

typedef enum {
    NORMAL_2D,
    HORIZ_1ST,
    HORIZ_2ND
} STATE_2D;

typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the image                          */
    DWORD    dwInNextPos;     /* file pos for subsequent input                */
    DWORD    dwOutNextPos;    /* file pos for subsequent output               */
    DWORD    dwValidChk;      /* struct validity check value                  */

    /* Variables for getting bits: */
    BYTE    *gb_buf_p;        /* beginning of our buffer                      */
    BYTE    *gb_buf_after_p;  /* 1st byte after our buffer                    */
    BYTE    *gb_byte_p;       /* ptr to next byte                             */
    int      gb_cache_cnt;    /* # of available bits in gb_cache              */
    DWORD    gb_cache;        /* 32-bit buffer to cache the next few bits     */
                              /* is also pos of next avail bit; msb=32, lsb=1 */
    int      gb_num_zeroes;   /* # of successive zero-bits we've gotten       */

    /* Variables for row-decoding functions: */
    int      pixel_pos;       /* coordinate of next pixel; 0 is leftmost      */
    BYTE     white;           /* doing a white run? (00=black, FF=white)      */
    STATE_2D state_2d;        /* state of the 2-dim decoder                   */
    int      a0;              /* pixel before 1st is an imaginary white pixel */
    BYTE    *prior_p;         /* buffer containing prior row                  */
    BOOL     ref_row_invalid; /* (MR only) reference row invalid due to error?*/

    /* Variables for the exported functions: */
    BYTE     input_format;    /* input format (IP_FAX_MH/MR/MMR)              */
    BYTE     num_eols;        /* number of successive EOLs we've gotten       */
    BOOL     no_eols;         /* are EOLs not present in input?               */
    BOOL     toss_everything; /* are we discarding all data due to prior err? */
    BOOL     flushing_to_eol; /* are we ignoring bits until an EOL?           */
    BOOL     got_fill;        /* gotten any fill-zeroes?                      */
    BOOL     got_black;       /* set any black pixels in the row?             */
    BOOL     two_dim;         /* next row is 2-dimensional encoding?          */
    int      row_len;         /* # pixels in each row                         */
    int      bytes_in_row;    /* # bytes in each row                          */
} DEC_INST, *PDEC_INST;



/*****************************************************************************
 *                                                                           *
 *                       F E T C H I N G   B I T S                           *
 *                                                                           *
 *****************************************************************************


Interface into this section:

    bits_init         - inits this section
    bits_buf_open     - gives us (this section) a buffer to consume
    BITS_REFILL_CACHE - fills cache; must be called before parsing
    BITS_IN_CACHE     - returns # bits currently in the cache
    BITS_LOAD         - returns next N bits of input (no advance is done)
    BITS_ADVANCE      - advances input by the given # of bits
    bits_buf_close    - returns # bytes consumed in buffer
    bits_flush        - discards all unread bits
    bits_flush_to_eol - flushes input bits until EOL is encountered
*/



/*____________________________________________________________________________
 |           |                                                                |
 | bits_init | initializes this "fetching bits" section                       |
 |___________|________________________________________________________________|
*/
static void bits_init (DEC_INST *g)
{
    g->gb_num_zeroes = 0;
    g->gb_cache_cnt = 0;    /* the cache is empty */
}



/*____________________________________________________________________________
 |               |                                                            |
 | bits_buf_open | gives us (this section) a buffer to consume                |
 |_______________|____________________________________________________________|
*/
static void bits_buf_open (
    DEC_INST  *g,
    BYTE      *buf_p,
    int        num_bytes)
{
    g->gb_byte_p      = buf_p;
    g->gb_buf_p       = buf_p;
    g->gb_buf_after_p = buf_p + num_bytes;
}



/*____________________________________________________________________________
 |                |                                                           |
 | bits_buf_close | returns # bytes consumed in buffer                        |
 |________________|___________________________________________________________|
*/
static UINT bits_buf_close (DEC_INST *g)
{
    return (g->gb_byte_p - g->gb_buf_p);
}



/*____________________________________________________________________________
 |            |                                                               |
 | bits_flush | discards all unread bits                                      |
 |____________|_______________________________________________________________|
*/
static void bits_flush (DEC_INST *g)
{
    g->gb_cache_cnt = 0;
    g->gb_byte_p = g->gb_buf_after_p;
}



/*____________________________________________________________________________
 |                   |                                                        |
 | BITS_REFILL_CACHE | fills cache as full as possible                        |
 |___________________|________________________________________________________|
*/
#define BITS_REFILL_CACHE(g)                                        \
{                                                                   \
    int    cache_cnt   = g->gb_cache_cnt;                           \
    DWORD  cache       = g->gb_cache;                               \
    BYTE  *byte_p      = g->gb_byte_p;                              \
    BYTE  *buf_after_p = g->gb_buf_after_p;                         \
                                                                    \
    while (cache_cnt<=24 && byte_p<buf_after_p) {                   \
        cache = (cache << 8) | (*byte_p++);                         \
        cache_cnt += 8;                                             \
    }                                                               \
                                                                    \
    g->gb_cache_cnt = cache_cnt;                                    \
    g->gb_cache     = cache;                                        \
    g->gb_byte_p    = byte_p;                                       \
}



/*____________________________________________________________________________
 |               |                                                            |
 | BITS_IN_CACHE | returns # bits currently in the bit-cache                  |
 |_______________|____________________________________________________________|
*/
#define BITS_IN_CACHE(g)  (g->gb_cache_cnt)



/*____________________________________________________________________________
 |                   |                                                        |
 | bits_flush_to_eol | flushes input bits until EOL is encountered            |
 |___________________|________________________________________________________|
 |                                                                            |
 | If got_fill is TRUE, then we merely scan for a set bit.                    |
 | Otherwise, we first count leading zeroes until it reaches 11.              |
 |                                                                            |
 | Return value:  TRUE  = We hit an EOL.  In this case, if leave_a_bit is     |
 |                        TRUE, the cache will contain at least one bit so    |
 |                        you can fetch a 1-dim/2-dim bit.                    |
 |                FALSE = We need more input data.                            |
 |____________________________________________________________________________|
*/
static BOOL bits_flush_to_eol (
    DEC_INST *g,         /* our instance vars */
    BOOL    got_fill,    /* have we gotten fill zeroes? */
    BOOL    leave_a_bit) /* after hitting EOL, insure that cache isn't empty? */
{
    #define CLEAR_UNUSED_CACHE_BITS                             \
        if (g->gb_cache_cnt < 32)                               \
            g->gb_cache &= (1lu << g->gb_cache_cnt) - 1lu;

    DWORD bit;
    BYTE  byt;

      /*********************************************/
     /* Scan input until 11 zeroes have been seen */
    /*********************************************/

    if (g->gb_num_zeroes >= 11)
        got_fill = TRUE;

    if (! got_fill)
    {
        if (g->gb_cache_cnt != 0) {
            for (bit = (1lu<<(g->gb_cache_cnt-1));
                 bit != 0;
                 bit >>= 1) {
                g->gb_cache_cnt -= 1;
                if (bit & g->gb_cache)
                    g->gb_num_zeroes =0;
                else {
                    g->gb_num_zeroes += 1;
                    if (g->gb_num_zeroes >= 11)
                        break;
                }
            }
        }

        /* the cache is now empty; start scanning bytes */

        while (TRUE) {
            if (g->gb_byte_p >= g->gb_buf_after_p)
                return FALSE;
            byt = *(g->gb_byte_p)++;
            g->gb_num_zeroes += baLeftZeroesTbl[byt];
            if (g->gb_num_zeroes >= 11) {
                g->gb_byte_p -= 1;
                break;
            }
            if (byt != 0)
                g->gb_num_zeroes = baRightZeroesTbl[byt];
        }
    }

      /*******************************************/
     /* Scan input until a non-zero bit is seen */
    /*******************************************/

    while (TRUE)
    {
        BITS_REFILL_CACHE (g)
        if (g->gb_cache_cnt==0 || (leave_a_bit && g->gb_cache_cnt==1))
            return FALSE;

        CLEAR_UNUSED_CACHE_BITS

        if (g->gb_cache == 0)
            g->gb_cache_cnt = 0;   /* cache is all zeroes; discard it */
        else {
            /* we hit the EOL */
            for (bit = (1lu<<(g->gb_cache_cnt-1)); 
                 (bit & g->gb_cache) == 0;
                 bit >>= 1)
                g->gb_cache_cnt -= 1;   /* discard the 0's before the 1 */

            /* After discarding the set bit, if leave_a_bit is TRUE, we want
             * the cache to be non-empty so that a 1-dim/2-dim bit can then
             * be fetched.  Hence the check for 2 bits in cache below.
             */
            if (!leave_a_bit || g->gb_cache_cnt>=2) {
                g->gb_cache_cnt -= 1;  /* discard the set bit we found above */
                g->gb_num_zeroes = 0;
                return TRUE;
            }
        }

        /* discard zero bytes */

        /* Warning: If fax_decode_convert_row was told to flush, both
         * pointers gb_byte_p and gb_buf_after_p can be NULL.  So the
         * pointer-compare below must be *before* the dereference in the
         * test of the while loop, to avoid dereferencing a NULL pointer.
         */
        if (g->gb_cache_cnt == 0)
            while (g->gb_byte_p<g->gb_buf_after_p && *(g->gb_byte_p)==0)
                g->gb_byte_p++;
    }
}



/*____________________________________________________________________________
 |           |                                                                |
 | BITS_LOAD | returns the next num_bits of input, with NO advance            |
 |___________|________________________________________________________________|
*/
#define BITS_LOAD(g, num_bits, par_result) {                                 \
    int n_bits = (int)(num_bits);                                            \
                                                                             \
    par_result = (g->gb_cache >> (g->gb_cache_cnt-n_bits))                   \
                 & ((1u<<n_bits) - 1u);                                      \
}

#if 0

#define BITS_LOAD(g, num_bits, par_result) {                                 \
    int n_bits = (int)(num_bits);                                            \
                                                                             \
    par_result = g->gb_cache;                                                \
                                                                             \
    asm ("extract %1,%2,%0"                                                  \
        : "=d" (par_result)                                                  \
        : "dI" (g->gb_cache_cnt - n_bits), "dI" (n_bits), "0" (par_result)); \
}

#endif



/*____________________________________________________________________________
 |              |                                                             |
 | BITS_ADVANCE | advances input by num_bits bits                             |
 |______________|_____________________________________________________________|
*/
#define BITS_ADVANCE(g, num_bits) {             \
    g->gb_cache_cnt -= (num_bits);              \
}



/*****************************************************************************
 *                                                                           *
 *                           U T I L I T I E S                               *
 *                                                                           *
 *****************************************************************************/



/*____________________________________________________________________________
 |                    |                                                       |
 | parse_code_routine | Parses a Huffman code using the given code tables     |
 |____________________|_______________________________________________________|
 |                                                                            |
 | Function return values:  RET_GOT_CODE                                      |
 |                          RET_BAD_CODE                                      |
 |                          RET_FILL                                          |
 |                          RET_HIT_EOL                                       |
 |                          RET_NEED_MORE                                     |
 |                                                                            |
 | This function must NOT be called to scan for an EOL after it has           |
 | returned RET_BAD_CODE or RET_FILL.                                         |
 |                                                                            |
 | Warning:  When this returns RET_HIT_EOL, it must guarantee that the        |
 | bit-cache is not empty so a 1-dim/2-dim bit can be fetched.                |
 |____________________________________________________________________________|
*/

static UINT parse_code_routine (
    DEC_INST    *g,
    int          bits_in_index, /* in:  # bits in index for index_tbl */
    const BYTE   index_tbl[],   /* in:  contains indices into value_tbl */
    const USHORT value_tbl[],   /* in:  contains [codelen, value] pairs */
    int         *value_p)       /* out: the value corresponding to the code */
{
    UINT blob, values;

    BITS_REFILL_CACHE(g)
    if (BITS_IN_CACHE(g) < bits_in_index)
        return RET_NEED_MORE;

    BITS_LOAD (g, bits_in_index, blob);
    values = value_tbl[index_tbl[blob]];

    if (values != 0) {
        /* normal case: we got a valid code */
        BITS_ADVANCE (g, values >> CODELEN_SHIFT);
        *value_p = values & VALUE_MASK;
        return RET_GOT_CODE;
    }

    if (BITS_IN_CACHE(g) < MAX_CODE_LEN)
        return RET_NEED_MORE;

    BITS_LOAD (g, EOL_LEN, blob);

    if (blob == 1) {
        BITS_ADVANCE (g, EOL_LEN);
        return RET_HIT_EOL;
    }

    if (blob == 0) {
        BITS_ADVANCE (g, EOL_LEN);
        return RET_FILL;
    }

    return RET_BAD_CODE;
}



/*____________________________________________________________________________
 |            |                                                               |
 | PARSE_CODE | A fast macro for parsing Huffman codes                        |
 |____________|_______________________________________________________________|
*/
#define PARSE_CODE(                                                           \
    g,                                                                        \
    bits_in_index,  /* in:  # bits in index for index_tbl */                  \
    index_tbl,      /* in:  contains indices into value_tbl */                \
    value_tbl,      /* in:  contains [codelen, value] pairs */                \
    out_result,     /* out: RET_GOT_CODE, RET_BAD_CODE, etc */                \
    out_value)      /* out: the value corresponding to the code */            \
do {                                                                          \
    UINT blob, values;                                                        \
                                                                              \
    if (BITS_IN_CACHE(g) < bits_in_index)                                     \
        goto call##value_tbl;                                                 \
                                                                              \
    BITS_LOAD (g, bits_in_index, blob);                                       \
    values = value_tbl[index_tbl[blob]];                                      \
                                                                              \
    if (values == 0) {                                                        \
        call##value_tbl:                                                      \
        out_result = parse_code_routine                                       \
                       (g, bits_in_index, index_tbl, value_tbl, &out_value);  \
    } else {                                                                  \
        BITS_ADVANCE (g, values >> CODELEN_SHIFT);                            \
        out_result = RET_GOT_CODE;                                            \
        out_value  = values & VALUE_MASK;                                     \
    }                                                                         \
} while (0)



/*____________________________________________________________________________
 |            |                                                               |
 | runs_array | all possible runs that fit in 16 bits                         |
 |____________|_______________________________________________________________|
 |                                                                            |
 | This array is indexed by (leftbit<<4)+rightbit.                            |
 | Leftbit and rightbit are in 0..15.  0=msb, 15=lsb.                         |
 |____________________________________________________________________________|
*/

static const USHORT runs_array_le[256] = {   /* little-endian array */
    0x0080, 0x00c0, 0x00e0, 0x00f0, 0x00f8, 0x00fc, 0x00fe, 0x00ff, 
    0x80ff, 0xc0ff, 0xe0ff, 0xf0ff, 0xf8ff, 0xfcff, 0xfeff, 0xffff, 
    0x0000, 0x0040, 0x0060, 0x0070, 0x0078, 0x007c, 0x007e, 0x007f, 
    0x807f, 0xc07f, 0xe07f, 0xf07f, 0xf87f, 0xfc7f, 0xfe7f, 0xff7f, 
    0x0000, 0x0000, 0x0020, 0x0030, 0x0038, 0x003c, 0x003e, 0x003f, 
    0x803f, 0xc03f, 0xe03f, 0xf03f, 0xf83f, 0xfc3f, 0xfe3f, 0xff3f, 
    0x0000, 0x0000, 0x0000, 0x0010, 0x0018, 0x001c, 0x001e, 0x001f, 
    0x801f, 0xc01f, 0xe01f, 0xf01f, 0xf81f, 0xfc1f, 0xfe1f, 0xff1f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0008, 0x000c, 0x000e, 0x000f, 
    0x800f, 0xc00f, 0xe00f, 0xf00f, 0xf80f, 0xfc0f, 0xfe0f, 0xff0f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0x0006, 0x0007, 
    0x8007, 0xc007, 0xe007, 0xf007, 0xf807, 0xfc07, 0xfe07, 0xff07, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0003, 
    0x8003, 0xc003, 0xe003, 0xf003, 0xf803, 0xfc03, 0xfe03, 0xff03, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 
    0x8001, 0xc001, 0xe001, 0xf001, 0xf801, 0xfc01, 0xfe01, 0xff01, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x7f00, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x2000, 0x3000, 0x3800, 0x3c00, 0x3e00, 0x3f00, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x1000, 0x1800, 0x1c00, 0x1e00, 0x1f00, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0800, 0x0c00, 0x0e00, 0x0f00, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0600, 0x0700, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0300, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 
};

static const USHORT runs_array_be[256] = {    /* big-endian array */
    0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00, 
    0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 
    0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x7f00, 
    0x7f80, 0x7fc0, 0x7fe0, 0x7ff0, 0x7ff8, 0x7ffc, 0x7ffe, 0x7fff, 
    0x0000, 0x0000, 0x2000, 0x3000, 0x3800, 0x3c00, 0x3e00, 0x3f00, 
    0x3f80, 0x3fc0, 0x3fe0, 0x3ff0, 0x3ff8, 0x3ffc, 0x3ffe, 0x3fff, 
    0x0000, 0x0000, 0x0000, 0x1000, 0x1800, 0x1c00, 0x1e00, 0x1f00, 
    0x1f80, 0x1fc0, 0x1fe0, 0x1ff0, 0x1ff8, 0x1ffc, 0x1ffe, 0x1fff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0800, 0x0c00, 0x0e00, 0x0f00, 
    0x0f80, 0x0fc0, 0x0fe0, 0x0ff0, 0x0ff8, 0x0ffc, 0x0ffe, 0x0fff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0600, 0x0700, 
    0x0780, 0x07c0, 0x07e0, 0x07f0, 0x07f8, 0x07fc, 0x07fe, 0x07ff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0300, 
    0x0380, 0x03c0, 0x03e0, 0x03f0, 0x03f8, 0x03fc, 0x03fe, 0x03ff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 
    0x0180, 0x01c0, 0x01e0, 0x01f0, 0x01f8, 0x01fc, 0x01fe, 0x01ff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0080, 0x00c0, 0x00e0, 0x00f0, 0x00f8, 0x00fc, 0x00fe, 0x00ff, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0040, 0x0060, 0x0070, 0x0078, 0x007c, 0x007e, 0x007f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0020, 0x0030, 0x0038, 0x003c, 0x003e, 0x003f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0010, 0x0018, 0x001c, 0x001e, 0x001f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0008, 0x000c, 0x000e, 0x000f, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0x0006, 0x0007, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0003, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 
};

static const USHORT *runs_array = runs_array_le;

static void initRunArray(void) {
    int iTest=1;
    char *pcTest=((char *)(&iTest));

    if (*pcTest==1) {
        runs_array = runs_array_le;
    } else {
        runs_array = runs_array_be;
    }
}


/*____________________________________________________________________________
 |         |                                                                  |
 | set_run | Sets a run of pixels to black                                    |
 |_________|__________________________________________________________________|
 |                                                                            |
 | This routine does bounds-checking to avoid setting any pixels outside the  |
 | given buffer.                                                              |
 |                                                                            |
 | Since this is called often, it's written to be as fast as I could make it. |
 |____________________________________________________________________________|
*/
static void set_run (
    BYTE   *buf_p,      /* buffer in which we're to set the run          */
    int     start_pos,  /* pixel-index of left side of run               */
    int     run_len,    /* # of pixels in the run (non-negative)         */
    int     row_len)    /* # of pixels in the buffer                     */
{
    /*************************************************************************
     *
     * PERFORMANCE NOTE
     *
     * Inlining this routine slows it down a little.
     * Seconds to MMR encode+decode 1000 rows of 1728 pixels (bench_fax.c):
     *
     *    black density:           0%    10%     20%     50%
     *    calling this routine:   2.2     7.4    12.8    29.9
     *    inlining this routine:  2.2     7.5    13.2    30.6
     *
     * Inlining this routine *hurt* performance a little because more MMR
     * code was kicked out of the cache.
     *
     *************************************************************************/

    int    end_pos;     /* pixel-index of rightmost pixel in run         */
    int    start_index; /* byte-index of first byte in run               */
    BYTE  *left_p;      /* ptr to byte containing leftmost pixel in run  */
    BYTE  *right_p;     /* ptr to byte containing rightmost pixel in run */

    end_pos = start_pos + run_len - 1;

    if (end_pos >= row_len)
        end_pos = row_len - 1;

    start_index = start_pos >> 4;

    if ((end_pos >> 4) == start_index) {
        /* the run is contained within an even-aligned 2-byte word */
        ((USHORT*)buf_p)[start_index] |=
            runs_array [((start_pos & 15)<<4) | (end_pos & 15)];
    } else if (end_pos > start_pos) {
        /* the run spans two or more bytes */
        left_p  = buf_p + (start_pos >> 3);
        right_p = buf_p + (end_pos   >> 3);
    
        *left_p  |= (BYTE )0xFFu >> (UINT)(      start_pos & 7u );
        *right_p  = (BYTE )0xFFu << (UINT)(7u - (end_pos   & 7u));
        left_p++;

        while (left_p < right_p)
            *left_p++ = 0xFFu;
    }
}


/*****************************************************************************
 *                                                                           *
 *                          DECODING ROW-DATA                                *
 *                                                                           *
 *****************************************************************************


 Interface into this section:

        decode_row_init - inits instance-vars for this section
        decode_row_1d   - parses a row of 1-dim data
        decode_row_2d   - parses a row of 2-dim data

 The parsing routines return these bit-values:

        IP_PRODUCED_ROW
        IP_INPUT_ERROR
        DECODE_HIT_EOL  (defined below, not in the public interface)
        DECODE_HIT_FILL (ditto)

 A return-value of zero (ie, none of the above bits are set) means that the
 routine wants to be called again with more input-bytes.

 These routines fetch input by calling BITS_LOAD, so bits_init and
 bits_buf_open must have already been called.
*/

#define DECODE_HIT_EOL  0x4000u
#define DECODE_HIT_FILL 0x8000u


/*____________________________________________________________________________
 |                 |                                                          |
 | decode_row_init | inits instance-variables for this section                |
 |_________________|__________________________________________________________|
*/
static void decode_row_init (DEC_INST *g)
{
    g->pixel_pos = 0;
    g->white = 0xFFu;  /* start with a white run */
    g->state_2d = NORMAL_2D;
    g->a0 = -1;  /* pixel before 1st is an imaginary white pixel */
    g->got_black = FALSE;   /* haven't gotten any black pixels */
}



/*____________________________________________________________________________
 |               |                                                            |
 | decode_row_1d | Decodes a row of MH (1-dimensional G3) data                |
 |_______________|____________________________________________________________|
*/
static UINT decode_row_1d (
    DEC_INST *g,             /* in:  pointer to our instance-variables    */
    BYTE     *pbPrevOutBuf,  /* in:  prev output row (for error-handling) */
    BYTE     *pbOutBuf)      /* out: output buf                           */
{
    int           run_len;        /* # pixels in current run */
    UINT          white;
    int           pixel_pos;
    UINT          result;
    UINT          ret_val = 0;   /* init to zap a compiler warning */
    int           index_length;
    const BYTE   *index_tbl_p;
    const USHORT *value_tbl_p;

    if (g->pixel_pos == 0) {
        memset (pbOutBuf, 0, g->bytes_in_row);   /* set whole row to white */
        g->ref_row_invalid = TRUE;
    }

    white = (UINT)(int )(signed char)g->white;  /* must be all 0's or all 1's */
    pixel_pos = g->pixel_pos;

    while (TRUE) {

        if (white) {
            index_length = MAX_WHITE_CODELEN;
            index_tbl_p = fax_white_huff_index;
            value_tbl_p = fax_white_huff;
        } else {
            index_length = MAX_BLACK_CODELEN;
            index_tbl_p = fax_black_huff_index;
            value_tbl_p = fax_black_huff;
        }

        PARSE_CODE (g, index_length, index_tbl_p, value_tbl_p, result, run_len);
        PRINT (_T("pc result=%d, len=%d\n"), result, run_len);

        if (result == RET_GOT_CODE) {
            /* todo: below, we call set_run for EVERY make-up code (slow) */
            if (! white) {
                set_run (pbOutBuf, pixel_pos, run_len, g->row_len);
                g->got_black = TRUE;
            }

            pixel_pos += run_len;
            if (run_len <= 63)  /* this is a final run (not a make-up) */
                white = ~white;
            continue;   /* go to top of main 'while' loop */
        }

        switch (result) {    /* handle unusual condition */

            case RET_NEED_MORE:
                ret_val = 0;
                goto bail_out;
                break;

            case RET_BAD_CODE:
                memcpy (pbOutBuf, pbPrevOutBuf, g->bytes_in_row);
                ret_val = IP_PRODUCED_ROW | IP_INPUT_ERROR;
                goto bail_out;
                break;

            case RET_FILL:
            case RET_HIT_EOL:
                ret_val = result==RET_FILL ? DECODE_HIT_FILL : DECODE_HIT_EOL;
                if (pixel_pos == 0)
                    goto bail_out;
                if (pixel_pos == g->row_len) {
                    ret_val |= IP_PRODUCED_ROW;
                    g->ref_row_invalid = FALSE;
                    goto bail_out;
                }
                memcpy (pbOutBuf, pbPrevOutBuf, g->bytes_in_row);
                ret_val |= IP_PRODUCED_ROW | IP_INPUT_ERROR;
                goto bail_out;
                break;

            default:
                assert (FALSE);
        } /* end of switch */
    } /* end of while */

    bail_out:

    g->white     = white;
    g->pixel_pos = pixel_pos;

    return ret_val;
}



/*____________________________________________________________________________
 |               |                                                            |
 | decode_row_2d | Decodes a row of MR/MMR (2-dimensional) data               |
 |_______________|____________________________________________________________|
*/
static UINT decode_row_2d (
    DEC_INST *g,             /* in:  pointer to our instance-variables */
    BYTE     *pbPrevBuf,     /* in:  prev output row ("reference row") */
    BYTE     *pbOutBuf)      /* out: output buffer                     */
{
    UINT  group4;
    int   iAction;
    UINT  white;
    UINT  result = 0;   /* the 0 eliminates a compiler warning */
    int   row_len;
    int   run_len;
    int   a0, a1;
    int   b1=0, b2;     /* the 0 eliminates a compiler warning */
    UINT  need_b0;
    UINT  ret_val = 0;  /* the 0 eliminates a compiler warning */

    group4 = (g->input_format == IP_FAX_MMR);
    row_len = g->row_len;
    white = (UINT)(int)(signed char)g->white;  /* must be all 0's or all 1's so sign extend */
    a0 = g->a0;

    if (a0 < 0) {
        memset (pbOutBuf, 0, g->bytes_in_row);   /* set whole row to white */
        pbPrevBuf[g->bytes_in_row] = 0x55u;   /* scan_to requires this */
    }

    while (TRUE) {

          /****************************************/
         /* Process a usual code -- normal state */
        /****************************************/

        need_b0 = TRUE;

        while (g->state_2d == NORMAL_2D) {

            if (a0>=row_len && group4) {
                /* ret_val = IP_PRODUCED_ROW; goto bail_out;
                 * todo: undelete the line above? */
                if (a0 == row_len) ret_val = IP_PRODUCED_ROW;
                else               ret_val = IP_INPUT_ERROR;
                goto bail_out;
            }

            PARSE_CODE (g, MAX_VERT_CODELEN, fax_vert_huff_index,
                        fax_vert_huff, result, iAction);
            if (result != RET_GOT_CODE)
                goto unusual_condition;

            if (iAction == HORIZ_MODE) {
                g->state_2d = HORIZ_1ST;
                break;
            }

            if (a0 < 0) {
                a0 = 0;
                b1 = -1;   /* a kludge so scan below will start at pixel 0 */
            } else if (need_b0)
                b1 = scan_to (white, pbPrevBuf, a0, row_len);

            b1 = scan_to (~white, pbPrevBuf, b1+1, row_len);

            if (iAction <= LAST_VERT) {   /* vertical mode */
                iAction -= 3;
                a1 = b1 + iAction;
                need_b0 = (iAction<-1 || iAction>0);
                if (! white) {
                    set_run (pbOutBuf, a0, a1-a0, row_len);
                    g->got_black = TRUE;
                }
                if (a1<a0 || a1>row_len) goto corrupt;
                a0 = a1;
                white = ~white;
            } else {     /* pass mode */
                b2 = scan_to (white, pbPrevBuf, b1+1, row_len);
                if (! white) {
                    set_run (pbOutBuf, a0, b2-a0, row_len);
                    g->got_black = TRUE;
                }
                need_b0 = FALSE;
                a0 = b2;
                b1 = b2;
            }
        } /* while NORMAL_2D */

          /*******************************************/
         /* Process a usual code -- horizontal mode */
        /*******************************************/

        while (TRUE) {   /* HORIZ_1ST or HORIZ_2ND */
            int           index_length;
            const BYTE   *index_tbl_p;
            const USHORT *value_tbl_p;

            do {
                if (white) {
                    index_length = MAX_WHITE_CODELEN;
                    index_tbl_p = fax_white_huff_index;
                    value_tbl_p = fax_white_huff;
                } else {
                    index_length = MAX_BLACK_CODELEN;
                    index_tbl_p = fax_black_huff_index;
                    value_tbl_p = fax_black_huff;
                }

                PARSE_CODE (g, index_length, index_tbl_p, value_tbl_p,
                            result, run_len);

                if (result != RET_GOT_CODE)
                    goto unusual_condition;
                if (a0 < 0)  /* Exception:  See Fascicle VII.3, */
                    a0 = 0;  /*   rec T.4, section 4.2.1.3.4 */
                /* todo: below, we call set_run for EVERY make-up code (slow) */
                if (! white) {
                    set_run (pbOutBuf, a0, run_len, row_len);
                    g->got_black = TRUE;
                }
                a0 += run_len;
            } while (run_len > 63);

            white = ~white;
            if (g->state_2d == HORIZ_1ST)
                g->state_2d = HORIZ_2ND;
            else {
                g->state_2d = NORMAL_2D;
                if (a0 > row_len) goto corrupt;
                break;   /* exit while */
            }
        }

        continue;   /* no unusual condition, so resume main while loop */

          /*****************************/
         /* Handle unusual conditions */
        /*****************************/

        corrupt:
            result = RET_BAD_CODE;

        unusual_condition:

        switch (result) {

            case RET_GOT_CODE:
                /* normal case: was handled in the block of code above */
                break;

            case RET_NEED_MORE:
                ret_val = 0;
                goto bail_out;
                break;

            case RET_BAD_CODE:
                g->state_2d = NORMAL_2D;
                memcpy (pbOutBuf, pbPrevBuf, g->bytes_in_row);
                ret_val = IP_PRODUCED_ROW | IP_INPUT_ERROR;
                g->ref_row_invalid = TRUE;
                goto bail_out;
                break;

            case RET_FILL:
            case RET_HIT_EOL:
                ret_val = result==RET_FILL ? DECODE_HIT_FILL : DECODE_HIT_EOL;
                g->state_2d = NORMAL_2D;

                if (group4) {
                    /* We should only hit EOL when no row-data is present */
                    /* todo:  Always report error if fill-zeroes were seen? */
                    if (a0 >= 0)
                        ret_val |= IP_INPUT_ERROR;
                } else {
                    if (!g->ref_row_invalid && a0==row_len)
                        ret_val |= IP_PRODUCED_ROW;
                    else if (a0 >= 0) {
                        memcpy (pbOutBuf, pbPrevBuf, g->bytes_in_row);
                        ret_val |= IP_PRODUCED_ROW | IP_INPUT_ERROR;
                        g->ref_row_invalid = TRUE;
                    }
                }
                goto bail_out;
                break;

            default:
                assert (FALSE);

        } /* end of switch to handle unusual condition */
    } /* end of while */

    bail_out:

    /* save variables needed in next call */
    g->a0 = a0;
    g->white = white;

    return ret_val;
}



/*****************************************************************************\
 *
 * faxDecode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD faxDecode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PDEC_INST g;

    initRunArray();
    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(DEC_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(DEC_INST));
    g->dwValidChk = CHECK_VALUE;

    bits_init (g);
    decode_row_init (g);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecode_setDefaultInputTraits - Specifies default input image traits
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

static WORD faxDecode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PDEC_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we actually use or care about are known */
    INSURE (pTraits->iPixelsPerRow > 0);   /* we need the row-length */
    INSURE (pTraits->iBitsPerPixel == 1);  /* image must be bi-level */

    g->traits = *pTraits;   /* a structure copy */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD faxDecode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->input_format = (BYTE)aXformInfo[IP_FAX_FORMAT].dword;
    g->no_eols      = (BOOL)aXformInfo[IP_FAX_NO_EOLS].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD faxDecode_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * faxDecode_getActualTraits - Parses header, and returns input & output traits
 *
 *****************************************************************************
 *
 * There is no header, so this routine merely computes some stuff
 *
\*****************************************************************************/

static WORD faxDecode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PDEC_INST g;
    int       inBytes;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */

    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /* Since we don't change traits, just copy out the default traits */

    *pInTraits  = g->traits;
    *pOutTraits = g->traits;

    /* Compute some stuff */

    g->row_len      = g->traits.iPixelsPerRow;   /* todo: zap redundant var */
    g->bytes_in_row = inBytes = (g->row_len+7) / 8;
    g->two_dim      = (g->input_format == IP_FAX_MMR);

    /* For MH and MR:  Discard bits before the first EOL */
    g->flushing_to_eol = ! g->two_dim;

    /* allocate the prior-row buffer */

    if (g->prior_p != NULL)
        IP_MEM_FREE (g->prior_p);
    IP_MEM_ALLOC (inBytes, g->prior_p);
    memset (g->prior_p, 0, inBytes);

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * faxDecode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD faxDecode_getActualBufSizes (
    IP_XFORM_HANDLE  hXform,           /* in:  handle for xform */
    PDWORD           pdwMinInBufLen,   /* out: min input buf size */
    PDWORD           pdwMinOutBufLen)  /* out: min output buf size */
{
    PDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = 1;
    *pdwMinOutBufLen = g->bytes_in_row;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD faxDecode_convert (
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
    PDEC_INST g;
    UINT      bit;
    UINT      ret_val;   /* return-value of this function */

    HANDLE_TO_PTR (hXform, g);

    *pdwOutputUsed = 0;

    if (dwInputAvail==0 && BITS_IN_CACHE(g)<MAX_CODE_LEN) {
        /* We're being told to flush; indicate we're done. */
        /* Our only buffer is the cache */
        *pdwInputUsed     = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    if (g->toss_everything) {
        *pdwInputUsed     = dwInputAvail;
        *pdwInputNextPos  = g->dwInNextPos = g->dwInNextPos + dwInputAvail;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_READY_FOR_DATA;
    }

    bits_buf_open (g, pbInputBuf, dwInputAvail);

    if (g->flushing_to_eol) {
        ret_val = bits_flush_to_eol(g, g->got_fill,
                                    (BOOL)(g->input_format==IP_FAX_MR))
                  ? DECODE_HIT_EOL : 0;
    } else {
        if (g->two_dim) ret_val = decode_row_2d(g, g->prior_p, pbOutputBuf);
        else            ret_val = decode_row_1d(g, g->prior_p, pbOutputBuf);
        PRINT (_T("decoder returned %x\n"), ret_val, 0);
    }

    if (ret_val & IP_INPUT_ERROR) {
        if (g->input_format == IP_FAX_MMR) {
            bits_flush (g);
            g->toss_everything = TRUE;
            /* This isn't fatal any more. We merely toss all following data */
            /* ret_val |= IP_INPUT_ERROR; */
        } else {
            g->flushing_to_eol = TRUE;
            g->num_eols = 0;
        }
    }

    if (ret_val & IP_PRODUCED_ROW) {
        if (g->got_black) ret_val |= NONWHITE_ROW | IP_CONSUMED_ROW;
        else              ret_val |= WHITE_ROW    | IP_CONSUMED_ROW;
        *pdwOutputUsed = g->bytes_in_row;
        memcpy (g->prior_p, pbOutputBuf, g->bytes_in_row);
        decode_row_init (g);
        g->num_eols = 0;   /* ignore all EOLs before the row */
    }

    if (ret_val & DECODE_HIT_FILL) {
        g->flushing_to_eol = TRUE;
        g->got_fill = TRUE;
    }

    if (ret_val & DECODE_HIT_EOL) {
        if (g->input_format == IP_FAX_MR) {
            /* read the tag-bit for 1-dim/2-dim */
            BITS_LOAD (g, 1, bit)
            g->two_dim = (bit == 0);
            BITS_ADVANCE (g, 1)
        }

        decode_row_init (g);
        g->flushing_to_eol = FALSE;
        g->got_fill = FALSE;
        g->num_eols += 1;

        if (g->num_eols>=EOLS_FOR_MH_MR ||
            (g->input_format==IP_FAX_MMR && g->num_eols>=EOLS_FOR_MMR)) {
            /* WE HIT END OF PAGE */
            g->num_eols = 0;
            bits_flush (g);
            ret_val |= IP_NEW_OUTPUT_PAGE;
        }
    }

    *pdwInputUsed     = bits_buf_close (g);
    g->dwInNextPos   += *pdwInputUsed;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = (ret_val & IP_PRODUCED_ROW) ? g->bytes_in_row : 0;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += *pdwOutputUsed;

#if 0
    {
        int i;
        for (i=0; i<*pdwInputUsed; i++)
            PRINT (_T("%02x  "), pbInputBuf[i], 0);
    }
#endif

    return (ret_val | IP_READY_FOR_DATA)
            & (IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA |
               /* NONWHITE_ROW | WHITE_ROW | */
               IP_NEW_OUTPUT_PAGE |
               IP_INPUT_ERROR | IP_FATAL_ERROR);

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD faxDecode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * faxDecode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD faxDecode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: flush bits until we see a page boundary */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * faxDecode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD faxDecode_closeXform (IP_XFORM_HANDLE hXform)
{
    PDEC_INST g;

    HANDLE_TO_PTR (hXform, g);

    if (g->prior_p != NULL)
        IP_MEM_FREE (g->prior_p);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * faxDecodeTbl - Jump-table for Decoder
 *
\*****************************************************************************/

IP_XFORM_TBL faxDecodeTbl = {
    faxDecode_openXform,
    faxDecode_setDefaultInputTraits,
    faxDecode_setXformSpec,
    faxDecode_getHeaderBufSize,
    faxDecode_getActualTraits,
    faxDecode_getActualBufSizes,
    faxDecode_convert,
    faxDecode_newPage,
    faxDecode_insertedData,
    faxDecode_closeXform
};




/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@                                                                            @
@                  T A B L E S   F O R   D E C O D E R                       @
@                                                                            @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/



const USHORT fax_white_huff[105] = {  0,
    0x4002, 0x4003, 0x4004, 0x4005, 0x4006, 0x4007, 0x500a, 0x500b,
    0x5080, 0x5008, 0x5009, 0x5040, 0x600d, 0x6001, 0x600c, 0x60c0,
    0x6680, 0x6010, 0x6011, 0x600e, 0x600f, 0x7016, 0x7017, 0x7014,
    0x7013, 0x701a, 0x7015, 0x701c, 0x701b, 0x7012, 0x7018, 0x7019,
    0x7100, 0x801d, 0x801e, 0x802d, 0x802e, 0x802f, 0x8030, 0x8021,
    0x8022, 0x8023, 0x8024, 0x8025, 0x8026, 0x801f, 0x8020, 0x8035,
    0x8036, 0x8027, 0x8028, 0x8029, 0x802a, 0x802b, 0x802c, 0x803d,
    0x803e, 0x803f, 0x8000, 0x8140, 0x8180, 0x803b, 0x803c, 0x8031,
    0x8032, 0x8033, 0x8034, 0x8037, 0x8038, 0x8039, 0x803a, 0x81c0,
    0x8200, 0x8280, 0x8240, 0x95c0, 0x9600, 0x9640, 0x96c0, 0x92c0,
    0x9300, 0x9340, 0x9380, 0x93c0, 0x9400, 0x9440, 0x9480, 0x94c0,
    0x9500, 0x9540, 0x9580, 0xb700, 0xb740, 0xb780, 0xc7c0, 0xc800,
    0xc840, 0xc880, 0xc8c0, 0xc900, 0xc940, 0xc980, 0xc9c0, 0xca00,
};


const BYTE fax_white_huff_index[4096] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    92, 92, 95, 96, 97, 98, 99, 100, 93, 93, 94, 94, 101, 102, 103, 104,
    34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
    41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
    42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
    43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
    44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
    46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
    47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
    26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
    52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
    59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
    76, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77, 77, 77, 77, 77,
    78, 78, 78, 78, 78, 78, 78, 78, 79, 79, 79, 79, 79, 79, 79, 79,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
    67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
    69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69,
    70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70,
    71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
    73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
    80, 80, 80, 80, 80, 80, 80, 80, 81, 81, 81, 81, 81, 81, 81, 81,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
    82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 83, 83, 83, 83, 83, 83,
    84, 84, 84, 84, 84, 84, 84, 84, 85, 85, 85, 85, 85, 85, 85, 85,
    86, 86, 86, 86, 86, 86, 86, 86, 87, 87, 87, 87, 87, 87, 87, 87,
    88, 88, 88, 88, 88, 88, 88, 88, 89, 89, 89, 89, 89, 89, 89, 89,
    90, 90, 90, 90, 90, 90, 90, 90, 91, 91, 91, 91, 91, 91, 91, 91,
    33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
    33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
};


const USHORT fax_black_huff[105] = {  0,
    0x2003, 0x2002, 0x3001, 0x3004, 0x4006, 0x4005, 0x5007, 0x6009,
    0x6008, 0x700a, 0x700b, 0x700c, 0x800d, 0x800e, 0x900f, 0xa012,
    0xa040, 0xa010, 0xa011, 0xa000, 0xb700, 0xb740, 0xb780, 0xb018,
    0xb019, 0xb017, 0xb016, 0xb013, 0xb014, 0xb015, 0xc7c0, 0xc800,
    0xc840, 0xc880, 0xc8c0, 0xc900, 0xc940, 0xc980, 0xc9c0, 0xca00,
    0xc034, 0xc037, 0xc038, 0xc03b, 0xc03c, 0xc140, 0xc180, 0xc1c0,
    0xc035, 0xc036, 0xc032, 0xc033, 0xc02c, 0xc02d, 0xc02e, 0xc02f,
    0xc039, 0xc03a, 0xc03d, 0xc100, 0xc030, 0xc031, 0xc03e, 0xc03f,
    0xc01e, 0xc01f, 0xc020, 0xc021, 0xc028, 0xc029, 0xc080, 0xc0c0,
    0xc01a, 0xc01b, 0xc01c, 0xc01d, 0xc022, 0xc023, 0xc024, 0xc025,
    0xc026, 0xc027, 0xc02a, 0xc02b, 0xd280, 0xd2c0, 0xd300, 0xd340,
    0xd500, 0xd540, 0xd580, 0xd5c0, 0xd600, 0xd640, 0xd680, 0xd6c0,
    0xd200, 0xd240, 0xd380, 0xd3c0, 0xd400, 0xd440, 0xd480, 0xd4c0,
};


const BYTE fax_black_huff_index[8192] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    21, 21, 21, 21, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36,
    22, 22, 22, 22, 23, 23, 23, 23, 37, 37, 38, 38, 39, 39, 40, 40,
    16, 16, 16, 16, 16, 16, 16, 16, 41, 41, 85, 86, 87, 88, 42, 42,
    43, 43, 89, 90, 91, 92, 44, 44, 45, 45, 93, 94, 24, 24, 24, 24,
    25, 25, 25, 25, 95, 96, 46, 46, 47, 47, 48, 48, 97, 98, 49, 49,
    50, 50, 99, 100, 101, 102, 103, 104, 17, 17, 17, 17, 17, 17, 17, 17,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    26, 26, 26, 26, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56, 56,
    57, 57, 58, 58, 59, 59, 60, 60, 18, 18, 18, 18, 18, 18, 18, 18,
    19, 19, 19, 19, 19, 19, 19, 19, 61, 61, 62, 62, 63, 63, 64, 64,
    65, 65, 66, 66, 67, 67, 68, 68, 69, 69, 70, 70, 27, 27, 27, 27,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 76, 76, 28, 28, 28, 28,
    29, 29, 29, 29, 77, 77, 78, 78, 79, 79, 80, 80, 81, 81, 82, 82,
    30, 30, 30, 30, 83, 83, 84, 84, 20, 20, 20, 20, 20, 20, 20, 20,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
};


const USHORT fax_vert_huff[10] = {  0,
    0x1003, 0x3008, 0x3002, 0x3004, 0x4007, 0x6001, 0x6005, 0x7000,
    0x7006, 
};


const BYTE fax_vert_huff_index[128] = {
     0,  0,  8,  9,  6,  6,  7,  7,  5,  5,  5,  5,  5,  5,  5,  5,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
};

/* End of File */

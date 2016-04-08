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
 * xjpg_enc.c - Converts raw gray image into a valid JPEG file
 *
 *****************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    jpgEncodeTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_JPG_ENCODE_QUALITY_FACTORS]:
 *            Quality factors. Each 1..255 (best..worst), normal=20 or 0.
 *            If 2nd least significant byte is non-zero, then it is the
 *            DC factor, and lsb is the AC factor.  If 2nd lsb is zero,
 *            then the lsb is both AC and DC factor.  Q factors match PML.
 *    aXformInfo[IP_JPG_ENCODE_SAMPLE_FACTORS]:
 *            Sample factors in nibbles: HHHHVVVV, 0 means use defaults.
 *    aXformInfo[IP_JPG_ENCODE_ALREADY_SUBSAMPLED]:
 *            Is raw data already subsampled? 0=no, 1=yes.
 *    aXformInfo[IP_JPG_ENCODE_FOR_DENALI]:
 *            Output is for Denali? 0=no, 1=yes
 *    aXformInfo[IP_JPG_ENCODE_OUTPUT_DNL]:
 *            Output a DNL (Define Number of Lines) marker? 0=no, 1=yes.
 *    aXformInfo[IP_JPG_ENCODE_FOR_COLOR_FAX]:
 *            Output an APP1 per the G3 color fax standard? 0=no, 1=yes.
 *    aXformInfo[IP_JPG_ENCODE_DUMMY_HEADER_LEN]:
 *            # bytes in dummy header.  0 means output normal header.
 *            The firmware discards the header in the first raster data
 *            record. This value is the # data bytes in that record.
 *            This MUST be zero for JPEG files not being sent to firmware.
 *
 *    The aXformInfo items above may all be set to 0 for typical JPEG files.
 *
 *    For Denali, the JPEG that's output will be changed thusly:
 *        - An EOB will always follow every 8x8 block
 *        - A small change in the Huffman tables (no 15-bit codes)
 *
 * Capabilities and Limitations:
 *
 *    Encodes a standard JPEG file with a JFIF 1.0 marker.
 *    Will *not* output a non-interleaved file; it's interleaved only.
 *    If the number of rows was unknown in the input traits, this encoder
 *    seeks back to the beginning of the file and fills in the row-count.
 *    A DNL is always output if the always-output-DNL item is set in
 *    the aXformInfo array above.
 *    Handles 8-bit gray, or 3-component 24-bit color.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel   * must be 1 or 3       same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Jan 1998:  Ported to Image Processor module of Windows software.
 * Feb 1996:  Written for firmware, Mark Overton;
 *            sections of this code were ported from HP-Labs (Hugh P. Nguyen).
 *
\*****************************************************************************/

#include <string.h>
#include "hpip.h"
#include "ipdefs.h"
#include "xjpg_dct.h"
#include "xjpg_mrk.h"


#if 0
    #include "stdio.h"
    #include <tchar.h>
    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stdout, _T("(jpeg) ") msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif


#define RUN_OF_16         0xf0  /* RLE code for run-of-16-zeroes */
#define MAX_HEADER_SIZE   2000
#define MAX_BLOCKS_IN_MCU 6
#define MAX_MCU_SIZE      (MAX_BLOCKS_IN_MCU*304)
                                /* max encoded MCU size, plus stuff-bytes */
#define OUTBUF_NUM_MCUS   2     /* workbuf will be this multiple of max MCU */

#define Q_DEFAULT         20    /* default quality-factor */
#define MONO_FACTORS      0x10001000u  /* default mono  sample factors */
#define COLOR_FACTORS     0x21102110u  /* default color sample factors */

#define CHECK_VALUE 0xAceC0de4U


/*____________________________________________________________________________
 |                                                                            |
 | Configuration Variables                                                    |
 |____________________________________________________________________________|
*/

/* Encoding is centered around in_rows_ap.  It is indexed by
 * [color_component_number][row_number].
 * color_component_number 0 is Y (intensity); mono only has this component.
 *
 * Each component in in_rows_ap has a height (# rows) equal to the number of
 * samples in the MCU, and a width equal to the number of samples in all the
 * MCUs in a row.  That is, pixels are stored in in_rows_ap with NO REPLICATION.
 * So if a component has sample factors of H and V, it will have 8*V rows and
 * pixels_in_row*H/max_horiz_sam_fac columns in in_rows_ap.
 */

typedef struct {

    /**** Configuration ****/

    BYTE  lum_quant[64];
    BYTE  chrom_quant[64];

    int   wino_lum_quant[64];
    int   wino_lum_quant_thres[64];
    int   wino_chrom_quant[64];
    int   wino_chrom_quant_thres[64];

    BOOL  input_subsampled;
    BOOL  fDenali;
    BOOL  fOutputDNL;
    BOOL  fOutputG3APP1;
    DWORD dwDummyHeaderBytes;
    UINT  rows_in_mcu;         /* # rows & cols in each MCU */
    UINT  cols_in_mcu;
    UINT  mcus_in_row;         /* # of MCUs in each row */
    DWORD sample_factors;      /* H and V sample factors, one/nibble */
    BYTE  horiz_sam_facs[4];   /* horizontal sampling factors */
    BYTE  vert_sam_facs [4];   /* vertical sampling factors */
    BYTE  max_horiz_sam_fac;   /* max sample factors */
    BYTE  max_vert_sam_fac;
    BYTE  dc_q_factor;
    BYTE  ac_q_factor;
    BYTE  comps_in_pixel;      /* # components per pixel (1 or 3) */
    BYTE  whitePixel[4];       /* the value of a white pixel */
    UINT  pixels_in_row;
    int   rows_in_page;        /* negative means "unknown" */
    int   xRes;                /* dots per inch */
    int   yRes;

    /**** Writing Bits ****/

    DWORD wr_bit_buf;
    /* Bits to be written to outbuf (accumulated left-to-right). */

    int wr_bits_avail;
    /* Number of bits not yet written in wr_bit_buf (= 32 - number written). */

    BYTE *wr_outbuf_beg;
    /* The beginning of the output buffer. */

    BYTE *write_buf_next;
    /* Next byte in outbuf to be written. */

    /**** Encoding Blocks ****/

    int  enc_block[64];           /* scratch-pad 8x8 block */
    int *enc_block_zz[64+16];     /* zig-zag ptrs into above block */
    int  prior_DC[4];

    /**** Top Level Control ****/

    UINT rows_received;
    UINT rows_loaded;
    UINT mcus_sent;
    BOOL loading_rows;

    /**** Miscellaneous ****/

    IP_IMAGE_TRAITS traits;
    BYTE *in_rows_ap[4][32];   /* row-buffers [component][row] */
    BOOL  fDidHeader;          /* output the header yet? */
    DWORD dwInNextPos;         /* next read pos in input file */
    DWORD dwOutNextPos;        /* next write pos in output file */
    DWORD dwValidChk;          /* struct validity check value */

} JENC_INST, *PJENC_INST;


/*____________________________________________________________________________
 |                                                                            |
 | Normal Quantization Tables                                                 |
 |____________________________________________________________________________|
*/

#if 0

static const BYTE orig_lum_quant[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99
};


static const BYTE orig_chrom_quant[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

#endif


/*____________________________________________________________________________
 |                                                                            |
 | Zigzag of Normal Quantization Tables                                       |
 |____________________________________________________________________________|
*/

static const BYTE orig_lum_quant[64] = {
  #if 0
     /* these make color fax look better, but break gray copy
      * because JPEG is sent to the device, so our tables must
      * match those in the firmware
      */
     10,  10,  10,  14,  12,  10,  16,  14,
  #else
     16,  11,  12,  14,  12,  10,  16,  14,
  #endif
     13,  14,  18,  17,  16,  19,  24,  40,
     26,  24,  22,  22,  24,  49,  35,  37,
     29,  40,  58,  51,  61,  60,  57,  51,
     56,  55,  64,  72,  92,  78,  64,  68,
     87,  69,  55,  56,  80, 109,  81,  87,
     95,  98, 103, 104, 103,  62,  77, 113,
    121, 112, 100, 120,  92, 101, 103,  99
};


static const BYTE orig_chrom_quant[64] = {
  #if 0
    10,  14,  14,  24,  21,  24,  47,  26,
  #else
    17,  18,  18,  24,  21,  24,  47,  26,
  #endif
    26,  47,  99,  66,  56,  66,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
};


/*____________________________________________________________________________
 |                |                                                           |
 | codesize_array | Array giving # of bits required to represent the index    |
 |________________|___________________________________________________________|
*/

static const BYTE codesize_array[256] = {
   0,
   1,
   2, 2,
   3, 3, 3, 3,
   4, 4, 4, 4, 4, 4, 4, 4,
   5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};



/*____________________________________________________________________________
 |                                                                            |
 | Huffman Tables                                                             |
 |____________________________________________________________________________|
*/

static const BYTE lum_DC_counts[16] = {
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const BYTE lum_DC_values[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
};

static const BYTE chrom_DC_counts[16] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const BYTE chrom_DC_values[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
};

static const BYTE lum_AC_counts[16] = {
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
    0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d
};

static const BYTE lum_AC_counts_Denali[16] = {
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
    0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x00, 0x7e
    /* Above, the [01,7d] in the normal table was changed to [00,7e].
     * This alteration eliminates the sole 15-bit code, and yields
     * Huffman codes as follows:
     *    - common codes are 12 bits wide or less,
     *    - uncommon codes are exactly 16 bits wide, and all those codes
     *      start with nine '1' bits, leaving seven bits of useful info.
     * Denali uses a 4K-entry table for the common codes, and a
     * quick lookup for the 7-bit leftover codes.  So parsing of all
     * codes is simple and fast.
     */
};

static const BYTE lum_AC_values[162] = {
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

static const BYTE chrom_AC_counts[16] = {
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
    0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
};

static const BYTE chrom_AC_values[162] = {
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
 |                                                                            |
 | Huffman tables output by mk_jpg_huff program                               |
 |____________________________________________________________________________|
*/

typedef struct {
    WORD code;    /* the Huff code to use */
    BYTE size;    /* number of bits in above Huff code */
} huff_elem_t;


static const huff_elem_t lum_DC_table[] = {
    { 0x0000,  2 }, { 0x0002,  3 }, { 0x0003,  3 }, { 0x0004,  3 },
    { 0x0005,  3 }, { 0x0006,  3 }, { 0x000e,  4 }, { 0x001e,  5 },
    { 0x003e,  6 }, { 0x007e,  7 }, { 0x00fe,  8 }, { 0x01fe,  9 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, };


static const huff_elem_t lum_AC_table[] = {
    { 0x000a,  4 }, { 0x0000,  2 }, { 0x0001,  2 }, { 0x0004,  3 },
    { 0x000b,  4 }, { 0x001a,  5 }, { 0x0078,  7 }, { 0x00f8,  8 },
    { 0x03f6, 10 }, { 0xff82, 16 }, { 0xff83, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x000c,  4 }, { 0x001b,  5 }, { 0x0079,  7 },
    { 0x01f6,  9 }, { 0x07f6, 11 }, { 0xff84, 16 }, { 0xff85, 16 },
    { 0xff86, 16 }, { 0xff87, 16 }, { 0xff88, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x001c,  5 }, { 0x00f9,  8 }, { 0x03f7, 10 },
    { 0x0ff4, 12 }, { 0xff89, 16 }, { 0xff8a, 16 }, { 0xff8b, 16 },
    { 0xff8c, 16 }, { 0xff8d, 16 }, { 0xff8e, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003a,  6 }, { 0x01f7,  9 }, { 0x0ff5, 12 },
    { 0xff8f, 16 }, { 0xff90, 16 }, { 0xff91, 16 }, { 0xff92, 16 },
    { 0xff93, 16 }, { 0xff94, 16 }, { 0xff95, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003b,  6 }, { 0x03f8, 10 }, { 0xff96, 16 },
    { 0xff97, 16 }, { 0xff98, 16 }, { 0xff99, 16 }, { 0xff9a, 16 },
    { 0xff9b, 16 }, { 0xff9c, 16 }, { 0xff9d, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x007a,  7 }, { 0x07f7, 11 }, { 0xff9e, 16 },
    { 0xff9f, 16 }, { 0xffa0, 16 }, { 0xffa1, 16 }, { 0xffa2, 16 },
    { 0xffa3, 16 }, { 0xffa4, 16 }, { 0xffa5, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x007b,  7 }, { 0x0ff6, 12 }, { 0xffa6, 16 },
    { 0xffa7, 16 }, { 0xffa8, 16 }, { 0xffa9, 16 }, { 0xffaa, 16 },
    { 0xffab, 16 }, { 0xffac, 16 }, { 0xffad, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x00fa,  8 }, { 0x0ff7, 12 }, { 0xffae, 16 },
    { 0xffaf, 16 }, { 0xffb0, 16 }, { 0xffb1, 16 }, { 0xffb2, 16 },
    { 0xffb3, 16 }, { 0xffb4, 16 }, { 0xffb5, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f8,  9 }, { 0x7fc0, 15 }, { 0xffb6, 16 },
    { 0xffb7, 16 }, { 0xffb8, 16 }, { 0xffb9, 16 }, { 0xffba, 16 },
    { 0xffbb, 16 }, { 0xffbc, 16 }, { 0xffbd, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f9,  9 }, { 0xffbe, 16 }, { 0xffbf, 16 },
    { 0xffc0, 16 }, { 0xffc1, 16 }, { 0xffc2, 16 }, { 0xffc3, 16 },
    { 0xffc4, 16 }, { 0xffc5, 16 }, { 0xffc6, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01fa,  9 }, { 0xffc7, 16 }, { 0xffc8, 16 },
    { 0xffc9, 16 }, { 0xffca, 16 }, { 0xffcb, 16 }, { 0xffcc, 16 },
    { 0xffcd, 16 }, { 0xffce, 16 }, { 0xffcf, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x03f9, 10 }, { 0xffd0, 16 }, { 0xffd1, 16 },
    { 0xffd2, 16 }, { 0xffd3, 16 }, { 0xffd4, 16 }, { 0xffd5, 16 },
    { 0xffd6, 16 }, { 0xffd7, 16 }, { 0xffd8, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x03fa, 10 }, { 0xffd9, 16 }, { 0xffda, 16 },
    { 0xffdb, 16 }, { 0xffdc, 16 }, { 0xffdd, 16 }, { 0xffde, 16 },
    { 0xffdf, 16 }, { 0xffe0, 16 }, { 0xffe1, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x07f8, 11 }, { 0xffe2, 16 }, { 0xffe3, 16 },
    { 0xffe4, 16 }, { 0xffe5, 16 }, { 0xffe6, 16 }, { 0xffe7, 16 },
    { 0xffe8, 16 }, { 0xffe9, 16 }, { 0xffea, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0xffeb, 16 }, { 0xffec, 16 }, { 0xffed, 16 },
    { 0xffee, 16 }, { 0xffef, 16 }, { 0xfff0, 16 }, { 0xfff1, 16 },
    { 0xfff2, 16 }, { 0xfff3, 16 }, { 0xfff4, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x07f9, 11 }, { 0xfff5, 16 }, { 0xfff6, 16 }, { 0xfff7, 16 },
    { 0xfff8, 16 }, { 0xfff9, 16 }, { 0xfffa, 16 }, { 0xfffb, 16 },
    { 0xfffc, 16 }, { 0xfffd, 16 }, { 0xfffe, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
};


static const huff_elem_t lum_AC_table_Denali[] = {
    { 0x000a,  4 }, { 0x0000,  2 }, { 0x0001,  2 }, { 0x0004,  3 },
    { 0x000b,  4 }, { 0x001a,  5 }, { 0x0078,  7 }, { 0x00f8,  8 },
    { 0x03f6, 10 }, { 0xff81, 16 }, { 0xff82, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x000c,  4 }, { 0x001b,  5 }, { 0x0079,  7 },
    { 0x01f6,  9 }, { 0x07f6, 11 }, { 0xff83, 16 }, { 0xff84, 16 },
    { 0xff85, 16 }, { 0xff86, 16 }, { 0xff87, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x001c,  5 }, { 0x00f9,  8 }, { 0x03f7, 10 },
    { 0x0ff4, 12 }, { 0xff88, 16 }, { 0xff89, 16 }, { 0xff8a, 16 },
    { 0xff8b, 16 }, { 0xff8c, 16 }, { 0xff8d, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003a,  6 }, { 0x01f7,  9 }, { 0x0ff5, 12 },
    { 0xff8e, 16 }, { 0xff8f, 16 }, { 0xff90, 16 }, { 0xff91, 16 },
    { 0xff92, 16 }, { 0xff93, 16 }, { 0xff94, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003b,  6 }, { 0x03f8, 10 }, { 0xff95, 16 },
    { 0xff96, 16 }, { 0xff97, 16 }, { 0xff98, 16 }, { 0xff99, 16 },
    { 0xff9a, 16 }, { 0xff9b, 16 }, { 0xff9c, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x007a,  7 }, { 0x07f7, 11 }, { 0xff9d, 16 },
    { 0xff9e, 16 }, { 0xff9f, 16 }, { 0xffa0, 16 }, { 0xffa1, 16 },
    { 0xffa2, 16 }, { 0xffa3, 16 }, { 0xffa4, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x007b,  7 }, { 0x0ff6, 12 }, { 0xffa5, 16 },
    { 0xffa6, 16 }, { 0xffa7, 16 }, { 0xffa8, 16 }, { 0xffa9, 16 },
    { 0xffaa, 16 }, { 0xffab, 16 }, { 0xffac, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x00fa,  8 }, { 0x0ff7, 12 }, { 0xffad, 16 },
    { 0xffae, 16 }, { 0xffaf, 16 }, { 0xffb0, 16 }, { 0xffb1, 16 },
    { 0xffb2, 16 }, { 0xffb3, 16 }, { 0xffb4, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f8,  9 }, { 0xff80, 16 }, { 0xffb5, 16 },
    { 0xffb6, 16 }, { 0xffb7, 16 }, { 0xffb8, 16 }, { 0xffb9, 16 },
    { 0xffba, 16 }, { 0xffbb, 16 }, { 0xffbc, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f9,  9 }, { 0xffbd, 16 }, { 0xffbe, 16 },
    { 0xffbf, 16 }, { 0xffc0, 16 }, { 0xffc1, 16 }, { 0xffc2, 16 },
    { 0xffc3, 16 }, { 0xffc4, 16 }, { 0xffc5, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01fa,  9 }, { 0xffc6, 16 }, { 0xffc7, 16 },
    { 0xffc8, 16 }, { 0xffc9, 16 }, { 0xffca, 16 }, { 0xffcb, 16 },
    { 0xffcc, 16 }, { 0xffcd, 16 }, { 0xffce, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x03f9, 10 }, { 0xffcf, 16 }, { 0xffd0, 16 },
    { 0xffd1, 16 }, { 0xffd2, 16 }, { 0xffd3, 16 }, { 0xffd4, 16 },
    { 0xffd5, 16 }, { 0xffd6, 16 }, { 0xffd7, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x03fa, 10 }, { 0xffd8, 16 }, { 0xffd9, 16 },
    { 0xffda, 16 }, { 0xffdb, 16 }, { 0xffdc, 16 }, { 0xffdd, 16 },
    { 0xffde, 16 }, { 0xffdf, 16 }, { 0xffe0, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x07f8, 11 }, { 0xffe1, 16 }, { 0xffe2, 16 },
    { 0xffe3, 16 }, { 0xffe4, 16 }, { 0xffe5, 16 }, { 0xffe6, 16 },
    { 0xffe7, 16 }, { 0xffe8, 16 }, { 0xffe9, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0xffea, 16 }, { 0xffeb, 16 }, { 0xffec, 16 },
    { 0xffed, 16 }, { 0xffee, 16 }, { 0xffef, 16 }, { 0xfff0, 16 },
    { 0xfff1, 16 }, { 0xfff2, 16 }, { 0xfff3, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x07f9, 11 }, { 0xfff4, 16 }, { 0xfff5, 16 }, { 0xfff6, 16 },
    { 0xfff7, 16 }, { 0xfff8, 16 }, { 0xfff9, 16 }, { 0xfffa, 16 },
    { 0xfffb, 16 }, { 0xfffc, 16 }, { 0xfffd, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
};


static const huff_elem_t chrom_DC_table[] = {
    { 0x0000,  2 }, { 0x0001,  2 }, { 0x0002,  2 }, { 0x0006,  3 },
    { 0x000e,  4 }, { 0x001e,  5 }, { 0x003e,  6 }, { 0x007e,  7 },
    { 0x00fe,  8 }, { 0x01fe,  9 }, { 0x03fe, 10 }, { 0x07fe, 11 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, };


static const huff_elem_t chrom_AC_table[] = {
    { 0x0000,  2 }, { 0x0001,  2 }, { 0x0004,  3 }, { 0x000a,  4 },
    { 0x0018,  5 }, { 0x0019,  5 }, { 0x0038,  6 }, { 0x0078,  7 },
    { 0x01f4,  9 }, { 0x03f6, 10 }, { 0x0ff4, 12 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x000b,  4 }, { 0x0039,  6 }, { 0x00f6,  8 },
    { 0x01f5,  9 }, { 0x07f6, 11 }, { 0x0ff5, 12 }, { 0xff88, 16 },
    { 0xff89, 16 }, { 0xff8a, 16 }, { 0xff8b, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x001a,  5 }, { 0x00f7,  8 }, { 0x03f7, 10 },
    { 0x0ff6, 12 }, { 0x7fc2, 15 }, { 0xff8c, 16 }, { 0xff8d, 16 },
    { 0xff8e, 16 }, { 0xff8f, 16 }, { 0xff90, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x001b,  5 }, { 0x00f8,  8 }, { 0x03f8, 10 },
    { 0x0ff7, 12 }, { 0xff91, 16 }, { 0xff92, 16 }, { 0xff93, 16 },
    { 0xff94, 16 }, { 0xff95, 16 }, { 0xff96, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003a,  6 }, { 0x01f6,  9 }, { 0xff97, 16 },
    { 0xff98, 16 }, { 0xff99, 16 }, { 0xff9a, 16 }, { 0xff9b, 16 },
    { 0xff9c, 16 }, { 0xff9d, 16 }, { 0xff9e, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x003b,  6 }, { 0x03f9, 10 }, { 0xff9f, 16 },
    { 0xffa0, 16 }, { 0xffa1, 16 }, { 0xffa2, 16 }, { 0xffa3, 16 },
    { 0xffa4, 16 }, { 0xffa5, 16 }, { 0xffa6, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0079,  7 }, { 0x07f7, 11 }, { 0xffa7, 16 },
    { 0xffa8, 16 }, { 0xffa9, 16 }, { 0xffaa, 16 }, { 0xffab, 16 },
    { 0xffac, 16 }, { 0xffad, 16 }, { 0xffae, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x007a,  7 }, { 0x07f8, 11 }, { 0xffaf, 16 },
    { 0xffb0, 16 }, { 0xffb1, 16 }, { 0xffb2, 16 }, { 0xffb3, 16 },
    { 0xffb4, 16 }, { 0xffb5, 16 }, { 0xffb6, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x00f9,  8 }, { 0xffb7, 16 }, { 0xffb8, 16 },
    { 0xffb9, 16 }, { 0xffba, 16 }, { 0xffbb, 16 }, { 0xffbc, 16 },
    { 0xffbd, 16 }, { 0xffbe, 16 }, { 0xffbf, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f7,  9 }, { 0xffc0, 16 }, { 0xffc1, 16 },
    { 0xffc2, 16 }, { 0xffc3, 16 }, { 0xffc4, 16 }, { 0xffc5, 16 },
    { 0xffc6, 16 }, { 0xffc7, 16 }, { 0xffc8, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f8,  9 }, { 0xffc9, 16 }, { 0xffca, 16 },
    { 0xffcb, 16 }, { 0xffcc, 16 }, { 0xffcd, 16 }, { 0xffce, 16 },
    { 0xffcf, 16 }, { 0xffd0, 16 }, { 0xffd1, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01f9,  9 }, { 0xffd2, 16 }, { 0xffd3, 16 },
    { 0xffd4, 16 }, { 0xffd5, 16 }, { 0xffd6, 16 }, { 0xffd7, 16 },
    { 0xffd8, 16 }, { 0xffd9, 16 }, { 0xffda, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x01fa,  9 }, { 0xffdb, 16 }, { 0xffdc, 16 },
    { 0xffdd, 16 }, { 0xffde, 16 }, { 0xffdf, 16 }, { 0xffe0, 16 },
    { 0xffe1, 16 }, { 0xffe2, 16 }, { 0xffe3, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x07f9, 11 }, { 0xffe4, 16 }, { 0xffe5, 16 },
    { 0xffe6, 16 }, { 0xffe7, 16 }, { 0xffe8, 16 }, { 0xffe9, 16 },
    { 0xffea, 16 }, { 0xffeb, 16 }, { 0xffec, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x3fe0, 14 }, { 0xffed, 16 }, { 0xffee, 16 },
    { 0xffef, 16 }, { 0xfff0, 16 }, { 0xfff1, 16 }, { 0xfff2, 16 },
    { 0xfff3, 16 }, { 0xfff4, 16 }, { 0xfff5, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
    { 0x03fa, 10 }, { 0x7fc3, 15 }, { 0xfff6, 16 }, { 0xfff7, 16 },
    { 0xfff8, 16 }, { 0xfff9, 16 }, { 0xfffa, 16 }, { 0xfffb, 16 },
    { 0xfffc, 16 }, { 0xfffd, 16 }, { 0xfffe, 16 }, { 0x0000,  0 },
    { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 }, { 0x0000,  0 },
};



/******************************************************************************
 ******************************************************************************

                            WRITING-BITS SECTION

 ******************************************************************************
 ******************************************************************************


 Interface into this section:

    write_init          - inits this section
    write_buf_open      - we are being given a (new) output buffer
    write_buf_close     - done with current output buffer; return # bytes sent
    write_buf_next      - (a var) the next byte to write in the output buffer
    WRITE_BITS_OPEN     - setup code for WRITE_BITS
    WRITE_BITS          - outputs bits (fast)
    WRITE_BITS_HIZERO   - outputs bits (faster) assuming higher-order bits are 0
    WRITE_BITS_CLOSE    - teardown code for WRITE_BITS
    write_bits_flush    - flushes the bit-cache

 ******************************************************************************/



#define INITIAL_BITS_AVAIL (8*sizeof(DWORD))
    /* Number of bits in wr_bit_buf, which is the # of bits in a DWORD . */


/*____________________________________________________________________________
 |                |                                                           |
 | PUT_BYTE_STUFF | Puts byte into outbuf, with byte-stuffing as needed       |
 |________________|___________________________________________________________|
 |                                                                            |
 | A 0xff byte within bit-data is always followed by a 0x00 byte to           |
 | distinguish it from markers, which are 0xff followed by a non-zero byte.   |
 | Therefore, PUT_BYTE_STUFF should never be used to write markers.           |
 |____________________________________________________________________________|
*/
#define PUT_BYTE_STUFF(g, pbs_byte_expression) {               \
    BYTE pbs_byte;                                             \
                                                               \
    pbs_byte = (BYTE)(pbs_byte_expression);                    \
    *(g->write_buf_next)++ = pbs_byte;                         \
    if (pbs_byte == (BYTE)0xff)                                \
        *(g->write_buf_next)++ = 0;                            \
}



/*____________________________________________________________________________
 |            |                                                               |
 | write_init | Inits this package, and registers the "write bytes" callback  |
 |____________|_______________________________________________________________|
*/
static void write_init (PJENC_INST g)
{
    g->wr_bits_avail = INITIAL_BITS_AVAIL;
    g->wr_bit_buf = 0;
}



/*____________________________________________________________________________
 |                |                                                           |
 | write_buf_open | We are being given a (new) buffer to receive output       |
 |________________|___________________________________________________________|
 |                                                                            |
 | This routine records the location of the new output buffer.                |
 |____________________________________________________________________________|
*/
static void write_buf_open (PJENC_INST g, BYTE *buf_p)
{
    g->wr_outbuf_beg = g->write_buf_next = buf_p;
}



/*____________________________________________________________________________
 |                 |                                                          |
 | write_buf_close | We are done with the current output buffer               |
 |_________________|__________________________________________________________|
 |                                                                            |
 | This function returns # bytes written to the output buffer                 |
 |____________________________________________________________________________|
*/
static int write_buf_close (PJENC_INST g)
{
    return g->write_buf_next - g->wr_outbuf_beg;
}



/*____________________________________________________________________________
 |            |                                                               |
 | WRITE_BITS | Writes the given number of bits                               |
 |____________|_______________________________________________________________|
 |                                                                            |
 | WRITE_BITS_HIZERO assumes that the higher-order bits are all zero.         |
 | WRITE_BITS masks them out for you.                                         |
 |____________________________________________________________________________|
*/

#define WRITE_BITS_OPEN(g)                         \
    int    tmp_bits_avail = g->wr_bits_avail;      \
    DWORD  tmp_bit_buf    = g->wr_bit_buf;


#define WRITE_BITS_HIZERO(g,wb_value,wb_nbits) {                        \
    DWORD  bits_loc;                                                    \
    int    length_loc;                                                  \
                                                                        \
    length_loc = (int)(wb_nbits);                                       \
    bits_loc = (DWORD)(wb_value);                                       \
                                                                        \
    if (length_loc > tmp_bits_avail) {                                  \
        do {                                                            \
            PUT_BYTE_STUFF (g, tmp_bit_buf >> 24)                       \
            tmp_bit_buf <<= 8;                                          \
            tmp_bits_avail += 8;                                        \
        } while (tmp_bits_avail <= 24);                                 \
    }                                                                   \
                                                                        \
    tmp_bits_avail -= length_loc;                                       \
    tmp_bit_buf |= bits_loc << tmp_bits_avail;                          \
}


#define WRITE_BITS(g,wb_value,wb_nbits) {                                   \
    int len_loc;                                                            \
                                                                            \
    len_loc = (int)(wb_nbits);                                              \
    WRITE_BITS_HIZERO (g, (DWORD )(wb_value) & ((1ul<<len_loc)-1), len_loc) \
}


#define WRITE_BITS_CLOSE(g) {                      \
    g->wr_bits_avail = tmp_bits_avail;             \
    g->wr_bit_buf    = tmp_bit_buf;                \
}



/*____________________________________________________________________________
 |                  |                                                         |
 | write_bits_flush | Writes any buffered bits, with buffering via outbuf     |
 |__________________|_________________________________________________________|
 |                                                                            |
 | Bits are right-padded with ones if necessary to a byte-boundary.           |
 |____________________________________________________________________________|
*/
static void write_bits_flush (PJENC_INST g)
{
    int bits_used;

    if (g->wr_bits_avail != INITIAL_BITS_AVAIL) {
        /* JPEG wants pad-bits to be 1's */
        g->wr_bit_buf |= ((DWORD )1 << g->wr_bits_avail) - 1;

        for (bits_used = INITIAL_BITS_AVAIL - g->wr_bits_avail;
             bits_used > 0;
             bits_used -= 8) {
            PUT_BYTE_STUFF (g, g->wr_bit_buf >> 24)
            g->wr_bit_buf <<= 8;
        }

        g->wr_bits_avail = INITIAL_BITS_AVAIL;
        g->wr_bit_buf = 0;
    }
}



/******************************************************************************
 ******************************************************************************

                              MARKERS SECTION

 ******************************************************************************
 ******************************************************************************/



/*____________________________________________________________________________
 |                                                                            |
 | Macros                                                                     |
 |____________________________________________________________________________|
*/


#define PUT_MARKER(g,marker)                    \
do {                                            \
    *(g->write_buf_next)++ = 0xff;              \
    *(g->write_buf_next)++ = (BYTE)marker;      \
} while (0)


#define PUT_INT(g,value)                        \
do {                                            \
    unsigned loc_val = (unsigned)(value);       \
    *(g->write_buf_next)++ = loc_val >> 8;      \
    *(g->write_buf_next)++ = loc_val & 0xff;    \
} while (0)



/*____________________________________________________________________________
 |              |                                                             |
 | em_write_SOI | Writes Start-Of-Image marker                                |
 |______________|_____________________________________________________________|
*/
static void em_write_SOI (PJENC_INST g)
{
    PUT_MARKER(g, MARKER_SOI);
}


/*____________________________________________________________________________
 |              |                                                             |
 | em_write_EOI | Writes End-Of-Image marker                                  |
 |______________|_____________________________________________________________|
*/
static void em_write_EOI (PJENC_INST g)
{
    PUT_MARKER(g, MARKER_EOI);
}


/*____________________________________________________________________________
 |                    |                                                       |
 | em_write_JFIF_APP0 | Writes Application marker for JFIF 1,0                |
 |____________________|_______________________________________________________|
*/
static void em_write_JFIF_APP0 (
    PJENC_INST g,
    int        xRes,    /* X resolution of image (number of pixels per inch) */
    int        yRes)    /* Y resolution of image (number of pixels per inch) */
{
    PUT_MARKER (g, MARKER_APP+0);
    PUT_INT (g, 16);  /* the length */
    
    /*** ID ***/
    *(g->write_buf_next)++ = 'J';
    *(g->write_buf_next)++ = 'F';
    *(g->write_buf_next)++ = 'I';
    *(g->write_buf_next)++ = 'F';
    *(g->write_buf_next)++ = '\0';
    
    /*** Version 1.0 ***/
    *(g->write_buf_next)++ = 0x01;
    *(g->write_buf_next)++ = 0x00;
    
    /*** Units (dots per inch) ***/
    *(g->write_buf_next)++ = 0x01;
    
    PUT_INT(g, xRes);
    PUT_INT(g, yRes);

    /*** Thumbnail X and Y ***/
    *(g->write_buf_next)++ = 0x00;
    *(g->write_buf_next)++ = 0x00;
}


/*____________________________________________________________________________
 |                  |                                                         |
 | em_write_G3_APP1 | Writes Application marker for G3 color fax standard     |
 |__________________|_________________________________________________________|
*/
static void em_write_G3_APP1 (
    PJENC_INST g,
    int        res)    /* resolution of image (number of pixels per inch) */
{
    res = ((res+50)/100)*100;   /* round res to the nearest 100 */

    PUT_MARKER (g, MARKER_APP+1);
    PUT_INT (g, 12);  /* the length */

    /*** ID ***/
    *(g->write_buf_next)++ = 'G';
    *(g->write_buf_next)++ = '3';
    *(g->write_buf_next)++ = 'F';
    *(g->write_buf_next)++ = 'A';
    *(g->write_buf_next)++ = 'X';
    *(g->write_buf_next)++ = 0;

    PUT_INT (g, 1994);   /* version is year the std was approved */
    PUT_INT (g, res);    /* finally, the DPI */
}


/*____________________________________________________________________________
 |              |                                                             |
 | em_write_SOF | Writes Start-Of-Frame marker                                |
 |______________|_____________________________________________________________|
*/
static void em_write_SOF (
    PJENC_INST g,
    int        width,         /* width of image (number of pixels per row) */
    int        height,        /* height of image (number of rows) */
    int        ncomps,        /* # of color components; 1 is gray, 3 is color */
    BYTE       h_sam_facs[],  /* horizontal sampling factors */
    BYTE       v_sam_facs[])  /* vertical sampling factors */
{
    int i;

    PUT_MARKER(g, MARKER_SOF0);
    PUT_INT(g, 8 + ncomps*3);  /* the length */
    *(g->write_buf_next)++ = 8;
    PUT_INT(g, height);
    PUT_INT(g, width);
    *(g->write_buf_next)++ = ncomps;

    for (i=0; i<ncomps; i++) {
       *(g->write_buf_next)++ = i;
       *(g->write_buf_next)++ = h_sam_facs[i]<<4 | v_sam_facs[i];
       *(g->write_buf_next)++ = (i==0 ? 0 : 1);
    }
}


/*____________________________________________________________________________
 |              |                                                             |
 | em_write_DQT | Writes Define-Quantization-Table marker                     |
 |______________|_____________________________________________________________|
*/
static void em_write_DQT (
    PJENC_INST g,
    int        precision,        /* 0 = 8-bit, 1 = 16-bit */
    int        ident,            /* which table, 0-3 */
    BYTE       elements[64])
{
    PUT_MARKER(g, MARKER_DQT);
    PUT_INT(g, 67);  /* the length */
    *(g->write_buf_next)++ = (precision << 4) + ident;
    memcpy (g->write_buf_next, elements, 64);
    g->write_buf_next += 64;
}


/*____________________________________________________________________________
 |               |                                                            |
 | em_write_DHTs | Writes Define-Huffman-Tables marker                        |
 |_______________|____________________________________________________________|
*/
static void em_write_DHTs (
    PJENC_INST  g,
    int         ntables,          /* number of tables */
    BYTE        hclass[],         /* 0 = DC or lossless table, 1 = AC table */
    BYTE        ident[],          /* 0-3 = which Huffman table this is */
    const BYTE *counts[],    /* # Huffman codes of lengths 1-16 */
    const BYTE *huffval[])   /* list of associated values */
{
    int nvals, i, j;

    nvals = 0;
    for (i=0; i<ntables; i++) {
        for (j=0; j<16; j++)
            nvals += counts[i][j];
    }

    PUT_MARKER(g, MARKER_DHT);
    PUT_INT(g, 2 + 17*ntables + nvals);  /* the length */

    for (i=0; i<ntables; i++) {
        for (nvals=j=0; j<16; j++)
            nvals += counts[i][j];

        *(g->write_buf_next)++ = hclass[i]<<4 | ident[i];
        for (j=0; j<16; j++)    *(g->write_buf_next)++ = counts[i][j];
        for (j=0; j<nvals; j++) *(g->write_buf_next)++ = huffval[i][j];
    }
}


/*____________________________________________________________________________
 |              |                                                             |
 | em_write_SOS | Writes Start-Of-Scan marker                                 |
 |______________|_____________________________________________________________|
*/
static void em_write_SOS (
    PJENC_INST g,
    int        ncomps)  /* number of color components; 1 (gray) or 3 (color) */
{
    int i;

    PUT_MARKER(g, MARKER_SOS);
    PUT_INT(g, 6 + 2*ncomps);  /* the length */
    *(g->write_buf_next)++ = ncomps;

    for (i=0; i<ncomps; i++) {
        *(g->write_buf_next)++ = i;
        *(g->write_buf_next)++ = (i==0 ? 0x00 : 0x11);
    }

    *(g->write_buf_next)++ = 0;
    *(g->write_buf_next)++ = 63;
    *(g->write_buf_next)++ = 0;
}


/*____________________________________________________________________________
 |              |                                                             |
 | em_write_DNL | Writes Define-Number-of-Lines marker                        |
 |______________|_____________________________________________________________|
*/
static void em_write_DNL (
    PJENC_INST g,
    int nlines)  /* number of lines (raster rows) in the JPEG file */
{
    PUT_MARKER(g, MARKER_DNL);
    PUT_INT(g, 4);  /* the length */
    PUT_INT(g, nlines);
}



/*____________________________________________________________________________
 |                       |                                                    |
 | em_write_short_header | Writes a short non-standard header                 |
 |_______________________|____________________________________________________|
 |                                                                            |
 | This header is also output by the firmware, and is only in this            |
 | JPEG encoder for testing purposes.                                         |
 |____________________________________________________________________________|
*/
static void em_write_short_header (
    PJENC_INST g,
    UINT  rows_in_page,
    UINT  pixels_in_row,
    UINT  xRes,
    UINT  yRes,
    UINT  dc_q_factor,
    UINT  ac_q_factor,
    UINT  comps_in_pixel,
    DWORD sample_factors)
{
    PUT_MARKER (g, MARKER_SHORT_HEADER);
    PUT_INT (g, 18);                              /* the length */
    PUT_INT (g, rows_in_page);
    PUT_INT (g, pixels_in_row);
    PUT_INT (g, xRes);
    PUT_INT (g, yRes);
    *(g->write_buf_next)++ = ac_q_factor * 5 / 2;
    *(g->write_buf_next)++ = comps_in_pixel;
    PUT_INT (g, sample_factors >> 16);               /* horiz sample factors */
    PUT_INT (g, sample_factors & 0x0000ffffu);       /* vert  sample factors */
    *(g->write_buf_next)++ = dc_q_factor * 5 / 2;
    *(g->write_buf_next)++ = 0;                   /* reserved for future use */
}



/******************************************************************************
 ******************************************************************************

                           WINOGRAD DCT SECTION

 ******************************************************************************
 ******************************************************************************/



#define BITS_IN_DCT_FRAC 15
#define ROUND(x) (((x)+(1l<<(BITS_IN_DCT_FRAC-1))) >> BITS_IN_DCT_FRAC)


/*____________________________________________________________________________
 |          |                                                                 |
 | As table | The As multiplication constants used in the Winograd transform  |
 |__________|_________________________________________________________________|
 |                                                                            |
 | These have 15 bits of fraction.                                            |
 |____________________________________________________________________________|
*/
#define a1 23170L 
#define a2 17734L
#define a3 23170L
#define a4 42813L
#define a5 12540L


/*____________________________________________________________________________
 |               |                                                            |
 | wino_norm_tbl | DCT scale-factors; this table has been zigzagged           |
 |_______________|____________________________________________________________|
*/
/* todo: do this in fixed-point (see image_proc.c in firmware) */
static float const wino_norm_tbl[] = {
    0.125000f, 0.090120f, 0.090120f, 0.095671f,
    0.064973f, 0.095671f, 0.106304f, 0.068975f,
    0.068975f, 0.106304f, 0.125000f, 0.076641f,
    0.073223f, 0.076641f, 0.125000f, 0.159095f,
    0.090120f, 0.081361f, 0.081361f, 0.090120f,
    0.159095f, 0.230970f, 0.114701f, 0.095671f,
    0.090404f, 0.095671f, 0.114701f, 0.230970f,
    0.453064f, 0.166520f, 0.121766f, 0.106304f,
    0.106304f, 0.121766f, 0.166520f, 0.453064f,
    0.326641f, 0.176777f, 0.135299f, 0.125000f,
    0.135299f, 0.176777f, 0.326641f, 0.346760f,
    0.196424f, 0.159095f, 0.159095f, 0.196424f,
    0.346760f, 0.385299f, 0.230970f, 0.202489f,
    0.230970f, 0.385299f, 0.453064f, 0.293969f,
    0.293969f, 0.453064f, 0.576641f, 0.426777f,
    0.576641f, 0.837153f, 0.837153f, 1.642134f
};


/*____________________________________________________________________________
 |                |                                                           |
 | scale_for_wino | Computes quant-table and threshold-table for Wino's DCT   |
 |________________|___________________________________________________________|
*/
static void scale_for_wino (
    BYTE *in,        /* in:  regular quantization table */
    int  *out,       /* out: winograd quantization table */
    int  *thresh)    /* out: threshold table */
{
    #define FIX(x)  ((long) ((x)*(1l<<BITS_IN_DCT_FRAC) + 0.5))
    float const *fptr;
    int i, q;

    /* Note that in order for FIX(fptr[i]/in[i]) to fit inside 16-bit signed
     * int, (fptr[i]/in[i] < 1) => (in[i] > fptr[i]) (1). Since in[i] >= 1,
     * and (fptr[i] < 1) for i = 0 to 62, (1) is true for i = 0 to 62. For
     * i = 63, (1) is true only when in[63] >= 2
     */
    if (in[63] < 2) in[63] = 2;

    fptr = wino_norm_tbl;
    for (i=0; i<64; i++) {
        q = FIX (*fptr++ / (float)(*in++));
        *out++ = q;
        if (q == 0) *thresh++ = 32767;
        else        *thresh++ = (1 << (BITS_IN_DCT_FRAC-1)) / q;
    }
}



/******************************************************************************
 ******************************************************************************

                              E N C O D I N G

 ******************************************************************************
 ******************************************************************************/



static const BYTE zigzag_index_table[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};


#define WRITE_HUFF_CODE(g,val,huffman) {                                \
    int whc_val = (int)(val);                                                 \
    WRITE_BITS_HIZERO (g, huffman[whc_val].code, huffman[whc_val].size)    \
}



static void zero_prior_DC (PJENC_INST g)
{
    g->prior_DC[0] = g->prior_DC[1] = g->prior_DC[2] = g->prior_DC[3] = 0;
}



/*____________________________________________________________________________
 |             |                                                              |
 | encode_init | Inits this section                                           |
 |_____________|______________________________________________________________|
*/
static void encode_init (PJENC_INST g)
{
    BYTE const *zig_p;
    int       **block_pp;

    zig_p = zigzag_index_table;
    for (block_pp=g->enc_block_zz; block_pp<g->enc_block_zz+(64+16); block_pp++)
        *block_pp = &(g->enc_block[*zig_p++]);
}



/*____________________________________________________________________________
 |              |                                                             |
 | encode_block | Encodes an 8x8 block of pixels into Huffman data            |
 |______________|_____________________________________________________________|
 |                                                                            |
 | The input data is variable 'enc_block' via 'enc_block_zz'.                 |
 |                                                                            |
 | The data to be quantized is compared to thresh to determine if the         |
 | quantized data will be zero.  In most cases, this is true, thus            |
 | eliminating the quantization steps (multiplication + add + shift).         |
 |____________________________________________________________________________|
*/
static void encode_block (
    PJENC_INST         g,
    int                comp,       /* image component number */
    const huff_elem_t *dc_huff_p,
    const huff_elem_t *ac_huff_p,
    int               *quant_p,
    int               *thresh_p)
{
    int  **block_p;
    int    i, data, absdata, run, siz;
    int    diff, absdiff;
    WRITE_BITS_OPEN(g)

      /************************************/
     /* Quantize and Encode DC component */
    /************************************/

    thresh_p++;
    block_p = g->enc_block_zz;
    data = ROUND ((long)*(*block_p++) * (*quant_p++));
    absdiff = diff = data - g->prior_DC[comp];
    if (absdiff < 0) absdiff = -absdiff;
    g->prior_DC[comp] = data;

    if (absdiff < 256) siz = codesize_array[absdiff];
    else               siz = codesize_array[absdiff>>8] + 8;

    WRITE_HUFF_CODE (g, siz, dc_huff_p)
    WRITE_BITS (g, diff<0 ? diff-1 : diff, siz)

      /************************************************/
     /* Quantize, Zigzag, and Encode AC coefficients */
    /************************************************/

    run = 0;

    for (i=63; i>0; i--)   /* do 63 times... */
    {
        absdata = data = *(*block_p++);
        if (absdata < 0) absdata = -absdata;

        if (absdata <= *thresh_p++) {
            /* quantization would be zero */
            quant_p++;
            run++;       /* increment run-length of zeroes */
        }
        else     /* need to quantize */
        {
            while (run >= 16) {
                WRITE_HUFF_CODE (g, RUN_OF_16, ac_huff_p)
                run -= 16;
            }

            absdata = ROUND ((DWORD )absdata * (DWORD )(*quant_p++));
            if (absdata < 256) siz = codesize_array[absdata];
            else               siz = codesize_array[absdata>>8] + 8;

            /* output the RLE code, and the AC term */
            WRITE_HUFF_CODE (g, (run<<4) + siz, ac_huff_p)
            WRITE_BITS (g, data<0 ? ~absdata : absdata, siz)
            run = 0;
        }
    }

    if (run>0 || g->fDenali)
        WRITE_HUFF_CODE (g, 0, ac_huff_p)   /* output EOB code */

    WRITE_BITS_CLOSE(g)
}



/*____________________________________________________________________________
 |            |                                                               |
 | encode_MCU | encodes the given MCU from the input rows in in_rows_ap       |
 |____________|_______________________________________________________________|
*/
static void encode_MCU (
    PJENC_INST g,
    UINT       mcu_index)
{
    int   *block_p;
    UINT   comp;
    UINT   h_block, v_block;
    UINT   ul_row, ul_col;
    UINT   row;
    BYTE **row_pp;
    BYTE  *row_p;

    for (comp=0; comp<g->comps_in_pixel; comp++) {
        for (v_block=0; v_block<g->vert_sam_facs[comp]; v_block++) {
            for (h_block=0; h_block<g->horiz_sam_facs[comp]; h_block++) {

                /***** level-shift and copy the block into 'enc_block' *****/

                ul_row = v_block*8;
                ul_col = (mcu_index*g->horiz_sam_facs[comp] + h_block) * 8;
                row_pp = &(g->in_rows_ap[comp][ul_row]);
                block_p = g->enc_block;

                for (row=0; row<8; row++) {
                    row_p = *row_pp++ + ul_col;

                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p++ - 128;
                    *block_p++ = (int)(UINT)*row_p   - 128;
                }

                dct_forward (g->enc_block);

                /***** encode and output the block *****/
    
                if (comp == 0) {
                    encode_block (g, comp, lum_DC_table, g->fDenali ? lum_AC_table_Denali : lum_AC_table,
                                  g->wino_lum_quant, g->wino_lum_quant_thres);
                } else {
                    encode_block (g, comp, chrom_DC_table, chrom_AC_table,
                                  g->wino_chrom_quant, g->wino_chrom_quant_thres);
                }
            }
        }
    }
}



/*____________________________________________________________________________
 |              |                                                             |
 | copy_in_rows | copies a row (or rows) from input buffer to in_rows_ap      |
 |______________|_____________________________________________________________|
 |                                                                            |
 | If input_subsampled is true, the input row data must be in the same odd    |
 | order which the ASIC outputs it.                                           |
 |                                                                            |
 | View a period as being a group of max_horiz_sam_fac by max_vert_sam_fac    |
 | samples.  (BTW, an MCU contains exactly 8x8 periods.)  Divide the entire   |
 | image into a grid of such periods.  The ASIC outputs the periods left to   |
 | right, top to bottom.  It outputs all the data in each period, before      |
 | moving to the next period.  Within a period, it outputs the components in  |
 | succession; within a component, it outputs the samples left to right, top  |
 | to bottom.                                                                 |
 |                                                                            |
 | For example:                                                               |
 |     Three components = Y U V                                               |
 |     Horiz sample factors = 2 1 1                                           |
 |     Vert  sample factors = 2 1 1                                           |
 |     A period is 2x2 samples (i.e., a 2x2 square).                          |
 |     Each period contains these samples:  YYYYUV.                           |
 |     Bytes output:  YYYYUV YYYYUV YYYYUV ....                               |
 |                                                                            |
 | In this routine,                                                           |
 |                                                                            |
 |     vert_period  = index of which period we're inputting vertically within |
 |                    an MCU (0-7).                                           |
 |                                                                            |
 |     horiz_period = index of which period we're inputting horizontally;     |
 |                    since an MCU has 8 periods, this is 0 .. 8*mcus_in_row. |
 |____________________________________________________________________________|
*/
static void copy_in_rows (
    PJENC_INST g,
    UINT  row_index,         /* in:  index of next row to get within MCU */
    BYTE *inbuf_p,           /* in:  input row data */
    UINT *n_rows_p,          /* out: # of rows copied in */
    UINT *n_bytes_p)         /* out: # of bytes copied in */
{
    BYTE *in_p;
    UINT  comp;
    UINT  vert_period, vert_mod;

    vert_period = row_index / g->max_vert_sam_fac;
    vert_mod    = row_index % g->max_vert_sam_fac;

    if (!g->input_subsampled || (g->max_vert_sam_fac==1 && g->max_horiz_sam_fac==1))
    {
          /********************************************************/
         /* Perform subsampling (or copying of non-sampled data) */
        /********************************************************/

        BYTE *row_p;
        UINT  sam_fac;
        UINT  col, horiz_mod;
        UINT  inc;

        for (comp=0; comp<g->comps_in_pixel; comp++) {
            sam_fac = g->vert_sam_facs[comp];

            if (vert_mod < sam_fac) {
                in_p = inbuf_p + comp;
                row_p = g->in_rows_ap[comp][vert_period*sam_fac + vert_mod];
                horiz_mod = 0;
                sam_fac = g->horiz_sam_facs[comp];

                if (sam_fac == g->max_horiz_sam_fac) {

                    /***** fast case: copying all pixels *****/

                    if (g->comps_in_pixel == 1) {
                        memcpy (row_p, in_p, g->pixels_in_row);
                    } else {
                        for (col=0; col<g->pixels_in_row; col+=4) {
                            *row_p++ = *in_p;  in_p += g->comps_in_pixel;
                            *row_p++ = *in_p;  in_p += g->comps_in_pixel;
                            *row_p++ = *in_p;  in_p += g->comps_in_pixel;
                            *row_p++ = *in_p;  in_p += g->comps_in_pixel;
                        }
                    }

                } else if (sam_fac==1 && g->max_horiz_sam_fac==2) {

                    /***** fast case: copying every other pixel *****/

                    inc = 2 * g->comps_in_pixel;
                    for (col=0; col<g->pixels_in_row; col+=8) {
                        *row_p++ = *in_p; in_p += inc;
                        *row_p++ = *in_p; in_p += inc;
                        *row_p++ = *in_p; in_p += inc;
                        *row_p++ = *in_p; in_p += inc;
                    }

                } else {

                    /***** slow general case *****/

                    for (col=0; col<g->pixels_in_row; col++) {
                        if (horiz_mod < sam_fac) *row_p++ = *in_p;
                        in_p += g->comps_in_pixel;
                        horiz_mod += 1;
                        if (horiz_mod == g->max_horiz_sam_fac) horiz_mod = 0;
                    }
                }
            }
        }

        *n_rows_p = 1;
        *n_bytes_p = g->comps_in_pixel * g->pixels_in_row;

    } else {

          /************************************************/
         /* Already subsampled (in the ASIC's odd order) */
        /************************************************/

        UINT horiz_period, periods_in_row;
        UINT ul_row, ul_col, row, col;
        BYTE  *row_y1_p, *row_y2_p, *row_cb_p, *row_cr_p;

        in_p = inbuf_p;
        periods_in_row = g->mcus_in_row * 8;

        if (g->sample_factors == 0x21102110u) {  /* fast case: 4-1-1 subsampling */
            row_y1_p = g->in_rows_ap[0][2*vert_period];
            row_y2_p = g->in_rows_ap[0][2*vert_period+1];
            row_cb_p = g->in_rows_ap[1][vert_period];
            row_cr_p = g->in_rows_ap[2][vert_period];
            for (horiz_period=0; horiz_period<periods_in_row; horiz_period++) {
                *row_y1_p++ = *in_p++;
                *row_y1_p++ = *in_p++;
                *row_y2_p++ = *in_p++;
                *row_y2_p++ = *in_p++;
                *row_cb_p++ = *in_p++;
                *row_cr_p++ = *in_p++;
            }
        } else {  /* slow general case */

            for (horiz_period=0; horiz_period<periods_in_row; horiz_period++) {
                for (comp=0; comp<g->comps_in_pixel; comp++) {
                    ul_row =  vert_period *  g->vert_sam_facs[comp];
                    ul_col = horiz_period * g->horiz_sam_facs[comp];

                    for (row=ul_row; row<ul_row+ g->vert_sam_facs[comp]; row++)
                    for (col=ul_col; col<ul_col+g->horiz_sam_facs[comp]; col++) {
                        g->in_rows_ap[comp][row][col] = *in_p++;
                    }
                }
            }
        }

        *n_rows_p = g->max_vert_sam_fac;
        *n_bytes_p = in_p - inbuf_p;
    }
}


/******************************************************************************
 ******************************************************************************

                             EXPORTED ROUTINES

 ******************************************************************************
 ******************************************************************************/



/*____________________________________________________________________________
 |               |                                                            |
 | scale_q_table | scales a q-table according to the q-factors                |
 |_______________|____________________________________________________________|
*/
static void scale_q_table (
    UINT        dc_q_factor,
    UINT        ac_q_factor,
    const BYTE *in,
    BYTE       *out)
{
    #define FINAL_DC_INDEX  9

    UINT i, val;
    UINT q;

    q = dc_q_factor;

    for (i=0; i<64; i++) {
        val = ((*in++)*q + Q_DEFAULT/2) / Q_DEFAULT;
        if (val < 1)   val = 1;
        if (val > 255) val = 255;
        *out++ = (BYTE)val;
        if (i == FINAL_DC_INDEX)
            q = ac_q_factor;
    }
}



/*____________________________________________________________________________
 |               |                                                            |
 | output_header | outputs the JPEG header                                    |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Inputs:  lum_quant, chrom_quant, xRes, yRes, pixels_in_row, comps_in_pixel,|
 |          horiz_sam_facs, vert_sam_facs                                     |
 |____________________________________________________________________________|
*/
static UINT output_header (
    PJENC_INST g,
    BYTE       *buf_p)
{
          BYTE  hclass[4];
          BYTE  ident[4];
    const BYTE *counts[4];
    const BYTE *huffval[4];

    hclass [0] = 0;
    ident  [0] = 0;
    counts [0] = lum_DC_counts;
    huffval[0] = lum_DC_values;

    hclass [1] = 1;
    ident  [1] = 0;
    counts [1] = g->fDenali ? lum_AC_counts_Denali : lum_AC_counts;
    huffval[1] = lum_AC_values;

    hclass [2] = 0;
    ident  [2] = 1;
    counts [2] = chrom_DC_counts;
    huffval[2] = chrom_DC_values;

    hclass [3] = 1;
    ident  [3] = 1;
    counts [3] = chrom_AC_counts;
    huffval[3] = chrom_AC_values;

    write_buf_open (g, buf_p);
        em_write_SOI (g);

        if (0  /* todo */ ) {
            em_write_short_header (g, g->rows_in_page<0 ? 0 : g->rows_in_page,
                                   g->pixels_in_row,
                                   g->xRes, g->yRes,
                                   g->dc_q_factor, g->ac_q_factor,
                                   g->comps_in_pixel,
                                   g->sample_factors);
        } else {
            if (g->fOutputG3APP1)
                em_write_G3_APP1 (g, g->xRes);
            else
                em_write_JFIF_APP0 (g, g->xRes, g->yRes);
            em_write_SOF (g, g->pixels_in_row, g->rows_in_page<0 ? 0 : g->rows_in_page,
                          g->comps_in_pixel, g->horiz_sam_facs, g->vert_sam_facs);
            em_write_DQT (g, 0, 0, g->lum_quant);
            if (g->comps_in_pixel > 1)
                em_write_DQT (g, 0, 1, g->chrom_quant);
            em_write_DHTs (g, g->comps_in_pixel==1 ? 2 : 4,
                           hclass, ident, counts, huffval);
            em_write_SOS (g, g->comps_in_pixel);
        }
    return write_buf_close (g);
}



/*****************************************************************************\
 *
 * jpgEncode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD jpgEncode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PJENC_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(JENC_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(JENC_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_setDefaultInputTraits - Specifies default input image traits
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

static WORD jpgEncode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PJENC_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->traits = *pTraits;   /* a structure copy */

    INSURE (g->traits.iPixelsPerRow > 0);
    INSURE (g->traits.iComponentsPerPixel==1 || g->traits.iComponentsPerPixel==3);

    g->pixels_in_row  = g->traits.iPixelsPerRow;
    g->comps_in_pixel = g->traits.iComponentsPerPixel;
    g->rows_in_page   = g->traits.lNumRows;
    g->xRes           = g->traits.lHorizDPI >> 16;
    g->yRes           = g->traits.lVertDPI  >> 16;
    if (g->xRes < 0) g->xRes = 300;
    if (g->yRes < 0) g->yRes = 300;

    g->fDidHeader = FALSE;
    return IP_DONE;

    fatal_error:
        return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD jpgEncode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PJENC_INST g;
    UINT       qfacs;

    HANDLE_TO_PTR (hXform, g);

    qfacs                 =       aXformInfo[IP_JPG_ENCODE_QUALITY_FACTORS].dword;
    g->sample_factors     =       aXformInfo[IP_JPG_ENCODE_SAMPLE_FACTORS].dword;
    g->input_subsampled   =       aXformInfo[IP_JPG_ENCODE_ALREADY_SUBSAMPLED].dword;
    g->fDenali            = (BOOL)aXformInfo[IP_JPG_ENCODE_FOR_DENALI].dword;
    g->fOutputDNL         = (BOOL)aXformInfo[IP_JPG_ENCODE_OUTPUT_DNL].dword;
    g->fOutputG3APP1      = (BOOL)aXformInfo[IP_JPG_ENCODE_FOR_COLOR_FAX].dword;
    g->dwDummyHeaderBytes =       aXformInfo[IP_JPG_ENCODE_DUMMY_HEADER_LEN].dword;

    g->dc_q_factor = (BYTE)(qfacs >> 8);
    g->ac_q_factor = (BYTE)(qfacs);

    return IP_DONE;

    fatal_error:
        return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD jpgEncode_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    PJENC_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;

    fatal_error:
        return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD jpgEncode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PJENC_INST g;

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
 * jpgEncode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD jpgEncode_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PJENC_INST g;
    WORD       n;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen = g->comps_in_pixel * g->pixels_in_row;

    n = OUTBUF_NUM_MCUS * MAX_MCU_SIZE;
    if (n < MAX_HEADER_SIZE) n = MAX_HEADER_SIZE;
    *pdwMinOutBufLen = n;

    return IP_DONE;

    fatal_error:
        return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD jpgEncode_convert (
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
    PJENC_INST g;
    unsigned   ret_val;
    UINT       row, n_rows, n_bytes;
    int        comp;
    UINT       row_len;
    UINT       n_loaded;

    HANDLE_TO_PTR (hXform, g);

    *pdwInputNextPos  = g->dwInNextPos;
    *pdwInputUsed     = 0;
    *pdwOutputThisPos = g->dwOutNextPos;
    *pdwOutputUsed    = 0;
    ret_val           = IP_READY_FOR_DATA;

      /****************************************************/
     /* Init and output the Header if we haven't already */
    /****************************************************/

    if (! g->fDidHeader) {
        UINT  row_len;
        BYTE *p;
        DWORD factors;
        UINT  fac;

        /* Init */

        g->rows_loaded = 0;
        g->mcus_sent = 0;
        g->loading_rows = TRUE;
        write_init (g);
        zero_prior_DC (g);
        encode_init (g);

        if (g->ac_q_factor == 0)
            g->ac_q_factor = Q_DEFAULT;
        if (g->dc_q_factor == 0)
            g->dc_q_factor = g->ac_q_factor;

        if (g->sample_factors == 0)
            g->sample_factors = g->comps_in_pixel==1 ? MONO_FACTORS : COLOR_FACTORS;

        factors = g->sample_factors >> 16;
        g->max_horiz_sam_fac = 0;
        for (comp=3; comp>=0; comp--) {
            fac = factors & 0x0fu;
            if (fac > g->max_horiz_sam_fac) g->max_horiz_sam_fac = fac;
            g->horiz_sam_facs[comp] = fac;
            factors >>= 4;
        }

        factors = g->sample_factors & 0x0000ffffu;
        g->max_vert_sam_fac = 0;
        for (comp=3; comp>=0; comp--) {
            fac = factors & 0x0fu;
            if (fac > g->max_vert_sam_fac) g->max_vert_sam_fac = fac;
            g->vert_sam_facs[comp] = fac;
            factors >>= 4;
        }

        g->cols_in_mcu = g->max_horiz_sam_fac * 8;
        g->rows_in_mcu =  g->max_vert_sam_fac * 8;
        g->mcus_in_row = (g->pixels_in_row + g->cols_in_mcu - 1) / g->cols_in_mcu;

        scale_q_table (g->dc_q_factor, g->ac_q_factor, orig_lum_quant,   g->lum_quant);
        scale_q_table (g->dc_q_factor, g->ac_q_factor, orig_chrom_quant, g->chrom_quant);

        /* scale lum_quant & chrom_quant for Wino DCT */
        scale_for_wino (g->lum_quant,   g->wino_lum_quant,   g->wino_lum_quant_thres);
        scale_for_wino (g->chrom_quant, g->wino_chrom_quant, g->wino_chrom_quant_thres);

        g->whitePixel[0] = 255;   /* YCC value of a white pixel (color) */
        g->whitePixel[1] = 128;   /* The 255 is white in mono too */
        g->whitePixel[2] = 128;
        g->whitePixel[3] = 255;

        g->dwOutNextPos = *pdwOutputUsed = (g->dwDummyHeaderBytes == 0)
            ? output_header (g,pbOutputBuf)
            : g->dwDummyHeaderBytes;

        *pdwOutputThisPos = 0;

        /* Allocate the row-buffers in in_rows_ap */

        memset (g->in_rows_ap, 0, sizeof(g->in_rows_ap));

        for (comp=0; comp<(int)g->comps_in_pixel; comp++) {
            row_len = g->horiz_sam_facs[comp] * g->mcus_in_row * 8;
            n_rows = g->vert_sam_facs[comp] * 8;

            for (row=0; row<n_rows; row++) {
                IP_MEM_ALLOC (row_len, p);
                g->in_rows_ap[comp][row] = p;
            }
        }

        *pdwInputUsed    = 0;
        *pdwInputNextPos = 0;
        g->dwInNextPos = 0;
        g->fDidHeader = TRUE;

        return IP_READY_FOR_DATA;
    }

      /*********************************/
     /* We are filling the input rows */
    /*********************************/

    if (g->loading_rows) {

        /***** Init all row-buffers to white if starting new row-set *****/

        n_loaded = g->rows_loaded % g->rows_in_mcu;

        if (n_loaded == 0) {
            for (comp=0; comp<g->comps_in_pixel; comp++) {
                row_len = g->horiz_sam_facs[comp] * g->mcus_in_row * 8;
                n_rows = g->vert_sam_facs[comp] * 8;

                for (row=0; row<n_rows; row++)
                    memset (g->in_rows_ap[comp][row], g->whitePixel[comp], row_len);
            }
        }

        if (pbInputBuf == NULL) {

            /***** We are being told to flush *****/

            if (n_loaded != 0) {
                /* some rows were loaded; start compressing rows now */
                g->loading_rows = FALSE;
                /* boost rows_loaded to next multiple of rows_in_mcu so
                 * n_loaded will be 0 next time around */
                g->rows_loaded += g->rows_in_mcu - n_loaded;
            } else if ((g->rows_in_page<0 || (int)g->rows_received<g->rows_in_page)
                       && g->dwDummyHeaderBytes == 0) {
                /* row-count was unknown or too big, output header w/ correct row-count */
                g->rows_in_page = g->rows_received;
                *pdwOutputUsed    = output_header (g, pbOutputBuf);
                *pdwOutputThisPos = 0;
            } else {
                /* no rows were loaded, and row-count is valid, so we're done */
                PRINT (_T("jpeg_encode_convert_row: Done\n"),0,0);
                write_buf_open (g, pbOutputBuf);
                    write_bits_flush (g);
                    if (g->fOutputDNL)
                        em_write_DNL (g, g->rows_received);
                    em_write_EOI (g);
                *pdwOutputUsed = write_buf_close (g);
                ret_val |= IP_DONE;
            }
        } else {

            /***** Copy the row(s) into in_rows_ap *****/

            copy_in_rows (
                g,
                n_loaded,              /* in:  index of next row to get */
                pbInputBuf,            /* in:  copied-in row data */
                &n_rows,               /* out: # of rows copied in */
                &n_bytes);             /* out: # of bytes fetched */

            *pdwInputUsed     = n_bytes;
            *pdwInputNextPos  = (g->dwInNextPos += n_bytes);
            g->rows_loaded   += n_rows;
            g->rows_received += n_rows;
            n_loaded = g->rows_loaded % g->rows_in_mcu;

            /* it's hard to set IP_PRODUCED_ROW 8 times per 8 rows, so cheat */
            if (n_rows > 0)
                ret_val |= IP_CONSUMED_ROW | IP_PRODUCED_ROW;

            /***** Check if all needed rows are loaded *****/

            if (n_loaded == 0) {
                /* # rows loaded is rows_in_mcu, but the mod wrapped to zero */
                g->loading_rows = FALSE;  /* start compressing rows now */
            }
        }
    }

      /************************************************************/
     /* Compress as many MCUs as will fit into the output buffer */
    /************************************************************/

    while (!g->loading_rows &&
           dwOutputAvail-*pdwOutputUsed > MAX_MCU_SIZE) {
        PRINT (_T("jpeg_encode_convert_row: Encoding MCU, mcus_sent=%d\n"),
                g->mcus_sent, 0);

        write_buf_open (g, pbOutputBuf + *pdwOutputUsed);
            encode_MCU (g, g->mcus_sent);
        n_bytes = write_buf_close (g);

        g->dwOutNextPos += n_bytes;
        *pdwOutputUsed  += n_bytes;

        g->mcus_sent += 1;
        if (g->mcus_sent >= g->mcus_in_row) {
            g->loading_rows = TRUE;
            g->mcus_sent = 0;
        }
    }

    PRINT (_T("jpeg_encode_convert_row: Returning %04x, out_used=%d\n"),
           ret_val, *pdwOutputUsed);
    return ret_val;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD jpgEncode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * jpgEncode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD jpgEncode_newPage (
    IP_XFORM_HANDLE hXform)
{
    PJENC_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD jpgEncode_closeXform (IP_XFORM_HANDLE hXform)
{
    PJENC_INST g;
    BYTE       **row_pp, **after_pp, *p;

    HANDLE_TO_PTR (hXform, g);
    row_pp = &(g->in_rows_ap[0][0]);
    after_pp = row_pp + (sizeof(g->in_rows_ap)/sizeof(BYTE*));

    for ( ; row_pp<after_pp; row_pp++) {
        p = *row_pp;
        if (p != NULL) {
            IP_MEM_FREE (p);
            *row_pp = NULL;
        }
    }

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgEncodeTbl - Jump-table for encoder
 *
\*****************************************************************************/

IP_XFORM_TBL jpgEncodeTbl = {
    jpgEncode_openXform,
    jpgEncode_setDefaultInputTraits,
    jpgEncode_setXformSpec,
    jpgEncode_getHeaderBufSize,
    jpgEncode_getActualTraits,
    jpgEncode_getActualBufSizes,
    jpgEncode_convert,
    jpgEncode_newPage,
    jpgEncode_insertedData,
    jpgEncode_closeXform
};

/* End of File */

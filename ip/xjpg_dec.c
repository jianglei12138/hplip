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
 * xjpg_dec.c - Decodes a JPEG file into a raw gray image
 *
 *****************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    jpgDecodeTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_JPG_DECODE_OUTPUT_SUBSAMPLED]:
 *            Output only subsampled raw data? 0=no, 1=yes.
 *    aXformInfo[IP_JPG_DECODE_FROM_DENALI]:
 *            Data came from a Denali? 0=no, 1=yes.
 *
 *    The aXformInfo items above may all be set to 0 for typical JPEG files.
 *
 *    For Denali, we assume the following:
 *        - Every 8x8 block ends with an EOB
 *        - A slight change in the Huffman tables (no 15-bit code)
 *        - Denali sends us a proprietary short header (APP1 marker)
 *
 * Capabilities and Limitations:
 *
 *    Decodes a standard JPEG file.  Also handles JFIF 1.0 (APP0 marker).
 *    Also handles the non-standard short header output by OfficeJet firmware
 *    (APP1 marker).  Also handles APP1 markers defined by color fax standard.
 *    Will *not* decode a non-interleaved file; it must be interleaved.
 *    Handles 1-4 components per pixel, and 4 Huffman tables, so SOF1 with
 *    8 bits/component is okay.
 *
 * Default Input Traits, and Output Traits:
 *
 *    For decoder:
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
 * Kludges:
 *
 *    Chromafax and other software inserts fill 0's instead of fill 1's
 *    before a marker, such as the all-important EOI marker.  So when
 *    we get a syntax error when parsing those 0's, we do not report
 *    an error if there's a following marker, but simply proceed to
 *    process the marker as usual.
 *
 *    Chromafax uses an SOF1 instead of the correct SOF0, so we allow
 *    that, and also handle four Huffman tables that SOF1 requires.
 *
 * Jan 1998 Mark Overton -- Ported to new software Image Processor
 * Apr 1996 Mark Overton -- Finished software-only JPEG decoder
 * Feb 1996 Mark Overton -- initial code
 *
\*****************************************************************************/

#include <string.h>
#include <assert.h>

#include "hpip.h"
#include "ipdefs.h"
#include "setjmp.h"
#include "xjpg_dct.h"
#include "xjpg_mrk.h"


#define DUMP_JPEG 0

#if DUMP_JPEG
    #include "stdio.h"
    #include <tchar.h>

    #define DUMP(msg,arg1,arg2,arg3) \
        _ftprintf(stdout, msg, (int)(arg1), (int)(arg2), (int)(arg3))
#else
    #define DUMP(msg,arg1,arg2,arg3)
#endif


#if 0
    #include "stdio.h"
    #include <tchar.h>
    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stdout, _T("(jpeg) ") msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif


/*____________________________________________________________________________
 |                                                                            |
 | Constants                                                                  |
 |____________________________________________________________________________|
*/

#define MAX_HUFF_TBLS     4

#define UNEXPECTED_MARKER       1
#define BAD_MARKER_ID           2
#define BAD_MARKER_DATA         3
#define NO_RESTART_MARKER       4
#define BAD_HUFF_CODE           5
#define UNEXPECTED_END_OF_DATA  6
#define NOT_IMPLEMENTED         7

#define MAX_MARKER_LEN    15000   /* large in case marker holds a thumbnail */
#define MAX_HEADER_SIZE   (MAX_MARKER_LEN+2000)
#define MAX_BLOCKS_IN_MCU 6
#define MAX_MCU_SIZE      (MAX_BLOCKS_IN_MCU*304)
                                  /* max encoded MCU size, plus stuff-bytes */
#define INBUF_NUM_MCUS    2       /* workbuf will be this multiple of max MCU */

#define DC_TBL_INDEX_LEN  9
#define AC_TBL_INDEX_LEN  12

#define CHECK_VALUE  0x1ce5ca7e



/*____________________________________________________________________________
 |                                                                            |
 | Instance Variables                                                         |
 |____________________________________________________________________________|
*/

typedef struct {
    BYTE  size;     /* number of bits in the Huff code (code is the index) */
    BYTE  value;    /* value that was coded */
} main_huff_elem_t;

typedef struct {
    WORD   code;    /* Huff code to use for the 'value' below */
    BYTE   size;    /* number of bits in above Huff 'code' */
    BYTE   value;   /* value that was coded */
} aux_huff_elem_t;

typedef struct {
    BYTE             *index_p;
    main_huff_elem_t *main_p;
    aux_huff_elem_t  *aux_p;
} huff_tbl_t;


/* Decoding is centered around out_rows_ap.  It is indexed by
 * [color_component_number][row_number].
 * color_component_number 0 is Y (intensity); mono only has this component.
 *
 * Each component in out_rows_ap has a height (# rows) equal to the number of
 * samples in the MCU, and a width equal to the number of samples in all the
 * MCUs in a row. That is, pixels are stored in out_rows_ap with NO REPLICATION.
 * So if a component has sample factors of H and V, it will have 8*V rows and
 * pixels_in_row*H/max_horiz_samp_fac columns in out_rows_ap.
 */

typedef struct {
    BYTE   *out_rows_ap[4][32];  /* row-buffers [component][row] */

    /***** Items from SOF, Start Of Frame *****/

    IP_IMAGE_TRAITS traits;      /* traits of the image */
    BYTE    num_comps;           /* # of components (1 => mono) */
    BYTE    horiz_samp_facs[4];  /* horizontal sampling factors */
    BYTE    vert_samp_facs [4];  /* vertical sampling factors */
    BYTE    max_horiz_samp_fac;  /* max sample factors */
    BYTE    max_vert_samp_fac;
    BYTE    which_quant_tbl[4];  /* selects q tbl for component */
    UINT    rows_per_mcu;        /* # rows & cols in each MCU */
    UINT    cols_per_mcu;
    UINT    mcus_per_row;        /* # of MCUs in each row */
    UINT    rowCountOffset;

    /***** Items from other markers *****/

    WORD    restart_interval;    /* restart interval (0 -> none) */
    long    quant_tbls[4][64];   /* quantization tables */
    BYTE    which_dc_tbl[4];     /* selects DC tbl for component */
    BYTE    which_ac_tbl[4];     /* selects AC tbl for component */
    BOOL    fColorFax;           /* is this from a fax? */

    /***** Huffman tables *****/

    huff_tbl_t dc_tbls[MAX_HUFF_TBLS];
    huff_tbl_t ac_tbls[MAX_HUFF_TBLS];

    /***** Configuration variables *****/

    BOOL     output_subsampled;   /* output subsampled data? */
    BOOL     fDenali;             /* data is from a Denali? */

    /***** Variables used while decoding *****/

    DWORD    dwInNextPos;         /* next read pos in input file */
    DWORD    dwOutNextPos;        /* next write pos in output file */
    long     rows_done;           /* # rows decoded and output */
    UINT     mcus_done;           /* # MCUs decoded so far in row */
    BOOL     sending_rows;        /* returning the decoded rows? */
    BOOL     got_short_header;    /* got an OfficeJet short header? */
    BOOL     got_EOI;             /* hit the end-of-image marker? */
    BYTE     restart_cur_marker;  /* index of next expected marker */
    WORD     restart_cur_mcu;     /* current MCU-count in interval */
    int      prior_dc[4];         /* DC values of prior block */
    jmp_buf  syntax_error;        /* jump-target for syntax errors */
    jmp_buf  old_syntax_error;
    DWORD    dwValidChk;          /* struct validity check value */

    /***** Reading bits variables *****/

    DWORD rd_bit_buf;
        /* Bits to be read from inbuf (read left-to-right). */

    int rd_bits_avail;
        /* Number of bits not yet read in rd_bit_buf (= 32 - number read). */

    BYTE *rd_inbuf_beg;
        /* The beginning of the input buffer. */

    BYTE *rd_inbuf_next;
        /* Next byte in inbuf to be read. */

    /***** Decoding 8x8 blocks *****/

    int  block[64];           /* scratch-pad 8x8 block */
    int *block_zz[64+16];     /* zig-zag ptrs into above block */

} JDEC_INST, *PJDEC_INST;



/*____________________________________________________________________________
 |                                                                            |
 | Forward Routines                                                           |
 |____________________________________________________________________________|
*/

static void huff_define_table (
    PJDEC_INST   g,
    BOOL         ac,         /* defining an AC table? (else DC) */
    UINT         id,         /* which table is being defined (0-3) */
    const BYTE   counts[16], /* number of Huffman codes of each length 1-16 */
    const BYTE   values[]);  /* values associated with codes of above lengths */

void wino_scale_table (long *tbl_p);




/******************************************************************************
 ******************************************************************************

                                R E A D I N G


 Interface into this section:

     read_init          - inits this section
     read_buf_open      - we are being given a (new) input buffer
     read_buf_close     - done with current input buffer; return # bytes used
     read_byte          - returns next input byte (syncs to byte-boundary)
     read_uint          - returns next 2-byte integer (syncs)
     read_skip_forward  - discards the given number of input bytes
     read_skip_backward - backs up the given number of bytes
     READ_BITS_LOAD     - loads N bits of input into lsb's of a var (no advance)
     READ_BITS_ADVANCE  - advance input N bits; you must call this to eat input

 ******************************************************************************
 ******************************************************************************/



/*____________________________________________________________________________
 |           |                                                                |
 | read_init | Inits this section                                             |
 |___________|________________________________________________________________|
*/
static void read_init (PJDEC_INST g)
{
    g->rd_bits_avail = 0;
    g->rd_bit_buf    = 0;
}



/*____________________________________________________________________________
 |               |                                                            |
 | read_buf_open | We are being given a (new) buffer to read input            |
 |_______________|____________________________________________________________|
 |                                                                            |
 | This routine records the location of the new input buffer.                 |
 |____________________________________________________________________________|
*/
static void read_buf_open (PJDEC_INST g, BYTE *buf_p)
{
    g->rd_inbuf_beg  = buf_p;
    g->rd_inbuf_next = buf_p;
}



/*____________________________________________________________________________
 |                |                                                           |
 | read_buf_close | We are done with the current input buffer                 |
 |________________|___________________________________________________________|
 |                                                                            |
 | This function returns # bytes read from the input buffer.                  |
 |____________________________________________________________________________|
*/
static int read_buf_close (PJDEC_INST g)
{
    return g->rd_inbuf_next - g->rd_inbuf_beg;
}



/*____________________________________________________________________________
 |         |                                                                  |
 | rd_sync | Empties the bit-cache, and syncs to a byte-boundary              |
 |_________|__________________________________________________________________|
*/
static void rd_sync (PJDEC_INST g)
{
    g->rd_bits_avail = 0;
}



/*____________________________________________________________________________
 |           |                                                                |
 | read_byte | returns next input byte (syncs to byte-boundary)               |
 |___________|________________________________________________________________|
*/
static BYTE  read_byte (PJDEC_INST g)
{
    rd_sync (g);
    return *(g->rd_inbuf_next)++;
}



/*____________________________________________________________________________
 |           |                                                                |
 | read_uint | returns next 2-byte integer (syncs)                            |
 |___________|________________________________________________________________|
*/
static unsigned read_uint (PJDEC_INST g)
{
    UINT uval;

    rd_sync (g);
    uval = (unsigned)*(g->rd_inbuf_next)++ << 8;
    return uval | *(g->rd_inbuf_next)++;
}



/*____________________________________________________________________________
 |                   |                                                        |
 | read_skip_forward | discards the given number of input bytes               |
 |___________________|________________________________________________________|
*/
static void read_skip_forward (PJDEC_INST g, UINT n)
{
    if (n > MAX_MARKER_LEN)
        longjmp (g->syntax_error, BAD_MARKER_DATA);
    rd_sync (g);
    g->rd_inbuf_next += n;
}



/*____________________________________________________________________________
 |                    |                                                       |
 | read_skip_backward | backs up N bytes in the input buffer                  |
 |____________________|_______________________________________________________|
*/
static void read_skip_backward (PJDEC_INST g, UINT n)
{
    rd_sync (g);
    g->rd_inbuf_next -= n;
}



/*____________________________________________________________________________
 |                |                                                           |
 | READ_BITS_LOAD | loads next par_n_bits of input into par_value (no advance)|
 |________________|___________________________________________________________|
 |                                                                            |
 | If a marker is encountered:                                                |
 |     - this macro will goto hit_marker                                      |
 |     - the marker will be the next two bytes in the buffer                  |
 |     - the cache will be empty (ie, any fill 1's were discarded)            |
 |                                                                            |
 | If the JPEG file erroneously has fill 0's (instead of 1's) before a        |
 | marker, we'll parse them as Huffman codes.  If such a bogus Huffman code   |
 | has more bits than were in our bit-buffer, the extra non-existent bits     |
 | will be returned as zeroes.                                                |
 |____________________________________________________________________________|
*/

#define CANT_LOAD_NEAR_MARKER(g)                                            \
    (g->rd_bits_avail<=0 ||                                                 \
     (g->rd_bits_avail<=7 && (((1u<<g->rd_bits_avail)-1) & ~g->rd_bit_buf) == 0))
        /* we just parsed past erroneous fill 0's  OR */
        /* cached bits are just fill 1's; toss them */


#define READ_BITS_LOAD(g, fixed_len, par_n_bits, par_value, hit_marker)     \
{                                                                           \
    if ((int)(par_n_bits) > g->rd_bits_avail) {                             \
        BYTE  a_byte;                                                       \
                                                                            \
        do {                                                                \
            a_byte = *(g->rd_inbuf_next)++;                                 \
            DUMP (_T("<%02x>"), a_byte, 0, 0);                                  \
                                                                            \
            if (a_byte == (BYTE )0xffu) {                                   \
                if (*(g->rd_inbuf_next)++ != 0) {   /* we hit a marker */   \
                    g->rd_inbuf_next -= 2;                                  \
                    if (CANT_LOAD_NEAR_MARKER(g))                           \
                        goto hit_marker;                                    \
                    break;   /* exit 'while' loop */                        \
                }                                                           \
            }                                                               \
                                                                            \
            g->rd_bit_buf = (g->rd_bit_buf<<8) | a_byte;                    \
            g->rd_bits_avail += 8;                                          \
        } while (g->rd_bits_avail <= 24);                                   \
    }                                                                       \
                                                                            \
    par_value = (g->rd_bit_buf << (32-g->rd_bits_avail)) >> (32-(par_n_bits)); \
}



/*____________________________________________________________________________
 |                   |                                                        |
 | READ_BITS_ADVANCE | discards next par_n_bits of input                      |
 |___________________|________________________________________________________|
*/
#define READ_BITS_ADVANCE(g, par_n_bits)   \
{                                          \
    g->rd_bits_avail -= par_n_bits;        \
}



/******************************************************************************
 ******************************************************************************

                               M A R K E R S


 Interface into this section:

     mar_get   - parses and returns next marker-id
     mar_parse - parses the data associated with the marker

 ******************************************************************************
 ******************************************************************************/



/*____________________________________________________________________________
 |               |                                                            |
 | parse_factors | parses sample-factors as nibbles into array                |
 |_______________|____________________________________________________________|
 |                                                                            |
 | This is only called by parse_app_short_header below.                       |
 | Function return value = maximum sample factor.                             |
 |____________________________________________________________________________|
*/
static UINT parse_factors (
    UINT factors,       /* in:  sample factors, one nibble each, l-to-r */
    BYTE fac_array[4])  /* out: array of sample factors */
{
    int  i;
    UINT fac;
    UINT max_fac;

    max_fac = 0;

    for (i=3; i>=0; i--) {
        fac = factors & 0x000fu;
        factors >>= 4;
        if (fac > max_fac) max_fac = fac;
        fac_array[i] = fac;
    }

    return max_fac;
}



/*____________________________________________________________________________
 |                  |                                                         |
 | calc_quant_table | calculates a quantization table                         |
 |__________________|_________________________________________________________|
 |                                                                            |
 | This is only called by parse_app_short_header below.                       |
 | Warning: The calculation below should match the firmware.                  |
 |____________________________________________________________________________|
*/
static void calc_quant_table (
    PJDEC_INST  g,
    UINT        dc_q_fac,      /* orig DC  is  scaled by (dc_q_fac/50) */
    UINT        ac_q_fac,      /* orig ACs are scaled by (ac_q_fac/50) */
    const BYTE *orig_tbl_p,    /* original table */
    UINT        which_q_tbl)   /* which table is being defined (0-3) */
{
    int   i;
    UINT  quant;
    long *tbl_p;

    tbl_p = g->quant_tbls[which_q_tbl];

    for (i=0; i<64; i++) {
        quant = ((*orig_tbl_p++)*(i==0 ? dc_q_fac : ac_q_fac) + 25u) / 50u;
        if (quant ==  0) quant = 1;
        if (quant > 255) quant = 255;
        tbl_p[i] = quant;
    }

    wino_scale_table (tbl_p);
}



/*____________________________________________________________________________
 |         |                                                                  |
 | mar_get | parses and returns next marker-id                                |
 |_________|__________________________________________________________________|
*/
static UINT mar_get (PJDEC_INST g)
{
    UINT   marker = 0;   /* init to eliminate a compiler warning */
    BOOL   got_ff;

    got_ff = FALSE;

    while (TRUE) {
        marker = read_byte (g);
        if (marker == 0xff) got_ff = TRUE;
        else                break;
    }

    if (!got_ff || marker==0)
        longjmp (g->syntax_error, BAD_MARKER_ID);

    return marker;
}



/*____________________________________________________________________________
 |           |                                                                |
 | mar_flush | discards data associated with current marker                   |
 |___________|________________________________________________________________|
*/
static void mar_flush (PJDEC_INST g, UINT marker)
{
    if (marker==MARKER_SOI || marker==MARKER_EOI ||
        (marker>=MARKER_RST0 && marker<=MARKER_RST7))
        return;   /* marker has no associated segment */

    read_skip_forward (g, read_uint(g) - 2);
}



/*____________________________________________________________________________
 |                |                                                           |
 | parse_app_jfif | parses a JFIF APP0 marker                                 |
 |________________|___________________________________________________________|
 |                                                                            |
 | Sets these fields in 'traits': lHorizDPI, lVertDPI                         |
 |____________________________________________________________________________|
*/
static void parse_app_jfif (PJDEC_INST g)
{
    UINT  len;
    BYTE  byt;
    UINT  h_dpi, v_dpi;

    len = read_uint (g);
    byt = read_byte (g);
    if (! (len==16 && (byt==(BYTE )'J' || byt==(BYTE )'j'))) {
        /* This is not a JFIF APP-marker, so silently discard it */
        read_skip_forward (g, len-3);
        return;
    }

    read_skip_forward (g,6);  /* discard "FIF\0" + 0x01 + 0x00 */
    byt = read_byte (g);
    h_dpi = read_uint (g);
    v_dpi = read_uint (g);
    read_skip_forward (g,2);  /* discard thumbnail X and Y */

    if (byt == 1) {   /* 1 means that units are dots/inch */
        g->traits.lHorizDPI = (DWORD)h_dpi << 16;
        g->traits.lVertDPI  = (DWORD)v_dpi << 16;
    }
}



/*____________________________________________________________________________
 |                 |                                                          |
 | parse_app_g3fax | parses a color fax APP1 marker                           |
 |_________________|__________________________________________________________|
 |                                                                            |
 | Can set these fields in 'traits': lHorizDPI, lVertDPI                      |
 |____________________________________________________________________________|
 */
static void parse_app_g3fax (PJDEC_INST g)
{
    int  i;
    UINT len, dpi;
    BYTE id_ver[8];
    const BYTE valid_id_ver[8] = {
        0x47, 0x33, 0x46, 0x41, 0x58, 0x00,  /* "G3FAX" plus a 0 byte */
        0x07, 0xca };                        /* version is year of std, 1994 */

    len = read_uint (g);
    if (len != 12) {
        /* wrong version, or a gamut or illuminant spec which we ignore */
        read_skip_forward (g, len-2);
        return;
    }

    for (i=0; i<8; i++)   /* fax id is 6 bytes; version is 2 bytes */
        id_ver[i] = read_byte (g);

    dpi = read_uint (g);

    if (memcmp(id_ver,valid_id_ver,8) == 0
        && (dpi==200 || dpi==300 || dpi==400)) {
        /* everything is valid (whew!), so save dpi */
       g->traits.lHorizDPI = g->traits.lVertDPI = (long)dpi << 16;
       g->fColorFax = TRUE;
    }
}



/*____________________________________________________________________________
 |                        |                                                   |
 | parse_app_short_header | An APP1 marker output by OfficeJet firmware       |
 |________________________|___________________________________________________|
*/
static void parse_app_short_header (PJDEC_INST g)
{
    /* Since the firmware does not supply tables in its header,
     * the tables used in the firmware are supplied below.  */

    static const BYTE orig_lum_quant[64] = {
         16,  11,  12,  14,  12,  10,  16,  14,
         13,  14,  18,  17,  16,  19,  24,  40,
         26,  24,  22,  22,  24,  49,  35,  37,
         29,  40,  58,  51,  61,  60,  57,  51,
         56,  55,  64,  72,  92,  78,  64,  68,
         87,  69,  55,  56,  80, 109,  81,  87,
         95,  98, 103, 104, 103,  62,  77, 113,
        121, 112, 100, 120,  92, 101, 103,  99
    };


    static const BYTE orig_chrom_quant[64] = {
        17,  18,  18,  24,  21,  24,  47,  26,
        26,  47,  99,  66,  56,  66,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99,
        99,  99,  99,  99,  99,  99,  99,  99
    };

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

    UINT len;
    UINT dc_q_fac, ac_q_fac;
    UINT reserved;

    /***** default items not in the short header *****/

    g->restart_interval = 0;

    g->which_quant_tbl[0] = 0;
    g->which_quant_tbl[1] = 1;
    g->which_quant_tbl[2] = 1;

    g->which_dc_tbl[0] = 0;
    g->which_ac_tbl[0] = 0;
    g->which_dc_tbl[1] = 1;
    g->which_ac_tbl[1] = 1;
    g->which_dc_tbl[2] = 1;
    g->which_ac_tbl[2] = 1;

    huff_define_table (g, FALSE, 0, lum_DC_counts  , lum_DC_values);
    huff_define_table (g, TRUE , 0, g->fDenali     ? lum_AC_counts_Denali
                                                   : lum_AC_counts, lum_AC_values);
    huff_define_table (g, FALSE, 1, chrom_DC_counts, chrom_DC_values);
    huff_define_table (g, TRUE , 1, chrom_AC_counts, chrom_AC_values);

    /***** parse the short header *****/

    len = read_uint (g);
    if (len != 18)
        longjmp (g->syntax_error, BAD_MARKER_DATA);

    g->traits.lNumRows      = read_uint (g);
    g->traits.iPixelsPerRow = read_uint (g);
    g->traits.lHorizDPI     = (long)read_uint(g) << 16;
    g->traits.lVertDPI      = (long)read_uint(g) << 16;
    ac_q_fac                = read_byte (g);
    g->num_comps            = read_byte (g);
    g->max_horiz_samp_fac   = parse_factors (read_uint(g), g->horiz_samp_facs);
    g->max_vert_samp_fac    = parse_factors (read_uint(g), g->vert_samp_facs);
    dc_q_fac                = read_byte (g);
    reserved                = read_byte (g);   /* should be 0 */

    g->traits.iComponentsPerPixel = g->num_comps;
    g->traits.iBitsPerPixel       = g->num_comps * 8;
    if (g->traits.lNumRows == 0)
        g->traits.lNumRows = -1;   /* -1 means 'unknown' */
    if (dc_q_fac == 0)
        dc_q_fac = ac_q_fac;

    calc_quant_table (g, dc_q_fac, ac_q_fac, orig_lum_quant, 0);
    calc_quant_table (g, dc_q_fac, ac_q_fac, orig_chrom_quant, 1);

    g->got_short_header = TRUE;
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_app | application-specific marker                                |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Can set these fields in 'traits': lHorizDPI, lVertDPI                      |
 |____________________________________________________________________________|
 */
static void mar_parse_app (PJDEC_INST g, UINT marker)
{
    UINT len;
    BYTE id1, id2, id3;

    len = read_uint (g);
    if (len <= 5) {
        /* unknown marker; discard it */
        read_skip_forward (g, len-2);
        return;
    }

    id1 = read_byte (g);
    id2 = read_byte (g);
    id3 = read_byte (g);
    read_skip_backward (g, 5);

    if (marker==MARKER_APP+1 && id1==0x47 && id2==0x33 && id3==0x46) {
        /* G3 color fax APP1 marker */
        parse_app_g3fax (g);
    } else if (marker==MARKER_APP+0 && (id1=='J' || id1=='j')
               && (id2=='F' || id2=='f') && (id3=='I' || id3=='i')) {
        /* JFIF APP0 marker for generic JPEG files */
        parse_app_jfif (g);
    } else if (marker==MARKER_APP+1 && len==18) {
        /* assume that APP1 marker is a short header */
        parse_app_short_header (g);
    } else {
        /* unrecognized APP marker; discard it */
        read_skip_forward (g, len);
    }
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_sof | start of frame                                             |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Sets these fields in 'traits':  lNumRows, iPixelsPerRow, iBitsPerPixel,    |
 |                                 iComponentsPerPixel.                       |
 |                                                                            |
 | Sets these variables:  num_comps (equal to iComponentsPerPixel),           |
 |    horiz_samp_facs, vert_samp_facs, which_quant_tbl,                       |
 |    max_horiz_samp_fac, max_vert_samp_fac                                   |
 |____________________________________________________________________________|
*/
static void mar_parse_sof (PJDEC_INST g, UINT marker)
{
    UINT len;
    UINT uBitsPerComp;
    UINT comp;
    BYTE comp_id;
    BYTE hv_samp;
    BYTE q_table;
    BYTE h, v;

    len                                          = read_uint (g);
    uBitsPerComp                                 = read_byte (g);
    g->rowCountOffset=g->rd_inbuf_next-g->rd_inbuf_beg;
    g->traits.lNumRows                           = read_uint (g);
    g->traits.iPixelsPerRow                      = read_uint (g);
    g->traits.iComponentsPerPixel = g->num_comps = read_byte (g);
    g->traits.iBitsPerPixel       = g->num_comps * uBitsPerComp;

    if (g->traits.lNumRows == 0)
        g->traits.lNumRows = -1;   /* -1 means 'unknown' */

    if (len != (8u + 3u*g->num_comps) || g->num_comps==0)
        longjmp (g->syntax_error, BAD_MARKER_DATA);

    /* Below, we also check for SOF1 because Chromafax erroneously outputs it */
    if ((marker!=MARKER_SOF0 && marker!=MARKER_SOF0+1)
        || uBitsPerComp!=8 || g->num_comps>4)
        longjmp (g->syntax_error, NOT_IMPLEMENTED);

    g->max_horiz_samp_fac = 1;
    g->max_vert_samp_fac  = 1;

    #if DUMP_JPEG
        _ftprintf (stderr, _T("\nSOF marker:\n"));
        _ftprintf (stderr, _T("   bits per sample = %d\n"), uBitsPerComp);
        _ftprintf (stderr, _T("   number of rows  = %d\n"), g->traits.lNumRows);
        _ftprintf (stderr, _T("   pixels per row  = %d\n"), g->traits.iPixelsPerRow);
        _ftprintf (stderr, _T("   # components    = %d\n"), g->num_comps);
    #endif

    for (comp=0; comp<g->num_comps; comp++) {
        comp_id = read_byte (g);
        hv_samp = read_byte (g);
        q_table = read_byte (g);

        #if DUMP_JPEG
            _ftprintf (stderr,
                     _T("    %d: comp id = %d, hv samp fac = %02x, which q = %d\n"),
                     comp, comp_id, hv_samp, q_table);
        #endif

        g->horiz_samp_facs[comp] = h = hv_samp >> 4;
        g->vert_samp_facs [comp] = v = hv_samp & 0x0fu;
        g->which_quant_tbl[comp] = q_table;

        if (h > g->max_horiz_samp_fac) g->max_horiz_samp_fac = h;
        if (v > g->max_vert_samp_fac ) g->max_vert_samp_fac  = v;
    }
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_dqt | discrete quantization table                                |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Sets the following variables:  quant_tbls.                                 |
 |____________________________________________________________________________|
*/
static void mar_parse_dqt (PJDEC_INST g)
{
    int   len, i;
    BYTE  pt;
    long *tbl_p;

    len = read_uint(g) - 2;

    while (len >= 65) {
        len -= 65;
        pt = read_byte (g);
        if ((pt & 0xfcu) != 0)
            longjmp (g->syntax_error, NOT_IMPLEMENTED);
        tbl_p = g->quant_tbls[pt & 3];
        DUMP (_T("\nDQT marker:  table=%d"), pt & 3, 0, 0);
        for (i=0; i<64; i++) {
            if ((i & 15) == 0) DUMP(_T("\n    "), 0,0,0);
            tbl_p[i] = read_byte (g);
            DUMP (_T("%2d "), tbl_p[i], 0, 0);
        }
        DUMP (_T("\n"), 0,0,0);
        wino_scale_table (tbl_p);
    }

    if (len != 0)
        longjmp (g->syntax_error, BAD_MARKER_DATA);
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_dht | define Huffman tables                                      |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Sets the following variables:  dc_tbls, ac_tbls.                           |
 |____________________________________________________________________________|
*/
static void mar_parse_dht (PJDEC_INST g)
{
    BYTE num_codes[16];
    BYTE values[256];

    int  len, i, tot_codes;
    BYTE class_id;

    len = read_uint(g) - 2;

    while (len > 17) {
        class_id = read_byte (g);

        for (tot_codes=0, i=0; i<=15; i++) {
            num_codes[i] = read_byte (g);
            tot_codes += num_codes[i];
        }

        len -= 17;
        if (len < tot_codes)
            longjmp (g->syntax_error, BAD_MARKER_DATA);

        for (i=0; i<tot_codes; i++)
            values[i] = read_byte (g);
        len -= tot_codes;

        #if DUMP_JPEG
            _ftprintf (stderr, _T("\nDHT marker:  class=%d, id=%d\n   counts = "),
                     (BOOL  )(class_id>>4), class_id & 0x0f);
            for (i=0; i<16; i++)
                _ftprintf (stderr, _T("%2d "), num_codes[i]);
            _ftprintf (stderr, _T("\n   values = "));
            for (i=0; i<tot_codes; i++)
                _ftprintf (stderr, _T("%02x "), values[i]);
            _ftprintf (stderr, _T("\n"));
        #endif

        huff_define_table (g, (BOOL)(class_id>>4), class_id & 0x0f,
                           num_codes, values);
    }

    if (len != 0)
        longjmp (g->syntax_error, BAD_MARKER_DATA);
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_dri | define restart interval                                    |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Sets the following variable:  restart_interval.                            |
 |____________________________________________________________________________|
*/
static void mar_parse_dri (PJDEC_INST g)
{
    UINT len;

    len = read_uint (g);
    if (len != 4)
        longjmp (g->syntax_error, BAD_MARKER_DATA);

    g->restart_interval = read_uint (g);
    DUMP (_T("\nDRI marker:  restart interval = %d\n"), g->restart_interval, 0,0);
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_sos | start of scan                                              |
 |_______________|____________________________________________________________|
*/
static void mar_parse_sos (PJDEC_INST g)
{
    UINT len;
    UINT comp;
    UINT cs;
    UINT dc_ac;

    len = read_uint (g);
    if (len != 6u+2u*g->num_comps)
        longjmp (g->syntax_error, BAD_MARKER_DATA);

    read_byte (g);   /* skip Ns value (number of components in scan) */

    DUMP (_T("\nSOS marker:\n"), 0,0,0);

    for (comp=0; comp<g->num_comps; comp++) {
        cs = read_byte (g);    /* skip Cs value (component selector) */
        dc_ac = read_byte (g);
        DUMP (_T("   %d: cs = %d, which dc_ac tbl = %02x\n"), comp, cs, dc_ac);
        g->which_dc_tbl[comp] = dc_ac >> 4;
        g->which_ac_tbl[comp] = dc_ac & 0x0fu;
    }

    read_byte (g);   /* skip Ss value (start selection) */
    read_byte (g);   /* skip Se value (end selection) */
    read_byte (g);   /* skip AhAl value (approximation bit positions) */
}



/*____________________________________________________________________________
 |               |                                                            |
 | mar_parse_dnl | Define Number of Lines                                     |
 |_______________|____________________________________________________________|
 |                                                                            |
 | Sets the following variable:  traits.lNumRows                              |
 |____________________________________________________________________________|
*/
static void mar_parse_dnl (PJDEC_INST g)
{
    UINT len, nRows;

    len   = read_uint (g);
    nRows = read_uint (g);

    if (len!=4u || nRows==0)
        longjmp (g->syntax_error, BAD_MARKER_DATA);

    DUMP (_T("\nDNL marker: %d\n"), nRows,0,0);
    g->traits.lNumRows = nRows;
}



/*____________________________________________________________________________
 |           |                                                                |
 | mar_parse | parses the marker, storing results in global variables         |
 |___________|________________________________________________________________|
*/
static void mar_parse (PJDEC_INST g, UINT marker)
{
    PRINT (_T("mar_parse: marker = %02xh\n"), marker, 0);

    switch (marker)
    {
        case MARKER_APP+0:
        case MARKER_APP+1:
        case MARKER_APP+2:
        case MARKER_APP+3:
        case MARKER_APP+4:
        case MARKER_APP+5:
        case MARKER_APP+6:
        case MARKER_APP+7:
        case MARKER_APP+8:
        case MARKER_APP+9:
        case MARKER_APP+10:
        case MARKER_APP+11:
        case MARKER_APP+12:
        case MARKER_APP+13:
        case MARKER_APP+14:
        case MARKER_APP+15:
            mar_parse_app (g, marker);
            break;

        case MARKER_COM:  /* comment marker */
        case MARKER_JPG+0:
        case MARKER_JPG+1:
        case MARKER_JPG+2:
        case MARKER_JPG+3:
        case MARKER_JPG+4:
        case MARKER_JPG+5:
        case MARKER_JPG+6:
        case MARKER_JPG+7:
        case MARKER_JPG+8:
        case MARKER_JPG+9:
        case MARKER_JPG+10:
        case MARKER_JPG+11:
        case MARKER_JPG+12:
        case MARKER_JPG+13:
            mar_flush (g, marker);
            break;

        case MARKER_SOF0:
        case MARKER_SOF1:
        case MARKER_SOF2:
        case MARKER_SOF3:
        case MARKER_SOF5:
        case MARKER_SOF6:
        case MARKER_SOF7:
        case MARKER_SOF8:
        case MARKER_SOF9:
        case MARKER_SOFA:
        case MARKER_SOFB:
        case MARKER_SOFD:
        case MARKER_SOFE:
        case MARKER_SOFF:
            mar_parse_sof (g, marker);
            break;

        case MARKER_RST0:
        case MARKER_RST1:
        case MARKER_RST2:
        case MARKER_RST3:
        case MARKER_RST4:
        case MARKER_RST5:
        case MARKER_RST6:
        case MARKER_RST7:
            DUMP (_T("\nRST marker.\n"), 0,0,0);
            break;

        case MARKER_DHT:  mar_parse_dht (g);   break;
        case MARKER_DQT:  mar_parse_dqt (g);   break;
        case MARKER_DRI:  mar_parse_dri (g);   break;
        case MARKER_SOS:  mar_parse_sos (g);   break;
        case MARKER_DNL:  mar_parse_dnl (g);   break;

        case MARKER_DAC:
        case MARKER_DHP:
        case MARKER_EXP:
            longjmp (g->syntax_error, NOT_IMPLEMENTED);
            break;

        /* The following markers have no data following them */

        case MARKER_SOI:
            DUMP (_T("\nSOI marker.\n"), 0,0,0);
            break;

        case MARKER_EOI:
            DUMP (_T("\nEOI marker.\n"), 0,0,0);
            break;

        default:
            longjmp (g->syntax_error, BAD_MARKER_DATA);
    }
}



/******************************************************************************
 ******************************************************************************

                               H U F F M A N


 Interface into this section:

     huff_init         - inits this section
     huff_free_tbl     - deallocates memory for one table
     huff_free_all     - deallocates memory for all tables
     huff_define_table - defines a new Huffman table
     DECODE_HUFF       - decodes and returns next Huffman code

 ******************************************************************************
 ******************************************************************************/



/*____________________________________________________________________________
 |             |                                                              |
 | DECODE_HUFF | Decodes a Huffman code, returning corresponding value        |
 |_____________|______________________________________________________________|
*/
#define DECODE_HUFF(                                                    \
    g,                                                                  \
    huff_tbl_p,                                                         \
    main_ix_len,                                                        \
    par_result,                                                         \
    hit_marker)                                                         \
{                                                                       \
    UINT tbl_index, code, size;                                         \
    main_huff_elem_t *elem;                                             \
                                                                        \
    READ_BITS_LOAD (g, FALSE, main_ix_len, code, hit_marker)            \
    tbl_index = huff_tbl_p->index_p[code];                              \
    elem = &(huff_tbl_p->main_p[tbl_index]);                            \
    par_result = elem->value;                                           \
    size = elem->size;                                                  \
                                                                        \
    if (size == 0) {                                                    \
        par_result = parse_aux_code (g, huff_tbl_p->aux_p);             \
    } else {                                                            \
        DUMP (_T("    %2d-%04x-%3d(main)  "), size, code, par_result);      \
        READ_BITS_ADVANCE (g, size)                                     \
    }                                                                   \
}



/*____________________________________________________________________________
 |                |                                                           |
 | parse_aux_code | Code is not in short-width table; look in aux table       |
 |________________|___________________________________________________________|
 |                                                                            |
 | Returns the value associated with the code.                                |
 |____________________________________________________________________________|
*/
static UINT parse_aux_code (
    PJDEC_INST       g,
    aux_huff_elem_t *aux_tbl_par_p)
{
    UINT code, size, val;
    UINT diff;
    UINT excess;
    aux_huff_elem_t *lo_p, *hi_p, *mid_p;

    READ_BITS_LOAD (g, FALSE, 16, code, syntax_err)

#if 0   /* we are no longer using ROM tables */
    if ((BYTE *)aux_tbl_par_p == dec_AC_aux_tbl)
    {
        /* We are using our default ROM table */
        val = dec_AC_aux_tbl [code & 0x7f];
        DUMP (_T("    %2d-%4x-%3d(aux-rom)  "), 16, code, val);
        if (val==0 || (code & 0xff80)!=0xff80) goto syntax_err;
        READ_BITS_ADVANCE (g,16)
    }
#endif

    lo_p = aux_tbl_par_p;
    hi_p = lo_p + lo_p->size - 1;
    lo_p += 1;  /* 1st table-entry is a dummy containing above table-size */

    while ((diff=(hi_p-lo_p)) > 1) {
        mid_p = lo_p + (diff>>1);
        if (mid_p->code > code) hi_p = mid_p;
        else                    lo_p = mid_p;
    }

    size = lo_p->size;
    excess = 16u - size;
    if ((code>>excess) != (UINT)(lo_p->code>>excess)) {
        lo_p = hi_p;
        size = lo_p->size;
        excess = 16u - size;
        if ((code>>excess) != (UINT)(lo_p->code>>excess)) {
            PRINT (_T("aux code of %x not found\n"), code, 0);
            goto syntax_err;
        }
    }

    val = lo_p->value;
    READ_BITS_ADVANCE (g,size)
    DUMP (_T("    %2d-%4x-%3d(aux)  "), size, code, val);

    return val;

    syntax_err:
        PRINT (_T("parse_aux_code: syntax error\n"), 0, 0);
        longjmp (g->syntax_error, BAD_HUFF_CODE);
}



/*____________________________________________________________________________
 |           |                                                                |
 | huff_init | Inits this section                                             |
 |___________|________________________________________________________________|
*/
static void huff_init (PJDEC_INST g)
{
    memset (g->dc_tbls, 0, sizeof(g->dc_tbls));
    memset (g->ac_tbls, 0, sizeof(g->ac_tbls));
}



/*____________________________________________________________________________
 |               |                                                            |
 | huff_free_tbl | Frees memory allocated for the given table                 |
 |_______________|____________________________________________________________|
*/
static void huff_free_tbl (PJDEC_INST g, huff_tbl_t *tbl_p)
{
    if (tbl_p->index_p != NULL)
        IP_MEM_FREE (tbl_p->index_p);
    if (tbl_p->main_p != NULL)
        IP_MEM_FREE (tbl_p->main_p);
    if (tbl_p->aux_p != NULL)
        IP_MEM_FREE (tbl_p->aux_p);

    tbl_p->index_p = NULL;
    tbl_p->main_p  = NULL;
    tbl_p->aux_p   = NULL;
}



/*____________________________________________________________________________
 |               |                                                            |
 | huff_free_all | Frees all memory allocated for Huffman tables              |
 |_______________|____________________________________________________________|
*/
static void huff_free_all (PJDEC_INST g)
{
    int i;

    for (i=0; i<MAX_HUFF_TBLS; i++) {
        huff_free_tbl (g, &(g->dc_tbls[i]));
        huff_free_tbl (g, &(g->ac_tbls[i]));
    }
}



/*____________________________________________________________________________
 |            |                                                               |
 | calc_table | Defines a Huffman table                                       |
 |____________|_______________________________________________________________|
*/
static void calc_table (
    const BYTE   counts[16],    /* in:  # Huffman codes of each length 1-16 */
    const BYTE   huffval[],     /* in:  values for codes of above lengths */
    UINT         main_ix_len,   /* in:  # bits in main tbl index (0=use max) */
    huff_tbl_t  *huff_tbl_p)    /* out: the three tables */
{
    BYTE              huffsize[257];
    WORD              huffcode[257];
    int               tot_codes;
    int               i;
    BYTE             *index_p;
    main_huff_elem_t *main_p;
    aux_huff_elem_t  *aux_p;

       /***************************************************/
      /* Compute a complete Huffman table.               */
     /* output:  huffval, huffsize, huffcode, tot_codes */
    /***************************************************/

    {
        int i, j, k, code, siz;

        /* Generate size array -- see JPEG document
         *
         * Note that list BITS in JPEG doc. has index from 1-16, but this list
         * is called 'counts', indexed 0-15.  This thus BITS[i] is replaced
         * by counts[i-1]
         */
        tot_codes = 0;
        for (i=1; i<=16; i++)
            for (j=1; j<=counts[i-1]; j++)
                huffsize[tot_codes++] = i;

        huffsize[tot_codes] = 0;

        #if 0   /* we now only use RAM tables */
        if (memcmp(counts, rom_counts, 16) == 0  &&
            memcmp(huffval, rom_val, tot_codes) == 0) {
           PRINT (_T("calc_table: using ROM Huffman tables\n"), 0, 0);
           return FALSE;   /* tell caller to use ROM-tables instead */
        }
        #endif

        /* Generate code array -- see JPEG document
         *
         * The resulting table is sorted by increasing 'code', and also by
         * increasing 'size'.
         */
        k = 0;
        code = 0;
        siz = huffsize[0];
        while (TRUE) {
            do {
                huffcode[k++] = code++;
            } while (huffsize[k]==siz && k<257);   /* Overflow Detection */
            if (huffsize[k] == 0)
                break;   /* all done */
            do {              /* Shift next code to expand prefix */
                code <<= 1;
                siz += 1;
            } while (huffsize[k] != siz);
        }
    }

      /****************************************/
     /* Make the main table (output: main_p) */
    /****************************************/

    {   /* This "main" table is indexed by the output of the "table table",
         * which is indexed by 'main_ix_len' bits of input.
         * If the table-entry has a 'size' of 0, the aux table is examined.
         */
        int  i, nbytes;
        int  extra_bits;
        UINT first, final, code_plus_junk;

        if (main_ix_len == 0)
            main_ix_len = huffsize[tot_codes-1];

        nbytes = (tot_codes+1) * sizeof(main_huff_elem_t);
        IP_MEM_ALLOC (nbytes, main_p);
        memset (main_p, 0, nbytes);

        nbytes = 1lu << main_ix_len;
        IP_MEM_ALLOC (nbytes, index_p);
        memset (index_p, 0, nbytes);

        for (i=0; i<tot_codes && huffsize[i]<=main_ix_len; i++) {
            main_p[i+1].value = huffval [i];
            main_p[i+1].size  = huffsize[i] ;

            extra_bits = main_ix_len - huffsize[i];
            first = huffcode[i] << extra_bits;
            final = first + (1lu << extra_bits) - 1;
            for (code_plus_junk = first;
                 code_plus_junk <= final;
                 code_plus_junk++)
                index_p[code_plus_junk] = i+1;
        }
    }

      /********************************************/
     /* Make the auxiliary table (output: aux_p) */
    /********************************************/

    {   /* This is used when an entry in the main table was 0, meaning that
         * the code is longer than 'main_ix_len'.  The aux table consists of
         * all [code, size, value] triples for sizes > main_ix_len.  A binary
         * search is used to locate the code.
         * The first table-entry is a dummy whose 'size' field is the number
         * of table-entries (including the dummy).
         */
        int first, n_entries;
        aux_huff_elem_t *p;

        /* locate first huffsize > main_ix_len */
        for (first=0; first<tot_codes && huffsize[first]<=main_ix_len; first++);

        if (first == tot_codes) {
            /* the main table captured everything; no aux table is needed */
            IP_MEM_ALLOC (1, aux_p);
        } else {
            n_entries = tot_codes - first + 1;  /* +1 because of dummy entry */
            IP_MEM_ALLOC (n_entries * sizeof(aux_huff_elem_t), aux_p);

            /* fill-in the dummy entry (contains # entries in table) */
            p = aux_p;
            p->size = (UINT) n_entries;
            p->code = p->value = 0;
            p += 1;

            for (i=first; i<tot_codes; i++) {
                p->size  = huffsize[i];
                p->code  = huffcode[i] << (16u - p->size);
                p->value = huffval [i];
                p += 1;
            }
        }
    }

    #if DUMP_JPEG
    {
        int i, n;

        _ftprintf (stderr, _T("\nOriginal size-code-val tuples:\n"));
        for (i=0; i<tot_codes; i++) {
            if ((i&7) == 0) _ftprintf(stderr, _T("   "));
            _ftprintf (stderr, _T("%2d-%04x-%02x "),
                huffsize[i], huffcode[i], huffval[i]);
            if ((i&7)==7 || i==tot_codes-1) _ftprintf(stderr, _T("\n"));
        }

        _ftprintf(stderr, _T("\nIndex table: (%d entries)\n"), 1lu<<main_ix_len);
        for (i=0; i<(1lu<<main_ix_len); i++) {
            if ((i&7) == 0) _ftprintf(stderr, _T("   %3d: "), i);
            _ftprintf (stderr, _T("%3d, "), index_p[i]);
            if ((i&7) == 7) _ftprintf(stderr, _T("\n"));
        }

        _ftprintf(stderr, _T("\nMain table: (%d entries)\n"), tot_codes+1);
        for (i=0; i<tot_codes; i++) {
            if ((i&7) == 0) _ftprintf(stderr, _T("   %3d: "), i);
            _ftprintf (stderr, _T("%04x(%2d) "), main_p[i].value, main_p[i].size);
            if ((i&7)==7 || i==tot_codes-1) _ftprintf(stderr, _T("\n"));
        }

        if (huffsize[tot_codes-1] > main_ix_len) {
            n = aux_p[0].size;
            _ftprintf(stderr, _T("\nAux table: (%d entries of size-code-value)\n"),n);
            for (i=0; i<n; i++) {
                if ((i&3) == 0) _ftprintf(stderr, _T("   %3d: "), i);
                _ftprintf (stderr, _T("%2d-%4x-%3d  "),
                        aux_p[i].size, aux_p[i].code, aux_p[i].value);
                if ((i&3)==3 || i==n-1) _ftprintf(stderr, _T("\n"));
            }
        }
    }
    #endif

    huff_tbl_p->index_p = index_p;
    huff_tbl_p->main_p  = main_p;
    huff_tbl_p->aux_p   = aux_p;
    return;

    fatal_error:
        assert (0);   /* todo: eliminate this assert */
}



/*____________________________________________________________________________
 |                   |                                                        |
 | huff_define_table | Defines the given Huffman table                        |
 |___________________|________________________________________________________|
 |                                                                            |
 | Sets the following variables:  dc_tbls, ac_tbls.                           |
 |____________________________________________________________________________|
*/
static void huff_define_table (
    PJDEC_INST g,
    BOOL       ac,         /* defining an AC table? (else DC) */
    UINT       id,         /* which table is being defined (0-3) */
    const BYTE counts[16], /* number of Huffman codes of each length 1-16 */
    const BYTE values[])   /* values associated with codes of above lengths */
{
    huff_tbl_t *tbl_p;

    tbl_p = ac ? &(g->ac_tbls[id]) : &(g->dc_tbls[id]);
    huff_free_tbl (g,tbl_p);

    DUMP (_T("\nhuff_define_table: ac=%d, id=%d\n"), ac, id, 0);
    calc_table (counts, values,
                ac ? AC_TBL_INDEX_LEN : DC_TBL_INDEX_LEN,
                tbl_p);
}



/******************************************************************************
 ******************************************************************************

                              W I N O G R A D


 Interface into this section:

     QNORM_TO_INPUT   - adjusts precision of scaled DCT value for wino-input
     INPUT_PRECISION  - scales result of wino_inverse_dct back to pixels
     wino_scale_table - scale a quantization table into a Winograd table
     wino_inverse_dct - computes inverse DCT using Winograd's algorithm

 ******************************************************************************
 ******************************************************************************/



#define SHIFT_TRUNC(mvalue,mshift)  ((mvalue) >> (mshift))

#define QNORM_TO_INPUT(mval)  SHIFT_TRUNC(mval, QNORM_PRECISION-INPUT_PRECISION)

#define QNORM_PRECISION 16  /* prec of q-table scaled by Wino norm constants */
#define INPUT_PRECISION  5  /* prec of input to inverse-DCT */
#define CONST_PRECISION  9  /* prec of b1-b5 below */

#define b1 724L
#define b2 1338L
#define b3 724L
#define b4 554L
#define b5 392L

#if 0
#define CONST_PRECISION 15

#define b1 46341L
#define b2 85627L
#define b3 46341L
#define b4 35468L
#define b5 25080L
#endif



/*____________________________________________________________________________
 |                  |                                                         |
 | wino_scale_table | Scales a quantization table into a Winograd table       |
 |__________________|_________________________________________________________|
 |                                                                            |
 | Multiplies all the components of the Quantization matrix by the fractional |
 | normalization constants as required by the Winograd Transform.  The        |
 | results are fixed-point with QNORM_PRECISION bits of fraction.             |
 |____________________________________________________________________________|
*/

static const float inv_dct_norm[] = {
    0.125000f, 0.173380f, 0.173380f, 0.163320f,
    0.240485f, 0.163320f, 0.146984f, 0.226532f, 
    0.226532f, 0.146984f, 0.125000f, 0.203873f,
    0.213388f, 0.203873f, 0.125000f, 0.098212f, 
    0.173380f, 0.192044f, 0.192044f, 0.173380f,
    0.098212f, 0.067650f, 0.136224f, 0.163320f, 
    0.172835f, 0.163320f, 0.136224f, 0.067650f,
    0.034566f, 0.093833f, 0.128320f, 0.146984f, 
    0.146984f, 0.128320f, 0.093833f, 0.034566f,
    0.047944f, 0.088388f, 0.115485f, 0.125000f, 
    0.115485f, 0.088388f, 0.047944f, 0.045162f,
    0.079547f, 0.098212f, 0.098212f, 0.079547f, 
    0.045162f, 0.040645f, 0.067650f, 0.077165f,
    0.067650f, 0.040645f, 0.034566f, 0.053152f, 
    0.053152f, 0.034566f, 0.027158f, 0.036612f,
    0.027158f, 0.018707f, 0.018707f, 0.009558f
};

void wino_scale_table (
    long   *tbl_p)
{
    const float *fptr;
    int i;

    fptr = inv_dct_norm;
    for (i=0; i<64; i++) {
        *tbl_p = (long  ) ((*tbl_p) * (*fptr++) * (1l<<QNORM_PRECISION) + 0.5);
        tbl_p += 1;
    }
}



/******************************************************************************
 ******************************************************************************

                               D E C O D I N G


 Interface into this section:

     zero_prior_DC - sets the predicted DC values to zero
     decode_MCU    - decodes a single Minimum Coded Unit

 ******************************************************************************
 ******************************************************************************/



static BYTE const zigzag_index_tbl[] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
     0,  0,  0,  0,  0,  0,  0,  0,    /* extra 16 elements in case */
     0,  0,  0,  0,  0,  0,  0,  0     /* parse_block overruns */
};



/*____________________________________________________________________________
 |             |                                                              |
 | decode_init | inits this section                                           |
 |_____________|______________________________________________________________|
*/
static void decode_init (PJDEC_INST g)
{
    BYTE const *zig_p;
    int       **block_pp;

    zig_p = zigzag_index_tbl;
    for (block_pp=g->block_zz; block_pp<g->block_zz+(64+16); block_pp++)
        *block_pp = &(g->block[*zig_p++]);
}



/*____________________________________________________________________________
 |               |                                                            |
 | zero_prior_DC | zeroes the predicted DC values                             |
 |_______________|____________________________________________________________|
*/
static void zero_prior_DC (PJDEC_INST g)
{
    g->prior_dc[0] = g->prior_dc[1] = g->prior_dc[2] = g->prior_dc[3] = 0;
}



/*____________________________________________________________________________
 |             |                                                              |
 | parse_block | parses an 8x8 block; return data is ready for inverse DCT    |
 |_____________|______________________________________________________________|
 |                                                                            |
 | Return value:  TRUE  = we parsed a block,                                  |
 |                FALSE = hit a marker.                                       |
 | Output data is put in 'block' array.                                       |
 |____________________________________________________________________________|
*/
static BOOL parse_block (
    PJDEC_INST g,
    int        comp)    /* in: image component number */
{
    /* FIX_TERM - changes a term (fetched from JPEG file) into an integer.
     * The term is in ones-complement, with the sign-bit (and higher bits)
     * zeroed.  So the leftmost bit within the given size is the opposite of
     * the desired sign bit.  If that bit is 1, the number is positive, and
     * needs no change.  If it's 0, we set the higher bits to 1s, and add 1
     * to convert from ones-complement to twos-complement.
     * Select the macro below which is fastest on your computer.
     */

   #if 1
    #define FIX_TERM(msize,mterm) {                     \
        if ((mterm & (1u<<((msize)-1))) == 0)           \
            mterm = (mterm | (-1 << (msize))) + 1;      \
    }
   #endif

   #if 0
    #define FIX_TERM(msize,mterm) {                     \
        int mask = -1 << msize;                         \
        if (((mterm<<1) & mask) == 0)                   \
            mterm += mask + 1;                          \
    }
   #endif

   #if 0
    #define FIX_TERM(msize,mterm)                    \
        mterm += ((mterm>>(msize-1)) - 1) & ~((1<<msize) - 2);
   #endif

    huff_tbl_t  *huff_p;
    int        **block_p;
    long        *dequant_p;
    UINT         siz, run, rl_byte;
    int          dc, ac;

    /* memset (block, 0, 64*sizeof(int)); */
    {
        int *block_p, *after_p;

        for (block_p=g->block, after_p=g->block+64; block_p<after_p; ) {
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
            *block_p++ = 0;
        }
    }

    dequant_p = g->quant_tbls[g->which_quant_tbl[comp]];
    block_p = g->block_zz;

      /**************************************/
     /* Decode and dequantize the DC value */
    /**************************************/

    DUMP (_T("\nStart of block for component %d:\n"), comp, 0, 0);

    huff_p = &(g->dc_tbls[g->which_dc_tbl[comp]]);
    DECODE_HUFF (g, huff_p, DC_TBL_INDEX_LEN, siz, hit_marker)

    if (siz == 0) {
        DUMP (_T("dc=0, size of dc=0\n"), 0,0,0);
        dc = 0;
    } else {
        READ_BITS_LOAD (g, TRUE, siz, dc, syntax_err)
        READ_BITS_ADVANCE (g, siz)
        DUMP (_T("dc=%x, size of dc=%d\n"), dc, siz, 0);
        FIX_TERM (siz, dc)
    }

    dc += g->prior_dc[comp];
    g->prior_dc[comp] = dc;
    *(*block_p++) = QNORM_TO_INPUT ((long  )*dequant_p++ * dc);

      /*************************************************/
     /* Decode, dezigzag and dequantize the AC values */
    /*************************************************/

    huff_p = &(g->ac_tbls[g->which_ac_tbl[comp]]);

    /* The two 'hit_marker's below should be 'syntax_error' instead. But
     * Chromafax and other software erroneously uses fill 0's instead
     * of fill 1's, so we parse a few of those 0's before hitting the
     * marker.
     */

    while (TRUE) {
        DECODE_HUFF (g, huff_p, AC_TBL_INDEX_LEN, rl_byte, hit_marker);
        run = rl_byte >> 4;
        siz = rl_byte & 0x0f;
        DUMP (_T("rlc: run=%d, size of ac=%d"), run, siz, 0);

        if (siz == 0) {
            if (run == 15) {
                DUMP (_T(", run of 16\n"), 0,0,0);
                block_p   += 16;
                dequant_p += 16;
            } else if (run == 0) {
                DUMP (_T(", EOB\n"), 0,0,0);
                break;   /* hit EOB */
            } else
                goto syntax_err;
        } else {
            block_p   += run;
            dequant_p += run;
            READ_BITS_LOAD (g, TRUE, siz, ac, hit_marker);
            READ_BITS_ADVANCE (g, siz);
            DUMP (_T(", ac=%x\n"), ac, siz, 0);
            FIX_TERM (siz, ac);
            *(*block_p++) = QNORM_TO_INPUT ((long  )*dequant_p++ * ac);
        }

        if (block_p >= g->block_zz+64) {
            if (block_p > g->block_zz+64) {
                PRINT(_T("parse_block: over 63 AC terms\n"), 0,0);
                goto syntax_err;
            }
            if (! g->fDenali)
                break;   /* 63rd AC term was non-zero */
        }
    }

    return TRUE;

    syntax_err:
        PRINT(_T("parse_block: syntax error\n"), 0,0);
        longjmp (g->syntax_error, BAD_HUFF_CODE);

    hit_marker:
        return FALSE;

    #undef FIX_TERM
}



/*____________________________________________________________________________
 |                      |                                                     |
 | handleMarkerInStream | handles a marker in the data-stream                 |
 |______________________|_____________________________________________________|
 |                                                                            |
 | Parses EOI, DNL and RST; other markers cause an error (longjmp).           |
 | For an EOI, g->got_EOI is set to true.                                     |
 |____________________________________________________________________________|
*/
static void handleMarkerInStream (PJDEC_INST g)
{
    BYTE marker;

    marker = mar_get(g);

    if (marker == MARKER_EOI) {
        PRINT (_T("handleMarkerInStream: parsed EOI\n"), 0, 0);
        g->got_EOI = TRUE;
    } else if (marker == MARKER_DNL) {
        PRINT (_T("handleMarkerInStream: parsing DNL\n"), 0, 0);
        mar_parse_dnl (g);
    } else if (g->restart_interval > 0
        && g->restart_cur_mcu == g->restart_interval
        && (marker-MARKER_RST0) == g->restart_cur_marker)
    {
        /* it's a restart, and we expected it at this point in the data */
        PRINT (_T("handleMarkerInStream: parsed expected RST\n"), 0, 0);
        g->restart_cur_marker = (g->restart_cur_marker+1) & 7;
        g->restart_cur_mcu = 0;
        zero_prior_DC (g);
    } else {
        PRINT (_T("handleMarkerInStream: illegal marker=0x%2.2X\n"), marker, 0);
        longjmp (g->syntax_error, BAD_MARKER_DATA);
    }
}



/*____________________________________________________________________________
 |                    |                                                       |
 | LevelShiftAndRound | Level-shifts, rounds, and outputs pixels in 0..255    |
 |____________________|_______________________________________________________|
*/
static void LevelShiftAndRound (int *inBlock_p, BYTE *outBlock_p)
{
    BYTE *outAfter;
    int   pixel;

    for (outAfter = outBlock_p + 64;
         outBlock_p < outAfter;
         outBlock_p++, inBlock_p++)
    {
        pixel = (*inBlock_p +
                 ((1<<(INPUT_PRECISION-1)) + (128<<INPUT_PRECISION)))
                >> INPUT_PRECISION;
        if (pixel>>8 != 0)
            pixel = pixel>0 ? 255 : 0;  /* clamp to 0 or 255 */
        *outBlock_p = (BYTE) pixel;
    }
}



/*____________________________________________________________________________
 |            |                                                               |
 | decode_MCU | Parses a Minimum Coded Unit, and loads pixels into out_rows_ap|
 |____________|_______________________________________________________________|
 |                                                                            |
 | The pixels are loaded starting at mcus_done (not incremented).             |
 | This routine handles the restart interval logic, and markers.              |
 |                                                                            |
 | Returns TRUE if an MCU was parsed, else FALSE.                             |
 |____________________________________________________________________________|
*/
static BOOL decode_MCU (PJDEC_INST g)
{
    BYTE   baPixels[256];
    BYTE  *pPixel;    
    int    comp;
    int    h_block, v_block;
    int    ul_row, ul_col;
    int    row;
    BYTE **row_pp;
    BYTE  *row_p;

    for (comp=0; comp<g->num_comps; comp++) {
        for (v_block=0; v_block<g->vert_samp_facs[comp]; v_block++) {
            for (h_block=0; h_block<g->horiz_samp_facs[comp]; h_block++) {

                /***** parse and inverse-dct an 8x8 block *****/

                while (! parse_block(g,comp)) {
                    /* we hit a marker */
                    #if 0
                        /* The error-check below is commented-out to make us
                         * more forgiving of unexpected in-data markers, which
                         * we saw the Genoa color-fax test suite output at the
                         * end of the file.
                         */
                        if (comp>0 || v_block>0 || h_block>0)
                            longjmp (g->syntax_error, UNEXPECTED_MARKER);
                    #endif
                    handleMarkerInStream(g);
                    if (g->got_EOI)
                        return FALSE;  /* we're out of data; cannot proceed */
                }

                dct_inverse (g->block);

                /***** compute output pixels *****/

                LevelShiftAndRound (g->block, baPixels);

                /***** copy block into out_rows_ap *****/

                ul_row = v_block*8;
                ul_col = (g->mcus_done*g->horiz_samp_facs[comp] + h_block) * 8;
                row_pp = &(g->out_rows_ap[comp][ul_row]);
                pPixel = baPixels;

                for (row=0; row<8; row++) {
                    row_p = *row_pp++ + ul_col;
                    /* copy the 8 pixel row quickly, using two dword transfers */
                    *(DWORD*) row_p    = *(DWORD*) pPixel;
                    *(DWORD*)(row_p+4) = *(DWORD*)(pPixel+4);
                    pPixel += 8;
                }
            }
        }
    }

    if (g->restart_interval>0 && g->restart_cur_mcu==g->restart_interval)
        longjmp (g->syntax_error, NO_RESTART_MARKER);
    g->restart_cur_mcu += 1;

    return TRUE;
}



/*____________________________________________________________________________
 |               |                                                            |
 | copy_out_rows | copies a row (or rows) from out_rows_ap to output buffer   |
 |_______________|____________________________________________________________|
 |                                                                            |
 | If output_subsampled is true, we output row data in the same odd order     |
 | which the ASIC outputs it.                                                 |
 |                                                                            |
 | View a period as being a group of max_horiz_samp_fac by max_vert_samp_fac  |
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
 |     vert_period  = index of which period we're outputting vertically within|
 |                    an MCU (0-7).                                           |
 |                                                                            |
 |     horiz_period = index of which period we're outputting horizontally;    |
 |                    since an MCU has 8 periods, this is 0 .. 8*mcus_per_row.|
 |____________________________________________________________________________|
*/
static void copy_out_rows (
    PJDEC_INST  g,
    UINT        row_index,    /* in:  index of next row to send within MCU */
    BYTE       *outbuf_p,     /* out: copied-out row data */
    UINT       *n_rows_p,     /* out: # of rows copied out */
    UINT       *n_bytes_p)    /* out: # of bytes output */
{
    BYTE *out_p;
    UINT  comp;
    UINT  vert_period, vert_mod;
    UINT  row, col;
    BYTE *row_p;

    vert_period = row_index / g->max_vert_samp_fac;
    vert_mod    = row_index % g->max_vert_samp_fac;

    if (! g->output_subsampled) {

          /*******************************************/
         /* Perform duplication to undo subsampling */
        /*******************************************/

        UINT samp_fac;
        UINT horiz_mod;

        for (comp=0; comp<g->num_comps; comp++) {
            samp_fac = g->vert_samp_facs[comp];
            out_p = outbuf_p + comp;
            row = vert_period*samp_fac +
                    (vert_mod<samp_fac ? vert_mod : samp_fac-1);
            row_p = g->out_rows_ap[comp][row];

            horiz_mod = 0;
            samp_fac = g->horiz_samp_facs[comp];

            if (samp_fac == g->max_horiz_samp_fac) {

                /***** fast case: copying all pixels, with no duplication *****/

                if (g->num_comps == 1) {
                    memcpy (out_p, row_p, g->traits.iPixelsPerRow);
                } else {
                    for (col=0; col<(UINT)g->traits.iPixelsPerRow; col++) {
                        *out_p = *row_p++;
                        out_p += g->num_comps;
                    }
                }

            } else if (samp_fac==1 && g->max_horiz_samp_fac==2) {

                /***** fast case: duplicating every other pixel *****/

                BYTE  prev_pix;
                for (col=0; col<(UINT)g->traits.iPixelsPerRow; col+=2) {
                    *out_p = prev_pix = *row_p++;
                    out_p += g->num_comps;
                    *out_p = prev_pix;
                    out_p += g->num_comps;
                }

            } else {

                /***** slow general case *****/

                for (col=0; col<(UINT)g->traits.iPixelsPerRow; col++) {
                    *out_p = horiz_mod<samp_fac ? *row_p++ : row_p[-1];
                    out_p += g->num_comps;
                    horiz_mod += 1;
                    if (horiz_mod == g->max_horiz_samp_fac) horiz_mod = 0;
                }
            }
        }

        *n_rows_p = 1;
        *n_bytes_p = g->num_comps * g->traits.iPixelsPerRow;

    } else {

          /**********************************************/
         /* Output subsampled data in ASIC's odd order */
        /**********************************************/

        UINT horiz_period, periods_per_row;
        UINT ul_row, ul_col;
        BYTE *row_y1_p, *row_y2_p, *row_cb_p, *row_cr_p;

        out_p = outbuf_p;
        periods_per_row = g->mcus_per_row * 8;

        if (g->num_comps==1 && g->max_horiz_samp_fac==1 && g->max_vert_samp_fac==1) {

            /***** fast case: one component; just copy the row *****/

            memcpy (out_p, g->out_rows_ap[0][row_index], g->traits.iPixelsPerRow);
            out_p += g->traits.iPixelsPerRow;

        } else if (g->num_comps==3 &&
                   (g->horiz_samp_facs[0]==1 &&
                    g->horiz_samp_facs[1]==1 &&
                    g->horiz_samp_facs[2]==1) &&
                   (g->vert_samp_facs[0]==1 &&
                    g->vert_samp_facs[1]==1 &&
                    g->vert_samp_facs[2]==1)) {

            /***** fast case: color with no subsampling *****/

            row_y1_p = g->out_rows_ap[0][vert_period];
            row_cb_p = g->out_rows_ap[1][vert_period];
            row_cr_p = g->out_rows_ap[2][vert_period];
            for (col=0; col<(UINT)g->traits.iPixelsPerRow; col++) {
                *out_p++ = *row_y1_p++;
                *out_p++ = *row_cb_p++;
                *out_p++ = *row_cr_p++;
            }

        } else if (g->num_comps==3 &&
                   (g->horiz_samp_facs[0]==2 &&
                    g->horiz_samp_facs[1]==1 &&
                    g->horiz_samp_facs[2]==1) &&
                   (g->vert_samp_facs[0]==2 &&
                    g->vert_samp_facs[1]==1 &&
                    g->vert_samp_facs[2]==1)) {

            /***** fast case: 4-1-1 color subsampling *****/

            row_y1_p = g->out_rows_ap[0][2*vert_period];
            row_y2_p = g->out_rows_ap[0][2*vert_period+1];
            row_cb_p = g->out_rows_ap[1][vert_period];
            row_cr_p = g->out_rows_ap[2][vert_period];
            for (horiz_period=0; horiz_period<periods_per_row; horiz_period++) {
                *out_p++ = *row_y1_p++;
                *out_p++ = *row_y1_p++;
                *out_p++ = *row_y2_p++;
                *out_p++ = *row_y2_p++;
                *out_p++ = *row_cb_p++;
                *out_p++ = *row_cr_p++;
            }

        } else {

            /***** slow general case *****/

            for (horiz_period=0; horiz_period<periods_per_row; horiz_period++) {
                for (comp=0; comp<g->num_comps; comp++) {
                    ul_row =  vert_period *  g->vert_samp_facs[comp];
                    ul_col = horiz_period * g->horiz_samp_facs[comp];

                    for (row=ul_row; row<ul_row+ g->vert_samp_facs[comp]; row++)
                    for (col=ul_col; col<ul_col+g->horiz_samp_facs[comp]; col++) {
                        *out_p++ = g->out_rows_ap[comp][row][col];
                    }
                }
            }
        }

        *n_rows_p = g->max_vert_samp_fac;
        *n_bytes_p = out_p - outbuf_p;
    }
}




/******************************************************************************
 ******************************************************************************

                      E X P O R T E D   R O U T I N E S

 ******************************************************************************
 ******************************************************************************/



/*****************************************************************************\
 *
 * jpgDecode_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD jpgDecode_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PJDEC_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(JDEC_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(JDEC_INST));
    g->dwValidChk = CHECK_VALUE;
    decode_init (g);
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_setDefaultInputTraits - Specifies default input image traits
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

static WORD jpgDecode_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PJDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD jpgDecode_setXformSpec (
    IP_XFORM_HANDLE  hXform,         /* in: handle for xform */
    DWORD_OR_PVOID   aXformInfo[])   /* in: xform information */
{
    PJDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    g->output_subsampled = aXformInfo[IP_JPG_DECODE_OUTPUT_SUBSAMPLED].dword;
    g->fDenali           = aXformInfo[IP_JPG_DECODE_FROM_DENALI].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD jpgDecode_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)    /* out: buf size for parsing header */
{
    PJDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwInBufLen = MAX_HEADER_SIZE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD jpgDecode_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PJDEC_INST g;
    UINT  comp;
    UINT  marker;
    UINT  row_len, n_rows;
    UINT  row;
    UINT  err;
    BYTE *p;

      /**************/
     /* Misc. Init */
    /**************/

    HANDLE_TO_PTR (hXform, g);

    g->rows_done = 0;
    g->mcus_done = 0;
    g->sending_rows = FALSE;
    g->got_EOI = FALSE;
    g->got_short_header = FALSE;
    read_init (g);
    huff_init (g);
    zero_prior_DC (g);
    g->restart_cur_mcu    = 0;
    g->restart_cur_marker = 0;
    g->dwOutNextPos = 0;

      /********************/
     /* Parse the header */
    /********************/

    if ((err=setjmp(g->syntax_error)) != 0) {
        PRINT (_T("jpeg_decode_parse_header: syntax error = %d\n"), err, 0);
        return IP_FATAL_ERROR | IP_INPUT_ERROR;
    }

    read_buf_open (g, pbInputBuf);

    if (mar_get(g) != MARKER_SOI)
        return IP_FATAL_ERROR | IP_INPUT_ERROR;

    do {
        marker = mar_get (g);
        mar_parse (g, marker);
        if (marker == MARKER_EOI)
            return IP_FATAL_ERROR | IP_INPUT_ERROR;
    } while (!(marker==MARKER_SOS || g->got_short_header));

    *pdwInputNextPos = g->dwInNextPos = *pdwInputUsed = read_buf_close (g);

    /* todo: check that all markers arrived */

    PRINT (_T("jpeg_decode_parse_header: pixels/row=%d, num_rows=%d\n"),
           g->traits.iPixelsPerRow, g->traits.lNumRows);

    g->cols_per_mcu = g->max_horiz_samp_fac * 8;
    g->rows_per_mcu =  g->max_vert_samp_fac * 8;
    g->mcus_per_row = (g->traits.iPixelsPerRow + g->cols_per_mcu - 1) / g->cols_per_mcu;

      /*******************************************/
     /* Allocate the row-buffers in out_rows_ap */
    /*******************************************/

    memset (g->out_rows_ap, 0, sizeof(g->out_rows_ap));

    for (comp=0; comp<g->num_comps; comp++) {
        row_len =  g->horiz_samp_facs[comp] * g->mcus_per_row * 8;
        n_rows = g->vert_samp_facs[comp] * 8;

        for (row=0; row<n_rows; row++) {
            IP_MEM_ALLOC (row_len, p);
            g->out_rows_ap[comp][row] = p;
        }
    }

      /***************/
     /* Return info */
    /***************/

    *pInTraits  = g->traits;
    *pOutTraits = g->traits;

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
        return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * jpgDecode_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD jpgDecode_getActualBufSizes (
    IP_XFORM_HANDLE hXform,           /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,   /* out: min input buf size */
    PDWORD          pdwMinOutBufLen)  /* out: min output buf size */
{
    PJDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = INBUF_NUM_MCUS * MAX_MCU_SIZE;
    *pdwMinOutBufLen = g->num_comps * g->traits.iPixelsPerRow;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_convert - the work-horse routine
 *
\*****************************************************************************/

static WORD jpgDecode_convert (
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
    PJDEC_INST g;
    /* The "static" keyword for ret_val and bDecoded is to prevent the
     * "variable `' might be clobbered by `longjmp' or `vfork'" warning. */
    static unsigned   ret_val;
    UINT       comp, row, row_len, n_rows, n_bytes, trash;
    int        iErrorCode;
    static BOOL       bDecoded;
    const BYTE ycc_white[4] = { 255, 128, 128, 255 };

    HANDLE_TO_PTR (hXform, g);

    *pdwInputUsed     = 0;
    *pdwOutputThisPos = g->dwOutNextPos;
    *pdwOutputUsed    = 0;
    ret_val           = IP_READY_FOR_DATA;

    if (setjmp(g->syntax_error) != 0) {
        PRINT (_T("got a syntax error\n"), 0, 0);
        *pdwInputNextPos = g->dwInNextPos;
        return IP_FATAL_ERROR | IP_INPUT_ERROR;
    }

      /**********************/
     /* Parsing input data */
    /**********************/

    if (! g->sending_rows) {
        if (pbInputBuf == NULL) {

            /* We are being told to flush, so we should have consumed the EOI,
             * and have discarded any bytes following it.  */
            if (! g->got_EOI) {
                /* Unexpected end of data: we did not get an EOI */
                ret_val |= IP_INPUT_ERROR;
            }
            ret_val |= IP_DONE;

        } else if (g->got_EOI) {

            /* Discard bytes after the EOI. We should report an error here, but
             * the ASIC+firmware of Denali/Kodiak can send stuff after the EOI. */
            *pdwInputUsed = dwInputAvail;

        } else {

            if (g->mcus_done == 0) {
                /* init all row-buffers to white */
                PRINT (_T("initing all row-buffers to white\n"), 0, 0);
                for (comp=0; comp<g->num_comps; comp++) {
                    row_len = g->horiz_samp_facs[comp] * g->mcus_per_row * 8;
                    n_rows = g->vert_samp_facs[comp] * 8;

                    for (row=0; row<n_rows; row++)
                        memset (g->out_rows_ap[comp][row], ycc_white[comp], row_len);
                }
            }

            do {
                bDecoded = FALSE;
                read_buf_open (g, pbInputBuf + *pdwInputUsed);
                    memcpy(&(g->old_syntax_error),  &(g->syntax_error), sizeof(jmp_buf));
                    iErrorCode = setjmp(g->syntax_error);
                    if (iErrorCode == 0) {
                        bDecoded = decode_MCU(g);

                        /* Check for (and handle) a marker (DNL, RST, EOI).
                         * We need to handle DNL here so the sending_rows state below
                         * won't output the pad rows on the bottom of the image
                         */
                        if (! g->got_EOI) {
                            READ_BITS_LOAD (g, FALSE, 8, trash, at_a_marker);
                            goto no_marker;
                            at_a_marker:
                                handleMarkerInStream(g);
                            no_marker:;
                        }
                    }
                    memcpy(&(g->syntax_error),  &(g->old_syntax_error), sizeof(jmp_buf));
                n_bytes = read_buf_close (g);

                *pdwInputUsed  += n_bytes;
                g->dwInNextPos += n_bytes;

                if (*pdwInputUsed > dwInputAvail) {
                    /* Parser read past end of input buffer */
                    *pdwInputUsed = dwInputAvail;
                    g->dwInNextPos += dwInputAvail - n_bytes;
                    /* We will not make this a fatal error because that would
                     * immediately shut down the remaining xforms in the pipeline.
                     * Instead, we assume that the input data was truncated, and
                     * let the pipeline finish up normally so that it'll give a
                     * valid output file.
                     */
                    ret_val |= IP_INPUT_ERROR;
                } else if (iErrorCode != 0) {
                    /* An error within the data: Make it fatal */
                    longjmp (g->syntax_error, iErrorCode);
                } else if (g->got_EOI) {
                    ret_val |= IP_NEW_OUTPUT_PAGE;
                    /* Note: we should have just done the final MCU in a row, but
                     * we don't check for this because the firmware of Denali/Kodiak
                     * can't guarantee it. */
                }

                if (bDecoded) {
                    g->mcus_done += 1;

                    if (g->mcus_done >= g->mcus_per_row) {
                        PRINT (_T("done with row of MCUs; starting row-sends\n"), 0, 0);
                        g->sending_rows = TRUE;
                        g->mcus_done = 0;
                    }
                }
            } while (!g->got_EOI && !g->sending_rows &&
                     (dwInputAvail-*pdwInputUsed) >= MAX_MCU_SIZE);
        }
    }

      /***************************/
     /* Sending the output rows */
    /***************************/

    if (g->sending_rows) {
        if (g->rows_done>=g->traits.lNumRows && g->traits.lNumRows>=0) {
            /* we've already output all rows, so discard these */
            g->sending_rows = FALSE;
        } else {
            copy_out_rows (
                g,
                g->rows_done % g->rows_per_mcu,  /* in:  index of next row to send */
                pbOutputBuf,               /* out: copied-out row data */
                &n_rows,                   /* out: # of rows copied out */
                &n_bytes);                 /* out: # of bytes output */
            PRINT (_T("copied out %d rows\n"), n_rows, 0);
            *pdwOutputUsed   = n_bytes;
            g->dwOutNextPos += n_bytes;
            g->rows_done    += n_rows;

            if ((g->rows_done>=g->traits.lNumRows && g->traits.lNumRows>=0)
                || (g->rows_done % g->rows_per_mcu)==0) {
                g->sending_rows = FALSE;
            }
            /* n_rows is 1, unless the never-used output_subsampled feature is used */
            if (n_rows > 0)
                ret_val |= IP_CONSUMED_ROW | IP_PRODUCED_ROW;
        }
    }

    *pdwInputNextPos = g->dwInNextPos;

    PRINT (_T("jpeg_decode_convert_row: Returning %04x, in_used=%d\n"),
           ret_val, *pdwInputUsed);
    return ret_val;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD jpgDecode_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * jpgDecode_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD jpgDecode_newPage (
    IP_XFORM_HANDLE hXform)
{
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */
}



/*****************************************************************************\
 *
 * jpgDecode_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD jpgDecode_closeXform (IP_XFORM_HANDLE hXform)
{
    PJDEC_INST g;
    BYTE       **row_pp, **after_pp, *p;

    HANDLE_TO_PTR (hXform, g);
    PRINT (_T("jpeg_decode_close\n"), 0, 0);

    row_pp = &(g->out_rows_ap[0][0]);
    after_pp = row_pp + (sizeof(g->out_rows_ap)/sizeof(BYTE *));

    for ( ; row_pp<after_pp; row_pp++) {
        p = *row_pp;
        if (p != NULL) {
            IP_MEM_FREE (p);
            *row_pp = NULL;
        }
    }

    huff_free_all (g);
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecode_getRowCountInfo - Returns information for determining row count
 *
\*****************************************************************************/

WORD jpgDecode_getRowCountInfo(IP_XFORM_HANDLE hXform,
    int *pRcCountup,int *pRcTraits,int *pSofOffset)
{
    PJDEC_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pRcCountup=g->rows_done;
    *pRcTraits=g->traits.lNumRows;
    *pSofOffset=g->rowCountOffset;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * jpgDecodeTbl - Jump-table for decoder
 *
\*****************************************************************************/

IP_XFORM_TBL jpgDecodeTbl = {
    jpgDecode_openXform,
    jpgDecode_setDefaultInputTraits,
    jpgDecode_setXformSpec,
    jpgDecode_getHeaderBufSize,
    jpgDecode_getActualTraits,
    jpgDecode_getActualBufSizes,
    jpgDecode_convert,
    jpgDecode_newPage,
    jpgDecode_insertedData,
    jpgDecode_closeXform
};

/* End of File */











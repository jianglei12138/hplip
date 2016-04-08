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
 * Ported to Linux (minus bilevel scaling) by David Paschal.
 */

/******************************************************************************\
 *
 * xscale.c - Scales (and interpolates) bi-level, gray and color
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    scaleTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[IP_SCALE_HORIZ_FACTOR] = Horizontal scale-factor, in 8.24 fixed point
 *
 *    aXformInfo[IP_SCALE_VERT_FACTOR] = Vertical scale-factor, in 8.24 fixed point
 *
 *    aXformInfo[IP_SCALE_FAST] = scale fast using simple pixel-replication? 0=no, 1=yes
 *                                currently, this only affects bi-level up-scaling.
 *
 * Capabilities and Limitations:
 *
 *    This driver can scale both up and down, independently.  That is, one
 *    direction can up-scale while the other direction down-scales. Bi-level
 *    scaling is done using tables to maximize speed.  24-bit color,
 *    8-bit gray and bi-level pixels can be scaled.
 *
 *    Color and gray data are interpolated intellegently.
 *    Bi-level data are smoothed when up-scaling (several patents), and
 *    data-loss is avoided when down-scaling (another patent).
 *
 *    The allowed scale-factor ranges differ based on data type (bi-level,
 *    gray and color).  An assert will occur if range is exceeded.
 *    These ranges are:
 *
 *        bi-level:   horiz_fac=[1/2..4.0]  vert_fac=[1/16..4.0]
 *        gray:       horiz_fac=[1/4..6.0]  vert_fac=[1/4..6.0]
 *        color:      horiz_fac=[1/4..6.0]  vert_fac=[1/4..6.0]
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait               default input            output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   computed
 *    iBitsPerPixel         * passed into output   same as default input
 *    iComponentsPerPixel   * passed into output   same as default input
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   computed
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 *
 * Feb 1998 Mark Overton, ported to new Windows software Image Processor
 * Jun 1997 Mark Overton, added color scaling
 * May 1997 Mark Overton, added gray scaling
 *     1995 Mark Overton, wrote original code (bi-level only)
 *
\******************************************************************************/

#include "string.h"    /* for memset and memcpy */
#include "assert.h"

#include "hpip.h"
#include "ipdefs.h"


#if 0
    #include "stdio.h"
    #include <tchar.h>

    #define PRINT(msg,arg1,arg2) \
        _ftprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x1ce5ca7e
#define SC_WHITE_ROW 1   /* was for tracking all-white rows; not used */


/*____________________________________________________________________________
 |                                                                            |
 | Things common to all image-types                                           |
 |____________________________________________________________________________|
*/


#define MAX_ROWS_AP     6   /* Number of entries in rows_ap */
#define HELD_ARRAY_SIZE 7   /* # entries in apHeldOutRows array */

typedef enum {
    IM_BILEVEL,
    IM_GRAY,
    IM_COLOR
} IM_TYPE;


/* SC_INST contains all the variables for a scaling-instance */

typedef struct {
    IM_TYPE image_type;      /* type of image (bilevel, gray, color)          */
    BOOL    fast;            /* scale (quickly) using pixel replication?      */
    BYTE    nMoreFlush;      /* # more flush calls reqd to send buffered rows */
    ULONG   horiz_fac;       /* horiz scale-factor (16.16 fixed-pt)           */
    ULONG   vert_fac;        /* vert  scale-factor (16.16 fixed-pt)           */
    long    vert_pos;        /* current vert pos (16.16; signed, we use neg)  */
    int     in_row_nbytes;   /* # bytes in each input row                     */
    int     out_row_nbytes;  /* # bytes in each output row                    */
    int     out_row_pixels;  /* # pixels in each output row                   */
    int     in_nrows;        /* number of rows read in so far                 */
    int     out_nrows;       /* number of rows written so far                 */
    BYTE   *rows_ap[MAX_ROWS_AP];  /* ptrs to successive input rows           */
    int     nMaxOutRows;     /* max # output rows resulting from one input row*/
    int     nMoreHeldOutRows;/* more output rows needed to be returned        */
    int     iNextHeldOutRow; /* index of next output row to be returned       */
    BYTE   *apHeldOutRows[HELD_ARRAY_SIZE];
                             /* output rows stored for subsequent returning;  */
                             /* index [0] is caller's output buffer           */
    int     top_row;         /* bilevel: index of top of 3 rows in rows_ap    */
    ULONG   post_fac;        /* bilevel: additional upscaling                 */
    BYTE    mid_traits;      /* bilevel: trait-bits of middle row             */

    ULONG   inv_horiz_fac;   /* gray: inverse of horiz scaling factor (16.16) */
    ULONG   inv_vert_fac;    /* gray: inverse of vert scaling factor (16.16)  */
    long    inv_vert_pos;    /* cur inv vert pos (16.16; signed, we use neg)  */
    BYTE    n_saved_rows;    /* gray: # rows saved in rows_ap for vert scaling*/
    BYTE    n_alloced_rows;  /* gray: # rows allocated in rows_ap             */

    IP_IMAGE_TRAITS inTraits;/* traits of the input image                     */
    DWORD   dwInNextPos;     /* file pos for subsequent input                 */
    DWORD   dwOutNextPos;    /* file pos for subsequent output                */
    DWORD   dwValidChk;      /* struct validity check value                   */
} SC_INST, *PSC_INST;


/******************************************************************************
 ******************************************************************************

                           DUMMY BILEVEL FUNCTIONS

 ******************************************************************************
 ******************************************************************************/

/* These functions were removed because they contained patented algorithms.
 * Therefore, only gray and color (not bilevel) scaling are currently
 * supported.  If this is ever fixed, then remove the assert in
 * scale_setDefaultInputTraits(). */

void bi_fast_open(PSC_INST g, UINT in_row_len) {
    fatalBreakPoint();
}

void bi_scale_open(PSC_INST g, UINT in_row_len) {
    fatalBreakPoint();
}

int bi_fast_row(PSC_INST g,PBYTE pbInputBuf,BYTE src_traits,
    BYTE *apHeldOutRows[],ULONG *pdest_traits) {
    fatalBreakPoint();
    return 0;
}

int bi_scale_row(PSC_INST g,PBYTE pbInputBuf,BYTE src_traits,
    BYTE *apHeldOutRows[],ULONG *pdest_traits) {
    fatalBreakPoint();
    return 0;
}

void bi_fast_close(PSC_INST g) {
    fatalBreakPoint();
}

void bi_scale_close(PSC_INST g) {
    fatalBreakPoint();
}


/******************************************************************************
 ******************************************************************************

                           CONTONE (GRAY AND COLOR)

 ******************************************************************************
 ******************************************************************************/


/*
 * Scaling Algorithms
 * 
 *   The descriptions below are for the X axis.  For the Y axis, substitute
 *   "input row" for "input pixel", and "output row" for "output pixel", and
 *   apply weights to entire rows using the same algorithms.
 * 
 *   Each axis is scaled separately using the these methods.  So you can
 *   down-scale in one axis and up-scale in the other, if you wish.
 * 
 * 
 * Down-scaling Algorithm
 * 
 *   Imagine marking the pixel-boundaries on two rulers, and laying them
 *   next to each other:
 * 
 *               in1 pos in2   in3
 *      |______|______|______|______|______|______|______|  <-- input pixels
 *      |           |           |           |           |   <-- output pixels
 *                  0    out    1
 * 
 *   An output pixel consists of a weighted sum of the input pixels that
 *   it overlaps.  The weights are the amount of overlap.
 * 
 *   pos is the right side of the leftmost input pixel that overlaps the
 *   current output pixel.  This position is in units of output pixels (a
 *   distance of 1 is the width of an output pixel), and 0 is the left side
 *   of the current output pixel.  Since this position is in units of output
 *   pixels, it can be used to easily compute the weights.
 * 
 *   The weights of the overlapping input pixels are:
 * 
 *      leftmost:                pos
 *      middle pixels (if any):  scale factor (which is less than 1)
 *      rightmost:               1.0 - (sum of above weights)
 *
 *   Above, the pixel 'out' is computed as:
 *
 *      out = pos*in1 + scalefactor*in2 + (1-pos-scalefactor)*in3
 * 
 * 
 * Up-scaling Algorithm
 * 
 *   I first tried an overlap method like that used for down-scaling, but
 *   for large scale-factors, one output pixel would overlap two input
 *   pixels, and many output pixels would overlap just one input pixel.
 *   So the result looked almost as bad as pixel-replication, because many
 *   pixels were indeed just replications.
 * 
 *   So I devised an interpolating algorithm wherein a pos value is the
 *   weight of the next input pixel, and (1-pos) is the weight of the
 *   current input pixel.  As pos increases along the current input pixel,
 *   the weight will smoothly shift from it to the next input pixel,
 *   eliminating jaggies.
 * 
 *                  0    in0    1    in1
 *      |___________|___________|___________|___________|   <-- input_pixels
 *      |      |      | out  |      |      |      |      |  <-- output pixels
 *                   pos
 * 
 *   pos is the left side of the current output pixel, in units of input
 *   pixels.  0 is the left side of the current input pixel (in0 above).
 * 
 *   pos is advanced by adding 1/scalefactor to it.  When it becomes >= 1,
 *   move to the next input pixel, and subtract 1 from pos.
 * 
 *   Above, the pixel 'out' is computed as:  out = (1-pos)*in0 + pos*in1
 */

#define CONTONE_MIN_HORIZ_FAC  (ULONG)0x04000   /* 0.25 */
   /* Minimum horizontal scale-factor (16.16) */

#define CONTONE_MAX_HORIZ_FAC  ((ULONG)MAX_ROWS_AP << 16)
   /* Maximum horizontal scale-factor (arbitrary) */

#define CONTONE_MIN_VERT_FAC   (ULONG)0x04000   /* 0.25 */
   /* Minimum vertical scale-factor (16.16) */

#define CONTONE_MAX_VERT_FAC   ((ULONG)MAX_ROWS_AP << 16)
   /* Maximum vertical scale-factor (16.16) */



/*____________________________________________________________________________
 |                  |                                                         |
 | gray_horiz_scale | Scales the given input row into the given output row    |
 |__________________|_________________________________________________________|
 |                                                                            |
 | Up-scaling is done by interpolation using horiz_pos.                       |
 | Down-scaling is done by blending (averaging two or more input pixels       |
 | together, forming an output pixel).                                        |
 |____________________________________________________________________________|
*/
static void gray_horiz_scale (
    SC_INST *g,       /* in:  our instance variables */
    BYTE    *src_p,   /* in:  input row to be scaled */
    BYTE    *dest_p)  /* out: output row that we scaled */
{
    ULONG horiz_pos, new_pos;
    BYTE *in_p, *out_p, *out_aft_p;
    UINT  sum, pix;
    UINT  w1, w2;
    UINT  n_pix, u;

    in_p      = src_p;
    out_p     = dest_p;
    out_aft_p = out_p + g->out_row_nbytes;
    in_p[g->in_row_nbytes] = in_p[g->in_row_nbytes-1];  /* dup right pixel */

      /**************/
     /* Up-scaling */
    /**************/

    if (g->horiz_fac >= 0x00010000u)
    {
        horiz_pos = 0;

        while (out_p < out_aft_p) {
            do {
                w2 = (UINT) (horiz_pos >> 8);
                w1 = 256 - w2;
                *out_p++ = (w1*in_p[0] + w2*in_p[1]) >> 8;
                horiz_pos += g->inv_horiz_fac;
            } while ((horiz_pos>>16) == 0);

            horiz_pos &= 0x0000ffffu;
            in_p += 1;
        }
    }

      /****************/
     /* Down-scaling */
    /****************/

    else if (g->fast)
    {
        horiz_pos = 0;
        while (out_p < out_aft_p) {
            *out_p++ = *in_p;
            horiz_pos += g->inv_horiz_fac;
            in_p += horiz_pos >> 16;
            horiz_pos &= 0x0000ffffu;
        }
    }
    else   // not fast
    {
        horiz_pos = g->horiz_fac;
        w2 = g->horiz_fac >> 8;

        while (out_p < out_aft_p) {
            new_pos = horiz_pos;
            n_pix = 1;
            do {
                new_pos += g->horiz_fac;
                n_pix += 1;
            } while ((new_pos>>16) == 0);

            /* Blend n_pix pixels together using these weights:
             *    1st pixel:    horiz_pos
             *    mid pixels:   horiz_fac
             *    final pixel:  1.0 - (sum of above weights)
             */
            sum = horiz_pos >> 8;
            pix = sum * (*in_p++);        /* 1st pixel */

            for (u=1; u<=n_pix-2; u++) {
                pix += w2 * (*in_p++);    /* middle pixels */
                sum += w2;
            }

            pix += (256-sum) * (*in_p);   /* final pixel */

            *out_p++ = pix >> 8;
            horiz_pos = new_pos & 0x0000ffffu;
        }
    } /* end if up-scaling else down-scaling */
}



/*____________________________________________________________________________
 |                   |                                                        |
 | color_horiz_scale | Scales the given input row into the given output row   |
 |___________________|________________________________________________________|
 |                                                                            |
 | Up-scaling is done by interpolation using horiz_pos.                       |
 | Down-scaling is done by blending (averaging two or more input pixels       |
 | together, forming an output pixel).                                        |
 |____________________________________________________________________________|
*/
static void color_horiz_scale (
    SC_INST *g,       /* in:  our instance variables */
    BYTE    *src_p,   /* in:  input row to be scaled */
    BYTE    *dest_p)  /* out: output row that we scaled */
{
    ULONG horiz_pos, new_pos;
    BYTE *in_p, *out_p, *out_aft_p, *p;
    UINT  sum, pix_y, pix_u, pix_v;
    UINT  w1, w2;
    UINT  n_pix, u;

    in_p      = src_p;
    out_p     = dest_p;
    out_aft_p = out_p + g->out_row_nbytes;

    p = in_p + g->in_row_nbytes;
    p[0] = p[-3];  /* dup right pixel */
    p[1] = p[-2];
    p[2] = p[-1];

      /**************/
     /* Up-scaling */
    /**************/

    if (g->horiz_fac >= 0x00010000u)
    {
        horiz_pos = 0;

        while (out_p < out_aft_p) {
            do {
                w2 = (UINT) (horiz_pos >> 8);
                w1 = 256 - w2;
                *out_p++ = (w1*in_p[0] + w2*in_p[3]) >> 8;   /* y component */
                *out_p++ = (w1*in_p[1] + w2*in_p[4]) >> 8;   /* u component */
                *out_p++ = (w1*in_p[2] + w2*in_p[5]) >> 8;   /* v component */
                horiz_pos += g->inv_horiz_fac;
            } while ((horiz_pos>>16) == 0);

            horiz_pos &= 0x0000ffffu;
            in_p += 3;
        }
    }

      /****************/
     /* Down-scaling */
    /****************/

    else if (g->fast)
    {
        int iStep;
        horiz_pos = 0;
        while (out_p < out_aft_p) {
            *out_p++ = in_p[0];
            *out_p++ = in_p[1];
            *out_p++ = in_p[2];

            horiz_pos += g->inv_horiz_fac;
            iStep = horiz_pos >> 16;
            in_p += iStep + iStep + iStep;
            horiz_pos &= 0x0000ffffu;
        }
    }
    else  // down-scaling, not fast
    {
        horiz_pos = g->horiz_fac;
        w2 = g->horiz_fac >> 8;

        while (out_p < out_aft_p) {
            new_pos = horiz_pos;
            n_pix = 1;
            do {
                new_pos += g->horiz_fac;
                n_pix += 1;
            } while ((new_pos>>16) == 0);

            /* Blend n_pix pixels together using these weights:
             *    1st pixel:    horiz_pos
             *    mid pixels:   horiz_fac
             *    final pixel:  1.0 - (sum of above weights)
             */
            sum = horiz_pos >> 8;
            pix_y = sum * (*in_p++);        /* 1st pixel */
            pix_u = sum * (*in_p++);
            pix_v = sum * (*in_p++);

            for (u=1; u<=n_pix-2; u++) {
                pix_y += w2 * (*in_p++);    /* middle pixels */
                pix_u += w2 * (*in_p++);
                pix_v += w2 * (*in_p++);
                sum += w2;
            }

            sum = 256 - sum;
            pix_y += sum * in_p[0];   /* final pixel */
            pix_u += sum * in_p[1];
            pix_v += sum * in_p[2];

            *out_p++ = pix_y >> 8;
            *out_p++ = pix_u >> 8;
            *out_p++ = pix_v >> 8;
            horiz_pos = new_pos & 0x0000ffffu;
        }
    } /* end if up-scaling else down-scaling */
}



/*____________________________________________________________________________
 |                 |                                                          |
 | weight_two_rows | Output row is a weighted-average of two rows in rows_ap  |
 |_________________|__________________________________________________________|
 |                                                                            |
 | rows_ap[0] is weighted by first_weight.                                    |
 | rows_ap[1] is weighted by 1.0 - first_weight.                              |
 | The output is written to dest_p.                                           |
 |____________________________________________________________________________|
*/
static void weight_two_rows (
    SC_INST *g,             /* in:  our instance variables */
    ULONG    first_weight,  /* in:  weight for first row (16.16) */
    BYTE    *dest_p)        /* out: output row */
{
    BYTE *p1, *p2;
    BYTE *out_p, *out_aft_p;

    p1 = g->rows_ap[0];
    p2 = g->rows_ap[1];
    out_p = dest_p;
    out_aft_p = out_p + g->out_row_nbytes;

#if 0
    UINT w1, w2;
    w1 = first_weight >> 8;
    w2 = 256 - w1;
    while (out_p < out_aft_p)
        *out_p++ = (w1*(*p1++) + w2*(*p2++)) >> 8;
#else
    switch ((first_weight+(1u<<12)) >> 13)   /* round weight to closest 8th */
    {
        case 0:
            memcpy (out_p, p2, g->out_row_nbytes);
            break;
        case 1:
            while (out_p < out_aft_p)
                *out_p++ = (*p1>>3) + *p2 - (*p2>>3);
                p1++; p2++;
            break;
        case 2:
            while (out_p < out_aft_p)
                *out_p++ = (*p1>>2) + *p2 - (*p2>>2);
                p1++; p2++;
            break;
        case 3:
            while (out_p < out_aft_p)
                *out_p++ = (*p1>>2) + (*p1>>3) + (*p2>>1) + (*p2>>3);
                p1++; p2++;
            break;
        case 4:
            while (out_p < out_aft_p)
                *out_p++ = (*p1>>1) + (*p2>>1);
                p1++; p2++;
            break;
        case 5:
            while (out_p < out_aft_p)
                *out_p++ = (*p1>>1) + (*p1>>3) + (*p2>>2) + (*p2>>3);
                p1++; p2++;
            break;
        case 6:
            while (out_p < out_aft_p)
                *out_p++ = *p1 - (*p1>>2) + (*p2>>2);
                p1++; p2++;
            break;
        case 7:
            while (out_p < out_aft_p)
                *out_p++ = *p1 - (*p1>>3) + (*p2>>3);
                p1++; p2++;
            break;
        case 8:
            memcpy (out_p, p1, g->out_row_nbytes);
            break;
        default:
            assert (0);
    }
#endif
}



/*____________________________________________________________________________
 |               |                                                            |
 | weight_n_rows | Blends two or more rows in rows_ap into one output row     |
 |_______________|____________________________________________________________|
 |                                                                            |
 | The weights are:                                                           |
 |     1st row:     first_weight                                              |
 |     middle rows: mid_weight                                                |
 |     final row:   1.0 - (sum of above weights)                              |
 |____________________________________________________________________________|
*/
static void weight_n_rows (
    SC_INST *g,             /* in:  our instance variables */
    UINT     n_rows,        /* in:  number of rows to blend together */
    ULONG    first_weight,  /* in:  weight of first row (16.16) */
    ULONG    mid_weight,    /* in:  weight of middle rows (16.16) */
    BYTE    *dest_p)        /* out: output row */
{
    BYTE *in_p[MAX_ROWS_AP];
    BYTE *out_p, *out_aft_p;
    UINT  weights[MAX_ROWS_AP];
    UINT  sum;
    UINT  u;

    assert (n_rows>=2 && n_rows<=MAX_ROWS_AP);

    if (n_rows == 2) {
        weight_two_rows (g, first_weight, dest_p);
        return;
    }

    out_p = dest_p;
    out_aft_p = out_p + g->out_row_nbytes;
    for (u=0; u<n_rows; u++)
        in_p[u] = g->rows_ap[u];

    sum = weights[0] = first_weight >> 8;
    for (u=1; u<=n_rows-2; u++)
        sum += (weights[u] = mid_weight >> 8);
    weights[n_rows-1] = 256 - sum;

    while (out_p < out_aft_p) {
        sum = 0;
        for (u=0; u<n_rows; u++)
            sum += weights[u] * (*in_p[u]++);
        *out_p++ = sum >> 8;
    }
}



/*____________________________________________________________________________
 |                    |                                                       |
 | contone_scale_open | sets up the given scaling job                         |
 |____________________|_______________________________________________________|
*/
static void contone_scale_open (
    SC_INST *g,            /* ptr to our scaling instance             */
    UINT     in_row_len)   /* # pixels per input row                  */
{
    ULONG horiz_fac;  /* scale-factors in 16.16 fixed-point */
    ULONG vert_fac;
    UINT  n;

    horiz_fac = g->horiz_fac;
    vert_fac  = g->vert_fac;

    if (! g->fast) {
        assert (horiz_fac>=CONTONE_MIN_HORIZ_FAC &&
                horiz_fac<=CONTONE_MAX_HORIZ_FAC);
        assert ( vert_fac>=CONTONE_MIN_VERT_FAC  &&
                 vert_fac<=CONTONE_MAX_VERT_FAC);
    }

    g->vert_pos       = 0;
    g->in_row_nbytes  = in_row_len;
    g->out_row_pixels = g->out_row_nbytes = (horiz_fac*in_row_len) >> 16;

    if (g->image_type == IM_COLOR) {
        g->in_row_nbytes  *= 3;
        g->out_row_nbytes *= 3;
    }

    g->inv_horiz_fac  = ((0x80000000u / horiz_fac) << 1) + 1u;
    g->inv_vert_fac   = ((0x80000000u /  vert_fac) << 1) + 1u;
    /* We added 1 to the inverse factors above as an unusual way of rounding */

    if (g->fast) {
        g->n_alloced_rows = 0;
    } else if (vert_fac >= 0x00010000u) {  /* up-scaling vertically */
        g->inv_vert_pos = g->inv_vert_fac;
        g->n_alloced_rows = 2;
    } else {                       /* down-scaling vertically */
        g->n_alloced_rows = (BYTE)((g->inv_vert_fac+0x0000ffffu) >> 16) + 1;
        g->vert_pos = vert_fac;
    }

    for (n=0; n<g->n_alloced_rows; n++) {
        IP_MEM_ALLOC (g->out_row_nbytes, g->rows_ap[n]);
        memset (g->rows_ap[n], 0xff, g->out_row_nbytes + 4);
    }

    g->nMoreFlush = 0;   /* no flush-calls are needed */
    return;

    fatal_error:
    assert (0);
}



/*____________________________________________________________________________
 |                     |                                                      |
 | contone_scale_close | de-allocates scaling instance                        |
 |_____________________|______________________________________________________|
*/
static void contone_scale_close (SC_INST *g)
{
    UINT n;

    for (n=0; n<g->n_alloced_rows; n++)
        IP_MEM_FREE (g->rows_ap[n]);
}



/*____________________________________________________________________________
 |                   |                                                        |
 | contone_scale_row | scales the given input row into 0 or more output rows  |
 |___________________|________________________________________________________|
*/
static int contone_scale_row (
    SC_INST *g,               /* in:  ptr to our scaling instance        */
    BYTE    *src_row_p,       /* in:  input row                          */
    BYTE    *dest_rows_ap[])  /* out: output rows                        */
{
    UINT  n_out_rows;
    UINT  u;
    long  weight;
    long  new_pos;
    BYTE *p;

    assert (src_row_p != NULL);

    if (g->fast && g->vert_fac<=0x00010000u)
    {
        /* down-scaling fast */
        g->vert_pos += g->vert_fac;
        n_out_rows = g->vert_pos >> 16;
        g->vert_pos &= 0x0000ffffu;

        if (n_out_rows > 0) {
            if (g->image_type == IM_GRAY) gray_horiz_scale  (g, src_row_p, dest_rows_ap[0]);
            else                          color_horiz_scale (g, src_row_p, dest_rows_ap[0]);
        }

        return n_out_rows;
    }

    p = g->rows_ap[g->n_saved_rows];
    if (g->image_type == IM_GRAY) gray_horiz_scale  (g, src_row_p, p);
    else                          color_horiz_scale (g, src_row_p, p);

    g->n_saved_rows += 1;

    if (g->n_saved_rows == 1) {
        /* call again to duplicate the first row to get us started */
        return contone_scale_row (g, src_row_p, dest_rows_ap);
    }

      /*************************/
     /* Up-scaling Vertically */
    /*************************/

    if (g->vert_fac >= 0x00010000u)
    {
        n_out_rows = 0;

        if (g->n_saved_rows == 2) {
#if 0
            do {
                weight_two_rows (g, 0x10000u-g->vert_pos,
                                 dest_rows_ap[n_out_rows]);
                n_out_rows += 1;
                g->vert_pos += g->inv_vert_fac;
            } while ((g->vert_pos>>16) == 0);
            g->vert_pos &= 0x0000ffffu;
#else
            /* We use vert_pos solely to determine the number of rows.
             * We use inv_vert_pos to create the weights.
             * In the commented-out code above, inv_vert_pos did both,
             * but the problem was that the number of rows output can
             * off by 1 compared with (num_in_rows*vert_fac), which is
             * what our callers expect.
             */
            g->vert_pos += g->vert_fac;
            n_out_rows = (UINT)(g->vert_pos >> 16);
            g->vert_pos &= 0x0000ffffu;

            for (u=0; u<n_out_rows; u++) {
                weight = 0x10000 - g->inv_vert_pos;
                if (weight < 0) weight = 0;
                else if (weight > 0x10000) weight = 0x10000;
                weight_two_rows (g, weight, dest_rows_ap[u]);
                g->inv_vert_pos += g->inv_vert_fac;
            }

            g->inv_vert_pos -= 0x10000;
#endif
            /* discard the oldest row */
            g->n_saved_rows = 1;
            p = g->rows_ap[0];
            g->rows_ap[0] = g->rows_ap[1];
            g->rows_ap[1] = p;
        }
    }

      /***************************/
     /* Down-scaling Vertically */
    /***************************/

    else
    {
        n_out_rows = 0;
        new_pos = g->vert_pos + (g->n_saved_rows-1)*g->vert_fac;

        if ((new_pos>>16) != 0) {
            weight_n_rows (g, g->n_saved_rows, g->vert_pos, g->vert_fac,
                           dest_rows_ap[0]);
            n_out_rows = 1;
            g->vert_pos = new_pos & 0x0000ffffu;

            /* retain the newest row */
            p = g->rows_ap[0];
            g->rows_ap[0] = g->rows_ap[g->n_saved_rows-1];
            g->rows_ap[g->n_saved_rows-1] = p;
            g->n_saved_rows = 1;
        }
    }

    return n_out_rows;
}



/******************************************************************************
 ******************************************************************************

                       E X P O R T E D   R O U T I N E S

 ******************************************************************************
 ******************************************************************************/



/*****************************************************************************\
 *
 * scale_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD scale_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PSC_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(SC_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(SC_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scale_setDefaultInputTraits - Specifies default input image traits
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

static WORD scale_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PSC_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (   (pTraits->iBitsPerPixel==24 && pTraits->iComponentsPerPixel==3)
            || (pTraits->iBitsPerPixel==8  && pTraits->iComponentsPerPixel==1)
            || (pTraits->iBitsPerPixel==1  && pTraits->iComponentsPerPixel==1));
    INSURE (pTraits->iPixelsPerRow > 0);

    switch (pTraits->iBitsPerPixel) {
        case 1:  g->image_type = IM_BILEVEL;  break;
        case 8:  g->image_type = IM_GRAY;     break;
        case 24: g->image_type = IM_COLOR;    break;
    }

    /* We don't actually support IM_BILEVEL currently. */
    INSURE(g->image_type != IM_BILEVEL);

    g->inTraits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scale_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD scale_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PSC_INST g;
    HANDLE_TO_PTR (hXform, g);

    g->horiz_fac = (aXformInfo[IP_SCALE_HORIZ_FACTOR].dword+0x80) >> 8;
    g->vert_fac  = (aXformInfo[IP_SCALE_VERT_FACTOR].dword+0x80) >> 8;
    g->fast      =  aXformInfo[IP_SCALE_FAST].dword;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scale_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD scale_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pdwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pdwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * scale_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD scale_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            dwInputAvail,   /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pdwInputUsed,   /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pInTraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PSC_INST g;
    int      i;
    UINT     in_row_len;

    HANDLE_TO_PTR (hXform, g);

    /**** Since there is no header, we'll report no usage of input ****/

    *pdwInputUsed    = 0;
    *pdwInputNextPos = 0;

    /**** Open the particular type of scaler we'll need ****/

    in_row_len = g->inTraits.iPixelsPerRow;

    switch (g->image_type) {
        case IM_BILEVEL:
            if (g->vert_fac < 0x10000u)
                g->fast = FALSE;  /* bi-level down-scale doesn't have fast case */
            if (g->fast) bi_fast_open  (g, in_row_len);
            else         bi_scale_open (g, in_row_len);
            break;

        case IM_GRAY:
        case IM_COLOR:
            if (g->vert_fac > 0x10000u)
                g->fast = FALSE;  /* contone up-scale doesn't have fast case */
            contone_scale_open (g, in_row_len);
            break;
    }

    /**** Allocate the held output rows ****/

    g->nMaxOutRows = (g->vert_fac+0x0000ffffu) >> 16;
    INSURE (g->nMaxOutRows <= HELD_ARRAY_SIZE);
    for (i=1; i<g->nMaxOutRows; i++)
        IP_MEM_ALLOC (g->out_row_nbytes, g->apHeldOutRows[i]);

    /**** Report back input- and output-traits ****/

    *pInTraits  = g->inTraits;   /* structure copies */
    *pOutTraits = g->inTraits;
    pOutTraits->iPixelsPerRow = g->out_row_pixels;
    if (pInTraits->lNumRows >= 0) {
        /* use floating point because fixed-point product would
         * overflow if # rows is over 16 bits */
        pOutTraits->lNumRows = (long)
            ((float)pInTraits->lNumRows * g->vert_fac / 65536.0);
    }

    return IP_DONE | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * scale_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD scale_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pdwMinInBufLen,  /* out: min input buf size */
    PDWORD          pdwMinOutBufLen) /* out: min output buf size */
{
    PSC_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pdwMinInBufLen  = g->in_row_nbytes;
    *pdwMinOutBufLen = g->out_row_nbytes;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scale_convert - Converts one row
 *
\*****************************************************************************/

static WORD scale_convert (
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
    PSC_INST g;
    BYTE     src_traits;
    ULONG    dest_traits;
    int      inUsed, outUsed;
    int      n_rows;
    WORD     wResults;

    HANDLE_TO_PTR (hXform, g);
    inUsed  = 0;
    outUsed = 0;

    /**** Return next stored output-row, if any ****/

    if (g->nMoreHeldOutRows > 0) {
        memcpy (pbOutputBuf,
                g->apHeldOutRows[g->iNextHeldOutRow],
                g->out_row_nbytes);
        g->nMoreHeldOutRows -= 1;
        g->iNextHeldOutRow  += 1;
        outUsed = g->out_row_nbytes;
        goto finish;
    }

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT (_T("scale_convert: Told to flush.\n"), 0, 0);
        if (g->nMoreFlush == 0)
            goto finish;
        g->nMoreFlush -= 1;
        /* do "scale a row" section below, with pbInputBuf equal to NULL */
    } else
        inUsed = g->in_row_nbytes;

    /**** Scale a Row ****/

    g->apHeldOutRows[0] = pbOutputBuf;   /* 1st out-row is client's buffer */
    n_rows = 0;                          /* init to avoid compiler warning */
    src_traits = 0;

    switch (g->image_type) {
        case IM_BILEVEL:
            if (g->fast)
                n_rows = bi_fast_row (g, pbInputBuf, src_traits,
                                      g->apHeldOutRows, &dest_traits);
            else
                n_rows = bi_scale_row (g, pbInputBuf, src_traits,
                                       g->apHeldOutRows, &dest_traits);
            break;

        case IM_GRAY:
        case IM_COLOR:
            n_rows = contone_scale_row (g, pbInputBuf, g->apHeldOutRows);
            break;
    }

    INSURE (n_rows <= g->nMaxOutRows);
    if (n_rows > 0) {
        g->nMoreHeldOutRows = n_rows - 1;
        g->iNextHeldOutRow = 1;
        outUsed = g->out_row_nbytes;
    }

    /**** Report results and return (inUsed and outUsed are valid here) ****/

    finish:

    *pdwInputUsed     = (DWORD)inUsed;
    g->dwInNextPos   += (DWORD)inUsed;
    *pdwInputNextPos  = g->dwInNextPos;

    *pdwOutputUsed    = (DWORD)outUsed;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += (DWORD)outUsed;

    wResults = ( inUsed>0              ? IP_CONSUMED_ROW   : 0)
             | (outUsed>0              ? IP_PRODUCED_ROW   : 0)
             | (g->nMoreHeldOutRows==0 ? IP_READY_FOR_DATA : 0)
             | (pbInputBuf==NULL &&
                g->nMoreFlush==0 &&
                g->nMoreHeldOutRows==0 ? IP_DONE           : 0);
    return wResults;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scale_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD scale_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           dwNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * scale_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD scale_newPage (
    IP_XFORM_HANDLE hXform)
{
    PSC_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * scale_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD scale_closeXform (IP_XFORM_HANDLE hXform)
{
    PSC_INST g;
    int      i;

    HANDLE_TO_PTR (hXform, g);

    switch (g->image_type) {
        case IM_BILEVEL:
            if (g->fast) bi_fast_close  (g);
            else         bi_scale_close (g);
            break;

        case IM_GRAY:
        case IM_COLOR:
            contone_scale_close (g);
            break;
    }

    for (i=1; i<g->nMaxOutRows; i++)
        IP_MEM_FREE (g->apHeldOutRows[i]);
    
    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * scaleTbl - Jump-table for scaler
 *
\*****************************************************************************/

IP_XFORM_TBL scaleTbl = {
    scale_openXform,
    scale_setDefaultInputTraits,
    scale_setXformSpec,
    scale_getHeaderBufSize,
    scale_getActualTraits,
    scale_getActualBufSizes,
    scale_convert,
    scale_newPage,
    scale_insertedData,
    scale_closeXform
};

/* End of File */

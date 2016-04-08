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

/******************************************************************************\
 *
 * xgamma.c - Applies Gamma transform
 *
 ******************************************************************************
 *
 * Name of Global Jump-Table:
 *
 *    gammaTbl
 *
 * Items in aXformInfo array passed into setXformSpec:
 *
 *    aXformInfo[0] = gamma value, in 16.16 fixed point
 *
 * Capabilities and Limitations:
 *
 *    Gamma values must range between 0.0 and 3.5.
 *    Only operates on 8-bit grayscale data.
 *    Uses cubic splines for speed.
 *
 * Default Input Traits, and Output Traits:
 *
 *          trait             default input             output
 *    -------------------  ---------------------  ------------------------
 *    iPixelsPerRow         * passed into output   same as default input
 *    iBitsPerPixel         * must be 8            8
 *    iComponentsPerPixel   * must be 1            1
 *    lHorizDPI               passed into output   same as default input
 *    lVertDPI                passed into output   same as default input
 *    lNumRows                passed into output   same as default input
 *    iNumPages               passed into output   same as default input
 *    iPageNum                passed into output   same as default input
 *
 *    Above, a "*" by an item indicates it must be valid (not negative).
 *
 * Feb   1998 Mark Overton -- ported code to software
 * early 1997 Mark Overton -- wrote original code for Kodiak firmware
 *
\******************************************************************************/

#include "hpip.h"
#include "ipdefs.h"
#include "string.h"    /* for memset and memcpy */


#if 0
    #include "stdio.h"
    #define PRINT(msg,arg1,arg2) \
        fprintf(stderr, msg, (int)arg1, (int)arg2)
#else
    #define PRINT(msg,arg1,arg2)
#endif

#define CHECK_VALUE 0x4ba1dace


typedef struct {
    IP_IMAGE_TRAITS traits;   /* traits of the input image */
    BYTE     gammaTable[256]; /* the gamma table */
    WORD     wRowsDone;       /* number of rows converted so far */
    DWORD    dwInNextPos;     /* file pos for subsequent input */
    DWORD    dwOutNextPos;    /* file pos for subsequent output */
    DWORD    dwValidChk;      /* struct validity check value */
} GAM_INST, *PGAM_INST;




/*____________________________________________________________________________
 |                   |                                                        |
 | fast_sin fast_cos | fast sine and cosine functions                         |
 |___________________|________________________________________________________|
 |                                                                            |
 | Input:    Units of angle is fraction of circle (e.g., 1.0 is 360 deg).     |
 |           Format is 0.32 fixed-point (i.e., 32 bits of fraction).          |
 |                                                                            |
 | Returns:  Sine or cosine expressed as 16.16 signed fixed-point.            |
 |                                                                            |
 | Accuracy: Max error is 0.000054 (i.e., 14 good bits of fraction).          |
 |____________________________________________________________________________|
*/
static long fast_sin (ULONG ang)
{
    ULONG ang31, ang30, delta_ang, result;
    UINT  index, base, next;

    /* 90 deg is divided into this many intervals */
    #define INDEX_BITS      6
    #define N_INTERVALS     (1<<INDEX_BITS)
    #define HALF_WORST_ERR  3

    static const USHORT sin_table[N_INTERVALS+2] = {
            0,  1608,  3216,  4821,  6424,  8022,  9616, 11204,
        12785, 14359, 15924, 17479, 19024, 20557, 22078, 23586,
        25080, 26558, 28020, 29466, 30893, 32303, 33692, 35062,
        36410, 37736, 39040, 40320, 41576, 42806, 44011, 45190,
        46341, 47464, 48559, 49624, 50660, 51665, 52639, 53581,
        54491, 55368, 56212, 57022, 57798, 58538, 59244, 59914,
        60547, 61145, 61705, 62228, 62714, 63162, 63572, 63944,
        64277, 64571, 64827, 65043, 65220, 65358, 65457, 65516,
        65535, 65535  /* <-- these should be 65536 (won't fit in 16 bits) */
    };

    /* zero the msb (representing 180 deg), leaving 31 bits */
    ang31 = (ang << 1) >> 1;

    /* fold angle to first quadrant, leaving 30 bits */
    ang30 = ang31;
    if (ang30 >= 0x40000000u)
        ang30 = 0x80000000u - ang30;   /* sin(180-a) = sin(a) */

    index = ang30 >> (30-INDEX_BITS);
    base = sin_table [index];
    next = sin_table [index+1];
    delta_ang = ang30>>(14-INDEX_BITS) & 0x0000ffffu;
    result = ((next-base)*delta_ang >> 16) + base + HALF_WORST_ERR;

    /* negate result if 180-bit was set in original angle */
    if ((long)ang < 0)
        result = (ULONG)(-(long)result);

    return (long)result;

    #undef INDEX_BITS
    #undef N_INTERVALS
    #undef HALF_WORST_ERR
}


static /* inline */ int fast_cos (UINT ang)
{
    return fast_sin(0x40000000u-ang);
}


#if 0

#include <stdio.h>
#include <math.h>

void main (void)
{
    int i;
    float err, max_err;

    max_err = 0.0;

    for (i=0; i<=16*65536; i++) {
        err = sin ((float)i * (2.0*3.1415926535897932384626/(16*65536.0)))
                - fast_sin(i<<12)/65536.0;
        if (err < 0) err = -err;
        if (err > max_err) max_err = err;
    }

    printf ("max err = %f\n", max_err);
}

#endif


#if 0

#include <stdio.h>
#include <math.h>

void main (void)
{
    int i;
    float s;

    for (i=0; i<=64; i++) {
        s = sin ((float)i * (3.1415926535897932384626/(2.0*64.0)));
        printf ("%5d, ", (int)(s*(1<<16) + 0.5));
        if (i%8 == 7) puts("");
    }
}

#endif



typedef struct {
    long a, b, c;   /* x = at + bt^2 + ct^3 */
    long d, e, f;   /* y = dt + et^2 + ft^3 */
} cubic_t;



/*____________________________________________________________________________
 |                   |                                                        |
 | calc_cubic_coeffs | calcs coefficients given start/end angles/velocities   |
 |___________________|________________________________________________________|
 |                                                                            |
 | Units of angle is fraction of circle (e.g., 1.0 is 360 deg).               |
 | Format is 0.32 fixed-point (i.e., 32 bits of fraction).                    |
 |____________________________________________________________________________|
*/
static void calc_cubic_coeffs (
    ULONG    start_ang,
    ULONG    final_ang, 
    long     start_vel,   /* velocities are in 16.16 fixed point */
    long     final_vel,
    cubic_t *p)
{
    long start_x, start_y, final_x, final_y;

    /* For both start and final below:  vel = vel * 2.0 / (1.0 + cos(ang)),
     * where vel is 16.16 before calc, and is 20.12 after the calc below.
     */
    start_vel = (start_vel<<11) / ((1lu<<14) + (fast_cos(start_ang)>>2));
    final_vel = (final_vel<<11) / ((1lu<<14) + (fast_cos(final_ang)>>2));

    /* Below:   (vel is 20.12) * (cos>>2 is 18.14) yields a 6.26;
     *          the >>10 changes 6.26 into a 16.16.
     */
    start_x = start_vel*(fast_cos(start_ang)>>2) >> 10;
    start_y = start_vel*(fast_sin(start_ang)>>2) >> 10;
    final_x = final_vel*(fast_cos(final_ang)>>2) >> 10;
    final_y = final_vel*(fast_sin(final_ang)>>2) >> 10;

    p->a = start_x;
    p->d = start_y;
    p->b = (3<<16) - 2*start_x - final_x;
    p->e = -2*start_y - final_y;
    p->c = start_x + final_x - (2<<16);
    p->f = start_y + final_y;
}



/*____________________________________________________________________________
 |                 |                                                          |
 | transform_cubic | makes cubic end at (x_end,y_end) instead of (1,0)        |
 |_________________|__________________________________________________________|
*/
static void transform_cubic (
    int      x_end,
    int      y_end,
    cubic_t *p)
{
    cubic_t q;

    q.a = p->a*x_end - p->d*y_end;
    q.b = p->b*x_end - p->e*y_end;
    q.c = p->c*x_end - p->f*y_end;

    q.d = p->a*y_end + p->d*x_end;
    q.e = p->b*y_end + p->e*x_end;
    q.f = p->c*y_end + p->f*x_end;

    *p = q;
}



/*____________________________________________________________________________
 |                        |                                                   |
 | calc_gamma_from_coeffs | calcs gamma table, given the coefficients         |
 |________________________|___________________________________________________|
*/
static void calc_gamma_from_coeffs (
    cubic_t *p,       /* in:  coefficients of cubic curve */
    BYTE     tbl[])   /* out: array to receive the gamma table */
{
    #define LG_N_INTERVALS 7   /* 7 is the max; 8 causes overflow */
    #define N_INTERVALS    (1u<<LG_N_INTERVALS)
    #define RND_PROD(p)    (((p) + ((1<<LG_N_INTERVALS)-1)) >> LG_N_INTERVALS)

    int  t, x, y, xprev=0, yprev=0, xtmp, ytmp;

    for (t=0; t<=N_INTERVALS; t+=1) {
        x = (((RND_PROD((RND_PROD(p->c*t) + p->b)*t) + p->a)*t
                >> (LG_N_INTERVALS+15)) + 1) >> 1;
        y = (((RND_PROD((RND_PROD(p->f*t) + p->e)*t) + p->d)*t
                >> (LG_N_INTERVALS+15)) + 1) >> 1;

        if (t==0 || x!=xprev) tbl[x] = y;

        if (t > 0) {
            xtmp = xprev + 1;
            ytmp = (yprev+y) / 2;
            while (xtmp < x)
                tbl[xtmp++] = ytmp;
        }

        xprev = x;  yprev = y;
    }
}



/*____________________________________________________________________________
 |                  |                                                         |
 | calc_gamma_table | calculates gamma table using a cubic curve              |
 |__________________|_________________________________________________________|
*/
static void calc_gamma_table (
    long gamma,  /* in:  gamma value as 24.8 (0.7 .. 3.5 is useful range) */
    BYTE tbl[])  /* out: array to receive the gamma table */
{
    #define FLOAT_TO_FIX(f) ((short)((f)*0x1000 + 0.5))  /* output is 8.8 */
    #define FLOAT_TO_ANG(f) ((USHORT)(long)((f)*0x10000/360.0 + 0.5))

    #define MIN_INDEX 0  /* corresponds to gamma=0.0 */
    #define MAX_INDEX 6  /* corresponds to gamma=3.0 */

    typedef struct {  /* ang_base=0.16; all others are 4.12 */
        short  start_vel_base;   short start_vel_slope;
        USHORT start_ang_base;   short start_ang_slope;
        short  final_vel_base;   short final_vel_slope;
        USHORT final_ang_base;   short final_ang_slope;
    } spec_t;

    static const spec_t specs[MAX_INDEX-MIN_INDEX+1] = {
        {   /* 0.0 .. 0.5  (just does a straight line) */
            FLOAT_TO_FIX(1.0),   FLOAT_TO_FIX(0.0),          /* start vel */
            FLOAT_TO_ANG(0.0),   FLOAT_TO_FIX(0.0),          /* start ang */
            FLOAT_TO_FIX(1.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(0.0),   FLOAT_TO_FIX(0.0),          /* final ang */
        },
        {   /* 0.5 .. 1.0 (bogus below about 0.7) */
            FLOAT_TO_FIX(0.05),  FLOAT_TO_FIX(0.5),          /* start vel */
            FLOAT_TO_ANG(-75.0), FLOAT_TO_FIX(150.0/360.0),  /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(15.0),  FLOAT_TO_FIX(-30.0/360.0),  /* final ang */
        },
        {   /* 1.0 .. 1.5 */
            FLOAT_TO_FIX(0.3),   FLOAT_TO_FIX(0.2),          /* start vel */
            FLOAT_TO_ANG(0.0),   FLOAT_TO_FIX(60.0/360.0),   /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(0.0),   FLOAT_TO_FIX(-20.0/360.0),  /* final ang */
        },
        {   /* 1.5 .. 2.0 */
            FLOAT_TO_FIX(0.4),   FLOAT_TO_FIX(0.2),          /* start vel */
            FLOAT_TO_ANG(30.0),  FLOAT_TO_FIX(20.0/360.0),   /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(-10.0), FLOAT_TO_FIX(-16.0/360.0),  /* final ang */
        },
        {   /* 2.0 .. 2.5 */
            FLOAT_TO_FIX(0.5),   FLOAT_TO_FIX(0.3),          /* start vel */
            FLOAT_TO_ANG(40.0),  FLOAT_TO_FIX(4.0/360.0),    /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(-18.0), FLOAT_TO_FIX(-8.0/360.0),   /* final ang */
        },
        {   /* 2.5 .. 3.0 */
            FLOAT_TO_FIX(0.65),  FLOAT_TO_FIX(0.4),          /* start vel */
            FLOAT_TO_ANG(42.0),  FLOAT_TO_FIX(4.0/360.0),    /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(-22.0), FLOAT_TO_FIX(-6.0/360.0),   /* final ang */
        },
        {   /* 3.0 .. 3.5 */
            FLOAT_TO_FIX(0.85),  FLOAT_TO_FIX(0.5),          /* start vel */
            FLOAT_TO_ANG(44.0),  FLOAT_TO_FIX(1.0/360.0),    /* start ang */
            FLOAT_TO_FIX(2.0),   FLOAT_TO_FIX(0.0),          /* final vel */
            FLOAT_TO_ANG(-25.0), FLOAT_TO_FIX(-4.0/360.0),   /* final ang */
        },
    };

    #define BEGIN_BLACKS 0      /* # of 0's at beginning of gamma table */
    #define END_WHITES   4      /* # of 255's at final of gamma table */

    const spec_t *p;
    long    start_vel, final_vel;
    ULONG   start_ang, final_ang;
    cubic_t cubic;
    int     i;

    i = gamma >> 7;
    if (i > MAX_INDEX) i = MAX_INDEX;
    p = &specs[i-MIN_INDEX];
    gamma -= i << 7;

    start_vel = ((long)p->start_vel_base << 4)
                + (p->start_vel_slope*gamma >> 4);
    final_vel = ((long)p->final_vel_base << 4)
                + (p->final_vel_slope*gamma >> 4);
    start_ang = ((long)p->start_ang_base << 16)
                + (p->start_ang_slope*gamma << 12);
    final_ang = ((long)p->final_ang_base << 16)
                + (p->final_ang_slope*gamma << 12);

    calc_cubic_coeffs (start_ang, final_ang, start_vel, final_vel, &cubic);
    transform_cubic (255-BEGIN_BLACKS-END_WHITES, 254, &cubic);
    calc_gamma_from_coeffs (&cubic, tbl);

    memmove (&tbl[BEGIN_BLACKS], &tbl[0], (256-BEGIN_BLACKS)*sizeof(BYTE));
    tbl[0] = 0;
    for (i=0; i<BEGIN_BLACKS; i++)
        tbl[i] = 0;

    for (i=256-END_WHITES; i<=255; i++)
        tbl[i] = 255;
}




/*****************************************************************************\
 *
 * gamma_openXform - Creates a new instance of the transformer
 *
 *****************************************************************************
 *
 * This returns a handle for the new instance to be passed into
 * all subsequent calls.
 *
 * Return value: IP_DONE=success; IP_FATAL_ERROR=misc error.
 *
\*****************************************************************************/

static WORD gamma_openXform (
    IP_XFORM_HANDLE *pXform)   /* out: returned handle */
{
    PGAM_INST g;

    INSURE (pXform != NULL);
    IP_MEM_ALLOC (sizeof(GAM_INST), g);
    *pXform = g;
    memset (g, 0, sizeof(GAM_INST));
    g->dwValidChk = CHECK_VALUE;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gamma_setDefaultInputTraits - Specifies default input image traits
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

static WORD gamma_setDefaultInputTraits (
    IP_XFORM_HANDLE  hXform,     /* in: handle for xform */
    PIP_IMAGE_TRAITS pTraits)    /* in: default image traits */
{
    PGAM_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Insure that values we care about are correct */
    INSURE (pTraits->iBitsPerPixel == 8);
    INSURE (pTraits->iComponentsPerPixel == 1);
    INSURE (pTraits->iPixelsPerRow > 0);

    g->traits = *pTraits;   /* a structure copy */
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gamma_setXformSpec - Provides xform-specific information
 *
\*****************************************************************************/

static WORD gamma_setXformSpec (
    IP_XFORM_HANDLE hXform,         /* in: handle for xform */
    DWORD_OR_PVOID  aXformInfo[])   /* in: xform information */
{
    PGAM_INST g;
    DWORD     gamma;

    HANDLE_TO_PTR (hXform, g);
    gamma = aXformInfo[0].dword;
    INSURE (gamma <= 0x38000u);   /* 3.5 is our limit */

    calc_gamma_table ((gamma+0x0080u)>>8, g->gammaTable);
    /* todo: make calc_gamma_table accept a 16.16 gamma value */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gamma_getHeaderBufSize- Returns size of input buf needed to hold header
 *
\*****************************************************************************/

static WORD gamma_getHeaderBufSize (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD           *pwInBufLen)     /* out: buf size for parsing header */
{
    /* since input is raw pixels, there is no header, so set it to zero */
    *pwInBufLen = 0;
    return IP_DONE;
}



/*****************************************************************************\
 *
 * gamma_getActualTraits - Parses header, and returns input & output traits
 *
\*****************************************************************************/

static WORD gamma_getActualTraits (
    IP_XFORM_HANDLE  hXform,         /* in:  handle for xform */
    DWORD            wInputAvail,    /* in:  # avail bytes in input buf */
    PBYTE            pbInputBuf,     /* in:  ptr to input buffer */
    PDWORD           pwInputUsed,    /* out: # bytes used from input buf */
    PDWORD           pdwInputNextPos,/* out: file-pos to read from next */
    PIP_IMAGE_TRAITS pIntraits,      /* out: input image traits */
    PIP_IMAGE_TRAITS pOutTraits)     /* out: output image traits */
{
    PGAM_INST g;

    HANDLE_TO_PTR (hXform, g);

    /* Since there is no header, we'll report no usage of input */
    *pwInputUsed = 0;
    *pdwInputNextPos = 0;

    *pIntraits  = g->traits;   /* structure copies */
    *pOutTraits = g->traits;

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/****************************************************************************\
 *
 * gamma_getActualBufSizes - Returns buf sizes needed for remainder of job
 *
\****************************************************************************/

static WORD gamma_getActualBufSizes (
    IP_XFORM_HANDLE hXform,          /* in:  handle for xform */
    PDWORD          pwMinInBufLen,   /* out: min input buf size */
    PDWORD          pwMinOutBufLen)  /* out: min output buf size */
{
    PGAM_INST g;

    HANDLE_TO_PTR (hXform, g);
    *pwMinInBufLen = *pwMinOutBufLen = g->traits.iPixelsPerRow;
    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gamma_convert - Converts one row
 *
\*****************************************************************************/

static WORD gamma_convert (
    IP_XFORM_HANDLE hXform,
    DWORD           wInputAvail,      /* in:  # avail bytes in in-buf */
    PBYTE           pbInputBuf,       /* in:  ptr to in-buffer */
    PDWORD          pwInputUsed,      /* out: # bytes used from in-buf */
    PDWORD          pdwInputNextPos,  /* out: file-pos to read from next */
    DWORD           wOutputAvail,     /* in:  # avail bytes in out-buf */
    PBYTE           pbOutputBuf,      /* in:  ptr to out-buffer */
    PDWORD          pwOutputUsed,     /* out: # bytes written in out-buf */
    PDWORD          pdwOutputThisPos) /* out: file-pos to write the data */
{
    PGAM_INST g;
    int       nBytes;
    PBYTE     pIn, pOut, pOutAfter;

    HANDLE_TO_PTR (hXform, g);

    /**** Check if we were told to flush ****/

    if (pbInputBuf == NULL) {
        PRINT ("gamma_convert: Told to flush.\n", 0, 0);
        *pwInputUsed = *pwOutputUsed = 0;
        *pdwInputNextPos  = g->dwInNextPos;
        *pdwOutputThisPos = g->dwOutNextPos;
        return IP_DONE;
    }

    /**** Output a Row ****/

    nBytes = g->traits.iPixelsPerRow;
    INSURE (wInputAvail  >= nBytes );
    INSURE (wOutputAvail >= nBytes);

    pIn  = pbInputBuf;
    pOut = pbOutputBuf;
    pOutAfter = pOut + nBytes;

    while (pOut < pOutAfter) {
        pOut[0] = g->gammaTable[pIn[0]];
        pOut[1] = g->gammaTable[pIn[1]];
        pOut[2] = g->gammaTable[pIn[2]];
        pOut[3] = g->gammaTable[pIn[3]];
        pOut[4] = g->gammaTable[pIn[4]];
        pOut[5] = g->gammaTable[pIn[5]];
        pOut[6] = g->gammaTable[pIn[6]];
        pOut[7] = g->gammaTable[pIn[7]];

        pIn  += 8;
        pOut += 8;
    }

    *pwInputUsed      = nBytes;
    g->dwInNextPos   += nBytes;
    *pdwInputNextPos  = g->dwInNextPos;

    *pwOutputUsed     = nBytes;
    *pdwOutputThisPos = g->dwOutNextPos;
    g->dwOutNextPos  += nBytes;

    g->wRowsDone += 1;

    PRINT ("gamma_convert: Returning, out used = %d\n", out_used, 0);
    return IP_CONSUMED_ROW | IP_PRODUCED_ROW | IP_READY_FOR_DATA;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gamma_insertedData - client inserted into our output stream
 *
\*****************************************************************************/

static WORD gamma_insertedData (
    IP_XFORM_HANDLE hXform,
    DWORD           wNumBytes)
{
    fatalBreakPoint ();
    return IP_FATAL_ERROR;   /* must never be called (can't insert data) */
}



/*****************************************************************************\
 *
 * gamma_newPage - Tells us to flush this page, and start a new page
 *
\*****************************************************************************/

static WORD gamma_newPage (
    IP_XFORM_HANDLE hXform)
{
    PGAM_INST g;

    HANDLE_TO_PTR (hXform, g);
    /* todo: return fatal error if convert is called again? */
    return IP_DONE;   /* can't insert page-breaks, so ignore this call */

    fatal_error:
    return IP_FATAL_ERROR;

}



/*****************************************************************************\
 *
 * gamma_closeXform - Destroys this instance
 *
\*****************************************************************************/

static WORD gamma_closeXform (IP_XFORM_HANDLE hXform)
{
    PGAM_INST g;

    HANDLE_TO_PTR (hXform, g);

    g->dwValidChk = 0;
    IP_MEM_FREE (g);       /* free memory for the instance */

    return IP_DONE;

    fatal_error:
    return IP_FATAL_ERROR;
}



/*****************************************************************************\
 *
 * gammaTbl - Jump-table for transform driver
 *
\*****************************************************************************/

IP_XFORM_TBL gammaTbl = {
    gamma_openXform,
    gamma_setDefaultInputTraits,
    gamma_setXformSpec,
    gamma_getHeaderBufSize,
    gamma_getActualTraits,
    gamma_getActualBufSizes,
    gamma_convert,
    gamma_newPage,
    gamma_insertedData,
    gamma_closeXform
};

/* End of File */

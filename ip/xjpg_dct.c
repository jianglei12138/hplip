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

/*____________________________________________________________________________
 |            |                                                               |
 | xjpg_dct.c | Computes forward and inverse DCT for JPEG                     |
 |____________|_______________________________________________________________|
 |                                                                            |
 | Mark Overton, May 1997                                                     |
 |____________________________________________________________________________|
*/

#include "xjpg_dct.h"


/*____________________________________________________________________________
 |             |                                                              |
 | SUB_AND_ADD | replaces a with a-b, and b with a+b using no temp registers  |
 |_____________|______________________________________________________________|
*/
#define SUB_AND_ADD(a,b) {              \
    b += a;                             \
    a += a;                             \
    a -= b;                             \
}



/*____________________________________________________________________________
 |           |                                                                |
 | MUL_ROUND | computes x = round(x*c), using no temp registers               |
 |___________|________________________________________________________________|
 |                                                                            |
 | c is assumed to have CONST_FRAC_BITS bits of fraction.                     |
 |____________________________________________________________________________|
*/
#if (defined _WINDOWS) && !(defined _WIN32)

    /* We are compiling for 16-bit Windows 3.1 */

    #define MUL_ROUND(c,x) {                                         \
        long product;                                                \
        product = (long)(x) * ((long)(c) << (16-CONST_FRAC_BITS));   \
        x = (product+0x8000L) >> 16;                                 \
    }

#else

    #define MUL_ROUND(c,x) {                                     \
        x = (short)(x) * (short)(c);                             \
        x = ((x)+(1l<<(CONST_FRAC_BITS-1))) >> CONST_FRAC_BITS;  \
    }

#endif



/*____________________________________________________________________________
 |             |                                                              |
 | dct_forward | computes DCT for JPEG                                        |
 |_____________|______________________________________________________________|
 |                                                                            |
 | This is the DCT algorithm based on the small FFT Winograd transform        |
 | from Trans. IEICE, vol. E 71(11), 1095-1097, Nov. 1988                     |
 |                                                                            |
 | Input:  'block' is 64 level-shifted pixels (-128..127 each).               |
 |                                                                            |
 | Output: 'block' is the DCT (64 words).                                     |
 |         These values need to be scaled by the forward correction matrix    |
 |         for the Winograd DCT.                                              |
 |____________________________________________________________________________|
*/
void dct_forward (register int *block_p)
{
    #define CONST_FRAC_BITS 14   /* bits of frac in CONST_1-CONST_5 below */

    #define CONST_1 (23170/2)  /* 15 bits of frac shifted down to 14 */
    #define CONST_2 (17734/2)
    #define CONST_3 (23170/2)
    #define CONST_4 (42813/2)  /* this one wouldn't fit with 15 bits of frac */
    #define CONST_5 (12540/2)

    int *data_p;
    int  d0, d1, d2, d3, d4, d5, d6, d7;

      /******************/
     /* Transform Rows */
    /******************/

    for (data_p=block_p; data_p<block_p+64; data_p+=8)
    {
        d0 = data_p[0];
        d1 = data_p[1];
        d2 = data_p[2];
        d3 = data_p[3];
        d4 = data_p[4];
        d5 = data_p[5];
        d6 = data_p[6];
        d7 = data_p[7];

        SUB_AND_ADD (d0, d7)
        SUB_AND_ADD (d1, d6)
        SUB_AND_ADD (d2, d5)
        SUB_AND_ADD (d4, d3)

        SUB_AND_ADD (d7, d3)
        SUB_AND_ADD (d6, d5)

        SUB_AND_ADD (d3, d5)
        data_p[4] = d3;
        data_p[0] = d5;

        d6 += d7;
        MUL_ROUND (CONST_1, d6)
        SUB_AND_ADD (d7, d6)
        data_p[6] = d7;
        data_p[2] = d6;

        /* At this point, the only live math vars are in:  d0, d1, d2, d4 */

        d7 = d1 + d2;
        MUL_ROUND (CONST_3, d7)
        d1 += d0;
        SUB_AND_ADD (d0, d7)
        d4 -= d2;
        d6 = d1 + d4;
        MUL_ROUND (CONST_5, d6)
        MUL_ROUND (CONST_4, d1)
        d1 -= d6;

        SUB_AND_ADD (d7, d1)
        data_p[7] = d7;
        data_p[1] = d1;

        MUL_ROUND (CONST_2, d4)
        d4 += d6;
        SUB_AND_ADD (d0, d4)
        data_p[5] = d0;
        data_p[3] = d4;
    }

      /*********************/
     /* Transform Columns */
    /*********************/

    for (data_p=block_p; data_p<block_p+8; data_p++)
    {
        d0 = data_p[0*8];
        d7 = data_p[7*8];
        SUB_AND_ADD (d0, d7)

        d4 = data_p[4*8];
        d3 = data_p[3*8];
        SUB_AND_ADD (d4, d3)

        d1 = data_p[1*8];
        d6 = data_p[6*8];
        SUB_AND_ADD (d1, d6)

        d2 = data_p[2*8];
        d5 = data_p[5*8];
        SUB_AND_ADD (d2, d5)

        SUB_AND_ADD (d7, d3)
        SUB_AND_ADD (d6, d5)

        SUB_AND_ADD (d3, d5)
        data_p[4*8] = d3;
        data_p[0*8] = d5;

        d6 += d7;
        MUL_ROUND (CONST_1, d6)
        SUB_AND_ADD (d7, d6)
        data_p[6*8] = d7;
        data_p[2*8] = d6;

        /* At this point, the only live math vars are in:  d0, d1, d2, d4 */

        d7 = d1 + d2;
        MUL_ROUND (CONST_3, d7)
        d1 += d0;
        SUB_AND_ADD (d0, d7)
        d4 -= d2;
        d6 = d1 + d4;
        MUL_ROUND (CONST_5, d6)
        MUL_ROUND (CONST_4, d1)
        d1 -= d6;

        SUB_AND_ADD (d7, d1)
        data_p[7*8] = d7;
        data_p[1*8] = d1;

        MUL_ROUND (CONST_2, d4)
        d4 += d6;
        SUB_AND_ADD (d0, d4)
        data_p[5*8] = d0;
        data_p[3*8] = d4;
    }

    #undef CONST_FRAC_BITS
    #undef CONST_1
    #undef CONST_2
    #undef CONST_3
    #undef CONST_4
    #undef CONST_5
} /* end of dct_forward */



/*____________________________________________________________________________
 |             |                                                              |
 | dct_inverse | computes inverse DCT for JPEG                                |
 |_____________|______________________________________________________________|
 |                                                                            |
 | This is the DCT algorithm based on the small FFT Winograd transform        |
 | from Trans. IEICE, vol. E 71(11), 1095-1097, Nov. 1988                     |
 |                                                                            |
 | Input:  'block' is the DCT (64 words).                                     |
 |         These values are assumed to have been scaled by the inverse        |
 |         correction matrix for the Winograd DCT.                            |
 |                                                                            |
 | Output: 'block' is 64 level-shifted pixels.  These values will have        |
 |         as many bits of fraction as the input DCT had.  After rounding     |
 |         and level-shifting, you must clamp these values to 0..255.         |
 |____________________________________________________________________________|
*/
void dct_inverse (register int *block_p)
{
    #define CONST_FRAC_BITS 13  /* bits of frac in CONST_1-CONST_5 below */

    #define CONST_1 ((46341+2)/4)   /* 15 bits of frac shifted down to 13 */
    #define CONST_2 ((85627+2)/4)
    #define CONST_3 ((46341+2)/4)
    #define CONST_4 ((35468+2)/4)
    #define CONST_5 ((25080+2)/4)

    int *data_p;
    int  d0, d1, d2, d3, d4, d5, d6, d7, tmp;

      /*********************/
     /* Transform Columns */
    /*********************/

    for (data_p=block_p; data_p<block_p+8; data_p++)
    {
        d0 = data_p[0*8];
        d4 = data_p[4*8];
        SUB_AND_ADD (d0, d4)

        d1 = data_p[1*8];
        d7 = data_p[7*8];
        SUB_AND_ADD (d1, d7)

        d2 = data_p[2*8];
        d6 = data_p[6*8];
        SUB_AND_ADD (d2, d6)

        d5 = data_p[5*8];
        d3 = data_p[3*8];
        SUB_AND_ADD (d5, d3)

        MUL_ROUND (CONST_1, d2)
        d2 -= d6;
        SUB_AND_ADD (d0, d2)
        SUB_AND_ADD (d4, d6)
        SUB_AND_ADD (d7, d3)

        tmp = d3;
        SUB_AND_ADD (d6, d3)
        data_p[7*8] = d6;
        data_p[0*8] = d3;

        d6 = d5 - d1;
        MUL_ROUND (CONST_5, d6);
        MUL_ROUND (CONST_4, d1)
        d1 -= d6;
        d1 -= tmp;
        MUL_ROUND (CONST_3, d7)
        d7 -= d1;
        MUL_ROUND (CONST_2, d5)
        d6 -= d5;
        d6 += d7;

        SUB_AND_ADD (d2, d1)
        data_p[6*8] = d2;
        data_p[1*8] = d1;

        SUB_AND_ADD (d0, d7)
        data_p[5*8] = d0;
        data_p[2*8] = d7;

        SUB_AND_ADD (d4, d6)
        data_p[3*8] = d4;
        data_p[4*8] = d6;
    }

      /******************/
     /* Transform Rows */
    /******************/

    for (data_p=block_p; data_p<block_p+64; data_p+=8)
    {
        d0 = data_p[0];
        d1 = data_p[1];
        d2 = data_p[2];
        d3 = data_p[3];
        d4 = data_p[4];
        d5 = data_p[5];
        d6 = data_p[6];
        d7 = data_p[7];

        SUB_AND_ADD (d0, d4)
        SUB_AND_ADD (d1, d7)
        SUB_AND_ADD (d2, d6)
        SUB_AND_ADD (d5, d3)

        MUL_ROUND (CONST_1, d2)
        d2 -= d6;
        SUB_AND_ADD (d0, d2)
        SUB_AND_ADD (d4, d6)
        SUB_AND_ADD (d7, d3)

        tmp = d3;
        SUB_AND_ADD (d6, d3)
        data_p[7] = d6;
        data_p[0] = d3;

        d6 = d5 - d1;
        MUL_ROUND (CONST_5, d6)
        MUL_ROUND (CONST_4, d1)
        d1 -= d6;
        d1 -= tmp;
        MUL_ROUND (CONST_3, d7)
        d7 -= d1;
        MUL_ROUND (CONST_2, d5)
        d6 -= d5;
        d6 += d7;

        SUB_AND_ADD (d2, d1)
        data_p[6] = d2;
        data_p[1] = d1;

        SUB_AND_ADD (d0, d7)
        data_p[5] = d0;
        data_p[2] = d7;

        SUB_AND_ADD (d4, d6)
        data_p[3] = d4;
        data_p[4] = d6;
    }

    #undef CONST_FRAC_BITS
    #undef CONST_1
    #undef CONST_2
    #undef CONST_3
    #undef CONST_4
    #undef CONST_5
} /* end of dct_inverse */

/* End of File */

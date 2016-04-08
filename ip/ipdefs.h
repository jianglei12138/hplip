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
 * ipdefs.h - Definitions common among image processor files
 *
 * Mark Overton, Jan 1998
 *
\*****************************************************************************/

#if ! defined IPDEFS_INC
#define IPDEFS_INC

#ifdef EXPORT_TRANSFORM
#define fatalBreakPoint()   assert(0);
#else
extern void fatalBreakPoint(void);
#endif

#define INSURE(must_be_true)                                \
do {                                                        \
    if (! (must_be_true)) {                                 \
        fatalBreakPoint();                                  \
        goto fatal_error;                                   \
    }                                                       \
} while (0)


#define HANDLE_TO_PTR(hJob_macpar, inst_macpar)             \
do {                                                        \
    inst_macpar = (void*)hJob_macpar;                       \
    INSURE (inst_macpar->dwValidChk == CHECK_VALUE);        \
} while (0)


#define IP_MEM_ALLOC(nBytes_macpar, ptr_macpar)                 \
do {                                                            \
    /* the weirdness below is equivalent to a type cast */      \
    /* the 12 below is buffer-overrun allowance */              \
/*    *(void**)&(ptr_macpar) = (void*) malloc(nBytes_macpar+12); */ \
    (ptr_macpar) = malloc(nBytes_macpar+12);  \
    INSURE (ptr_macpar != NULL);                                \
} while (0)


#define IP_MEM_FREE(ptr_macpar)                             \
do {                                                        \
    if (ptr_macpar != NULL)                                 \
        free (ptr_macpar);                                  \
} while (0)


#define IP_MAX(valOne,valTwo)  \
    ((valOne)>(valTwo) ? (valOne) : (valTwo))

#define IP_MIN(valOne,valTwo)  \
    ((valOne)<(valTwo) ? (valOne) : (valTwo))


/* We use approx NTSC weights of 5/16, 9/16, 2/16 for R, G and B respectively. */
#define NTSC_LUMINANCE(Rval,Gval,Bval)  \
    ((((Rval)<<2) + (Rval) + ((Gval)<<3) + (Gval) + ((Bval)<<1)) >> 4)


/* The MUL32HIHALF macro does a signed 32x32->64 multiply, and then
 * discards the low 32 bits, giving you the high 32 bits.  The way
 * this is done is compiler-dependent.
 */
#if 0
#define MUL32HIHALF(firstpar, secondpar, hihalfresult) {    \
    __int64 prod64;                                            \
    prod64 = (__int64)(firstpar) * (secondpar);                \
    hihalfresult = ((int*)&prod64)[1];                        \
    /* above, use a [0] for big endian */                    
#endif
#define MUL32HIHALF(firstpar, secondpar, hihalfresult) {       \
    hihalfresult = ((__int64)(firstpar) * (secondpar)) >> 32;  \
}

#endif

/* End of File */

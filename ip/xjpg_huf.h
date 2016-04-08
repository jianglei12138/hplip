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
 | xjpg_huf.h | Huffman tables for JPEG encoder and decoder                   |
 |____________|_______________________________________________________________|
 |                                                                            |
 | Mark Overton, April 1996                                                   |
 |____________________________________________________________________________|
*/

#include "hpip.h"

typedef struct {
    WORD code;
    BYTE size;
} enc_huff_elem_t;

typedef struct {
    BYTE size;
    BYTE value;
} dec_huff_elem_t;


/* Tables used by encoder */
extern const enc_huff_elem_t enc_DC_table[17];
extern const enc_huff_elem_t enc_AC_table[256];

/* Tables used by decoder */
extern const dec_huff_elem_t dec_DC_table[13];
extern const BYTE           dec_DC_table_index[512];
extern const dec_huff_elem_t dec_AC_main_table[37];
extern const BYTE           dec_AC_main_table_index[4096];
extern const BYTE           dec_AC_aux_table[128];

/* Tables from which Huffman tables are computed */
extern const BYTE LuminanceDCCounts[16];
extern const BYTE LuminanceDCValues[12];
extern const BYTE LuminanceACCounts[16];
extern const BYTE LuminanceACValues[162];

#if 0
extern const BYTE ChrominanceDCCounts[16];
extern const BYTE ChrominanceDCValues[12];
extern const BYTE ChrominanceACCounts[16];
extern const BYTE ChrominanceACValues[162];
#endif

/* End of File */

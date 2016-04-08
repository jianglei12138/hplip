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
 | xjpg_mrk.h | Markers used in JPEG images                                   |
 |____________|_______________________________________________________________|
 |                                                                            |
 | Mark Overton, Feb 1996                                                     |
 |____________________________________________________________________________|
*/

#define MARKER_NONE 0x00
#define MARKER_END_FILE 0x100

#define MARKER_SOF0 0xc0
#define MARKER_SOF1 0xc1
#define MARKER_SOF2 0xc2
#define MARKER_SOF3 0xc3
#define MARKER_SOF5 0xc5
#define MARKER_SOF6 0xc6
#define MARKER_SOF7 0xc7
#define MARKER_SOF8 0xc8
#define MARKER_SOF9 0xc9
#define MARKER_SOFA 0xca
#define MARKER_SOFB 0xcb
#define MARKER_SOFD 0xcd
#define MARKER_SOFE 0xce
#define MARKER_SOFF 0xcf

#define MARKER_DHT  0xc4
#define MARKER_DAC  0xcc

#define MARKER_RST0 0xd0
#define MARKER_RST1 0xd1
#define MARKER_RST2 0xd2
#define MARKER_RST3 0xd3
#define MARKER_RST4 0xd4
#define MARKER_RST5 0xd5
#define MARKER_RST6 0xd6
#define MARKER_RST7 0xd7

#define MARKER_SOI  0xd8
#define MARKER_EOI  0xd9
#define MARKER_SOS  0xda
#define MARKER_DQT  0xdb
#define MARKER_DNL  0xdc
#define MARKER_DRI  0xdd
#define MARKER_DHP  0xde
#define MARKER_EXP  0xdf

#define MARKER_APP  0xe0        /* from 0xe0 - 0xef */
#define MARKER_JPG  0xf0        /* from 0xf0 - 0xfd */
#define MARKER_COM  0xfe

#define MARKER_SHORT_HEADER (MARKER_APP+1)

/* End of File */

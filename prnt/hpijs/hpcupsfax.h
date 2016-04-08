/*****************************************************************************\
    hpcupsfax.h : HP Cups Fax Filter

    Copyright (c) 2001 - 2015, HP Co.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the Hewlett-Packard nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#ifndef HPCUPSFAX_H
#define HPCUPSFAX_H

/*
 * Raster data encoding methods
 */

#define RASTER_BITMAP      0
#define RASTER_GRAYMAP     1
#define RASTER_MH          2
#define RASTER_MR          3
#define RASTER_MMR         4
#define RASTER_RGB         5
#define RASTER_YCC411      6
#define RASTER_JPEG        7
#define RASTER_PCL         8
#define RASTER_NOT         9
#define RASTER_TIFF        10 
#define RASTER_AUTO        99

#define HPLIPFAX_MONO	1
#define HPLIPFAX_COLOR	2

#define HPLIPPUTINT32(bytearr, val) \
		bytearr[0] = (val & 0xFF000000) >> 24; \
		bytearr[1] = (val & 0x00FF0000) >> 16; \
		bytearr[2] = (val & 0x0000FF00) >> 8;  \
		bytearr[3] = (val & 0x000000FF)

#define HPLIPPUTINT16(bytearr, val) \
		bytearr[0] = (val & 0x0000FF00) >> 8; \
		bytearr[1] = (val & 0x000000FF)

#endif        /* HPCUPSFAX_H */


/*****************************************************************************\
  interp.h : ResSyn routines and structures

  Copyright (c) 1996 - 2015, HP Co.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/


#ifndef APDK_INTERP_H
#define APDK_INTERP_H

APDK_BEGIN_NAMESPACE

/* Constants for RS structure */
#define NUMBER_RASTERS  3 /* The number of Rasters to Buffer */


typedef struct rst
{
    int     Width;
    int     ScaleFactorMultiplier;
    int     ScaleFactorDivisor;
//  int     CallerAlloc;
    int     RastersinBuffer;
    unsigned char*  Bufferpt[NUMBER_RASTERS];
    int     BufferSize;
    unsigned char*  Buffer;

} RESSYNSTRUCT;


void ResSynStart ( int WidthInPixels,
                   int ScaleFactorMultiplier,
                   int ScaleFactorDivisor,
//                   int CallerAlloc,
                   RESSYNSTRUCT *ResSynStruct);


int ResSyn(RESSYNSTRUCT *ResSynStruct, unsigned char *raster_in,
                   unsigned char *raster_out );

APDK_END_NAMESPACE

#endif //APDK_INTERP_H

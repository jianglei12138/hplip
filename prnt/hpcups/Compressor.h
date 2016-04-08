/*****************************************************************************\
  Compressor.h : Interface for Compressor class

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

#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "Processor.h"
#include "CommonDefinitions.h"

class Compressor : public Processor
{
public:
    Compressor(unsigned int RasterSize, bool useseed);
    virtual ~Compressor();
    virtual bool Process(RASTERDATA* InputRaster=NULL)=0;
    virtual void Flush() { }    // no pending output
    unsigned int GetMaxOutputWidth();
    virtual bool NextOutputRaster(RASTERDATA& next_raster)=0;
    void SetSeedRow(BYTE* seed) { SeedRow = seed; }
    DRIVER_ERROR constructor_error;

    // buffer is public for use by GraphicsTranslator
    BYTE* compressBuf;      // output buffer
    BYTE* SeedRow;
    bool UseSeedRow;
    BYTE* originalKData;
    unsigned int compressedsize;
    unsigned int inputsize;
    bool seeded;
}; // Compressor

#endif // COMPRESSOR_H


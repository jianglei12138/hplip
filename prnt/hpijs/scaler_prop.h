/*****************************************************************************\
  scaler_prop.h : Interface for the Scaler_Prop class

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


#ifndef APDK_SCALER_PROP_H
#define APDK_SCALER_PROP_H

APDK_BEGIN_NAMESPACE

class Scaler_Prop : public Scaler
{
public:
    Scaler_Prop(SystemServices* pSys,int inputwidth,
                int numerator,int denominator);
    ~Scaler_Prop();
    BOOL Process(RASTERDATA* InputRaster=NULL);
    BYTE* NextOutputRaster(COLORTYPE color);
protected:
    RESSYNSTRUCT* pRSstruct;
    void Pixel_ReplicateF(int color, int h_offset, int v_offset,
                          unsigned char **out_raster, int plane);
    void PixelMultiply(unsigned char* buffer, unsigned int width, unsigned int factor);

    void rez_synth(RESSYNSTRUCT *ResSynStruct, unsigned char *raster_out);
    void InitInternals();

    unsigned int ResSyn(const unsigned char *raster_in);
    int create_out(BOOL simple);
	unsigned char flag;
};

APDK_END_NAMESPACE

#endif //APDK_SCALER_PROP_H

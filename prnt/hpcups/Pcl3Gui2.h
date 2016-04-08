/*****************************************************************************\
  Pcl3Gui2.h : Interface for Pcl3Gui2 class

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

#ifndef PCL3GUI2_H
#define PCL3GUI2_H

#include "CommonDefinitions.h"
#include "Pipeline.h"
#include "Encapsulator.h"

typedef enum
{
    eCrd_black_only,
    eCrd_color_only,
    eCrd_both
} eCrdType;

class Pcl3Gui2: public Encapsulator
{
public:
    Pcl3Gui2();
    ~Pcl3Gui2();
    DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane);
    DRIVER_ERROR    StartPage(JobAttributes *pJA);
    DRIVER_ERROR    Configure(Pipeline **pipeline);
    bool            UnpackBits() {return false;}

protected:
    virtual bool jobAttrPJLAllowed() {return true;}
    void configureRasterData ();
private:

    DRIVER_ERROR encapsulateRaster(BYTE *input_raster, unsigned int num_bytes, COLORTYPE c_type);
    bool    speed_mech_enabled;
    int     page_number;
    bool    m_run_ernie_filter;
    eCrdType    crd_type;
};

#endif // PCL3GUI2_H


/*****************************************************************************\
  ModeJbig.h : Interface for the ModeJbig class

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

  Author: Naga Samrat Chowdary Narla,
\*****************************************************************************/

#ifndef MODE_JBIG_H
#define MODE_JBIG_H

#include "CommonDefinitions.h"
#include "Processor.h"
#include "Compressor.h"

#define INDY_STRIP_HEIGHT 128
typedef enum
{
    ZJSTREAM,
    ZXSTREAM,
    ZJCOLOR,
    ZJCOLOR2
} ZJPLATFORM;

class ModeJbig : public Compressor
{
public:
    ModeJbig(unsigned int RasterSize);
    virtual ~ModeJbig();
    bool Process(RASTERDATA *input);
    bool NextOutputRaster(RASTERDATA &next_raster);
    DRIVER_ERROR Init(int height, int num_planes, int bpp, ZJPLATFORM ezj_platform);
    void NewPage()
    {
        m_pszCurPtr = m_pszInputRasterData;
        if (m_pszCurPtr)
            memset (m_pszCurPtr, 0, (m_iWidth * m_iLastRaster * m_iPlanes * m_iBPP));
    }
    void Flush();
    void SetBandHeight(int height)
    {
        m_iLastRaster = height;
    }
    void SetLastBand()
    {
        m_bLastBand = true;
    }

private:
    void  compress(int plane_number = 0);
    bool  processZJStream(RASTERDATA *input);
    bool  processZXStream(RASTERDATA *input);
    bool  processZJColor(RASTERDATA *input);

    static const BYTE szByte1[256];
    static const BYTE szByte2[256];

    void   *m_hHPLibHandle;
    int    m_iWidth;
    int    m_iLastRaster;
    int    m_iOrgHeight;
    BYTE   *m_pszCurPtr;
    BYTE   *m_pszInputRasterData;
    int    m_iPlanes;
    int    m_iBPP;
    bool   m_bLastBand;
    ZJPLATFORM    m_ezj_platform;
    int    m_iP[4];
    int    m_iCurRasterPerPlane[4];
    int    m_iPlaneNumber;
    int    m_iCurrentPlane;
}; // ModeJbig

#endif // MODE_JBIG_H


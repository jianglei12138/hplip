/*****************************************************************************\
  ModeJbig.cpp : Implementation for the ModeJbig class

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

#include "CommonDefinitions.h"
#include "Compressor.h"
#include "Pipeline.h"
#include "ModeJbig.h"
#include "hpjbig_wrapper.h"
#include <dlfcn.h>
#include "Utils.h"
#include "utils.h"

extern "C"
{
int (*HPLJJBGCompress) (int iWidth, int iHeight, unsigned char **pBuff,
                        HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt);
int (*HPLJSoInit) (int iFlag);
}

const BYTE ModeJbig::szByte1[256] =
    {
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   2,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   2,   2,   8,   8,   8,   8,
          8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,
         10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
         10,  10,  10,  10,  32,  32,  32,  32,  32,  32,  32,  32,
         32,  32,  32,  32,  32,  32,  32,  32,  34,  34,  34,  34,
         34,  34,  34,  34,  34,  34,  34,  34,  34,  34,  34,  34,
         40,  40,  40,  40,  40,  40,  40,  40,  40,  40,  40,  40,
         40,  40,  40,  40,  42,  42,  42,  42,  42,  42,  42,  42,
         42,  42,  42,  42,  42,  42,  42,  42, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
        130, 130, 130, 130, 136, 136, 136, 136, 136, 136, 136, 136,
        136, 136, 136, 136, 136, 136, 136, 136, 138, 138, 138, 138,
        138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
        160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
        160, 160, 160, 160, 162, 162, 162, 162, 162, 162, 162, 162,
        162, 162, 162, 162, 162, 162, 162, 162, 168, 168, 168, 168,
        168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
        170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
        170, 170, 170, 170,
    };
const BYTE ModeJbig::szByte2[256] =
    {
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,   0,   2,   8,  10,  32,  34,  40,  42,
        128, 130, 136, 138, 160, 162, 168, 170,   0,   2,   8,  10,
         32,  34,  40,  42, 128, 130, 136, 138, 160, 162, 168, 170,
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,   0,   2,   8,  10,  32,  34,  40,  42,
        128, 130, 136, 138, 160, 162, 168, 170,   0,   2,   8,  10,
         32,  34,  40,  42, 128, 130, 136, 138, 160, 162, 168, 170,
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,   0,   2,   8,  10,  32,  34,  40,  42,
        128, 130, 136, 138, 160, 162, 168, 170,   0,   2,   8,  10,
         32,  34,  40,  42, 128, 130, 136, 138, 160, 162, 168, 170,
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,   0,   2,   8,  10,  32,  34,  40,  42,
        128, 130, 136, 138, 160, 162, 168, 170,   0,   2,   8,  10,
         32,  34,  40,  42, 128, 130, 136, 138, 160, 162, 168, 170,
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,   0,   2,   8,  10,  32,  34,  40,  42,
        128, 130, 136, 138, 160, 162, 168, 170,   0,   2,   8,  10,
         32,  34,  40,  42, 128, 130, 136, 138, 160, 162, 168, 170,
          0,   2,   8,  10,  32,  34,  40,  42, 128, 130, 136, 138,
        160, 162, 168, 170,
    };


ModeJbig::ModeJbig (unsigned int RasterSize) : Compressor (RasterSize, false)
{
    m_hHPLibHandle = 0;
    m_pszInputRasterData = 0;
    m_iWidth = ((RasterSize + 31) / 32) * 4;
    m_iPlaneNumber  = 0;
    m_iCurrentPlane = 0;
    m_bLastBand     = false;
    for (int i = 0; i < 4; i++)
    {
        m_iCurRasterPerPlane[i] = 0;
    }

    m_iP[0] = 0;
}

ModeJbig::~ModeJbig()
{
    unload_library(m_hHPLibHandle);

    if (m_pszInputRasterData)
    {
        delete [] m_pszInputRasterData;
    }
}

DRIVER_ERROR ModeJbig::Init(int iLastRaster, int iPlanes, int iBPP, ZJPLATFORM zj_platform)
{
    m_iLastRaster  = iLastRaster;
    m_iOrgHeight   = iLastRaster;
    m_iPlanes      = iPlanes;
    m_iBPP         = iBPP;
    m_ezj_platform = zj_platform;

    m_hHPLibHandle = load_plugin_library(UTILS_PRINT_PLUGIN_LIBRARY, PRNT_PLUGIN_LJ);
    if (m_hHPLibHandle)
    {
        dlerror ();
        *(void **) (&HPLJJBGCompress) = get_library_symbol(m_hHPLibHandle, "hp_encode_bits_to_jbig");
        *(void **) (&HPLJSoInit) = get_library_symbol(m_hHPLibHandle, "hp_init_lib");
        if (!HPLJSoInit || (HPLJSoInit && !HPLJSoInit (1)))
        {
            return PLUGIN_LIBRARY_MISSING;
        }
    }
    else
    {
        return PLUGIN_LIBRARY_MISSING;
    }

    if (iPlanes == 4)
    {
        m_iP[0] = 3;
        m_iP[1] = 0;
        m_iP[2] = 1;
        m_iP[3] = 2;
        if(zj_platform == ZJCOLOR2)
        {
          m_iP[1] = 2;
          m_iP[3] = 0;
        } 
    }

    int    buffer_size = m_iWidth * m_iLastRaster * m_iPlanes * m_iBPP;
    m_pszInputRasterData = new BYTE[buffer_size];
    if (m_pszInputRasterData == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    compressBuf = new BYTE[m_iWidth * m_iLastRaster * m_iBPP];
    if (compressBuf == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    m_pszCurPtr = m_pszInputRasterData;
    memset(m_pszCurPtr, 0, buffer_size);
    return NO_ERROR;
}

void ModeJbig::Flush()
{
    if (m_iCurRasterPerPlane[m_iPlaneNumber] > 0)
    {
        int    height = m_iLastRaster;
        m_iLastRaster = m_iCurRasterPerPlane[m_iPlaneNumber];
	if (m_iPlanes == 1) {
	    compress();
            m_iLastRaster = height;
        }
        iRastersReady = 1;
    }
}

bool ModeJbig::Process (RASTERDATA* input)
{
    if (input==NULL)
    {
        compressedsize = 0;
        iRastersReady  = 0;
        return false;
    }

    bool    bResult = false;
    switch(m_ezj_platform)
    {
        case ZJSTREAM:
            bResult = processZJStream(input);
            break;
        case ZXSTREAM:
            bResult = processZXStream(input);
            break;
        case ZJCOLOR:
        case ZJCOLOR2:
            if (m_iPlanes == 1)
            {
                bResult = processZXStream(input);
                break;
            }
            bResult = processZJColor(input);
            break;
    }
    if (iRastersReady)
    {
        m_iCurRasterPerPlane[m_iPlaneNumber] = 0;
        m_pszCurPtr = m_pszInputRasterData;
    }
    return bResult;
}

bool ModeJbig::processZJStream(RASTERDATA *input)
{
    if (input->rasterdata[COLORTYPE_COLOR])
    {
        memcpy(m_pszCurPtr, input->rasterdata[COLORTYPE_COLOR],
               input->rastersize[COLORTYPE_COLOR]);
    }

    m_iCurRasterPerPlane[0]++;
    m_pszCurPtr += m_iWidth;

    if (m_iCurRasterPerPlane[0] == m_iLastRaster)
    {
        compress();
        iRastersReady = 1;
        return true;
    }
    return false;
}

bool ModeJbig::processZXStream(RASTERDATA *input)
{
    if (input->rasterdata[COLORTYPE_COLOR])
    {
        for (int i = 0; i < input->rastersize[COLORTYPE_COLOR]; i++)
        {
            m_pszCurPtr[i*m_iBPP]   = szByte1[input->rasterdata[COLORTYPE_COLOR][i]];
            m_pszCurPtr[i*m_iBPP+1] = szByte2[input->rasterdata[COLORTYPE_COLOR][i]];
            m_pszCurPtr[i*m_iBPP]   |= (m_pszCurPtr[i*m_iBPP] >> 1);
            m_pszCurPtr[i*m_iBPP+1] |= (m_pszCurPtr[i*m_iBPP+1] >> 1);
        }
    }

    m_iCurRasterPerPlane[0]++;
    m_pszCurPtr += m_iWidth * m_iBPP;
    if (m_iCurRasterPerPlane[0] == m_iLastRaster)
    {
        compress();
        iRastersReady = 1;
        return true;
    }
    return false;
}
bool ModeJbig::processZJColor(RASTERDATA *input)
{
    BYTE    *p = m_pszCurPtr + (m_iP[m_iPlaneNumber] * m_iWidth * m_iBPP) * m_iLastRaster;
    if (input->rasterdata[COLORTYPE_COLOR])
    {
        for (int i = 0; i < input->rastersize[COLORTYPE_COLOR]; i++)
        {
            p[i*m_iBPP] = szByte1[input->rasterdata[COLORTYPE_COLOR][i]];
            p[i*m_iBPP+1] = szByte2[input->rasterdata[COLORTYPE_COLOR][i]];
            p[i*m_iBPP] |= (p[i*m_iBPP] >> 1);
            p[i*m_iBPP+1] |= (p[i*m_iBPP+1] >> 1);
        }
    }

    m_iCurRasterPerPlane[m_iPlaneNumber]++;
    if (m_iCurRasterPerPlane[m_iPlaneNumber] == m_iLastRaster && m_iPlaneNumber == 3)
    {
        iRastersReady = 1;
        m_pszCurPtr = m_pszInputRasterData;
        m_iPlaneNumber = 0;
        return true;
    }
    m_iPlaneNumber++;
    if (m_iPlaneNumber == 4)
    {
        m_pszCurPtr += m_iBPP * m_iWidth;
        m_iPlaneNumber = 0;
    }
    return false;
}

bool ModeJbig::NextOutputRaster(RASTERDATA& next_raster)
{
    if (iRastersReady == 0)
    {
        return false;
    }

    if (m_iPlanes == 4)
    {
        compress(m_iCurrentPlane++);
    }
    next_raster.rastersize[COLORTYPE_COLOR] = compressedsize;
    next_raster.rasterdata[COLORTYPE_COLOR] = compressBuf;
    next_raster.rastersize[COLORTYPE_BLACK] = 0;
    next_raster.rasterdata[COLORTYPE_BLACK] = NULL;

    if (m_iPlanes == 1 || (m_iCurrentPlane == 4))
    {
        iRastersReady = 0;
        m_iCurrentPlane = 0;
        m_iLastRaster = m_iOrgHeight;
    }
    return true;
}

void ModeJbig::compress (int plane_number)
{
    HPLJZjcBuff         myBuffer;

    HPLJZjsJbgEncSt   se;
    myBuffer.pszCompressedData = compressBuf;
    myBuffer.dwTotalSize = 0;
    BYTE    *p = m_pszInputRasterData + (m_iWidth * m_iBPP * m_iLastRaster * plane_number);

    memset (myBuffer.pszCompressedData, 0, m_iWidth * m_iLastRaster * m_iBPP);
    HPLJJBGCompress (m_iWidth * 8 * m_iBPP, m_iLastRaster, &p, &myBuffer, &se);

    compressedsize = myBuffer.dwTotalSize;
    memcpy(compressBuf+compressedsize, &se, sizeof(se));
    iRastersReady = 1;
    m_iCurRasterPerPlane[plane_number] = 0;
    m_pszCurPtr = m_pszInputRasterData;
    int    buffer_size = m_iWidth * m_iLastRaster * m_iBPP;
    memset(p, 0, buffer_size);
    return;
}


/*****************************************************************************\
  LJFastRaster.cpp : Implementation of LJFastRaster class

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

#include "CommonDefinitions.h"
#include "LJFastRaster.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "ModeDeltaPlus.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"

LJFastRaster::LJFastRaster() : Encapsulator()
{
    memset(&m_PM, 0, sizeof(m_PM));
    strcpy(m_szLanguage, "PCLXL");
    m_pModeDeltaPlus = NULL;
}

LJFastRaster::~LJFastRaster()
{
}

DRIVER_ERROR LJFastRaster::addJobSettings()
{
    cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
        "@PJL SET RESOLUTION=600\012@PJL SET BITSPERPIXEL=1\012@PJL SET TIMEOUT=900\012");
    cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
        "@PJL SET PAGEPROTECT=AUTO\012@PJL SET DENSITY=5\012");
    if (m_pQA->print_quality == -1)
    {
        cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
        "@PJL SET RET=OFF\012@PJL SET ECONOMODE=ON\012");
    }
    addToHeader("%s", "@PJL ENTER LANGUAGE=PCLXL\012) HP-PCL XL;2;0;Comment\012");
    addToHeader(FRBeginSession, sizeof(FRBeginSession));
    addToHeader(FRVUExtn3, sizeof(FRVUExtn3));
    addToHeader(FRVendorUniq, sizeof(FRVendorUniq));
    addToHeader(FROpenDataSource, sizeof(FROpenDataSource));
    return NO_ERROR;
}

DRIVER_ERROR LJFastRaster::Configure(Pipeline **pipeline)
{
    DRIVER_ERROR    err = NO_ERROR;
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    ColorMatcher *pColorMatcher;
    Halftoner    *pHalftoner;
    int          iRows[MAXCOLORPLANES];
    unsigned int uiResBoost;
    head = *pipeline;

/*
 *  I need a flag in the printmode structure to whether create a CMYGraymap
 *  and set the ulMap1 to it.
 */

    m_PM.dyeCount = 1;
    m_PM.ColorDepth[0] = 1;
    m_PM.cmap.ulMap1 = ulMapDJ600_CCM_K;
    m_PM.BaseResX = m_pQA->horizontal_resolution;
    m_PM.BaseResY = m_pQA->vertical_resolution;
    m_PM.eHT = FED;
    m_PM.BlackFEDTable = HTBinary_open;
    m_PM.ColorFEDTable = HTBinary_open;
    m_PM.MixedRes = false;

    for (int i = 0; i < MAXCOLORPLANES; i++)
    {
        m_PM.ResolutionX[i] = m_pQA->horizontal_resolution;
        m_PM.ResolutionY[i] = m_pQA->vertical_resolution;
        iRows[i] = m_PM.ResolutionX[i] / m_PM.BaseResX;
    }
    uiResBoost = m_PM.BaseResX / m_PM.BaseResY;
    if (uiResBoost == 0)
        uiResBoost = 1;

    width = m_pMA->printable_width;

    pColorMatcher = new ColorMatcher(m_PM.cmap, m_PM.dyeCount, width);
    head = new Pipeline(pColorMatcher);
    pHalftoner = new Halftoner (&m_PM, width, iRows, uiResBoost, m_PM.eHT == MATRIX);
    p = new Pipeline(pHalftoner);
    head->AddPhase(p);
    m_pModeDeltaPlus = new ModeDeltaPlus(width);
    p = new Pipeline(m_pModeDeltaPlus);
    head->AddPhase(p);
    m_pModeDeltaPlus->myplane = COLORTYPE_COLOR;
    err = m_pModeDeltaPlus->Init();

    *pipeline = head;
    return err;
}

DRIVER_ERROR LJFastRaster::StartPage(JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;
    page_number++;

    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;

    m_pModeDeltaPlus->NewPage();

/*     Orienatation: is FrFeedOrientationSeq[1]. Can take the following values:
 *      Portrait                : 0x00
 *      Landscape:              : 0x01
 *      Reversed    Portrait    : 0x02
 *      Reversed    Landscape   : 0x03
 *      Image       Orientataion: 0x04
 */

    addToHeader(FRFeedOrientation, sizeof(FRFeedOrientation));

//  Set media source, type, size and quality modes.

    if (m_pMA->pcl_id == 96)    // custom paper size
    {
        BYTE    szCustomSize[16];
        union
        {
            float       fValue;
            uint32_t    uiValue;
        } LJFUnion;
        uint32_t    uiXsize;
        uint32_t    uiYsize;
        int         k = 0;
        LJFUnion.fValue = (float) m_pMA->physical_width / m_pQA->horizontal_resolution;
        uiXsize = LJFUnion.uiValue;
        LJFUnion.fValue = (float) m_pMA->physical_height / m_pQA->horizontal_resolution;
        uiYsize = LJFUnion.uiValue;
        szCustomSize[k++] = 0xD5;
        szCustomSize[k++] = (BYTE) (uiXsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0xFF000000) >> 24);
        szCustomSize[k++] = (BYTE) (uiYsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0xFF000000) >> 24);
        szCustomSize[k++] = 0xF8;
        szCustomSize[k++] = 0x2F;
        addToHeader(szCustomSize, k);
        addToHeader(FRCustomMediaSize, sizeof(FRCustomMediaSize));
    }
    else
    {
        memcpy(cur_pcl_buffer_ptr, FRPaperSize, sizeof(FRPaperSize));
        cur_pcl_buffer_ptr[1] = (BYTE) m_pMA->pcl_id;
        cur_pcl_buffer_ptr += sizeof(FRPaperSize);
    }

    addToHeader(FRBeginPage, sizeof(FRBeginPage));
    err = Cleanup();
    return err;
}

DRIVER_ERROR LJFastRaster::SendCAPy(int iOffset)
{
    return NO_ERROR;
}

DRIVER_ERROR LJFastRaster::FormFeed()
{
    return sendBuffer(FREndPage, sizeof(FREndPage));
}

DRIVER_ERROR LJFastRaster::EndJob()
{
    DRIVER_ERROR    err;
    addToHeader(FRCloseDataSource, sizeof(FRCloseDataSource));
    addToHeader(FREndSession, sizeof(FREndSession));
    addToHeader((const BYTE *) PJLExit, strlen(PJLExit));
    err = Cleanup();
    return err;
}

#define        FAST_RASTER_HEADERSIZE    25

typedef enum
{
    eDelta32,
    eDeltaPlus = 24,
    eFX = 18,
    eRAW = 2
} CompressionMethod;

//** Faster Raster Path Header address values

#define        BASE_ADDRESS                     0
#define        PAGE_NUM_ADDRESS                 1
#define        RESOLUTION_ADDRESS_HI            2
#define        RESOLUTION_ADDRESS_LO            3
#define        COMPRESSION_ADDRESS_HI           4
#define        COMPRESSION_ADDRESS_LO           5
#define        COLOR_PLANE_SPECIFIER_ADDRESS    6
#define        COMPRESSION_RATIO                7
#define        PRODUCT_ID                       8
#define        IMAGE_SIZE_ADDRESS_HIWORD_HI    12
#define        IMAGE_SIZE_ADDRESS_HIWORD_LO    13
#define        IMAGE_SIZE_ADDRESS_LOWORD_HI    14
#define        IMAGE_SIZE_ADDRESS_LOWORD_LO    15
#define        IMAGE_WIDTH_ADDRESS_HI          16
#define        IMAGE_WIDTH_ADDRESS_LO          17
#define        IMAGE_HEIGTH_ADDRESS_HI         18
#define        IMAGE_HEIGTH_ADDRESS_LO         19
#define        ABS_X_ADDRESS_HI                20
#define        ABS_X_ADDRESS_LO                21
#define        ABS_Y_ADDRESS_HI                22
#define        ABS_Y_ADDRESS_LO                23
#define        BIT_DEPTH_ADDRESS               24

#define eK 3
DRIVER_ERROR LJFastRaster::Encapsulate (RASTERDATA *InputRaster, bool bLastPlane)
{
    BYTE    res[64];
    DRIVER_ERROR    err = NO_ERROR;

    //** form FR header
    unsigned char    pucHeader[FAST_RASTER_HEADERSIZE];
    long lImageWidth = ((m_pMA->printable_width + 7) / 8) * 8;
    long lResolution = 600;
    long lBlockOffset = ((m_pModeDeltaPlus->GetCurrentRasterRow() + 127) / 128) * 128 - 128;
    long lBitDepth = 1;
    long lBlockHeight = m_pModeDeltaPlus->GetCurrentBlockHeight();

    unsigned short wTemp = LOWORD (lBlockOffset);
    BYTE byHIByte = 0;
    BYTE byLOByte = 0;

    memset (pucHeader, 0, FAST_RASTER_HEADERSIZE);

    pucHeader[ABS_X_ADDRESS_HI] = 0;
    pucHeader[ABS_X_ADDRESS_LO] = 0;
    pucHeader[ABS_Y_ADDRESS_HI] = HIBYTE (wTemp);
    pucHeader[ABS_Y_ADDRESS_LO] = LOBYTE (wTemp);

    pucHeader[BASE_ADDRESS] = 0;
    pucHeader[PAGE_NUM_ADDRESS] = 1;

    wTemp = (unsigned short) (lResolution );
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);
    pucHeader[RESOLUTION_ADDRESS_HI] = byHIByte;
    pucHeader[RESOLUTION_ADDRESS_LO] = byLOByte;
    
    wTemp = m_pModeDeltaPlus->IsCompressed() ? (unsigned short) eDeltaPlus : (unsigned short) eRAW;
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);
    pucHeader[COMPRESSION_ADDRESS_HI] = byHIByte;
    pucHeader[COMPRESSION_ADDRESS_LO] = byLOByte;

    pucHeader[COLOR_PLANE_SPECIFIER_ADDRESS]    = (BYTE) eK;
    pucHeader[COMPRESSION_RATIO]                = (BYTE) m_pModeDeltaPlus->GetFRatio();
    wTemp = HIWORD (InputRaster->rastersize[COLORTYPE_COLOR]);
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);
    pucHeader[IMAGE_SIZE_ADDRESS_HIWORD_HI] = byHIByte;
    pucHeader[IMAGE_SIZE_ADDRESS_HIWORD_LO] = byLOByte;
    
    wTemp = LOWORD (InputRaster->rastersize[COLORTYPE_COLOR]);
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);
    pucHeader[IMAGE_SIZE_ADDRESS_LOWORD_HI] = byHIByte;
    pucHeader[IMAGE_SIZE_ADDRESS_LOWORD_LO] = byLOByte;

    wTemp = LOWORD (lImageWidth * 8);
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);

    pucHeader[IMAGE_WIDTH_ADDRESS_HI] = byHIByte;
    pucHeader[IMAGE_WIDTH_ADDRESS_LO] = byLOByte;
    
    wTemp = LOWORD (lBlockHeight);
    byHIByte = HIBYTE (wTemp);
    byLOByte = LOBYTE (wTemp);
    pucHeader[IMAGE_HEIGTH_ADDRESS_HI] = byHIByte;
    pucHeader[IMAGE_HEIGTH_ADDRESS_LO] = byLOByte;
                
    wTemp = LOWORD (lBitDepth);
    pucHeader[BIT_DEPTH_ADDRESS] = LOBYTE (wTemp);

    unsigned int   ulVUDataLength = (int)(InputRaster->rastersize[COLORTYPE_COLOR] + FAST_RASTER_HEADERSIZE);

    err = this->Send (FREnterFRMode, sizeof(FREnterFRMode));
    ERRCHECK;
    res[0] = (BYTE) (ulVUDataLength & 0xFF);
    res[1] = (BYTE) ((ulVUDataLength & 0x0000FF00) >> 8);
    res[2] = (BYTE) ((ulVUDataLength & 0x00FF0000) >> 16);
    res[3] = (BYTE) ((ulVUDataLength & 0xFF000000) >> 24);
    res[4] = 0xF8;
    res[5] = 0x92;
    res[6] = 0x46;
    err = this->Send (res, 7);
    ERRCHECK;

    //** now embed raster data, header and all     
    err = this->Send (pucHeader, FAST_RASTER_HEADERSIZE);
    err = Cleanup();
    if (err == NO_ERROR)
        err = sendBuffer ((const BYTE *) InputRaster->rasterdata[COLORTYPE_COLOR],
                          InputRaster->rastersize[COLORTYPE_COLOR]);
    return err;
}


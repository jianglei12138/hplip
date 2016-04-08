/*****************************************************************************\
  LJMono.cpp : Implementation of LJMono class

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
#include "LJMono.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "Mode2.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"

LJMono::LJMono() : Encapsulator()
{
    memset(&m_PM, 0, sizeof(m_PM));
    strcpy(m_szLanguage, "PCL");
}

LJMono::~LJMono()
{
}

DRIVER_ERROR LJMono::addJobSettings()
{
    cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
        "@PJL SET PAGEPROTECT=AUTO\012@PJL SET RESOLUTION=%d\012@PJL SET DENSITY=5\012", m_pQA->horizontal_resolution);
    if (m_pQA->print_quality == -1)
    {
        cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
                              "@PJL SET RET=OFF\012@PJL SET ECONOMODE=ON\012");
    }
    if (m_pJA->e_duplex_mode != DUPLEXMODE_NONE)
    {
        cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr,
            "@PJL SET DUPLEX=ON\012@PJL SET BINDING=%s\012",
            (m_pJA->e_duplex_mode == DUPLEXMODE_BOOK) ? "LONGEDGE" :
                                                        "SHORTEDGE");
    }
    else
    {
        addToHeader("@PJL SET DUPLEX=OFF\012", 19);
    }
    cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr, "@PJL ENTER LANGUAGE=%s\012", m_szLanguage);
    sendJobHeader();
    DRIVER_ERROR err = Cleanup();
    return err;
}

DRIVER_ERROR LJMono::Configure(Pipeline **pipeline)
{
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    ColorMatcher *pColorMatcher;
    Mode2        *pMode2;
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
//    unsigned int SeedBufferSize = m_pMA->printable_width * 3;

    pColorMatcher = new ColorMatcher(m_PM.cmap, m_PM.dyeCount, width);
    head = new Pipeline(pColorMatcher);
    m_pHalftoner = new Halftoner (&m_PM, width, iRows, uiResBoost, m_PM.eHT == MATRIX);
    p = new Pipeline(m_pHalftoner);
    head->AddPhase(p);
    pMode2 = new Mode2(width);
    p = new Pipeline(pMode2);
    head->AddPhase(p);
    pMode2->myplane = COLORTYPE_COLOR;

    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR LJMono::StartPage(JobAttributes *pJA)
{
    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;
    page_number++;
    return NO_ERROR;
}

DRIVER_ERROR LJMono::FormFeed()
{
    DRIVER_ERROR    err;
    err = Cleanup();
    err = m_pSystemServices->Send((const BYTE *) "\x0C", 1);
    return err;
}

DRIVER_ERROR LJMono::EndJob()
{
    DRIVER_ERROR    err = NO_ERROR;
    err = Cleanup();
    err = m_pSystemServices->Send((const BYTE *) "\x1B*rC", 4);
    err = m_pSystemServices->Send(Reset, sizeof(Reset));
    if (err == NO_ERROR)
        err = m_pSystemServices->Send(UEL, sizeof(UEL));
    return err;
}

DRIVER_ERROR LJMono::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err = NO_ERROR;
    char            tmpStr[16];
    int             iLen;
    BYTE            c = m_pHalftoner->LastPlane() ? 'W' : 'V';
    iLen = sprintf (tmpStr, "\x1b*b%u%c", InputRaster->rastersize[COLORTYPE_COLOR], c);
    err = this->Send((const BYTE *) tmpStr, iLen);
    if (err == NO_ERROR && InputRaster->rastersize[COLORTYPE_COLOR] > 0)
    {
        err = this->Send(InputRaster->rasterdata[COLORTYPE_COLOR],
                         InputRaster->rastersize[COLORTYPE_COLOR]);
    }
    return err;
}


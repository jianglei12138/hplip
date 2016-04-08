/*****************************************************************************\
  LJColor.cpp : Implementation of LJColor class

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
#include "LJColor.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "Mode2.h"
#include "Mode3.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"

LJColor::LJColor() : Encapsulator()
{
    memset(&m_PM, 0, sizeof(m_PM));
    strcpy(m_szLanguage, "PCL");
}

LJColor::~LJColor()
{
}

DRIVER_ERROR LJColor::addJobSettings()
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
    cur_pcl_buffer_ptr += sprintf((char *) cur_pcl_buffer_ptr, "@PJL ENTER LANGUAGE=PCL\012");
    sendJobHeader();
    DRIVER_ERROR err = Cleanup();
    return err;
}

DRIVER_ERROR LJColor::Configure(Pipeline **pipeline)
{
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    head = *pipeline;
    width = m_pMA->printable_width;

/*
 *  I need a flag in the printmode structure to whether create a CMYGraymap
 *  and set the ulMap1 to it.
 */


//  color_mode: 0 - color, 1 - grey_cmy, 2 - grey_k

    m_PM.BaseResX = m_pQA->horizontal_resolution;
    m_PM.BaseResY = m_pQA->vertical_resolution;
    if (m_pJA->color_mode != 0)
    {
        ColorMatcher *pColorMatcher;
        Mode2        *pMode2;
        int          iRows[MAXCOLORPLANES];
        unsigned int uiResBoost;
        m_PM.dyeCount = 1;
        m_PM.ColorDepth[0] = 1;
        m_PM.cmap.ulMap1 = ulMapDJ600_CCM_K;
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

        pColorMatcher = new ColorMatcher(m_PM.cmap, m_PM.dyeCount, width);
        head = new Pipeline(pColorMatcher);
        m_pHalftoner = new Halftoner (&m_PM, width, iRows, uiResBoost, m_PM.eHT == MATRIX);
        p = new Pipeline(m_pHalftoner);
        head->AddPhase(p);
        pMode2 = new Mode2(width);
        p = new Pipeline(pMode2);
        head->AddPhase(p);
        pMode2->myplane = COLORTYPE_COLOR;
    }
    else
    {
        m_pMode3 = new Mode3(width * 3);
        head = new Pipeline(m_pMode3);
        m_pMode3->myplane = COLORTYPE_COLOR;
    }

    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR LJColor::StartPage(JobAttributes *pJA)
{
    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;
    page_number++;
    return NO_ERROR;
}

DRIVER_ERROR LJColor::FormFeed()
{
    DRIVER_ERROR    err;
    err = Cleanup();
    err = m_pSystemServices->Send((const BYTE *) "\x0C", 1);
    return err;
}

DRIVER_ERROR LJColor::EndJob()
{
    DRIVER_ERROR    err = NO_ERROR;
    err = Cleanup();
    err = m_pSystemServices->Send((const BYTE *) "\x1B*rC", 4);
    err = m_pSystemServices->Send(Reset, sizeof(Reset));
    if (err == NO_ERROR)
        err = m_pSystemServices->Send(UEL, sizeof(UEL));
    return err;
}

DRIVER_ERROR LJColor::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err = NO_ERROR;
    char            tmpStr[16];
    int             iLen;
    m_iYPos++;
    iLen = sprintf (tmpStr, "\x1b*b%uW", InputRaster->rastersize[COLORTYPE_COLOR]);
    err = this->Send((const BYTE *) tmpStr, iLen);
    if (err == NO_ERROR && InputRaster->rastersize[COLORTYPE_COLOR] > 0)
    {
        err = this->Send(InputRaster->rasterdata[COLORTYPE_COLOR],
                         InputRaster->rastersize[COLORTYPE_COLOR]);
    }

/*
 *  Printers with low memory (64 MB or less) can run out of memory during decompressing
 *  the image data and will abort the job. To prevent this, restart raster command.
 *  Raghu
 */

    if (m_pJA->color_mode == 0 &&
        m_pQA->horizontal_resolution >= 600 &&
        m_iYPos % 1200 == 0)
    {
        // Reset seed our seed row
        m_pMode3->Flush();
        err = this->Send ((const BYTE *) "\033*rC\033*r1A\033*b3M", 14);
    }

    return err;
}

void LJColor::configureRasterData()
{

/*
 *  Configure image data - ESC*v#W - # = 6 bytes
 *  02 - RGB colorspace (00 - Device RGB)
 *  03 - Direct pixel
 *  08 - bits per index - ignored for direct pixel
 *  08, 08, 08 - bits per primary each
 */

    addToHeader ((const BYTE *) "\033*v6W\00\03\010\010\010\010", 11);

//  Continues tone dither
//  Logical operation - 0

    addToHeader ((const BYTE *) "\033*t18J", 6);

/*
 *  Driver Configuration Command - ESC*#W - # = 3 bytes
 *  device id - 6 = color HP LaserJet Printer
 *  func index - 4 = Select Colormap
 *  argument - 2   = Vivid Graphics
 */

    addToHeader ((const BYTE *) "\033*o3W\06\04\06", 8);

/*
 *  Program color palette entries
 */
    addToHeader ((const BYTE *) "\033*v255A\033*v255B\033*v255C\033*v0I", 26);
    addToHeader ((const BYTE *) "\033*v255A\033*v0B\033*v0C\033*v6I", 22);
    addToHeader ((const BYTE *) "\033*v0A\033*v255B\033*v0C\033*v5I", 22);
    addToHeader ((const BYTE *) "\033*v0A\033*v0B\033*v255C\033*v3I", 22);
    addToHeader ((const BYTE *) "\033*v255A\033*v255B\033*v0C\033*v4I", 24);
    addToHeader ((const BYTE *) "\033*v255A\033*v0B\033*v255C\033*v2I", 24);
    addToHeader ((const BYTE *) "\033*v0A\033*v255B\033*v255C\033*v1I", 24);
    addToHeader ((const BYTE *) "\033*v0A\033*v0B\033*v0C\033*v7I", 20);

//  Foreground color

    addToHeader ((const BYTE *) "\033*v7S", 5);
}


/*****************************************************************************\
  Pcl3.cpp : Implementation of Pcl3 class

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
#include "Pcl3.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "Mode9.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"
#include "Pcl3PrintModes.h"

Pcl3::Pcl3() : Encapsulator()
{
    m_pPM = NULL;
    strcpy(m_szLanguage, "PCL3");
}

Pcl3::~Pcl3()
{
}

DRIVER_ERROR Pcl3::Configure(Pipeline **pipeline)
{
    Pipeline    *p = NULL;
    Pipeline    *head;
    unsigned int width;
    ColorMatcher *pColorMatcher;
    Mode9       *pMode9;
    int          iRows[MAXCOLORPLANES];
    unsigned int uiResBoost;
    head = *pipeline;

/*
 *  I need a flag in the printmode structure to whether create a CMYGraymap
 *  and set the ulMap1 to it.
 */

    if (!selectPrintMode())
    {
        dbglog("selectPrintMode failed, PrintMode name = %s\n", m_pQA->print_mode_name);
        return UNSUPPORTED_PRINTMODE;
    }
    if (m_pPM->BaseResX != m_pQA->horizontal_resolution ||
        m_pPM->BaseResY != m_pQA->vertical_resolution)
    {
        dbglog("Requested resolution not supported with requested printmode\n");
	return UNSUPPORTED_PRINTMODE;
    }

    for (int i = 0; i < MAXCOLORPLANES; i++)
    {
        iRows[i] = m_pPM->ResolutionX[i] / m_pPM->BaseResX;
    }
    uiResBoost = m_pPM->BaseResX / m_pPM->BaseResY;
    if (uiResBoost == 0)
        uiResBoost = 1;

    width = m_pMA->printable_width;
//    unsigned int SeedBufferSize = m_pMA->printable_width * 3;

    pColorMatcher = new ColorMatcher(m_pPM->cmap, m_pPM->dyeCount, width);
    head = new Pipeline(pColorMatcher);
    m_pHalftoner = new Halftoner (m_pPM, width, iRows, uiResBoost, m_pPM->eHT == MATRIX);
    p = new Pipeline(m_pHalftoner);
    head->AddPhase(p);
    pMode9 = new Mode9(width, false);
    p = new Pipeline(pMode9);
    head->AddPhase(p);
    pMode9->myplane = COLORTYPE_COLOR;

    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR Pcl3::StartPage(JobAttributes *pJA)
{
    DRIVER_ERROR    err = NO_ERROR;
    char            szStr[256];
    int             top_margin = 0;
    page_number++;

//  Under windows, pJA address may have changed, re-init here.

    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;

//  Set media source, type, size and quality modes.

    sprintf(szStr, "\033&l%dH\033&l%dM\033&l%dA\033*o%dM", m_pJA->media_source, m_pQA->media_type,
            m_pMA->pcl_id, m_pQA->print_quality);

//  Now add media subtype

    int    i = strlen(szStr);
    memcpy(szStr+i, MediaSubtypeSeq, sizeof(MediaSubtypeSeq));
    i += sizeof(MediaSubtypeSeq);
    szStr[i++] = (char) (m_pQA->media_subtype & 0xFFFF) >> 8;
    szStr[i++] = (char) m_pQA->media_subtype & 0xFF;
    addToHeader((const BYTE *) szStr, i);

    configureRasterData();

    addToHeader("\033&u%dD\033*t%dR", m_pPM->BaseResX, m_pPM->BaseResY);

    // another command that helps PCLviewer
    unsigned int width = m_pMA->printable_width;
    addToHeader("\033*r%dS", width);

/*
 *  Custom papersize command
 */

    if (m_pMA->pcl_id == CUSTOM_MEDIA_SIZE) {
        short   sWidth, sHeight;
        BYTE    b1, b2;
        sWidth  = static_cast<short>(m_pMA->physical_width);
        sHeight = static_cast<short>(m_pMA->physical_height);
        memcpy (szStr, "\x1B*o5W\x0E\x05\x00\x00\x00\x1B*o5W\x0E\x06\x00\x00\x00", 20);
        b1 = (BYTE) ((sWidth & 0xFF00) >> 8);
        b2 = (BYTE) (sWidth & 0xFF);
        szStr[8] = b1;
        szStr[9] = b2;
        b1 = (BYTE) ((sHeight & 0xFF00) >> 8);
        b2 = (BYTE) (sHeight & 0xFF);
        szStr[18] = b1;
        szStr[19] = b2;
        addToHeader((const BYTE *) szStr, 20);
    }

    if (m_pPM->dyeCount != 3)
    {
        addToHeader((const BYTE *) black_extract_off, sizeof(black_extract_off));
    }

    addToHeader("\x1B&l0L", 5);    // perforation skip off

/*
 *  Calculate top margin:
 *     This is given in terms of text line spacing, the smallest available
 *     granularity of which is 48.
 *     There is a starting unprintable distance, which is platform dependent.
 *  So our formula is: marginvalue = granularity * (startY/verticalres - unprintable)
 */

    top_margin = m_pMA->printable_start_y - ((m_pJA->mech_offset * m_pQA->actual_vertical_resolution)/1000);
    top_margin = (top_margin * 48) / m_pQA->actual_vertical_resolution;
    addToHeader("\x1B&l48D", 6);    // granularity
    addToHeader("\x1B&l%dE", top_margin);
    addToHeader((const BYTE *) grafStart, sizeof(grafStart));
    addToHeader((const BYTE *) grafMode9, sizeof(grafMode9));
    addToHeader((const BYTE *) seedSame, sizeof(seedSame));

    addToHeader("\x1b*p0Y", 5);

//  Now send media pre-load command
    addToHeader((const BYTE *) "\033&l-2H", 6);

    err = Cleanup();
    return err;
}

DRIVER_ERROR Pcl3::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
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

int  Pcl3::colorLevels(int color_plane)
{
    int    bit_depth = m_pPM->ColorDepth[color_plane];
    int    levels = 1;
    for (int i = 0; i < bit_depth; i++)
    {
        levels *= 2;
    }
    return levels;
}

void Pcl3::configureRasterData()
{
    if (m_pPM == NULL)
    {
        dbglog("configureRasterData: m_pPM is NULL\n");
        return;
    }
    BYTE    *p = cur_pcl_buffer_ptr;

    *p++ = 0x1b;
    *p++ = '*';
    *p++ = 'g';
    p += sprintf((char *) p, "%d", m_pPM->dyeCount * MAXCOLORPLANES + 2);
    *p++ = 'W';
    *p++ = 2;    // crdFormat
    *p++ = m_pPM->dyeCount;
    int    start = K;
    if (m_pPM->dyeCount == 3)
        start = C;

    int    depth;
    for (unsigned int color = start; color < (start + m_pPM->dyeCount); color++)
    {
        *p++ = m_pPM->ResolutionX[color] / 256;
        *p++ = m_pPM->ResolutionX[color] % 256;
        *p++ = m_pPM->ResolutionY[color] / 256;
        *p++ = m_pPM->ResolutionY[color] % 256;
        depth = colorLevels(color);
        *p++ = depth / 256;
        *p++ = depth % 256;
    }

    cur_pcl_buffer_ptr = p;
} //configureRasterData

bool Pcl3::selectPrintMode()
{
    if (m_pJA->printer_platform[0] == 0)
    {
        dbglog("printer_platform is undefined\n");
        return false;
    }
    for (unsigned int i = 0; i < sizeof(pcl3_print_modes_table) / sizeof(pcl3_print_modes_table[0]); i++)
    {
        if (!strcmp(m_pJA->printer_platform, pcl3_print_modes_table[i].printer_platform_name))
	{
	    return selectPrintMode(i);
	}
    }
    dbglog("Unsupported printer_platform: %s\n", m_pJA->printer_platform);
    return false;
}

bool Pcl3::selectPrintMode(int index)
{
    PrintMode    *p = pcl3_print_modes_table[index].print_modes;
    for (int i = 0; i < pcl3_print_modes_table[index].count; i++, p++)
    {
        if (!strcmp(m_pJA->quality_attributes.print_mode_name, p->name))
        {
            m_pPM = p;
            return true;
        }
    }
    return false;
}


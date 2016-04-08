/*****************************************************************************\
  LJZxStream.cpp : Implementation for the LJZxStream class

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
#include "Pipeline.h"
#include "Encapsulator.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "ModeJbig.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"
#include "LJZxStream.h"
#include "Utils.h"
#include "hpjbig_wrapper.h"

LJZxStream::LJZxStream () : Encapsulator ()
{
    memset(&m_PM, 0, sizeof(m_PM));
    strcpy(m_szLanguage, "ZJS");
    m_pModeJbig = NULL;
}

LJZxStream::~LJZxStream()
{
}

DRIVER_ERROR LJZxStream::addJobSettings()
{
    char    szStr[256];
    strcpy (szStr, "\x1B\x25-12345X@PJL JOB\x0D\x0A");
    strcpy (szStr+strlen (szStr), "@PJL SET JAMRECOVERY=OFF\x0D\x0A");
    strcpy (szStr+strlen (szStr), "@PJL SET DENSITY=3\x0D\x0A");
    strcpy (szStr+strlen (szStr), "@PJL SET RET=MEDIUM\x0D\x0A");
    strcpy (szStr+strlen (szStr), "@PJL SET ECONOMODE=");
    if (m_pQA->print_quality == -1)
    {
        strcpy (szStr+strlen (szStr), "ON\x0D\x0A");
    }
    else
    {
        strcpy (szStr+strlen (szStr), "OFF\x0D\x0A");
    }
    addToHeader ((const BYTE *) szStr, strlen (szStr));

    strcpy (szStr, "\x1B\x25-12345X,XQX");
    addToHeader ((const BYTE *) szStr, strlen (szStr));
    memset (szStr, 0x0, 92);
    szStr[3] = 0x01;
    szStr[7] = 0x07;
    int i = 8;
    i += SendIntItem ((BYTE *) szStr+i, 0x80000000, 0x04, 0x54);
    i += SendIntItem ((BYTE *) szStr+i, 0x10000005, 0x04, 0x01);
    i += SendIntItem ((BYTE *) szStr+i, 0x10000001, 0x04, 0x00);
    i += SendIntItem ((BYTE *) szStr+i, 0x10000002, 0x04, 0x00);
    i += SendIntItem ((BYTE *) szStr+i, 0x10000000, 0x04, 0x00);
    i += SendIntItem ((BYTE *) szStr+i, 0x10000003, 0x04, 0x01);
    i += SendIntItem ((BYTE *) szStr+i, 0x80000001, 0x04, 0xDEADBEEF);
    addToHeader ((const BYTE *) szStr, i);
    DRIVER_ERROR err = Cleanup();
    return err;
}

DRIVER_ERROR LJZxStream::Configure(Pipeline **pipeline)
{
    DRIVER_ERROR err;
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
    m_pModeJbig = new ModeJbig(width);
    p = new Pipeline(m_pModeJbig);
    head->AddPhase(p);
    m_pModeJbig->myplane = COLORTYPE_COLOR;
    err = m_pModeJbig->Init(m_pMA->printable_height, 1, 2, ZXSTREAM);

    *pipeline = head;
    return err;
}

DRIVER_ERROR LJZxStream::StartPage (JobAttributes *pJA)
{
    DRIVER_ERROR    err;
    BYTE    szStr[256];
    int     i;
    int    iOutputResolution = m_pJA->integer_values[1];
    int    width;

    width = ((m_pMA->printable_width + 31) / 32) * 32;
    if (m_pQA->print_quality == 0)
        iOutputResolution = m_pQA->vertical_resolution;
    memset (szStr, 0x0, sizeof (szStr));
    szStr[3] = 0x03;
    szStr[7] = 0x0F;
    err = sendBuffer ((const BYTE *) szStr, 8);
    i = 0;
    i += SendIntItem (szStr+i, 0x80000000, 0x04, 0xB4);
    i += SendIntItem (szStr+i, 0x20000005, 0x04, 0x01);
    i += SendIntItem (szStr+i, 0x20000006, 0x04, 0x07);
    i += SendIntItem (szStr+i, 0x20000000, 0x04, 0x01);
    i += SendIntItem (szStr+i, 0x20000007, 0x04, 0x01);
    i += SendIntItem (szStr+i, 0x20000008, 0x04, m_pQA->horizontal_resolution);
    i += SendIntItem (szStr+i, 0x20000009, 0x04, iOutputResolution);
    i += SendIntItem (szStr+i, 0x2000000D, 0x04, width * 2);
    i += SendIntItem (szStr+i, 0x2000000E, 0x04, m_pMA->printable_height);
    i += SendIntItem (szStr+i, 0x2000000A, 0x04, 2);
    i += SendIntItem (szStr+i, 0x2000000F, 0x04, width);
    i += SendIntItem (szStr+i, 0x20000010, 0x04, m_pMA->printable_height);
    i += SendIntItem (szStr+i, 0x20000011, 0x04, 0x01);
    i += SendIntItem (szStr+i, 0x20000001, 0x04, m_pMA->pcl_id);
    i += SendIntItem (szStr+i, 0x80000001, 0x04, 0xDEADBEEF);
    err = sendBuffer ((const BYTE *) szStr, i);
    return err;
}

DRIVER_ERROR LJZxStream::FormFeed ()
{
    BYTE                szStr[16];
    memset(szStr, 0, sizeof(szStr));
    szStr[3] = 0x06;
    szStr[11] = 0x04;
    return sendBuffer ((const BYTE *) szStr, 16);
}

DRIVER_ERROR LJZxStream::EndJob()
{
    char    szStr[64];
    memset(szStr, 0, sizeof(szStr));
    szStr[3] = 2;
    strcpy (szStr+8, "\x1B\x25-12345X@PJL EOJ\x0D\x0A\x1B\x25-12345X");
    return this->sendBuffer((const BYTE *) szStr, 8 + strlen(szStr+8));
}

DRIVER_ERROR    LJZxStream::Encapsulate (RASTERDATA *raster, bool bLastPlane)
{
    DRIVER_ERROR        err = NO_ERROR;
    BYTE                szStr[128];
    int                 i = 0;
    HPLJZjsJbgEncSt     *se = (HPLJZjsJbgEncSt *) (raster->rasterdata[COLORTYPE_COLOR] + raster->rastersize[COLORTYPE_COLOR]);

/*
 *  Send JBIG header info
 */

    memset (szStr, 0x0, 128);
    szStr[3] = 0x05;
    szStr[7] = 0x04;
    i = 8;
    i += SendIntItem (szStr+i, 0x80000000, 0x04, 0x40);
    i += SendIntItem (szStr+i, 0x40000000, 0x04, 0x00);
    i += SendIntItem (szStr+i, 0x40000002, 0x14, 0x00);
    i -= 4;
    szStr[i++] = se->dl;
    szStr[i++] = se->d;
    szStr[i++] = se->planes;
    szStr[i++] = 0;
    for (int j = 3; j >= 0; j--)
    {
        szStr[i] = (BYTE) ((se->xd  >> (8 * j)) & 0xFF);
        szStr[4+i] = (BYTE) ((se->yd  >> (8 * j)) & 0xFF);
        szStr[8+i] = (BYTE) ((se->l0  >> (8 * j)) & 0xFF);
        i++;
    }
    i = 56;
    szStr[i++] = se->mx;
    szStr[i++] = se->my;
    szStr[i++] = se->order;
    szStr[i++] = se->options;
    i += SendIntItem (szStr+i, 0x80000001, 0x04, 0xDEADBEEF);

    BYTE    *p = raster->rasterdata[COLORTYPE_COLOR] + 20;
    int     iDataSize = raster->rastersize[COLORTYPE_COLOR] - 20;
    char    szTmpStr[32];
    memset (szTmpStr, 0x0, 32);
    szTmpStr[3] = 0x07;
    szTmpStr[5] = 0x01;
    szTmpStr[19] = 0x06;
    szTmpStr[27] = 0x04;
    while (iDataSize > 65536)
    {
        err = sendBuffer ((const BYTE *) szStr, i);
        ERRCHECK;
        err = sendBuffer ((const BYTE *) szTmpStr, 8);
        ERRCHECK;
        err = sendBuffer ((const BYTE *) p, 65536);
        ERRCHECK;
        iDataSize -= 65536;
        p += 65536;
//        err = sendBuffer ((const BYTE *) szTmpStr+16, 16);
    }
    if (iDataSize > 0)
    {
        err = sendBuffer ((const BYTE *) szStr, i);
        ERRCHECK;
        szTmpStr[5] = (char) ((iDataSize >> 16) & 0xFF);
        szTmpStr[6] = (char) ((iDataSize >> 8) & 0xFF);
        szTmpStr[7] = (char) (iDataSize & 0xFF);
        err = sendBuffer ((const BYTE *) szTmpStr, 8);
        ERRCHECK;
        err = sendBuffer ((const BYTE *) p, iDataSize);
        ERRCHECK;
//        err = sendBuffer ((const BYTE *) szTmpStr+16, 16);
    }

    return err;
}


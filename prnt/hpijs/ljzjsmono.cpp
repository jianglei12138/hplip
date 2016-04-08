/*****************************************************************************\
  ljzjsmono.cpp : Implementation for the LJZjsMono class

  Copyright (c) 1996 - 2007, HP Co.
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

#ifdef APDK_LJZJS_MONO

#include "header.h"
#include "io_defs.h"
#include "printerproxy.h"
#include "resources.h"
#include "ljzjs.h"
#include "ljzjsmono.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

extern uint32_t ulMapDJ600_CCM_K[9 * 9 * 9];

LJZjsMono::LJZjsMono (SystemServices* pSS, int numfonts, BOOL proto)
    : LJZjs (pSS, numfonts, proto)
{

    ePen = BLACK_PEN;

    pMode[GRAYMODE_INDEX]    = new LJZjsMonoDraftGrayMode ();
    pMode[DEFAULTMODE_INDEX] = new LJZjsMonoNormalGrayMode ();
    ModeCount = 2;

    CMYMap = NULL;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif
    m_pszInputRasterData = NULL;
    m_dwCurrentRaster = 0;
    m_cmColorMode = GREY_K;
    m_bStartPageSent = FALSE;
    m_iPlaneNumber = 0;
    m_iBPP = 1;
    for (int i = 1; i < 4; i++)
    {
        m_iP[i] = i - 1; //{3, 0, 1, 2};
    }
    m_iP[0] = 0;
    m_bIamColor = FALSE;
    m_iPrinterType = eLJZjsMono;
}

LJZjsMono::~LJZjsMono ()
{
}

LJZjsMonoDraftGrayMode::LJZjsMonoDraftGrayMode ()
: GrayMode(/*ulMapDJ600_CCM_K*/ulMapGRAY_K_6x6x1)
{

    ResolutionX[0] =
    ResolutionY[0] = 600;
    BaseResX =
    BaseResY = 600;
    MixedRes = FALSE;
    bFontCapable = FALSE;
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    Config.bCompress = FALSE;
    medium = mediaAuto;    // compatible with any media type
}

LJZjsMonoNormalGrayMode::LJZjsMonoNormalGrayMode ()
: GrayMode(/*ulMapDJ600_CCM_K*/ulMapGRAY_K_6x6x1)
{

    ResolutionX[0] =
    ResolutionY[0] = 600;
    BaseResX =
    BaseResY = 600;
    TextRes  = 600;
    MixedRes = FALSE;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    Config.bCompress = FALSE;
    medium = mediaAuto;    // compatible with any media type
}

DRIVER_ERROR LJZjsMono::Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane)
{
    if (pRasterData != NULL)
    {
        memcpy (m_pszCurPtr, pRasterData->rasterdata[COLORTYPE_COLOR],
                pRasterData->rastersize[COLORTYPE_COLOR]);
    }

    m_dwCurrentRaster++;
    m_pszCurPtr += m_dwWidth;
    if (m_dwCurrentRaster == m_dwLastRaster)
    {
        JbigCompress ();
    }
    return NO_ERROR;
}

DRIVER_ERROR LJZjsMono::EndPage ()
{
    DRIVER_ERROR        err = NO_ERROR;
    BYTE                szStr[16];

    SendChunkHeader (szStr, 16, ZJT_END_PAGE, 0);
    err = Send ((const BYTE *) szStr, 16);

    m_bStartPageSent = FALSE;

    m_dwCurrentRaster = 0;
    m_pszCurPtr = m_pszInputRasterData;

    return err;
}

DRIVER_ERROR    LJZjsMono::SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride)
{
    DRIVER_ERROR        err = NO_ERROR;
    BYTE                szStr[36];
    int                 i = 0;

/*
 *  Send JBIG header info
 */

    i = SendChunkHeader (szStr, 36, ZJT_JBIG_BIH, 0);

    memcpy (szStr + 16, pcBuff->pszCompressedData, 20);
    err = Send ((const BYTE *) szStr, 36);
    ERRCHECK;

    pcBuff->dwTotalSize -= 20;
    int     iPadCount = 0;
    if (pcBuff->dwTotalSize % 16)
    {
        iPadCount = ((pcBuff->dwTotalSize / 16 + 1) * 16) - pcBuff->dwTotalSize;
    }
    DWORD    dwTotal = pcBuff->dwTotalSize;
    BYTE     *p = pcBuff->pszCompressedData + 20;
    i = dwTotal / 65536;
    for (int j = 0; j < i; j++)
    {
        SendChunkHeader (szStr, 16 + 65536, ZJT_JBIG_HID, 0);
        err = Send ((const BYTE *) szStr, 16);
        err = Send ((const BYTE *) p, 65536);
        dwTotal -= 65536;
        p += 65536;
    }
    i = SendChunkHeader (szStr, 16 + dwTotal + iPadCount, ZJT_JBIG_HID, 0);
    err = Send ((const BYTE *) szStr, 16);
    err = Send ((const BYTE *) p, dwTotal);
    ERRCHECK;
    if (iPadCount != 0)
    {
        memset (szStr, 0, iPadCount);
        err = Send ((const BYTE *) szStr, iPadCount);
    }
    i = SendChunkHeader (szStr, 16, ZJT_END_JBIG, 0);
    err = Send ((const BYTE *) szStr, 16);
    return err;
}

DRIVER_ERROR LJZjsMono::VerifyPenInfo()
{
    ePen = BLACK_PEN;
    return NO_ERROR;
}

DRIVER_ERROR LJZjsMono::ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter)
{
    ePen = BLACK_PEN;

    return NO_ERROR;
}

APDK_END_NAMESPACE

#endif  // APDK_LJZJS_MONO

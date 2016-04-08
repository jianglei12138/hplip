/*****************************************************************************\
  ljm1005.cpp : Implementation for the LJM1005 class

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

#ifdef APDK_LJM1005

#include "header.h"
#include "io_defs.h"
#include "printerproxy.h"
#include "resources.h"
#include "ljzjs.h"
#include "ljm1005.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

extern uint32_t ulMapDJ600_CCM_K[9 * 9 * 9];

LJM1005::LJM1005 (SystemServices* pSS, int numfonts, BOOL proto)
    : LJZjs (pSS, numfonts, proto)
{

	ePen = BLACK_PEN;

    pMode[GRAYMODE_INDEX]    = new LJM1005DraftGrayMode ();
    pMode[DEFAULTMODE_INDEX] = new LJM1005NormalGrayMode ();
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
	m_iBPP = 2;
	for (int i = 1; i < 4; i++)
	{
		m_iP[i] = i - 1; //{3, 0, 1, 2};
	}
    m_iP[0] = 0;
    m_bIamColor = FALSE;
    m_iPrinterType = eLJM1005;
}

LJM1005::~LJM1005 ()
{
}

LJM1005DraftGrayMode::LJM1005DraftGrayMode ()
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
    Config.bCompress = FALSE;
}

LJM1005NormalGrayMode::LJM1005NormalGrayMode ()
: GrayMode(/*ulMapDJ600_CCM_K*/ulMapGRAY_K_6x6x1)
{

    ResolutionX[0] =
    ResolutionY[0] = 600;
    BaseResX =
    BaseResY = 600;
	TextRes  = 600;
    MixedRes = FALSE;
    bFontCapable = FALSE;
    Config.bCompress = FALSE;
}

DRIVER_ERROR LJM1005::Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane)
{
	if (pRasterData != NULL)
	{
        for (int i = 0; i < pRasterData->rastersize[COLORTYPE_COLOR]; i++)
        {
            m_pszCurPtr[i*m_iBPP]   = szByte1[pRasterData->rasterdata[COLORTYPE_COLOR][i]];
            m_pszCurPtr[i*m_iBPP+1] = szByte2[pRasterData->rasterdata[COLORTYPE_COLOR][i]];
            m_pszCurPtr[i*m_iBPP]   |= (m_pszCurPtr[i*m_iBPP] >> 1);
            m_pszCurPtr[i*m_iBPP+1] |= (m_pszCurPtr[i*m_iBPP+1] >> 1);
        }
	}

	m_dwCurrentRaster++;
	m_pszCurPtr += (m_iBPP * m_dwWidth);
    if (m_dwCurrentRaster == m_dwLastRaster)
    {
        JbigCompress ();
    }
    return NO_ERROR;
}

DRIVER_ERROR LJM1005::EndPage ()
{
    DRIVER_ERROR        err = NO_ERROR;
    BYTE                szStr[16];

    memset (szStr, 0, 16);
    szStr[3] = 0x06;
    szStr[11] = 0x04;
    err = Send ((const BYTE *) szStr, 16);

    m_bStartPageSent = FALSE;

    m_dwCurrentRaster = 0;
    m_pszCurPtr = m_pszInputRasterData;

    return err;
}

DRIVER_ERROR    LJM1005::SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride)
{
    DRIVER_ERROR        err = NO_ERROR;
    BYTE                szStr[128];
    int                 i = 0;

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

    BYTE    *p = pcBuff->pszCompressedData + 20;
    int     iDataSize = pcBuff->dwTotalSize - 20;
    char    szTmpStr[32];
    memset (szTmpStr, 0x0, 32);
    szTmpStr[3] = 0x07;
    szTmpStr[5] = 0x01;
    szTmpStr[19] = 0x06;
    szTmpStr[27] = 0x04;
    while (iDataSize > 65536)
    {
        err = Send ((const BYTE *) szStr, i);
        ERRCHECK;
        err = Send ((const BYTE *) szTmpStr, 8);
        ERRCHECK;
        err = Send ((const BYTE *) p, 65536);
        ERRCHECK;
        iDataSize -= 65536;
        p += 65536;
//        err = Send ((const BYTE *) szTmpStr+16, 16);
    }
    if (iDataSize > 0)
    {
        err = Send ((const BYTE *) szStr, i);
        ERRCHECK;
        szTmpStr[5] = (char) ((iDataSize >> 16) & 0xFF);
        szTmpStr[6] = (char) ((iDataSize >> 8) & 0xFF);
        szTmpStr[7] = (char) (iDataSize & 0xFF);
        err = Send ((const BYTE *) szTmpStr, 8);
        ERRCHECK;
        err = Send ((const BYTE *) p, iDataSize);
        ERRCHECK;
//        err = Send ((const BYTE *) szTmpStr+16, 16);
    }

    return err;
}

DRIVER_ERROR LJM1005::VerifyPenInfo()
{
    ePen = BLACK_PEN;
    return NO_ERROR;
}

DRIVER_ERROR LJM1005::ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter)
{
    ePen = BLACK_PEN;

    return NO_ERROR;
}

APDK_END_NAMESPACE

#endif  // APDK_LJM1005


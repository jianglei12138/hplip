/*****************************************************************************\
  ljzjs.cpp : Implementation for the LJZjs class

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


#if defined (APDK_LJZJS_MONO) || defined (APDK_LJZJS_COLOR) || defined (APDK_LJM1005)

#include "header.h"
#include "io_defs.h"
#include "printerproxy.h"
#include "resources.h"
#include "ljzjs.h"
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

extern "C"
{
int (*HPLJJBGCompress) (int iWidth, int iHeight, unsigned char **pBuff,
                        HPLJZjcBuff *pOutBuff, HPLJZjsJbgEncSt *pJbgEncSt);
int (*HPLJSoInit) (int iFlag);
}

APDK_BEGIN_NAMESPACE

#ifdef HAVE_LIBDL
extern void *LoadPlugin (const char *szPluginName);
#endif

const unsigned char LJZjs::szByte1[256] =
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
const unsigned char LJZjs::szByte2[256] =
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

LJZjs::LJZjs (SystemServices* pSS, int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    CMYMap = NULL;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif
    m_pszInputRasterData = NULL;
    m_dwCurrentRaster = 0;
    m_bStartPageSent = FALSE;
    HPLJJBGCompress = NULL;
    m_hHPLibHandle = NULL;
    m_iPrinterType = UNSUPPORTED;
#ifdef HAVE_LIBDL
    m_hHPLibHandle = LoadPlugin ("lj.so");
    if (m_hHPLibHandle)
    {
        dlerror ();
        *(void **) (&HPLJJBGCompress) = dlsym (m_hHPLibHandle, "hp_encode_bits_to_jbig");
        *(void **) (&HPLJSoInit) = dlsym (m_hHPLibHandle, "hp_init_lib");
        if (!HPLJSoInit || (HPLJSoInit && !HPLJSoInit (1)))
        {
            constructor_error = PLUGIN_LIBRARY_MISSING;
        }
    }
#endif
    if (HPLJJBGCompress == NULL)
    {
        constructor_error = PLUGIN_LIBRARY_MISSING;
    }
	//Issue: LJZJSMono class printers not printing in RHEL 
	//Cause: Since start page is common for LJZJSMono and LJZJSColor class, the items of 
	//LJZJSColor-2 format was used for LJZJSMono due to below variable not initialised 
	//Fix: Added initialisation so that correct LJZJSMono items are used.  
	//Variable is updated in LJZJSColor. 
	m_bLJZjsColor2Printer = FALSE; 

}

LJZjs::~LJZjs ()
{
#ifdef HAVE_LIBDL
    if (m_hHPLibHandle)
    {
        dlclose (m_hHPLibHandle);
    }
#endif
    if (m_pszInputRasterData)
    {
        delete [] m_pszInputRasterData;
    }
}

HeaderLJZjs::HeaderLJZjs (Printer* p,PrintContext* pc)
    : Header(p,pc)
{
}

DRIVER_ERROR HeaderLJZjs::Send ()
{
    DRIVER_ERROR err = NO_ERROR;
    char        szStr[256];
    WORD        wItems[3] = {ZJI_DMCOLLATE, ZJI_PAGECOUNT, ZJI_DMDUPLEX};
    int         i = 4;

    QUALITY_MODE    eQuality;
    MEDIATYPE       cmt;
    BOOL            cdt;
    COLORMODE       ccm;
    thePrintContext->GetPrintModeSettings (eQuality, cmt, ccm, cdt);

    if (((LJZjs *) thePrinter)->m_iPrinterType == eLJM1005)
    {
        strcpy (szStr, "\x1B\x25-12345X@PJL JOB\x0D\x0A");
        strcpy (szStr+strlen (szStr), "@PJL SET JAMRECOVERY=OFF\x0D\x0A");
        strcpy (szStr+strlen (szStr), "@PJL SET DENSITY=3\x0D\x0A");
        strcpy (szStr+strlen (szStr), "@PJL SET RET=MEDIUM\x0D\x0A");
        strcpy (szStr+strlen (szStr), "@PJL SET ECONOMODE=");
        if (eQuality == QUALITY_DRAFT)
        {
            strcpy (szStr+strlen (szStr), "ON\x0D\x0A");
        }
        else
        {
            strcpy (szStr+strlen (szStr), "OFF\x0D\x0A");
        }
        err = thePrinter->Send ((const BYTE *) szStr, strlen (szStr));
        ERRCHECK;

        strcpy (szStr, "\x1B\x25-12345X,XQX");
        err = thePrinter->Send ((const BYTE *) szStr, strlen (szStr));
        memset (szStr, 0x0, 92);
        szStr[3] = 0x01;
        szStr[7] = 0x07;
        i = 8;
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x80000000, 0x04, 0x54);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x10000005, 0x04, 0x01);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x10000001, 0x04, 0x00);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x10000002, 0x04, 0x00);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x10000000, 0x04, 0x00);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x10000003, 0x04, 0x01);
        i += ((LJZjs *) thePrinter)->SendIntItem ((BYTE *) szStr+i, 0x80000001, 0x04, 0xDEADBEEF);
        err = thePrinter->Send ((const BYTE *) szStr, i);
        return err;
    }
    
    strcpy (szStr, "\x1B\x25-12345X@PJL ENTER LANGUAGE=ZJS\x0A");
    err = thePrinter->Send ((const BYTE *) szStr, strlen (szStr));
    ERRCHECK;

    memset (szStr, 0, 256);

    strcpy (szStr, "JZJZ");
    i = 0;
    szStr[i+7]  = 52;
    szStr[i+11] = ZJT_START_DOC;
    szStr[i+15] = 3;
    szStr[i+17] = 36;
    szStr[i+18] = 'Z';
    szStr[i+19] = 'Z';

    i += 20;
    for (int j = 0; j < 3; j++)
    {
        szStr[i+3] = 12;
        szStr[i+5] = (char) wItems[j];
        szStr[i+6] = ZJIT_UINT32;
        szStr[i+11] = j / 2;
        i += 12;
    }
    err = thePrinter->Send ((const BYTE *) szStr, i);
    return err;
}

int LJZjs::MapPaperSize ()
{
    switch (thePrintContext->GetPaperSize ())
    {
        case LETTER:         return 1;
        case LEGAL:          return 5;
        case A4:             return 9;
        case B4:             return 12;
        case B5:             return 357;
        case OUFUKU:         return 43;
        case HAGAKI:         return 43;
#ifdef APDK_EXTENDED_MEDIASIZE
        case A3:             return 8;
        case A5:             return 11;
//        case LEDGER:         return 4;
        case EXECUTIVE:      return 7;
//        case CUSTOM_SIZE:    return 96;
	case ENVELOPE_NO_10: return 20;
	case ENVELOPE_DL:    return 27;
        case FLSA:           return 258;
#endif
        default:             return 1;
    }
}

DRIVER_ERROR LJZjs::StartPage (DWORD dwWidth, DWORD dwHeight)
{
    DRIVER_ERROR        err = NO_ERROR;
    QUALITY_MODE        cqm;
    MEDIATYPE           cmt;
    BOOL                cdt;
    DWORD               dwNumItems = (m_bIamColor) ? 15 : 14;
    BYTE                szStr[16 + 15 * 12];
    int                 iPlanes = 1;
    int                 i;
    int                 iMediaType = 1; // Plain paper

    if (m_bStartPageSent)
    {
        return NO_ERROR;
    }
    m_bStartPageSent = TRUE;
    err = thePrintContext->GetPrintModeSettings (cqm, cmt, m_cmColorMode, cdt);
    if (cmt == MEDIA_TRANSPARENCY)
    {
        iMediaType = 2;
    }
    else if (cmt == MEDIA_PHOTO)
    {
        iMediaType = 3;
    }

    if (m_iPrinterType == eLJM1005)
    {
        int    iOutputResolution = GetOutputResolutionY ();
        if (cqm == QUALITY_BEST)
            iOutputResolution = (int) thePrintContext->EffectiveResolutionY ();
        memset (szStr, 0x0, sizeof (szStr));
        szStr[3] = 0x03;
        szStr[7] = 0x0F;
        err = Send ((const BYTE *) szStr, 8);
        i = 0;
        i += SendIntItem (szStr+i, 0x80000000, 0x04, 0xB4);
        i += SendIntItem (szStr+i, 0x20000005, 0x04, 0x01);
        i += SendIntItem (szStr+i, 0x20000006, 0x04, 0x07);
        i += SendIntItem (szStr+i, 0x20000000, 0x04, 0x01);
        i += SendIntItem (szStr+i, 0x20000007, 0x04, 0x01);
        i += SendIntItem (szStr+i, 0x20000008, 0x04, (int) thePrintContext->EffectiveResolutionX ());
        i += SendIntItem (szStr+i, 0x20000009, 0x04, iOutputResolution);
        i += SendIntItem (szStr+i, 0x2000000D, 0x04, (int) dwWidth);
        i += SendIntItem (szStr+i, 0x2000000E, 0x04, (int) m_dwLastRaster);
        i += SendIntItem (szStr+i, 0x2000000A, 0x04, m_iBPP);
        i += SendIntItem (szStr+i, 0x2000000F, 0x04, (int) dwWidth/m_iBPP);
        i += SendIntItem (szStr+i, 0x20000010, 0x04, (int) m_dwLastRaster);
        i += SendIntItem (szStr+i, 0x20000011, 0x04, 0x01);
        i += SendIntItem (szStr+i, 0x20000001, 0x04, MapPaperSize ());
        i += SendIntItem (szStr+i, 0x80000001, 0x04, 0xDEADBEEF);
        err = Send ((const BYTE *) szStr, i);
        return err;
    }

	if(m_bLJZjsColor2Printer)
	{
		dwNumItems = 13;
	}
    if (m_cmColorMode == COLOR && m_bIamColor)
    {
        iPlanes = 4;
    }

    i = 0;
    i += SendChunkHeader (szStr, 16 + dwNumItems * 12, ZJT_START_PAGE, dwNumItems);

    if (m_bIamColor)
    {
        i += SendItem (szStr+i, ZJIT_UINT32, ZJI_PLANE, iPlanes);
    }
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_DMPAPER, MapPaperSize ());
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_DMCOPIES, thePrintContext->GetCopyCount ());
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_DMDEFAULTSOURCE, thePrintContext->GetMediaSource ());
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_DMMEDIATYPE, iMediaType);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_NBIE, iPlanes);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_RESOLUTION_X, thePrintContext->EffectiveResolutionX ());
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_RESOLUTION_Y, thePrintContext->EffectiveResolutionY ());
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_RASTER_X, dwWidth);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_RASTER_Y, m_dwLastRaster);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_VIDEO_BPP, m_iBPP);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_VIDEO_X, dwWidth/m_iBPP);
    i += SendItem (szStr+i, ZJIT_UINT32, ZJI_VIDEO_Y, m_dwLastRaster);
	if(!m_bLJZjsColor2Printer)
	{
		i += SendItem (szStr+i, ZJIT_UINT32, ZJI_RET, RET_ON);
		i += SendItem (szStr+i, ZJIT_UINT32, ZJI_TONER_SAVE, (cqm == QUALITY_DRAFT) ? 1 : 0);
	}

    err = Send ((const BYTE *) szStr, i);
    return err;
}

int LJZjs::SendChunkHeader (BYTE *szStr, DWORD dwSize, DWORD dwChunkType, DWORD dwNumItems)
{
    for (int j = 3, i = 0; j >= 0; j--)
    {
        szStr[i] = (BYTE) ((dwSize >> (8 * (j))) & 0xFF);
        szStr[4+i] = (BYTE) ((dwChunkType >> (8 * (j))) & 0xFF);
        szStr[8+i] = (BYTE) ((dwNumItems >> (8 * (j))) & 0xFF);
        i++;
    }

    szStr[12] = (BYTE) (((dwNumItems * 12) & 0xFF00) >> 8);
    szStr[13] = (BYTE) (((dwNumItems * 12) & 0x00FF));

    szStr[14] = 'Z';
    szStr[15] = 'Z';
    return 16;
}

int LJZjs::SendItem (BYTE *szStr, BYTE cType, WORD wItem, DWORD dwValue, DWORD dwExtra)
{
    int        i, j;
    dwExtra += 12;
    for (j = 3, i = 0; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((dwExtra >> (8 * (j))) & 0xFF);
    }
    szStr[i++] = (BYTE) ((wItem & 0xFF00) >> 8);
    szStr[i++] = (BYTE) ((wItem & 0x00FF));
    szStr[i++] = (BYTE) cType;
    szStr[i++] = 0;
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((dwValue >> (8 * (j))) & 0xFF);
    }
    return i;
}

int LJZjs::SendIntItem (BYTE *szStr, int iItem, int iItemType, int iItemValue)
{
    int        i = 0;
    int        j;
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItem >> (8 * (j))) & 0xFF);
    }
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItemType >> (8 * (j))) & 0xFF);
    }
    for (j = 3; j >= 0; j--)
    {
        szStr[i++] = (BYTE) ((iItemValue >> (8 * (j))) & 0xFF);
    }
    return i;
}

DRIVER_ERROR LJZjs::SkipRasters (int iBlankRasters)
{
    DRIVER_ERROR    err = NO_ERROR;
    BOOL            bLastPlane;
    int             iPlanes = (m_cmColorMode == COLOR) ? 4 : 1;
    for (int i = 1; i <= iPlanes; i++)
    {
        bLastPlane = (i == iPlanes) ? TRUE : FALSE;
        for (int j = 0; j < iBlankRasters; j++)
        {
            err = this->Encapsulate (NULL, bLastPlane);
        }
    }
    return err;
}

DRIVER_ERROR HeaderLJZjs::FormFeed ()
{
    DRIVER_ERROR err = NO_ERROR;

    err = thePrinter->Flush (0);

    return err;
}

DRIVER_ERROR HeaderLJZjs::SendCAPy (unsigned int iAbsY)
{
    return NO_ERROR;
}

DRIVER_ERROR LJZjs::Flush (int FlushSize)
{
    DRIVER_ERROR    err = NO_ERROR;
    if (m_dwCurrentRaster == 0)
    {
        return NO_ERROR;
    }
    err = SkipRasters ((m_dwLastRaster - m_dwCurrentRaster));
    return err;
}

DRIVER_ERROR LJZjs::JbigCompress ()
{
    DRIVER_ERROR        err = NO_ERROR;
    HPLJZjcBuff         myBuffer;
    int                 iPlanes = (m_cmColorMode == COLOR) ? 4 : 1;
    int                 iIncr = (m_bIamColor) ? 100 : m_dwLastRaster;

    HPLJZjsJbgEncSt   se;
    BYTE    *bitmaps[4] =
            {
                m_pszInputRasterData,
                m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster),
                m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster * 2),
                m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster * 3)
            };
    myBuffer.pszCompressedData = new BYTE[m_dwWidth * m_dwLastRaster * m_iBPP];
    myBuffer.dwTotalSize = 0;

    BYTE    *p;
    int      iHeight;
    for (DWORD y = 0; y < m_dwLastRaster; y += iIncr)
    {
        for (int i = 0; i < iPlanes; i++)
        {
            memset (myBuffer.pszCompressedData, 0, m_dwWidth * m_dwLastRaster * m_iBPP);
            myBuffer.dwTotalSize = 0;
            p = bitmaps[i] + (y * m_dwWidth * m_iBPP);
            iHeight = iIncr;
            if (y + iIncr > m_dwLastRaster)
            {
                iHeight = m_dwLastRaster - y;
            }

            HPLJJBGCompress (m_dwWidth * 8 * m_iBPP, iHeight, &p, &myBuffer, &se);

            if (i == 0)
            {
                StartPage (se.xd, se.yd);
            }
            err = this->SendPlaneData (i + 1, &se, &myBuffer, (y + iIncr) >= m_dwLastRaster);
        }
    }

    delete [] myBuffer.pszCompressedData;
    m_dwCurrentRaster = 0;
    m_pszCurPtr = m_pszInputRasterData;
    memset (m_pszCurPtr, 0, (m_dwWidth * m_dwLastRaster * iPlanes * m_iBPP));

    err = EndPage ();

    return err;
}


/*JBig Compress for LJZjsColor-2 Printers
Separate function written for LJZjsColor-2 Printers, since for them, compression is done for whole plane data at a time
whereas for other deiveces, compression is done for 100 lines of each plane*/
DRIVER_ERROR LJZjs::JbigCompress_LJZjsColor2 ()
{
    DRIVER_ERROR        err = NO_ERROR;
    HPLJZjcBuff         myBuffer;
    
	int                 iPlanes = (m_cmColorMode == COLOR) ? 4 : 1;
	int					arrPlanesOrder[] = {3,2,1,4};
	int					nByteCount = 0;
	int					iHeight = 0;
	
    HPLJZjsJbgEncSt   se;
    BYTE    *pbUnCompressedData = NULL;
    
	BYTE    *bitmaps[4] =
	{
		m_pszInputRasterData,
			m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster),
			m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster * 2),
			m_pszInputRasterData + (m_dwWidth * m_iBPP * m_dwLastRaster * 3)
	};

    myBuffer.pszCompressedData = new BYTE[m_dwWidth * m_dwLastRaster * m_iBPP];
	if(NULL == myBuffer.pszCompressedData)
	{
		return ALLOCMEM_ERROR;
	}
    myBuffer.dwTotalSize = 0;    	
	
	for (int nPlaneCount = 0; nPlaneCount < iPlanes; nPlaneCount++)    
    {
		
		memset (myBuffer.pszCompressedData, 0, m_dwWidth * m_dwLastRaster * m_iBPP);
		myBuffer.dwTotalSize = 0;
		
		if(4 == iPlanes)/*If there are 4 planes follow LJZjsColor-2 order of 3 2 1 4*/
		{
			pbUnCompressedData = bitmaps[arrPlanesOrder[nPlaneCount]-1] ;
		}
		else /* Should not happen */
		{
			return SYSTEM_ERROR;
		}

		
		iHeight = m_dwLastRaster; /*Send all scan lines at one go*/
		
		HPLJJBGCompress (m_dwWidth * 8 * m_iBPP, iHeight, &pbUnCompressedData, &myBuffer, &se);
		
		if(0 == nPlaneCount)
		{
			StartPage (se.xd, se.yd);
		}
		
		err = this->SendPlaneData (arrPlanesOrder[nPlaneCount], &se, &myBuffer, FALSE);
        
    }
	
    delete [] myBuffer.pszCompressedData;
    m_dwCurrentRaster = 0;
    m_pszCurPtr = m_pszInputRasterData;
    memset (m_pszCurPtr, 0, (m_dwWidth * m_dwLastRaster * iPlanes * m_iBPP));
	
    err = EndPage ();
	
    return err;
}

DRIVER_ERROR HeaderLJZjs::EndJob ()
{
    DRIVER_ERROR    err = NO_ERROR;
    char            szStr[64];

    if (((LJZjs *) thePrinter)->m_iPrinterType == eLJM1005)
    {
        memset (szStr, 0, 8);
        szStr[3] = 2;
        strcpy ((char *) szStr+8, "\x1B\x25-12345X@PJL EOJ\x0D\x0A\x1B\x25-12345X");
        err = thePrinter->Send ((const BYTE *) szStr, 8 + strlen ((char *) (szStr+8)));
        return err;
    }


    memset (szStr, 0, 64);

    szStr[3] = 16;
    szStr[7] = ZJT_END_DOC;
    szStr[14] = 'Z';
    szStr[15] = 'Z';
    err = thePrinter->Send ((const BYTE *) szStr, 16);
    return err;
}

Header *LJZjs::SelectHeader (PrintContext* pc)
{
    DRIVER_ERROR    err = NO_ERROR;
    DWORD           dwSize;
    int             iPlanes = 1;
    QUALITY_MODE        cqm;
    MEDIATYPE           cmt;
    BOOL                cdt;
    err = pc->GetPrintModeSettings (cqm, cmt, m_cmColorMode, cdt);

    if (m_cmColorMode == COLOR && m_bIamColor)
    {
        iPlanes = 4;
        m_iP[0] = 3;
    }
    m_dwWidth = pc->OutputPixelsPerRow () / 8;
    if (m_dwWidth % 8)
    {
        m_dwWidth = (m_dwWidth / 8 + 1) * 8;
    }
    m_dwLastRaster = (int) (pc->PrintableHeight () * pc->EffectiveResolutionY () + 0.5);
    dwSize = m_dwWidth * m_dwLastRaster * iPlanes * m_iBPP;
    m_pszInputRasterData = new BYTE[dwSize];
    if (m_pszInputRasterData == NULL)
    {
        return NULL;
    }
    m_pszCurPtr = m_pszInputRasterData;
    memset (m_pszCurPtr, 0, dwSize);
    thePrintContext = pc;
    return new HeaderLJZjs (this,pc);
}

DRIVER_ERROR LJZjs::VerifyPenInfo()
{

    ePen = BOTH_PENS;
    return NO_ERROR;
}

DRIVER_ERROR LJZjs::ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter)
{

    ePen = BOTH_PENS;

    return NO_ERROR;
}

APDK_END_NAMESPACE

#endif  // defined APDK_LJZJS_MONO || defined APDK_LJZJS_COLOR || defined APDK_LJM1005

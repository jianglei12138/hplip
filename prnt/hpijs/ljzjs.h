/*****************************************************************************\
  ljzjs.h : Interface for the LJZjs class

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


#ifndef APDK_LJZJS_H
#define APDK_LJZJS_H

#if defined (APDK_LJZJS_MONO) || defined (APDK_LJZJS_COLOR) || defined (APDK_LJM1005)

#include "hpjbig_wrapper.h"

APDK_BEGIN_NAMESPACE


/*!
\internal
*/
class LJZjs : public Printer
{
public:
    LJZjs (SystemServices* pSS, int numfonts = 0, BOOL proto = FALSE);
    ~LJZjs ();

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);
    virtual DRIVER_ERROR Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane)
    {
        return SYSTEM_ERROR;
    }
    virtual DRIVER_ERROR Flush (int FlushSize);
    virtual DRIVER_ERROR SkipRasters (int iBlankRasters);
    int SendChunkHeader (BYTE *szStr, DWORD dwSize, DWORD dwChunkType, DWORD dwNumItems);
    int SendItem (BYTE *szStr, BYTE cType, WORD wItem, DWORD dwValue, DWORD dwExtra = 0);
    int SendIntItem (BYTE *szStr, int iItem, int iItemType, int iItemValue);
    DRIVER_ERROR SendChunkAndItemHeader (BYTE type, void *obj);

    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.25;
        fMargins[1] = (float) 0.25;
        fMargins[2] = (float) 0.2;
        fMargins[3] = (float) 0.2;
        return TRUE;
    }
    PRINTER_TYPE    m_iPrinterType;

protected:

#ifdef APDK_HP_UX
    virtual LJZjs & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

//private:
    int             MapPaperSize ();
    DRIVER_ERROR    JbigCompress ();
	DRIVER_ERROR    JbigCompress_LJZjsColor2 ();
    DRIVER_ERROR    SendItemData (BYTE ItemType, WORD Item, DWORD dwValue);
    DRIVER_ERROR    StartPage (DWORD dwWidth, DWORD dwHeight);
    virtual DRIVER_ERROR    EndPage ()
    {
        return SYSTEM_ERROR;
    }
    virtual DRIVER_ERROR    SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride)
    {
        return SYSTEM_ERROR;
    }
    virtual int GetOutputResolutionY ()
    {
        return 600;
    }

    static const unsigned char szByte1[256];
    static const unsigned char szByte2[256];
    void            *m_hHPLibHandle;

    DWORD           m_dwWidth;
    DWORD           m_dwCurrentRaster;
    DWORD           m_dwLastRaster;
    BYTE            *m_pszInputRasterData;
    BYTE            *m_pszCurPtr;
    PrintContext    *thePrintContext;
    BOOL            m_bStartPageSent;
    COLORMODE       m_cmColorMode;
    BOOL            m_bIamColor;
    int             m_iPlaneNumber;
    int             m_iBPP;
    int             m_iP[4];
	BOOL			m_bLJZjsColor2Printer ; /*TRUE when the Printer model follows LJZjsColor-2 encapsulation format, else FALSE*/
}; // LJZjs

typedef enum
{
    ZJT_START_DOC,
    ZJT_END_DOC,
    ZJT_START_PAGE,
    ZJT_END_PAGE,
    ZJT_JBIG_BIH,
    ZJT_JBIG_HID,
    ZJT_END_JBIG,
    ZJT_SIGNATURE,
    ZJT_RAW_IMAGE,
    ZJT_START_PLANE,
    ZJT_END_PLANE,
    ZJT_PAUSE,
    ZJT_BITMAP
} CHUNK_TYPE;

typedef enum
{
/* 0x00*/    ZJI_PAGECOUNT,
/* 0x01*/    ZJI_DMCOLLATE,
/* 0x02*/    ZJI_DMDUPLEX,

/* 0x03*/    ZJI_DMPAPER,
/* 0x04*/    ZJI_DMCOPIES,
/* 0x05*/    ZJI_DMDEFAULTSOURCE,
/* 0x06*/    ZJI_DMMEDIATYPE,
/* 0x07*/    ZJI_NBIE,
/* 0x08*/    ZJI_RESOLUTION_X,
/* 0x09*/    ZJI_RESOLUTION_Y,
/* 0x0A */    ZJI_OFFSET_X,
/* 0x0B */    ZJI_OFFSET_Y,
/* 0x0C */    ZJI_RASTER_X,
/* 0x0D */    ZJI_RASTER_Y,

/* 0x0E */    ZJI_COLLATE,
/* 0x0F */    ZJI_QUANTITY,

/* 0x10 */    ZJI_VIDEO_BPP,
/* 0x11 */    ZJI_VIDEO_X,
/* 0x12 */    ZJI_VIDEO_Y,
/* 0x13 */    ZJI_INTERLACE,
/* 0x14 */    ZJI_PLANE,
/* 0x15 */    ZJI_PALETTE,

/* 0x16 */    ZJI_RET,
/* 0x17 */    ZJI_TONER_SAVE,

/* 0x18 */    ZJI_MEDIA_SIZE_X,
/* 0x19 */    ZJI_MEDIA_SIZE_Y,
/* 0x1A */    ZJI_MEDIA_SIZE_UNITS,

/* 0x1B */    ZJI_CHROMATIC,

/* 0x63 */    ZJI_PAD = 99,

/* 0x64 */    ZJI_PROMPT,

/* 0x65 */    ZJI_BITMAP_TYPE,
/* 0x66 */    ZJI_ENCODING_DATA,
/* 0x67 */    ZJI_END_PLANE,

/* 0x68 */    ZJI_BITMAP_PIXELS,
/* 0x69 */    ZJI_BITMAP_LINES,
/* 0x6A */    ZJI_BITMAP_BPP,
/* 0x6B */    ZJI_BITMAP_STRIDE,

} ZJ_ITEM;

typedef enum
{
    RET_OFF = 0,
    RET_ON,
    RET_AUTO,
    RET_LIGHT,
    RET_MEDIUM,
    RET_DARK
} RET_VALUE;

typedef enum
{
    ZJIT_UINT32 = 1,
    ZJIT_INT32,
    ZJIT_STRING,
    ZJIT_BYTELUT
} CHUNK_ITEM_TYPE;

APDK_END_NAMESPACE

#endif // defned (APDK_LJZJS_MONO) || defined (APDK_LJZJS_COLOR) || defined (APDK_LJM1005)
#endif //APDK_LJZJS_H


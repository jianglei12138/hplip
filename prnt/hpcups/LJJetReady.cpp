/*****************************************************************************\
  LJJetReady.cpp : Implementation of LJJetReady class

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

#include "Encapsulator.h"
#include "ModeJpeg.h"
#include "LJJetReady.h"
#include "PrinterCommands.h"

#define MOJAVE_STRIP_HEIGHT 128

LJJetReady::LJJetReady() : Encapsulator()
{
    m_eCompressMode = COMPRESS_MODE_JPEG;
    memset(&m_QTableInfo, 0, sizeof(m_QTableInfo));
}

LJJetReady::~LJJetReady()
{
}

DRIVER_ERROR LJJetReady::addJobSettings()
{
    DRIVER_ERROR    err;
    addToHeader("@PJL SET STRINGCODESET=UTF8\012");
    addToHeader("@PJL SET COPIES=1\012");
    if (m_pJA->color_mode != 0)
    {
        addToHeader("@PJL SET PLANESINUSE=1\012");
    }
    if (m_pJA->e_duplex_mode != DUPLEXMODE_NONE)
    {
        addToHeader("@PJL SET DUPLEX=ON\012@PJL SET BINDING=%s\012",
                    m_pJA->e_duplex_mode == DUPLEXMODE_BOOK ? "LONGEDGE" : "SHORTEDGE");
    }
    addToHeader("@PJL SET RESOLUTION=600\012");
    addToHeader("@PJL SET TIMEOUT=90\012");
    addToHeader("@PJL ENTER LANGUAGE=PCLXL\012");
    addToHeader(") HP-PCL XL;3;0;Comment, PCL-XL JetReady generator\012");
    addToHeader(JRBeginSessionSeq, (int) sizeof(JRBeginSessionSeq));
    err = Cleanup();
    return err;
}

DRIVER_ERROR LJJetReady::Configure(Pipeline **pipeline)
{
    Pipeline    *head;
    ModeJpeg    *pModeJpeg;
    pModeJpeg = new ModeJpeg(((m_pMA->printable_width + 31) / 32) * 32);
    if (pModeJpeg == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    m_eCompressMode = m_pQA->print_quality == 1 ? COMPRESS_MODE_LJ : COMPRESS_MODE_JPEG;
    pModeJpeg->myplane = COLORTYPE_COLOR;
    m_QTableInfo.qFactor = 6;
    pModeJpeg->Init(m_pJA->color_mode, MOJAVE_STRIP_HEIGHT, &m_eCompressMode, &m_QTableInfo);
    head = new Pipeline(pModeJpeg);
    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR LJJetReady::StartPage(JobAttributes *pJA)
{
    DRIVER_ERROR    err;
    BYTE            JRPaperSizeSeq[] = {0xC0, 0x00, 0xF8, 0x25};
    BYTE            szCustomSize[16];
    m_iStripHeight = 0;
    if (m_eCompressMode != COMPRESS_MODE_LJ)
        m_bSendQTable = true;
    addToHeader((const BYTE *) JRFeedOrientationSeq, sizeof(JRFeedOrientationSeq));
    if (pJA->media_attributes.pcl_id == 96) // Custom paper size
    {

        union
        {
            float       fValue;
            uint32_t    uiValue;
        } JRCustomPaperSize;
        uint32_t    uiXsize;
        uint32_t    uiYsize;
        int         k = 0;

        // Physical width and height in inches
        JRCustomPaperSize.fValue = (float) pJA->media_attributes.physical_width / (float) pJA->quality_attributes.horizontal_resolution;
        uiXsize = JRCustomPaperSize.uiValue;
        JRCustomPaperSize.fValue = (float) pJA->media_attributes.physical_height / (float) pJA->quality_attributes.vertical_resolution;;
        uiYsize = JRCustomPaperSize.uiValue;
        szCustomSize[k++] = 0xD5;
        szCustomSize[k++] = (BYTE) (uiXsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiXsize & 0xFF000000) >> 24);
        szCustomSize[k++] = (BYTE) (uiYsize & 0x000000FF);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x0000FF00) >> 8);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0x00FF0000) >> 16);
        szCustomSize[k++] = (BYTE) ((uiYsize & 0xFF000000) >> 24);
        addToHeader((const BYTE *) szCustomSize, k);
        addToHeader(JRCustomPaperSizeSeq, sizeof(JRCustomPaperSizeSeq));
    }
    else
    {
        JRPaperSizeSeq[1] = pJA->media_attributes.pcl_id;
        addToHeader((const BYTE *) JRPaperSizeSeq, sizeof(JRPaperSizeSeq));
    }
    BYTE    szPrintableAreaSeq[] = {0xD1, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x74};
    int     iWidth = pJA->media_attributes.printable_width;
    int     iHeight = pJA->media_attributes.printable_height;

//  The minimum printable width is 1600 pixels (3 inch * 600 - 200 for margins)
    if (iWidth < 1600)
    {
        iWidth = 1600;
    }
//  The source width must be a mutiple of 32
    iWidth = ((iWidth + 31) / 32) * 32;
    iHeight = ((iHeight + (MOJAVE_STRIP_HEIGHT - 1)) / MOJAVE_STRIP_HEIGHT) * MOJAVE_STRIP_HEIGHT;
    szPrintableAreaSeq[1] = (BYTE) (iWidth & 0xFF);
    szPrintableAreaSeq[2] = (BYTE) ((iWidth >> 8) & 0xFF);
    szPrintableAreaSeq[3] = (BYTE) (iHeight & 0xFF);
    szPrintableAreaSeq[4] = (BYTE) ((iHeight >> 8) & 0xFF);
    addToHeader((const BYTE *) szPrintableAreaSeq, sizeof(szPrintableAreaSeq));

    addToHeader(JRBeginPageSeq, sizeof(JRBeginPageSeq));

//  Colormode must be 0 - color and 1 - grayscale
    BYTE szColorModeSeq[] = {0xC0, 0x06, 0xF8, 0x03, 0x6A};
    szColorModeSeq[1] += pJA->color_mode;
    addToHeader((const BYTE *) szColorModeSeq, sizeof(szColorModeSeq));
    err = Cleanup();

    BYTE    szStr[16];
    addToHeader(JRBeginImageSeq, sizeof(JRBeginImageSeq));
    szStr[0] = (BYTE) (iWidth & 0xFF);
    szStr[1] = (BYTE) ((iWidth & 0xFF00) >> 8);
    szStr[2] = 0xF8;
    szStr[3] = 0x6C;
    szStr[4] = 0xC1;
    szStr[5] = (BYTE) (iHeight & 0xFF);
    szStr[6] = (BYTE) ((iHeight & 0xFF00) >> 8);
    szStr[7] = 0xF8;
    szStr[8] = 0x6B;
    szStr[9] = 0xC1;
    addToHeader((const BYTE *) szStr, 10);
    unsigned int uiStripCount = iHeight / MOJAVE_STRIP_HEIGHT;
    szStr[0] = (BYTE) (uiStripCount & 0xFF);
    szStr[1] = (BYTE) ((uiStripCount & 0xFF00) >> 8);
    szStr[2] = 0xF8;
    szStr[3] = 0x93;
    szStr[4] = 0xC1;
    addToHeader((const BYTE *) szStr, 5);
    const BYTE szStripHeightSeq[] = {0x80, 0x00, 0xF8, 0x94};
    addToHeader(szStripHeightSeq, sizeof(szStripHeightSeq));
    szStr[0] = 0xC0;
    szStr[1] = 0x00;
    szStr[2] = 0xF8;
    szStr[3] = 0x97;
    if (m_pJA->color_mode == 0)
    {
        szStr[1] = 0x04;
    }
    addToHeader((const BYTE *) szStr, 4);
//  Interleaved Color Enumeration sequence
    addToHeader(JRICESeq, sizeof(JRICESeq));
    addToHeader(JRVueVersionTagSeq, sizeof(JRVueVersionTagSeq));
    BYTE szDataLengthSeq[] = {0xC2, 0x38, 0x03, 0x00, 0x00, 0xF8, 0x92, 0x46};
    if (m_eCompressMode == COMPRESS_MODE_LJ)
    {
        szDataLengthSeq[1] = 0;
        szDataLengthSeq[2] = 0;
    }
    addToHeader(szDataLengthSeq, sizeof(szDataLengthSeq));
    err = Cleanup();
    return err;
}

DRIVER_ERROR LJJetReady::FormFeed()
{
    DRIVER_ERROR    err;
    addToHeader(JRVueExtn3Seq, sizeof(JRVueExtn3Seq));
    addToHeader(JRVueVersionTagSeq, sizeof(JRVueVersionTagSeq));
    addToHeader(JRVendorUniqueSeq, sizeof(JRVendorUniqueSeq));
    addToHeader(JREndPageSeq, sizeof(JREndPageSeq));
    err = Cleanup();
    return err;
}

DRIVER_ERROR LJJetReady::EndJob()
{
    DRIVER_ERROR    err;
    addToHeader(JREndSessionSeq, sizeof(JREndSessionSeq));
    addToHeader((const BYTE *) PJLExit, strlen(PJLExit));
    err = Cleanup();
    return err;
}

DRIVER_ERROR LJJetReady::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err;
    int             iJpegHeaderSize = 623;
    unsigned int    ulVuDataLength;
    BYTE            *pDataPtr;

    if (m_eCompressMode != COMPRESS_MODE_LJ)
    {
        err = sendJPEGHeaderInfo(InputRaster);
    }

    if (m_pJA->color_mode != 0 || m_eCompressMode == COMPRESS_MODE_LJ)
        iJpegHeaderSize = 0;

    if (InputRaster->rasterdata[COLORTYPE_COLOR] == NULL)
    {
        return NO_ERROR;
    }

    ulVuDataLength = InputRaster->rastersize[COLORTYPE_COLOR] - iJpegHeaderSize;
    pDataPtr       = InputRaster->rasterdata[COLORTYPE_COLOR] + iJpegHeaderSize;
    BYTE    szStr[16];
    szStr[0] = m_iStripHeight & 0xFF;
    szStr[1] = (m_iStripHeight & 0xFF00) >> 8;
    m_iStripHeight += MOJAVE_STRIP_HEIGHT;
    addToHeader(JRReadImageSeq, sizeof(JRReadImageSeq));
    addToHeader(szStr, 2);
    addToHeader(JRStripHeightSeq, sizeof(JRStripHeightSeq));
    addToHeader(JRTextObjectTypeSeq, sizeof(JRTextObjectTypeSeq));
    addToHeader(JRVueVersionTagSeq, sizeof(JRVueVersionTagSeq));
    int    i = 0;
    szStr[i++] = 0xC2;
    ulVuDataLength += 6;
    szStr[i++] = (BYTE) (ulVuDataLength & 0xFF);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0x0000FF00) >> 8);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0x00FF0000) >> 16);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0xFF000000) >> 24);
    szStr[i++] = 0xF8;
    szStr[i++] = 0x92;
    szStr[i++] = 0x46;
    if (m_eCompressMode == COMPRESS_MODE_LJ)
        szStr[i++] = 0x11;
    else
        szStr[i++] = 0x21;
    szStr[i++] = 0x90;
    ulVuDataLength -= 6;
    szStr[i++] = (BYTE) (ulVuDataLength & 0xFF);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0x0000FF00) >> 8);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0x00FF0000) >> 16);
    szStr[i++] = (BYTE) ((ulVuDataLength & 0xFF000000) >> 24);
    addToHeader((const BYTE *) szStr, i);
    err = Cleanup();
    if (err == NO_ERROR)
        err = sendBuffer((const BYTE *) pDataPtr, ulVuDataLength);
    return err;
}

void LJJetReady::addQTable(DWORD *qtable)
{
    for (int i = 0; i < QTABLE_SIZE; i++)
    {
        *cur_pcl_buffer_ptr++ = (BYTE) (qtable[i] & 0xFF);
        *cur_pcl_buffer_ptr++ = (BYTE) ((qtable[i] >> 8)  & 0xFF);
        *cur_pcl_buffer_ptr++ = 0;
        *cur_pcl_buffer_ptr++ = 0;
    }
}

DRIVER_ERROR LJJetReady::sendJPEGHeaderInfo(RASTERDATA *InputRaster)
{
    DRIVER_ERROR    err;
    if (!m_bSendQTable)
    {
        return NO_ERROR;
    }
    union
    {
        short    s;
        BYTE     c[2];
    } endianness;
    endianness.s = 0x0A0B;
    err = sendBuffer(JRQTSeq, sizeof(JRQTSeq));
    if (endianness.c[0] == 0x0B)
    {
        err = sendBuffer((const BYTE *) &m_QTableInfo, sizeof(DWORD) * QTABLE_SIZE * 3);
    }
    else
    {
        addQTable(m_QTableInfo.qtable0);
        addQTable(m_QTableInfo.qtable1);
        addQTable(m_QTableInfo.qtable2);
        err = Cleanup();
    }
    m_bSendQTable = false;
    addToHeader(JRCRSeq, sizeof(JRCRSeq));
    BYTE    szCR3Seq[] = {0x00, 0x00, 0x00, 0x00};
    if (m_pJA->color_mode != 0)
    {
        addToHeader(JRCR1GSeq, sizeof(JRCR1GSeq));
    }
    else
    {
        addToHeader(JRCR1CSeq, sizeof(JRCR1CSeq));
        szCR3Seq[0] = 0x01;
    }
    addToHeader(szCR3Seq, sizeof(szCR3Seq));
    for (int i = 0; i < 9; i++)
    {
        addToHeader(JRSCSeq[i], 4);
    }
    err = Cleanup();
    return err;
}


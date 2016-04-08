/*****************************************************************************\
  QuickConnect.cpp : Implementation of QuickConnect class

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
#include "QuickConnect.h"
#include "ModeJpeg.h"

QuickConnect::QuickConnect() : Encapsulator()
{
}

QuickConnect::~QuickConnect()
{
}

DRIVER_ERROR QuickConnect::StartJob(SystemServices *pSystemServices, JobAttributes *pJA)
{
    m_pSystemServices = pSystemServices;
    m_pJA = pJA;
    m_pMA = &pJA->media_attributes;
    m_pQA = &pJA->quality_attributes;
    return NO_ERROR;
}

DRIVER_ERROR QuickConnect::Configure(Pipeline **pipeline)
{
    Pipeline    *head;
    ModeJpeg    *pModeJpeg;
    pModeJpeg = new ModeJpeg(m_pMA->printable_width);
    if (pModeJpeg == NULL)
    {
        return ALLOCMEM_ERROR;
    }

//  This is the max jpeg file size value
    pModeJpeg->myplane = COLORTYPE_COLOR;
    pModeJpeg->Init(m_pJA->integer_values[1], m_pMA->printable_height);
    head = new Pipeline(pModeJpeg);
    *pipeline = head;
    return NO_ERROR;
}

DRIVER_ERROR QuickConnect::StartPage(JobAttributes *pJA)
{
    char    szStr[632];
    DRIVER_ERROR    err;
    const char *szPJLHeader = "\x1B\x45\x1B%-12345X@PJL ENTER LANGUAGE=PHOTOJPEG\012";
    memset(szStr, 0, 600);
    err = m_pSystemServices->Send((const BYTE *) szStr, 600);
    sprintf(szStr, "%s@PJL SET COPIES=%d\012@PJL SET JOBID=%d\012", szPJLHeader, 1, m_pJA->job_id);
    sprintf(szStr+strlen(szStr),
            "@PJL SET PAPER=%d\012@PJL SET MEDIATYPE=%d\012@PJL SET PRINTQUALITY=%d\012@PJL SET BORDERLESS=%d\012",
            m_pMA->pcl_id, m_pQA->media_type, m_pQA->print_quality, m_pJA->integer_values[2]);
    err = m_pSystemServices->Send((const BYTE *) szStr, strlen(szStr));
    return err;
}

DRIVER_ERROR QuickConnect::Encapsulate(RASTERDATA *InputRaster, bool bLastPlane)
{
    DRIVER_ERROR    err;
    const           char *szPJLEndJob = "\x1B\x45\x1B%-12345X";
    int             header_size = 0;
    if (InputRaster->rasterdata[COLORTYPE_COLOR] == NULL)
    {
        return NO_ERROR;
    }

    err = sendExifHeader(InputRaster->rasterdata[COLORTYPE_COLOR], &header_size);
    if (err != NO_ERROR)
    {
        return err;
    }
    err = m_pSystemServices->Send((const BYTE *) InputRaster->rasterdata[COLORTYPE_COLOR] + header_size,
                                   InputRaster->rastersize[COLORTYPE_COLOR] - header_size);
    if (err == NO_ERROR)
    {
        err = m_pSystemServices->Send((const BYTE *) szPJLEndJob, strlen(szPJLEndJob));
    }
    return err;
}

DRIVER_ERROR QuickConnect::sendExifHeader(BYTE *jpeg_buffer, int *header_size)
{
    DRIVER_ERROR    err;
    BYTE            *pBuffer = jpeg_buffer;

/*
 *  Jpeg APP2 Marker
 *  APP2 Header|   Length  |         Identifier     | Version |Number of Tags
 *  -------------------------------------------------------------------------
 *  0xFF|0xE2  |0x00 | 0x23|0x48|0x50|0x51|0x43|0x00|0x00|0x01|0x00|0x02
 *      Length = No. of Tags * length of tag + length of APP2 marker
 *  -------------------------------------------------------------------------
 *  Tag ID   |field Type|        Count      |Value Offset
 *  -------------------------------------------------------------
 *  0x00|0x01|0x00|0x03 |0x00|0x00|0x00|0x01|0x00|0x00|0x00|0x01
 *  -------------------------------------------------------------
 *      Field Type 0x0003 stands for short 
 *      Count and Value Offset are 4 bytes in TIFF convention. 
 *      If the count <=4, Value Offset satisfies.  If the count is bigger than 4 bytes,
 *      it will be offset to where data is located. 
 */

    unsigned char App2[] = {"\xFF\xE2\x00\x23\x48\x50\x51\x43\x00\x00\x01\x00\x02"};
    unsigned char szApp2Markers[2][12];
    int     iNumTags = 0;
    int     iOpts[2];

// Things to set are: PhotoFix, RedEyeRemoval

    iOpts[0] = m_pJA->integer_values[4];    // Red Eye flag
    iOpts[1] = m_pJA->integer_values[3];    // Photo fix flag

    short   skey;
    unsigned char szTag[] = {"\x00\x01\x00\x03\x00\x00\x00\x01\x00\x00\x00\x01"};
    unsigned int iVal;

    for (skey = 1; skey <= 2; skey++)
    {
        szTag[0] = (BYTE) ((skey & 0xFF) >> 8);
        szTag[1] = (BYTE) (skey & 0xFF);

        iVal = iOpts[skey];
        szTag[8]  = (BYTE) ((iVal >> 24) & 0xFF);
        szTag[9]  = (BYTE) ((iVal >> 16) & 0xFF);
        szTag[10] = (BYTE) ((iVal >>  8) & 0xFF);
        szTag[11] = (BYTE) (iVal & 0xFF);
        memcpy (szApp2Markers[iNumTags], szTag, 12);
        iNumTags++;

        skey = (short) iNumTags * 12 + 11;
        App2[2]  = (BYTE) ((skey >> 8) & 0xFF);
        App2[3]  = (BYTE) (skey & 0xFF); 
        App2[11] = (BYTE) ((iNumTags >> 8) & 0xFF);
        App2[12] = (BYTE) (iNumTags & 0xFF);
    }

/*
 *  First write the SOI and JFIF header
 *  File structure is:
 *
 *  BYTE SOI[2];          // 00h  Start of Image Marker
 *  BYTE APP0[2];         // 02h  Application Use Marker
 *  BYTE Length[2];       // 04h  Length of APP0 Field
 *  BYTE Identifier[5];   // 06h  "JFIF" (zero terminated) Id String
 *  BYTE Version[2];      // 07h  JFIF Format Revision
 *  BYTE Units;           // 09h  Units used for Resolution
 *  BYTE Xdensity[2];     // 0Ah  Horizontal Resolution     
 *  BYTE Ydensity[2];     // 0Ch  Vertical Resolution
 *  BYTE XThumbnail;      // 0Eh  Horizontal Pixel Count
 *  BYTE YThumbnail;      // 0Fh  Vertical Pixel Count
 */

    short sJFIFHeaderSize = ((((short) pBuffer[4]) << 8) | pBuffer[5]) + 4;
    err = m_pSystemServices->Send ((const BYTE *) pBuffer, sJFIFHeaderSize);
    if (err != NO_ERROR)
    {
        return err;
    }

    *header_size = sJFIFHeaderSize;

    if (iNumTags != 0)
    {
        err = m_pSystemServices->Send ((const BYTE *) App2, 13);
        for (int i = 0; i < iNumTags; i++)
        {
            err = m_pSystemServices->Send ((const BYTE *) szApp2Markers[i], 12);
        }
    }
    return err;
}


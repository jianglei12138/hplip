/*****************************************************************************\
  quickconnect.cpp : Implementation for the QuickConnect class

  Copyright (c) 2008, HP Co.
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


#ifdef APDK_QUICKCONNECT

#include "header.h"
#include "io_defs.h"
#include "quickconnect.h"
#include "printerproxy.h"
#include "resources.h"
#include <setjmp.h>

APDK_BEGIN_NAMESPACE

#define MAX_JPEG_FILE_SIZE 2097152

QuickConnect::QuickConnect (SystemServices* pSS, BOOL proto)
    : Printer (pSS, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo ();
        CERRCHECK;
    }
    else
        ePen = BOTH_PENS;    // matches default mode

    ModeCount = 0;
    pMode[ModeCount++] = new QCAutomatic ();
    pMode[ModeCount++] = new QCFastNormal ();
    pMode[ModeCount++] = new QCNormal ();
    pMode[ModeCount++] = new QCBest ();

    CMYMap = NULL;

    m_iJpegBufferPos = 0;
    m_iMaxFileSize = MAX_JPEG_FILE_SIZE;
    m_iInputBufferSize = 0;
    m_pbyJpegBuffer = NULL;
    m_pbyInputBuffer = NULL;

    m_iPhotoFix = 0;
    m_iRemoveRedEye = 1;
    m_iJobId = pSS->GetSystemTickCount ();
}

QuickConnect::~QuickConnect ()
{
    if (m_pbyJpegBuffer)
    {
        pSS->FreeMemory (m_pbyJpegBuffer);
    }
    if (m_pbyInputBuffer)
    {
        pSS->FreeMemory (m_pbyInputBuffer);
    }
}

QCAutomatic::QCAutomatic ()
: PrintMode (NULL)
{
    ResolutionX[0] = 300;
    ResolutionY[0] = 300;
    BaseResX = BaseResY = 300;

    bFontCapable = FALSE;

	theQuality = qualityAuto;
    medium     = mediaAuto;

    pmQuality = QUALITY_AUTO;
    pmMediaType   = MEDIA_PLAIN;
	Config.bColorImage = FALSE;
    Config.bCompress = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
}

QCFastNormal::QCFastNormal ()
: PrintMode (NULL)
{
    ResolutionX[0] = 300;
    ResolutionY[0] = 300;
    BaseResX = BaseResY = 300;

    bFontCapable = FALSE;

	theQuality = qualityFastNormal;
    medium     = mediaAuto;

    pmQuality = QUALITY_FASTNORMAL;
    pmMediaType   = MEDIA_PLAIN;
	Config.bColorImage = FALSE;
    Config.bCompress = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
}

QCNormal::QCNormal ()
: PrintMode (NULL)
{
    ResolutionX[0] = 300;
    ResolutionY[0] = 300;
    BaseResX = BaseResY = 300;

    bFontCapable = FALSE;

	theQuality = qualityNormal;
    medium     = mediaAuto;

    pmQuality = QUALITY_NORMAL;
    pmMediaType   = MEDIA_PLAIN;
	Config.bColorImage = FALSE;
    Config.bCompress = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
}

QCBest::QCBest ()
: PrintMode (NULL)
{
    ResolutionX[0] = 300;
    ResolutionY[0] = 300;
    BaseResX = BaseResY = 300;

    bFontCapable = FALSE;

	theQuality = qualityPresentation;
    medium     = mediaAuto;

    pmQuality = QUALITY_BEST;
    pmMediaType   = MEDIA_PLAIN;
	Config.bColorImage = FALSE;
    Config.bCompress = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
}

Header *QuickConnect::SelectHeader (PrintContext *pc)
{
    m_iRowNumber = 0;
    m_pPC = pc;
    m_iRowWidth = pc->OutputPixelsPerRow () * 3;

    m_iInputBufferSize = m_iRowWidth * (int) (pc->PhysicalPageSizeY () * pc->EffectiveResolutionY ());
    m_pbyInputBuffer = (BYTE *) pSS->AllocMem (m_iInputBufferSize);
    m_pbyJpegBuffer = (BYTE *) pSS->AllocMem (m_iMaxFileSize);
    if (m_pbyInputBuffer == NULL || m_pbyJpegBuffer == NULL)
    {
        constructor_error = ALLOCMEM_ERROR;
        return NULL;
    }
    memset (m_pbyInputBuffer, 0xFF, m_iInputBufferSize);
    return new HeaderQuickConnect (this, pc);
}

DRIVER_ERROR QuickConnect::Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane)
{
    memcpy (m_pbyInputBuffer + (m_iRowNumber * m_iRowWidth), InputRaster->rasterdata[COLORTYPE_COLOR],
            InputRaster->rastersize[COLORTYPE_COLOR]);
    m_iRowNumber++;
    return NO_ERROR;
}

DRIVER_ERROR QuickConnect::ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char        *str;
    int         num_pens = 0;
    ePen = BOTH_PENS;

    DRIVER_ERROR err = SetPenInfo (str, QueryPrinter);
    if (err != NO_ERROR)
    {
        return NO_ERROR;
    }

    num_pens = str[0] - '0';
    if (num_pens == 0)
    {
        ePen = NO_PEN;
        return NO_ERROR;
    }

    if ((int) strlen (str) < (num_pens * 8))
    {
        return BAD_DEVICE_ID;
    }

    return NO_ERROR;
}

DRIVER_ERROR QuickConnect::VerifyPenInfo ()
{
    ePen = BOTH_PENS;
    DRIVER_ERROR err = NO_ERROR;

    if (IOMode.bDevID == FALSE)
        return err;

    ePen = NO_PEN;

    err = ParsePenInfo (ePen);

    if(err == UNSUPPORTED_PEN) // probably Power Off - pens couldn't be read
    {

        // have to delay or the POWER ON will be ignored
        if (pSS->BusyWait ((DWORD) 2000) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        DWORD length = sizeof (DJ895_Power_On);
        err = pSS->ToDevice (DJ895_Power_On, &length);
        ERRCHECK;

        err = pSS->FlushIO ();
        ERRCHECK;

        // give the printer some time to power up
        if (pSS->BusyWait ((DWORD) 2500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err = ParsePenInfo (ePen);
    }

    ERRCHECK;
    while (ePen == NO_PEN)
    {
        pSS->DisplayPrinterStatus (DISPLAY_NO_PENS);

        if (pSS->BusyWait (500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }
        err =  ParsePenInfo (ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus (DISPLAY_PRINTING);

    return err;
}

int QuickConnect::MapPaperSize (PAPER_SIZE ePaperSize)
{
    switch (ePaperSize)
    {
        case PHOTO_5x7:
            return 2;
        case L:
            return 3;
        case PHOTO_4x8:
            return 4;
        case PHOTO_4x12:
            return 5;
        case CARD_4x6:
        case HAGAKI:
        case A6:
            return 1;
        default:
            return 0;
    }
}

int QuickConnect::MapMediaType (MEDIATYPE eMediaType)
{
    switch (eMediaType)
    {
        case MEDIA_HIGHRES_PHOTO:
            return 1;
        case MEDIA_PREMIUM:
            return 2;
        case MEDIA_PLAIN:
            return 3;
        case MEDIA_PHOTO:
            return 4;
        case MEDIA_AUTO:
        default:
            return 0;
    }
}

int QuickConnect::MapQualityMode (QUALITY_MODE eQualityMode)
{
    switch (eQualityMode)
    {
        case QUALITY_NORMAL:
            return 2;
        case QUALITY_FASTNORMAL:
            return 3;
        case QUALITY_BEST:
            return 1;
        case QUALITY_AUTO:
        default:
            return 0;
    }
}

DRIVER_ERROR QuickConnect::SetHint (PRINTER_HINT eHint, int iValue)
{
    switch (eHint)
    {
        case PHOTO_FIX_HINT:
        {
            m_iPhotoFix = iValue & 0x1;
            break;
        }
        case RED_EYE_REMOVAL_HINT:
        {
            m_iRemoveRedEye = iValue & 0x1;
            break;
        }
        case MAX_FILE_SIZE_HINT:
        {
            m_iMaxFileSize = iValue;
            break;
        }
        default:
            break;
    }
    return NO_ERROR;
}


DRIVER_ERROR    QuickConnect::Flush (int iBufferSize)
{
    DRIVER_ERROR    err = NO_ERROR;
    if (iBufferSize == -1 && m_iRowNumber > 0)
    {
        char    szStr[256];
        int     iPaperSize, iMediaType, iPrintQuality;
        char    szCopiesStr[64];
        char    szNullBytes[632];
        static  const char    szPJLHeader[] = "\x1B\x45\x1B%-12345X@PJL ENTER LANGUAGE=PHOTOJPEG\012";
        static  const char    szPJLEndJob[] = "\x1B\x45\x1B%-12345X";

        err = Compress ();
        ERRCHECK;

        memset (szNullBytes, 0, sizeof (szNullBytes));
        sprintf (szCopiesStr, "@PJL SET COPIES=%d\012@PJL SET JOBID=%d\012", m_pPC->GetCopyCount (), m_iJobId);

        Send ((const BYTE *) szNullBytes, 600);
        Send ((const BYTE *) szPJLHeader, sizeof(szPJLHeader) - 1);
        Send ((const BYTE *) szCopiesStr, strlen (szCopiesStr));

        COLORMODE       eColorMode = COLOR;
        MEDIATYPE       eMediaType;
        QUALITY_MODE    eQualityMode;
        BOOL            bDeviceText;

        m_pPC->GetPrintModeSettings (eQualityMode, eMediaType, eColorMode, bDeviceText);
        iMediaType = MapMediaType (eMediaType);
        iPrintQuality = MapQualityMode (eQualityMode);
        iPaperSize = MapPaperSize (m_pPC->GetPaperSize ());

        sprintf (szStr,
                 "@PJL SET PAPER=%d\012@PJL SET MEDIATYPE=%d\012@PJL SET PRINTQUALITY=%d\012@PJL SET BORDERLESS=%d\012",
                 iPaperSize, iMediaType, iPrintQuality, m_pPC->IsBorderless ());
        err = Send ((const BYTE *) szStr, strlen (szStr));

        err = AddExifHeader ();
        ERRCHECK;

        memset (m_pbyInputBuffer, 0xFF, m_iInputBufferSize);

//      Send the END JOB PJL command

        Send ((const BYTE *) szPJLEndJob, sizeof(szPJLEndJob) - 1); 

        m_iRowNumber = 0;
    }
    return err;
}

HeaderQuickConnect::HeaderQuickConnect (Printer *p, PrintContext *pc)
    : Header (p,pc)
{ 
}

DRIVER_ERROR HeaderQuickConnect::Send ()
{
    return NO_ERROR;
}

DRIVER_ERROR HeaderQuickConnect::EndJob ()
{
    DRIVER_ERROR    err = NO_ERROR;

    return err;
}

DRIVER_ERROR HeaderQuickConnect::FormFeed ()
{
	DRIVER_ERROR    err = NO_ERROR;
    thePrinter->Flush (-1);
	return err;
}

DRIVER_ERROR HeaderQuickConnect::SendCAPy (unsigned int iAbsY)
{
    return NO_ERROR;
}

void HPFlush_output_buffer_callback (JOCTET *outbuf, BYTE *buffer, int size)
{
    QuickConnect    *pQC = (QuickConnect *) outbuf;
    pQC->JpegData (buffer, size);
}

void QuickConnect::JpegData (BYTE *buffer, int iSize)
{
    m_iJpegBufferPos += iSize;
    if (m_iJpegBufferPos < m_iMaxFileSize)
    {
        memcpy (m_pbyJpegBuffer + m_iJpegBufferPos - iSize, buffer, iSize);
    }
    else
    {
        m_iJpegBufferPos = m_iMaxFileSize + 1;
    }
}

//----------------------------------------------------------------
// These are "overrides" to the JPEG library error routines
//----------------------------------------------------------------

void HPJpeg_error (j_common_ptr cinfo)
{

}

extern "C"
{
void jpeg_buffer_dest (j_compress_ptr cinfo, JOCTET* outbuff, void* flush_output_buffer_callback);
void hp_rgb_ycc_setup (int iFlag);
}

DRIVER_ERROR  QuickConnect::Compress () 
{

	BYTE	*p;
    int     volatile iQuality = 100;

/*
 *  Convert the byte buffer to jpg, if converted size is greater than 2MB, delete it and
 *  convert with a higher compression factor.
 */

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;
    jmp_buf                     setjmp_buffer;

    BOOL    bRedo;

//  Use the standard RGB to YCC table rather than the modified one for JetReady

    hp_rgb_ycc_setup (0);
    do
    {
        bRedo = 0;

        m_iJpegBufferPos = 0;
        memset (m_pbyJpegBuffer, 0xFF, m_iMaxFileSize);

        cinfo.err = jpeg_std_error (&jerr);
        jerr.error_exit = HPJpeg_error;
        if (setjmp (setjmp_buffer))
        {
            jpeg_destroy_compress (&cinfo);
            return SYSTEM_ERROR;
        }

        jpeg_create_compress (&cinfo);
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults (&cinfo);
        cinfo.image_width = m_iRowWidth / 3;
        cinfo.image_height = m_iRowNumber;
        cinfo.input_components = 3;
        cinfo.data_precision = 8;
        jpeg_set_quality (&cinfo, iQuality, TRUE);
        jpeg_buffer_dest (&cinfo, (JOCTET *) this, (void *) (HPFlush_output_buffer_callback));
        jpeg_start_compress (&cinfo, TRUE);
        JSAMPROW    pRowArray[1];
        p = m_pbyInputBuffer;
        for (int i = 0; i < m_iRowNumber; i++)
        {
            pRowArray[0] = (JSAMPROW) p;
            jpeg_write_scanlines (&cinfo, pRowArray, 1);
            p += (m_iRowWidth);
            if (m_iJpegBufferPos > m_iMaxFileSize)
            {
                m_iJpegBufferPos = 0;
                bRedo = 1;
            }
        }
        jpeg_finish_compress (&cinfo);
        jpeg_destroy_compress (&cinfo);
        iQuality -= 10;
        if (iQuality == 0)
        {
            m_iJpegBufferPos = 0;
            return SYSTEM_ERROR;
        }
    } while (bRedo);

#ifdef _DEBUG
    char    szFileName[64];
    sprintf (szFileName, "C:\\temp\\HPJpeg.jpg");
    FILE    *fp = fopen (szFileName, "wb");
    if (fp)
    {
        fwrite (m_pbyJpegBuffer, 1, m_iJpegBufferPos, fp);
        fclose (fp);
    }
#endif
                    
    return NO_ERROR;
}

DRIVER_ERROR QuickConnect::AddExifHeader ()
{

    DRIVER_ERROR    err;
    BYTE            *pBuffer = m_pbyJpegBuffer;

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

    iOpts[0] = m_iRemoveRedEye;
    iOpts[1] = m_iBorderless;

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
    err = Send ((const BYTE *) pBuffer, sJFIFHeaderSize);
    ERRCHECK;

    m_iJpegBufferPos -= sJFIFHeaderSize;
    pBuffer += sJFIFHeaderSize;

    if (iNumTags != 0)
    {
        err = Send ((const BYTE *) App2, 13);
        for (int i = 0; i < iNumTags; i++)
        {
            err = Send ((const BYTE *) szApp2Markers[i], 12);
        }
    }

	err = Send ((const BYTE *) pBuffer, m_iJpegBufferPos);
    return err;
}

APDK_END_NAMESPACE

#endif  // defined APDK_QUICKCONNECT


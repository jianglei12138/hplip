/*****************************************************************************\
  dj9xxvip.cpp : Implimentation for the DJ9xxVIP class

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


#ifdef APDK_DJ9xxVIP

#include "header.h"
#include "io_defs.h"
#include "dj9xxvip.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE


#define OUR_PJL_JOBNAME "_PJL_pjl_PJL_pjl_" // this can be anything we want it to be
#define DRIVERWARE_JOBNAME  "NEWDRIVERWARE" // don't change this - it is defined in firmware!

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];

const char GrayscaleSeq[]= {ESC, '*', 'o', '5', 'W', 0x0B, 0x01, 0x00, 0x00, 0x02};

const BYTE ExtraDryTime[] = "\033&b16WPML \004\000\006\001\004\001\004\001\006\010\001";

extern BYTE EscAmplCopy(BYTE *dest, int num, char end);
extern void AsciiHexToBinary(BYTE* dest, char* src, int count);
extern MediaType MediaTypeToPcl (MEDIATYPE eMediaType);

DJ9xxVIP::DJ9xxVIP
(
    SystemServices* pSS,
    BOOL proto
) :
    Printer(pSS, NUM_DJ6XX_FONTS, proto),
    PCL3acceptsDriverware(TRUE)
{

    m_bVIPPrinter = TRUE;

    if (!proto && IOMode.bDevID)
    {
        bCheckForCancelButton = TRUE;
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen = BOTH_PENS;    // matches default mode

    PCL3acceptsDriverware = IsPCL3DriverwareAvailable();

    ModeCount = 0;
    pMode[ModeCount++] = new GrayModeDJ990(ulMapDJ600_CCM_K,PCL3acceptsDriverware);  // Grayscale K
    pMode[ModeCount++] = new DJ990Mode();           // Automatic Color
    pMode[ModeCount++] = new DJ990CMYGrayMode();    // Automatic Grayscale CMY

#ifdef APDK_AUTODUPLEX

/*
 *  When bidi is available, query printer for duplexer
 *  For now, this is available only on Linux which is unidi only.
 */

    bDuplexCapable = TRUE;
#endif 

#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[ModeCount++] = new DJ990KGrayMode ();        // Normal Grayscale K
    pMode[ModeCount++] = new DJ9902400Mode ();         // HiRes
    pMode[ModeCount++] = new DJ990DraftMode ();        // Draft Color
#endif
    pMode[ModeCount++] = new DJ990BestMode ();         // Photo Best
    pMode[ModeCount++] = new DJ990PhotoNormalMode ();  // Photo Normal

    m_cExtraDryTime    = 0;
    m_iLeftOverspray   = 0;
    m_iTopOverspray    = 0;
    m_iRightOverspray  = 0;
    m_iBottomOverspray = 0;
}

GrayModeDJ990::GrayModeDJ990
(
    uint32_t *map,
    BOOL PCL3OK
) :
    GrayMode(map)
{
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage=FALSE;

    if (!PCL3OK)
    {
        bFontCapable = FALSE;
    }

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#ifdef APDK_EXTENDED_MEDIASIZE
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
#endif
}

DJ990Mode::DJ990Mode()
: PrintMode(NULL)
{

/*
 *  The resolutions here are set to 300 for better performance from Cameras.
 *  REVISIT: Must provide a true 600 dpi printmode later.
 *  12/21/01
 *
 *  For now have added APDK_HIGH_RES_MODES which sets VIP_BASE_RES to 600 in dj9xxvip.h
 *  If APDK_HIGH_RES_MODES is not defined then VIP_BASE_RES is 300.
 *  1/9/2002 - JLM
 */

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

    medium = mediaAuto;     // enable media-detect

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
} //DJ990Mode


DJ990CMYGrayMode::DJ990CMYGrayMode()
: PrintMode(NULL)
{

/*
 *  See comments above regarding 300/600 dpi change
 */

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

    medium = mediaAuto;     // enable media-detect

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    pmColor = GREY_CMY;
    bFontCapable = FALSE;
} //DJ990CMYGrayMode

#ifdef APDK_EXTENDED_MEDIASIZE
DJ990KGrayMode::DJ990KGrayMode () : PrintMode (NULL)
{

/*
 *  See comments above regarding 300/600 dpi change.
 */

    BaseResX = BaseResY = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;
    medium = mediaPlain;
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
    pmColor = GREY_K;
    bFontCapable = FALSE;
    dyeCount = 1;
} //DJ990KGrayMode

DJ9902400Mode::DJ9902400Mode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 1200;
    ResolutionX[0] = 1200;
    ResolutionY[0] = 1200;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium = mediaHighresPhoto;
    theQuality = qualityPresentation;
    pmMediaType = MEDIA_PHOTO;
    pmQuality = QUALITY_HIGHRES_PHOTO;
} // DJ9902400Mode

DJ990DraftMode::DJ990DraftMode () : PrintMode (NULL)
{
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

   Config.bColorImage = FALSE;

    medium = mediaAuto;
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
} // DJ990DraftMode

#endif  // APDK_EXTENDED_MEDIASIZE

/*
 *  Some VIP printers do not have Media Sensing device. To enable
 *  selection of Photo/Best mode for these printers, use this mode.
 */

DJ990BestMode::DJ990BestMode () : PrintMode (NULL)
{
    BaseResX = BaseResY = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

	bFontCapable = FALSE;

#ifdef  APDK_AUTODUPLEX
	bDuplexCapable = TRUE;
#endif

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

   Config.bColorImage = FALSE;

	medium      = mediaGlossy;
	theQuality  = qualityPresentation;
	pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;
} // DJ990BestMode

DJ990PhotoNormalMode::DJ990PhotoNormalMode () : PrintMode (NULL)
{
    BaseResX = BaseResY = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

	bFontCapable = FALSE;

#ifdef  APDK_AUTODUPLEX
	bDuplexCapable = TRUE;
#endif

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

   Config.bColorImage = FALSE;

	medium      = mediaGlossy;
	theQuality  = qualityNormal;
	pmQuality   = QUALITY_NORMAL;
    pmMediaType = MEDIA_PHOTO;
} // DJ990PhotoNormalMode

BOOL DJ9xxVIP::UseGUIMode
(
    PrintMode* pPrintMode
)
{
	// The reason to change all print mode in DJ9xxVIP family to PCL3GUI mode is that 
	// the PCL3 path in VIP printers is not tested.  They don't support newer CRD 
	// command.  The device fint support for this group is not recommended.

	return TRUE;
	/*
	if (pPrintMode->medium == mediaHighresPhoto)
	{
		return TRUE;
	}
	else
	{
#ifdef APDK_AUTODUPLEX
		if (pPrintMode->QueryDuplexMode() != DUPLEXMODE_NONE)
		{
			return TRUE;
		}
#endif
	}
#if defined(APDK_FONTS_NEEDED)
//    return ((!pPrintMode->bFontCapable) && (!PCL3acceptsDriverware));
    return (!pPrintMode->bFontCapable);
#else
    return TRUE;
#endif
	*/ 
} //UseGUIMode

DRIVER_ERROR DJ9xxVIP::AddPJLHeader ()
{
    char            *szPJLBuffer = NULL;
    DRIVER_ERROR    err = NO_ERROR;
    int             iPJLBufferSize;
    if (((iPJLBufferSize = pSS->GetPJLHeaderBuffer (&szPJLBuffer)) > 0) && (szPJLBuffer != NULL))
    {
        err = Send ((const BYTE *) szPJLBuffer, iPJLBufferSize);
    }
    return err;
}

Mode10::Mode10
(
    SystemServices* pSys,
    Printer* pPrinter,
    unsigned int PlaneSize
) :
    Compressor(pSys, PlaneSize, TRUE),
    thePrinter(pPrinter)    // needed by Flush
{
    if (constructor_error != NO_ERROR)  // if error in base constructor
    {
        return;
    }

	// In the worst case, compression expands data by 50%
	compressBuf = (BYTE*)pSS->AllocMem(PlaneSize + PlaneSize/2);
	if (compressBuf == NULL)
		constructor_error=ALLOCMEM_ERROR;

    memset(SeedRow,0xFF,PlaneSize);
} //Mode10


Mode10::~Mode10()
{ }


const BYTE ResetSeedrow[]={ESC,'*','b','0','Y'};

void Mode10::Flush()
// special problem here regarding white rasters: we can't just reset our seedrow to white,
// because if a true white raster comes next it will compress to zero, and firmware will
// print ITS seedrow. So when we zero our seedrow, we need to zero the firmware seedrow.
// The way to do this is to send ESC*b0Y. So that's what we will do here.
{
    if (!seeded)
    {
        return;
    }
	compressedsize=0;
	
    iRastersReady = 0;
    seeded = FALSE;
    memset(SeedRow,0xFF,inputsize);
    thePrinter->Send(ResetSeedrow, sizeof(ResetSeedrow));
} //Flush


Compressor* DJ9xxVIP::CreateCompressor(unsigned int RasterSize)
{
    return new Mode10(pSS,this,RasterSize);
}


Header* DJ9xxVIP::SelectHeader(PrintContext* pc)
{
    return new HeaderDJ990(this,pc);
}


HeaderDJ990::HeaderDJ990(Printer* p,PrintContext* pc)
    : Header(p,pc)
{
    SetMediaSource (pc->GetMediaSource());
}


DRIVER_ERROR HeaderDJ990::ConfigureRasterData()
// This is the more sophisticated way of setting color and resolution info.
//
// NOTE: Will need to be overridden for DJ5xx.
{

	char buff[50];      // 20 + 3 for crdstart + 3 for ##W
	char *out = buff;


    // begin the CRD command
    memcpy(out,crdStart,sizeof(crdStart) );
    out += sizeof(crdStart);

    // now set up the "#W" part, where #= number of data bytes in the command
    // #= 20
    //      format,ID,components(2bytes),resolutions (4bytes),
    //      compresssion method, orientation, bits/component,planes/component
	*out++ = 0x32;    // "2"
    *out++ = 0x30;    // "0"
    *out++ = 'W';

#define VIPcrdFormat 6
#define VIPKRGBID 0x1F

    *out++ = VIPcrdFormat;
    *out++ = VIPKRGBID;

    // 2-byte component count field
    // number of components for sRGB is 1 by defintiion
    *out++ = 0;     // leading byte
    *out++ = 2;     // 2 "component"

    *out++ = ResolutionX[0]/256;
    *out++ = ResolutionX[0]%256;
    *out++ = ResolutionY[0]/256;
    *out++ = ResolutionY[0]%256;

    // compression method
    *out++ = 9;    // mode 9 compression

    // orientation
    *out++ = 0;     // pixel major code

    // bits per component
    *out++ = 1;    // 1 bits each for k

    // planes per component
    *out++ = 1;     // K = one component, one "plane"

    *out++ = ResolutionX[0]/256;
    *out++ = ResolutionX[0]%256;
    *out++ = ResolutionY[0]/256;
    *out++ = ResolutionY[0]%256;

    // compression method
    *out++ = 10;    // mode 10 compression

    // orientation
    *out++ = 1;     // pixel major code

    // bits per component
    *out++ = 32;    // 8 bits each for s,R,G,B

    // planes per component
    *out++ = 1;     // sRGB = one component, one "plane"

    return thePrinter->Send((const BYTE*) buff, out-buff);
} //ConfigureRasterData


DRIVER_ERROR HeaderDJ990::ConfigureImageData()
{
    DRIVER_ERROR err = thePrinter->Send((const BYTE*)cidStart, sizeof(cidStart));
    ERRCHECK;
    char sizer[3];
    sprintf(sizer,"%dW",6);
    err = thePrinter->Send((const BYTE*)sizer,2);
    ERRCHECK;

    BYTE colorspace = 2;    // RGB
    BYTE coding = 3;
    BYTE bitsindex = 3;     // ??
    BYTE bitsprimary = 8;
    BYTE CID[6];
    CID[0] = colorspace;
    CID[1]=coding;
    CID[2] = bitsindex;
    CID[3] = CID[4] = CID[5] = bitsprimary;

    return thePrinter->Send((const BYTE*) CID, 6);
} //ConfigureImageData


DRIVER_ERROR HeaderDJ990::Send()
/// ASSUMES COMPRESSION ALWAYS ON -- required by DJ990
{
    DRIVER_ERROR err;
//    PRINTMODE_VALUES    *pPMV;
    COLORMODE       eColorMode = COLOR;
    MEDIATYPE       eMediaType;
    QUALITY_MODE    eQualityMode;
    BOOL            bDeviceText;

    thePrintContext->GetPrintModeSettings (eQualityMode, eMediaType, eColorMode, bDeviceText);

    if (eMediaType == MEDIA_CDDVD)
    {
        thePrintContext->SetMediaSource (sourceTrayCDDVD);
        SetMediaSource (sourceTrayCDDVD);
    }

    StartSend();

#ifdef APDK_AUTODUPLEX
    if (thePrintContext->QueryDuplexMode () != DUPLEXMODE_NONE)
    {
        err = thePrinter->Send ((const BYTE *) EnableDuplex, sizeof (EnableDuplex));
        BYTE    cDryTime;
        cDryTime = (BYTE) ((thePrinter->GetHint (EXTRA_DRYTIME_HINT)) & 0xFF);
        if (cDryTime != 0)
        {
            err = thePrinter->Send (ExtraDryTime, sizeof (ExtraDryTime) - 1);
            err = thePrinter->Send ((const BYTE *) &cDryTime, 1);
            ERRCHECK;
        }
    }
#endif

    err = ConfigureImageData();
    ERRCHECK;

    err = ConfigureRasterData();
    ERRCHECK;

    if (thePrintMode->dyeCount == 1)    // grayscale
    {
        err=thePrinter->Send((const BYTE*)GrayscaleSeq, sizeof(GrayscaleSeq) );
        ERRCHECK;
    }
    else
    {
        if (eColorMode == GREY_CMY)
        {
            char    pStr[12];
            memcpy (pStr, GrayscaleSeq, 10);
            pStr[9] = 0x01;
            err = thePrinter->Send ((const BYTE *) pStr, 10);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////
    // unit-of-measure command --- seems to be need in DJ990 PCL3 mode
    char uom[10];
    sprintf(uom,"%c%c%c%d%c",ESC,'&','u',thePrintMode->ResolutionX[K],'D');
    err = thePrinter->Send((const BYTE*)uom, strlen (uom));
    ERRCHECK;

    // another command that helps PCLviewer
    unsigned int width=thePrintContext->OutputPixelsPerRow();
    unsigned int digits = 1;
    unsigned int x = width;
    while (x > 10)
    {
        digits++;
        x = x / 10;
    }
    sprintf(uom,"%c%c%c%d%c",ESC,'*','r', width,'S');
    err = thePrinter->Send((const BYTE*)uom, 4 + digits );
    ERRCHECK;

    ////////////////////////////////////////////////////////////////////////////////////

/*
 *  Custom papersize command
 */

    if (thePrintContext->thePaperSize == CUSTOM_SIZE)
    {
        BYTE    szStr[32];
        short   sWidth, sHeight;
        BYTE    b1, b2;
        sWidth  = (short) (thePrintContext->PhysicalPageSizeX () * thePrintContext->EffectiveResolutionX ());
        sHeight = (short) (thePrintContext->PhysicalPageSizeY () * thePrintContext->EffectiveResolutionY ());
        memcpy (szStr, "\x1B*o5W\x0E\x05\x00\x00\x00\x1B*o5W\x0E\x06\x00\x00\x00", 20);
        b1 = (BYTE) ((sWidth & 0xFF00) >> 8);
        b2 = (BYTE) (sWidth & 0xFF);
        szStr[8] = b1;
        szStr[9] = b2;
        b1 = (BYTE) ((sHeight & 0xFF00) >> 8);
        b2 = (BYTE) (sHeight & 0xFF);
        szStr[18] = b1;
        szStr[19] = b2;
        err = thePrinter->Send ((const BYTE *) szStr, 20);
    }

    float   fXOverSpray = 0.0;
    float   fYOverSpray = 0.0;
    float   fLeftOverSpray = 0.0;
    float   fTopOverSpray  = 0.0;
	FullbleedType   fbType;
    if (thePrintContext->bDoFullBleed &&
        thePrinter->FullBleedCapable (thePrintContext->thePaperSize,
									  &fbType,
                                      &fXOverSpray, &fYOverSpray,
                                      &fLeftOverSpray, &fTopOverSpray))
    {

/*
 *      To get the printer to do fullbleed printing, send the top and
 *      left overspray commands. Overspray is needed to take care of
 *      skew during paper pick. These values may be mech dependent.
 *      Currently, supported only on PhotoSmart 100. Malibu supports
 *      fullbleed printing also. The current values for overspray are
 *      0.059 inch for top, bottom and left edges and 0.079 for right edge.
 */

        BYTE cBuf[4];
        BYTE TopOverSpraySeq[]  = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0E, 0x02, 0x00};
                                // "Esc*o5W 0E 02 00 00 00" Top edge overspray for full-bleed printing

        BYTE LeftOverSpraySeq[] = {0x1b, 0x2A, 0x6F, 0x35, 0x57, 0x0E, 0x01, 0x00};
                                // "Esc*o5W 0E 01 00 00 00" Left edge overspray for full-bleed printing

        short topspray = (short)(fTopOverSpray * thePrintContext->EffectiveResolutionY() + 0.5);
        cBuf[1] = topspray & 0xFF;
        cBuf[0] = topspray >> 8;

        err = thePrinter->Send ((const BYTE *) TopOverSpraySeq, sizeof (TopOverSpraySeq));
        err = thePrinter->Send ((const BYTE *) cBuf, 2);

        // set the left overspray value based on resolution and global constant for horizontal overspray
        short leftspray = (short)(fLeftOverSpray * thePrintContext->EffectiveResolutionX() + 0.5);
        cBuf[1] = leftspray & 0xFF;
        cBuf[0] = leftspray >> 8;

        err = thePrinter->Send ((const BYTE *) LeftOverSpraySeq, sizeof (LeftOverSpraySeq));
        err = thePrinter->Send ((const BYTE *) cBuf, 2);
    }

//  Now send media pre-load command
    err = thePrinter->Send ((const BYTE *) "\033&l-2H", 6);   // Moved from Modes(), des 3/11/03
    ERRCHECK;

//  Send speed mech command
    thePrinter->SetHint (SPEED_MECH_HINT, 0);

    // no need for compression command, it's in the CRD

    err = thePrinter->Send((const BYTE*)grafStart, sizeof(grafStart) );
    ERRCHECK;

    if (!thePrinter->UseGUIMode (thePrintContext->CurrentMode) &&
        (thePrintContext->PhysicalPageSizeX ()) < 8.0)
    {

        BYTE    blankRow[40];
        memset (blankRow, 0xFF, 40);
        strcpy ((char *) blankRow, "\033*p0Y\033*b23W");
        blankRow[11] = 18;   // ce, 7f
        blankRow[30] = 0xCE; blankRow[31] = 0x7F;
        err = thePrinter->Send ((const BYTE *) blankRow, 34);
    }

    return err;
} //Send


DRIVER_ERROR HeaderDJ990::StartSend()
{
    DRIVER_ERROR err;

    err = thePrinter->Send((const BYTE*)Reset,sizeof(Reset));
    ERRCHECK;

    err = thePrinter->Send((const BYTE*)UEL,sizeof(UEL));
    ERRCHECK;

    err = thePrinter->AddPJLHeader ();
    ERRCHECK;

    err = thePrinter->Send((const BYTE*)EnterLanguage,sizeof(EnterLanguage));
    ERRCHECK;

    if (!thePrinter->UseGUIMode(thePrintContext->CurrentMode))
    {
        err = thePrinter->Send((const BYTE*)PCL3,sizeof(PCL3));
    }
    else
    {
        err = thePrinter->Send((const BYTE*)PCLGUI,sizeof(PCLGUI));
    }
    ERRCHECK;


    err = thePrinter->Send((const BYTE*)&LF,1);
    ERRCHECK;

    err = Modes ();            // Set media source, type, size and quality modes.
    ERRCHECK;

//  Send media subtype if set
    int    iMediaSubtype = thePrintContext->GetMediaSubtype ();
    if (iMediaSubtype != APDK_INVALID_VALUE)
    {
        BYTE    szMediaSubtypeSeq[] = {0x1B, '*', 'o', '5', 'W', 0x0D, 0x03, 0x00, 0x00, 0x00};
        szMediaSubtypeSeq[8] = (BYTE) ((iMediaSubtype & 0xFF00) >> 8);
        szMediaSubtypeSeq[9] = (BYTE) (iMediaSubtype & 0x00FF);
        err = thePrinter->Send ((const BYTE *) szMediaSubtypeSeq, sizeof(szMediaSubtypeSeq));
        ERRCHECK;
    }

    if (!thePrinter->UseGUIMode(thePrintContext->CurrentMode))
    {
        err = Margins();          // set margins
    }

// special GUI mode top margin set

    else if ((thePrintContext->PrintableStartY ()) > 0.0)
    {
        CAPy = thePrintContext->GUITopMargin();
    }

    if ((thePrintContext->GetMediaSource()) == sourceTrayCDDVD)
    {
        err = thePrinter->Send ((const BYTE *) "\033*o5W\x0D\x03\x00\x04\x0C", 10);
        ERRCHECK;
    }

    return err;
} //StartSend

/* This could replace Header::SetMediaSource in header.cpp. des 8/5/02 */
void HeaderDJ990::SetMediaSource(MediaSource msource)
// Sets value of PCL::mediasource and associated counter msrccount
{
    msrccount=EscAmplCopy((BYTE*)mediasource,msource,'H');
    if (msource == sourceTrayCDDVD)
    {
        SetMediaType (mediaCDDVD);
        SetQuality (qualityPresentation);
        return;
    }
    if (msource == sourceTray2 || msource > sourceTrayAuto)
    {
        SetMediaType (mediaPlain);
    }
}

DRIVER_ERROR DJ9xxVIP::VerifyPenInfo()
{
    DRIVER_ERROR err=NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;
    err = ParsePenInfo(ePen);

    if(err == UNSUPPORTED_PEN) // probably Power Off - pens couldn't be read
    {
        DBG1("DJ9xxVIP::Need to do a POWER ON to get penIDs\n");

        // have to delay or the POWER ON will be ignored
        if (pSS->BusyWait((DWORD)2000) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        DWORD length = sizeof(DJ895_Power_On);
        err = pSS->ToDevice(DJ895_Power_On,&length);
        ERRCHECK;

        err = pSS->FlushIO();
        ERRCHECK;

        // give the printer some time to power up
        if (pSS->BusyWait ((DWORD) 2500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err = ParsePenInfo(ePen);
    }

    ERRCHECK;

    // check for the normal case
    if (ePen == BOTH_PENS || ePen == MDL_BLACK_AND_COLOR_PENS)
    {
        return NO_ERROR;
    }

//  Should we return NO_ERROR for MDL_BOTH also - malibu??

    while ( ePen != BOTH_PENS &&  pSS->GetVIPVersion () == 1  )
    {

        switch (ePen)
        {
            case BLACK_PEN:
                // black pen installed, need to install color pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_COLOR_PEN);
                break;
            case COLOR_PEN:
                // color pen installed, need to install black pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_BLACK_PEN);
                break;
            case NO_PEN:
                // neither pen installed
            default:
                pSS->DisplayPrinterStatus(DISPLAY_NO_PENS);
                break;
        }

        if (pSS->BusyWait(500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;
} //VerifyPenInfo


DRIVER_ERROR DJ9xxVIP::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char* str;
    int num_pens = 0;
    PEN_TYPE temp_pen1 = NO_PEN;
    BYTE penInfoBits[4];
    int     i;

/*
 *  First check if this printer's firmware version is one I know about. Currently
 *  I know upto S:04. I can't guess if the pen info nibbles have shifted in the
 *  new version. Can't tell the user about missing pens.
 */

    if ((pSS->GetVIPVersion ()) > 5)
    {
        ePen = BOTH_PENS;
        return NO_ERROR;
    }

    DRIVER_ERROR err = SetPenInfo (str, QueryPrinter);
//    ERRCHECK;
    if (err != NO_ERROR)
    {
        ePen = BOTH_PENS;
        return NO_ERROR;
    }
    iNumMissingPens = 0;

    // the first byte indicates how many pens are supported
    if ((str[0] >= '0') && (str[0] <= '9'))
    {
        num_pens = str[0] - '0';
    }
    else if ((str[0] >= 'A') && (str[0] <= 'F'))
    {
        num_pens = 10 + (str[0] - 'A');
    }
    else if ((str[0] >= 'a') && (str[0] <= 'f'))
    {
        num_pens = 10 + (str[0] - 'a');
    }
    else
    {
        return BAD_DEVICE_ID;
    }

    if ((int) strlen (str) < (num_pens * 8))
    {
        return BAD_DEVICE_ID;
    }

//  DJ990 style DevID

    if (pSS->GetVIPVersion () == 1)
    {
        if (num_pens < 2)
        {
            return UNSUPPORTED_PEN;
        }

        // parse pen1 (should be black)
        AsciiHexToBinary(penInfoBits, str+1, 4);
        penInfoBits[1] &= 0xf8; // mask off ink level trigger bits

        if ((penInfoBits[0] == 0xc1) && (penInfoBits[1] == 0x10))
        {
            temp_pen1 = BLACK_PEN;
        }
        else if (penInfoBits[0] == 0xc0)
        {   // missing pen
            temp_pen1 = NO_PEN;
            iNumMissingPens = 1;
        }
        else
        {
            return UNSUPPORTED_PEN;
        }

        // now check pen2 (should be color)
        AsciiHexToBinary(penInfoBits, str+9, 4);
        penInfoBits[1] &= 0xf8; // mask off ink level trigger bits

        if ((penInfoBits[0] == 0xc2) && (penInfoBits[1] == 0x08))
        {   // Chinook
            if (temp_pen1 == BLACK_PEN)
            {
                ePen = BOTH_PENS;
            }
            else
            {
                ePen = COLOR_PEN;
            }
        }
        else if (penInfoBits[0] == 0xc0)
        {   // missing pen
            ePen = temp_pen1;
            iNumMissingPens = 1;
        }
        else
        {
            return UNSUPPORTED_PEN;
        }

        return NO_ERROR;
    }

//  Check for missing pens

    if (*(str - 1) == '1' && *(str - 2) == '1')
    {
        return UNSUPPORTED_PEN;
    }

    char    *p = str + 1;

/*
 *  Pen Type Info
 *
    Bit 31 (1 bit)

    1 if these fields describe a print head
    0 otherwise
    Bit 30 (1 bit)

    1 if these fields describe an ink supply
    0 otherwise

    Bits 29 .. 24 (6 bits) describes the pen/supply type:

    0  = none
    1  = black
    2  = CMY
    3  = KCM
    4  = Cyan
    5  = Meganta
    6  = Yellow
	7  = Cyan - low dye load 
    8  = Magenta - low dye load 
    9  = Yellow - low dye load (may never be used, but reserve space anyway) [def added Jun 3, 2002]
    10 = gGK - two shades of grey plus black; g=light grey, G=medium Grey, K=black  [added Sep 12, 02]
    11 = Blue Pen
    12 .. 62 = reserved for future use
    63 = Unknown

    Bits 23 .. 19 (5 bits) describes the pen/supply id:

    0 = none
    1 = color (Formerly N)
    2 = black (Formerly H)
    3 = Flash (Formerly F)
    4 = (Formerly R -- only 6xx family)
    5 = (Formerly C -- only 6xx family)
    6 = (Formerly M -- only for 6xx family)
    7 = (Cp1160 Pen)
    8 = Europa (Jupiter Ink)
    9 = Wax (pen/ink combo; k)  
    10 = (pen/ink combo; cmy)  
    11 = (pen/ink combo; kcm) 
    12 = (pen/ink combo; k)    [def added Jun 27, 2002]
    13 = (k)  [def added Jun 27, 2002]
    14 = pen/ink combo; gGK)  [added Sep 12, 02] 
    15 .. 30 = reserved for future use    [def added Jun 27, 2002]
    31 = Other/Unknown  [def added Jun 27, 2002]
 */

    ePen = NO_PEN;

    if (pSS->GetVIPVersion () == 3)
    {
        for (i = 0; i < num_pens; i++, p += 4)
        {
            if (*p > 0 && *p < '5')
            {
                continue;
            }
            switch (*p)
            {
                case 0:
                {
                    iNumMissingPens++;
                    break;
                }
                case '5':
                {
                    ePen = BLACK_PEN;
                    break;
                }
                case '6':
                case '7':
                case '8':
                {
                    if (ePen == BLACK_PEN)
                    {
                        ePen = BOTH_PENS;
                    }
                    else if (ePen != BOTH_PENS)
                    {
                        ePen = COLOR_PEN;
                    }
                    break;
                }
            }

        }
        if (iNumMissingPens != 0)
        {
            return MISSING_PENS;
        }
        return NO_ERROR;
    }

#if 0

    BYTE    penColor;

/*
 *  These printers don't need all pens to be installed to print. Parsing the pen info
 *  is not all that useful. Also, the note about incompatible pen is not valid anymore,
 *  it just means unknown or other pen.
 *  Raghu - 11/17/05
 */

    for (i = 0; i < num_pens; i++, p += 8)
    {
        AsciiHexToBinary (penInfoBits, p, 8);

        if ((penInfoBits[1] & 0xf8) == 0xf8)
        {

//          The high 5 bits in the 3rd and 4th nibble (second byte) identify the
//          installed pen. If all 5 bits are on, user has installed an incompatible pen.

            return UNSUPPORTED_PEN;
        }

        if ((penInfoBits[0] & 0x40) != 0x40)         // if Bit 31 is 1, this is not a pen
        {
            continue;
        }
        penColor = penInfoBits[0] & 0x3F;
        switch (penColor)
        {
            case 0:
            {
                iNumMissingPens++;
                break;
            }
            case 1:
                ePen = BLACK_PEN;
                break;
            case 2:
            {
                if (ePen == BLACK_PEN)
                {
                    ePen = BOTH_PENS;
                }
                else if (ePen == MDL_PEN)
                {
                    ePen = MDL_BOTH;
                }
                else if (ePen == GREY_PEN)
                {
                    ePen = GREY_BOTH;
                }
                else
                {
                    ePen = COLOR_PEN;
                }
                break;
            }
            case 3:
                if (ePen == BLACK_PEN)
                {
                    ePen = MDL_AND_BLACK_PENS;
                }
                else if (ePen == COLOR_PEN)
                {
                    ePen = MDL_BOTH;
                }
                else if (ePen == BOTH_PENS)
                {
                    ePen = MDL_BLACK_AND_COLOR_PENS;
                }
                else if (ePen == GREY_PEN)
                {
                    ePen = MDL_AND_GREY_PENS;
                }
                else if (ePen == GREY_BOTH)
                {
                    ePen = MDL_GREY_AND_COLOR_PENS;
                }
                else
                {
                    ePen = MDL_PEN;
                }
                break;
            case 4:             // cyan pen
            case 5:             // magenta pen
            case 6:             // yellow pen
            case 7:             // low dye load cyan pen
            case 8:             // low dye load magenta pen
            case 9:             // low dye load yellow pen
            case 11:            // blue pen
                if (ePen == BLACK_PEN || ePen == BOTH_PENS)
                {
                    ePen = BOTH_PENS;
                }
                else
                {
                    ePen = COLOR_PEN;
                }
                break;
            case 10:
                ePen = GREY_PEN;
                break;
            default:
                ePen = UNKNOWN_PEN;
        }
    }
    return NO_ERROR;
#endif

    ePen = BOTH_PENS;
    return NO_ERROR;

} //ParsePenInfo


BOOL DJ9xxVIP::IsPCL3DriverwareAvailable()
{
#ifdef  APDK_LINUX
    // Linux supports Bi-Di, but is not enabled at this time. See comments below.
    return TRUE;
#else

    BOOL pcl3driverware = TRUE;     // default to TRUE since this is the case for all but some early units
    BOOL inAJob = FALSE;
    char *pStr;
    char *pEnd;
    BYTE devIDBuff[DevIDBuffSize];
    int maxWaitTries;
    int i;

    // if don't have bidi can't check so assume driverware is ok since only certain
    // 990s don't handle driverware in PCL3 mode
    if (!IOMode.bDevID)
    {
        return TRUE;
    }

    if (pSS->GetDeviceID(devIDBuff, DevIDBuffSize, TRUE) != NO_ERROR)
    {
        goto cleanup;
    }

    // if printer does not have firmware based on the first 990 release
    // don't bother checking, can assume driverware commands are OK
    if (!strstr((const char*)devIDBuff+2,"MDL:DESKJET 990C") &&
        !strstr((const char*)devIDBuff+2,"MDL:PHOTOSMART 1215") &&
        !strstr((const char*)devIDBuff+2,"MDL:PHOTOSMART 1218") )
    {
        return TRUE;
    }

    // high-level process to check if driverware is available in PCL3 mode:
    //  1. set JobName through the normal PJL command to some string X
    //  2. poll DeviceID until see this JobName (syncs host with printer in case
    //     printer is still printing an earlier job etc.)
    //  3. go into PCL3 mode
    //  4. send driverware command to set JobName
    //  5. get the DeviceID and look at the JobName, if it is not "NEWDRIVERWARE" (what
    //     the firmware driverware command sets it to) conclude that driverware is
    //     not available in PCL3 mode; if  the JobName is "NEWDRIVERWARE" then conclude
    //     that driverware is available in PCL3 mode
    //  6. exit this "job" (send a UEL)



    // set the JobName via PJL
    if (Flush() != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)UEL,sizeof(UEL)) != NO_ERROR) goto cleanup;
    inAJob = TRUE;
    if (Send((const BYTE*)JobName,sizeof(JobName)) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)&Quote,1) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)OUR_PJL_JOBNAME,strlen(OUR_PJL_JOBNAME)) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)&Quote,1) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)&LF,1) != NO_ERROR) goto cleanup;
    if (Flush() != NO_ERROR) goto cleanup;          // we flush our ::Send buffer
    if (pSS->FlushIO() != NO_ERROR) goto cleanup;   // we flush any external I/O buffer

    // wait for printer to see this and set JobName in the DeviceID
    // we know printer will respond, it is just a matter of time so wait
    // a while until see it since it may take a few seconds
    // for it to sync up with us if it is busy printing or picking
    //
    // this is a pretty long timeout but if the printer is finishing up
    // a preceding job we need to give it time to finish it since it won't
    // set the new jobname until the last job is complete
    // one possible enhancement would be to look at the flags in the DeviceID
    // to see if a job is active on more than one i/o connection
    maxWaitTries = 120;  // wait max of 60sec
    for (i=0; i<maxWaitTries; i++)
    {
        if (pSS->GetDeviceID(devIDBuff, DevIDBuffSize, TRUE) != NO_ERROR)    goto cleanup;
        if ( (pStr=(char *)strstr((const char*)devIDBuff+2,";J:")) )
        {
            pStr += 3;
            if ( (pEnd=(char *)strstr((const char*)pStr,";")) )
            {
                *pEnd = '\0';
                while (pEnd > pStr) // take out trailing spaces in JobName before compare
                {
                    if (*(pEnd-1) == ' ')
                    {
                        *(pEnd-1) = '\0';
                    }
                    else
                    {
                        break;
                    }
                    pEnd--;
                }
                if (!strcmp(pStr, OUR_PJL_JOBNAME))
                {
                    break;
                }
            }
            pSS->BusyWait((DWORD)500);
        }
        else
        {
            DBG1("JobName missing from DeviceID strings");
            goto cleanup;
        }
    }
    if (i>=maxWaitTries)
    {
        // printer didn't respond to driverware in PCL3GUI mode withing allowed timeout
        DBG1("Printer didn't respond to PJL\n");
        goto cleanup;
    }

    // now printer is in sync with us so try PCL3 mode and expect it to react to command
    // immediately or will assume that it ignores driverware in that mode
    if (Send((const BYTE*)EnterLanguage,sizeof(EnterLanguage)) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)PCL3,sizeof(PCL3)) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)&LF,1) != NO_ERROR) goto cleanup;
    if (Send((const BYTE*)DriverwareJobName,sizeof(DriverwareJobName)) != NO_ERROR) goto cleanup;
    if (Flush() != NO_ERROR) goto cleanup;          // we flush our ::Send buffer
    if (pSS->FlushIO() != NO_ERROR) goto cleanup;   // we flush any external I/O buffer

    // wait for printer to see this and set DeviceID to reflect this command
    // since we are sending in PCL3 mode we don't know if printer will respond
    // so don't wait very long
    maxWaitTries = 4;
    for (i=0; i<maxWaitTries; i++)
    {
        if (pSS->GetDeviceID(devIDBuff, DevIDBuffSize, TRUE) != NO_ERROR)    goto cleanup;
        if ( (pStr=(char *)strstr((const char*)devIDBuff+2,";J:")) )
        {
            pStr += 3;
            if ( (pEnd=(char *)strstr((const char*)pStr,";")) )
            {
                *pEnd = '\0';
                // firmware may have garbage in remainder of JobName buffer - truncate
                // it to the length of the string that should be set before compare
                if (!strncmp(pStr, DRIVERWARE_JOBNAME, strlen(DRIVERWARE_JOBNAME)))
                {
                    pcl3driverware = TRUE;
                    break;
                }
            }
            pSS->BusyWait((DWORD)500);
        }
        else
        {
            DBG1("JobName missing from DeviceID string");
            goto cleanup;
        }
    }
    if (i>=maxWaitTries)
    {
        // since we haven't gotten a response assume that the printer ignores driverware
        // commands in PCL3 mode
        pcl3driverware = FALSE;
    }


cleanup:
    if (inAJob)   // send UEL in case left printer in the context of a job
    {
        if (Send((const BYTE*)UEL,sizeof(UEL)) != NO_ERROR) goto bailout;
        if (pSS->FlushIO() != NO_ERROR) goto bailout;
    }

bailout:
    return pcl3driverware;

#endif  // APDK_LINUX

} //IsPCL3DriverwareAvailable


DISPLAY_STATUS DJ9xxVIP::ParseError(BYTE status_reg)
{
    DBG1("DJ9xxVIP, parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE DevIDBuffer[DevIDBuffSize];
    char *pStr;
    int     iVersion = pSS->GetVIPVersion ();

    if(IOMode.bDevID && iVersion < 6)
    {
        // If a bi-di cable was plugged in and everything was OK, let's see if it's still
        // plugged in and everything is OK
        err = pSS->GetDeviceID (DevIDBuffer, DevIDBuffSize, TRUE);
        if(err != NO_ERROR)
        {
            // job was bi-di but now something's messed up, probably cable unplugged
            return DISPLAY_COMM_PROBLEM;
        }

        if ( (pStr=(char *)strstr((const char*)DevIDBuffer+2,";S:")) == NULL )
        {
            return DISPLAY_COMM_PROBLEM;
        }

        // point to PrinterState
        BYTE b1,b2;

        pStr+=5;   // 3 for ";S:", 2 for version -- point to first byte of "printer state"
        b1=*pStr;

        if (b1=='9')
        {
            return DISPLAY_TOP_COVER_OPEN;
        }

        if (iVersion < 3)
        {
            pStr += 12;     // point to "feature state"
        }
        else if (iVersion < 5)
        {
            pStr += 14;
        }
        else
        {
            pStr += 18;
        }

        b1=*pStr++;
        b2=*pStr++;
        if ((b1=='0') && (b2=='9'))     // 09 = OOP state
        {
            DBG1("Out of Paper [from Encoded DevID]\n");
            return DISPLAY_OUT_OF_PAPER;
        }

        if (b1 == '0' && (b2 == 'c' || b2 == 'C'))      // 0C - PhotoTray Mismatch
        {

/*
 *          PhotoTray is engaged, but requested paper size is larger than A6.
 *          It may also mean A6/Photo size is requested but photo tray is not engaged.
 */

            DBG1("Photo Tray Mismatch [from Encoded DevID]\n");
            return DISPLAY_PHOTOTRAY_MISMATCH;
        }

//      Paper Jam or Paper Stall

        if ((b1 == '1' && b2 == '0') ||        // Paper Jam
            (b1 == '0' && (b2 == 'e' || b2 == 'E')))
        {
            return DISPLAY_PAPER_JAMMED;
        }

//      Carriage Stall

        if (b1 == '0' && (b2 == 'F' || b2 == 'f'))
        {
            return DISPLAY_ERROR_TRAP;
        }

//      Job Cancelled (AIO printer turn idle after job canceled)
        if ((b1=='0') && (b2=='5'))     // 05 = CNCL state
        {
            DBG1("Printing Canceled [from Encoded DevID]\n");
            return DISPLAY_PRINTING_CANCELED;
        }

        // VerifyPenInfo will handle prompting the user
        // if this is a problem
        err = VerifyPenInfo ();

        if(err != NO_ERROR)
        {
            // VerifyPenInfo returned an error, which can only happen when ToDevice
            // or GetDeviceID returns an error. Either way, it's BAD_DEVICE_ID or
            // IO_ERROR, both unrecoverable.  This is probably due to the printer
            // being turned off during printing, resulting in us not being able to
            // power it back on in VerifyPenInfo, since the buffer still has a
            // partial raster in it and we can't send the power-on command.
            return DISPLAY_COMM_PROBLEM;
        }
    }

    // check for errors we can detect from the status reg
    if (IOMode.bStatus)
    {
        // Although DJ8XX is OOP, printer continues taking data and BUSY bit gets
        // set.  See IO_defs.h
        if ( DEVICE_IS_OOP(status_reg) )
        {
            DBG1("Out Of Paper [from status byte]\n");
            return DISPLAY_OUT_OF_PAPER;
        }

        // DJ8XX doesn't go offline, so SELECT bit is set even when paper jammed.
        // See IO_defs.h
        if (DEVICE_PAPER_JAMMED(status_reg))
        {
            DBG1("Jammed [from status byte]\n");
            return DISPLAY_PAPER_JAMMED;
        }
/*
 *      Do not process this. Malibu printers set the NFAULT bit low when a pen is
 *      missing, eventhough they support reserve mode printing. This causes us to
 *      report ERROR_TRAP message. Consequence is that we may not catch carriage
 *      stall, but should eventually result in time out.
        if (DEVICE_IO_TRAP(status_reg))
        {
            DBG1("Jammed or trapped [from status byte]\n");
            return DISPLAY_ERROR_TRAP;
        }
 */
    }

    // don't know what the problem is-
    //  Is the PrinterAlive?
    if (pSS->PrinterIsAlive())
    {
        iTotal_SLOW_POLL_Count += iMax_SLOW_POLL_Count;
#if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
        printf("iTotal_SLOW_POLL_Count = %d\n",iTotal_SLOW_POLL_Count);
#endif
        // -Note that iTotal_SLOW_POLL_Count is a multiple of
        //  iMax_SLOW_POLL_Count allowing us to check this
        //  on an absolute time limit - not relative to the number
        //  of times we happen to have entered ParseError.
        // -Also note that we have different thresholds for uni-di & bi-di.
        if(
            ((IOMode.bDevID == FALSE) && (iTotal_SLOW_POLL_Count >= 60)) ||
            ((IOMode.bDevID == TRUE)  && (iTotal_SLOW_POLL_Count >= 120))
          )
        {
            return DISPLAY_BUSY;
        }
        else
        {
            return DISPLAY_PRINTING;
        }
    }
    else
    {
        return DISPLAY_COMM_PROBLEM;
    }
} //ParseError


DRIVER_ERROR DJ9xxVIP::CleanPen()
{
    const BYTE DJ990_User_Output_Page[] = {ESC, '%','P','u','i','f','p','.',
        'm','u','l','t','i','_','b','u','t','t','o','n','_','p','u','s','h',' ','3',';',
        'u','d','w','.','q','u','i','t',';',ESC,'%','-','1','2','3','4','5','X' };

    DWORD length = sizeof(PEN_CLEAN_PML);
    DRIVER_ERROR err = pSS->ToDevice(PEN_CLEAN_PML, &length);
    ERRCHECK;

    // send this page so that the user sees some output.  If you don't send this, the
    // pens get serviced but nothing prints out.
    length = sizeof(DJ990_User_Output_Page);
    return pSS->ToDevice(DJ990_User_Output_Page, &length);
} //CleanPen


#if defined(APDK_VIP_COLORFILTERING)
/// ERNIE ////////////////////////////////////////////////////////////////
BOOL TErnieFilter::Process(RASTERDATA* ImageData)
{
	if (ImageData == NULL || (ImageData->rasterdata[COLORTYPE_COLOR] == NULL && ImageData->rasterdata[COLORTYPE_BLACK] == NULL))
	{
		return FALSE;
	}
	else
	{
		if (ImageData->rasterdata[COLORTYPE_COLOR])
		{
			submitRowToFilter(ImageData->rasterdata[COLORTYPE_COLOR]);

			if (ImageData->rasterdata[COLORTYPE_BLACK])
			{
				memcpy(fBlackRowPtr[RowIndex], ImageData->rasterdata[COLORTYPE_BLACK], ImageData->rastersize[COLORTYPE_BLACK]);
			}
			BlackRasterSize[RowIndex++] = ImageData->rastersize[COLORTYPE_BLACK];

			if (RowIndex == 4)
				RowIndex = 0;

			// something ready after 4th time only
			return (fNumberOfBufferedRows == 0);
		}
		else
		{
			iRastersReady = 1;
		}
	}
	return TRUE;
} //Process


unsigned int TErnieFilter::GetOutputWidth(COLORTYPE  rastercolor)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[COLORTYPE_COLOR] == NULL)
			return 0;
		else
			return fRowWidthInPixels * BYTES_PER_PIXEL;
	}
	else
	{
		if (raster.rasterdata[COLORTYPE_COLOR] == NULL)
			return raster.rastersize[rastercolor];
		else
			return BlackRasterSize[iRastersDelivered-1];
	}
} //GetOutputWidth


BYTE* TErnieFilter::NextOutputRaster(COLORTYPE  rastercolor)
{
	if (iRastersReady == 0)
		return (BYTE*)NULL;
	if (rastercolor == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[COLORTYPE_COLOR] == NULL)
		{
			iRastersReady--;
			return (BYTE*)NULL;
		}
		else
		{
			if (iRastersReady == 0)
				return (BYTE*)NULL;

			iRastersReady--;

			return fRowPtr[iRastersDelivered++];
		}
	}
	else
	{
		if (raster.rasterdata[COLORTYPE_COLOR] == NULL)
		{
			return raster.rasterdata[rastercolor];
		}
		else
		{
			if (BlackRasterSize[iRastersDelivered] == 0)
				return NULL;
			else
				return fBlackRowPtr[iRastersDelivered];
		}
	}
} //NextOutputRaster


void TErnieFilter::Flush()
{
    writeBufferedRows();
    iRastersDelivered=0;
    fPixelOffsetIndex = 0;
    iRastersReady = fNumberOfBufferedRows;
    fNumberOfBufferedRows = 0;
} //Flush

#endif //APDK_VIP_COLORFILTERING

inline uint32_t Mode10::get3Pixel
(
    BYTE* pixAddress,
    int pixelOffset
)
{
    pixAddress += ((pixelOffset << 1) + pixelOffset);     //pixAddress += pixelOffset * 3;

    BYTE r = *(pixAddress);
    BYTE g = *(pixAddress + 1);
    BYTE b = *(pixAddress + 2);

    return (kWhite & ((r << 16) | (g << 8) | (b)));

//  unsigned int toReturn = *((unsigned int*)pixAddress); // load toReturn with XRGB
//  toReturn &= kWhite; // Strip off unwanted X. EGW stripped lsb blue.
} //get3Pixel


void Mode10::put3Pixel
(
    BYTE* pixAddress,
    int pixelOffset,
    uint32_t pixel
)
{
    pixAddress += ((pixelOffset << 1) + pixelOffset);     //pixAddress += pixelOffset * 3;

    unsigned int temp = (pixel & kWhite);

    *(pixAddress) = ((temp >> 16) & 0x000000FF);
    *(pixAddress + 1) = ((temp >> 8) & 0x000000FF);
    *(pixAddress + 2) = (temp & 0x000000FF);

} //put3Pixel


unsigned short Mode10::ShortDelta
(
    uint32_t lastPixel,
    uint32_t lastUpperPixel
)
{
    int dr,dg,db;
    int result;

    dr = GetRed(lastPixel) - GetRed(lastUpperPixel);
    dg = GetGreen(lastPixel) - GetGreen(lastUpperPixel);
    db = GetBlue(lastPixel) - GetBlue(lastUpperPixel);

    if ((dr <= 15) && (dr >= -16) && (dg <= 15) && (dg >= -16) && (db <= 30) && (db >= -32))
    {   // Note db is divided by 2 to double it's range from -16..15 to -32..30
        result = ((dr << 10) & 0x007C00) | (((dg << 5) & 0x0003E0) | ((db >> 1) & 0x01F) | 0x8000);   // set upper bit to signify short delta
    }
    else
    {
        result = 0;  // upper bit is zero to signify delta won't work
    }

    return result;
}

BOOL Mode10::Process
(
    RASTERDATA* input
)
/****************************************************************************
Initially written by Elden Wood
August 1998

Similar to mode 9, though tailored for pixel data.
For more information see the Bert Compression Format document.

This function compresses a single row per call.
****************************************************************************/
{
    if (input==NULL || 
		(input->rasterdata[COLORTYPE_COLOR]==NULL && input->rasterdata[COLORTYPE_BLACK]==NULL))    // flushing pipeline
    {
        Flush();
        return FALSE;
    }

	if (myplane == COLORTYPE_BLACK || input->rasterdata[COLORTYPE_COLOR]==NULL)
	{
		iRastersReady = 1;
		compressedsize = 0;
		if (seeded)
		{
			thePrinter->Send(ResetSeedrow, sizeof(ResetSeedrow));
            memset(SeedRow,0xFF,inputsize);
			seeded = FALSE;
		}
		return TRUE;
	}
    unsigned int originalsize = input->rastersize[myplane];
	unsigned int size = input->rastersize[myplane];

    unsigned char *seedRowPtr = (unsigned char*)SeedRow;

    unsigned char *compressedDataPtr = compressBuf;
    unsigned char *curRowPtr =  (unsigned char*)input->rasterdata[myplane];
    unsigned int rowWidthInBytes = size;

    ASSERT(curRowPtr);
    ASSERT(seedRowPtr);
    ASSERT(compressedDataPtr);
    ASSERT(rowWidthInBytes >= BYTES_PER_PIXEL);
    ASSERT((rowWidthInBytes % BYTES_PER_PIXEL) == 0);

    unsigned char *compressedDataStart = compressedDataPtr;
    unsigned int lastPixel = (rowWidthInBytes / BYTES_PER_PIXEL) - 1;

    // Setup sentinal value to replace last pixel of curRow. Simplifies future end condition checking.
    uint32_t realLastPixel = getPixel(curRowPtr, lastPixel);

    uint32_t newLastPixel = realLastPixel;
    while ((getPixel(curRowPtr, lastPixel-1) == newLastPixel) ||
            (getPixel(seedRowPtr, lastPixel) == newLastPixel))
    {
        putPixel(curRowPtr, lastPixel, newLastPixel += 0x100); // add one to green.
    }

    unsigned int curPixel = 0;
    unsigned int seedRowPixelCopyCount;
    unsigned int cachedColor = kWhite;

    do // all pixels in row
    {
        unsigned char CMDByte = 0;
        int replacementCount;

        // Find seedRowPixelCopyCount for upcoming copy
        seedRowPixelCopyCount = curPixel;
        while (getPixel(seedRowPtr, curPixel) == getPixel(curRowPtr, curPixel))
        {
            curPixel++;
        }

        seedRowPixelCopyCount = curPixel - seedRowPixelCopyCount;
        ASSERT (curPixel <= lastPixel);

        int pixelSource = 0;

        if (curPixel == lastPixel) // On last pixel of row. RLE could also leave us on the last pixel of the row from the previous iteration.
        {
            putPixel(curRowPtr, lastPixel, realLastPixel);

            if (getPixel(seedRowPtr, curPixel) == realLastPixel)
            {
                goto mode10rtn;
            }
            else // code last pix as a literal
            {

                CMDByte = eLiteral;
                pixelSource = eeNewPixel;
                replacementCount = 1;
                curPixel++;
            }
        }
        else // prior to last pixel of row
        {
            ASSERT(curPixel < lastPixel);

            replacementCount = curPixel;
            uint32_t RLERun = getPixel(curRowPtr, curPixel);

            curPixel++; // Adjust for next pixel.
            while (RLERun == getPixel(curRowPtr, curPixel)) // RLE
            {
                curPixel++;
            }
            curPixel--; // snap back to current.
            replacementCount = curPixel - replacementCount;
            ASSERT(replacementCount >= 0);

            if (replacementCount > 0) // Adjust for total occurance and move to next pixel to do.
            {
                curPixel++;
                replacementCount++;

                if (cachedColor == RLERun)
                {
                    pixelSource = eeCachedColor;
                }
                else if (getPixel(seedRowPtr, curPixel-replacementCount+1) == RLERun)
                {
                    pixelSource = eeNEPixel;
                }
                else if ((curPixel-replacementCount > 0) &&  (getPixel(curRowPtr, curPixel-replacementCount-1) == RLERun))
                {
                    pixelSource = eeWPixel;
                }
                else
                {
                    pixelSource = eeNewPixel;
                    cachedColor = RLERun;
                }

                CMDByte = eRLE; // Set default for later.

            }

            if (curPixel == lastPixel)
            {
                ASSERT(replacementCount > 0); // Already found some RLE pixels

                if (realLastPixel == RLERun) // Add to current RLE. Otherwise it'll be part of the literal from the seedrow section above on the next iteration.
                {
                    putPixel(curRowPtr, lastPixel, realLastPixel);
                    replacementCount++;
                    curPixel++;
                }
            }

            if (0 == replacementCount) // no RLE so it's a literal by default.
            {
                uint32_t tempPixel = getPixel(curRowPtr, curPixel);

                ASSERT(tempPixel != getPixel(curRowPtr, curPixel+1)); // not RLE
                ASSERT(tempPixel != getPixel(seedRowPtr, curPixel)); // not seedrow copy

                CMDByte = eLiteral;

                if (cachedColor == tempPixel)
                {
                    pixelSource = eeCachedColor;

                }
                else if (getPixel(seedRowPtr, curPixel+1) == tempPixel)
                {
                    pixelSource = eeNEPixel;

                }
                else if ((curPixel > 0) &&  (getPixel(curRowPtr, curPixel-1) == tempPixel))
                {
                    pixelSource = eeWPixel;

                }
                else
                {

                    pixelSource = eeNewPixel;
                    cachedColor = tempPixel;
                }

                replacementCount = curPixel;
                uint32_t cachePixel;
                uint32_t nextPixel = getPixel(curRowPtr, curPixel+1);
                do
                {
                    if (++curPixel == lastPixel)
                    {
                        putPixel(curRowPtr, lastPixel, realLastPixel);
                        curPixel++;
                        break;
                    }
                    cachePixel = nextPixel;
                }
                while ((cachePixel != (nextPixel = getPixel(curRowPtr, curPixel+1))) &&
                        (cachePixel != getPixel(seedRowPtr, curPixel)));

                replacementCount = curPixel - replacementCount;

                ASSERT(replacementCount > 0);
            }
        }

        ASSERT(seedRowPixelCopyCount >= 0);

        // Write out compressed data next.
        if (eLiteral == CMDByte)
        {
            ASSERT(replacementCount >= 1);

            replacementCount -= 1; // normalize it

            CMDByte |= pixelSource; // Could put this directly into CMDByte above.
            CMDByte |= MIN(3, seedRowPixelCopyCount) << 3;
            CMDByte |= MIN(7, replacementCount);

            *compressedDataPtr++ = CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                outputVLIBytesConsecutively(seedRowPixelCopyCount - 3, compressedDataPtr);
            }

            replacementCount += 1; // denormalize it

            int totalReplacementCount = replacementCount;
            int upwardPixelCount = 1;

            if (eeNewPixel != pixelSource)
            {
                replacementCount -= 1; // Do not encode 1st pixel of run since it comes from an alternate location.
                upwardPixelCount = 2;
            }

            for ( ; upwardPixelCount <= totalReplacementCount; upwardPixelCount++)
            {
                ASSERT(totalReplacementCount >= upwardPixelCount);

                unsigned short compressedPixel = ShortDelta(    getPixel(curRowPtr, curPixel-replacementCount),
                                                                getPixel(seedRowPtr, curPixel-replacementCount));
                if (compressedPixel)
                {
                    *compressedDataPtr++ = compressedPixel >> 8;
                    *compressedDataPtr++ = (unsigned char)compressedPixel;

                }
                else
                {
                    uint32_t uncompressedPixel = getPixel(curRowPtr, curPixel-replacementCount);

                    uncompressedPixel >>= 1; // Lose the lsb of blue and zero out the msb of the 3 bytes.

                    *compressedDataPtr++ = (BYTE)(uncompressedPixel >> 16);
                    *compressedDataPtr++ = (BYTE)(uncompressedPixel >> 8);
                    *compressedDataPtr++ = (BYTE)(uncompressedPixel);

                }

                if (((upwardPixelCount-8) % 255) == 0)  // See if it's time to spill a single VLI byte.
                {
                    *compressedDataPtr++ = MIN(255, totalReplacementCount - upwardPixelCount);
                }

                replacementCount--;
            }
        }
        else // RLE
        {
            ASSERT(eRLE == CMDByte);
            ASSERT(replacementCount >= 2);

            replacementCount -= 2; // normalize it

            CMDByte |= pixelSource; // Could put this directly into CMDByte above.
            CMDByte |= MIN(3, seedRowPixelCopyCount) << 3;
            CMDByte |= MIN(7, replacementCount);

            *compressedDataPtr++ = CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                outputVLIBytesConsecutively(seedRowPixelCopyCount - 3, compressedDataPtr);
            }

            replacementCount += 2; // denormalize it

            if (eeNewPixel == pixelSource)
            {
                unsigned short compressedPixel = ShortDelta(getPixel(curRowPtr, curPixel - replacementCount),
                                                            getPixel(seedRowPtr, curPixel - replacementCount));
                if (compressedPixel)
                {
                    *compressedDataPtr++ = compressedPixel >> 8;
                    *compressedDataPtr++ = (unsigned char)compressedPixel;
                }
                else
                {
                    uint32_t uncompressedPixel = getPixel(curRowPtr, curPixel - replacementCount);

                    uncompressedPixel >>= 1;

                    *compressedDataPtr++ = (BYTE)(uncompressedPixel >> 16);
                    *compressedDataPtr++ = (BYTE)(uncompressedPixel >> 8);
                    *compressedDataPtr++ = (BYTE)(uncompressedPixel);
                }
            }

            if (replacementCount-2 >= 7) outputVLIBytesConsecutively(replacementCount - (7+2), compressedDataPtr);
        }
    } while (curPixel <= lastPixel);

mode10rtn:
    size = compressedDataPtr - compressedDataStart; // return # of compressed bytes.
    compressedsize = size;
    memcpy(SeedRow, input->rasterdata[myplane], originalsize);
    seeded = TRUE;
    iRastersReady = 1;
    return TRUE;
} //Process

BYTE* Mode10::NextOutputRaster(COLORTYPE color)
// since we return 1-for-1, just return result first call
{
	if (iRastersReady==0)
		return (BYTE*)NULL;

	if (color == COLORTYPE_BLACK)
	{
		return raster.rasterdata[color];
	}
	else
	{
		iRastersReady=0;
		if (raster.rasterdata[color] != NULL)
		{
			return compressBuf;
		}
		else
		{
			return raster.rasterdata[color];
		}
	}
}

BYTE DJ9xxVIP::PhotoTrayStatus
(
    BOOL bQueryPrinter
)
{
    DRIVER_ERROR err;
    char* pStr;

    BYTE bDevIDBuff[DevIDBuffSize];

    if (!IOMode.bDevID)
    {
        bQueryPrinter = FALSE;
    }

    err = pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, bQueryPrinter);
    if (err!=NO_ERROR)
    {
        return 0;
    }

    if ( (pStr=(char *)strstr((const char*)bDevIDBuff+2,";S:")) == NULL )
    {
        return 0;
    }

    // skip over ";S:<version=2bytes><topcover><inklid><duplexer>"
    pStr += 8;
    BYTE b = *pStr;
    return b;
} //PhotoTrayStatus


BOOL DJ9xxVIP::PhotoTrayPresent
(
    BOOL bQueryPrinter
)
{
    // for phototray present == 8
    return ((PhotoTrayStatus(bQueryPrinter) & 8) == 8);
} //PhotoTrayInstalled


PHOTOTRAY_STATE DJ9xxVIP::PhotoTrayEngaged
(
    BOOL bQueryPrinter
)
{

/*
 *  When you add any new printer models to this class, make sure that the printer
 *  has the phototray sensor and reports the phototray state correctly.
 */
    // for phototray present and engaged == 9
    return ((PHOTOTRAY_STATE) ((PhotoTrayStatus(bQueryPrinter) & 9) == 9));
} //PhotoTrayEngaged


PAPER_SIZE DJ9xxVIP::MandatoryPaperSize()
{
    if (PhotoTrayEngaged (TRUE))
        return PHOTO_SIZE;
    else return UNSUPPORTED_SIZE;   // code for "nothing mandatory"
} //MandatoryPaperSize


DRIVER_ERROR DJ9xxVIP::CheckInkLevel()
{
    DRIVER_ERROR    err;
    char            *pStr;
    int             i;

//    BYTE bDevIDBuff[DevIDBuffSize];

    if (!IOMode.bDevID)
    {
        return NO_ERROR;
    }

#if 0
/*
 *  Check for unknown device id version.
 */

    if ((pSS->GetVIPVersion ()) > 5)
    {
        return NO_ERROR;
    }

    err = pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, TRUE);
    if (err!=NO_ERROR)
    {
        return err;
    }

    if ( (pStr=(char *)strstr((const char*)bDevIDBuff+2,";S:")) == NULL )
    {
        return NO_ERROR;
    }
#endif

    // Skip to pen-count field

    err = SetPenInfo (pStr, FALSE);
    if (err != NO_ERROR)
    {
        return NO_ERROR;
    }

	/*
	 *  VIPVersion = DevID Version + 1 - DevID Version no is 2 bytes following ;S:
	 *  Version 00 and 01 report 12 bytes for status info
	 *  Version 02 and onwards, report two additional bytes before pen info
	 */

    if (pSS->GetVIPVersion () == 1)
    {

//      DJ990 style DeviceID

        // Next 3 should be 2C1
        // meaning 2 pens, first field is head/supply for K

//        pStr += 19;
        BYTE b1,b2,b3;
        b1=*pStr++; b2=*pStr++; b3=*pStr++;
        if ( (b1 != '2') || (b2 != 'C') || (b3 != '1'))
        {
            return NO_ERROR;
        }

        // black pen
        pStr++;     // skip pen identifier
        BYTE blackink = *pStr - 48;     // convert from ascii
        // only look at low 3 bits
        blackink = blackink & 7;

        // skip into color field
        pStr += 8;
        BYTE colorink = *pStr - 48;
        // only look at low 3 bits
        colorink = colorink & 7;

        if ((blackink<5) && (colorink<5))
        {
            return NO_ERROR;
        }

        if ((blackink>4) && (colorink>4))
        {
            return WARN_LOW_INK_BOTH_PENS;
        }
        if (blackink>4)
        {
            return WARN_LOW_INK_BLACK;
        }
        else
        {
            return WARN_LOW_INK_COLOR;
        }
    }

#if 0
    pStr += 21;
    if (pSS->GetVIPVersion () > 4)
    {
        pStr += 4;
    }
#endif

    int     numPens = 0;
    if (*pStr > '0' && *pStr < '9')
    {
        numPens = *pStr - '0';
    }
    else if (*pStr > 'A' && *pStr < 'F')
    {
        numPens = *pStr - 'A';
    }
    else if (*pStr > 'a' && *pStr < 'f')
    {
        numPens = *pStr - 'a';
    }

    pStr++;

    err = NO_ERROR;
    if (pSS->GetVIPVersion () == 3)
    {
        for (i = 0; i < numPens; i++, pStr += 4)
        {
            switch (*pStr)
            {
                case '5':
                {
                    if ((*(pStr+1) & 0xf3) > 1)
                    {
                        if (err != NO_ERROR)
                        {
                            err = WARN_LOW_INK_MULTIPLE_PENS;
                        }
                        else
                        {
                            err = WARN_LOW_INK_BLACK;
                        }
                    }
                    break;
                }
                case '6':
                {
                    if ((*(pStr+1) & 0xf3) > 1)
                    {
                        if (err != NO_ERROR)
                        {
                            err = WARN_LOW_INK_MULTIPLE_PENS;
                        }
                        else
                        {
                            err = WARN_LOW_INK_CYAN;
                        }
                    }
                    break;
                }
                case '7':
                {
                    if ((*(pStr+1) & 0xf3) > 1)
                    {
                        if (err != NO_ERROR)
                        {
                            err = WARN_LOW_INK_MULTIPLE_PENS;
                        }
                        else
                        {
                            err = WARN_LOW_INK_MAGENTA;
                        }
                    }

                    break;
                }
                case '8':
                {
                    if ((*(pStr+1) & 0xf3) > 1)
                    {
                        if (err != NO_ERROR)
                        {
                            err = WARN_LOW_INK_MULTIPLE_PENS;
                        }
                        else
                        {
                            err = WARN_LOW_INK_YELLOW;
                        }
                    }
                    break;
                }
            }
        }
        return err;
    }

	BYTE    penInfoBits[4];
    BYTE    blackink = 0;
    BYTE    colorink = 0;
    BYTE    photoink = 0;
    BYTE    greyink = 0;

    for (i = 0; i < numPens; i++, pStr += 8)
    {
        AsciiHexToBinary (penInfoBits, pStr, 8);

        if ((penInfoBits[0] & 0x40) != 0x40)        // if Bit 31 is 1, this is not a pen
        {
            continue;
        }
        int penColor = penInfoBits[0] & 0x3F;
        switch (penColor)
        {
            case 1:
                blackink =  penInfoBits[1] & 0x7;
                break;
            case 2:
                colorink =  penInfoBits[1] & 0x7;
                break;
            case 3:
                photoink =  penInfoBits[1] & 0x7;
                break;
            case 10:
                greyink =  penInfoBits[1] & 0x7;
                break;
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                colorink = penInfoBits[1] & 0x7;   // REVISIT: these are C, M, Y respectively
                break;
            default:
                break;
        }
    }
    if (blackink < 2 && colorink < 2 && photoink < 2 && greyink < 2)
    {
        return NO_ERROR;
    }
    else if (blackink > 1 && colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_BLACK_PHOTO;
    }
    else if (greyink > 1 && colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_GREY_PHOTO;
    }
    else if (blackink > 1 && colorink > 1)
    {
        return WARN_LOW_INK_BOTH_PENS;
    }
    else if (blackink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_BLACK_PHOTO;
    }
    else if (greyink > 1 && colorink > 1)
    {
        return WARN_LOW_INK_COLOR_GREY;
    }
    else if (greyink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_GREY_PHOTO;
    }
    else if (colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_PHOTO;
    }
    else if (blackink > 1)
    {
        return WARN_LOW_INK_BLACK;
    }
    else if (colorink > 1)
    {
        return WARN_LOW_INK_COLOR;
    }
    else if (photoink > 1)
    {
        return WARN_LOW_INK_PHOTO;
    }
    else if (greyink > 1)
    {
        return WARN_LOW_INK_GREY;
	}
    else if (colorink > 1)
    {
        return WARN_LOW_INK_COLOR;
    }
	else
	{
		return NO_ERROR;
	}
} //CheckInkLevel

APDK_END_NAMESPACE

#endif  //APDK_DJ9xxVIP

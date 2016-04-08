/*****************************************************************************\
  djgenericvip.cpp : Implimentation for the generic VIP class

  Copyright (c) 2001-2015, HP Co.
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


#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)

#include "header.h"
#include "dj9xxvip.h"
#include "djgenericvip.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];

/*
 *  All VIP printers that are released after the APDK release.
 *  This subclass is mainly there to allow any combination of
 *  installed pens.
 */

DJGenericVIP::DJGenericVIP (SystemServices* pSS, BOOL proto)
    : DJ9xxVIP (pSS, proto)
{

    if (!proto && IOMode.bDevID)
    {
        bCheckForCancelButton = TRUE;
        constructor_error = VerifyPenInfo ();
        if (constructor_error != NO_ERROR)
        {
            constructor_error = NO_ERROR;
            ePen = BOTH_PENS;
        }
    }
    else
        ePen = BOTH_PENS;

/*
 *  For this printer class, allow any print mode to be compatible with
 *  any installed pen set. Printer is expected to do the right thing.
 *  Also, language has to be PCL3GUI because some printers may not work
 *  properly in plain PCL3 mode. ex. PhotoSmart 1xx family. This means
 *  no device text support.
 */

    pMode[ModeCount++] = new VIPFastDraftMode ();        // Fast Draft
    pMode[ModeCount++] = new VIPGrayFastDraftMode ();    // Grayscale Fast Draft
    pMode[ModeCount++] = new VIPAutoPQMode ();           // Printer selects PrintMode
    pMode[ModeCount++] = new VIPFastPhotoMode ();        // Fast Photo
    pMode[ModeCount++] = new VIPCDDVDMode ();            // CD/DVD PrintMode

#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[ModeCount++] = new VIPBrochureNormalMode ();   // Normal, Brochure media
    pMode[ModeCount++] = new VIPPremiumNormalMode ();    // Normal, Premium paper
    pMode[ModeCount++] = new VIPPlainBestMode ();        // Best, Plain media
    pMode[ModeCount++] = new VIPBrochureBestMode ();     // Best, Brochure media
    pMode[ModeCount++] = new VIPPremiumBestMode ();      // Best, Premium media     
#endif

    for (int i = 0; i < (int) ModeCount; i++)
    {
        pMode[i]->bFontCapable = FALSE;
        for (int j = BLACK_PEN; j < MAX_COMPATIBLE_PENS; j++)
        {
            pMode[i]->CompatiblePens[j] = (PEN_TYPE) j;
        }
    }
#ifdef APDK_EXTENDED_MEDIASIZE
/*
 *  Set quality to marvellous for high res (1200 dpi) mode.
 *  This mode was not available in the older printers, which only supported the
 *  media_high_res paper type, but used qualityPresentation for 1200 dpi mode.
 */
    pMode[4]->theQuality = qualityMarvellous;
#endif
}

VIPFastDraftMode::VIPFastDraftMode () : PrintMode (NULL)
{
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

   Config.bColorImage = FALSE;

    medium = mediaPlain;
    theQuality = qualityFastDraft;
    pmQuality = QUALITY_FASTDRAFT;
    pmMediaType   = MEDIA_PLAIN;
} // VIPFastDraftMode

VIPGrayFastDraftMode::VIPGrayFastDraftMode () : GrayMode (ulMapDJ600_CCM_K)
{
    bFontCapable = FALSE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif

    dyeCount    = 1;
    medium      = mediaPlain;
    theQuality  = qualityFastDraft;
    pmQuality   = QUALITY_FASTDRAFT;
    pmMediaType = MEDIA_PLAIN;
    pmColor     = GREY_K;
}

VIPFastPhotoMode::VIPFastPhotoMode () : PrintMode (NULL)
{
    bFontCapable = FALSE;
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE;
#endif
    medium = mediaHighresPhoto;
    theQuality = qualityFastDraft;
    pmQuality = QUALITY_FASTDRAFT;
    pmMediaType = MEDIA_PHOTO;
} // VIPFastPhotoMode

VIPAutoPQMode::VIPAutoPQMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium     = mediaAuto;
    theQuality = qualityAuto;
    pmQuality  = QUALITY_AUTO;
} // VIPAutoPQMode

VIPCDDVDMode::VIPCDDVDMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaCDDVD;
    theQuality  = qualityPresentation;
    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_CDDVD;
}

VIPBrochureNormalMode::VIPBrochureNormalMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaBrochure;
    theQuality  = qualityNormal;
    pmQuality   = QUALITY_NORMAL;
    pmMediaType = MEDIA_BROCHURE;
} // Added by Lizzie

VIPPremiumNormalMode::VIPPremiumNormalMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaSpecial;
    theQuality  = qualityNormal;
    pmQuality   = QUALITY_NORMAL;
    pmMediaType = MEDIA_PREMIUM;
} // Added by Lizzie

VIPPlainBestMode::VIPPlainBestMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaPlain;
    theQuality  = qualityPresentation;
    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PLAIN;

} // Added by Lizzie

VIPBrochureBestMode::VIPBrochureBestMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaBrochure;
    theQuality  = qualityPresentation;
    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_BROCHURE;
} // Added by Lizzie

VIPPremiumBestMode::VIPPremiumBestMode () : PrintMode (NULL)
{
    BaseResX =
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    bFontCapable = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium      = mediaSpecial;
    theQuality  = qualityPresentation;
    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PREMIUM;
} // Added by Lizzie

BOOL DJGenericVIP::UseGUIMode (PrintMode* pPrintMode)
{
    return TRUE;
}

/*
 *  We don't really know beforehand the largest papersize the attached printer
 *  can support. The first reserved nibble after the VIP flag nibble contains
 *  this information. Only firmware version 4 or greater support this feature.
 */

PAPER_SIZE DJGenericVIP::MandatoryPaperSize ()
{
    BYTE    sDevIdStr[DevIDBuffSize];
    char    *pStr;

/*
 *  Try to get the DevID. Advance the pointer to the beginning of the status field.
 *  Currently, only 3 lower bits are used for reporting paper size. Meaning of these
 *  bit-values is as follows:
 *      000 - Allow any size
 *      001 - A6
 *      010 - Letter
 *      011 - B4
 *      100 - 13 X 19 size
 */

    if ((pSS->GetDeviceID (sDevIdStr, DevIDBuffSize, FALSE)) == NO_ERROR)
    {
        if ((pStr = strstr ((char *) sDevIdStr, ";S:")) && (pSS->GetVIPVersion ()) >= 3)
        {
#ifdef APDK_EXTENDED_MEDIASIZE
			PAPER_SIZE  PaperSizes[8] = {UNSUPPORTED_SIZE, A6, LETTER, B4, SUPERB_SIZE, UNSUPPORTED_SIZE, UNSUPPORTED_SIZE, UNSUPPORTED_SIZE};
#else
            PAPER_SIZE  PaperSizes[8] = {UNSUPPORTED_SIZE, A6, LETTER, B4, UNSUPPORTED_SIZE, UNSUPPORTED_SIZE, UNSUPPORTED_SIZE, UNSUPPORTED_SIZE};
#endif
			char  byte12 = pStr[17];
			short value = (byte12 >= '0' && byte12 <= '9') ? byte12 - '0' : 
					  ((byte12 >= 'A' && byte12 <= 'F') ? byte12 - 'A' + 10 :
					  ((byte12 >= 'a' && byte12 <= 'f') ? byte12 - 'a' + 10 : -1));
			return (value == -1) ? UNSUPPORTED_SIZE : PaperSizes[(value & 0x07)];
		}
    }
    return UNSUPPORTED_SIZE;
} //MandantoryPaperSize


/*
* byte 13 - indicates full bleed (to every edge of paper) is supported if 
* the modifier bit in the lowest position is "set".  
*

* 0000b or 0h = Unimplemented, or not specified.
* 0001b or 1h = Full bleed (4 edge) printing supported on photo quality papers; this field is a modifier upon the bits in byte 12. That is, if this bit is set, it implies that the "max print width supported" values and smaller widths, ALL support full bleed, 4 edge, printing on photo quality paper.
* 0010b or 2h = Full bleed (4 edge) printing supported on non-photo papers (e.g. plain, bond, etc.); this field is a modifier upon the bits in byte 12. If this bit is set, it implies that the "max print width supported" values and smaller widths, all support full bleed, 4 edge, printing on non-photo paper.
* nn00b  = The "nn" bits are reserved for future definitions
*/

BOOL DJGenericVIP::FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                     float *fLeftOverSpray, float *fTopOverSpray)
{
    BYTE    sDevIdStr[DevIDBuffSize];
    char    *pStr;
    sDevIdStr[0] = 0;

	// if overspray is passed down and set
	if (this->m_iTopOverspray && this->m_iLeftOverspray && this->m_iRightOverspray && this->m_iBottomOverspray)
    {

/*
 *      This check is needed because of method override. 
 *      Another caller would not pass down this parameter, 
 *      and this is default to NULL
 */
		if (fTopOverSpray) 
			*fTopOverSpray = (float) this->m_iTopOverspray / (float) 1000.0;
		if (fLeftOverSpray)
			*fLeftOverSpray = (float) this->m_iLeftOverspray / (float) 1000.0;
		
		*xOverSpray = (float) (this->m_iLeftOverspray + this->m_iRightOverspray) / (float) 1000.0;
		*yOverSpray = (float) (this->m_iTopOverspray + this->m_iBottomOverspray) / (float) 1000.0;

		*fbType = fullbleed4EdgeAllMedia;

		return TRUE;
	}

    if ((pSS->GetDeviceID (sDevIdStr, DevIDBuffSize, FALSE)) == NO_ERROR)
    {
        if ((pStr = strstr ((char *) sDevIdStr, ";S:")) && (pSS->GetVIPVersion ()) >= 3)
        {
			char  byte13 = pStr[18];
			short value = (byte13 >= '0' && byte13 <= '9') ? byte13 - '0' : 
					  ((byte13 >= 'A' && byte13 <= 'F') ? byte13 - 'A' + 10 :
					  ((byte13 >= 'a' && byte13 <= 'f') ? byte13 - 'a' + 10 : -1));
			switch (ps)
			{
				case PHOTO_SIZE:
				case A6:
				case CARD_4x6:
				case OUFUKU:
				case HAGAKI:
				case A6_WITH_TEAR_OFF_TAB:
				case CUSTOM_SIZE:
				{
					*xOverSpray = (float) 0.12;
					*yOverSpray = (float) 0.06;

					if (fLeftOverSpray)
						*fLeftOverSpray = (float) 0.05;
					if (fTopOverSpray)
						*fTopOverSpray  = (float) 0.03;

					if (ps == PHOTO_SIZE || ps == A6_WITH_TEAR_OFF_TAB)
						*fbType = fullbleed4EdgeAllMedia;
					else if ((value != -1) && ((value & 0x03) == 0x03))
						*fbType = fullbleed4EdgeAllMedia;
					else if ((value != -1) && ((value & 0x01) == 0x01))
						*fbType = fullbleed4EdgePhotoMedia;
					else if ((value != -1) && ((value & 0x02) == 0x02))
						*fbType = fullbleed4EdgeNonPhotoMedia;
					else
						*fbType = fullbleed3EdgeAllMedia;

					return TRUE;
				}
				default:
					break;
			}

			if ((value != -1) && (value & 0x03))
			{
				*xOverSpray = (float) 0.216;
				*yOverSpray = (float) 0.149;
				if (fLeftOverSpray)
					*fLeftOverSpray = (float) 0.098;
				if (fTopOverSpray)
					*fTopOverSpray  = (float) 0.051;

				if ((value & 0x03) == 0x03)
					*fbType = fullbleed4EdgeAllMedia;
				else if ((value & 0x01) == 0x01)
					*fbType = fullbleed4EdgePhotoMedia;
				else if ((value & 0x02) == 0x02)
					*fbType = fullbleed4EdgeNonPhotoMedia;

				return TRUE; 

			}
			else
			{
				*xOverSpray = (float) 0;
				*yOverSpray = (float) 0;
				if (fLeftOverSpray)
					*fLeftOverSpray = (float) 0;
				if (fTopOverSpray)
					*fTopOverSpray  = (float) 0;
				*fbType = fullbleedNotSupported;
				return FALSE;
			}
        }
    }

	switch (ps)
	{
		case PHOTO_SIZE:
		case A6:
		case CARD_4x6:
		case OUFUKU:
		case HAGAKI:
		case A6_WITH_TEAR_OFF_TAB:
		case LETTER:
		case A4:
		case CUSTOM_SIZE:
		{
			*xOverSpray = (float) 0.12;
			*yOverSpray = (float) 0.06;

			if (fLeftOverSpray)
				*fLeftOverSpray = (float) 0.05;
			if (fTopOverSpray)
				*fTopOverSpray  = (float) 0.03;

			if (ps == PHOTO_SIZE || ps == A6_WITH_TEAR_OFF_TAB)
				*fbType = fullbleed4EdgeAllMedia;
			else if (ps==LETTER||ps==A4||ps==CUSTOM_SIZE)
			{
				*fbType = fullbleed4EdgeAllMedia;								
				*xOverSpray = (float) 0.1;
				*yOverSpray = (float) 0.1;
				if (fLeftOverSpray)
					*fLeftOverSpray = (float) 0.05;
				if (fTopOverSpray)
					*fTopOverSpray  = (float) 0.05;
			}
			else
				*fbType = fullbleed3EdgeAllMedia;

			return TRUE;
		}
		default:
			break;
	}

    *xOverSpray = (float) 0;
    *yOverSpray = (float) 0;
    if (fLeftOverSpray)
        *fLeftOverSpray = (float) 0;
    if (fTopOverSpray)
        *fTopOverSpray  = (float) 0;
	*fbType = fullbleedNotSupported;
    return FALSE;
}

BOOL DJGenericVIP::GetMargins (PAPER_SIZE ps, float *fMargins)
{
    float           xo, yo, xl, yt;
    FullbleedType   fbType = fullbleedNotSupported;

    if (ps == CDDVD_120 || ps == CDDVD_80)
    {
        fMargins[0] = (float) 0.06;
        fMargins[1] = (float) 0.06;
        fMargins[2] = (float) 0.06;
        fMargins[3] = (float) 0.06;
        return TRUE;
    }

    fMargins[0] = (float) 0.125;
    fMargins[1] = (float) 0.125;
    fMargins[2] = (float) 0.125;
    fMargins[3] = (float) 0.5;
    if ((FullBleedCapable (ps, &fbType, &xo, &yo, &xl, &yt)) &&
        (fbType == fullbleed4EdgeAllMedia ||
         fbType == fullbleed4EdgePhotoMedia ||
         fbType == fullbleed4EdgeNonPhotoMedia))
    {
        fMargins[3] = (float) 0.125;
    }

    if (ps == SUPERB_SIZE)
    {
        fMargins[0] = (float) 0.125;
        fMargins[1] = (float) 0.5;
        fMargins[2] = (float) 0.125;
        fMargins[3] = (float) 0.75;
    }

    return TRUE;
}


PHOTOTRAY_STATE DJGenericVIP::PhotoTrayEngaged (BOOL bQueryPrinter)
{
    return UNKNOWN;
}

//! Returns TRUE if a hagaki feed is present in printer.
BOOL DJGenericVIP::HagakiFeedPresent(BOOL bQueryPrinter) 
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
    return ((b & 4) == 4);
}

#ifdef APDK_AUTODUPLEX
//!Returns TRUE if duplexer and hagaki feed (combined) unit is present in printer.
BOOL DJGenericVIP::HagakiFeedDuplexerPresent(BOOL bQueryPrinter)
{
    DRIVER_ERROR err;
    char* pStr;

    BYTE bDevIDBuff[DevIDBuffSize];

    if (!IOMode.bDevID)
    {
        bQueryPrinter = FALSE;
    }

    err = pSS->GetDeviceID (bDevIDBuff, DevIDBuffSize, bQueryPrinter);
    if (err!=NO_ERROR)
    {
        return 0;
    }

    if ( (pStr=(char *)strstr((const char*)bDevIDBuff+2,";S:")) == NULL )
    {
        return 0;
    }

    // skip over ";S:<version=2bytes><topcover><inklid>"
    pStr += 6;
    BYTE b = *pStr;
    return ((b & 4) == 4);
}
#endif


DRIVER_ERROR DJGenericVIP::VerifyPenInfo ()
{

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

/*
 *  If one or more pens are not installed, check the device id for ReserveMode
 *  capability for this printer. This info in in the flags nibble
 *  ;S:<XX><toplid><supplylic><duplexer><phototray><in-1><in-2><banner><flags>
 *  flags : x1xx - reserve mode supported, x0xx - not supported
 */

    BYTE    sDevIdStr[DevIDBuffSize];
    char    *pStr;
    BOOL    bNeedAllPens = FALSE;

    if (iNumMissingPens > 0 && ((pSS->GetDeviceID (sDevIdStr, DevIDBuffSize, FALSE)) == NO_ERROR))
    {
        if ((pStr = strstr ((char *) sDevIdStr, ";S:")))
        {
			char  byte7 = pStr[12];
			short value = (byte7 >= '0' && byte7 <= '9') ? byte7 - '0' : 
                      ((byte7 >= 'A' && byte7 <= 'F') ? byte7 - 'A' + 10 :
                      ((byte7 >= 'a' && byte7 <= 'f') ? byte7 - 'a' + 10 : 0));
			bNeedAllPens = !(value & 0x04);
        }
    }

    while (ePen == NO_PEN || (bNeedAllPens && iNumMissingPens > 0))
    {
        if (ePen == NO_PEN || iNumMissingPens > 0)
        {
            pSS->DisplayPrinterStatus (DISPLAY_NO_PENS);

            if (pSS->BusyWait (500) == JOB_CANCELED)
            {
                return JOB_CANCELED;
            }
        }
        err =  ParsePenInfo (ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;
} //ParasePenInfo

void DJGenericVIP::AdjustModeSettings (BOOL bDoFullBleed, MEDIATYPE ReqMedia,
                                       MediaType *medium, Quality *quality)
{

/*
 *  Malibu platform printers have a defect that prevents these printers from
 *  performing full-bleed and media detect functions correctly. Check for this
 *  case, i.e., full-bleed and media detect flags and explicitly set the
 *  media type and quality as requested by the application.
 */

    if (bDoFullBleed && *medium == mediaAuto)
    {

        if (ReqMedia == MEDIA_PHOTO)
        {
            *medium  = mediaGlossy;
            *quality = qualityPresentation;

        }
        else
        {
            *medium  = mediaPlain;
            *quality = qualityNormal;

        }

    }
    // Added by Lizzie - plain and premium will default to Auto when PQ is not fast draft
    // so fix the media type here
    if( ReqMedia == MEDIA_PLAIN)
        *medium = mediaPlain;

    if( ReqMedia == MEDIA_PREMIUM)
        *medium = mediaSpecial;

    if( ReqMedia == MEDIA_BROCHURE)
        *medium = mediaBrochure;

} // AdjustModeSettings

#ifdef APDK_LINUX
DRIVER_ERROR DJGenericVIP::SendSpeedMechCmd (BOOL bLastPage)
{
    DRIVER_ERROR    err = NO_ERROR;
    BYTE            szStr[16];
    if (m_iNumPages > 1)
    {
        memcpy (szStr, "\x1B*o5W\x0D\x02\x00\x00\x00", 10);
		szStr[7] = (BYTE) ((m_iNumPages & 0x00FF0000) >> 16);
		szStr[8] = (BYTE) ((m_iNumPages & 0x0000FF00) >> 8);
		szStr[9] = (BYTE) (m_iNumPages & 0x000000FF);
        err = Send ((const BYTE *) szStr, 10);
        if (bLastPage)
        {
            err = Send ((const BYTE *) "\x1B*o5W\x0D\x05\x00\x00\x01", 10);
        }
        else
        {
            err = Send ((const BYTE *) "\x1B*o5W\x0D\x05\x00\x00\x00", 10);
        }
    }
	return err;
}
#endif // APDK_LINUX

Header *DJGenericVIP::SelectHeader (PrintContext *pc)
{
    return new HeaderDJGenericVIP (this, pc);
}

HeaderDJGenericVIP::HeaderDJGenericVIP (Printer *p, PrintContext *pc) : HeaderDJ990 (p, pc)
{
    m_uiCAPy = 0;
}

DRIVER_ERROR HeaderDJGenericVIP::SendCAPy (unsigned int iAbsY)
{
    char    str[16];
    DRIVER_ERROR err = NO_ERROR;
    if (m_uiCAPy == 0)
    {
        sprintf (str, "\x1B*p%dY", iAbsY);
        err = thePrinter->Send ((const BYTE *) str, strlen (str));
        m_uiCAPy = iAbsY;
    }
    return err;
}

DRIVER_ERROR HeaderDJGenericVIP::FormFeed ()
{
    BYTE FF = 12;
    m_uiCAPy = 0;
    return thePrinter->Send ((const BYTE *) &FF, 1);
}

APDK_END_NAMESPACE


#endif // defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)

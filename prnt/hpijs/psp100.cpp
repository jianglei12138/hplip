/*****************************************************************************\
  psp100.cpp : Implimentation for the PSP100 class

  Copyright (c) 2015, HP Co.
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

#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)

#include "header.h"
#include "dj9xxvip.h"
#include "psp100.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

// PhotoSmart 100 - AtomAnt
PSP100::PSP100 (SystemServices* pSS, BOOL proto)
    : DJ9xxVIP (pSS, proto)
{

    for (unsigned int i = 0; i < ModeCount; i++)
    {
        if (pMode[i])
            delete pMode[i];
        pMode[i] = NULL;
    }

    ePen = COLOR_PEN;

    if (!proto && IOMode.bDevID)
    {
        bCheckForCancelButton = TRUE;
        constructor_error = VerifyPenInfo ();
        CERRCHECK;
    }

    pMode[GRAYMODE_INDEX]    = new GrayModePSP100 ();
    pMode[DEFAULTMODE_INDEX] = new PSP100Mode ();
	pMode[SPECIALMODE_INDEX] = new PSP100NormalMode ();

    ModeCount = 3;

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
#endif
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX+1] = new PSP1002400Mode ();
    ModeCount = 4;
#endif

} //PSP100


GrayModePSP100::GrayModePSP100 ()
    : PrintMode(NULL)
{
#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie=TRUE;
#endif

    Config.bColorImage=FALSE;

/*
 *  The resolutions here are set to 300 for better performance from Cameras.
 *  REVISIT: Must provide a true 600 dpi printmode later.
 *  12/21/01
 *
 *  See not in dj9xxvip.cpp - 1/9/2002 - JLM
 */

    BaseResX       =
    BaseResY       =
    TextRes        =
    ResolutionX[0] =
    ResolutionY[0] = VIP_BASE_RES;
    medium = mediaGlossy;
    CompatiblePens[0] = COLOR_PEN;
    theQuality = qualityPresentation;
    bFontCapable = FALSE;
    pmColor = GREY_CMY;
} //GrayModePSP100


PEN_TYPE PSP100::DefaultPenSet()
{ return COLOR_PEN; }


PSP100Mode::PSP100Mode () : PrintMode (NULL)
{

/*
 *  See comments above regarding 300/600 dpi change
 */

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium = mediaGlossy;
    theQuality = qualityPresentation;
	pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;
    CompatiblePens[0] = COLOR_PEN;
    bFontCapable = FALSE;
} //PSP100Mode

PSP100NormalMode::PSP100NormalMode () : PrintMode (NULL)
{

/*
 *  See comments above regarding 300/600 dpi change
 */

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = VIP_BASE_RES;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium = mediaSpecial;
    theQuality = qualityNormal;
    CompatiblePens[0] = COLOR_PEN;
    bFontCapable = FALSE;
} //PSP100Mode

#ifdef APDK_EXTENDED_MEDIASIZE
PSP1002400Mode::PSP1002400Mode () : PrintMode (NULL)
{

/*
 *  See comments above regarding 300/600 dpi change
 */

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = 1200;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium = mediaHighresPhoto;
    theQuality = qualityPresentation;
    pmMediaType = MEDIA_PHOTO;
    pmQuality = QUALITY_HIGHRES_PHOTO;
    CompatiblePens[0] = COLOR_PEN;
    bFontCapable = FALSE;
} //PSP1002400Mode
#endif

PAPER_SIZE PSP100::MandatoryPaperSize ()
{
    return A6;
   // return PHOTO_5x7;
} //MandantoryPaperSize


BOOL PSP100::UseGUIMode
(
    PrintMode* pPrintMode
)
{
    return TRUE;
} //UseGUIMode


DRIVER_ERROR PSP100::VerifyPenInfo ()
{
    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
    {
        return err;
    }
    ePen = NO_PEN;
    char    *str;
    while (ePen == NO_PEN)
    {
        err = SetPenInfo (str, TRUE);
        ERRCHECK;

        if (*(str-1) == '1' && *(str-2) == '1')
        {
            pSS->DisplayPrinterStatus (DISPLAY_NO_PENS);
        }
        else
        {
            ePen = COLOR_PEN;
            break;
        }
        if (pSS->BusyWait(500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }
    }

    return NO_ERROR;
} //VerifyPenInfo

APDK_END_NAMESPACE

#endif //APDK_PSP100

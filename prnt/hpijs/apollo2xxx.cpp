/*****************************************************************************\
  apollo2xxx.cpp : Implimentation for the Apollo2xxx class

  Copyright (c) 2000, 2015, HP Co.
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


#ifdef APDK_APOLLO2XXX

#include "header.h"
#include "apollo2xxx.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ690_CMYK[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ690_ClMlxx[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ660_CCM_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_CMY[ 9 * 9 * 9 ];

Apollo2xxx::Apollo2xxx(SystemServices* pSS, BOOL proto)
    : Printer(pSS, NUM_DJ6XX_FONTS,proto)
{

    if ((!proto) && (IOMode.bDevID))
      {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
      }
    else ePen=BOTH_PENS;    // matches default mode

    CMYMap = ulMapDJ600_CCM_CMY;    // used in Mode2xxxColor

    pMode[DEFAULTMODE_INDEX]        = new PrintMode(ulMapDJ660_CCM_KCMY);   // normal color kcmy
    pMode[SPECIALMODE_INDEX]        = new Mode2xxxColor();   // normal color cmy
    pMode[SPECIALMODE_INDEX+1]   = new Mode2xxxPhoto();   // photo
    pMode[GRAYMODE_INDEX]           = new GrayMode2xxx();   // normal gray k
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX+2] = new Mode2xxxDraftGrayK();
    pMode[SPECIALMODE_INDEX+3] = new Mode2xxxDraftColorKCMY();
    pMode[SPECIALMODE_INDEX+4] = new Mode2xxxDraftColorCMY();
    pMode[SPECIALMODE_INDEX+5] = new Mode2xxxBestGrayK();
    ModeCount=8;
#else
    ModeCount = 4;
#endif

    DBG1("AP2xxx created\n");

}

DRIVER_ERROR Apollo2xxx::VerifyPenInfo()
{
    DRIVER_ERROR err=NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    err = ParsePenInfo(ePen);
    ERRCHECK;

    // check for the normal case
    if (ePen == BOTH_PENS || ePen == MDL_BOTH || ePen == COLOR_PEN || ePen == BLACK_PEN)
        return NO_ERROR;

DBG1("Apollo2xxx::VerifyPenInfo(): ePen is not BOTH_PENS, MDL_BOTH, COLOR_PEN, or BLACK_PEN\n");

    // the 2100 printers require the color pen OR the black pen to be installed, so trap
    // on any pen type that does not include either the color pen or black pen
    while ( (ePen != BOTH_PENS) && (ePen != MDL_BOTH) && (ePen != COLOR_PEN) && (ePen != BLACK_PEN))
    {
DBG1("Apollo2xxx::VerifyPenInfo(): in while loop\n");

        switch (ePen)
        {
            case MDL_PEN:
                // photopen installed, need to install color pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_COLOR_PEN);
                break;
            case NO_PEN:
                // neither pen installed
            default:
                pSS->DisplayPrinterStatus(DISPLAY_NO_PENS);
                break;
        }

        if (pSS->BusyWait(500) == JOB_CANCELED)
            return JOB_CANCELED;

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;

}

DRIVER_ERROR Apollo2xxx::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char* str;
    DRIVER_ERROR err = SetPenInfo(str, QueryPrinter);
    ERRCHECK;

    if (*str != '$')
    {
        return BAD_DEVICE_ID;
    }

    str++;    // skip $
    // parse penID
    PEN_TYPE temp_pen1;
    // check pen1, assume it is black or MDL, pen2 is color
    switch (str[0])
    {
        // check for MDL in case someone wedged one in there
        case 'M': temp_pen1 = MDL_PEN; break; // (M)ulti-Dye load pen
        case 'C': temp_pen1 = BLACK_PEN; break; // (C)andide black
        default:  temp_pen1 = NO_PEN; break;
    }

    // now check pen2

    int i = 2;
    while(i < DevIDBuffSize && str[i]!='$') i++; // handles variable length penIDs

    if (i == DevIDBuffSize)
    {
        return BAD_DEVICE_ID;
    }

    i++;

    if(str[i]=='R') // we have the (R)obinhood color pen,
                    // check what pen1 was
    {
        if (temp_pen1 == BLACK_PEN)
                ePen = BOTH_PENS;
        else
        {
            if (temp_pen1 == MDL_PEN)
                ePen = MDL_BOTH;
            else
                ePen = COLOR_PEN;
        }
    }
    else // no color pen, just set what pen1 was
        ePen = temp_pen1;

    return NO_ERROR;
}


Mode2xxxPhoto::Mode2xxxPhoto()
// print mode for photo pen
: PrintMode(ulMapDJ690_CMYK,ulMapDJ690_ClMlxx)
{
   dyeCount=6;
   medium =  mediaSpecial;
   theQuality = qualityNormal;

   pmQuality=QUALITY_BEST;
   pmMediaType=MEDIA_PREMIUM;


   BaseResX = 600;
   for (int i=0; i < 6; i++)
        ResolutionX[i]=600;

   CompatiblePens[0] = MDL_BOTH;

//   strcpy(ModeName, "Photo");

}

Mode2xxxColor::Mode2xxxColor()
: PrintMode(ulMapDJ600_CCM_CMY)
{
   dyeCount=3;
   CompatiblePens[0] = COLOR_PEN;   // only color pen allowed

//   strcpy(ModeName, "Color");
}

GrayMode2xxx::GrayMode2xxx()
// print mode for photo pen
: PrintMode( ulMapDJ600_CCM_K )
{
   CompatiblePens[1] = MDL_BOTH;

   pmColor=GREY_K;
   dyeCount=1;

}

#ifdef APDK_EXTENDED_MEDIASIZE
Mode2xxxDraftColorKCMY::Mode2xxxDraftColorKCMY()
: PrintMode( ulMapDJ660_CCM_KCMY )
{
    theQuality = qualityDraft;
    Config.eHT = MATRIX;
    pmQuality = QUALITY_DRAFT;
}

Mode2xxxDraftGrayK::Mode2xxxDraftGrayK()
: PrintMode(ulMapDJ600_CCM_K)
{
   CompatiblePens[1] = MDL_BOTH;
   pmColor=GREY_K;
   dyeCount=1;
   theQuality = qualityDraft;
   pmQuality = QUALITY_DRAFT;
}

Mode2xxxDraftColorCMY::Mode2xxxDraftColorCMY()
: PrintMode( ulMapDJ600_CCM_CMY )
{
    dyeCount=3;
    CompatiblePens[0] = COLOR_PEN;   // only color pen allowed
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
}

Mode2xxxBestGrayK::Mode2xxxBestGrayK()
: GrayMode(ulMapDJ600_CCM_K)
{
   theQuality = qualityPresentation;
   pmQuality = QUALITY_BEST;
   BaseResX = BaseResY = 600;
   ResolutionX[0] = ResolutionY[0] = 600;
}
#endif // APDK_EXTENDED_MEDIASIZE

Header2xxx::Header2xxx(Printer* p,PrintContext* pc)
    : Header(p,pc)
{  }


Header* Apollo2xxx::SelectHeader(PrintContext* pc)
{
    return new Header2xxx(this,pc);
}

DRIVER_ERROR Header2xxx::Send()
{   DRIVER_ERROR err;

    StartSend();

    // don't disable black extraction if we are in the color pen only mode
    // since the printer may actually have both pens so we want the printer
    // to print the K with the K pen if it actually has one

    if (thePrintMode->dyeCount != 3)
    {
        err = thePrinter->Send((const BYTE*)BlackExtractOff,
                            sizeof(BlackExtractOff));
        ERRCHECK;
    }

    err = ConfigureRasterData();
    ERRCHECK;

    if (ResolutionY[0] == 600)
    {
        char uom[10];
        sprintf(uom,"%c%c%c%d%c",ESC,'&','u',thePrintMode->ResolutionY[K],'D');
        err=thePrinter->Send((const BYTE*)uom, 7 );
        ERRCHECK;
    }

    err=Graphics();     // start raster graphics and set compression mode

return err;
}

APDK_END_NAMESPACE

#endif  //APDK_APOLLO2XXX

/*****************************************************************************\
  dj630.cpp : Implimentation for the DJ630 class

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


#ifdef APDK_DJ630

#include "header.h"
#include "dj630.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ690_CMYK[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ690_ClMlxx[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ660_CCM_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_CMY[ 9 * 9 * 9 ];


DJ630::DJ630(SystemServices* pSS, BOOL proto)
    : Printer(pSS, NUM_DJ6XX_FONTS,proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else
    {
        ePen=COLOR_PEN;    // matches default mode
    }

    CMYMap = ulMapDJ600_CCM_CMY;

    pMode[DEFAULTMODE_INDEX]        = new Mode630Color();                       // normal color CMY
    pMode[SPECIALMODE_INDEX + 0]    = new PrintMode(ulMapDJ660_CCM_KCMY);    // normal color KCMY
    pMode[SPECIALMODE_INDEX + 1]    = new Mode630Photo();                       // photo
    pMode[GRAYMODE_INDEX]           = new GrayMode630(ulMapDJ600_CCM_K);    // normal gray K
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX+2] = new Mode630DraftGrayK();
    pMode[SPECIALMODE_INDEX+3] = new Mode630DraftColorKCMY();
    pMode[SPECIALMODE_INDEX+4] = new Mode630DraftColorCMY();
    pMode[SPECIALMODE_INDEX+5] = new Mode630BestGrayK();
    ModeCount=8;
#else
    ModeCount = 4;
#endif

    DBG1("DJ630 created\n");

}

PEN_TYPE DJ630::DefaultPenSet()
{
    return COLOR_PEN;
}

GrayMode630::GrayMode630(uint32_t *map)
: GrayMode(map)
{
    CompatiblePens[1] = BLACK_PEN;
    CompatiblePens[2] = MDL_BOTH;
}

DRIVER_ERROR DJ630::VerifyPenInfo()
{

    DRIVER_ERROR err=NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    err = ParsePenInfo(ePen);
    ERRCHECK;

    // check for the normal case
    if (ePen == BOTH_PENS || ePen == MDL_BOTH || ePen == COLOR_PEN)
        return NO_ERROR;

DBG1("DJ630::VerifyPenInfo(): ePen is not BOTH_PENS, MDL_BOTH, or COLOR_PEN\n");

    // the 630 printers require the color pen to be installed, so trap
    // on any pen type that does not include the color pen
    while ( (ePen != BOTH_PENS) && (ePen != MDL_BOTH) && (ePen != COLOR_PEN) )
    {
DBG1("DJ630::VerifyPenInfo(): in while loop\n");

        switch (ePen)
        {
            case MDL_PEN:
            case BLACK_PEN:
                // black or photopen installed, need to install color pen
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

DRIVER_ERROR DJ630::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
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

    int i=2;
    while((i < DevIDBuffSize) && str[i]!='$') i++; // handles variable length penIDs
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


Mode630Photo::Mode630Photo()
// print mode for photo pen
: PrintMode(ulMapDJ690_CMYK,ulMapDJ690_ClMlxx)
{
   dyeCount=6;
   medium =  mediaSpecial;
   theQuality = qualityNormal;


   BaseResX = 600;
   for (int i=0; i < 6; i++)
        ResolutionX[i]=600;

   CompatiblePens[0] = MDL_BOTH;

   pmQuality=QUALITY_BEST;
   pmMediaType=MEDIA_PREMIUM;

//   strcpy(ModeName, "Photo");

}

Mode630Color::Mode630Color()
: PrintMode(ulMapDJ600_CCM_CMY)
{
   dyeCount=3;
   CompatiblePens[0] = COLOR_PEN;   // only color pen allowed

//   strcpy(ModeName, "Color");
}

#ifdef APDK_EXTENDED_MEDIASIZE
Mode630DraftColorKCMY::Mode630DraftColorKCMY()
: PrintMode( ulMapDJ660_CCM_KCMY )
{
    theQuality = qualityDraft;
    Config.eHT = MATRIX;
    pmQuality = QUALITY_DRAFT;
}

Mode630DraftGrayK::Mode630DraftGrayK()
: GrayMode(ulMapDJ600_CCM_K)
{
   theQuality = qualityDraft;
   pmQuality = QUALITY_DRAFT;
}

Mode630DraftColorCMY::Mode630DraftColorCMY()
: PrintMode( ulMapDJ600_CCM_CMY )
{
    dyeCount=3;
    CompatiblePens[0] = COLOR_PEN;   // only color pen allowed
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
}

Mode630BestGrayK::Mode630BestGrayK()
: GrayMode(ulMapDJ600_CCM_K)
{
   theQuality = qualityPresentation;
   pmQuality = QUALITY_BEST;
   BaseResX = 600;
   ResolutionX[0] = 600;
#ifdef APDK_HIGH_RES_MODES
   BaseResY = 600;
   ResolutionY[0] = 600;
#else
   BaseResY = 300;
   ResolutionY[0] = 300;
#endif
}
#endif // APDK_EXTENDED_MEDIASIZE

Header630::Header630(Printer* p,PrintContext* pc)
    : Header(p,pc)
{  }

Header* DJ630::SelectHeader(PrintContext* pc)
{
    return new Header630(this,pc);
}

DRIVER_ERROR Header630::Send()
{   DRIVER_ERROR err;

    StartSend();

    // don't disable black extraction if we are in the color pen only mode
    // since the printer may actually have both pens so we want the printer
    // to print the K with the K pen if it actually has one
    if (thePrintMode->dyeCount != 3)
    {
        err = thePrinter->Send((const BYTE*)BlackExtractOff,
                            sizeof(BlackExtractOff)); // just pertains to 2-pen
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

#endif  // APDK_DJ630

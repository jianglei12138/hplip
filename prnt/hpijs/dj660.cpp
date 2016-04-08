/*****************************************************************************\
  dj660.cpp : Implimentation for the DJ660 class

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


#ifdef APDK_DJ6xx

#include "header.h"
#include "dj6xx.h"
#include "dj660.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ660_CCM_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_CMY[ 9 * 9 * 9 ];
//
// ** DJ660:Printer CLASS **
//

DJ660::DJ660(SystemServices* pSS,BOOL proto)
    : DJ6XX(pSS,NUM_DJ6XX_FONTS,proto)
// create two dummy font objects to be queried via EnumFont
{
DBG1("DJ660::VerifyPenInfo(): called\n");

    if ((!proto) && (IOMode.bDevID))
      {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
      }
    else ePen=BOTH_PENS;    // matches default mode

    pMode[DEFAULTMODE_INDEX] = new PrintMode( ulMapDJ660_CCM_KCMY );
    pMode[GRAYMODE_INDEX] = new GrayMode(ulMapDJ600_CCM_K);
    pMode[SPECIALMODE_INDEX] = new Mode660Draft();
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX+1] = new Mode660DraftGrayK();
    pMode[SPECIALMODE_INDEX+2] = new Mode660BestGrayK();
    ModeCount=5;
#else
    ModeCount=3;
#endif
    CMYMap = ulMapDJ660_CCM_KCMY;


DBG1("DJ 660 created\n");
}

Mode660Draft::Mode660Draft()
: PrintMode( ulMapDJ660_CCM_KCMY )
{
  theQuality = qualityDraft;

    Config.eHT = MATRIX;

    pmQuality = QUALITY_DRAFT;

//   strcpy(ModeName, "Draft");

}

#ifdef APDK_EXTENDED_MEDIASIZE
Mode660DraftGrayK::Mode660DraftGrayK()
: GrayMode(ulMapDJ600_CCM_K)
{
   theQuality = qualityDraft;
   pmQuality = QUALITY_DRAFT;
}

Mode660BestGrayK::Mode660BestGrayK()
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

Header* DJ660::SelectHeader(PrintContext* pc)
{
    return new Header6XX(this,pc);
}

DRIVER_ERROR DJ660::VerifyPenInfo()
{

    DRIVER_ERROR err=NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    err = ParsePenInfo(ePen);
    ERRCHECK;


    // check for the normal case
    if (ePen == BOTH_PENS)
        return NO_ERROR;


    // the 6XX printers are all two-pen, so trap
    // on any pen type that is not BOTH_PENS
    while ( ePen != BOTH_PENS )
    {

        switch (ePen)
        {
            case MDL_BOTH:
            case MDL_PEN:
                // user shouldn't be able to get photopen in a non-690...
                pSS->DisplayPrinterStatus(DISPLAY_PHOTO_PEN_WARN);
                break;
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
            return JOB_CANCELED;

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;

}

DRIVER_ERROR DJ660::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char* str;
    DRIVER_ERROR err = SetPenInfo(str, QueryPrinter);
    ERRCHECK;

    if (*str != '$')
    {
        return BAD_DEVICE_ID;
    }

    str++;  // skip $
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

APDK_END_NAMESPACE

#endif  //APDK_DJ660

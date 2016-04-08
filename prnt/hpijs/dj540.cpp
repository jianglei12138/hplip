/*****************************************************************************\
  dj540.cpp : Implimentation for the DJ540 class

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


#ifdef APDK_DJ540

#include "header.h"
#include "dj6xx.h"
#include "dj600.h"
#include "dj540.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_CMY[ 9 * 9 * 9 ];
//
// ** DJ540:Printer CLASS **
//
DJ540::DJ540(SystemServices* pSS, BOOL proto)
    : DJ6XX(pSS,NUM_DJ6XX_FONTS,proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else
    {
        ePen = COLOR_PEN;    // matches default mode
    }

    CMYMap = ulMapDJ600_CCM_CMY;
    pMode[DEFAULTMODE_INDEX] = new Mode600();
    pMode[GRAYMODE_INDEX] = new GrayMode(ulMapDJ600_CCM_K);
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX] = new Mode600DraftGrayK();
    pMode[SPECIALMODE_INDEX+1] = new Mode600DraftColor();
    ModeCount = 4;
#else
    ModeCount = 2;
#endif

DBG1("DeskJet 540 created\n");
}

PEN_TYPE DJ540::DefaultPenSet()
{
    return COLOR_PEN;
}

Header540::Header540(Printer* p,PrintContext* pc)
    : Header(p,pc)
{ }

DRIVER_ERROR Header540::Send()
// Sends 540-style header to printer.
// Identical to 600 Send except StartSend() has been removed.
// Removed ConfigureRasterData() to be downword compatable with 400 (uni-di only). des
{   DRIVER_ERROR err;

    // modes & margins are specified in StartSend - which the 540 doesn't call
    err=Modes();            // Set media source, type, size and quality modes.
    ERRCHECK;

    err=Margins();          // set margins
    ERRCHECK;

    err=Simple();           // set color mode and resolution
    ERRCHECK;

    err=Graphics();     // start raster graphics and set compression mode

return err;
}

Header* DJ540::SelectHeader(PrintContext* pc)
{
    return new Header540(this,pc);
}

DRIVER_ERROR DJ540::VerifyPenInfo()
// note: this function is identical to 600::VerifyPenInfo
{
    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    err = ParsePenInfo(ePen);
    ERRCHECK;

    if(ePen == BLACK_PEN || ePen == COLOR_PEN)
    // pen was recognized
    {
        return NO_ERROR;
    }

    // BLACK_PEN and COLOR_PEN are the only valid pens, so loop and
    // display error message until user cancels or a valid pen installed
    while(ePen != BLACK_PEN && ePen != COLOR_PEN)
    {
        pSS->DisplayPrinterStatus(DISPLAY_NO_PEN_DJ600);

        if(pSS->BusyWait(500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    // The 600/?540? will report OFFLINE for a while after the
    // pen has been installed.  Let's wait for it to
    // come online and not confuse the user with a potentially
    // bogus OFFLINE message

    if (pSS->BusyWait((DWORD)1000) == JOB_CANCELED)
        return JOB_CANCELED;

    return NO_ERROR;

}


DRIVER_ERROR DJ540::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
     char* c;
     DRIVER_ERROR err = SetPenInfo(c, QueryPrinter);
     ERRCHECK;

    if (*c != '$')
    {
        return BAD_DEVICE_ID;
    }

     c++;   // skip $

    // parse penID

    if(c[0] == 'D')         // D = kukla color pen
    {
        ePen = COLOR_PEN;
    }
    else if(c[0] == 'E')    // E = triad black pen
    {
            ePen = BLACK_PEN;
    }
    else
    {
        ePen = NO_PEN;
    }

    return NO_ERROR;
}

APDK_END_NAMESPACE

#endif  //APDK_DJ540

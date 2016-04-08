/*****************************************************************************\
  dj350.cpp : Implimentation for the DJ350 class

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


#ifdef APDK_DJ350

#include "header.h"
#include "dj6xx.h"
#include "dj600.h"
#include "dj350.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_CMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];

DJ350::DJ350(SystemServices* pSS, int numfonts, BOOL proto)
           : DJ600 (pSS, proto)
{
    CMYMap = ulMapDJ600_CCM_CMY;
//    pMode[SPECIALMODE_INDEX]   = new Mode350KPhoto (ulMapDJ600_CCM_K);
//    pMode[SPECIALMODE_INDEX] = new Mode350CPhoto (CMYMap);

#ifdef APDK_EXTENDED_MEDIASIZE
    ModeCount = 5;
#else
    ModeCount = 2;
#endif

    DBG1("DJ350 created\n");
}

#if 0
Mode350KPhoto::Mode350KPhoto (uint32_t *map)
    : PrintMode (map)
{
    theQuality = qualityPresentation;
    dyeCount = 1;
    BaseResX = 600;
    BaseResY = 600;
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    CompatiblePens[0] = BLACK_PEN;
//    medium = mediaGlossy;
    MixedRes = FALSE;
//    strcpy (ModeName, "BlackBest");
}

Mode350CPhoto::Mode350CPhoto (uint32_t *map)
    : PrintMode (map)
{
    theQuality = qualityPresentation;
    dyeCount = 3;

    CompatiblePens[0] = COLOR_PEN;  // only thing allowed
//    MixedRes = TRUE;
    medium =  mediaSpecial;

//    strcpy(ModeName, "ColorPhoto");
}
#endif

Header350::Header350 (Printer *p, PrintContext* pc) : Header (p, pc)
{ }

DRIVER_ERROR Header350::Send ()
{
    DRIVER_ERROR err;

    StartSend ();

    if (dyeCount == 3 || ResolutionY[0] == 600) // color pen
    {
        err = ConfigureRasterData ();
        ERRCHECK;
    }
    else                     // black pen
    {
        err = Simple ();    // set color mode and resolution
        ERRCHECK;
    }


    if (ResolutionY[0] == 600)
    {
        char uom[10];
        sprintf(uom,"%c%c%c%d%c",ESC,'&','u',thePrintMode->ResolutionY[K],'D');
        err=thePrinter->Send((const BYTE*)uom, 7 );
        ERRCHECK;
    }


    err = Graphics();       // start raster graphics and set compression mode

    return err;
}

Header* DJ350::SelectHeader (PrintContext *pc)
{
    return new Header350 (this, pc);
}

APDK_END_NAMESPACE

#endif  //APDK_DJ350

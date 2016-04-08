/*****************************************************************************\
  apollo21xx.cpp : Implimentation for the Apollo21xx class

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

#ifdef APDK_APOLLO21XX

#include "header.h"
#include "apollo2xxx.h"
#include "apollo21xx.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

//! Constructor for Apollo21xx
/*!
******************************************************************************/
Apollo21xx::Apollo21xx(SystemServices* pSS, BOOL proto)
    : Apollo2xxx(pSS,proto)
{
    // set ePen to COLOR in all cases
        ePen=COLOR_PEN;    // matches default mode
    // because of firmware bug, we make COLOR the default
    PrintMode* pm = pMode[DEFAULTMODE_INDEX];
    pMode[DEFAULTMODE_INDEX] = pMode[SPECIALMODE_INDEX];
    pMode[SPECIALMODE_INDEX] = pm;
}

DRIVER_ERROR Apollo21xx::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
// firmware always reports BOTH_PENS
// so we will act like it is a color pen only
{
    ePen=COLOR_PEN;
    return NO_ERROR;
}

Header21xx::Header21xx(Printer* p,PrintContext* pc)
    : Header(p,pc)
{  }

Header* Apollo21xx::SelectHeader(PrintContext* pc)
{
    return new Header21xx(this,pc);
}

DRIVER_ERROR Header21xx::Send()
{   DRIVER_ERROR err;

    StartSend();

    // the 2100 always reports two pens, AND mishandles CMY unless you send
    // BlackExtractOff

    err = thePrinter->Send((const BYTE*)BlackExtractOff,
                        sizeof(BlackExtractOff)); // just pertains to 2-pen
    ERRCHECK;

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

#endif  //APDK_APOLLO21XX

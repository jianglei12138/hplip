/*****************************************************************************\
  dj6xx.cpp : Implimentation for the DJ6XX class

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


#if defined(APDK_DJ540) || defined(APDK_DJ600) || defined(APDK_DJ6xx)

#include "header.h"
#include "dj6xx.h"

//#include "printerproxy.h"

APDK_BEGIN_NAMESPACE
//
// ** DJ6xx:Printer CLASS **
//

DJ6XX::DJ6XX(SystemServices* pSS,
                       int numfonts, BOOL proto)
       : Printer(pSS, numfonts, proto)
{
    DBG1("DJ6XX constructor called, colormap pending\n");
}



Header6XX::Header6XX(Printer* p,PrintContext* pc)
    : Header(p,pc)
{ }

DRIVER_ERROR Header6XX::Send()
{   DRIVER_ERROR err;

    StartSend();

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

#endif //APDK_DJ650 || APDK_DJ600 || APDK_DJ6xx

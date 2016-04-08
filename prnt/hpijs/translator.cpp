/*****************************************************************************\
   translator.h : Implimentation for the RasterSender class

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


#include "header.h"
#include "halftoner.h"

APDK_BEGIN_NAMESPACE

RasterSender::RasterSender(Printer* pP, PrintContext* pPC,
                       Job* pJob,Halftoner* pHalftoner)
    : thePrinter(pP), thePrintContext(pPC), theJob(pJob),theHalftoner(pHalftoner)
{
    constructor_error=NO_ERROR;
    m_lNBlankRasters = 0;
}

RasterSender::~RasterSender()
{
}

////////////////////////////////////////////////////////////////////////
BOOL RasterSender::Process(RASTERDATA* InputRaster)
{
    DRIVER_ERROR err=NO_ERROR;
    BOOL bOutput=FALSE;
    if (InputRaster && (InputRaster->rasterdata[COLORTYPE_COLOR] || InputRaster->rasterdata[COLORTYPE_BLACK]))
    {
        err=SendRaster(InputRaster);
        if (err==NO_ERROR)
            bOutput=TRUE;
    }
    else
    {

/*
 *      Replace CAP move command here with vertical raster move command.
 *      If buffering this causes code bloat, do send the command here.
 */
        err = theJob->SendCAPy ();

    }

    myphase->err = err;

  return bOutput;
}


DRIVER_ERROR RasterSender::SendRaster(RASTERDATA* InputRaster)
{
    DRIVER_ERROR err;
    BOOL lastplane,firstplane;

    if (theHalftoner)
    {
        lastplane = theHalftoner->LastPlane();
        firstplane = theHalftoner->FirstPlane();
    }
    else lastplane=firstplane=TRUE;

	// older code used to send regular CAPy move as part of text-synchronization scheme
    if (firstplane)
            err=theJob->SendCAPy();

    err = thePrinter->Encapsulate (InputRaster, lastplane);
    ERRCHECK;

  return err;
}

APDK_END_NAMESPACE


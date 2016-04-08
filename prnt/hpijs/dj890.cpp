/*****************************************************************************\
  dj890.cpp : Implimentation for the DJ890 class

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

#if defined(APDK_DJ890)

#include "header.h"
#include "io_defs.h"
#include "dj8xx.h"
#include "dj890.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ895_Binary_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[9 * 9 * 9 ];

DJ890::DJ890(SystemServices* pSS,
                       int numfonts, BOOL proto)
    : DJ8xx(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen=BOTH_PENS;    // matches default mode

#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[GRAYMODE_INDEX]      = new DJ895Mode5 ();   // Normal Gray K
#else
    pMode[GRAYMODE_INDEX]      = new GrayMode (ulMapDJ600_CCM_K);   // Normal Gray K
#endif
    pMode[DEFAULTMODE_INDEX]   = new DJ895Mode1 ();   // Normal Color
    pMode[SPECIALMODE_INDEX] = new DJ895Mode3 ();   // Draft Color
    pMode[SPECIALMODE_INDEX+1] = new DJ895Mode4 ();   // Draft Gray K
    ModeCount = 4;

    CMYMap = ulMapDJ895_Binary_KCMY;

    DBG1("DJ890 created\n");
}

Header* DJ890::SelectHeader(PrintContext* pc)
{
    return new Header895(this,pc);
}

APDK_END_NAMESPACE

#endif  // defined APDK_DJ890

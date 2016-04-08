/*****************************************************************************\
  apollo2560.cpp : Implimentation for the Apollo2560 class

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


#ifdef APDK_APOLLO2560

#include "header.h"
#include "apollo2xxx.h"
#include "apollo2560.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE

Apollo2560::Apollo2560(SystemServices* pSS, BOOL proto)
    : Apollo2xxx(pSS,proto)
// for 2500/2600
// set CMY default for unidi
{
    PrintMode* pm = pMode[DEFAULTMODE_INDEX];
    pMode[DEFAULTMODE_INDEX] = pMode[SPECIALMODE_INDEX];
    pMode[SPECIALMODE_INDEX] = pm;
}

APDK_END_NAMESPACE

#endif  //APDK_APOLLO2560

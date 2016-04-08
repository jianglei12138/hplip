/*****************************************************************************\
  creator.cpp : Open Source Imaging Halftoner and ColorMatcher Creation

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
#include "resources.h"

#include "hptypes.h"
#include "colormatch.h"
#include "colormatcher_open.h"
#include "halftoner.h"
#include "halftoner_open.h"

APDK_BEGIN_NAMESPACE

// routine to return generic ColorMatcher to Job
ColorMatcher* Create_ColorMatcher
(
    SystemServices* pSys,
    ColorMap cm,
    unsigned int DyeCount,
    unsigned int iInputWidth
)
{
    return new ColorMatcher_Open(pSys,cm,DyeCount,iInputWidth);
}

// routine to return generic Halftoner to Job
Halftoner* Create_Halftoner
(
    SystemServices* pSys,
    PrintMode* pPM,
    unsigned int iInputWidth,
    unsigned int iNumRows[],
    unsigned int HiResFactor,
    BOOL usematrix
)
{
    return new Halftoner_Open(pSys,pPM,iInputWidth, iNumRows,HiResFactor,usematrix);
}

BYTE* GetHT3x3_4()
{
    return (BYTE*)HT300x3004level_open;
}

BYTE* GetHT6x6_4()
{
    return (BYTE*)HT600x6004level895_open;
}

BYTE* GetHTBinary()
{
    return (BYTE*)HTBinary_open;
}

BYTE* GetHT6x6_4_970()
{
    return (BYTE*)HT600x6004level970_open;
}

#ifdef APDK_DJ3320
BYTE *GetHT12x12x1_4100_Photo_Best ()
{
    return (BYTE *) HT1200x1200x1PhotoBest_open;
}
#endif // APDK_DJ3320

// functions to identify versions of system
BOOL ProprietaryImaging()
{
    return FALSE;
}

BOOL ProprietaryScaling()
{
    return FALSE;
}

APDK_END_NAMESPACE


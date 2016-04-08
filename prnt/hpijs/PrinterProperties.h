/*****************************************************************************\
  Copyright (c) 2002 - 2015, HP Co.
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
#include <stdio.h>

#include "hpprintapi.h"

APDK_USING_NAMESPACE

class PlatformServices : public SystemServices
{
public:
    PlatformServices ();
    ~PlatformServices ();
    void            DisplayPrinterStatus (DISPLAY_STATUS ePrinterStatus);
    DRIVER_ERROR    BusyWait (DWORD msec);
    DRIVER_ERROR    ReadDeviceID (BYTE *strID, int iSize);
    BYTE            *AllocMem (int iMemSize);
    void            FreeMem (BYTE* pMem);
    BOOL            GetStatusInfo (BYTE *bStatReg);
    DRIVER_ERROR    ToDevice (const BYTE *pBuffer, DWORD *dwCount);
    DRIVER_ERROR    FromDevice (BYTE *pReadBuff, DWORD *wReadCount);
    DWORD           GetSystemTickCount (void);
    float           power (float x, float y);
};

typedef struct
{
    PAPER_SIZE      iPaperSize;
    const char      *szName;
} PaperSizeInfo;

PaperSizeInfo   PaperSizeInfoData[] =
{
    {LETTER, "Letter"},
    {LEGAL,  "Legal"},
    {EXECUTIVE, "Executive"},
    {A3,        "A3"},
    {A4,        "A4"},
    {A5,        "A5"},
    {A6,        "A6"},
    {PHOTO_SIZE,     "Photo"},
    {PHOTO_5x7,       "5x7"},
    {B4,        "B4"},
    {B5,        "B5"},
    {OUFUKU,    "Oufuku-Hagaki"},
    {HAGAKI,    "Hagaki"},
    {SUPERB_SIZE,   "Super B"},
    {FLSA,      "Flsa"},
    {ENVELOPE_NO_10,   "Number 10 Envelope"},
    {ENVELOPE_A2,   "A2 Envelope"},
    {ENVELOPE_C6,   "C6 Envelope"},
    {ENVELOPE_DL,   "DL Envelope"},
    {ENVELOPE_JPN3,    "Japanese Envelope #3"},
    {ENVELOPE_JPN4,    "Japanese Envelope #4"}
};

typedef struct
{
    char    szClassName[16];
    char    szPaperSize[32];
    float   fPhysicalWidth;
    float   fPhysicalHeight;
    float   fPrintableWidth;
    float   fPrintableHeight;
    float   fTopLeftX;
    float   fTopLeftY;
} PrinterProperties;

PlatformServices    *pSys = NULL;
PrintContext        *pPC  = NULL;
BOOL                CreatePrinterProperties ();
int                 iCurrentPrinter = UNSUPPORTED;
int                 iPrinterClasses[MAX_PRINTER_TYPE];
int                 iLastPrinter = 0;

#if 0
char    *szPaperSizeNames[] =
{
    "Letter",
    "A4",
    "Legal"
    "4x6 with tear-off tab",
    "A6",
    "4x6 Index Card",
    "B4",
    "B5",
    "Oufuku-Hagaki",
    "Hagaki",
    "A6 with tear-off tab",
    "A3",
    "A5",
    "Ledger",
    "Super B",
    "Executive",
    "Flsa",
    "CUSTOM_SIZE",
	"Number 10 Envelope",
	"A2 Envelope",
	"C6 Envelope",
	"DL Envelope",
	"Japanese Envelope #3",
	"Japanese Envelope #4",
    "5x7"
};
#endif

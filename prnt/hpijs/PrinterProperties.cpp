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
#include "header.h"

#include "PrinterProperties.h"
#include "models.h"

PlatformServices::PlatformServices () : SystemServices ()
{
}

PlatformServices::~PlatformServices ()
{
}

float PlatformServices::power (float x, float y)
{
    return float (pow (x, y));
}

BYTE * PlatformServices::AllocMem (int iMemSize)
{
    return (BYTE *) malloc (iMemSize);
}

void PlatformServices::FreeMem (BYTE *pMem)
{
    free (pMem);
}

DWORD PlatformServices::GetSystemTickCount (void)
{
    return 0;
}

DRIVER_ERROR PlatformServices::BusyWait (DWORD msec)
{
    return NO_ERROR;
}

void PlatformServices::DisplayPrinterStatus (DISPLAY_STATUS ePrinterStatus)
{
}

DRIVER_ERROR PlatformServices::ReadDeviceID (BYTE *strID, int iSize)
{
    DRIVER_ERROR    eResult = IO_ERROR;
    return eResult;
}

BOOL PlatformServices::GetStatusInfo (BYTE *bStatReg)
{
    BOOL    bStatus = FALSE;
    return bStatus;
}

DRIVER_ERROR PlatformServices::FromDevice (BYTE *pReadBuff, DWORD *wReadCount)
{
    return NO_ERROR;
}

DRIVER_ERROR PlatformServices::ToDevice (const BYTE *pBuffer, DWORD *dwCount)
{
    *dwCount = 0;
    return NO_ERROR;
}

BOOL CreatePrinterProperties ()
{
    pSys = new PlatformServices ();
    pSys->IOMode.bDevID = FALSE;
    pSys->IOMode.bStatus = FALSE;
    pPC = new PrintContext (pSys);
    iLastPrinter = MAX_PRINTER_TYPE;
    return TRUE;
}

int GetPrinterProperties (PrinterProperties *pPrinterProperties)
{
    BOOL    bResult = TRUE;
    int     i;
    DRIVER_ERROR    err;
    if (pPrinterProperties == NULL)
    {
        return FALSE;
    }
    if (pSys == NULL)
    {
        bResult = CreatePrinterProperties ();
        if (bResult == FALSE)
        {
            return (int) bResult;
        }
    }

    while (1)
    {
        iCurrentPrinter++;
        if (iCurrentPrinter == iLastPrinter)
        {
            if (pPC)
            {
                delete pPC;
            }
            if (pSys)
            {
                delete pSys;
            }
            return FALSE;
        }
        err = pPC->SelectDevice ((PRINTER_TYPE) iCurrentPrinter);
        if (err != NO_ERROR)
        {
            if (iCurrentPrinter < iLastPrinter)
            {
                continue;
            }
            delete pPC;
            delete pSys;
            return FALSE;
        }
        strcpy (pPrinterProperties->szClassName, ModelName[iCurrentPrinter]);
        for (i = 0; i < (int) (sizeof (PaperSizeInfoData) / sizeof (PaperSizeInfo)); i++)
        {
            err = pPC->SetPaperSize (PaperSizeInfoData[i].iPaperSize, 0);
            if (err == WARN_ILLEGAL_PAPERSIZE)
            {
                continue;
            }
            strcpy (pPrinterProperties->szPaperSize, PaperSizeInfoData[i].szName);
            pPrinterProperties->fPhysicalWidth   = pPC->PhysicalPageSizeX ();
            pPrinterProperties->fPhysicalHeight  = pPC->PhysicalPageSizeY ();
            pPrinterProperties->fPrintableWidth  = pPC->PrintableWidth ();
            pPrinterProperties->fPrintableHeight = pPC->PrintableHeight ();
            pPrinterProperties->fTopLeftX        = pPC->PrintableStartX ();
            pPrinterProperties->fTopLeftY        = pPC->PrintableStartY ();

            fprintf (stdout, "%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                     pPrinterProperties->szClassName,
                     pPrinterProperties->szPaperSize,
                     pPrinterProperties->fPhysicalWidth,
                     pPrinterProperties->fPhysicalHeight,
                     pPrinterProperties->fTopLeftX,
                     pPrinterProperties->fTopLeftY,
                     pPrinterProperties->fPrintableWidth,
                     pPrinterProperties->fPrintableHeight);

        }
    }
}

int    main ()
{
    PrinterProperties   pPrProp;
    GetPrinterProperties (&pPrProp);
    return 0;
}

/*****************************************************************************\
    services.h : HP Inkjet Server

    Copyright (c) 2001 - 2015, HP Co.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the Hewlett-Packard nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#ifndef hpijs_services_INCLUDED
#define hpijs_services_INCLUDED

#include <stdio.h>
#include <math.h>

#define EVENT_PRINT_FAILED_MISSING_PLUGIN 502

#include "global_types.h"

APDK_USING_NAMESPACE

#define BUFFER_CHUNK_SIZE 1024 * 1024 * 2

class UXServices:public SystemServices
{
public:
  UXServices ();
  virtual ~ UXServices ();

  DRIVER_ERROR BusyWait (DWORD msec);

  DRIVER_ERROR ToDevice (const BYTE* pBuffer, DWORD* wCount);

  DRIVER_ERROR FromDevice (BYTE* pReadBuff, DWORD * wReadCount)
  {
    return NO_ERROR;
  }

  DRIVER_ERROR ReadDeviceID (BYTE * strID, int iSize);
  BOOL GetStatusInfo (BYTE * bStatReg);

  DWORD GetSystemTickCount (void)
  {
    return 0;
  }

  /////////////////////////////////////////////////////////
  BOOL YieldToSystem (void)
  {
    return 0;
  }

  BYTE GetRandomNumber ()
  {
    return rand ();
  }

  void DisplayPrinterStatus (DISPLAY_STATUS ePrinterStatus);

  BYTE GetStatus ()
  {
    return 0;
  }

  DRIVER_ERROR GetDevID (BYTE * pDevIDString, int *iDevIDLength);

  DRIVER_ERROR GetECPStatus (BYTE * pStatusString, int *pECPLength,
			     int ECPChannel)
  {
    return NO_ERROR;
  }

#ifdef HP_PRINTVIEW
    int GetPJLHeaderBuffer (char **szPJLBuffer);
#endif

  BOOL GetVerticalAlignmentValue(BYTE* cVertAlignVal);
  BOOL GetVertAlignFromDevice();

  BYTE *AllocMem (int iMemSize)
  {
    return (BYTE *) malloc (iMemSize);
  }

  void FreeMem (BYTE * pMem)
  {
    free (pMem);
  }

  float power (float x, float y)
  {
    return pow (x, y);
  }

  int ProcessRaster(char *raster, char *k_raster);
  int InitDuplexBuffer();
  int SendBackPage ();
  int MapPaperSize(float width, float height);
  void MapModel(const char *nam);
  const char * GetDriverMessage(DRIVER_ERROR err);

  void  ResetIOMode (BOOL bDevID, BOOL bStatus);

  void InitSpeedMechBuffer ();
  int  CopyData (const BYTE *pBuffer, DWORD iCount);
  int  SendPreviousPage ();
  void SendLastPage ();
  BOOL IsSpeedMechEnabled ()
  {
      return m_bSpeedMechEnabled;
  }
  void EnableSpeedMech (BOOL bFlag)
  {
      m_bSpeedMechEnabled = bFlag;
  }

  BOOL BackPage;
  int CurrentRaster;
  BYTE **RastersOnPage;
  BYTE **KRastersOnPage;

  IjsPageHeader ph;

  const float *Margin;
  int Model;      /* selected device: -1=no, 1=yes */
  int OutputPath;   /* open file descriptor */
  QUALITY_MODE Quality;
  MEDIATYPE MediaType;
  COLORMODE ColorMode;
  PEN_TYPE PenSet;
  int MediaPosition;
  float PaperWidth;    /* physical width in inches */
  float PaperHeight;   /* physical height in inches */
  int Duplex;
  int Tumble;
  int FullBleed;
  int FirstRaster;
  int KRGB;            /* 0=no, 1=yes */
  int hpFD;          /* CUPS hp backend file descriptor. */
  DISPLAY_STATUS DisplayStatus; /* current DisplayPrinterStatus */
  int VertAlign;    /* for Crossbow/Spear */

  PrintContext *pPC;
  Job *pJob;
  FILE    *outfp;
  int     m_iLogLevel;

protected:
  
  // for internal use
  virtual BYTE* AllocMem (int iMemSize, BOOL trackmemory)
  { return AllocMem(iMemSize); }
  
  virtual void FreeMem (BYTE* pMem, BOOL trackmemory)
  { FreeMem(pMem); }
  BOOL  CanDoBiDi ();

private:
    int    m_iPageCount;
    BOOL   m_bSpeedMechEnabled;
    int    m_iPclBufferSize;
    int    m_iCurPclBufferPos;
    BYTE   *m_pbyPclBuffer;
};

#endif        /* hpijs_services_INCLUDED */


/*****************************************************************************\
  capture.cpp : Implimentation for capturing functions

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


#ifdef APDK_CAPTURE
#include "header.h"
#include "script.h"

APDK_BEGIN_NAMESPACE

//////////////////////////////////////////////////////////////
// Capture functions belonging to API


void PrintContext::Capture_PrintContext(unsigned int InputPixelsPerRow,
                                        unsigned int OutputPixelsPerRow,
                                        PAPER_SIZE ps,
                                        IO_MODE IOMode)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokPrintContext);
    pS->PutDebugInt(InputPixelsPerRow);
    pS->PutDebugInt(OutputPixelsPerRow);
    pS->PutDebugByte(ps);
    pS->PutDebugByte(IOMode.bDevID);
    pS->PutDebugByte(IOMode.bStatus);
    if (!IOMode.bDevID)
        return;

    // need to simulate bidi, remember model and pens
    pS->PutDebugString((const char*)pSS->strModel,strlen(pSS->strModel));
    pS->PutDebugString((const char*)pSS->strPens,strlen(pSS->strPens));

}


void PrintContext::Capture_RealizeFont(const unsigned int ptr,
                                       const unsigned int index,
                                       const BYTE bSize,
                                       const TEXTCOLOR eColor,
                                       const BOOL bBold,
                                       const BOOL bItalic,
                                       const BOOL bUnderline)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokRealizeFont);
    pS->PutDebugInt(ptr);
    pS->PutDebugByte(index);
    pS->PutDebugByte(bSize);
    pS->PutDebugByte(eColor);
    pS->PutDebugByte(bBold);
    pS->PutDebugByte(bItalic);
    pS->PutDebugByte(bUnderline);

}

void PrintContext::Capture_SetPixelsPerRow(unsigned int InputPixelsPerRow,
                                           unsigned int OutputPixelsPerRow)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSetPixelsPerRow);
    pS->PutDebugInt(InputPixelsPerRow);
    pS->PutDebugInt(OutputPixelsPerRow);
}

void PrintContext::Capture_SetInputResolution(unsigned int Res)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSetRes);
    pS->PutDebugInt(Res);

}


void PrintContext::Capture_SelectDevice(const PRINTER_TYPE Model)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSelectDevice);
    pS->PutDebugByte(Model);

}


void PrintContext::Capture_SelectDevice(const char* szDevIdString)
{
    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSelectDevice);
    pS->PutDebugString(szDevIdString, strlen(szDevIdString));
}


void PrintContext::Capture_SelectPrintMode(unsigned int modenum)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSelectPrintMode);
    pS->PutDebugByte(modenum);
}

void PrintContext::Capture_SetPaperSize(PAPER_SIZE ps, BOOL bFullBleed)
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokSetPaperSize);
    pS->PutDebugByte(ps);
    pS->PutDebugByte (bFullBleed);
}




void Job::Capture_Job(PrintContext* pPC)
{

    if (! thePrintContext->pSS->Capturing)
        return;

    Scripter *pS = thePrintContext->pSS->pScripter;

    pS->PutDebugToken(tokJob);

}

void Job::Capture_SendRasters(BYTE* BlackImageData, BYTE* ColorImageData)
{

    if (! thePrintContext->pSS->Capturing)
        return;

    Scripter *pS = thePrintContext->pSS->pScripter;

    pS->PutDebugToken(tokSendRasters);
    unsigned int len=0;
    if (BlackImageData != NULL)
        len= (thePrintContext->InputWidth/8) + (thePrintContext->InputWidth%8);
	pS->PutDebugStream(BlackImageData, len);
	len = 0;
    if (ColorImageData != NULL)
        len= thePrintContext->OutputWidth*3;
    pS->PutDebugStream(ColorImageData, len);

}

#if defined(APDK_FONTS_NEEDED)
void Job::Capture_TextOut(const char* pTextString,
                          unsigned int iLenString,
                          const Font& font,
                          unsigned int iAbsX,
                          unsigned int iAbsY)
{
    SystemServices* pSS = thePrintContext->pSS;

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokTextOut);
    pS->PutDebugInt((int)&font);
    pS->PutDebugString(pTextString,iLenString);
    pS->PutDebugInt(iAbsX);
    pS->PutDebugInt(iAbsY);
}
#endif

void Job::Capture_NewPage()
{
    SystemServices* pSS = thePrintContext->pSS;
    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokNewPage);

}


void Job::Capture_dJob()
{
    SystemServices* pSS = thePrintContext->pSS;
    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokdJob);

}

#if defined(APDK_FONTS_NEEDED)
void Font::Capture_dFont(const unsigned int ptr)
{

    if (internal)
        return;
    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokdFont);
    pS->PutDebugInt(ptr);
}
#endif

void PrintContext::Capture_dPrintContext()
{

    if (! pSS->Capturing)
        return;

    Scripter *pS = pSS->pScripter;

    pS->PutDebugToken(tokdPrintContext);

}

APDK_END_NAMESPACE

#endif

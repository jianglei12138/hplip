/*****************************************************************************\
  job.h : Interface/Implimentation for the Job class

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


#ifndef APDK_JOB_H
#define APDK_JOB_H

APDK_BEGIN_NAMESPACE

#if defined(APDK_VIP_COLORFILTERING)
class TErnieFilter;
#endif

class RasterSender;
class Header;
class TextTranslator;
class TextManager;
class Halftoner;
class ColorMatcher;
class Scaler;


//Job
//! Manages processes to create output for the device
/*! \class Job job.h "hpprintapi.h"
This object receives processes and transmits data to be printed by one device
on successive pages. The PrintContext and Job together form the core of the
driver proper.

\sa PrintContext
******************************************************************************/
class Job
{
friend class RasterSender;

public:
    Job(PrintContext* pPC);

    virtual ~Job();

    DRIVER_ERROR constructor_error;     // caller must check upon return

    DRIVER_ERROR SendRasters(BYTE* ImageData=(BYTE*)NULL);

	DRIVER_ERROR SendRasters(BYTE* BlackImageData, BYTE* ColorImageData);

	DRIVER_ERROR SupportSeparateBlack(BOOL* bSeparateBlack);

#if defined(APDK_FONTS_NEEDED)
    DRIVER_ERROR TextOut(const char* pTextString, unsigned int iLenString,
                            const Font& font, int iAbsX, int iAbsY );
    // return theTextManager->TextOut(pTextString,iLenString,font,iAbsX,iAbsY);
#endif

    DRIVER_ERROR Flush();
    DRIVER_ERROR NewPage();

private:

    PrintContext* thePrintContext;
    SystemServices* pSS;
    Printer* thePrinter;
    Pipeline* thePipeline;
    TextTranslator* pText;
    RasterSender* pSender;
    Halftoner* pHalftoner;
    ColorMatcher* pColorMatcher;
    Scaler* pResSynth;
	Scaler* pBlackPlaneReplicator;
    Scaler* pReplicator;
    Compressor* theCompressor;
	Compressor* pBlackPlaneCompressor;
    Header* pHead;
#if defined(APDK_VIP_COLORFILTERING)
    TErnieFilter* pErnie;
#endif

#if defined(APDK_FONTS_NEEDED)
    TextManager* theTextManager;
#endif

    PrintMode* CurrentMode;

    unsigned int fcount;
    unsigned int skipcount;
    unsigned int RowsInput;
    BYTE* BlankRaster;
	BYTE* BlackRaster;

    unsigned int CAPy;          // maintains cursor-pos for graphics purposes,
                        // independent of intervening text positioning
    BOOL DataSent;

    unsigned int ResBoost;                  // for horizontal expansion
    unsigned int numrows[MAXCOLORPLANES];   // rows per call for mixed-res only
    unsigned int OutputWidth;
    unsigned int InputWidth;
    unsigned int ScaleFactor;

    DRIVER_ERROR Configure();
    DRIVER_ERROR InitScaler();

    DRIVER_ERROR newpage();
    DRIVER_ERROR SetupHalftoning(HALFTONING_ALGORITHM eHT);
    DRIVER_ERROR SetupColorMatching();
    DRIVER_ERROR SendCAPy();
    DRIVER_ERROR sendrasters(BYTE* BlackImageData=(BYTE*)NULL, BYTE* ColorImageData=(BYTE*)NULL);
    DRIVER_ERROR setblankraster();
	DRIVER_ERROR setblackraster();
    void SetOutputWidth();

#ifdef APDK_CAPTURE
    void Capture_Job(PrintContext* pPC);
    void Capture_dJob();
    void Capture_SendRasters(BYTE* BlackImageData, BYTE* ColorImageData);
#if defined(APDK_FONTS_NEEDED)
    void Capture_TextOut(const char* pTextString, unsigned int iLenString,
         const Font& font, unsigned int iAbsX, unsigned int iAbsY );
#endif
    void Capture_NewPage();
#endif

#ifdef APDK_USAGE_LOG
    int UTextCount;
    int UText;
#define UTextSize 100
    char UHeader[UTextSize*2];
#endif

}; //Job

APDK_END_NAMESPACE

#endif //APDK_JOB_H

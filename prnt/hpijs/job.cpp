/*****************************************************************************\
  job.cpp : Implimentation for the Job class

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
#include "colormatch.h"
#include "scaler_open.h"
#include "scaler_prop.h"

APDK_BEGIN_NAMESPACE
extern Halftoner* Create_Halftoner( SystemServices* pSys, PrintMode* pPM,
                unsigned int iInputWidth, unsigned int iNumRows[], unsigned int HiResFactor,
                BOOL usematrix);
extern Scaler* Create_Scaler(SystemServices* pSys,int inputwidth,
                             int numerator,int denominator,BOOL vip,unsigned int BytesPerPixel);
extern ColorMatcher* Create_ColorMatcher( SystemServices* pSys,
                                   ColorMap cm,unsigned int DyeCount,
                                   unsigned int iInputWidth);
extern BOOL ProprietaryImaging();
extern BOOL ProprietaryScaling();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern float frac(float f);

#define InitialPixelSize 3  // means first data into pipeline is RGB24
#define Bytes(x) ((x/8)+(x%8))

//////////////////////////////////////////////////////////////////////////
// The Job object receives data for a contiguous set of pages targeted at
// a single printer, with a single group of settings encapsulated in the
// PrintContext.
// At least one page will be ejected for a job, if any data at all
// is printed (no half-page jobs). If settings are to be changed, this
// must be done between jobs.
//

/*!
constructor - must pass a valid PrintContext to create the job.
Check constructor_error for NO_ERROR before continuing.
*/
Job::Job
(
    PrintContext* pPC
) :
    thePrintContext(pPC),
    pSS(pPC->pSS),
    thePrinter(NULL),
    thePipeline(NULL),
    pText(NULL),
    pSender(NULL),
    pHalftoner(NULL),
    pColorMatcher(NULL),
    pResSynth(NULL),
	pBlackPlaneReplicator(NULL),
    pReplicator(NULL),
    theCompressor(NULL),
	pBlackPlaneCompressor(NULL),
    pHead(NULL),
#if defined(APDK_VIP_COLORFILTERING)
    pErnie(NULL),
#endif
#if defined(APDK_FONTS_NEEDED)
    theTextManager(NULL),
#endif
    CurrentMode(pPC->CurrentMode),
    fcount(0),
    skipcount(0),
    RowsInput(0),
    BlankRaster(NULL),
	BlackRaster(NULL),
    DataSent(FALSE)
#ifdef APDK_USAGE_LOG
    , UText(0),
    UTextCount(0)
#endif
{
#ifdef APDK_CAPTURE
    Capture_Job(pPC);
#endif
    unsigned int i;
    constructor_error = NO_ERROR;

    if (!thePrintContext->PrinterSelected())
      {
        constructor_error = NO_PRINTER_SELECTED;
        return;
      }
    thePrinter = thePrintContext->thePrinter;


    for (i=0; i<(unsigned)MAXCOLORPLANES; i++)
        if (CurrentMode->MixedRes)      // means res(K) !+ res(C,M,Y)
            numrows[i] =  CurrentMode->ResolutionX[i] / CurrentMode->BaseResX;
        else numrows[i]=1;

    ResBoost = CurrentMode->BaseResX / CurrentMode->BaseResY;
    if (ResBoost==0) ResBoost=1;                    // safety


    pHead = thePrinter->SelectHeader(thePrintContext);
    CNEWCHECK(pHead);


    // set up blank raster used by SendRasters
    constructor_error=setblankraster();
    CERRCHECK;

     // flush any garbage out of the printer's buffer
    constructor_error=thePrinter->Flush();
    CERRCHECK;
    // send the PCL header
    constructor_error=pHead->Send();
    CERRCHECK;
    // set CAPy after sending header in case header used it
    CAPy=pHead->CAPy;


    constructor_error=Configure();
    CERRCHECK;


#if defined(APDK_FONTS_NEEDED)

    pText = new TextTranslator(thePrinter, pHead->QualityCode(),
                               thePrintContext->CurrentMode->dyeCount);
    CNEWCHECK(pText);

    theTextManager = NULL;
    if (CurrentMode->bFontCapable)
    {
        unsigned int pixelsX = (unsigned int) (int) (thePrintContext->printablewidth() *
                                CurrentMode->BaseResX);
        unsigned int pixelsY = (unsigned int) (int) (thePrintContext->printableheight() *
                                CurrentMode->BaseResY);

        theTextManager=new TextManager( pText, pixelsX,pixelsY );
        CNEWCHECK(theTextManager);
        constructor_error = theTextManager->constructor_error;
        CERRCHECK;
    }
#endif

    if (!thePrintContext->ModeAgreesWithHardware(TRUE))
        constructor_error = WARN_MODE_MISMATCH;

#ifdef APDK_USAGE_LOG
    UHeader[0]='\0';
#endif
} //Job

DRIVER_ERROR Job::SetupHalftoning(HALFTONING_ALGORITHM eHT)
// Subroutine of Job constructor to handle ColorMatcher/Halftoner initialization.
// OutputWidth set in InitScaler (it is not the ultimate width if horizontal doubling is needed)
// Has to decide whether to use matrix or fast-error-diffusion, open or proprietary.
{
// first level of decision on matrix-vs.FED is the printmode setting itself
    BOOL usematrix = (eHT == MATRIX);
// but this is overridden in speed-optimized builds
// we also can simulate the build-switch at runtime in the test harness
#ifdef APDK_OPTIMIZE_FOR_SPEED
        usematrix = TRUE;
#endif

    pHalftoner = Create_Halftoner(thePrintContext->pSS, CurrentMode,
                                  OutputWidth, numrows,ResBoost,usematrix);
    NEWCHECK(pHalftoner);
    return pHalftoner->constructor_error;
} //SetupHalftoning

DRIVER_ERROR Job::SetupColorMatching()
{

    unsigned int cmWidth=InputWidth;
    if (pResSynth)      // if 2X scaling happens before ColorMatching
        cmWidth *= 2;

    pColorMatcher = Create_ColorMatcher(thePrintContext->pSS, CurrentMode->cmap,
                                        CurrentMode->dyeCount, cmWidth);
    NEWCHECK(pColorMatcher);
    return pColorMatcher->constructor_error;
} //SetupColorMatching


Job::~Job()
{
#ifdef APDK_CAPTURE
    Capture_dJob();
#endif

    // Client isn't required to call NewPage at end of last page, so
    // we may need to eject a page now.
    if (DataSent)
    {
        MediaSource     mSource = thePrintContext->GetMediaSource ();
        if (mSource == sourceBanner)
        {
            thePrintContext->SetMediaSource (sourceTrayAuto);
        }
        newpage();
        thePrintContext->SetMediaSource (mSource);
    }


    // Tell printer that job is over.
    if(pHead)
    {
        pHead->EndJob();
    }//end if

// Delete the 4 components created in Job constructor.
#if defined(APDK_FONTS_NEEDED)
DBG1("deleting TextManager\n");
    if (theTextManager)
    {
        delete theTextManager;
    }
#endif

DBG1("deleting RasterSender\n");
    if (pSender)
    {
        delete pSender;
    }

    if (pHalftoner)
    {
        delete pHalftoner;
    }

    if (pColorMatcher)
    {
        delete pColorMatcher;
    }

    if (BlankRaster)
    {
        pSS->FreeMemory(BlankRaster);
    }

    if (BlackRaster)
    {
        pSS->FreeMemory(BlackRaster);
    }

    if (pBlackPlaneReplicator)
    {
        delete pBlackPlaneReplicator;
    }

    if (pReplicator)
    {
        delete pReplicator;
    }

    if (pResSynth)
    {
        delete pResSynth;
    }

    if (thePipeline)
    {
        delete thePipeline;
    }

    if (pHead)
    {
        delete pHead;
    }

#if defined(APDK_VIP_COLORFILTERING)
    if (pErnie)
    {
        delete pErnie;
    }
#endif

    if (theCompressor)
    {
        delete theCompressor;
    }

    if (pBlackPlaneCompressor)
    {
        delete pBlackPlaneCompressor;
    }

#if defined(APDK_FONTS_NEEDED)
    if (pText)
    {
        delete pText;
    }
#endif // defined(APDK_FONTS_NEEDED)

    if(thePrinter)
    {
        BYTE temp = 0;
        thePrinter->EndJob = TRUE;  // the Job is done -
        thePrinter->Send(&temp,0);  // call Send to dump any buffered data
    }//end if

DBG1("done with ~Job\n");
} //~Job


DRIVER_ERROR Job::SendCAPy()
{
    return pHead->SendCAPy(CAPy++);
} //SendCAPy

///////////////////////////////////////////////////////////////////////////////////
//
/*!
This is the fundamental method for the driver, by means of which graphics data
is sent to the printer.
*/
DRIVER_ERROR Job::SendRasters(BYTE* ImageData)      // RGB24 data for one raster
// This is how graphical data is sent to the driver.

// We do not check for rasters where data is supplied but happens
// to be all blank (white); caller should have used NULL in this case.
{
#ifdef APDK_CAPTURE
    Capture_SendRasters(NULL, ImageData);
#endif

 return sendrasters(NULL, ImageData);
} //SendRasters


///////////////////////////////////////////////////////////////////////////////////
//
/*!
This is the fundamental method for the driver, by means of which graphics data
is sent to the printer.
*/
DRIVER_ERROR Job::SendRasters(BYTE* BlackImageData, BYTE* ColorImageData)      // 1bit K data and RGB24 data for one raster
// This is how graphical data is sent to the driver.

// We do not check for rasters where data is supplied but happens
// to be all blank (white); caller should have used NULL in this case.
{
#ifdef APDK_CAPTURE
	Capture_SendRasters(BlackImageData, ColorImageData);
#endif
	return sendrasters(BlackImageData, ColorImageData);
} //SendRasters

/*!
This is the method for use to check if they can send a separate 1 bit black channel
*/
DRIVER_ERROR Job::SupportSeparateBlack(BOOL* bSeparateBlack)
{
	*bSeparateBlack = thePrinter->SupportSeparateBlack(CurrentMode);
	return NO_ERROR;
}

// internal entry point, used by newpage
DRIVER_ERROR Job::sendrasters(BYTE* BlackImageData, BYTE* ColorImageData)
{
    DRIVER_ERROR err = NO_ERROR;

    if (thePipeline == NULL)
    {
        return SYSTEM_ERROR;
    }

    // need to put out some data occasionally in case of all-text page,
    // so printer will go ahead and start printing the text before end of page
    if (BlackImageData == NULL && ColorImageData == NULL)
    {
		if (thePrinter->GetDataFormat() == RASTER_STRIP)
		{
			skipcount=0;
			ColorImageData=BlankRaster;
		}
		else
		{
			skipcount++;
			if (skipcount >= 200)
			{
				skipcount=0;
				ColorImageData=BlankRaster;
			}
			else
			{
			   fcount += ScaleFactor;
			}

			err = thePipeline->Flush();
			ERRCHECK;
		}
    }
    else
    {
        skipcount=0;
        DataSent=TRUE;      // so we know we need a formfeed when job ends
    }

    RowsInput++;        // needed in case res > 300

	if (BlackImageData || ColorImageData)
	{
		if (fcount > 1000)
		{
		   CAPy += fcount/1000;

			err = thePrinter->SkipRasters ((fcount / 1000)); // needed for DJ3320
			ERRCHECK;
		    fcount =fcount % 1000;
		}

		if (BlackImageData && CurrentMode->dyeCount != 3 && CurrentMode->dyeCount != 6)
		{
			err = setblackraster();
			ERRCHECK;

			for (unsigned int i = 0; i < thePrintContext->InputWidth / 8; i++)
			{
				unsigned char  bitflag = 0x80;
				for (unsigned int j = 0; j < 8; j++)
				{
					if (BlackImageData[i] & bitflag)
						BlackRaster[i*8+j] = 1;
					bitflag = bitflag >> 1;
				}
			}
			if (thePrintContext->InputWidth % 8 > 0)
			{
				unsigned int i = thePrintContext->InputWidth / 8;
				unsigned char bitflag = 0x80;
				for (unsigned int j = 0; j < thePrintContext->InputWidth % 8; j++)
				{
					if (BlackImageData[i] & bitflag)
						BlackRaster[i*8+j] = 1;
					bitflag = bitflag >> 1;
				}
			}
			thePipeline->Exec->raster.rastersize[COLORTYPE_BLACK] = thePrintContext->InputWidth;
			thePipeline->Exec->raster.rasterdata[COLORTYPE_BLACK] = BlackRaster;
		}
		else
		{
		    thePipeline->Exec->raster.rastersize[COLORTYPE_BLACK] = 0;
			thePipeline->Exec->raster.rasterdata[COLORTYPE_BLACK] = NULL;
		}
		if (ColorImageData)
		{
			thePipeline->Exec->raster.rastersize[COLORTYPE_COLOR] = thePrintContext->InputWidth*3;
			thePipeline->Exec->raster.rasterdata[COLORTYPE_COLOR] = ColorImageData;
		}
		else
		{
		    thePipeline->Exec->raster.rastersize[COLORTYPE_COLOR] = 0;
			thePipeline->Exec->raster.rasterdata[COLORTYPE_COLOR] = NULL;
		}
	    err = thePipeline->Execute(&(thePipeline->Exec->raster));
	}

	return err;
} //sendrasters


DRIVER_ERROR Job::newpage()
// (for internal use, called by external NewPage)
{
	DRIVER_ERROR err;

	if (thePrinter->GetDataFormat() == RASTER_STRIP)
	{
		pHead->SetLastBand(TRUE);
	}

    sendrasters();     // flush pipeline

    if ((thePrintContext->GetMediaSource ()) == sourceBanner)
    {
        CAPy = 0;
    }
    else
    {
        err = pHead->FormFeed();
        ERRCHECK;

        // reset vertical cursor counter
        if (thePrinter->UseGUIMode(thePrintContext->CurrentMode) &&
            ((int) (thePrintContext->PrintableStartY () * 100)) != 0)
        // DJ895 in GUImode doesn't accept top-margin setting, so we use CAP for topmargin
        // Start at the top for full-bleed printing - PhotoSmart 100 for now
        {
            CAPy = thePrintContext->GUITopMargin();
        }
        else
        {
            CAPy = 0;
        }
    }

    skipcount = RowsInput = 0;
    fcount = 0;

// reset flag used to see if formfeed needed
    DataSent = FALSE;

    if (!thePrintContext->ModeAgreesWithHardware(TRUE))
    {
        return WARN_MODE_MISMATCH;
    }

#ifdef APDK_USAGE_LOG
    theTranslator->PrintDotCount(UHeader);
    theTranslator->NextPage();
    UTextCount=UText=0;
#endif

    return NO_ERROR;
} //newpage

/*!
This method forces the associated printer to flush the remaining buffered
data and tells it that the job is ended.

Calling this method is not mandatory, since the object destructor already
performs this action. It may be required, however, if the destructor is not
called for some reason (for instance, when the call depends on the execution
of a "finalize" method of a Java wrapper.
*/

DRIVER_ERROR Job::Flush()
{
    if(thePrinter)
    {
        BYTE temp = 0;
        thePrinter->EndJob = TRUE;  // the Job is done -
        return thePrinter->Send(&temp,0);  // call Send to dump any buffered data
    }//end if
    return NO_PRINTER_SELECTED;
}

/*!
Resets counters, flushes buffers, and sends a form feed to the printer.
*/
DRIVER_ERROR Job::NewPage()
// External entry point
{
#ifdef APDK_CAPTURE
    Capture_NewPage();
#endif
    return newpage();
} //NewPage


void Job::SetOutputWidth()
// set OutputWidth -- this is local and may not agree with PrintContext
// in cases where horizontal doubling is needed
{
    OutputWidth = thePrintContext->OutputWidth;
    InputWidth = thePrintContext->InputWidth;

    if (CurrentMode->BaseResX != CurrentMode->BaseResY) // if horizontal expansion needed
    {
        int mixedfactor = CurrentMode->BaseResX / CurrentMode->BaseResY;
        int widthfactor = OutputWidth / InputWidth;
        if (widthfactor >= mixedfactor)
        {
            OutputWidth = OutputWidth / mixedfactor;
        }
    }
} //SetOutputWidth


DRIVER_ERROR Job::InitScaler()
// sets pResSynth & pReplicator
{
    unsigned int numerator, denominator;
    // set OutputWidth -- this is local and may not agree with PrintContext
    // in cases where horizontal doubling is needed (see next if clause)
    SetOutputWidth();

    if ((OutputWidth % InputWidth) == 0)
    {
        numerator = OutputWidth / InputWidth;
        denominator = 1;
    }
    else
    {
        numerator = OutputWidth;
        denominator = InputWidth;
    }


     float tempfactor = (float)numerator / (float)denominator;
     tempfactor *= 1000.0f;
     ScaleFactor = (unsigned int) (int) tempfactor;

	// Two paths: if ResSynth included (proprietary path), then use it for the first doubling;
	//            otherwise do it all in PixelReplication phase
	//  but don't use ResSynth anyway if printer-res=600 and scale<4 (i.e. original-res<150),
	//  or printer-res=300 and scale<2.33 (i.e. original-res<133)
    BOOL RSok;
    BOOL speedy=FALSE;
#ifdef APDK_OPTIMIZE_FOR_SPEED
    speedy = TRUE;
#endif  // APDK_OPTIMIZE_FOR_SPEED
    if (speedy)
    {
        RSok = FALSE;
    }
    else
    {
        if (thePrintContext->EffectiveResolutionX()>300)
        {
            // I don't know about 1200dpi, so I'm doing it this way
            RSok = ScaleFactor >= 4000;
        }
        else
        {
            RSok = ScaleFactor >= 2333;
        }
    }


    unsigned int ReplFormat;                    // how many bytes per pixel
    if (CurrentMode->Config.bColorImage)
    {
        ReplFormat = CurrentMode->dyeCount;      // dealing with colormatching output
    }
    else
    {
        ReplFormat = 3;                        // RGB only
    }

    BOOL vip = !CurrentMode->Config.bColorImage;

    if ((!ProprietaryScaling())  || !RSok )
    // everything happens at Replication phase; input may be KCMY (or K, CMY, etc.) or RGB
    {
        pResSynth=NULL;
        pReplicator = new Scaler_Open(pSS,InputWidth,numerator,denominator,vip,ReplFormat);
        NEWCHECK(pReplicator);
        return pReplicator->constructor_error;
    }

	// Proprietary path and scalefactor>=2, so break it up into 2 parts
	// first give ResSynth a factor of 2, which is all it really does anyway.

	// Scaler_Prop only operates on RGB, so it doesn't need the last two parameters
    pResSynth = Create_Scaler(pSS,InputWidth,2,1,
                              TRUE,     // as if VIP since it is working on RGB only at this phase
                              3);       // 3 bytes per pixel
    NEWCHECK(pResSynth);
	pResSynth->myplane = COLORTYPE_COLOR;
    if (pResSynth->constructor_error != NO_ERROR)
    {
        return pResSynth->constructor_error;
    }

	pBlackPlaneReplicator = new Scaler_Open(pSS,InputWidth,2,1,FALSE,1);
    NEWCHECK(pBlackPlaneReplicator);
	pBlackPlaneReplicator->myplane = COLORTYPE_BLACK;
    if (pBlackPlaneReplicator->constructor_error != NO_ERROR)
    {
        return pBlackPlaneReplicator->constructor_error;
    }

	// now replicate the rest of the way -- only half as much left
    pReplicator = new Scaler_Open(pSS,2*InputWidth,numerator,2*denominator,vip,ReplFormat);
    NEWCHECK(pReplicator);
    return pReplicator->constructor_error;

} //InitScaler


DRIVER_ERROR Job::Configure()
// mode has been set -- now set up rasterwidths and pipeline
{
   DRIVER_ERROR err;
   Pipeline* p=NULL;
   unsigned int width;
   BOOL useRS=FALSE;

   err = InitScaler();      // create pReplicator and maybe pResSynth
   ERRCHECK;

   if ((CurrentMode->Config.bResSynth) && ProprietaryScaling()
       && pResSynth)        // pResSynth==NULL if no scaling required
   {
       p = new Pipeline(pBlackPlaneReplicator);
       NEWCHECK(p);
       if (thePipeline)
          thePipeline->AddPhase(p);
       else thePipeline=p;

       p = new Pipeline(pResSynth);
       NEWCHECK(p);
       if (thePipeline)
          thePipeline->AddPhase(p);
       else thePipeline=p;
       useRS=TRUE;
   }

#if (defined(APDK_DJ9xxVIP) || defined(APDK_LJJETREADY)) && defined(APDK_VIP_COLORFILTERING)
   if (CurrentMode->Config.bErnie)
   {
       // create Ernie (need pixelwidth for constructor)
       if (p)
       {
           width = p->GetMaxOutputWidth(COLORTYPE_COLOR) / 3;     // GetOutputWidth returns # of bytes
       }
       else
       {
           width = thePrintContext->InputWidth;
       }

		// calculate Ernie threshold value
		//Normal: threshold = (resolution) * (0.0876) - 2
		// roughly: image at original 300 implies threshold=24; 600=>48, 150=>12, 75=>6
		// to get resolution of "original" image, divide target resolution by scalefactor
       float scale = (float)thePrintContext->OutputWidth / (float)thePrintContext->InputWidth;
       float original_res = ((float)thePrintContext->EffectiveResolutionX()) / scale;
       if (useRS && (scale >= 2.0f))
       {
           // image already doubled by ResSynth so consider the resolution as of now
            original_res *= 2.0f;
       }
       float fthreshold = original_res * 0.0876f;
       int threshold = (int)fthreshold - 2;

       pErnie = new TErnieFilter(width, eBGRPixelData, threshold);
       p = new Pipeline(pErnie);
       NEWCHECK(p);
       if (thePipeline)
       {
          thePipeline->AddPhase(p);
       }
       else
       {
           thePipeline = p;
       }
	}
#endif //APDK_DJ9xxVIP && APDK_VIP_COLORFILTERING

   if (CurrentMode->Config.bColorImage)
   {
       err = SetupColorMatching();      // create pColorMatcher
       ERRCHECK;
       p = new Pipeline(pColorMatcher);
       NEWCHECK(p);
       if (thePipeline)
       {
          thePipeline->AddPhase(p);
      }
       else
       {
           thePipeline = p;
       }
   }

   if (CurrentMode->Config.bPixelReplicate)
   {
       // create Replicator
       p = new Pipeline(pReplicator);
       NEWCHECK(p);
       if (thePipeline)
       {
          thePipeline->AddPhase(p);
      }
       else
       {
           thePipeline = p;
       }
   }

   if (CurrentMode->Config.bColorImage)
   {
       err = SetupHalftoning(CurrentMode->Config.eHT);     // create pHalftoner
       ERRCHECK;
       p = new Pipeline(pHalftoner);
       NEWCHECK(p);
       if (thePipeline)
       {
          thePipeline->AddPhase(p);
      }
       else
       {
           thePipeline = p;
       }
   }

   if (CurrentMode->Config.bCompress)
   {
       if (p)
       {
            width = p->GetMaxOutputWidth(COLORTYPE_COLOR);
       }
       else
       {
           width = thePrintContext->InputWidth;
       }
       unsigned int SeedBufferSize;
       if (pHalftoner)      // if data is halftoned-output
       {
                        // format is 1 bit-per-pixel for each ink,drop,pass
           SeedBufferSize = MAXCOLORPLANES * MAXCOLORDEPTH * MAXCOLORROWS * width;
       }
       else              // VIP data is just RGB24 here
       {
           SeedBufferSize = width;
       }

       theCompressor = thePrinter->CreateCompressor(SeedBufferSize);
       NEWCHECK(theCompressor);
       err = theCompressor->constructor_error;
       ERRCHECK;
	   theCompressor->myplane = COLORTYPE_COLOR;

       p = new Pipeline(theCompressor);
       NEWCHECK(p);
       if (thePipeline)
       {
           thePipeline->AddPhase(p);
       }
       else
       {
           thePipeline = p;
       }
	   if (thePrinter->VIPPrinter())
	   {
			width = (width/3+7)/8;
		    if (width > 0)
			{
			   // VIP black data is 1 bit here
			   unsigned int SeedBufferSize;
			   SeedBufferSize = width;
			   pBlackPlaneCompressor = thePrinter->CreateBlackPlaneCompressor(SeedBufferSize, TRUE);
			   NEWCHECK(pBlackPlaneCompressor);
			   err = pBlackPlaneCompressor->constructor_error;
			   ERRCHECK;
			   pBlackPlaneCompressor->myplane = COLORTYPE_BLACK;

			   p = new Pipeline(pBlackPlaneCompressor);
			   NEWCHECK(p);
			   if (thePipeline)
			   {
				  thePipeline->AddPhase(p);
			   }
			   else
			   {
				   thePipeline = p;
			   }
		   }
	   }

   }

   // always end pipeline with RasterSender
   // create RasterSender object
   pSender = new RasterSender(thePrinter,thePrintContext,this,pHalftoner);
   NEWCHECK(pSender);
   err = pSender->constructor_error;
   ERRCHECK;

   p = new Pipeline(pSender);

   if (thePipeline)
   {
      thePipeline->AddPhase(p);
  }
   else
   {
       thePipeline = p;
   }

  return NO_ERROR;
} //Configure


DRIVER_ERROR Job::setblankraster()
{
    if (BlankRaster)
    {
        pSS->FreeMemory(BlankRaster);
    }

    size_t    BlankRasterSize = thePrintContext->OutputWidth * 3;          // Raghu
    BlankRaster = (BYTE*)pSS->AllocMem (BlankRasterSize);
    NEWCHECK(BlankRaster);

    memset (BlankRaster, 0xFF, BlankRasterSize);    // Raghu

    return NO_ERROR;
} //setblankraster

DRIVER_ERROR Job::setblackraster()
{
	size_t    BlackRasterSize = thePrintContext->InputWidth;  
    if (!BlackRaster)
    {
		BlackRaster = (BYTE*)pSS->AllocMem (BlackRasterSize);
		NEWCHECK(BlackRaster);
	}
    memset (BlackRaster, 0, BlackRasterSize);

    return NO_ERROR;
} //setblankraster


//////////////////////////////////////////////////////////////////////
#if defined(APDK_FONTS_NEEDED)

/*!
This method is used to send ASCII data to the printer, which it will render
as determined by the Font object.
*/
DRIVER_ERROR Job::TextOut
(
    const char* pTextString,
    unsigned int iLenString,
    const Font& font,
    int iAbsX,
    int iAbsY
)
// This is how ASCII data is sent to the driver.
// Everything is really handled by the TextManager, including error checking.
{
#ifdef APDK_CAPTURE
    Capture_TextOut(pTextString, iLenString, font, iAbsX, iAbsY);
#endif

    // Map coordinates (assumed to be in the graphical space) to the text-placement grid,
    // which may be of lower resolution, indicated by TextRes
    //
    // using floats to cover the unusual case where graphical res is lower than text
    float xfactor = ((float)CurrentMode->BaseResX) / ((float)CurrentMode->TextRes);
    float yfactor = ((float)CurrentMode->BaseResY) / ((float)CurrentMode->TextRes);

    float x = (float)iAbsX / xfactor;
    float y = (float)iAbsY / yfactor;

    iAbsX = (unsigned int)(int)x;   // double cast for webtv compiler
    iAbsY = (unsigned int)(int)y;

    DRIVER_ERROR err = theTextManager->TextOut(pTextString,iLenString,font,iAbsX,iAbsY);
    ERRCHECK;

    DataSent=TRUE;

#ifdef APDK_USAGE_LOG
    if (iLenString > UTextSize)
    {
        iLenString = UTextSize-1;
        UHeader[iLenString] = '\0';
    }
    if (UTextCount<2)
    {
        strcpy(UHeader,pTextString);
        UText += iLenString;
    }
    if (UTextCount==1)
    {
        UHeader[UText] = '\0';
    }
    UTextCount++;

#endif

    return err;
} //TextOut

#endif


///////////////////////////////////////////////////////////
// Pipeline management
Pipeline::Pipeline
(
    Processor* E
) :
    next(NULL),
    prev(NULL)
{
    Exec = E;
    Exec->myphase = this;
} //Pipeline


void Pipeline::AddPhase(Pipeline* newp)
{
    Pipeline* p = this;
    while (p->next)
    {
        p = p->next;
    }
    p->next = newp;
    newp->prev = p;
} //AddPhase


Pipeline::~Pipeline()
{
    if (next)
    {
        delete next;
    }
} //~Pipeline


BOOL Pipeline::Process(RASTERDATA* raster)
{
    return Exec->Process(raster);
} //Process


DRIVER_ERROR Pipeline::Execute(RASTERDATA* InputRaster)
{
    err=NO_ERROR;

    if (Process(InputRaster)        // true if output ready; may set err
        && (err==NO_ERROR))
    {
        if (next)
        {
			next->Exec->raster.rasterdata[COLORTYPE_BLACK] = NextOutputRaster(COLORTYPE_BLACK);
			next->Exec->raster.rasterdata[COLORTYPE_COLOR] = NextOutputRaster(COLORTYPE_COLOR);
            while (next->Exec->raster.rasterdata[COLORTYPE_COLOR] || 
				   next->Exec->raster.rasterdata[COLORTYPE_BLACK])
            {
				next->Exec->raster.rastersize[COLORTYPE_COLOR] = GetOutputWidth(COLORTYPE_COLOR);
				next->Exec->raster.rastersize[COLORTYPE_BLACK] = GetOutputWidth(COLORTYPE_BLACK);
                err = next->Execute(&(next->Exec->raster));
                ERRCHECK;
				next->Exec->raster.rasterdata[COLORTYPE_BLACK] = NextOutputRaster(COLORTYPE_BLACK);
				next->Exec->raster.rasterdata[COLORTYPE_COLOR] = NextOutputRaster(COLORTYPE_COLOR);
            }
        }
    }
    return err;
} //Execute


DRIVER_ERROR Pipeline::Flush()
{
    err=NO_ERROR;

    Exec->Flush();

    if (next && (err == NO_ERROR))
    {
		next->Exec->raster.rasterdata[COLORTYPE_BLACK] = NextOutputRaster(COLORTYPE_BLACK);
		next->Exec->raster.rasterdata[COLORTYPE_COLOR] = NextOutputRaster(COLORTYPE_COLOR);
        while (next->Exec->raster.rasterdata[COLORTYPE_COLOR] || next->Exec->raster.rasterdata[COLORTYPE_BLACK])
        {
			next->Exec->raster.rastersize[COLORTYPE_BLACK] = GetOutputWidth(COLORTYPE_BLACK);
			next->Exec->raster.rastersize[COLORTYPE_COLOR] = GetOutputWidth(COLORTYPE_COLOR);
            err = next->Execute(&(next->Exec->raster));
			next->Exec->raster.rasterdata[COLORTYPE_BLACK] = NextOutputRaster(COLORTYPE_BLACK);
			next->Exec->raster.rasterdata[COLORTYPE_COLOR] = NextOutputRaster(COLORTYPE_COLOR);
            ERRCHECK;
        }
		// one more to continue flushing downstream
		err = next->Flush();
    }

    return err;
} //Flush


Processor::Processor() :
    iRastersReady(0),
    iRastersDelivered(0),
    myphase(NULL),
	myplane(COLORTYPE_BOTH)
{
	for (int i = COLORTYPE_COLOR; i < MAX_COLORTYPE; i++)
	{
		raster.rasterdata[i] = NULL;
		raster.rastersize[i] = 0;
	}
} //Processor


Processor::~Processor()
{
} //~Processor

APDK_END_NAMESPACE
